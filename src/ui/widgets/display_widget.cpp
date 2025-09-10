#include "display_widget.h"
#include "../../packet/processing/field_extractor.h"
#include "../../logging/logger.h"
#include "../../profiling/profiler.h"

#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFormLayout>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QColorDialog>
#include <QFontDialog>
#include <QCheckBox>
#include <QTextEdit>
#include <QRegularExpression>
#include <QMetaType>
#include <QMessageBox>
#include <QGroupBox>
#include <QSplitter>
#include <QListWidget>
#include <QStringListModel>
#include <cmath>
#include <numeric>
#include <algorithm>

DisplayWidget::DisplayWidget(const QString& widgetId, const QString& windowTitle, QWidget* parent)
    : BaseWidget(widgetId, windowTitle, parent)
    , m_displayConfigAction(nullptr)
    , m_triggerAction(nullptr)
    , m_resetFormattingAction(nullptr)
{
    PROFILE_SCOPE("DisplayWidget::constructor");
    
    Monitor::Logging::Logger::instance()->debug("DisplayWidget", 
        QString("Display widget '%1' created").arg(widgetId));
}

DisplayWidget::~DisplayWidget() {
    PROFILE_SCOPE("DisplayWidget::destructor");
    
    Monitor::Logging::Logger::instance()->debug("DisplayWidget", 
        QString("Display widget '%1' destroyed").arg(getWidgetId()));
}

void DisplayWidget::setDisplayConfig(const QString& fieldPath, const DisplayConfig& config) {
    if (fieldPath.isEmpty()) {
        return;
    }
    
    m_displayConfigs[fieldPath] = config;
    
    // Trigger display update for this field
    auto it = m_fieldValues.find(fieldPath);
    if (it != m_fieldValues.end()) {
        it->second.hasNewValue = true;
        if (isUpdateEnabled() && isVisible()) {
            refreshDisplay();
        }
    }
    
    Monitor::Logging::Logger::instance()->debug("DisplayWidget", 
        QString("Display config updated for field '%1' in widget '%2'").arg(fieldPath).arg(getWidgetId()));
}

DisplayWidget::DisplayConfig DisplayWidget::getDisplayConfig(const QString& fieldPath) const {
    auto it = m_displayConfigs.find(fieldPath);
    return (it != m_displayConfigs.end()) ? it->second : DisplayConfig();
}

void DisplayWidget::resetDisplayConfig(const QString& fieldPath) {
    auto it = m_displayConfigs.find(fieldPath);
    if (it != m_displayConfigs.end()) {
        it->second = DisplayConfig();
        
        // Trigger display update
        auto valueIt = m_fieldValues.find(fieldPath);
        if (valueIt != m_fieldValues.end()) {
            valueIt->second.hasNewValue = true;
            if (isUpdateEnabled() && isVisible()) {
                refreshDisplay();
            }
        }
    }
}

void DisplayWidget::setTriggerCondition(const TriggerCondition& condition) {
    m_triggerCondition = condition;
    
    // Parse the condition for performance
    if (condition.enabled && !condition.expression.isEmpty()) {
        parseConditionExpression(condition.expression, m_triggerCondition.parsed);
    }
    
    Monitor::Logging::Logger::instance()->debug("DisplayWidget", 
        QString("Trigger condition %1 for widget '%2': %3")
        .arg(condition.enabled ? "enabled" : "disabled")
        .arg(getWidgetId())
        .arg(condition.expression));
}

void DisplayWidget::clearTriggerCondition() {
    m_triggerCondition = TriggerCondition();
}

QVariant DisplayWidget::getFieldValue(const QString& fieldPath) const {
    auto it = m_fieldValues.find(fieldPath);
    return (it != m_fieldValues.end()) ? it->second.currentValue : QVariant();
}

QVariant DisplayWidget::getTransformedValue(const QString& fieldPath) const {
    auto it = m_fieldValues.find(fieldPath);
    return (it != m_fieldValues.end()) ? it->second.transformedValue : QVariant();
}

