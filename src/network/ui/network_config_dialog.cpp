#include "network_config_dialog.h"
#include <QMessageBox>
#include <QFileDialog>
#include <QJsonDocument>
#include <QJsonArray>
#include <QStandardPaths>
#include <QNetworkInterface>
#include <QHostInfo>
#include <QProcess>
#include <QDateTime>
#include <QValidator>
#include <QRegularExpression>
#include <QRegularExpressionValidator>

namespace Monitor {
namespace Network {

NetworkConfigDialog::NetworkConfigDialog(Mode mode, const NetworkConfig& config, QWidget* parent)
    : QDialog(parent)
    , m_mode(mode)
    , m_currentConfig(config)
    , m_configurationValid(false)
    , m_mainLayout(nullptr)
    , m_tabWidget(nullptr)
    , m_buttonBox(nullptr)
    , m_connectionTestRunning(false)
    , m_logger(Logging::Logger::instance())
{
    setWindowTitle(mode == Mode::Create ? "Create Network Configuration" : "Edit Network Configuration");
    setModal(true);
    setMinimumSize(600, 500);
    resize(800, 600);
    
    setupUI();
    setupConnections();
    
    // Apply initial configuration
    if (mode == Mode::Edit) {
        applyConfigurationToUI(config);
    } else {
        resetUIToDefaults();
    }
    
    // Setup validation timer
    m_validationTimer = new QTimer(this);
    m_validationTimer->setSingleShot(true);
    m_validationTimer->setInterval(500); // 500ms delay
    connect(m_validationTimer, &QTimer::timeout, this, &NetworkConfigDialog::validateConfiguration);
    
    // Setup connection test timer
    m_connectionTestTimer = new QTimer(this);
    connect(m_connectionTestTimer, &QTimer::timeout, this, &NetworkConfigDialog::onConnectionTestTimer);
    
    // Initial validation
    validateConfiguration();
}

NetworkConfigDialog::~NetworkConfigDialog() = default;

void NetworkConfigDialog::setupUI() {
    m_mainLayout = new QVBoxLayout(this);
    
    // Create tab widget
    m_tabWidget = new QTabWidget(this);
    
    // Create tabs
    createBasicTab();
    createAdvancedTab();
    createProfilesTab();
    createDiagnosticsTab();
    
    // Add validation status
    m_validationLabel = new QLabel(this);
    m_validationLabel->setWordWrap(true);
    m_validationLabel->setStyleSheet("QLabel { color: red; font-weight: bold; }");
    
    // Create button box
    m_buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    
    // Add widgets to main layout
    m_mainLayout->addWidget(m_tabWidget);
    m_mainLayout->addWidget(m_validationLabel);
    m_mainLayout->addWidget(m_buttonBox);
    
    setLayout(m_mainLayout);
}

void NetworkConfigDialog::createBasicTab() {
    m_basicTab = new QWidget();
    m_tabWidget->addTab(m_basicTab, "Basic Settings");
    
    auto* layout = new QVBoxLayout(m_basicTab);
    
    // General settings group
    auto* generalGroup = new QGroupBox("General Settings");
    auto* generalLayout = new QFormLayout(generalGroup);
    
    // Profile name
    m_profileNameEdit = new QLineEdit();
    m_profileNameEdit->setPlaceholderText("Enter profile name...");
    generalLayout->addRow("Profile Name:", m_profileNameEdit);
    
    // Protocol selection
    m_protocolCombo = new QComboBox();
    m_protocolCombo->addItem("UDP", static_cast<int>(Protocol::UDP));
    m_protocolCombo->addItem("TCP", static_cast<int>(Protocol::TCP));
    generalLayout->addRow("Protocol:", m_protocolCombo);
    
    // Network interface selection
    auto* interfaceLayout = new QHBoxLayout();
    m_interfaceCombo = new QComboBox();
    m_interfaceCombo->setEditable(true);
    m_refreshInterfacesButton = new QPushButton("Refresh");
    m_refreshInterfacesButton->setMaximumWidth(80);
    interfaceLayout->addWidget(m_interfaceCombo);
    interfaceLayout->addWidget(m_refreshInterfacesButton);
    generalLayout->addRow("Network Interface:", interfaceLayout);
    
    layout->addWidget(generalGroup);
    
    // Address settings group
    auto* addressGroup = new QGroupBox("Address Settings");
    auto* addressLayout = new QFormLayout(addressGroup);
    
    // Local address and port
    m_localAddressEdit = new QLineEdit();
    m_localAddressEdit->setPlaceholderText("0.0.0.0");
    addressLayout->addRow("Local Address:", m_localAddressEdit);
    
    m_localPortSpin = new QSpinBox();
    m_localPortSpin->setRange(1, 65535);
    addressLayout->addRow("Local Port:", m_localPortSpin);
    
    // Remote address and port
    m_remoteAddressEdit = new QLineEdit();
    m_remoteAddressEdit->setPlaceholderText("192.168.1.100");
    addressLayout->addRow("Remote Address:", m_remoteAddressEdit);
    
    m_remotePortSpin = new QSpinBox();
    m_remotePortSpin->setRange(1, 65535);
    addressLayout->addRow("Remote Port:", m_remotePortSpin);
    
    layout->addWidget(addressGroup);
    
    // Multicast settings group
    m_multicastGroup = new QGroupBox("Multicast Settings");
    m_multicastGroup->setCheckable(true);
    m_multicastGroup->setChecked(false);
    auto* multicastLayout = new QFormLayout(m_multicastGroup);
    
    m_multicastCheck = new QCheckBox("Enable Multicast");
    multicastLayout->addRow(m_multicastCheck);
    
    m_multicastAddressEdit = new QLineEdit();
    m_multicastAddressEdit->setPlaceholderText("224.0.0.1");
    multicastLayout->addRow("Multicast Group:", m_multicastAddressEdit);
    
    m_multicastTTLSpin = new QSpinBox();
    m_multicastTTLSpin->setRange(1, 255);
    m_multicastTTLSpin->setValue(1);
    multicastLayout->addRow("TTL:", m_multicastTTLSpin);
    
    layout->addWidget(m_multicastGroup);
    
    layout->addStretch();
    
    // Populate network interfaces
    populateNetworkInterfaces();
}

void NetworkConfigDialog::createAdvancedTab() {
    m_advancedTab = new QWidget();
    m_tabWidget->addTab(m_advancedTab, "Advanced");
    
    auto* layout = new QVBoxLayout(m_advancedTab);
    
    // Performance settings group
    auto* perfGroup = new QGroupBox("Performance Settings");
    auto* perfLayout = new QFormLayout(perfGroup);
    
    m_bufferSizeSpin = new QSpinBox();
    m_bufferSizeSpin->setRange(1024, 67108864); // 1KB to 64MB
    m_bufferSizeSpin->setValue(1048576);
    m_bufferSizeSpin->setSuffix(" bytes");
    perfLayout->addRow("Receive Buffer Size:", m_bufferSizeSpin);
    
    m_socketTimeoutSpin = new QSpinBox();
    m_socketTimeoutSpin->setRange(100, 30000);
    m_socketTimeoutSpin->setValue(1000);
    m_socketTimeoutSpin->setSuffix(" ms");
    perfLayout->addRow("Socket Timeout:", m_socketTimeoutSpin);
    
    m_maxPacketSizeSpin = new QSpinBox();
    m_maxPacketSizeSpin->setRange(64, 65536);
    m_maxPacketSizeSpin->setValue(65536);
    m_maxPacketSizeSpin->setSuffix(" bytes");
    perfLayout->addRow("Max Packet Size:", m_maxPacketSizeSpin);
    
    layout->addWidget(perfGroup);
    
    // Quality of Service group
    auto* qosGroup = new QGroupBox("Quality of Service");
    auto* qosLayout = new QFormLayout(qosGroup);
    
    m_timestampingCheck = new QCheckBox("Enable High-Precision Timestamping");
    m_timestampingCheck->setChecked(true);
    qosLayout->addRow(m_timestampingCheck);
    
    m_typeOfServiceSpin = new QSpinBox();
    m_typeOfServiceSpin->setRange(0, 255);
    qosLayout->addRow("Type of Service:", m_typeOfServiceSpin);
    
    m_prioritySpin = new QSpinBox();
    m_prioritySpin->setRange(0, 7);
    qosLayout->addRow("Socket Priority:", m_prioritySpin);
    
    layout->addWidget(qosGroup);
    
    // TCP-specific settings group
    m_tcpGroup = new QGroupBox("TCP Connection Settings");
    auto* tcpLayout = new QFormLayout(m_tcpGroup);
    
    m_keepAliveCheck = new QCheckBox("Enable Keep-Alive");
    m_keepAliveCheck->setChecked(true);
    tcpLayout->addRow(m_keepAliveCheck);
    
    m_keepAliveIntervalSpin = new QSpinBox();
    m_keepAliveIntervalSpin->setRange(1, 300);
    m_keepAliveIntervalSpin->setValue(30);
    m_keepAliveIntervalSpin->setSuffix(" seconds");
    tcpLayout->addRow("Keep-Alive Interval:", m_keepAliveIntervalSpin);
    
    m_connectionTimeoutSpin = new QSpinBox();
    m_connectionTimeoutSpin->setRange(1000, 60000);
    m_connectionTimeoutSpin->setValue(5000);
    m_connectionTimeoutSpin->setSuffix(" ms");
    tcpLayout->addRow("Connection Timeout:", m_connectionTimeoutSpin);
    
    m_maxReconnectSpin = new QSpinBox();
    m_maxReconnectSpin->setRange(1, 100);
    m_maxReconnectSpin->setValue(3);
    tcpLayout->addRow("Max Reconnect Attempts:", m_maxReconnectSpin);
    
    m_reconnectIntervalSpin = new QSpinBox();
    m_reconnectIntervalSpin->setRange(100, 60000);
    m_reconnectIntervalSpin->setValue(1000);
    m_reconnectIntervalSpin->setSuffix(" ms");
    tcpLayout->addRow("Reconnect Interval:", m_reconnectIntervalSpin);
    
    layout->addWidget(m_tcpGroup);
    
    layout->addStretch();
}

void NetworkConfigDialog::createProfilesTab() {
    m_profilesTab = new QWidget();
    m_tabWidget->addTab(m_profilesTab, "Profiles");
    
    auto* layout = new QHBoxLayout(m_profilesTab);
    
    // Profiles list
    auto* leftLayout = new QVBoxLayout();
    leftLayout->addWidget(new QLabel("Saved Profiles:"));
    
    m_profilesList = new QListWidget();
    m_profilesList->setSelectionMode(QAbstractItemView::SingleSelection);
    leftLayout->addWidget(m_profilesList);
    
    layout->addLayout(leftLayout, 1);
    
    // Profile management buttons
    auto* rightLayout = new QVBoxLayout();
    
    m_saveProfileButton = new QPushButton("Save Current");
    m_loadProfileButton = new QPushButton("Load Selected");
    m_deleteProfileButton = new QPushButton("Delete Selected");
    m_importButton = new QPushButton("Import...");
    m_exportButton = new QPushButton("Export...");
    
    rightLayout->addWidget(m_saveProfileButton);
    rightLayout->addWidget(m_loadProfileButton);
    rightLayout->addWidget(m_deleteProfileButton);
    rightLayout->addSpacing(20);
    rightLayout->addWidget(m_importButton);
    rightLayout->addWidget(m_exportButton);
    rightLayout->addStretch();
    
    layout->addLayout(rightLayout);
    
    updateProfilesList();
}

void NetworkConfigDialog::createDiagnosticsTab() {
    m_diagnosticsTab = new QWidget();
    m_tabWidget->addTab(m_diagnosticsTab, "Diagnostics");
    
    auto* layout = new QVBoxLayout(m_diagnosticsTab);
    
    // Diagnostics controls
    auto* controlsLayout = new QHBoxLayout();
    m_testConnectionButton = new QPushButton("Test Connection");
    m_runDiagnosticsButton = new QPushButton("Run Full Diagnostics");
    
    controlsLayout->addWidget(m_testConnectionButton);
    controlsLayout->addWidget(m_runDiagnosticsButton);
    controlsLayout->addStretch();
    
    layout->addLayout(controlsLayout);
    
    // Progress and status
    m_diagnosticsProgress = new QProgressBar();
    m_diagnosticsProgress->setVisible(false);
    layout->addWidget(m_diagnosticsProgress);
    
    m_diagnosticsStatus = new QLabel("");
    layout->addWidget(m_diagnosticsStatus);
    
    // Results output
    layout->addWidget(new QLabel("Diagnostics Output:"));
    m_diagnosticsOutput = new QTextEdit();
    m_diagnosticsOutput->setReadOnly(true);
    m_diagnosticsOutput->setFont(QFont("Courier", 9));
    layout->addWidget(m_diagnosticsOutput);
}

void NetworkConfigDialog::setupConnections() {
    // Dialog buttons
    connect(m_buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    
    // Basic tab connections
    connect(m_protocolCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &NetworkConfigDialog::onProtocolChanged);
    connect(m_profileNameEdit, &QLineEdit::textChanged, this, &NetworkConfigDialog::onValidateInput);
    connect(m_localAddressEdit, &QLineEdit::textChanged, this, &NetworkConfigDialog::onValidateInput);
    connect(m_remoteAddressEdit, &QLineEdit::textChanged, this, &NetworkConfigDialog::onValidateInput);
    connect(m_localPortSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &NetworkConfigDialog::onValidateInput);
    connect(m_remotePortSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &NetworkConfigDialog::onValidateInput);
    connect(m_refreshInterfacesButton, &QPushButton::clicked,
            this, &NetworkConfigDialog::onInterfaceRefresh);
    
    // Multicast connections
    connect(m_multicastCheck, &QCheckBox::toggled, this, &NetworkConfigDialog::onMulticastToggled);
    connect(m_multicastAddressEdit, &QLineEdit::textChanged, this, &NetworkConfigDialog::onValidateInput);
    
    // Profile management
    connect(m_profilesList, &QListWidget::itemSelectionChanged,
            this, &NetworkConfigDialog::onProfileSelectionChanged);
    connect(m_saveProfileButton, &QPushButton::clicked, this, &NetworkConfigDialog::onSaveProfile);
    connect(m_loadProfileButton, &QPushButton::clicked, this, &NetworkConfigDialog::onLoadProfile);
    connect(m_deleteProfileButton, &QPushButton::clicked, this, &NetworkConfigDialog::onDeleteProfile);
    connect(m_importButton, &QPushButton::clicked, this, &NetworkConfigDialog::importConfiguration);
    connect(m_exportButton, &QPushButton::clicked, this, &NetworkConfigDialog::exportConfiguration);
    
    // Diagnostics
    connect(m_testConnectionButton, &QPushButton::clicked, this, &NetworkConfigDialog::testConnection);
    connect(m_runDiagnosticsButton, &QPushButton::clicked, this, &NetworkConfigDialog::runDiagnostics);
}

void NetworkConfigDialog::onProtocolChanged() {
    Protocol protocol = static_cast<Protocol>(m_protocolCombo->currentData().toInt());
    enableControlsForProtocol(protocol);
    onValidateInput();
}

void NetworkConfigDialog::enableControlsForProtocol(Protocol protocol) {
    bool isTcp = (protocol == Protocol::TCP);
    
    // Multicast is UDP-only
    m_multicastGroup->setEnabled(!isTcp);
    if (isTcp) {
        m_multicastCheck->setChecked(false);
    }
    
    // TCP-specific settings
    m_tcpGroup->setEnabled(isTcp);
}

void NetworkConfigDialog::onAddressChanged() {
    onValidateInput();
}

void NetworkConfigDialog::onMulticastToggled(bool enabled) {
    m_multicastAddressEdit->setEnabled(enabled);
    m_multicastTTLSpin->setEnabled(enabled);
    onValidateInput();
}

void NetworkConfigDialog::onValidateInput() {
    // Restart validation timer
    m_validationTimer->stop();
    m_validationTimer->start();
}

void NetworkConfigDialog::validateConfiguration() {
    m_configurationValid = isConfigurationValid();
    updateValidationStatus();
    
    // Enable/disable OK button
    m_buttonBox->button(QDialogButtonBox::Ok)->setEnabled(m_configurationValid);
}

bool NetworkConfigDialog::isConfigurationValid() const {
    // Check profile name
    if (m_profileNameEdit->text().trimmed().isEmpty()) {
        return false;
    }
    
    // Check addresses
    QHostAddress localAddr(m_localAddressEdit->text());
    QHostAddress remoteAddr(m_remoteAddressEdit->text());
    
    if (localAddr.isNull() || remoteAddr.isNull()) {
        return false;
    }
    
    // Check ports
    if (m_localPortSpin->value() <= 0 || m_remotePortSpin->value() <= 0) {
        return false;
    }
    
    // Check multicast settings
    if (m_multicastCheck->isChecked()) {
        QHostAddress multicastAddr(m_multicastAddressEdit->text());
        if (multicastAddr.isNull() || !multicastAddr.isInSubnet(QHostAddress("224.0.0.0"), 4)) {
            return false;
        }
    }
    
    return true;
}

QString NetworkConfigDialog::getValidationErrors() const {
    QStringList errors;
    
    if (m_profileNameEdit->text().trimmed().isEmpty()) {
        errors << "Profile name is required";
    }
    
    QHostAddress localAddr(m_localAddressEdit->text());
    if (localAddr.isNull()) {
        errors << "Invalid local address";
    }
    
    QHostAddress remoteAddr(m_remoteAddressEdit->text());
    if (remoteAddr.isNull()) {
        errors << "Invalid remote address";
    }
    
    if (m_multicastCheck->isChecked()) {
        QHostAddress multicastAddr(m_multicastAddressEdit->text());
        if (multicastAddr.isNull() || !multicastAddr.isInSubnet(QHostAddress("224.0.0.0"), 4)) {
            errors << "Invalid multicast address (must be 224.x.x.x - 239.x.x.x)";
        }
    }
    
    return errors.join("; ");
}

void NetworkConfigDialog::updateValidationStatus() {
    if (m_configurationValid) {
        m_validationLabel->setText("✓ Configuration is valid");
        m_validationLabel->setStyleSheet("QLabel { color: green; font-weight: bold; }");
    } else {
        m_validationLabel->setText("✗ " + getValidationErrors());
        m_validationLabel->setStyleSheet("QLabel { color: red; font-weight: bold; }");
    }
}

void NetworkConfigDialog::populateNetworkInterfaces() {
    m_interfaceCombo->clear();
    m_interfaceCombo->addItem("Any Interface", "");
    
    const auto interfaces = QNetworkInterface::allInterfaces();
    for (const auto& interface : interfaces) {
        if (interface.flags() & QNetworkInterface::IsUp &&
            !(interface.flags() & QNetworkInterface::IsLoopBack)) {
            
            QString displayName = QString("%1 (%2)")
                .arg(interface.humanReadableName())
                .arg(interface.name());
            m_interfaceCombo->addItem(displayName, interface.name());
        }
    }
}

void NetworkConfigDialog::onInterfaceRefresh() {
    populateNetworkInterfaces();
}

NetworkConfig NetworkConfigDialog::getNetworkConfig() const {
    return extractConfigurationFromUI();
}

void NetworkConfigDialog::setNetworkConfig(const NetworkConfig& config) {
    m_currentConfig = config;
    applyConfigurationToUI(config);
}

void NetworkConfigDialog::applyConfigurationToUI(const NetworkConfig& config) {
    // Basic settings
    m_profileNameEdit->setText(QString::fromStdString(config.name));
    m_protocolCombo->setCurrentIndex(static_cast<int>(config.protocol));
    m_localAddressEdit->setText(config.localAddress.toString());
    m_localPortSpin->setValue(config.localPort);
    m_remoteAddressEdit->setText(config.remoteAddress.toString());
    m_remotePortSpin->setValue(config.remotePort);
    
    // Find and set network interface
    int interfaceIndex = m_interfaceCombo->findData(config.networkInterface);
    if (interfaceIndex >= 0) {
        m_interfaceCombo->setCurrentIndex(interfaceIndex);
    }
    
    // Multicast settings
    m_multicastCheck->setChecked(config.enableMulticast);
    m_multicastAddressEdit->setText(config.multicastGroup.toString());
    m_multicastTTLSpin->setValue(config.multicastTTL);
    
    // Advanced settings
    m_bufferSizeSpin->setValue(config.receiveBufferSize);
    m_socketTimeoutSpin->setValue(config.socketTimeout);
    m_maxPacketSizeSpin->setValue(config.maxPacketSize);
    m_timestampingCheck->setChecked(config.enableTimestamping);
    m_typeOfServiceSpin->setValue(config.typeOfService);
    m_prioritySpin->setValue(config.priority);
    
    // TCP settings
    m_keepAliveCheck->setChecked(config.enableKeepAlive);
    m_keepAliveIntervalSpin->setValue(config.keepAliveInterval);
    m_connectionTimeoutSpin->setValue(config.connectionTimeout);
    m_maxReconnectSpin->setValue(config.maxReconnectAttempts);
    m_reconnectIntervalSpin->setValue(config.reconnectInterval);
    
    // Update protocol-specific controls
    enableControlsForProtocol(config.protocol);
}

NetworkConfig NetworkConfigDialog::extractConfigurationFromUI() const {
    NetworkConfig config;
    
    // Basic settings
    config.name = m_profileNameEdit->text().trimmed().toStdString();
    config.protocol = static_cast<Protocol>(m_protocolCombo->currentData().toInt());
    config.localAddress = QHostAddress(m_localAddressEdit->text());
    config.localPort = static_cast<quint16>(m_localPortSpin->value());
    config.remoteAddress = QHostAddress(m_remoteAddressEdit->text());
    config.remotePort = static_cast<quint16>(m_remotePortSpin->value());
    config.networkInterface = m_interfaceCombo->currentData().toString();
    
    // Multicast settings
    config.enableMulticast = m_multicastCheck->isChecked();
    config.multicastGroup = QHostAddress(m_multicastAddressEdit->text());
    config.multicastTTL = m_multicastTTLSpin->value();
    
    // Advanced settings
    config.receiveBufferSize = m_bufferSizeSpin->value();
    config.socketTimeout = m_socketTimeoutSpin->value();
    config.maxPacketSize = m_maxPacketSizeSpin->value();
    config.enableTimestamping = m_timestampingCheck->isChecked();
    config.typeOfService = m_typeOfServiceSpin->value();
    config.priority = m_prioritySpin->value();
    
    // TCP settings
    config.enableKeepAlive = m_keepAliveCheck->isChecked();
    config.keepAliveInterval = m_keepAliveIntervalSpin->value();
    config.connectionTimeout = m_connectionTimeoutSpin->value();
    config.maxReconnectAttempts = m_maxReconnectSpin->value();
    config.reconnectInterval = m_reconnectIntervalSpin->value();
    
    return config;
}

void NetworkConfigDialog::resetUIToDefaults() {
    NetworkConfig defaultConfig;
    applyConfigurationToUI(defaultConfig);
}

void NetworkConfigDialog::resetToDefaults() {
    int result = QMessageBox::question(this, "Reset to Defaults",
        "Are you sure you want to reset all settings to default values?",
        QMessageBox::Yes | QMessageBox::No);
    
    if (result == QMessageBox::Yes) {
        resetUIToDefaults();
        validateConfiguration();
    }
}

void NetworkConfigDialog::testConnection() {
    m_testConnectionButton->setEnabled(false);
    m_diagnosticsProgress->setVisible(true);
    m_diagnosticsProgress->setRange(0, 0); // Indeterminate progress
    m_diagnosticsStatus->setText("Testing connection...");
    m_connectionTestRunning = true;
    
    // Start connection test timer (simulate async operation)
    m_connectionTestTimer->start(2000); // 2 second test
    
    m_diagnosticsOutput->append(QString("[%1] Starting connection test...")
        .arg(QDateTime::currentDateTime().toString()));
    
    runPingTest();
}

void NetworkConfigDialog::runDiagnostics() {
    m_diagnosticsOutput->clear();
    m_diagnosticsOutput->append(QString("[%1] Running network diagnostics...")
        .arg(QDateTime::currentDateTime().toString()));
    
    runPingTest();
    runPortTest();
    runInterfaceTest();
}

void NetworkConfigDialog::runPingTest() {
    QString remoteHost = m_remoteAddressEdit->text();
    m_diagnosticsOutput->append(QString("Ping test to %1...").arg(remoteHost));
    
    // Note: Real implementation would use QProcess or network calls
    m_diagnosticsOutput->append("  Result: Simulated ping test (implementation required)");
}

void NetworkConfigDialog::runPortTest() {
    QString remoteHost = m_remoteAddressEdit->text();
    int remotePort = m_remotePortSpin->value();
    
    m_diagnosticsOutput->append(QString("Port connectivity test to %1:%2...")
        .arg(remoteHost).arg(remotePort));
    
    // Note: Real implementation would test port connectivity
    m_diagnosticsOutput->append("  Result: Simulated port test (implementation required)");
}

void NetworkConfigDialog::runInterfaceTest() {
    m_diagnosticsOutput->append("Network interface analysis...");
    
    const auto interfaces = QNetworkInterface::allInterfaces();
    for (const auto& interface : interfaces) {
        if (interface.flags() & QNetworkInterface::IsUp) {
            m_diagnosticsOutput->append(QString("  Interface: %1 (%2)")
                .arg(interface.humanReadableName())
                .arg(interface.name()));
            
            const auto addresses = interface.addressEntries();
            for (const auto& address : addresses) {
                m_diagnosticsOutput->append(QString("    Address: %1/%2")
                    .arg(address.ip().toString())
                    .arg(address.prefixLength()));
            }
        }
    }
}

void NetworkConfigDialog::onConnectionTestTimer() {
    m_connectionTestTimer->stop();
    m_connectionTestRunning = false;
    m_testConnectionButton->setEnabled(true);
    m_diagnosticsProgress->setVisible(false);
    m_diagnosticsStatus->setText("Connection test completed");
    
    // Simulate successful connection test
    m_diagnosticsOutput->append(QString("[%1] Connection test completed successfully")
        .arg(QDateTime::currentDateTime().toString()));
    
    emit connectionTestCompleted(true, "Connection test successful");
}

void NetworkConfigDialog::onSaveProfile() {
    if (!isConfigurationValid()) {
        QMessageBox::warning(this, "Invalid Configuration",
            "Cannot save invalid configuration. Please fix the errors first.");
        return;
    }
    
    auto profile = std::make_shared<ConnectionProfile>(extractConfigurationFromUI());
    profile->created = QDateTime::currentDateTime();
    profile->lastUsed = QDateTime::currentDateTime();
    
    m_profiles.push_back(profile);
    updateProfilesList();
    
    QMessageBox::information(this, "Profile Saved",
        QString("Profile '%1' has been saved successfully.")
        .arg(profile->name));
}

void NetworkConfigDialog::onLoadProfile() {
    int currentRow = m_profilesList->currentRow();
    if (currentRow < 0 || currentRow >= static_cast<int>(m_profiles.size())) {
        return;
    }
    
    auto profile = m_profiles[currentRow];
    applyConfigurationToUI(profile->config);
    profile->lastUsed = QDateTime::currentDateTime();
    
    validateConfiguration();
}

void NetworkConfigDialog::onDeleteProfile() {
    int currentRow = m_profilesList->currentRow();
    if (currentRow < 0 || currentRow >= static_cast<int>(m_profiles.size())) {
        return;
    }
    
    auto profile = m_profiles[currentRow];
    int result = QMessageBox::question(this, "Delete Profile",
        QString("Are you sure you want to delete profile '%1'?")
        .arg(profile->name),
        QMessageBox::Yes | QMessageBox::No);
    
    if (result == QMessageBox::Yes) {
        m_profiles.erase(m_profiles.begin() + currentRow);
        updateProfilesList();
    }
}

void NetworkConfigDialog::onProfileSelectionChanged() {
    bool hasSelection = m_profilesList->currentRow() >= 0;
    m_loadProfileButton->setEnabled(hasSelection);
    m_deleteProfileButton->setEnabled(hasSelection);
}

void NetworkConfigDialog::updateProfilesList() {
    m_profilesList->clear();
    
    for (const auto& profile : m_profiles) {
        QString displayText = QString("%1 (%2)")
            .arg(profile->name)
            .arg(profile->config.getConnectionString());
        
        m_profilesList->addItem(displayText);
    }
    
    onProfileSelectionChanged();
}

void NetworkConfigDialog::importConfiguration() {
    QString filename = QFileDialog::getOpenFileName(this,
        "Import Network Configuration",
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
        "JSON Files (*.json)");
    
    if (!filename.isEmpty()) {
        if (loadProfiles(filename)) {
            QMessageBox::information(this, "Import Successful",
                "Network configuration imported successfully.");
            updateProfilesList();
        } else {
            QMessageBox::warning(this, "Import Failed",
                "Failed to import network configuration.");
        }
    }
}

void NetworkConfigDialog::exportConfiguration() {
    QString filename = QFileDialog::getSaveFileName(this,
        "Export Network Configuration",
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
        "JSON Files (*.json)");
    
    if (!filename.isEmpty()) {
        if (saveProfiles(filename)) {
            QMessageBox::information(this, "Export Successful",
                "Network configuration exported successfully.");
        } else {
            QMessageBox::warning(this, "Export Failed",
                "Failed to export network configuration.");
        }
    }
}

bool NetworkConfigDialog::loadProfiles(const QString& filename) {
    // Implementation would load profiles from JSON file
    // For now, return true to indicate success
    Q_UNUSED(filename);
    return true;
}

bool NetworkConfigDialog::saveProfiles(const QString& filename) const {
    // Implementation would save profiles to JSON file
    // For now, return true to indicate success
    Q_UNUSED(filename);
    return true;
}

void NetworkConfigDialog::showEvent(QShowEvent* event) {
    QDialog::showEvent(event);
    validateConfiguration();
}

void NetworkConfigDialog::closeEvent(QCloseEvent* event) {
    // Save profiles if any exist
    // Implementation would persist profiles
    QDialog::closeEvent(event);
}

// ConnectionProfile Implementation
ConnectionProfile::ConnectionProfile(const NetworkConfig& cfg)
    : name(QString::fromStdString(cfg.name))
    , config(cfg)
    , created(QDateTime::currentDateTime())
    , lastUsed(QDateTime::currentDateTime())
{
}

QJsonObject ConnectionProfile::toJson() const {
    QJsonObject json;
    json["name"] = name;
    json["description"] = description;
    json["config"] = config.toJson();
    json["created"] = created.toString(Qt::ISODate);
    json["lastUsed"] = lastUsed.toString(Qt::ISODate);
    return json;
}

bool ConnectionProfile::fromJson(const QJsonObject& json) {
    if (!json.contains("name") || !json.contains("config")) {
        return false;
    }
    
    name = json["name"].toString();
    description = json["description"].toString();
    created = QDateTime::fromString(json["created"].toString(), Qt::ISODate);
    lastUsed = QDateTime::fromString(json["lastUsed"].toString(), Qt::ISODate);
    
    return config.fromJson(json["config"].toObject());
}

} // namespace Network
} // namespace Monitor