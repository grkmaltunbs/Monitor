#ifndef PERFORMANCE_DASHBOARD_H
#define PERFORMANCE_DASHBOARD_H

#include <QDialog>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QTimer>
#include <QTabWidget>
#include <QTableWidget>
#include <QScrollArea>
#include <QFrame>
#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>
#include <QtCharts/QAreaSeries>
#include <QtCharts/QSplineSeries>
#include <QtCharts/QLegend>
#include <QJsonObject>
#include <QDateTime>
#include <QBrush>
#include <QPen>
#include <memory>
#include <unordered_map>
#include <deque>
#include <chrono>

/**
 * @brief Real-time performance monitoring dashboard
 * 
 * The Performance Dashboard provides comprehensive system monitoring
 * with real-time metrics, historical trends, and performance alerts.
 * 
 * Features:
 * - System resource monitoring (CPU, Memory, Network, Disk)
 * - Per-widget performance metrics
 * - Packet processing pipeline visualization
 * - Real-time alerts and notifications
 * - Historical trend analysis
 * - Exportable performance reports
 * - Configurable update intervals
 * - Bottleneck detection and highlighting
 */
class PerformanceDashboard : public QDialog
{
    Q_OBJECT

public:
    /**
     * @brief Performance metric types
     */
    enum class MetricType {
        CPU_Usage,          // Overall CPU utilization percentage
        Memory_Usage,       // Memory consumption in MB
        Network_Throughput, // Packets/sec and MB/sec
        Disk_IO,           // Read/write operations per second
        Widget_CPU,        // Per-widget CPU usage
        Widget_Memory,     // Per-widget memory consumption
        Widget_FPS,        // Widget update rate (FPS)
        Widget_Latency,    // Widget processing latency
        Packet_Rate,       // Packet reception rate
        Parser_Throughput, // Parser processing rate
        Queue_Depth,       // Buffer queue depth
        Test_Overhead,     // Test execution overhead
        Frame_Drops,       // UI frame drops per second
        Error_Rate         // Error/failure rate
    };

    /**
     * @brief Performance alert levels
     */
    enum class AlertLevel {
        Info,      // Informational
        Warning,   // Performance degradation detected
        Error,     // Significant performance issues
        Critical   // System at risk of failure
    };

    /**
     * @brief System performance metrics structure
     */
    struct SystemMetrics {
        // System resources
        double cpuUsage = 0.0;           // 0-100%
        double memoryUsage = 0.0;        // MB
        double memoryPercent = 0.0;      // 0-100%
        double networkRxPackets = 0.0;   // Packets/sec
        double networkRxMB = 0.0;        // MB/sec
        double diskReadOps = 0.0;        // Operations/sec
        double diskWriteOps = 0.0;       // Operations/sec
        
        // Application performance
        double packetRate = 0.0;         // Packets/sec
        double parserThroughput = 0.0;   // Packets/sec
        double avgQueueDepth = 0.0;      // Average queue depth
        double testOverhead = 0.0;       // % CPU used by tests
        double frameDrops = 0.0;         // Drops/sec
        double errorRate = 0.0;          // Errors/sec
        
        // Timestamp
        QDateTime timestamp = QDateTime::currentDateTime();
    };

    /**
     * @brief Per-widget performance metrics
     */
    struct WidgetMetrics {
        QString widgetId;
        QString widgetType;
        double cpuUsage = 0.0;      // % CPU
        double memoryUsage = 0.0;   // MB
        double fps = 0.0;           // Updates per second
        double latency = 0.0;       // Processing latency (ms)
        int queueDepth = 0;         // Pending operations
        bool isActive = false;      // Currently processing data
        QDateTime lastUpdate = QDateTime::currentDateTime();
    };

    /**
     * @brief Performance alert structure
     */
    struct PerformanceAlert {
        AlertLevel level;
        MetricType metricType;
        QString message;
        QString details;
        QDateTime timestamp;
        QString widgetId;  // For widget-specific alerts
        double value;      // The metric value that triggered alert
        double threshold;  // The threshold that was exceeded
        