QString DisplayWidget::getFormattedValue(const QString& fieldPath) const {
    auto it = m_fieldValues.find(fieldPath);
    if (it == m_fieldValues.end()) {
        return QString();
    }
    
    auto configIt = m_displayConfigs.find(fieldPath);
    DisplayConfig config = (configIt != m_displayConfigs.end()) ? configIt->second : DisplayConfig();
    
    return formatValue(it->second.transformedValue, config);
}

bool DisplayWidget::hasNewValue(const QString& fieldPath) const {
    auto it = m_fieldValues.find(fieldPath);
    return (it != m_fieldValues.end()) && it->second.hasNewValue;
}

void DisplayWidget::markValueProcessed(const QString& fieldPath) {
    auto it = m_fieldValues.find(fieldPath);
    if (it != m_fieldValues.end()) {
        it->second.hasNewValue = false;
    }
}

void DisplayWidget::initializeWidget() {
    PROFILE_SCOPE("DisplayWidget::initializeWidget");
    
    // Set default minimum size for display widgets
    setMinimumSize(300, 200);
    
    // Initialize field values for existing assignments
    for (const auto& assignment : m_fieldAssignments) {
        ensureDisplayConfig(assignment.fieldPath);
        m_fieldValues[assignment.fieldPath] = FieldValue();
    }
    
    Monitor::Logging::Logger::instance()->debug("DisplayWidget", 
        QString("Display widget '%1' initialized").arg(getWidgetId()));
}

void DisplayWidget::updateDisplay() {
    PROFILE_SCOPE("DisplayWidget::updateDisplay");
    
    // Extract and update field values from packets
    extractAndUpdateFieldValues();
    
    // Process transformations
    processFieldTransformations();
    
    // Check trigger condition
    if (!shouldUpdateDisplay()) {
        return;
    }
    
    // Update displays for fields with new values
    for (const auto& pair : m_fieldValues) {
        const QString& fieldPath = pair.first;
        const FieldValue& fieldValue = pair.second;
        
        if (fieldValue.hasNewValue) {
            updateFieldDisplay(fieldPath, fieldValue.transformedValue);
        }
    }
    
    // Mark all values as processed
    for (auto& pair : m_fieldValues) {
        pair.second.hasNewValue = false;
    }
}

void DisplayWidget::handleFieldAdded(const FieldAssignment& field) {
    ensureDisplayConfig(field.fieldPath);
    m_fieldValues[field.fieldPath] = FieldValue();
    
    Monitor::Logging::Logger::instance()->debug("DisplayWidget", 
        QString("Field '%1' added to display widget '%2'").arg(field.fieldPath).arg(getWidgetId()));
}

void DisplayWidget::handleFieldRemoved(const QString& fieldPath) {
    m_fieldValues.erase(fieldPath);
    m_displayConfigs.erase(fieldPath);
    
    clearFieldDisplay(fieldPath);
    
    Monitor::Logging::Logger::instance()->debug("DisplayWidget", 
        QString("Field '%1' removed from display widget '%2'").arg(fieldPath).arg(getWidgetId()));
}

void DisplayWidget::handleFieldsCleared() {
    m_fieldValues.clear();
    m_displayConfigs.clear();
    
    refreshAllDisplays();
    
    Monitor::Logging::Logger::instance()->debug("DisplayWidget", 
        QString("All fields cleared from display widget '%1'").arg(getWidgetId()));
}

