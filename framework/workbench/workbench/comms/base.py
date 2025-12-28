"""
Base class for Artie communication interfaces.
"""
from workbench.util import log

class ArtieCommsBase:
    """Base class for Artie communication interfaces."""
    
    def __init__(self, logging_handler=None):
        self.logging_handler = logging_handler
    
    def __enter__(self):
        """Open the serial connection when entering context"""
        self.open()

        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        """Close the serial connection when exiting context"""
        self.close()

        return False

    def open(self):
        """Open the communication interface"""
        if self.logging_handler is not None:
            log.add_handler(self.logging_handler)

    def close(self):
        """Close the communication interface"""
        if self.logging_handler is not None:
            log.remove_handler(self.logging_handler)

    def reset(self):
        """Close and re-open the connection."""
        self.close()
        self.open()