        PerformanceAlert() = default;
        PerformanceAlert(AlertLevel lvl, MetricType type, const QString& msg, 
                        const QString& detail = QString(), const QString& widget = QString())
            : level(lvl), metricType(type), message(msg), details(detail), 
              timestamp(QDateTime::currentDateTime()), widgetId(widget) {}
    };

    explicit PerformanceDashboard(QWidget* parent = nullptr);
    ~PerformanceDashboard() override;

    // Dashboard control
    void startMonitoring();
    void stopMonitoring();
    void pauseMonitoring();
    void resumeMonitoring();
    bool isMonitoring() const { return m_isMonitoring; }

    // Configuration
    void setUpdateInterval(int intervalMs);
    int getUpdateInterval() const { return m_updateInterval; }
    void setHistorySize(int minutes);
    int getHistorySize() const { return m_historyMinutes; }

    // Metrics management
    void updateSystemMetrics(const SystemMetrics& metrics);
    void updateWidgetMetrics(const QString& widgetId, const WidgetMetrics& metrics);
    SystemMetrics getCurrentSystemMetrics() const { return m_latestSystemMetrics; }
    WidgetMetrics getWidgetMetrics(const QString& widgetId) const;
    QStringList getMonitoredWidgets() const;

    // Alert management
    void addAlert(const PerformanceAlert& alert);
    void clearAlerts();
    void clearAlertsForWidget(const QString& widgetId);
    QList<PerformanceAlert> getActiveAlerts() const;
    QList<PerformanceAlert> getAlertsHistory() const;
    int getActiveAlertCount(AlertLevel level) const;

    // Export functionality
    bool exportReport(const QString& filePath = QString());
    QJsonObject generatePerformanceReport() const;

    // Threshold configuration
    void setThreshold(MetricType metric, AlertLevel level, double threshold);
    double getThreshold(MetricType metric, AlertLevel level) const;
    void resetThresholds();

public slots:
    // Control slots
    void onStartMonitoring();
    void onStopMonitoring();
    void onPauseMonitoring();
    void onResumeMonitoring();
    void onClearHistory();
    void onExportReport();
    void onResetThresholds();
    void onShowSettings();

    // Widget management
    void onWidgetCreated(const QString& widgetId, const QString& widgetType);
    void onWidgetDestroyed(const QString& widgetId);

signals:
    // Alert signals
    void alertTriggered(const PerformanceAlert& alert);
    void criticalAlertTriggered(const PerformanceAlert& alert);
    void alertsCleared();

    // Status signals
    void monitoringStarted();
    void monitoringStopped();
    void monitoringPaused();
    void monitoringResumed();

