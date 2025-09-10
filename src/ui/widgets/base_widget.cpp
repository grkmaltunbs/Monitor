#include "base_widget.h"
#include "../../packet/routing/subscription_manager.h"
#include "../../packet/processing/field_extractor.h"
#include "../../packet/core/packet.h"
#include "../../core/application.h"
#include "../../logging/logger.h"
#include "../../profiling/profiler.h"

// Mock implementations for Phase 6 testing
#include "../../packet/routing/subscription_manager_mock.h"
#include "../../packet/processing/field_extractor_mock.h"

#include <QApplication>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QMouseEvent>
#include <QMimeData>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QJsonDocument>
#include <algorithm>

BaseWidget::BaseWidget(const QString& widgetId, const QString& windowTitle, QWidget* parent)
    : QWidget(parent)
    , m_widgetId(widgetId)
    , m_windowTitle(windowTitle)
    , m_subscriptionManager(nullptr)
    , m_fieldExtractor(nullptr)
    , m_subscriptionManagerMock(nullptr)
    , m_fieldExtractorMock(nullptr)
    , m_useMockImplementations(true) // Use mocks for Phase 6
    , m_updateTimer(new QTimer(this))
    , m_updateEnabled(true)
    , m_updatePending(false)
    , m_maxUpdateRate(60) // 60 FPS default
    , m_lastUpdateTime(std::chrono::steady_clock::now())
    , m_contextMenu(new QMenu(this))
    , m_settingsAction(nullptr)
    , m_clearFieldsAction(nullptr)
    , m_refreshAction(nullptr)
    , m_isInitialized(false)
    , m_isVisible(false)
{
    PROFILE_SCOPE("BaseWidget::constructor");
    
    setupBaseWidget();
    setupUpdateTimer();
    setupBaseContextMenu();
    
    // Phase 6: Use mock implementations for testing, will be replaced with real ones in Phase 4
    if (m_useMockImplementations) {
        // Create mock implementations
        m_subscriptionManagerMock = new Monitor::Packet::SubscriptionManagerMock(this);
        m_fieldExtractorMock = new Monitor::Packet::FieldExtractorMock();
        
        Monitor::Logging::Logger::instance()->debug("BaseWidget", 
            QString("Widget '%1' created with mock implementations for Phase 6 testing").arg(m_widgetId));
    } else {
        // TODO: Get real managers from application when packet processing system is integrated
        m_subscriptionManager = nullptr;  // Will be connected in future integration
        m_fieldExtractor = nullptr;       // Will be connected in future integration
    }
    
    // Enable drag and drop
    setAcceptDrops(true);
    
    setWindowTitle(windowTitle);
}

BaseWidget::~BaseWidget() {
    PROFILE_SCOPE("BaseWidget::destructor");
    
    clearSubscriptions();
    
    // Clear field assignments without calling virtual methods during destruction
    m_fieldAssignments.clear();
    
    // Clean up mock implementations
    if (m_useMockImplementations) {
        delete m_fieldExtractorMock;
        m_fieldExtractorMock = nullptr;
        // m_subscriptionManagerMock is managed by Qt parent-child relationship
        m_subscriptionManagerMock = nullptr;
    }
    
    Monitor::Logging::Logger::instance()->debug("BaseWidget", 
        QString("Widget '%1' destroyed").arg(m_widgetId));
}

void BaseWidget::setWindowTitle(const QString& title) {
    if (m_windowTitle != title) {
        m_windowTitle = title;
        QWidget::setWindowTitle(title);
        
        Monitor::Logging::Logger::instance()->debug("BaseWidget", 
            QString("Widget '%1' title changed to '%2'").arg(m_widgetId).arg(title));
    }
}

