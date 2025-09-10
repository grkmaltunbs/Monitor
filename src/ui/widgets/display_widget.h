#ifndef DISPLAY_WIDGET_H
#define DISPLAY_WIDGET_H

#include "base_widget.h"
#include <QVariant>
#include <QColor>
#include <QFont>
#include <QString>
#include <QStringList>
#include <QJsonObject>
#include <QDialog>
#include <functional>
#include <unordered_map>
#include <variant>
#include <chrono>

// Forward declarations
namespace Monitor::Packet {
    class FieldExtractor;
}

/**
 * @brief Concrete base class for display widgets that show field values
 * 
 * DisplayWidget provides common functionality for widgets that display packet field values:
 * - Data transformation pipeline (type conversion, mathematical operations, functions)
 * - Trigger condition evaluation
 * - Display formatting (colors, fonts, prefixes/suffixes)
 * - Field value caching for performance
 * - Common settings dialogs
 * 
 * This class implements the display-related template methods from BaseWidget
 * while leaving the actual UI display to concrete widget implementations.
 */
class DisplayWidget : public BaseWidget
{
    Q_OBJECT

public:
    /**
     * @brief Data transformation types
     */
    enum class TransformationType {
        None,
        TypeConversion,     // Convert between data types
        Mathematical,       // Arithmetic operations
        Functional,         // Statistical/analytical functions
        Formatting         // Text formatting operations
    };

    /**
     * @brief Type conversion options
     */
    enum class ConversionType {
        NoConversion,
        ToInteger,
        ToDouble,
        ToHexadecimal,
        ToBinary,
        ToString,
        ToBoolean
    };

    /**
     * @brief Mathematical transformation operations
     */
    enum class MathOperation {
        None,
        Multiply,
        Divide,
        Add,
        Subtract,
        Modulo,
        Power,
        Absolute,
        Negate
    };

    /**
     * @brief Functional transformation operations
     */
    enum class FunctionType {
        None,
        Difference,         // Current - Previous
        CumulativeSum,      // Running total
        MovingAverage,      // Average over N values
        Minimum,           // Min over N values
        Maximum,           // Max over N values
        Range,             // Max - Min over N values
        StandardDeviation  // StdDev over N values
    };

    /**
     * @brief Field display configuration
     */
    struct DisplayConfig {
        // Type conversion
        ConversionType conversion = ConversionType::NoConversion;
        
        // Mathematical transformation
        MathOperation mathOp = MathOperation::None;
        double mathOperand = 1.0;
        
        // Functional transformation
        FunctionType function = FunctionType::None;
        int functionWindow = 10;    // Window size for functions
        
        // Display formatting
        QString prefix;
        QString suffix;
        int decimalPlaces = 2;
        bool useThousandsSeparator = false;
        bool useScientificNotation = false;
        
        // Visual formatting
        QColor textColor = QColor(Qt::black);
        QColor backgroundColor = QColor(Qt::transparent);
        QFont font;
        bool fontSet = false;
        
        // Field visibility and naming
        bool isVisible = true;
        QString customDisplayName;
        
        DisplayConfig() {
            font = QFont(); // Default font
        }
    };

    /**
     * @brief Trigger condition for conditional display updates
     */
    struct TriggerCondition {
        bool enabled = false;
        QString expression;         // Condition expression
        bool lastResult = true;     // Result of last evaluation
        std::chrono::steady_clock::time_point lastEvaluation;
        
        // Parsed condition components (for performance)
        struct ParsedCondition {
            QString fieldPath;
            QString operator_;
            QVariant value;
            bool isValid = false;
        } parsed;
        
        TriggerCondition() : lastEvaluation(std::chrono::steady_clock::now()) {}
    };

    /**
     * @brief Field value with transformation history
     */
    struct FieldValue {
        QVariant currentValue;
        QVariant transformedValue;
        std::vector<QVariant> history;  // For functions that need history
        std::chrono::steady_clock::time_point timestamp;
        bool hasNewValue = false;
        
