from PyQt6 import QtWidgets, QtCore
from artie_tooling import artie_profile
from comms import artie_serial
from util import log
import threading
from ... import colors

class WiFiSelectionPage(QtWidgets.QWizardPage):
    """Page for selecting WiFi network and entering credentials"""

    _error_signal = QtCore.pyqtSignal(str)
    """Signal for reporting errors from threads."""

    def __init__(self, config: artie_profile.ArtieProfile):
        super().__init__()
        self.config = config

        self._error_signal.connect(self._on_error)
        self._scanning_thread = threading.Thread(target=self._scan_networks, name='scanning thread', daemon=True)
        self.setTitle(f"<span style='color:{colors.BasePalette.BLACK};'>Configure WiFi</span>")
        self.setSubTitle(f"<span style='color:{colors.BasePalette.DARK_GRAY};'>Select a WiFi network for Artie to connect to.</span>")

        layout = QtWidgets.QVBoxLayout(self)

        # WiFi network list
        network_label = QtWidgets.QLabel("Available Networks:")
        layout.addWidget(network_label)

        self.network_table = QtWidgets.QTableWidget()
        header_labels = ["SSID", "Signal Level", "BSSID", "Frequency"]
        self.network_table.setColumnCount(len(header_labels))
        self.network_table.setHorizontalHeaderLabels(header_labels)
        self.network_table.horizontalHeader().setStretchLastSection(True)
        self.network_table.setSizePolicy(QtWidgets.QSizePolicy.Policy.Expanding, QtWidgets.QSizePolicy.Policy.Expanding)
        self.network_table.setMinimumHeight(self.network_table.fontInfo().pixelSize() * 10)
        layout.addWidget(self.network_table)

        # Scan button
        scan_layout = QtWidgets.QHBoxLayout()
        self.scan_button = QtWidgets.QPushButton("Scan for Networks")
        self.scan_button.setSizePolicy(QtWidgets.QSizePolicy.Policy.Fixed, QtWidgets.QSizePolicy.Policy.Fixed)
        self.scan_button.clicked.connect(self._spawn_scan_thread)
        scan_layout.addWidget(self.scan_button)
        scan_layout.addStretch()
        layout.addLayout(scan_layout)

        # WiFi credentials
        credentials_group = QtWidgets.QGroupBox("Network Credentials")
        credentials_layout = QtWidgets.QFormLayout(credentials_group)

        self.ssid_input = QtWidgets.QLineEdit()
        self.ssid_input.setPlaceholderText("Selected network SSID")
        self.ssid_input.setReadOnly(True)
        self.ssid_input.setSizePolicy(QtWidgets.QSizePolicy.Policy.Expanding, QtWidgets.QSizePolicy.Policy.Fixed)
        credentials_layout.addRow("SSID:", self.ssid_input)

        self.wifi_password_input = QtWidgets.QLineEdit()
        self.wifi_password_input.setEchoMode(QtWidgets.QLineEdit.EchoMode.Password)
        self.wifi_password_input.setPlaceholderText("Enter WiFi password")
        self.wifi_password_input.setSizePolicy(QtWidgets.QSizePolicy.Policy.Expanding, QtWidgets.QSizePolicy.Policy.Fixed)
        credentials_layout.addRow("Password:", self.wifi_password_input)

        self.bssid_input = QtWidgets.QLineEdit()
        self.bssid_input.setPlaceholderText("Selected network BSSID")
        self.bssid_input.setReadOnly(True)
        self.bssid_input.setSizePolicy(QtWidgets.QSizePolicy.Policy.Expanding, QtWidgets.QSizePolicy.Policy.Fixed)
        credentials_layout.addRow("BSSID:", self.bssid_input)

        layout.addWidget(credentials_group)

        # Static IP configuration (optional)
        static_ip_group = QtWidgets.QGroupBox("Static IP Configuration (Optional)")
        static_ip_layout = QtWidgets.QFormLayout(static_ip_group)

        self.use_static_ip = QtWidgets.QCheckBox("Use Static IP Address")
        self.use_static_ip.setChecked(False)
        static_ip_layout.addRow(self.use_static_ip)

        self.static_ip_input = QtWidgets.QLineEdit()
        self.static_ip_input.setPlaceholderText("e.g., 192.168.1.100")
        self.static_ip_input.setEnabled(False)
        self.static_ip_input.setSizePolicy(QtWidgets.QSizePolicy.Policy.Expanding, QtWidgets.QSizePolicy.Policy.Fixed)
        static_ip_layout.addRow("IP Address:", self.static_ip_input)

        self.subnet_mask_input = QtWidgets.QLineEdit()
        self.subnet_mask_input.setPlaceholderText("e.g., 255.255.255.0")
        self.subnet_mask_input.setText("255.255.255.0")
        self.subnet_mask_input.setEnabled(False)
        self.subnet_mask_input.setSizePolicy(QtWidgets.QSizePolicy.Policy.Expanding, QtWidgets.QSizePolicy.Policy.Fixed)
        static_ip_layout.addRow("Subnet Mask:", self.subnet_mask_input)

        self.gateway_input = QtWidgets.QLineEdit()
        self.gateway_input.setPlaceholderText("e.g., 192.168.1.1")
        self.gateway_input.setEnabled(False)
        self.gateway_input.setSizePolicy(QtWidgets.QSizePolicy.Policy.Expanding, QtWidgets.QSizePolicy.Policy.Fixed)
        static_ip_layout.addRow("Gateway:", self.gateway_input)

        self.dns_input = QtWidgets.QLineEdit()
        self.dns_input.setPlaceholderText("e.g., 8.8.8.8")
        self.dns_input.setText("8.8.8.8")
        self.dns_input.setEnabled(False)
        self.dns_input.setSizePolicy(QtWidgets.QSizePolicy.Policy.Expanding, QtWidgets.QSizePolicy.Policy.Fixed)
        static_ip_layout.addRow("DNS Server:", self.dns_input)

        layout.addWidget(static_ip_group)

        # Connect checkbox to enable/disable static IP fields
        self.use_static_ip.toggled.connect(self._on_static_ip_toggled)

        # Connect list selection to SSID field
        self.network_table.itemSelectionChanged.connect(self._on_network_selected)

        # Note
        note_label = QtWidgets.QLabel("<i>Note: WiFi credentials are stored on Artie's OS, not in the Workbench.</i>")
        note_label.setWordWrap(True)
        note_label.setStyleSheet(f"color: {colors.BasePalette.GRAY};")
        layout.addWidget(note_label)

        # Pass the SSID, password, and BSSID to the next page
        self.registerField('wifi.ssid', self.ssid_input, 'text')
        self.registerField('wifi.password', self.wifi_password_input, 'text')
        self.registerField('wifi.bssid', self.bssid_input, 'text')
        self.registerField('wifi.use_static_ip', self.use_static_ip)
        self.registerField('wifi.static_ip', self.static_ip_input, 'text')
        self.registerField('wifi.subnet_mask', self.subnet_mask_input, 'text')
        self.registerField('wifi.gateway', self.gateway_input, 'text')
        self.registerField('wifi.dns', self.dns_input, 'text')

    def initializePage(self):
        super().initializePage()
        # Resize the wizard to fit the content
        self.wizard().resize(self.sizeHint())
        # Re-center the wizard on screen
        qr = self.wizard().frameGeometry()
        cp = QtWidgets.QDesktopWidget().availableGeometry().center()
        qr.moveCenter(cp)
        self.wizard().move(qr.topLeft())

    def _on_static_ip_toggled(self, checked: bool):
        """Enable/disable static IP fields based on checkbox state"""
        self.static_ip_input.setEnabled(checked)
        self.subnet_mask_input.setEnabled(checked)
        self.gateway_input.setEnabled(checked)
        self.dns_input.setEnabled(checked)

    def _spawn_scan_thread(self):
        """Scan for available WiFi networks"""
        if self._scanning_thread.is_alive():
            # Shouldn't be possible due to button disabling, but just in case
            return

        self._scanning_thread = threading.Thread(target=self._scan_networks, name='scanning thread', daemon=True)
        self.network_table.clearContents()
        self._disable_scan_button()

        # Start the scanning thread. This will do its best to asyncronously
        # find the Wifi networks that Artie has access to and populate
        # them in the network list.
        self._scanning_thread.start()

    def _scan_networks(self):
        """This is the thread target for the scanning button."""
        with artie_serial.ArtieSerialConnection(port=self.field('serial.port')) as connection:
            err, wifi_networks = connection.scan_for_wifi_networks()
            if err:
                self._error_signal.emit(f"An error occurred while scanning for networks: {err}. Try scanning again.")
                self._enable_scan_button()
                return

        log.debug(f"Found {len(wifi_networks)} WiFi networks.")
        row_position = 0
        for network in wifi_networks:
            # If this index does not exist, add a new row
            if row_position >= self.network_table.rowCount():
                self.network_table.insertRow(row_position)
            self.network_table.setItem(row_position, 0, QtWidgets.QTableWidgetItem(network.ssid))
            self.network_table.setItem(row_position, 1, QtWidgets.QTableWidgetItem(str(network.signal_level)))
            self.network_table.setItem(row_position, 2, QtWidgets.QTableWidgetItem(network.bssid))
            self.network_table.setItem(row_position, 3, QtWidgets.QTableWidgetItem(str(network.frequency)))
            row_position += 1

        self._enable_scan_button()

    def _enable_scan_button(self):
        """Re-enable the scan button from the scanning thread."""
        self.scan_button.setEnabled(True)
        self.scan_button.setText("Scan for Networks")

    def _disable_scan_button(self):
        """Disable the scan button from the scanning thread."""
        self.scan_button.setEnabled(False)
        self.scan_button.setText("Scanning...")

    def _on_error(self, message: str):
        """Handle errors from threads by showing a message box."""
        QtWidgets.QMessageBox.critical(self, "Error", message)

    def _on_network_selected(self):
        """Update SSID field when network is selected"""
        # First, select the entire row when an item is clicked
        selected_ranges = self.network_table.selectedRanges()
        for selected_range in selected_ranges:
            for row in range(selected_range.topRow(), selected_range.bottomRow() + 1):
                self.network_table.selectRow(row)

        # Now determine the SSID of the selected row
        selected_items = self.network_table.selectedItems()
        if selected_items:
            ssid = selected_items[0].text()
            self.ssid_input.setText(ssid)
            bssid = selected_items[2].text()
            self.bssid_input.setText(bssid)

    def validatePage(self):
        """Validate WiFi selection"""
        ssid = self.ssid_input.text()
        bssid = self.bssid_input.text()
        password = self.wifi_password_input.text()

        # Pass the SSID, password, and BSSID to the next page
        self.setField('wifi.ssid', ssid)
        self.setField('wifi.password', password)
        self.setField('wifi.bssid', bssid)

        if not ssid:
            QtWidgets.QMessageBox.warning(self, "No Network Selected", "Please select a WiFi network.")
            return False

        if not password:
            QtWidgets.QMessageBox.warning(self, "No Password", "Please enter the WiFi password.")
            return False

        if not bssid:
            QtWidgets.QMessageBox.warning(self, "No Network Selected", "Please select a WiFi network.")
            return False

        # Validate static IP configuration if enabled
        if self.use_static_ip.isChecked():
            static_ip = self.static_ip_input.text().strip()

            if not static_ip:
                QtWidgets.QMessageBox.warning(self, "Invalid IP Address", "Please enter a valid IP address.")
                return False

            # Store static IP configuration
            self.setField('wifi.use_static_ip', True)
            self.setField('wifi.static_ip', static_ip)
            self.setField('wifi.subnet_mask', self.subnet_mask_input.text().strip())
            self.setField('wifi.gateway', self.gateway_input.text().strip())
            self.setField('wifi.dns', self.dns_input.text().strip())
        else:
            self.setField('wifi.use_static_ip', False)

        return True
