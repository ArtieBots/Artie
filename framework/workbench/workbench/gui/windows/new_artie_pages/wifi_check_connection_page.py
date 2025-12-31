from artie_tooling import artie_profile
from workbench.util import log
from workbench.gui.utils import loghandler
from PyQt6 import QtWidgets, QtCore
from comms import artie_serial
from ... import colors

class WiFiVerificationThread(QtCore.QThread):
    """Thread for verifying WiFi connection"""

    # Signals to communicate with the main thread
    success_signal = QtCore.pyqtSignal(str)  # Emits IP address on success
    failure_signal = QtCore.pyqtSignal(Exception)
    log_message_signal = QtCore.pyqtSignal(str)

    def __init__(self, serial_port: str, bssid: str, ssid: str, password: str, static_ip_config: artie_serial.StaticIPConfig = None):
        super().__init__()
        self.serial_port = serial_port
        self.bssid = bssid
        self.ssid = ssid
        self.password = password
        self.static_ip_config = static_ip_config

    def run(self):
        """Run the WiFi verification in a separate thread"""
        self.log_message_signal.emit("Attempting to connect to WiFi network...")
        if self.ssid == "Custom Configuration":
            self._check_custom_configuration()
        else:
            self._connect_to_wifi()

    def _check_custom_configuration(self):
        """Check if Artie is already connected to WiFi with a custom configuration"""
        self.log_message_signal.emit("Checking existing WiFi connection on Artie...")

        try:
            with artie_serial.ArtieSerialConnection(port=self.serial_port, logging_handler=loghandler.ThreadLogHandler(self.log_message_signal)) as connection:
                err, ip_address = connection.verify_wifi_connection()
                if err:
                    self.failure_signal.emit(err)
                    return

                self.success_signal.emit(ip_address)
        except Exception as e:
            self.failure_signal.emit(e)

    def _connect_to_wifi(self):
        """Connect to the specified WiFi network and verify connection"""
        self.log_message_signal.emit("Sending WiFi credentials to Artie...")

        try:
            with artie_serial.ArtieSerialConnection(port=self.serial_port, logging_handler=loghandler.ThreadLogHandler(self.log_message_signal)) as connection:
                err = connection.select_wifi(self.bssid, self.ssid, self.password, self.static_ip_config)
                if err:
                    self.failure_signal.emit(err)
                    return

                err, ip_address = connection.verify_wifi_connection()
                if err:
                    self.failure_signal.emit(err)
                    return

                self.success_signal.emit(ip_address)
        except Exception as e:
            self.failure_signal.emit(e)

