"""
This module contains a cache monitor for the Artie RPC Broker service.

The cache monitor is constantly monitoring the broker cache directory
for changes and invalidating the in-memory cache of services
when changes are detected.

This implementation uses Linux inotify system calls directly.
"""
import os
import pathlib
import threading
import struct
import select
import logging
from artie_util import artie_logging as alog
from ctypes import cdll, c_int, c_char_p, c_uint32

# Load libc for inotify system calls
libc = cdll.LoadLibrary("libc.so.6")

# inotify constants
IN_MODIFY = 0x00000002
IN_CLOSE_WRITE = 0x00000008
IN_NONBLOCK = 0x00000800
IN_CLOEXEC = 0x00080000

cache_valid = False
"""Whether the cache is valid or not. This variable is modified by the CacheMonitor class and read by the ArtieRPCBrokerServer class."""

class CacheMonitor(threading.Thread):
    """
    This class implements a cache monitor for the Artie RPC Broker service.
    It monitors the broker cache directory for changes and invalidates
    the in-memory cache of services when changes are detected.
    """
    def __init__(self, broker_cache_dpath: str):
        super().__init__(daemon=True)
        self._broker_cache_fname = "broker-cache.txt"
        self._broker_cache_fpath = pathlib.Path(broker_cache_dpath) / self._broker_cache_fname
        self._broker_cache_dpath = pathlib.Path(broker_cache_dpath)
        self._stop_event = threading.Event()
        self._inotify_fd = None
        self._watch_fd = None

    def run(self):
        """
        Starts the cache monitor.
        This method runs indefinitely, monitoring the broker cache directory for changes.
        """
        alog.info(f"Starting cache monitor for {self._broker_cache_dpath}")

        try:
            self._execute_monitor()
        except Exception as e:
            alog.error(f"Error in cache monitor: {e}", exc_info=True)
            self._cleanup()
            alog.error(f"Broker exiting due to cache monitor failure")
            exit(-1)

        self._cleanup()

    def _cleanup(self):
        """Clean up inotify resources."""
        if self._watch_fd is not None and self._inotify_fd is not None:
            try:
                libc.inotify_rm_watch(self._inotify_fd, self._watch_fd)
            except Exception as e:
                alog.error(f"Error removing inotify watch: {e}")

        if self._inotify_fd is not None:
            try:
                os.close(self._inotify_fd)
            except Exception as e:
                alog.error(f"Error closing inotify fd: {e}")

    def _execute_monitor(self):
        global cache_valid

        # Initialize inotify
        self._inotify_fd = libc.inotify_init1(IN_NONBLOCK | IN_CLOEXEC)
        if self._inotify_fd < 0:
            raise OSError("Failed to initialize inotify")

        # Add watch on the cache directory
        path_bytes = str(self._broker_cache_dpath).encode('utf-8')
        mask = IN_MODIFY | IN_CLOSE_WRITE
        self._watch_fd = libc.inotify_add_watch(self._inotify_fd, c_char_p(path_bytes), c_uint32(mask))
        if self._watch_fd < 0:
            raise OSError(f"Failed to add watch on {self._broker_cache_dpath}")

        # Monitor for events
        alog.info("Cache monitor started successfully")
        while not self._stop_event.is_set():
            # Use select with timeout to allow periodic checking of stop_event
            readable, _, _ = select.select([self._inotify_fd], [], [], 1.0)

            if not readable:
                continue

            # Read inotify events
            try:
                data = os.read(self._inotify_fd, 4096)
            except BlockingIOError:
                continue

            if not data:
                continue

            # Parse inotify events
            offset = 0
            while offset < len(data):
                # struct inotify_event { int wd; uint32_t mask; uint32_t cookie; uint32_t len; char name[]; }
                if offset + 16 > len(data):
                    break

                wd, mask, cookie, name_len = struct.unpack('iIII', data[offset:offset+16])
                offset += 16

                if name_len > 0:
                    name_bytes = data[offset:offset+name_len]
                    # Remove trailing null bytes
                    name = name_bytes.rstrip(b'\x00').decode('utf-8', errors='ignore')
                    offset += name_len

                    # Check if the broker-cache.txt file was modified
                    if name == self._broker_cache_fname and (mask & (IN_MODIFY | IN_CLOSE_WRITE)):
                        alog.info("Cache file changed, invalidating cache")
                        cache_valid = False
                else:
                    # Event on the directory itself
                    pass

    def stop(self):
        """Stop the cache monitor gracefully."""
        alog.info("Stopping cache monitor")
        self._stop_event.set()