bool BaseWidget::addField(const QString& fieldPath, Monitor::Packet::PacketId packetId, const QJsonObject& fieldInfo) {
    PROFILE_SCOPE("BaseWidget::addField");
    
    if (fieldPath.isEmpty() || packetId == 0) {
        Monitor::Logging::Logger::instance()->warning("BaseWidget", 
            QString("Invalid field assignment: path='%1', packetId=%2").arg(fieldPath).arg(packetId));
        return false;
    }
    
    // Check if field already exists
    if (hasField(fieldPath)) {
        Monitor::Logging::Logger::instance()->warning("BaseWidget", 
            QString("Field '%1' already exists in widget '%2'").arg(fieldPath).arg(m_widgetId));
        return false;
    }
    
    // Validate field assignment
    if (!validateFieldAssignment(fieldPath, packetId)) {
        return false;
    }
    
    // Create field assignment
    FieldAssignment assignment(fieldPath, packetId);
    assignment.displayName = generateUniqueDisplayName(fieldPath);
    assignment.fieldInfo = fieldInfo;
    assignment.typeName = fieldInfo.value("type").toString();
    
    // Add to collection
    m_fieldAssignments.push_back(assignment);
    
    // Subscribe to packet if not already subscribed
    subscribeToPacket(packetId);
    
    // Notify concrete widget
    handleFieldAdded(assignment);
    
    // Update statistics
    m_statistics.fieldsExtracted++;
    
    Monitor::Logging::Logger::instance()->info("BaseWidget", 
        QString("Field '%1' added to widget '%2' (packet ID %3)")
        .arg(fieldPath).arg(m_widgetId).arg(packetId));
    
    emit fieldAdded(fieldPath, packetId);
    return true;
}

bool BaseWidget::removeField(const QString& fieldPath) {
    PROFILE_SCOPE("BaseWidget::removeField");
    
    auto it = std::find_if(m_fieldAssignments.begin(), m_fieldAssignments.end(),
        [&fieldPath](const FieldAssignment& assignment) {
            return assignment.fieldPath == fieldPath;
        });
    
    if (it == m_fieldAssignments.end()) {
        Monitor::Logging::Logger::instance()->warning("BaseWidget", 
            QString("Field '%1' not found in widget '%2'").arg(fieldPath).arg(m_widgetId));
        return false;
    }
    
    Monitor::Packet::PacketId packetId = it->packetId;
    
    // Remove from collection
    m_fieldAssignments.erase(it);
    
    // Check if we need to unsubscribe from packet
    bool hasOtherFieldsFromPacket = std::any_of(m_fieldAssignments.begin(), m_fieldAssignments.end(),
        [packetId](const FieldAssignment& assignment) {
            return assignment.packetId == packetId;
        });
    
    if (!hasOtherFieldsFromPacket) {
        unsubscribeFromPacket(packetId);
    }
    
    // Notify concrete widget
    handleFieldRemoved(fieldPath);
    
    Monitor::Logging::Logger::instance()->info("BaseWidget", 
        QString("Field '%1' removed from widget '%2'").arg(fieldPath).arg(m_widgetId));
    
    emit fieldRemoved(fieldPath);
    return true;
}

void BaseWidget::clearFields() {
    PROFILE_SCOPE("BaseWidget::clearFields");
    
    if (m_fieldAssignments.empty()) {
        return;
    }
    
    // Clear all subscriptions
    clearSubscriptions();
    
    // Clear field assignments
    m_fieldAssignments.clear();
    
    // Notify concrete widget
    handleFieldsCleared();
    
    Monitor::Logging::Logger::instance()->info("BaseWidget", 
        QString("All fields cleared from widget '%1'").arg(m_widgetId));
    
    emit fieldsCleared();
}

QStringList BaseWidget::getAssignedFields() const {
    QStringList fields;
    fields.reserve(static_cast<int>(m_fieldAssignments.size()));
    
    for (const auto& assignment : m_fieldAssignments) {
        fields.append(assignment.fieldPath);
    }
    
    return fields;
}

