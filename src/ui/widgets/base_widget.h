#ifndef BASE_WIDGET_H
#define BASE_WIDGET_H

#include <QWidget>
#include <QJsonObject>
#include <QJsonArray>
#include <QTimer>
#include <QMimeData>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMenu>
#include <QAction>
#include <QString>
#include <QStringList>
#include <QVarLengthArray>
#include <chrono>
#include <memory>
#include <unordered_map>
#include <vector>
#include <atomic>

// Forward declarations
namespace Monitor::Packet {
    class SubscriptionManager;
    class FieldExtractor;
    class SubscriptionManagerMock;
    class FieldExtractorMock;
    class Packet;
    using PacketPtr = std::shared_ptr<Packet>;
    using SubscriberId = uint64_t;
    using PacketId = uint32_t;
}

/**
 * @brief Abstract base class for all display widgets in the Monitor application
 * 
 * The BaseWidget provides the fundamental functionality that all display widgets share:
 * - Packet subscription management
 * - Field extraction and processing
 * - Drag-and-drop field assignment
 * - Settings persistence
 * - Update throttling for performance
 * - Context menu framework
 * 
 * This class follows the Template Method pattern, with concrete widgets implementing
 * specific display logic while BaseWidget handles the common infrastructure.
 * 
 * Performance Characteristics:
 * - Updates throttled to 60 FPS maximum
 * - Zero-copy packet handling where possible
 * - Batched field extraction
 * - Efficient variant processing
 * - Memory pool integration
 */
class BaseWidget : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief Widget update statistics
     */
    struct UpdateStatistics {
        std::atomic<uint64_t> packetsReceived{0};
        std::atomic<uint64_t> packetsProcessed{0};
        std::atomic<uint64_t> fieldsExtracted{0};
        std::atomic<uint64_t> updatesSent{0};
        std::atomic<uint64_t> averageUpdateTimeNs{0};
        std::atomic<uint64_t> lastUpdateTimestamp{0};
        
        std::chrono::steady_clock::time_point startTime;
        
        UpdateStatistics() : startTime(std::chrono::steady_clock::now()) {}
        
        double getPacketRate() const {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - startTime).count();
            if (elapsed == 0) return 0.0;
            return static_cast<double>(packetsReceived.load()) / elapsed;
        }
        
        double getUpdateRate() const {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - startTime).count();
            if (elapsed == 0) return 0.0;
            return static_cast<double>(updatesSent.load()) / elapsed;
        }
    };

    /**
     * @brief Field assignment information
     */
    struct FieldAssignment {
        QString fieldPath;          ///< Full path to field (e.g., "velocity.x")
        QString displayName;        ///< Display name for UI
        QString typeName;           ///< Field type name
        Monitor::Packet::PacketId packetId;    ///< Packet ID containing this field
        QJsonObject fieldInfo;      ///< Additional field metadata
        bool isActive;              ///< Whether field is currently active
        
        FieldAssignment() : packetId(0), isActive(true) {}
        FieldAssignment(const QString& path, Monitor::Packet::PacketId pktId)
            : fieldPath(path), displayName(path), packetId(pktId), isActive(true) {}
    };

    explicit BaseWidget(const QString& widgetId, const QString& windowTitle, QWidget* parent = nullptr);
    ~BaseWidget() override;

    // Core widget interface
    QString getWidgetId() const { return m_widgetId; }
    QString getWindowTitle() const { return m_windowTitle; }
    void setWindowTitle(const QString& title);

    // Field assignment interface
    virtual bool addField(const QString& fieldPath, Monitor::Packet::PacketId packetId, const QJsonObject& fieldInfo);
    virtual bool removeField(const QString& fieldPath);
    virtual void clearFields();
    virtual QStringList getAssignedFields() const;
    virtual int getFieldCount() const { return m_fieldAssignments.size(); }

    // Subscription management
    bool subscribeToPacket(Monitor::Packet::PacketId packetId);
    bool unsubscribeFromPacket(Monitor::Packet::PacketId packetId);
    void clearSubscriptions();
    QList<Monitor::Packet::PacketId> getSubscribedPackets() const;

    // Settings interface
    virtual QJsonObject saveSettings() const;
    virtual bool restoreSettings(const QJsonObject& settings);
    virtual void resetToDefaults();

    // Statistics interface
    const UpdateStatistics& getStatistics() const { return m_statistics; }
    void resetStatistics();

    // Update control
    void setUpdateEnabled(bool enabled);
    bool isUpdateEnabled() const { return m_updateEnabled; }
    void setMaxUpdateRate(int fps);
    int getMaxUpdateRate() const { return m_maxUpdateRate; }

