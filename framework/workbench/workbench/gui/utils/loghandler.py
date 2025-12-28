"""
This module contains code for wrapping Qt Widgets into logging handlers.
"""
import logging
from PyQt6 import QtWidgets

class QTextEditLogHandler(logging.Handler):
    """
    A logging handler that outputs log messages to a QTextEdit widget.
    """
    def __init__(self, text_edit: QtWidgets.QTextEdit, level=logging.NOTSET, formatter: logging.Formatter = None):
        super().__init__(level=level)
        self.text_edit = text_edit
        if formatter is not None:
            self.setFormatter(formatter)

    def emit(self, record: logging.LogRecord):
        """Emit a log record to the QTextEdit."""
        msg = self.format(record)
        self.text_edit.append(msg)

class ThreadLogHandler(logging.Handler):
    """A logging handler that emits log messages via a Qt signal."""
    def __init__(self, signal, level=logging.NOTSET, formatter: logging.Formatter = None):
        super().__init__(level=level)
        self.signal = signal
        if formatter is not None:
            self.setFormatter(formatter)
    
    def emit(self, record: logging.LogRecord):
        """Emit a log record via the signal."""
        msg = self.format(record)
        self.signal.emit(msg)
