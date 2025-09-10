#pragma once

#include "../config/network_config.h"
#include "../../logging/logger.h"

#include <QDialog>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QGroupBox>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QListWidget>
#include <QTextEdit>
#include <QProgressBar>
#include <QTimer>
#include <QNetworkInterface>
#include <QHostAddress>
#include <QJsonObject>
#include <QJsonDocument>
#include <memory>

namespace Monitor {
namespace Network {

class ConnectionProfile;

/**
 * @brief Network configuration dialog for Ethernet mode setup
 * 
 * Comprehensive dialog for configuring network settings including:
 * - Protocol selection (UDP/TCP)
 * - Address and port configuration
 * - Multicast settings
 * - Performance tuning
 * - Connection profiles management
 * - Network diagnostics
 */
class NetworkConfigDialog : public QDialog {
    Q_OBJECT
    
public:
    /**
     * @brief Dialog mode enumeration
     */
    enum class Mode {
        Create,     ///< Creating new configuration
        Edit        ///< Editing existing configuration
    };
    
    /**
     * @brief Construct network configuration dialog
     * @param mode Dialog mode (create/edit)
     * @param config Initial configuration (for edit mode)
     * @param parent Parent widget
     */
    explicit NetworkConfigDialog(Mode mode = Mode::Create,
                               const NetworkConfig& config = NetworkConfig(),
                               QWidget* parent = nullptr);
    
    /**
     * @brief Destructor
     */
    ~NetworkConfigDialog() override;
    
    /**
     * @brief Get configured network settings
     */
    NetworkConfig getNetworkConfig() const;
    
    /**
     * @brief Set network configuration
     */
    void setNetworkConfig(const NetworkConfig& config);
    
    /**
     * @brief Load connection profiles from file
     */
    bool loadProfiles(const QString& filename);
    
    /**
     * @brief Save connection profiles to file
     */
    bool saveProfiles(const QString& filename) const;

public slots:
    /**
     * @brief Test network connection
     */
    void testConnection();
    
    /**
     * @brief Run network diagnostics
     */
    void runDiagnostics();
    
    /**
     * @brief Reset to default settings
     */
    void resetToDefaults();
    
    /**
     * @brief Import configuration from file
     */
    void importConfiguration();
    
    /**
     * @brief Export configuration to file
     */
    void exportConfiguration();

signals:
    /**
     * @brief Emitted when configuration is validated successfully
     */
    void configurationValidated(const NetworkConfig& config);
    
    /**
     * @brief Emitted when connection test completes
     */
    void connectionTestCompleted(bool success, const QString& message);
    
    /**
     * @brief Emitted when diagnostics complete
     */
    void diagnosticsCompleted(const QStringList& results);

protected:
    void showEvent(QShowEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

private slots:
    void onProtocolChanged();
    void onAddressChanged();
    void onMulticastToggled(bool enabled);
    void onValidateInput();
    void onProfileSelectionChanged();
    void onSaveProfile();
    void onDeleteProfile();
    void onLoadProfile();
    void onInterfaceRefresh();
    void onConnectionTestTimer();

private:
    void setupUI();
    void createBasicTab();
    void createAdvancedTab();
    void createProfilesTab();
    void createDiagnosticsTab();
    void setupConnections();
    void updateUI();
    void updateValidationStatus();
    void populateNetworkInterfaces();
    void validateConfiguration();
    bool isConfigurationValid() const;
    QString getValidationErrors() const;
    void applyConfigurationToUI(const NetworkConfig& config);
    NetworkConfig extractConfigurationFromUI() const;
    void resetUIToDefaults();
    void updateProfilesList();
    void enableControlsForProtocol(Protocol protocol);
    void runPingTest();
    void runPortTest();
    void runInterfaceTest();
    
    // Dialog properties
    Mode m_mode;
    NetworkConfig m_currentConfig;
    bool m_configurationValid;
    
    // UI Layout
    QVBoxLayout* m_mainLayout;
    QTabWidget* m_tabWidget;
    QDialogButtonBox* m_buttonBox;
    
    // Basic configuration tab
    QWidget* m_basicTab;
    QComboBox* m_protocolCombo;
    QLineEdit* m_profileNameEdit;
    QLineEdit* m_localAddressEdit;
    QSpinBox* m_localPortSpin;
    QLineEdit* m_remoteAddressEdit;
    QSpinBox* m_remotePortSpin;
    QComboBox* m_interfaceCombo;
    QPushButton* m_refreshInterfacesButton;
    
    // Multicast settings
    QGroupBox* m_multicastGroup;
    QCheckBox* m_multicastCheck;
    QLineEdit* m_multicastAddressEdit;
    QSpinBox* m_multicastTTLSpin;
    
    // Advanced configuration tab
    QWidget* m_advancedTab;
    QSpinBox* m_bufferSizeSpin;
    QSpinBox* m_socketTimeoutSpin;
    QSpinBox* m_maxPacketSizeSpin;
    QCheckBox* m_timestampingCheck;
    QSpinBox* m_typeOfServiceSpin;
    QSpinBox* m_prioritySpin;
    
    // TCP-specific settings
    QGroupBox* m_tcpGroup;
    QCheckBox* m_keepAliveCheck;
    QSpinBox* m_keepAliveIntervalSpin;
    QSpinBox* m_connectionTimeoutSpin;
    QSpinBox* m_maxReconnectSpin;
    QSpinBox* m_reconnectIntervalSpin;
    
    // Profiles management tab
    QWidget* m_profilesTab;
    QListWidget* m_profilesList;
    QPushButton* m_saveProfileButton;
    QPushButton* m_deleteProfileButton;
    QPushButton* m_loadProfileButton;
    QPushButton* m_importButton;
    QPushButton* m_exportButton;
    
    // Diagnostics tab
    QWidget* m_diagnosticsTab;
    QTextEdit* m_diagnosticsOutput;
    QPushButton* m_testConnectionButton;
    QPushButton* m_runDiagnosticsButton;
    QProgressBar* m_diagnosticsProgress;
    QLabel* m_diagnosticsStatus;
    
    // Connection profiles
    std::vector<std::shared_ptr<ConnectionProfile>> m_profiles;
    
    // Validation and testing
    QLabel* m_validationLabel;
    QTimer* m_validationTimer;
    QTimer* m_connectionTestTimer;
    bool m_connectionTestRunning;
    
    // Logging
    Logging::Logger* m_logger;
};

/**
 * @brief Connection profile for saving/loading network configurations
 */
class ConnectionProfile {
public:
    ConnectionProfile() = default;
    explicit ConnectionProfile(const NetworkConfig& config);
    
    QString name;
    QString description;
    NetworkConfig config;
    QDateTime created;
    QDateTime lastUsed;
    
    QJsonObject toJson() const;
    bool fromJson(const QJsonObject& json);
};

} // namespace Network
} // namespace Monitor