public slots:
    void onSettingsChanged();
    void showSettingsDialog();
    void refreshDisplay();

signals:
    void fieldAdded(const QString& fieldPath, Monitor::Packet::PacketId packetId);
    void fieldRemoved(const QString& fieldPath);
    void fieldsCleared();
    void settingsChanged();
    void updatePerformed();
    void errorOccurred(const QString& message);

protected:
    // Template methods for concrete widgets to implement
    virtual void initializeWidget() = 0;
    virtual void updateDisplay() = 0;
    virtual void handleFieldAdded(const FieldAssignment& field) = 0;
    virtual void handleFieldRemoved(const QString& fieldPath) = 0;
    virtual void handleFieldsCleared() = 0;

    // Settings template methods
    virtual QJsonObject saveWidgetSpecificSettings() const = 0;
    virtual bool restoreWidgetSpecificSettings(const QJsonObject& settings) = 0;

    // Context menu template methods
    virtual void setupContextMenu() = 0;
    virtual void showContextMenu(const QPoint& position);

    // Drag-and-drop support
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    virtual bool canAcceptDrop(const QMimeData* mimeData) const;
    virtual bool processDrop(const QMimeData* mimeData);

    // Widget lifecycle
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

    // Utility methods for concrete widgets
    FieldAssignment* findFieldAssignment(const QString& fieldPath);
    const FieldAssignment* findFieldAssignment(const QString& fieldPath) const;
    bool hasField(const QString& fieldPath) const;

    // Access to managers
    Monitor::Packet::SubscriptionManager* getSubscriptionManager() const { return m_subscriptionManager; }
    Monitor::Packet::FieldExtractor* getFieldExtractor() const { return m_fieldExtractor; }

    // Context menu
    QMenu* getContextMenu() const { return m_contextMenu; }

private slots:
    // Internal packet processing
    void onPacketReceived(Monitor::Packet::PacketPtr packet);
    void onUpdateTimer();
    
protected:
    // Field management (accessible to derived classes)
    std::vector<FieldAssignment> m_fieldAssignments;

private:
    // Core widget identity
    QString m_widgetId;
    QString m_windowTitle;
    std::unordered_map<Monitor::Packet::PacketId, Monitor::Packet::SubscriberId> m_subscriptions;

    // Managers (not owned for real, owned for mocks in Phase 6)
    Monitor::Packet::SubscriptionManager* m_subscriptionManager;
    Monitor::Packet::FieldExtractor* m_fieldExtractor;
    
    // Mock implementations for Phase 6 testing (owned)
    Monitor::Packet::SubscriptionManagerMock* m_subscriptionManagerMock;
    Monitor::Packet::FieldExtractorMock* m_fieldExtractorMock;
    bool m_useMockImplementations;

    // Update throttling
    QTimer* m_updateTimer;
    bool m_updateEnabled;
    bool m_updatePending;
    int m_maxUpdateRate;
    std::chrono::steady_clock::time_point m_lastUpdateTime;

    // Context menu
    QMenu* m_contextMenu;
    QAction* m_settingsAction;
    QAction* m_clearFieldsAction;
    QAction* m_refreshAction;

    // Statistics
    mutable UpdateStatistics m_statistics;

    // Widget state
    bool m_isInitialized;
    bool m_isVisible;

    // Helper methods
    void setupBaseWidget();
    void setupUpdateTimer();
    void setupBaseContextMenu();
    void performUpdate();
    void processFieldExtraction();
    bool validateFieldAssignment(const QString& fieldPath, Monitor::Packet::PacketId packetId) const;
    QString generateUniqueDisplayName(const QString& baseName) const;
};

#endif // BASE_WIDGET_H