bool BaseWidget::subscribeToPacket(Monitor::Packet::PacketId packetId) {
    if (m_useMockImplementations) {
        // Use mock subscription manager for Phase 6
        if (!m_subscriptionManagerMock) {
            Monitor::Logging::Logger::instance()->error("BaseWidget", "Mock SubscriptionManager not available");
            return false;
        }
        
        // Check if already subscribed
        if (m_subscriptions.find(packetId) != m_subscriptions.end()) {
            return true; // Already subscribed
        }
        
        // Create mock subscription
        std::string subscriberName = QString("Widget_%1").arg(m_widgetId).toStdString();
        auto subscriptionId = m_subscriptionManagerMock->subscribe(subscriberName, packetId);
        
        if (subscriptionId == 0) {
            Monitor::Logging::Logger::instance()->error("BaseWidget", 
                QString("Failed to subscribe widget '%1' to packet ID %2 (mock)").arg(m_widgetId).arg(packetId));
            return false;
        }
        
        m_subscriptions[packetId] = subscriptionId;
        
        Monitor::Logging::Logger::instance()->debug("BaseWidget", 
            QString("Widget '%1' subscribed to packet ID %2 (mock)").arg(m_widgetId).arg(packetId));
        
        return true;
        
    } else {
        // Use real subscription manager for Phase 4+
        if (!m_subscriptionManager) {
            Monitor::Logging::Logger::instance()->error("BaseWidget", "SubscriptionManager not available");
            return false;
        }
        
        // Check if already subscribed
        if (m_subscriptions.find(packetId) != m_subscriptions.end()) {
            return true; // Already subscribed
        }
        
        // Create subscription
        std::string subscriberName = QString("Widget_%1").arg(m_widgetId).toStdString();
        auto callback = [this](Monitor::Packet::PacketPtr packet) {
            // Schedule update on main thread
            QMetaObject::invokeMethod(this, [this, packet]() {
                onPacketReceived(packet);
            }, Qt::QueuedConnection);
        };
        
        auto subscriptionId = m_subscriptionManager->subscribe(subscriberName, packetId, callback);
        if (subscriptionId == 0) {
            Monitor::Logging::Logger::instance()->error("BaseWidget", 
                QString("Failed to subscribe widget '%1' to packet ID %2").arg(m_widgetId).arg(packetId));
            return false;
        }
        
        m_subscriptions[packetId] = subscriptionId;
        
        Monitor::Logging::Logger::instance()->debug("BaseWidget", 
            QString("Widget '%1' subscribed to packet ID %2").arg(m_widgetId).arg(packetId));
        
        return true;
    }
}

bool BaseWidget::unsubscribeFromPacket(Monitor::Packet::PacketId packetId) {
    auto it = m_subscriptions.find(packetId);
    if (it == m_subscriptions.end()) {
        return true; // Not subscribed
    }
    
    if (m_useMockImplementations) {
        if (m_subscriptionManagerMock) {
            m_subscriptionManagerMock->unsubscribe(it->second);
        }
    } else {
        if (m_subscriptionManager) {
            m_subscriptionManager->unsubscribe(it->second);
        }
    }
    
    m_subscriptions.erase(it);
    
    Monitor::Logging::Logger::instance()->debug("BaseWidget", 
        QString("Widget '%1' unsubscribed from packet ID %2").arg(m_widgetId).arg(packetId));
    
    return true;
}

void BaseWidget::clearSubscriptions() {
    if (m_useMockImplementations) {
        if (!m_subscriptionManagerMock) {
            return;
        }
        for (const auto& pair : m_subscriptions) {
            m_subscriptionManagerMock->unsubscribe(pair.second);
        }
    } else {
        if (!m_subscriptionManager) {
            return;
        }
        for (const auto& pair : m_subscriptions) {
            m_subscriptionManager->unsubscribe(pair.second);
        }
    }
    
    m_subscriptions.clear();
    
    Monitor::Logging::Logger::instance()->debug("BaseWidget", 
        QString("Widget '%1' cleared all subscriptions").arg(m_widgetId));
}

QList<Monitor::Packet::PacketId> BaseWidget::getSubscribedPackets() const {
    QList<Monitor::Packet::PacketId> packets;
    packets.reserve(static_cast<int>(m_subscriptions.size()));
    
    for (const auto& pair : m_subscriptions) {
        packets.append(pair.first);
    }
    
    return packets;
}

QJsonObject BaseWidget::saveSettings() const {
    QJsonObject settings;
    
    // Base widget settings
    settings["widgetId"] = m_widgetId;
    settings["windowTitle"] = m_windowTitle;
    settings["updateEnabled"] = m_updateEnabled;
    settings["maxUpdateRate"] = m_maxUpdateRate;
    
    // Field assignments
    QJsonArray fieldsArray;
    for (const auto& assignment : m_fieldAssignments) {
        QJsonObject fieldObj;
        fieldObj["fieldPath"] = assignment.fieldPath;
        fieldObj["displayName"] = assignment.displayName;
        fieldObj["typeName"] = assignment.typeName;
        fieldObj["packetId"] = static_cast<int>(assignment.packetId);
        fieldObj["isActive"] = assignment.isActive;
        fieldObj["fieldInfo"] = assignment.fieldInfo;
        fieldsArray.append(fieldObj);
    }
    settings["fields"] = fieldsArray;
    
    // Widget-specific settings
    settings["widgetSpecific"] = saveWidgetSpecificSettings();
    
    return settings;
}