QJsonObject DisplayWidget::saveWidgetSpecificSettings() const {
    QJsonObject settings;
    
    // Save display configurations
    QJsonObject displayConfigs;
    for (const auto& pair : m_displayConfigs) {
        const QString& fieldPath = pair.first;
        const DisplayConfig& config = pair.second;
        
        QJsonObject configObj;
        configObj["conversion"] = static_cast<int>(config.conversion);
        configObj["mathOp"] = static_cast<int>(config.mathOp);
        configObj["mathOperand"] = config.mathOperand;
        configObj["function"] = static_cast<int>(config.function);
        configObj["functionWindow"] = config.functionWindow;
        configObj["prefix"] = config.prefix;
        configObj["suffix"] = config.suffix;
        configObj["decimalPlaces"] = config.decimalPlaces;
        configObj["useThousandsSeparator"] = config.useThousandsSeparator;
        configObj["useScientificNotation"] = config.useScientificNotation;
        configObj["textColor"] = config.textColor.name();
        configObj["backgroundColor"] = config.backgroundColor.name();
        configObj["fontSet"] = config.fontSet;
        if (config.fontSet) {
            configObj["font"] = config.font.toString();
        }
        configObj["isVisible"] = config.isVisible;
        configObj["customDisplayName"] = config.customDisplayName;
        
        displayConfigs[fieldPath] = configObj;
    }
    settings["displayConfigs"] = displayConfigs;
    
    // Save trigger condition
    QJsonObject trigger;
    trigger["enabled"] = m_triggerCondition.enabled;
    trigger["expression"] = m_triggerCondition.expression;
    settings["trigger"] = trigger;
    
    return settings;
}

bool DisplayWidget::restoreWidgetSpecificSettings(const QJsonObject& settings) {
    // Restore display configurations
    QJsonObject displayConfigs = settings.value("displayConfigs").toObject();
    for (auto it = displayConfigs.begin(); it != displayConfigs.end(); ++it) {
        const QString& fieldPath = it.key();
        QJsonObject configObj = it.value().toObject();
        
        DisplayConfig config;
        config.conversion = static_cast<ConversionType>(configObj.value("conversion").toInt());
        config.mathOp = static_cast<MathOperation>(configObj.value("mathOp").toInt());
        config.mathOperand = configObj.value("mathOperand").toDouble(1.0);
        config.function = static_cast<FunctionType>(configObj.value("function").toInt());
        config.functionWindow = configObj.value("functionWindow").toInt(10);
        config.prefix = configObj.value("prefix").toString();
        config.suffix = configObj.value("suffix").toString();
        config.decimalPlaces = configObj.value("decimalPlaces").toInt(2);
        config.useThousandsSeparator = configObj.value("useThousandsSeparator").toBool();
        config.useScientificNotation = configObj.value("useScientificNotation").toBool();
        config.textColor = QColor(configObj.value("textColor").toString());
        config.backgroundColor = QColor(configObj.value("backgroundColor").toString());
        config.fontSet = configObj.value("fontSet").toBool();
        if (config.fontSet) {
            config.font.fromString(configObj.value("font").toString());
        }
        config.isVisible = configObj.value("isVisible").toBool(true);
        config.customDisplayName = configObj.value("customDisplayName").toString();
        
        m_displayConfigs[fieldPath] = config;
    }
    
    // Restore trigger condition
    QJsonObject trigger = settings.value("trigger").toObject();
    TriggerCondition condition;
    condition.enabled = trigger.value("enabled").toBool();
    condition.expression = trigger.value("expression").toString();
    setTriggerCondition(condition);
    
    return true;
}

void DisplayWidget::setupContextMenu() {
    if (!m_displayConfigAction) {
        m_displayConfigAction = getContextMenu()->addAction("Display Settings...", 
            this, &DisplayWidget::onShowDisplayConfigDialog);
        m_triggerAction = getContextMenu()->addAction("Trigger Condition...", 
            this, &DisplayWidget::onShowTriggerDialog);
        m_resetFormattingAction = getContextMenu()->addAction("Reset Formatting", 
            this, &DisplayWidget::onResetFieldFormatting);
    }
}

QStringList DisplayWidget::getVisibleFields() const {
    QStringList visibleFields;
    
    for (const auto& assignment : m_fieldAssignments) {
        auto configIt = m_displayConfigs.find(assignment.fieldPath);
        bool isVisible = (configIt == m_displayConfigs.end()) || configIt->second.isVisible;
        
        if (isVisible) {
            visibleFields.append(assignment.fieldPath);
        }
    }
    
    return visibleFields;
}

int DisplayWidget::getVisibleFieldCount() const {
    return getVisibleFields().count();
}

