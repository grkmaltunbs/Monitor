#include "chart_common.h"
#include <QtCharts/QChartView>
#include <QtCharts/QLegend>
#include <QtCharts/QBarCategoryAxis>
#include <QtCharts/QBarSet>
#include <QtCharts/QPieSlice>
#include <QtSvg/QSvgGenerator>
#include <QPainter>
#include <QPixmap>
#include <QApplication>
#include <QFileInfo>
#include <QDir>
#include <QDebug>
#include <algorithm>
#include <numeric>
#include <cmath>


namespace Monitor::Charts {

// ChartThemeConfig implementation
ChartThemeConfig ChartThemeConfig::getTheme(ChartTheme theme) {
    ChartThemeConfig config;
    
    switch (theme) {
        case ChartTheme::Light:
            // Already set to light theme defaults
            break;
            
        case ChartTheme::Dark:
            config.backgroundColor = QColor(42, 42, 42);
            config.plotAreaColor = QColor(42, 42, 42);
            config.gridLineColor = QColor(80, 80, 80);
            config.axisLineColor = QColor(200, 200, 200);
            config.axisLabelColor = QColor(200, 200, 200);
            config.titleColor = QColor(255, 255, 255);
            config.legendColor = QColor(200, 200, 200);
            break;
            
        case ChartTheme::BlueCerulean:
            config.backgroundColor = QColor(240, 248, 255);
            config.plotAreaColor = QColor(248, 252, 255);
            config.gridLineColor = QColor(176, 196, 222);
            config.axisLineColor = QColor(70, 130, 180);
            config.axisLabelColor = QColor(25, 25, 112);
            config.titleColor = QColor(25, 25, 112);
            config.legendColor = QColor(25, 25, 112);
            break;
            
        case ChartTheme::Custom:
            // Custom themes handled by caller
            break;
    }
    
    return config;
}

void ChartThemeConfig::applyToChart(QChart* chart) const {
    if (!chart) return;
    
    chart->setBackgroundBrush(QBrush(backgroundColor));
    chart->setPlotAreaBackgroundBrush(QBrush(plotAreaColor));
    chart->setPlotAreaBackgroundVisible(true);
    chart->setTitleFont(titleFont);
    chart->setTitleBrush(QBrush(titleColor));
    
    // Apply to legend
    QLegend* legend = chart->legend();
    if (legend) {
        legend->setFont(legendFont);
        legend->setBrush(QBrush(legendColor));
    }
    
    // Apply to axes (will be done when axes are created)
}

// PerformanceConfig implementation
PerformanceConfig PerformanceConfig::getConfig(PerformanceLevel level) {
    PerformanceConfig config;
    
    switch (level) {
        case PerformanceLevel::High:
            config.maxDataPoints = 100000;
            config.maxSeriesCount = 50;
            config.useOpenGL = true;
            config.enableAnimations = true;
            config.decimation = DecimationStrategy::None;
            config.updateThrottleMs = 16;
            break;
            
        case PerformanceLevel::Balanced:
            config.maxDataPoints = 10000;
            config.maxSeriesCount = 20;
            config.useOpenGL = true;
            config.enableAnimations = true;
            config.decimation = DecimationStrategy::LTTB;
            config.updateThrottleMs = 16;
            break;
            
        case PerformanceLevel::Fast:
            config.maxDataPoints = 1000;
            config.maxSeriesCount = 10;
            config.useOpenGL = true;
            config.enableAnimations = false;
            config.decimation = DecimationStrategy::Uniform;
            config.updateThrottleMs = 33; // ~30 FPS
            break;
            
        case PerformanceLevel::Adaptive:
            // Start with balanced, will be adjusted based on performance
            config = getConfig(PerformanceLevel::Balanced);
            break;
    }
    
    return config;
}

void PerformanceConfig::applyToChart(QChart* chart) const {
    if (!chart) return;
    
    chart->setAnimationOptions(enableAnimations ? 
        QChart::SeriesAnimations : QChart::NoAnimation);
}

void PerformanceConfig::applyToView(QChartView* view) const {
    if (!view) return;
    
    if (useOpenGL) {
        view->setRenderHint(QPainter::Antialiasing, true);
        // Note: OpenGL support would need QOpenGLWidget, which we'll implement later
    } else {
        view->setRenderHint(QPainter::Antialiasing, false);
    }
}

// DataConverter implementation
double DataConverter::toDouble(const QVariant& value, bool* ok) {
    if (ok) *ok = true;
    
    switch (value.typeId()) {
        case QMetaType::Double:
        case QMetaType::Float:
            return value.toDouble();
            
        case QMetaType::Int:
        case QMetaType::UInt:
        case QMetaType::LongLong:
        case QMetaType::ULongLong:
            return static_cast<double>(value.toLongLong());
            
        case QMetaType::Bool:
            return value.toBool() ? 1.0 : 0.0;
            
        case QMetaType::QString: {
            bool conversionOk;
            double result = value.toString().toDouble(&conversionOk);
            if (ok) *ok = conversionOk;
            return conversionOk ? result : 0.0;
        }
        
        default:
            if (ok) *ok = false;
            return 0.0;
    }
}

QString DataConverter::toString(const QVariant& value) {
    return value.toString();
}

bool DataConverter::isNumeric(const QVariant& value) {
    bool ok;
    toDouble(value, &ok);
    return ok;
}

QString DataConverter::getAxisType(const QVariant& sampleValue) {
    if (isNumeric(sampleValue)) {
        return "ValueAxis";
    } else {
        return "CategoryAxis";
    }
}

QString DataConverter::formatValue(const QVariant& value, int decimalPlaces) {
    if (isNumeric(value)) {
        return QString::number(toDouble(value), 'f', decimalPlaces);
    } else {
        return toString(value);
    }
}

std::vector<QPointF> DataConverter::decimateData(const std::vector<QPointF>& data, 
                                               int maxPoints, 
                                               DecimationStrategy strategy) {
    if (data.size() <= static_cast<size_t>(maxPoints)) {
        return data; // No decimation needed
    }
    
    std::vector<QPointF> result;
    result.reserve(maxPoints);
    
    switch (strategy) {
        case DecimationStrategy::None:
            return data;
            
        case DecimationStrategy::Uniform: {
            double step = static_cast<double>(data.size()) / maxPoints;
            for (int i = 0; i < maxPoints; ++i) {
                int index = static_cast<int>(i * step);
                if (index < static_cast<int>(data.size())) {
                    result.push_back(data[index]);
                }
            }
            break;
        }
        
        case DecimationStrategy::MinMax: {
            int blockSize = data.size() / maxPoints;
            for (int block = 0; block < maxPoints; ++block) {
                int start = block * blockSize;
                int end = std::min(start + blockSize, static_cast<int>(data.size()));
                
                auto minMax = std::minmax_element(data.begin() + start, data.begin() + end,
                    [](const QPointF& a, const QPointF& b) { return a.y() < b.y(); });
                
                if (minMax.first->x() < minMax.second->x()) {
                    result.push_back(*minMax.first);
                    result.push_back(*minMax.second);
                } else {
                    result.push_back(*minMax.second);
                    result.push_back(*minMax.first);
                }
            }
            break;
        }
        
        case DecimationStrategy::LTTB: {
            // Largest Triangle Three Buckets algorithm
            // Simplified implementation
            if (data.size() < 3) return data;
            
            result.push_back(data[0]); // Always keep first point
            
            double bucketSize = static_cast<double>(data.size() - 2) / (maxPoints - 2);
            int a = 0;
            
            for (int i = 0; i < maxPoints - 2; ++i) {
                int bucketStart = static_cast<int>((i + 1) * bucketSize) + 1;
                int bucketEnd = static_cast<int>((i + 2) * bucketSize) + 1;
                bucketEnd = std::min(bucketEnd, static_cast<int>(data.size()) - 1);
                
                // Calculate bucket average for next bucket
                double avgX = 0, avgY = 0;
                int avgCount = bucketEnd - bucketStart;
                if (avgCount > 0) {
                    for (int j = bucketStart; j < bucketEnd; ++j) {
                        avgX += data[j].x();
                        avgY += data[j].y();
                    }
                    avgX /= avgCount;
                    avgY /= avgCount;
                }
                
                // Find point with largest triangle area
                double maxArea = -1;
                int maxIndex = a + 1;
                
                for (int j = a + 1; j < bucketStart; ++j) {
                    double area = std::abs(
                        (data[a].x() - avgX) * (data[j].y() - data[a].y()) -
                        (data[a].x() - data[j].x()) * (avgY - data[a].y())
                    ) / 2.0;
                    
                    if (area > maxArea) {
                        maxArea = area;
                        maxIndex = j;
                    }
                }
                
                result.push_back(data[maxIndex]);
                a = maxIndex;
            }
            
            result.push_back(data.back()); // Always keep last point
            break;
        }
        
        case DecimationStrategy::Adaptive:
            // Fall back to LTTB for now
            return decimateData(data, maxPoints, DecimationStrategy::LTTB);
    }
    
    return result;
}

double DataConverter::calculateMean(const std::vector<double>& data) {
    if (data.empty()) return 0.0;
    return std::accumulate(data.begin(), data.end(), 0.0) / data.size();
}

double DataConverter::calculateStdDev(const std::vector<double>& data) {
    if (data.size() < 2) return 0.0;
    
    double mean = calculateMean(data);
    double variance = 0.0;
    
    for (double value : data) {
        variance += (value - mean) * (value - mean);
    }
    
    return std::sqrt(variance / (data.size() - 1));
}

std::pair<double, double> DataConverter::calculateMinMax(const std::vector<double>& data) {
    if (data.empty()) return {0.0, 0.0};
    
    auto minMax = std::minmax_element(data.begin(), data.end());
    return {*minMax.first, *minMax.second};
}

// ChartExporter implementation
bool ChartExporter::exportChart(QChart* chart, const QString& filePath, ExportFormat format) {
    return exportChart(chart, filePath, format, QSize(800, 600));
}

bool ChartExporter::exportChart(QChart* chart, const QString& filePath, 
                              ExportFormat format, const QSize& size) {
    if (!chart) {
        qWarning() << "ChartExporter::exportChart: Chart is null";
        return false;
    }
    
    // Ensure directory exists
    QFileInfo fileInfo(filePath);
    QDir dir = fileInfo.absoluteDir();
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    
    switch (format) {
        case ExportFormat::PNG:
            return exportToPNG(chart, filePath, size);
        case ExportFormat::SVG:
            return exportToSVG(chart, filePath, size);
        case ExportFormat::PDF:
            return exportToPDF(chart, filePath, size);
        case ExportFormat::JPEG:
            return exportToJPEG(chart, filePath, size);
    }
    
    return false;
}

QStringList ChartExporter::getFileExtensions(ExportFormat format) {
    switch (format) {
        case ExportFormat::PNG: return {"png"};
        case ExportFormat::SVG: return {"svg"};
        case ExportFormat::PDF: return {"pdf"};
        case ExportFormat::JPEG: return {"jpg", "jpeg"};
    }
    return {};
}

QString ChartExporter::getFileFilter() {
    return "PNG Images (*.png);;SVG Images (*.svg);;PDF Documents (*.pdf);;JPEG Images (*.jpg *.jpeg)";
}

bool ChartExporter::exportToPNG(QChart* chart, const QString& filePath, const QSize& size) {
    QChartView chartView(chart);
    chartView.resize(size);
    chartView.setRenderHint(QPainter::Antialiasing);
    
    QPixmap pixmap = chartView.grab();
    return pixmap.save(filePath, "PNG");
}

bool ChartExporter::exportToSVG(QChart* chart, const QString& filePath, const QSize& size) {
    QSvgGenerator generator;
    generator.setFileName(filePath);
    generator.setSize(size);
    generator.setViewBox(QRect(0, 0, size.width(), size.height()));
    generator.setDescription("Chart exported from Monitor Application");
    
    QChartView chartView(chart);
    chartView.resize(size);
    chartView.setRenderHint(QPainter::Antialiasing);
    
    QPainter painter(&generator);
    chartView.render(&painter);
    
    return true; // QSvgGenerator doesn't provide error feedback
}

bool ChartExporter::exportToPDF(QChart* chart, const QString& filePath, const QSize& size) {
    // PDF export would require QPrinter - implement later if needed
    Q_UNUSED(chart);
    Q_UNUSED(filePath);
    Q_UNUSED(size);
    qWarning() << "ChartExporter::exportToPDF: PDF export not yet implemented";
    return false;
}

bool ChartExporter::exportToJPEG(QChart* chart, const QString& filePath, const QSize& size) {
    QChartView chartView(chart);
    chartView.resize(size);
    chartView.setRenderHint(QPainter::Antialiasing);
    
    QPixmap pixmap = chartView.grab();
    return pixmap.save(filePath, "JPEG", 95); // High quality JPEG
}

// SeriesFactory implementation
QLineSeries* SeriesFactory::createLineSeries(const QString& name, const QColor& color) {
    QLineSeries* series = new QLineSeries();
    series->setName(name);
    series->setColor(color);
    series->setPen(QPen(color, 2));
    return series;
}

QBarSeries* SeriesFactory::createBarSeries(const QString& name) {
    QBarSeries* series = new QBarSeries();
    series->setName(name);
    return series;
}

QPieSeries* SeriesFactory::createPieSeries(const QString& name) {
    QPieSeries* series = new QPieSeries();
    series->setName(name);
    return series;
}

void SeriesFactory::configureLineSeries(QLineSeries* series, const QJsonObject& config) {
    if (!series) return;
    
    if (config.contains("color")) {
        QColor color(config["color"].toString());
        series->setColor(color);
        series->setPen(QPen(color, config.value("lineWidth").toInt(2)));
    }
    
    if (config.contains("pointsVisible")) {
        series->setPointsVisible(config["pointsVisible"].toBool());
    }
    
    if (config.contains("pointSize")) {
        series->setMarkerSize(config["pointSize"].toDouble(6.0));
    }
}

void SeriesFactory::configureBarSeries(QBarSeries* series, const QJsonObject& config) {
    if (!series) return;
    
    if (config.contains("labelsVisible")) {
        series->setLabelsVisible(config["labelsVisible"].toBool());
    }
}

void SeriesFactory::configurePieSeries(QPieSeries* series, const QJsonObject& config) {
    if (!series) return;
    
    if (config.contains("holeSize")) {
        series->setHoleSize(config["holeSize"].toDouble(0.0));
    }
    
    if (config.contains("labelsVisible")) {
        series->setLabelsVisible(config["labelsVisible"].toBool());
    }
}

// AxisFactory implementation
QValueAxis* AxisFactory::createValueAxis(const QString& title, double min, double max) {
    QValueAxis* axis = new QValueAxis();
    axis->setTitleText(title);
    axis->setRange(min, max);
    axis->setTickCount(6);
    axis->setLabelFormat("%.2f");
    return axis;
}

QCategoryAxis* AxisFactory::createCategoryAxis(const QString& title, const QStringList& categories) {
    QCategoryAxis* axis = new QCategoryAxis();
    axis->setTitleText(title);
    
    for (int i = 0; i < categories.size(); ++i) {
        axis->append(categories[i], i);
    }
    
    return axis;
}

QDateTimeAxis* AxisFactory::createDateTimeAxis(const QString& title) {
    QDateTimeAxis* axis = new QDateTimeAxis();
    axis->setTitleText(title);
    axis->setFormat("hh:mm:ss");
    return axis;
}

void AxisFactory::autoScaleAxis(QValueAxis* axis, const std::vector<double>& data, 
                              double marginPercent) {
    if (!axis || data.empty()) return;
    
    auto minMax = DataConverter::calculateMinMax(data);
    double range = minMax.second - minMax.first;
    double margin = range * (marginPercent / 100.0);
    
    axis->setRange(minMax.first - margin, minMax.second + margin);
}

} // namespace Monitor::Charts