bool BaseWidget::restoreSettings(const QJsonObject& settings) {
    PROFILE_SCOPE("BaseWidget::restoreSettings");
    
    // Restore base settings
    QJsonValue updateEnabledValue = settings.value("updateEnabled");
    m_updateEnabled = updateEnabledValue.isUndefined() ? true : updateEnabledValue.toBool();
    
    QJsonValue maxUpdateRateValue = settings.value("maxUpdateRate");
    m_maxUpdateRate = maxUpdateRateValue.isUndefined() ? 60 : maxUpdateRateValue.toInt();
    setMaxUpdateRate(m_maxUpdateRate);
    
    QString title = settings.value("windowTitle").toString();
    if (!title.isEmpty()) {
        setWindowTitle(title);
    }
    
    // Clear existing fields
    clearFields();
    
    // Restore field assignments
    QJsonArray fieldsArray = settings.value("fields").toArray();
    for (const auto& value : fieldsArray) {
        QJsonObject fieldObj = value.toObject();
        
        QString fieldPath = fieldObj.value("fieldPath").toString();
        auto packetId = static_cast<Monitor::Packet::PacketId>(fieldObj.value("packetId").toInt());
        QJsonObject fieldInfo = fieldObj.value("fieldInfo").toObject();
        
        if (!fieldPath.isEmpty() && packetId != 0) {
            addField(fieldPath, packetId, fieldInfo);
            
            // Restore additional field properties
            auto* assignment = findFieldAssignment(fieldPath);
            if (assignment) {
                QJsonValue displayNameValue = fieldObj.value("displayName");
                assignment->displayName = displayNameValue.isUndefined() ? fieldPath : displayNameValue.toString();
                assignment->typeName = fieldObj.value("typeName").toString();
                
                QJsonValue isActiveValue = fieldObj.value("isActive");
                assignment->isActive = isActiveValue.isUndefined() ? true : isActiveValue.toBool();
            }
        }
    }
    
    // Restore widget-specific settings
    QJsonObject widgetSpecific = settings.value("widgetSpecific").toObject();
    if (!widgetSpecific.isEmpty()) {
        if (!restoreWidgetSpecificSettings(widgetSpecific)) {
            Monitor::Logging::Logger::instance()->warning("BaseWidget", 
                QString("Failed to restore widget-specific settings for '%1'").arg(m_widgetId));
        }
    }
    
    Monitor::Logging::Logger::instance()->info("BaseWidget", 
        QString("Settings restored for widget '%1'").arg(m_widgetId));
    
    emit settingsChanged();
    return true;
}

void BaseWidget::resetToDefaults() {
    clearFields();
    m_updateEnabled = true;
    setMaxUpdateRate(60);
    resetStatistics();
    
    Monitor::Logging::Logger::instance()->info("BaseWidget", 
        QString("Widget '%1' reset to defaults").arg(m_widgetId));
}

void BaseWidget::resetStatistics() {
    m_statistics.packetsReceived = 0;
    m_statistics.packetsProcessed = 0;
    m_statistics.fieldsExtracted = 0;
    m_statistics.updatesSent = 0;
    m_statistics.averageUpdateTimeNs = 0;
    m_statistics.lastUpdateTimestamp = 0;
    m_statistics.startTime = std::chrono::steady_clock::now();
}

void BaseWidget::setUpdateEnabled(bool enabled) {
    if (m_updateEnabled != enabled) {
        m_updateEnabled = enabled;
        
        if (enabled && m_updatePending) {
            // Trigger pending update
            QTimer::singleShot(0, this, &BaseWidget::onUpdateTimer);
        }
        
        Monitor::Logging::Logger::instance()->debug("BaseWidget", 
            QString("Widget '%1' updates %2").arg(m_widgetId).arg(enabled ? "enabled" : "disabled"));
    }
}

void BaseWidget::setMaxUpdateRate(int fps) {
    fps = qBound(1, fps, 120); // Clamp between 1 and 120 FPS
    if (m_maxUpdateRate != fps) {
        m_maxUpdateRate = fps;
        
        // Update timer interval
        int intervalMs = 1000 / fps;
        m_updateTimer->setInterval(intervalMs);
        
        Monitor::Logging::Logger::instance()->debug("BaseWidget", 
            QString("Widget '%1' max update rate set to %2 FPS").arg(m_widgetId).arg(fps));
    }
}