bool DisplayWidget::shouldUpdateDisplay() const {
    // Check trigger condition
    if (m_triggerCondition.enabled && !m_triggerCondition.expression.isEmpty()) {
        return const_cast<DisplayWidget*>(this)->evaluateTriggerCondition();
    }
    
    return true; // Default: always update
}

void DisplayWidget::onShowDisplayConfigDialog() {
    // This will be implemented with the actual dialog
    Monitor::Logging::Logger::instance()->debug("DisplayWidget", 
        QString("Display config dialog requested for widget '%1'").arg(getWidgetId()));
}

void DisplayWidget::onShowTriggerDialog() {
    // This will be implemented with the actual dialog
    Monitor::Logging::Logger::instance()->debug("DisplayWidget", 
        QString("Trigger dialog requested for widget '%1'").arg(getWidgetId()));
}

void DisplayWidget::onResetFieldFormatting() {
    for (auto& pair : m_displayConfigs) {
        pair.second = DisplayConfig();
    }
    
    // Mark all values for update
    for (auto& pair : m_fieldValues) {
        pair.second.hasNewValue = true;
    }
    
    refreshDisplay();
    
    Monitor::Logging::Logger::instance()->info("DisplayWidget", 
        QString("Formatting reset for widget '%1'").arg(getWidgetId()));
}

void DisplayWidget::extractAndUpdateFieldValues() {
    PROFILE_SCOPE("DisplayWidget::extractAndUpdateFieldValues");
    
    auto* extractor = getFieldExtractor();
    if (!extractor) {
        return;
    }
    
    // Group fields by packet ID for efficient extraction
    std::unordered_map<Monitor::Packet::PacketId, std::vector<QString>> fieldsByPacket;
    
    for (const auto& assignment : m_fieldAssignments) {
        fieldsByPacket[assignment.packetId].push_back(assignment.fieldPath);
    }
    
    // Extract fields for each packet type
    // Note: This is a simplified version - in a real implementation,
    // we would need to get the latest packets for each type
    
    // For now, we'll simulate field updates
    // This would be replaced with actual packet field extraction
}

void DisplayWidget::processFieldTransformations() {
    PROFILE_SCOPE("DisplayWidget::processFieldTransformations");
    
    for (auto& pair : m_fieldValues) {
        const QString& fieldPath = pair.first;
        FieldValue& fieldValue = pair.second;
        
        if (fieldValue.hasNewValue) {
            QVariant transformed = transformValue(fieldPath, fieldValue.currentValue);
            fieldValue.transformedValue = transformed;
            fieldValue.addToHistory(fieldValue.currentValue);
        }
    }
}

bool DisplayWidget::evaluateTriggerCondition() {
    if (!m_triggerCondition.enabled || !m_triggerCondition.parsed.isValid) {
        return true;
    }
    
    PROFILE_SCOPE("DisplayWidget::evaluateTriggerCondition");
    
    bool result = evaluateParsedCondition(m_triggerCondition.parsed);
    m_triggerCondition.lastResult = result;
    m_triggerCondition.lastEvaluation = std::chrono::steady_clock::now();
    
    return result;
}

void DisplayWidget::updateFieldValue(const QString& fieldPath, const QVariant& rawValue) {
    auto& fieldValue = m_fieldValues[fieldPath];
    fieldValue.currentValue = rawValue;
    fieldValue.hasNewValue = true;
    fieldValue.timestamp = std::chrono::steady_clock::now();
}

QVariant DisplayWidget::transformValue(const QString& fieldPath, const QVariant& rawValue) {
    auto configIt = m_displayConfigs.find(fieldPath);
    if (configIt == m_displayConfigs.end()) {
        return rawValue; // No transformation
    }
    
    const DisplayConfig& config = configIt->second;
    QVariant value = rawValue;
    
    // Apply type conversion
    if (config.conversion != ConversionType::NoConversion) {
        value = convertValue(value, config.conversion);
    }
    
    // Apply mathematical operation
    if (config.mathOp != MathOperation::None) {
        value = applyMathOperation(value, config.mathOp, config.mathOperand);
    }
    
    // Apply function (requires history)
    if (config.function != FunctionType::None) {
        auto fieldIt = m_fieldValues.find(fieldPath);
        if (fieldIt != m_fieldValues.end()) {
            value = applyFunction(fieldIt->second.history, config.function);
        }
    }
    
    return value;
}

