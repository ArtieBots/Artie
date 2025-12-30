from artie_tooling import artie_profile
from gui.utils import loghandler
from PyQt6 import QtWidgets, QtCore
from comms import artie_serial
from comms import tool
from ... import colors
import tempfile

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
        layout.addWidget(self.output_text)

        self.install_complete = False

    def initializePage(self):
        """Start the installation when page is shown"""
        super().initializePage()
        self.install_complete = False
        self.output_text.clear()
        QtCore.QTimer.singleShot(500, self._run_install)

    def _complete_install(self, success: bool, err=None):
        """Complete the installation process"""
        if success:
            self.output_text.append("\nInstallation complete!")
        else:
            self.output_text.append(f"\nERROR: Installation failed: {err}")
        self.progress.setRange(0, 1)
        self.progress.setValue(1)
        self.install_complete = success
        self.completeChanged.emit()

    def _run_install(self):
        """Run the artie-tool.py install command"""
        # First get the hardware configuration from the controller node
        with artie_serial.ArtieSerialConnection(self.field('serial.port'), logging_handler=loghandler.QTextEditLogHandler(self.output_text)) as artie_serial_conn:
            err, hw_config = artie_serial_conn.get_hardware_config()
            if err:
                self._complete_install(False, err)
                return
            self.config.hardware_config = hw_config

        with tempfile.NamedTemporaryFile(delete=True, mode='w+', delete_on_close=False) as hw_file:
            hw_file.write(self.config.hardware_config.to_json_str())
            hw_file.flush()
            hw_file.close()

            with tool.ArtieToolInvoker(self.config, logging_handler=loghandler.QTextEditLogHandler(self.output_text)) as artie_tool:
                err = artie_tool.install(hw_file.name)
                if err:
                    self._complete_install(False, err)
                    return

                err, success = artie_tool.join(timeout_s=60*10)  # 10 minute timeout
                if err:
                    self._complete_install(False, err)
                    return

                if not success:
                    self._complete_install(False, "artie-tool.py reported an error.")
                    return

        self._complete_install(True)

    def isComplete(self):
        """Only allow next when installation is complete"""
        return self.install_complete
