from artie_tooling import artie_profile
from artie_tooling import hw_config
from PyQt6 import QtWidgets
from ... import colors

class NamePage(QtWidgets.QWizardPage):
    """Page for naming the Artie and getting a hardware configuration file."""

    def __init__(self, config: artie_profile.ArtieProfile):
        super().__init__()
        self.config = config
        self.setTitle(f"<span style='color:{colors.BasePalette.BLACK};'>Name Your Artie</span>")
        self.setSubTitle(f"<span style='color:{colors.BasePalette.DARK_GRAY};'>Give this Artie a unique, memorable name.</span>")

        layout = QtWidgets.QVBoxLayout(self)

        form_layout = QtWidgets.QFormLayout()

        # Name input
        self.name_input = QtWidgets.QLineEdit()
        self.name_input.setPlaceholderText("e.g., Artie-Lab-01")
        form_layout.addRow("Artie Name:", self.name_input)

        # Hardware config file input
        hw_config_layout = QtWidgets.QHBoxLayout()
        self.hw_config_input = QtWidgets.QLineEdit()
        self.hw_config_input.setPlaceholderText("Path to hardware configuration file (optional)")
        self.hw_config_input.setToolTip("If you have a hardware configuration file, you can specify its path here. Otherwise, leave this blank to retrieve the configuration from the Artie device during installation.")
        hw_config_layout.addWidget(self.hw_config_input)

        browse_button = QtWidgets.QPushButton("Browse...")
        browse_button.clicked.connect(self._browse_hw_config)
        hw_config_layout.addWidget(browse_button)

        form_layout.addRow("Hardware Config File:", hw_config_layout)

        layout.addLayout(form_layout)

        # Info
        info_label = QtWidgets.QLabel(
            "<br>This name will be used to identify this Artie in the Workbench.<br>"
            "Choose a name that helps you distinguish between multiple Arties."
        )
        info_label.setWordWrap(True)
        layout.addWidget(info_label)

        layout.addStretch()

    def _browse_hw_config(self):
        """Open a file dialog to select a hardware config file"""
        file_path, _ = QtWidgets.QFileDialog.getOpenFileName(self, "Select Hardware Configuration File", "", "YAML Files (*.yaml *.yml);;All Files (*)")
        if file_path:
            self.hw_config_input.setText(file_path)

    def validatePage(self):
        """Store the name"""
        self.config.artie_name = self.name_input.text()

        try:
            if self.hw_config_input.text().strip():
                self.config.hardware_config = hw_config.HWConfig.from_config(self.hw_config_input.text().strip())
        except Exception as e:
            QtWidgets.QMessageBox.warning(self, "Invalid Hardware Config", f"Failed to load hardware configuration file:\n{str(e)}")
            return False

        return True
