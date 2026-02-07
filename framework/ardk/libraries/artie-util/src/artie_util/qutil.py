"""
Module for working with queues.
"""
import queue

def get(q: queue.Queue, timeout=None):
    """
    Get an item from the queue, with an optional timeout. If the timeout is reached, return None.
    """
    try:
        return q.get(timeout=timeout)
    except queue.Empty:
        return None

def get_no_wait(q: queue.Queue):
    """
    Get an item from the queue without waiting. If the queue is empty, return None.
    """
    return get(q, timeout=0)