        // Add value to history with window size limit
        void addToHistory(const QVariant& value, int maxSize = 100) {
            history.push_back(value);
            if (history.size() > static_cast<size_t>(maxSize)) {
                history.erase(history.begin());
            }
        }
        
        FieldValue() : timestamp(std::chrono::steady_clock::now()) {}
    };

    explicit DisplayWidget(const QString& widgetId, const QString& windowTitle, QWidget* parent = nullptr);
    ~DisplayWidget() override;

    // Display configuration
    void setDisplayConfig(const QString& fieldPath, const DisplayConfig& config);
    DisplayConfig getDisplayConfig(const QString& fieldPath) const;
    void resetDisplayConfig(const QString& fieldPath);

    // Trigger conditions
    void setTriggerCondition(const TriggerCondition& condition);
    TriggerCondition getTriggerCondition() const { return m_triggerCondition; }
    void clearTriggerCondition();

    // Value access for concrete widgets
    QVariant getFieldValue(const QString& fieldPath) const;
    QVariant getTransformedValue(const QString& fieldPath) const;
    QString getFormattedValue(const QString& fieldPath) const;
    bool hasNewValue(const QString& fieldPath) const;
    void markValueProcessed(const QString& fieldPath);

    // Display formatting helpers
    static QString formatValue(const QVariant& value, const DisplayConfig& config);
    static QVariant convertValue(const QVariant& input, ConversionType conversion);
    static QVariant applyMathOperation(const QVariant& input, MathOperation op, double operand);
    static QVariant applyFunction(const std::vector<QVariant>& history, FunctionType function);

protected:
    // BaseWidget implementation
    void initializeWidget() override;
    void updateDisplay() override;
    void handleFieldAdded(const FieldAssignment& field) override;
    void handleFieldRemoved(const QString& fieldPath) override;
    void handleFieldsCleared() override;

    // Settings implementation
    QJsonObject saveWidgetSpecificSettings() const override;
    bool restoreWidgetSpecificSettings(const QJsonObject& settings) override;

    // Context menu implementation
    void setupContextMenu() override;

    // Template methods for concrete display widgets
    virtual void updateFieldDisplay(const QString& fieldPath, const QVariant& value) = 0;
    virtual void clearFieldDisplay(const QString& fieldPath) = 0;
    virtual void refreshAllDisplays() = 0;

    // Utility methods for concrete widgets
    QStringList getVisibleFields() const;
    int getVisibleFieldCount() const;
    bool shouldUpdateDisplay() const;

private slots:
    void onShowDisplayConfigDialog();
    void onShowTriggerDialog();
    void onResetFieldFormatting();

private:
    // Field value storage
    std::unordered_map<QString, FieldValue> m_fieldValues;
    std::unordered_map<QString, DisplayConfig> m_displayConfigs;

    // Trigger condition
    TriggerCondition m_triggerCondition;

    // Context menu actions
    QAction* m_displayConfigAction;
    QAction* m_triggerAction;
    QAction* m_resetFormattingAction;

    // Helper methods
    void extractAndUpdateFieldValues();
    void processFieldTransformations();
    bool evaluateTriggerCondition();
    void updateFieldValue(const QString& fieldPath, const QVariant& rawValue);
    QVariant transformValue(const QString& fieldPath, const QVariant& rawValue);
    void ensureDisplayConfig(const QString& fieldPath);
    
    // Parsing and evaluation
    bool parseConditionExpression(const QString& expression, TriggerCondition::ParsedCondition& parsed);
    bool evaluateParsedCondition(const TriggerCondition::ParsedCondition& condition);
    QVariant getFieldValueForCondition(const QString& fieldPath) const;
};

// TODO: Settings dialogs will be implemented in Phase 6 widget settings system
// Forward declarations for future implementation
// class DisplayConfigDialog;
// class TriggerConditionDialog;

#endif // DISPLAY_WIDGET_H