    // Data signals
    void metricsUpdated(const SystemMetrics& metrics);
    void widgetMetricsUpdated(const QString& widgetId, const WidgetMetrics& metrics);

protected:
    void closeEvent(QCloseEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private slots:
    // Internal update handling
    void onUpdateTimer();
    void onMetricsTimer();
    void onAlertTimer();

    // UI event handling
    void onTabChanged(int index);
    void onWidgetSelectionChanged();
    void onAlertItemClicked();
    void onClearAlert();
    void onAcknowledgeAlert();

private:
    // UI setup methods
    void setupUI();
    void setupSystemOverviewTab();
    void setupWidgetMetricsTab();
    void setupPipelineTab();
    void setupAlertsTab();
    void setupHistoryTab();
    void setupControlsToolbar();

    // Chart creation helpers
    QChartView* createSystemChart();
    QChartView* createWidgetChart();
    QChartView* createPipelineChart();
    QChartView* createTrendChart();

    // Gauge creation helpers
    QWidget* createGauge(const QString& title, const QString& unit, 
                        double minValue, double maxValue, const QColor& color);
    void updateGauge(QWidget* gauge, double value, double maxValue);
    QWidget* createStatusIndicator(const QString& title);
    void updateStatusIndicator(QWidget* indicator, bool status, const QString& text);

    // Data management
    void collectSystemMetrics();
    void collectWidgetMetrics();
    void checkThresholds();
    void updateCharts();
    void updateGauges();
    void pruneHistoryData();

    // Alert management
    void processAlert(const PerformanceAlert& alert);
    void updateAlertDisplay();
    void triggerAlertNotification(const PerformanceAlert& alert);

    // Utility methods
    QString formatMetricValue(MetricType metric, double value) const;
    QString getMetricName(MetricType metric) const;
    QString getMetricUnit(MetricType metric) const;
    QColor getAlertColor(AlertLevel level) const;
    QIcon getAlertIcon(AlertLevel level) const;

    // Configuration persistence
    void saveConfiguration();
    void loadConfiguration();

private:
    // UI components
    QTabWidget* m_tabWidget;
    
    // System Overview tab
    QWidget* m_systemOverviewTab;
    QScrollArea* m_systemScrollArea;
    std::unordered_map<QString, QWidget*> m_systemGauges;
    std::unordered_map<QString, QWidget*> m_systemIndicators;
    QChartView* m_systemChartView;
    
    // Widget Metrics tab
    QWidget* m_widgetMetricsTab;
    QTableWidget* m_widgetTable;
    QChartView* m_widgetChartView;
    
    // Pipeline tab
    QWidget* m_pipelineTab;
    QChartView* m_pipelineChartView;
    std::unordered_map<QString, QWidget*> m_pipelineIndicators;
    
    // Alerts tab
    QWidget* m_alertsTab;
    QTableWidget* m_alertsTable;
    QPushButton* m_clearAlertsButton;
    QPushButton* m_ackAlertButton;
    std::unordered_map<AlertLevel, QLabel*> m_alertCountLabels;
    
    // History tab
    QWidget* m_historyTab;
    QChartView* m_historyChartView;
    QTableWidget* m_historyTable;
    
    // Control toolbar
    QFrame* m_controlsToolbar;
    QPushButton* m_startButton;
    QPushButton* m_stopButton;
    QPushButton* m_pauseButton;
    QPushButton* m_exportButton;
    QPushButton* m_settingsButton;
    QLabel* m_statusLabel;
    QLabel* m_updateIntervalLabel;

    // Data storage
    SystemMetrics m_latestSystemMetrics;
    std::unordered_map<QString, WidgetMetrics> m_widgetMetrics;
    std::deque<SystemMetrics> m_systemHistory;
    std::unordered_map<QString, std::deque<WidgetMetrics>> m_widgetHistory;
    std::deque<PerformanceAlert> m_activeAlerts;
    std::deque<PerformanceAlert> m_alertHistory;

    // Chart data series
    std::unordered_map<QString, QLineSeries*> m_systemSeries;
    std::unordered_map<QString, QLineSeries*> m_widgetSeries;
    QLineSeries* m_pipelineSeries;

    // Configuration
    int m_updateInterval; // Update interval in milliseconds
    int m_historyMinutes; // History retention in minutes
    int m_maxAlerts;     // Maximum active alerts to display
    bool m_enableNotifications; // Enable alert notifications
    std::unordered_map<MetricType, std::unordered_map<AlertLevel, double>> m_thresholds;

    // State management
    bool m_isMonitoring;
    bool m_isPaused;
    QTimer* m_updateTimer;
    QTimer* m_metricsTimer;
    QTimer* m_alertTimer;
    std::chrono::steady_clock::time_point m_startTime;

    // Constants
    static const int DEFAULT_UPDATE_INTERVAL_MS = 1000; // 1 second
    static const int DEFAULT_HISTORY_MINUTES = 5;       // 5 minutes
    static const int MAX_CHART_POINTS = 300;            // Maximum points per chart
    static const int MAX_ALERT_HISTORY = 1000;          // Maximum alert history
};

#endif // PERFORMANCE_DASHBOARD_H