class WiFiCheckConnectionPage(QtWidgets.QWizardPage):
    """Page that verifies the WiFi connection"""

    def __init__(self, config: artie_profile.ArtieProfile):
        super().__init__()
        self.config = config
        self.setTitle(f"<span style='color:{colors.BasePalette.BLACK};'>Verifying WiFi Connection</span>")
        self.setSubTitle(f"<span style='color:{colors.BasePalette.DARK_GRAY};'>Checking that Artie can connect to the selected network...</span>")
        self.setCommitPage(True)

        layout = QtWidgets.QVBoxLayout(self)

        # Connection status icon
        self.status_label = QtWidgets.QLabel()
        self.status_label.setAlignment(QtCore.Qt.AlignmentFlag.AlignCenter)
        self.status_label.setSizePolicy(QtWidgets.QSizePolicy.Policy.Expanding, QtWidgets.QSizePolicy.Policy.Minimum)
        self.status_label.setText("📡\n\nConnecting...")
        self.status_label.setObjectName("icon_label")
        self.status_label.setProperty(colors.QWizardPageStyle.wizard_page_property(), True)
        layout.addWidget(self.status_label)

        # Progress indicator
        self.progress = QtWidgets.QProgressBar()
        self.progress.setRange(0, 0)  # Indeterminate
        layout.addWidget(self.progress)

        # Status text
        self.status_text = QtWidgets.QLabel()
        self.status_text.setAlignment(QtCore.Qt.AlignmentFlag.AlignCenter)
        self.status_text.setWordWrap(True)
        self.status_text.setText("Please wait while we verify the WiFi connection...")
        layout.addWidget(self.status_text)

        # Output details
        self.details_group = QtWidgets.QGroupBox("Connection Details")
        details_layout = QtWidgets.QVBoxLayout(self.details_group)

        self.details_text = QtWidgets.QTextEdit()
        self.details_text.setReadOnly(True)
        self.details_text.setSizePolicy(QtWidgets.QSizePolicy.Policy.Expanding, QtWidgets.QSizePolicy.Policy.Maximum)
        self.details_text.setMinimumHeight(self.details_text.fontMetrics().lineSpacing() * 20)  # At least 20 lines high
        details_layout.addWidget(self.details_text)

        layout.addWidget(self.details_group)

        layout.addStretch()

        self.connection_verified = False
        self.verification_thread = None

    def initializePage(self):
        """Start the WiFi verification when page is shown"""
        # Reset wizard button layout to default (remove skip button from previous page)
        self.wizard().setOption(QtWidgets.QWizard.WizardOption.HaveCustomButton1, False)
        button_layout = [
            QtWidgets.QWizard.WizardButton.Stretch,
            QtWidgets.QWizard.WizardButton.BackButton,
            QtWidgets.QWizard.WizardButton.NextButton,
            QtWidgets.QWizard.WizardButton.CommitButton,
            QtWidgets.QWizard.WizardButton.FinishButton,
            QtWidgets.QWizard.WizardButton.CancelButton
        ]
        self.wizard().setButtonLayout(button_layout)

        self.connection_verified = False
        self.details_text.clear()
        self.status_label.setText("📡\n\nConnecting...")
        self.status_text.setText("Please wait while we verify the WiFi connection...")
        self.progress.setRange(0, 0)  # Indeterminate

        # Log WiFi details
        self.details_text.append(f"Network SSID: {self.field('wifi.ssid')}\n")

        # Start verification in a separate thread for real-time progress updates
        self.verification_thread = WiFiVerificationThread(
            serial_port=self.field('serial.port'),
            bssid=self.field('wifi.bssid'),
            ssid=self.field('wifi.ssid'),
            password=self.field('wifi.password'),
            static_ip_config=artie_serial.StaticIPConfig(
                ip_address=self.field('wifi.static_ip.address'),
                subnet_mask=self.field('wifi.static_ip.subnet'),
                gateway=self.field('wifi.static_ip.gateway'),
                dns=self.field('wifi.static_ip.dns')
            ) if self.field('wifi.use_static_ip') else None
        )
        self.verification_thread.success_signal.connect(self._handle_success_signal)
        self.verification_thread.failure_signal.connect(self._handle_failure_signal)
        self.verification_thread.log_message_signal.connect(self.details_text.append)
        self.verification_thread.start()

    def isComplete(self):
        """Only allow next when connection is verified"""
        return self.connection_verified

    def _handle_success_signal(self, ip_address: str):
        """Successful connection"""
        self.details_text.append("\nConnection established!")
        self.details_text.append(f"Artie is now connected to the network with IP address: {ip_address}.")

        # Update UI
        self.status_label.setText("✅\n\nConnected!")
        self.status_text.setText(f"Successfully connected to {self.field('wifi.ssid')}.")
        self.progress.setRange(0, 1)
        self.progress.setValue(1)

        self.connection_verified = True
        self.completeChanged.emit()

        # Update the config
        self.config.controller_node_ip = ip_address
        log.info(f"Updated controller_node_ip to {ip_address} in ArtieProfile.")

    def _handle_failure_signal(self, err: Exception):
        """Connection failure"""
        self.details_text.append(f"\nERROR: Failed to connect to network: {err}")
        self.details_text.append("Please check the WiFi password and try again.")

        # Update UI
        self.status_label.setText("❌\n\nConnection Failed")
        self.status_text.setText("Failed to connect to WiFi. Please go back and check your credentials.")
        self.progress.setRange(0, 1)
        self.progress.setValue(0)

        self.connection_verified = False
        self.completeChanged.emit()