void BaseWidget::onSettingsChanged() {
    refreshDisplay();
}

void BaseWidget::showSettingsDialog() {
    // This will be implemented by concrete widgets or through a settings manager
    Monitor::Logging::Logger::instance()->debug("BaseWidget", 
        QString("Settings dialog requested for widget '%1'").arg(m_widgetId));
}

void BaseWidget::refreshDisplay() {
    if (m_updateEnabled && m_isVisible) {
        performUpdate();
    }
}

void BaseWidget::setupBaseWidget() {
    // Set basic properties
    setAttribute(Qt::WA_DeleteOnClose, true);
    setMinimumSize(200, 100);
    
    // Set object name for easier debugging
    setObjectName(QString("BaseWidget_%1").arg(m_widgetId));
}

void BaseWidget::setupUpdateTimer() {
    m_updateTimer->setSingleShot(true);
    connect(m_updateTimer, &QTimer::timeout, this, &BaseWidget::onUpdateTimer);
    
    // Set initial interval
    setMaxUpdateRate(m_maxUpdateRate);
}

void BaseWidget::setupBaseContextMenu() {
    m_settingsAction = m_contextMenu->addAction("Settings...", this, &BaseWidget::showSettingsDialog);
    m_contextMenu->addSeparator();
    m_clearFieldsAction = m_contextMenu->addAction("Clear Fields", this, &BaseWidget::clearFields);
    m_refreshAction = m_contextMenu->addAction("Refresh", this, &BaseWidget::refreshDisplay);
    
    // Set context menu policy
    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, &QWidget::customContextMenuRequested, this, &BaseWidget::showContextMenu);
}

void BaseWidget::showContextMenu(const QPoint& position) {
    setupContextMenu(); // Let concrete widget add specific items
    
    if (!m_contextMenu->isEmpty()) {
        m_contextMenu->exec(mapToGlobal(position));
    }
}

void BaseWidget::dragEnterEvent(QDragEnterEvent* event) {
    if (canAcceptDrop(event->mimeData())) {
        event->acceptProposedAction();
    } else {
        event->ignore();
    }
}

void BaseWidget::dragMoveEvent(QDragMoveEvent* event) {
    if (canAcceptDrop(event->mimeData())) {
        event->acceptProposedAction();
    } else {
        event->ignore();
    }
}

void BaseWidget::dropEvent(QDropEvent* event) {
    if (processDrop(event->mimeData())) {
        event->acceptProposedAction();
    } else {
        event->ignore();
    }
}

bool BaseWidget::canAcceptDrop(const QMimeData* mimeData) const {
    // Accept drops with field path information
    return mimeData && mimeData->hasFormat("application/x-monitor-field");
}

bool BaseWidget::processDrop(const QMimeData* mimeData) {
    if (!mimeData || !mimeData->hasFormat("application/x-monitor-field")) {
        return false;
    }
    
    QByteArray data = mimeData->data("application/x-monitor-field");
    QJsonDocument doc = QJsonDocument::fromJson(data);
    QJsonObject fieldObj = doc.object();
    
    QString fieldPath = fieldObj.value("fieldPath").toString();
    auto packetId = static_cast<Monitor::Packet::PacketId>(fieldObj.value("packetId").toInt());
    QJsonObject fieldInfo = fieldObj.value("fieldInfo").toObject();
    
    if (fieldPath.isEmpty() || packetId == 0) {
        Monitor::Logging::Logger::instance()->warning("BaseWidget", 
            QString("Invalid drop data for widget '%1'").arg(m_widgetId));
        return false;
    }
    
    return addField(fieldPath, packetId, fieldInfo);
}

void BaseWidget::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    
    m_isVisible = true;
    
    if (!m_isInitialized) {
        initializeWidget();
        m_isInitialized = true;
    }
    
    // Start updates if enabled
    if (m_updateEnabled && !m_fieldAssignments.empty()) {
        refreshDisplay();
    }
}

void BaseWidget::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    
    m_isVisible = false;
    
    // Stop pending updates
    if (m_updateTimer->isActive()) {
        m_updateTimer->stop();
    }
}

