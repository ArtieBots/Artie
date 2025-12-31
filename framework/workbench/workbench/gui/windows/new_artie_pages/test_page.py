from artie_tooling import artie_profile
from gui.utils import loghandler
from PyQt6 import QtWidgets, QtCore
from comms import tool
from ... import colors

class TestThread(QtCore.QThread):
    """Thread for running the Artie hardware tests"""

    # Signals to communicate with the main thread
    success_signal = QtCore.pyqtSignal()
    failure_signal = QtCore.pyqtSignal(Exception)
    log_message_signal = QtCore.pyqtSignal(str)

    def __init__(self, config: artie_profile.ArtieProfile):
        super().__init__()
        self.config = config

    def run(self):
        """Run the tests in a separate thread"""
        try:
            with tool.ArtieToolInvoker(self.config, logging_handler=loghandler.ThreadLogHandler(self.log_message_signal)) as artie_tool:
                err = artie_tool.test("all-hw")
                if err:
                    self.failure_signal.emit(err)
                    return

                err, success = artie_tool.join(timeout_s=60*10)  # 10 minute timeout
                if err:
                    self.failure_signal.emit(err)
                    return

                if not success:
                    self.failure_signal.emit(Exception("artie-tool.py reported an error."))
                    return

            self.success_signal.emit()
        except Exception as e:
            self.failure_signal.emit(e)

class TestPage(QtWidgets.QWizardPage):
    """Page that runs the artie-tool.py test all-hw command"""

    def __init__(self, config: artie_profile.ArtieProfile):
        super().__init__()
        self.config = config
        self.setTitle(f"<span style='color:{colors.BasePalette.BLACK};'>Testing Hardware</span>")
        self.setSubTitle(f"<span style='color:{colors.BasePalette.DARK_GRAY};'>Running hardware tests...</span>")
        self.setCommitPage(True)

        layout = QtWidgets.QVBoxLayout(self)

        # Progress indicator
        self.progress = QtWidgets.QProgressBar()
        self.progress.setRange(0, 0)  # Indeterminate
        layout.addWidget(self.progress)

        # Output text
        self.output_text = QtWidgets.QTextEdit()
        self.output_text.setReadOnly(True)
        layout.addWidget(self.output_text)

        self.test_complete = False
        self.test_thread = None

    def initializePage(self):
        """Start the tests when page is shown"""
        self.test_complete = False
        self.output_text.clear()

        # Start tests in a separate thread for real-time progress updates
        self.test_thread = TestThread(config=self.config)
        self.test_thread.success_signal.connect(self._handle_success_signal)
        self.test_thread.failure_signal.connect(self._handle_failure_signal)
        self.test_thread.log_message_signal.connect(self.output_text.append)
        self.test_thread.start()

    def _handle_success_signal(self):
        """Handle successful tests"""
        self.output_text.append("\nAll tests passed!")
        self.progress.setRange(0, 1)
        self.progress.setValue(1)
        self.test_complete = True
        self.completeChanged.emit()

    def _handle_failure_signal(self, err: Exception):
        """Handle test failure"""
        self.output_text.append(f"\nERROR: Tests failed: {err}")
        self.progress.setRange(0, 1)
        self.progress.setValue(0)
        self.test_complete = False
        self.completeChanged.emit()

    def isComplete(self):
        """Only allow next when tests are complete"""
        return self.test_complete