void DisplayWidget::ensureDisplayConfig(const QString& fieldPath) {
    if (m_displayConfigs.find(fieldPath) == m_displayConfigs.end()) {
        m_displayConfigs[fieldPath] = DisplayConfig();
    }
}

bool DisplayWidget::parseConditionExpression(const QString& expression, TriggerCondition::ParsedCondition& parsed) {
    // Simple expression parser for conditions like "field > 100" or "field == true"
    // This is a basic implementation - a full parser would handle more complex expressions
    
    QStringList parts = expression.split(QRegularExpression("\\s*(>=|<=|==|!=|>|<)\\s*"));
    if (parts.size() != 2) {
        return false;
    }
    
    parsed.fieldPath = parts[0].trimmed();
    parsed.value = parts[1].trimmed();
    
    // Extract operator
    QRegularExpression opRegex("(>=|<=|==|!=|>|<)");
    QRegularExpressionMatch match = opRegex.match(expression);
    if (match.hasMatch()) {
        parsed.operator_ = match.captured(1);
        parsed.isValid = true;
        return true;
    }
    
    return false;
}

bool DisplayWidget::evaluateParsedCondition(const TriggerCondition::ParsedCondition& condition) {
    if (!condition.isValid) {
        return true;
    }
    
    QVariant fieldValue = getFieldValueForCondition(condition.fieldPath);
    if (!fieldValue.isValid()) {
        return false;
    }
    
    QVariant conditionValue = condition.value;
    
    // Convert values to comparable types
    if (fieldValue.typeId() != conditionValue.typeId()) {
        conditionValue.convert(QMetaType(fieldValue.typeId()));
    }
    
    // Evaluate based on operator
    if (condition.operator_ == "==") {
        return fieldValue == conditionValue;
    } else if (condition.operator_ == "!=") {
        return fieldValue != conditionValue;
    } else if (condition.operator_ == ">") {
        return fieldValue.toDouble() > conditionValue.toDouble();
    } else if (condition.operator_ == "<") {
        return fieldValue.toDouble() < conditionValue.toDouble();
    } else if (condition.operator_ == ">=") {
        return fieldValue.toDouble() >= conditionValue.toDouble();
    } else if (condition.operator_ == "<=") {
        return fieldValue.toDouble() <= conditionValue.toDouble();
    }
    
    return true;
}

QVariant DisplayWidget::getFieldValueForCondition(const QString& fieldPath) const {
    return getTransformedValue(fieldPath);
}

// Static helper methods
QString DisplayWidget::formatValue(const QVariant& value, const DisplayConfig& config) {
    if (!value.isValid()) {
        return QString("--");
    }
    
    QString formatted;
    
    // Basic formatting based on type
    switch (value.typeId()) {
        case QMetaType::Bool:
            formatted = value.toBool() ? "true" : "false";
            break;
            
        case QMetaType::Int:
        case QMetaType::UInt:
        case QMetaType::LongLong:
        case QMetaType::ULongLong:
            if (config.conversion == ConversionType::ToHexadecimal) {
                formatted = QString("0x%1").arg(value.toULongLong(), 0, 16, QChar('0')).toUpper();
            } else if (config.conversion == ConversionType::ToBinary) {
                formatted = QString("0b%1").arg(value.toULongLong(), 0, 2, QChar('0'));
            } else {
                formatted = QString::number(value.toLongLong());
                if (config.useThousandsSeparator) {
                    // Add thousands separators
                    QLocale locale;
                    formatted = locale.toString(value.toLongLong());
                }
            }
            break;
            
        case QMetaType::Double:
            if (config.useScientificNotation) {
                formatted = QString::number(value.toDouble(), 'e', config.decimalPlaces);
            } else {
                formatted = QString::number(value.toDouble(), 'f', config.decimalPlaces);
                if (config.useThousandsSeparator) {
                    QLocale locale;
                    formatted = locale.toString(value.toDouble(), 'f', config.decimalPlaces);
                }
            }
            break;
            
        case QMetaType::QString:
            formatted = value.toString();
            break;
            
        default:
            formatted = value.toString();
            break;
    }
    
    // Add prefix and suffix
    if (!config.prefix.isEmpty()) {
        formatted = config.prefix + formatted;
    }
    if (!config.suffix.isEmpty()) {
        formatted = formatted + config.suffix;
    }
    
    return formatted;
}

