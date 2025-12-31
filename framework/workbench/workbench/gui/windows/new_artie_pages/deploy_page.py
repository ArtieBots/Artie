from artie_tooling import artie_profile
from gui.utils import loghandler
from PyQt6 import QtWidgets, QtCore
from comms import tool
from ... import colors

class DeployThread(QtCore.QThread):
    """Thread for running the Artie deployment"""

    # Signals to communicate with the main thread
    success_signal = QtCore.pyqtSignal()
    failure_signal = QtCore.pyqtSignal(Exception)
    log_message_signal = QtCore.pyqtSignal(str)

    def __init__(self, config: artie_profile.ArtieProfile):
        super().__init__()
        self.config = config

    def run(self):
        """Run the deployment in a separate thread"""
        try:
            with tool.ArtieToolInvoker(self.config, logging_handler=loghandler.ThreadLogHandler(self.log_message_signal)) as artie_tool:
                err = artie_tool.deploy("base")
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

class DeployPage(QtWidgets.QWizardPage):
    """Page that runs the artie-tool.py deploy base command"""

    def __init__(self, config: artie_profile.ArtieProfile):
        super().__init__()
        self.config = config
        self.setTitle(f"<span style='color:{colors.BasePalette.BLACK};'>Deploying Base Configuration</span>")
        self.setSubTitle(f"<span style='color:{colors.BasePalette.DARK_GRAY};'>Running deployment script...</span>")
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

        self.deploy_complete = False
        self.deploy_thread = None

    def initializePage(self):
        """Start the deployment when page is shown"""
        self.deploy_complete = False
        self.output_text.clear()

        # Start deployment in a separate thread for real-time progress updates
        self.deploy_thread = DeployThread(config=self.config)
        self.deploy_thread.success_signal.connect(self._handle_success_signal)
        self.deploy_thread.failure_signal.connect(self._handle_failure_signal)
        self.deploy_thread.log_message_signal.connect(self.output_text.append)
        self.deploy_thread.start()

    def _handle_success_signal(self):
        """Handle successful deployment"""
        self.output_text.append("\nDeployment complete!")
        self.progress.setRange(0, 1)
        self.progress.setValue(1)
        self.deploy_complete = True
        self.completeChanged.emit()

    def _handle_failure_signal(self, err: Exception):
        """Handle deployment failure"""
        self.output_text.append(f"\nERROR: Deployment failed: {err}")
        self.progress.setRange(0, 1)
        self.progress.setValue(0)
        self.deploy_complete = False
        self.completeChanged.emit()

    def isComplete(self):
        """Only allow next when deployment is complete"""
        return self.deploy_complete
