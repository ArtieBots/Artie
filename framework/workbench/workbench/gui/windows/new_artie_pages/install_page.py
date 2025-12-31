from artie_tooling import artie_profile
from gui.utils import loghandler
from workbench.util import log
from PyQt6 import QtWidgets, QtCore
from comms import artie_serial
from comms import tool
from ... import colors
import tempfile

class InstallThread(QtCore.QThread):
    """Thread for running the Artie installation"""

    # Signals to communicate with the main thread
    success_signal = QtCore.pyqtSignal()
    failure_signal = QtCore.pyqtSignal(Exception)
    log_message_signal = QtCore.pyqtSignal(str)

    def __init__(self, config: artie_profile.ArtieProfile, serial_port: str):
        super().__init__()
        self.config = config
        self.serial_port = serial_port

    def run(self):
        """Run the installation in a separate thread"""
        try:
            # First get the hardware configuration from the controller node
            self.log_message_signal.emit("Retrieving hardware configuration from Artie...")
            with artie_serial.ArtieSerialConnection(port=self.serial_port, logging_handler=loghandler.ThreadLogHandler(self.log_message_signal)) as artie_serial_conn:
                err, hw_config = artie_serial_conn.get_hardware_config()
                if err:
                    self.failure_signal.emit(err)
                    return
                self.config.hardware_config = hw_config

            with tempfile.NamedTemporaryFile(delete=True, mode='w+', delete_on_close=False) as hw_file:
                hw_file.write(self.config.hardware_config.to_json_str())
                hw_file.flush()
                hw_file.close()

                self.log_message_signal.emit("Starting installation with artie-tool.py...")
                with tool.ArtieToolInvoker(self.config, logging_handler=loghandler.ThreadLogHandler(self.log_message_signal)) as artie_tool:
                    err = artie_tool.install(hw_file.name)
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

class InstallPage(QtWidgets.QWizardPage):
    """Page that runs the artie-tool.py install command"""

    def __init__(self, config: artie_profile.ArtieProfile):
        super().__init__()
        self.config = config
        self.setTitle(f"<span style='color:{colors.BasePalette.BLACK};'>Installing Artie</span>")
        self.setSubTitle(f"<span style='color:{colors.BasePalette.DARK_GRAY};'>Running installation script...</span>")
        self.setCommitPage(True)

        layout = QtWidgets.QVBoxLayout(self)

        # Progress indicator
        self.progress = QtWidgets.QProgressBar()
        self.progress.setRange(0, 0)  # Indeterminate
        layout.addWidget(self.progress)

        # Output text
        self.output_text = QtWidgets.QTextEdit()
        self.output_text.setReadOnly(True)
        self.output_text.setSizePolicy(QtWidgets.QSizePolicy.Policy.Expanding, QtWidgets.QSizePolicy.Policy.MinimumExpanding)
        self.output_text.setMinimumHeight(self.output_text.fontMetrics().lineSpacing() * 20)  # At least 20 lines high
        layout.addWidget(self.output_text)

        self.install_complete = False
        self.install_thread = None

    def initializePage(self):
        """Start the installation when page is shown"""
        self.install_complete = False
        self.output_text.clear()

        # Start installation in a separate thread for real-time progress updates
        self.install_thread = InstallThread(
            config=self.config,
            serial_port=self.field('serial.port')
        )
        self.install_thread.success_signal.connect(self._handle_success_signal)
        self.install_thread.failure_signal.connect(self._handle_failure_signal)
        self.install_thread.log_message_signal.connect(self.output_text.append)
        self.install_thread.start()

    def _handle_success_signal(self):
        """Handle successful installation"""
        self.output_text.append("\nInstallation complete!")
        self.progress.setRange(0, 1)
        self.progress.setValue(1)
        self.install_complete = True
        self.completeChanged.emit()

    def _handle_failure_signal(self, err: Exception):
        """Handle installation failure"""
        self.output_text.append(f"\nERROR: Installation failed: {err}")
        self.progress.setRange(0, 1)
        self.progress.setValue(0)
        self.install_complete = False
        self.completeChanged.emit()

    def isComplete(self):
        """Only allow next when installation is complete"""
        return self.install_complete
