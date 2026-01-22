from .driver import *
from .service import *
from .status_led import *

import functools
import typing

class Interface(typing.Protocol):
    __interface_name__: str

def interface_method(interface_class: Interface) -> typing.Callable:
    """
    Decorator to mark a method as implementing an interface method.

    This is entirely for documentation purposes and provides no runtime enforcement.

    Args:
        interface_class: The interface class whose method is being implemented.

    Returns:
        The decorated method.
    """
    def decorator(func: typing.Callable) -> typing.Callable:
        @functools.wraps(func)
        def wrapper(*args, **kwargs):
            return func(*args, **kwargs)
        return wrapper
    return decorator