QVariant DisplayWidget::convertValue(const QVariant& input, ConversionType conversion) {
    switch (conversion) {
        case ConversionType::NoConversion:
            return input;
            
        case ConversionType::ToInteger:
            return input.toLongLong();
            
        case ConversionType::ToDouble:
            return input.toDouble();
            
        case ConversionType::ToString:
            return input.toString();
            
        case ConversionType::ToBoolean:
            return input.toBool();
            
        case ConversionType::ToHexadecimal:
        case ConversionType::ToBinary:
            // These are handled in formatting
            return input;
    }
    
    return input;
}

QVariant DisplayWidget::applyMathOperation(const QVariant& input, MathOperation op, double operand) {
    double value = input.toDouble();
    
    switch (op) {
        case MathOperation::None:
            return input;
            
        case MathOperation::Multiply:
            return value * operand;
            
        case MathOperation::Divide:
            return (operand != 0.0) ? (value / operand) : QVariant();
            
        case MathOperation::Add:
            return value + operand;
            
        case MathOperation::Subtract:
            return value - operand;
            
        case MathOperation::Modulo:
            return (operand != 0.0) ? std::fmod(value, operand) : QVariant();
            
        case MathOperation::Power:
            return std::pow(value, operand);
            
        case MathOperation::Absolute:
            return std::abs(value);
            
        case MathOperation::Negate:
            return -value;
    }
    
    return input;
}

QVariant DisplayWidget::applyFunction(const std::vector<QVariant>& history, FunctionType function) {
    if (history.empty()) {
        return QVariant();
    }
    
    switch (function) {
        case FunctionType::None:
            return history.back();
            
        case FunctionType::Difference:
            if (history.size() >= 2) {
                return history.back().toDouble() - history[history.size()-2].toDouble();
            }
            return QVariant();
            
        case FunctionType::CumulativeSum: {
            double sum = 0.0;
            for (const auto& value : history) {
                sum += value.toDouble();
            }
            return sum;
        }
        
        case FunctionType::MovingAverage: {
            if (history.empty()) return QVariant();
            double sum = 0.0;
            for (const auto& value : history) {
                sum += value.toDouble();
            }
            return sum / static_cast<double>(history.size());
        }
        
        case FunctionType::Minimum: {
            auto it = std::min_element(history.begin(), history.end(),
                [](const QVariant& a, const QVariant& b) {
                    return a.toDouble() < b.toDouble();
                });
            return (it != history.end()) ? *it : QVariant();
        }
        
        case FunctionType::Maximum: {
            auto it = std::max_element(history.begin(), history.end(),
                [](const QVariant& a, const QVariant& b) {
                    return a.toDouble() < b.toDouble();
                });
            return (it != history.end()) ? *it : QVariant();
        }
        
        case FunctionType::Range: {
            if (history.empty()) return QVariant();
            auto minMax = std::minmax_element(history.begin(), history.end(),
                [](const QVariant& a, const QVariant& b) {
                    return a.toDouble() < b.toDouble();
                });
            return minMax.second->toDouble() - minMax.first->toDouble();
        }
        
        case FunctionType::StandardDeviation: {
            if (history.size() < 2) return QVariant();
            
            double mean = 0.0;
            for (const auto& value : history) {
                mean += value.toDouble();
            }
            mean /= static_cast<double>(history.size());
            
            double variance = 0.0;
            for (const auto& value : history) {
                double diff = value.toDouble() - mean;
                variance += diff * diff;
            }
            variance /= static_cast<double>(history.size() - 1);
            
            return std::sqrt(variance);
        }
    }
    
    return history.back();
}