void BaseWidget::closeEvent(QCloseEvent* event) {
    // Clean up subscriptions before closing
    clearSubscriptions();
    
    QWidget::closeEvent(event);
}

BaseWidget::FieldAssignment* BaseWidget::findFieldAssignment(const QString& fieldPath) {
    auto it = std::find_if(m_fieldAssignments.begin(), m_fieldAssignments.end(),
        [&fieldPath](const FieldAssignment& assignment) {
            return assignment.fieldPath == fieldPath;
        });
    
    return (it != m_fieldAssignments.end()) ? &(*it) : nullptr;
}

const BaseWidget::FieldAssignment* BaseWidget::findFieldAssignment(const QString& fieldPath) const {
    auto it = std::find_if(m_fieldAssignments.begin(), m_fieldAssignments.end(),
        [&fieldPath](const FieldAssignment& assignment) {
            return assignment.fieldPath == fieldPath;
        });
    
    return (it != m_fieldAssignments.end()) ? &(*it) : nullptr;
}

bool BaseWidget::hasField(const QString& fieldPath) const {
    return findFieldAssignment(fieldPath) != nullptr;
}

void BaseWidget::onPacketReceived(Monitor::Packet::PacketPtr packet) {
    if (!packet || !m_updateEnabled || !m_isVisible) {
        return;
    }
    
    m_statistics.packetsReceived++;
    
    // Schedule update if not already pending
    if (!m_updatePending) {
        m_updatePending = true;
        
        // Check if enough time has passed since last update
        auto now = std::chrono::steady_clock::now();
        auto timeSinceLastUpdate = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastUpdateTime).count();
        int minInterval = 1000 / m_maxUpdateRate;
        
        if (timeSinceLastUpdate >= minInterval) {
            // Update immediately
            QTimer::singleShot(0, this, &BaseWidget::onUpdateTimer);
        } else {
            // Schedule update for later
            int delay = minInterval - static_cast<int>(timeSinceLastUpdate);
            m_updateTimer->start(delay);
        }
    }
}

void BaseWidget::onUpdateTimer() {
    if (m_updateEnabled && m_isVisible) {
        performUpdate();
    }
}

void BaseWidget::performUpdate() {
    PROFILE_SCOPE("BaseWidget::performUpdate");
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    m_updatePending = false;
    m_lastUpdateTime = std::chrono::steady_clock::now();
    
    // Process any pending field extractions
    processFieldExtraction();
    
    // Call concrete widget update
    updateDisplay();
    
    // Update statistics
    auto endTime = std::chrono::high_resolution_clock::now();
    auto updateTime = std::chrono::duration_cast<std::chrono::nanoseconds>(endTime - startTime).count();
    
    m_statistics.updatesSent++;
    m_statistics.lastUpdateTimestamp = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()
        ).count());
    
    // Update average
    uint64_t currentAvg = m_statistics.averageUpdateTimeNs.load();
    uint64_t newAvg = (currentAvg + static_cast<uint64_t>(updateTime)) / 2;
    m_statistics.averageUpdateTimeNs.store(newAvg);
    
    emit updatePerformed();
}

void BaseWidget::processFieldExtraction() {
    // This could be optimized with batched extraction in the future
    // For now, extraction is handled on-demand by concrete widgets
}

bool BaseWidget::validateFieldAssignment(const QString& fieldPath, Monitor::Packet::PacketId packetId) const {
    Q_UNUSED(fieldPath);  // Will be used when field validation is implemented
    
    if (m_useMockImplementations) {
        // Use mock field extractor for Phase 6
        if (!m_fieldExtractorMock) {
            return false;
        }
        return m_fieldExtractorMock->hasFieldMap(packetId);
    } else {
        // Use real field extractor for Phase 4+
        if (!m_fieldExtractor) {
            return false;
        }
        return m_fieldExtractor->hasFieldMap(packetId);
    }
}

QString BaseWidget::generateUniqueDisplayName(const QString& baseName) const {
    QString name = baseName;
    int counter = 1;
    
    // Ensure unique display name
    while (std::any_of(m_fieldAssignments.begin(), m_fieldAssignments.end(),
        [&name](const FieldAssignment& assignment) {
            return assignment.displayName == name;
        })) {
        name = QString("%1_%2").arg(baseName).arg(counter++);
    }
    
    return name;
}