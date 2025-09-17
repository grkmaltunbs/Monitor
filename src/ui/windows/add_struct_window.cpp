#include "add_struct_window.h"
#include <QtCore/QLoggingCategory>
#include <QApplication>
#include <QStandardPaths>
#include <QDir>
#include <QTextStream>
#include <QFont>
#include <QFontMetrics>
#include <QPainter>
#include <QTextBlock>

Q_LOGGING_CATEGORY(addStructWindow, "Monitor.AddStructWindow")

// ============================================================================
// CppSyntaxHighlighter Implementation
// ============================================================================

CppSyntaxHighlighter::CppSyntaxHighlighter(QTextDocument *parent)
    : QSyntaxHighlighter(parent)
{
    HighlightingRule rule;

    // Keyword format
    keywordFormat.setForeground(QColor(86, 156, 214)); // Light blue
    keywordFormat.setFontWeight(QFont::Bold);
    const QStringList keywordPatterns = {
        QStringLiteral("\\btypedef\\b"), QStringLiteral("\\bstruct\\b"), QStringLiteral("\\bunion\\b"),
        QStringLiteral("\\bint\\b"), QStringLiteral("\\bfloat\\b"), QStringLiteral("\\bdouble\\b"),
        QStringLiteral("\\bchar\\b"), QStringLiteral("\\bbool\\b"), QStringLiteral("\\bvoid\\b"),
        QStringLiteral("\\bunsigned\\b"), QStringLiteral("\\bsigned\\b"), QStringLiteral("\\bshort\\b"),
        QStringLiteral("\\blong\\b"), QStringLiteral("\\bconst\\b"), QStringLiteral("\\bvolatile\\b"),
        QStringLiteral("\\bstatic\\b"), QStringLiteral("\\bextern\\b"), QStringLiteral("\\binline\\b"),
        QStringLiteral("\\buint8_t\\b"), QStringLiteral("\\buint16_t\\b"), QStringLiteral("\\buint32_t\\b"),
        QStringLiteral("\\buint64_t\\b"), QStringLiteral("\\bint8_t\\b"), QStringLiteral("\\bint16_t\\b"),
        QStringLiteral("\\bint32_t\\b"), QStringLiteral("\\bint64_t\\b"), QStringLiteral("\\bsize_t\\b")
    };

    for (const QString &pattern : keywordPatterns) {
        rule.pattern = QRegularExpression(pattern);
        rule.format = keywordFormat;
        highlightingRules.append(rule);
    }

    // Class format
    classFormat.setForeground(QColor(78, 201, 176)); // Teal
    classFormat.setFontWeight(QFont::Bold);
    rule.pattern = QRegularExpression(QStringLiteral("\\b[A-Za-z_][A-Za-z0-9_]*(?=\\s*[{;])"));
    rule.format = classFormat;
    highlightingRules.append(rule);

    // Single line comment format
    singleLineCommentFormat.setForeground(QColor(106, 153, 85)); // Green
    rule.pattern = QRegularExpression(QStringLiteral("//[^\n]*"));
    rule.format = singleLineCommentFormat;
    highlightingRules.append(rule);

    // Multi line comment format
    multiLineCommentFormat.setForeground(QColor(106, 153, 85)); // Green

    // Quotation format
    quotationFormat.setForeground(QColor(206, 145, 120)); // Light orange
    rule.pattern = QRegularExpression(QStringLiteral("\".*\""));
    rule.format = quotationFormat;
    highlightingRules.append(rule);

    // Function format
    functionFormat.setForeground(QColor(220, 220, 170)); // Light yellow
    rule.pattern = QRegularExpression(QStringLiteral("\\b[A-Za-z_][A-Za-z0-9_]*(?=\\()"));
    rule.format = functionFormat;
    highlightingRules.append(rule);
}

void CppSyntaxHighlighter::highlightBlock(const QString &text)
{
    foreach (const HighlightingRule &rule, highlightingRules) {
        QRegularExpressionMatchIterator matchIterator = rule.pattern.globalMatch(text);
        while (matchIterator.hasNext()) {
            QRegularExpressionMatch match = matchIterator.next();
            setFormat(match.capturedStart(), match.capturedLength(), rule.format);
        }
    }

    // Multi-line comments
    setCurrentBlockState(0);

    QRegularExpression startExpression(QStringLiteral("/\\*"));
    QRegularExpression endExpression(QStringLiteral("\\*/"));

    int startIndex = 0;
    if (previousBlockState() != 1)
        startIndex = text.indexOf(startExpression);

    while (startIndex >= 0) {
        QRegularExpressionMatch endMatch = endExpression.match(text, startIndex);
        int endIndex = endMatch.capturedStart();
        int commentLength = 0;
        if (endIndex == -1) {
            setCurrentBlockState(1);
            commentLength = text.length() - startIndex;
        } else {
            commentLength = endIndex - startIndex + endMatch.capturedLength();
        }
        setFormat(startIndex, commentLength, multiLineCommentFormat);
        startIndex = text.indexOf(startExpression, startIndex + commentLength);
    }
}

// ============================================================================
// StructureCodeEditor Implementation
// ============================================================================

StructureCodeEditor::StructureCodeEditor(QWidget *parent)
    : QPlainTextEdit(parent)
    , m_highlighter(new CppSyntaxHighlighter(document()))
    , m_lineNumberArea(nullptr)
    , m_textChangeTimer(new QTimer(this))
{
    // Setup font
    QFont font("Courier New", 10);
    font.setFixedPitch(true);
    setFont(font);

    // Setup timer for delayed text change signals
    m_textChangeTimer->setSingleShot(true);
    m_textChangeTimer->setInterval(500); // 500ms delay
    connect(m_textChangeTimer, &QTimer::timeout, this, &StructureCodeEditor::onDelayedTextChanged);
    connect(this, &QPlainTextEdit::textChanged, this, &StructureCodeEditor::onTextChanged);

    // Set tab width
    QFontMetrics fontMetrics(font);
    setTabStopDistance(fontMetrics.horizontalAdvance(' ') * 4);
}

void StructureCodeEditor::setPlaceholderText(const QString &text)
{
    m_placeholderText = text;
    if (toPlainText().isEmpty()) {
        update();
    }
}

void StructureCodeEditor::highlightError(int line, const QString &message)
{
    if (!m_errorLines.contains(line)) {
        m_errorLines.append(line);
        m_errorMessages.append(message);
        update();
    }
}

void StructureCodeEditor::clearErrorHighlights()
{
    m_errorLines.clear();
    m_errorMessages.clear();
    update();
}

void StructureCodeEditor::paintEvent(QPaintEvent *event)
{
    QPlainTextEdit::paintEvent(event);

    // Paint placeholder text if empty
    if (toPlainText().isEmpty() && !m_placeholderText.isEmpty()) {
        QPainter painter(viewport());
        painter.setPen(palette().color(QPalette::Disabled, QPalette::Text));
        painter.drawText(contentsRect().adjusted(5, 5, -5, -5),
                        Qt::AlignTop | Qt::AlignLeft | Qt::TextWordWrap,
                        m_placeholderText);
    }
}

void StructureCodeEditor::resizeEvent(QResizeEvent *event)
{
    QPlainTextEdit::resizeEvent(event);
}

void StructureCodeEditor::onTextChanged()
{
    clearErrorHighlights();
    m_textChangeTimer->start();
}

void StructureCodeEditor::onDelayedTextChanged()
{
    emit textChangedDelayed();
}

// ============================================================================
// StructurePreviewTree Implementation
// ============================================================================

StructurePreviewTree::StructurePreviewTree(QWidget *parent)
    : QTreeWidget(parent)
    , m_errorsItem(nullptr)
    , m_warningsItem(nullptr)
    , m_structuresItem(nullptr)
{
    setHeaderLabels({tr("Name"), tr("Type"), tr("Size"), tr("Offset")});
    setAlternatingRowColors(true);
    setRootIsDecorated(true);
    setItemsExpandable(true);
    setExpandsOnDoubleClick(true);

    // Set column widths
    setColumnWidth(0, 200);
    setColumnWidth(1, 150);
    setColumnWidth(2, 80);
    setColumnWidth(3, 80);
}

void StructurePreviewTree::updatePreview(const Monitor::Parser::StructureManager::ParseResult &result,
                                        Monitor::Parser::StructureManager *manager)
{
    clear();
    m_errorsItem = nullptr;
    m_warningsItem = nullptr;
    m_structuresItem = nullptr;

    // Show errors if any
    if (result.hasErrors()) {
        m_errorsItem = new QTreeWidgetItem(this, {tr("Errors"), "", "", ""});
        m_errorsItem->setForeground(0, QBrush(Qt::red));
        m_errorsItem->setIcon(0, style()->standardIcon(QStyle::SP_MessageBoxCritical));

        for (const auto &error : result.errors) {
            auto errorItem = new QTreeWidgetItem(m_errorsItem, {QString::fromStdString(error), "", "", ""});
            errorItem->setForeground(0, QBrush(Qt::red));
        }
        m_errorsItem->setExpanded(true);
    }

    // Show warnings if any
    if (result.hasWarnings()) {
        m_warningsItem = new QTreeWidgetItem(this, {tr("Warnings"), "", "", ""});
        m_warningsItem->setForeground(0, QBrush(QColor(255, 165, 0))); // Orange
        m_warningsItem->setIcon(0, style()->standardIcon(QStyle::SP_MessageBoxWarning));

        for (const auto &warning : result.warnings) {
            auto warningItem = new QTreeWidgetItem(m_warningsItem, {QString::fromStdString(warning), "", "", ""});
            warningItem->setForeground(0, QBrush(QColor(255, 165, 0)));
        }
        m_warningsItem->setExpanded(true);
    }

    // Show parsed structures
    if (result.success && manager) {
        m_structuresItem = new QTreeWidgetItem(this, {tr("Structures (%1)").arg(result.structuresParsed), "", "", ""});
        m_structuresItem->setIcon(0, style()->standardIcon(QStyle::SP_DirIcon));

        auto structNames = manager->getStructureNames();
        for (const auto &structName : structNames) {
            addStructureToTree(structName, manager);
        }

        if (result.structuresParsed > 0) {
            m_structuresItem->setExpanded(true);
        }
    }

    // Show summary
    auto summaryItem = new QTreeWidgetItem(this, {tr("Summary"), "", "", ""});
    summaryItem->setIcon(0, style()->standardIcon(QStyle::SP_FileDialogInfoView));

    new QTreeWidgetItem(summaryItem, {tr("Structures"), QString::number(result.structuresParsed), "", ""});
    new QTreeWidgetItem(summaryItem, {tr("Unions"), QString::number(result.unionsParsed), "", ""});
    new QTreeWidgetItem(summaryItem, {tr("Typedefs"), QString::number(result.typedefsParsed), "", ""});
    new QTreeWidgetItem(summaryItem, {tr("Parse Time"), QString("%1 ms").arg(result.totalTime.count()), "", ""});
}

void StructurePreviewTree::clear()
{
    QTreeWidget::clear();
    m_errorsItem = nullptr;
    m_warningsItem = nullptr;
    m_structuresItem = nullptr;
}

void StructurePreviewTree::showErrors(const std::vector<std::string> &errors)
{
    if (errors.empty()) return;

    if (!m_errorsItem) {
        m_errorsItem = new QTreeWidgetItem(this, {tr("Errors"), "", "", ""});
        m_errorsItem->setForeground(0, QBrush(Qt::red));
        m_errorsItem->setIcon(0, style()->standardIcon(QStyle::SP_MessageBoxCritical));
    }

    for (const auto &error : errors) {
        auto errorItem = new QTreeWidgetItem(m_errorsItem, {QString::fromStdString(error), "", "", ""});
        errorItem->setForeground(0, QBrush(Qt::red));
    }
    m_errorsItem->setExpanded(true);
}

void StructurePreviewTree::showWarnings(const std::vector<std::string> &warnings)
{
    if (warnings.empty()) return;

    if (!m_warningsItem) {
        m_warningsItem = new QTreeWidgetItem(this, {tr("Warnings"), "", "", ""});
        m_warningsItem->setForeground(0, QBrush(QColor(255, 165, 0)));
        m_warningsItem->setIcon(0, style()->standardIcon(QStyle::SP_MessageBoxWarning));
    }

    for (const auto &warning : warnings) {
        auto warningItem = new QTreeWidgetItem(m_warningsItem, {QString::fromStdString(warning), "", "", ""});
        warningItem->setForeground(0, QBrush(QColor(255, 165, 0)));
    }
    m_warningsItem->setExpanded(true);
}

void StructurePreviewTree::addStructureToTree(const std::string &structName,
                                             Monitor::Parser::StructureManager *manager)
{
    if (!manager || !m_structuresItem) return;

    try {
        auto structInfo = manager->getStructureInfos();

        // Find the structure info
        for (const auto &info : structInfo) {
            if (info.name == structName) {
                auto structItem = new QTreeWidgetItem(m_structuresItem, {
                    QString::fromStdString(structName),
                    "struct",
                    QString("%1 bytes").arg(info.totalSize),
                    "0"
                });
                structItem->setIcon(0, style()->standardIcon(QStyle::SP_ComputerIcon));

                // Add fields (this is a simplified version - would need more detailed field info from manager)
                if (info.fieldCount > 0) {
                    auto fieldsItem = new QTreeWidgetItem(structItem, {
                        tr("Fields (%1)").arg(info.fieldCount), "", "", ""
                    });
                    fieldsItem->setIcon(0, style()->standardIcon(QStyle::SP_FileIcon));
                }
                break;
            }
        }
    } catch (const std::exception &e) {
        qCWarning(addStructWindow) << "Error adding structure to tree:" << e.what();
    }
}

void StructurePreviewTree::addFieldToTree(QTreeWidgetItem *parent,
                                         const std::string &fieldName,
                                         const std::string &fieldType,
                                         size_t offset, size_t size)
{
    if (!parent) return;

    auto fieldItem = new QTreeWidgetItem(parent, {
        QString::fromStdString(fieldName),
        QString::fromStdString(fieldType),
        QString("%1 bytes").arg(size),
        QString("%1").arg(offset)
    });
    fieldItem->setIcon(0, style()->standardIcon(QStyle::SP_FileIcon));
}

// ============================================================================
// AddStructWindow Implementation
// ============================================================================

AddStructWindow::AddStructWindow(QWidget *parent)
    : QDialog(parent)
    , m_mainLayout(nullptr)
    , m_tabWidget(nullptr)
    , m_buttonLayout(nullptr)
    , m_headerDefineTab(nullptr)
    , m_reusableStructTab(nullptr)
    , m_packetStructTab(nullptr)
    , m_structureManager(nullptr)
    , m_hasUnsavedChanges(false)
    , m_currentTab(0)
{
    setWindowTitle(tr("Add Structure Window"));
    setWindowFlags(windowFlags() | Qt::Window);
    setModal(false);
    resize(1000, 700);

    setupUI();
    setupConnections();

    // Set default content
    m_defaultHeaderContent = tr("// Define the packet header structure here\n"
                               "// Example:\n"
                               "typedef struct {\n"
                               "    uint32_t packet_id;\n"
                               "    uint32_t sequence;\n"
                               "    uint64_t timestamp;\n"
                               "    uint16_t length;\n"
                               "    uint16_t checksum;\n"
                               "} PacketHeader;");

    m_defaultReusableContent = tr("// Define reusable structures here\n"
                                 "// Example:\n"
                                 "typedef struct {\n"
                                 "    float x;\n"
                                 "    float y;\n"
                                 "    float z;\n"
                                 "} Vector3D;\n\n"
                                 "typedef struct {\n"
                                 "    Vector3D position;\n"
                                 "    Vector3D velocity;\n"
                                 "} MotionData;");

    m_defaultPacketContent = tr("// Define complete packet structures here\n"
                               "// Must include the header at the top\n"
                               "// Example:\n"
                               "typedef struct {\n"
                               "    PacketHeader header;\n"
                               "    MotionData motion;\n"
                               "    float temperature;\n"
                               "    uint32_t status_flags;\n"
                               "} SensorPacket;");

    onResetToDefaults();

    qCInfo(addStructWindow) << "AddStructWindow created successfully";
}

AddStructWindow::~AddStructWindow()
{
    qCInfo(addStructWindow) << "AddStructWindow destroyed";
}

void AddStructWindow::setStructureManager(Monitor::Parser::StructureManager *manager)
{
    m_structureManager = manager;

    if (m_structureManager) {
        qCDebug(addStructWindow) << "Structure manager set successfully";

        // Connect to structure manager signals for real-time updates
        connect(m_structureManager, &Monitor::Parser::StructureManager::structureParsed,
                this, [this](const QString &name) {
                    qCDebug(addStructWindow) << "Structure parsed:" << name;
                    onParseAndPreview();
                });

        connect(m_structureManager, &Monitor::Parser::StructureManager::errorOccurred,
                this, [this](const QString &error) {
                    showError(tr("Parse Error"), error);
                });
    }
}

void AddStructWindow::showWindow()
{
    show();
    raise();
    activateWindow();
    emit windowShown();
    qCDebug(addStructWindow) << "Window shown";
}

void AddStructWindow::hideWindow()
{
    hide();
    emit windowHidden();
    qCDebug(addStructWindow) << "Window hidden";
}

bool AddStructWindow::isWindowVisible() const
{
    return isVisible();
}

void AddStructWindow::setupUI()
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(10, 10, 10, 10);
    m_mainLayout->setSpacing(10);

    setupTabWidget();
    setupButtonBox();

    setLayout(m_mainLayout);
}

void AddStructWindow::setupTabWidget()
{
    m_tabWidget = new QTabWidget(this);
    m_tabWidget->setTabPosition(QTabWidget::North);
    m_tabWidget->setMovable(false);

    createHeaderDefineTab();
    createReusableStructTab();
    createPacketStructTab();

    m_mainLayout->addWidget(m_tabWidget);
}

void AddStructWindow::createHeaderDefineTab()
{
    m_headerDefineTab = new QWidget();
    m_headerLayout = new QVBoxLayout(m_headerDefineTab);

    // Instruction label
    m_headerInstructionLabel = new QLabel(tr("Define the packet header structure that will be common to all incoming packets:"));
    m_headerInstructionLabel->setWordWrap(true);
    m_headerLayout->addWidget(m_headerInstructionLabel);

    // Code editor
    m_headerCodeEditor = new StructureCodeEditor();
    m_headerCodeEditor->setPlaceholderText(tr("Enter header structure definition..."));
    m_headerLayout->addWidget(m_headerCodeEditor, 1);

    // ID field layout
    m_headerIdLayout = new QHBoxLayout();
    m_headerIdLabel = new QLabel(tr("ID Field Name:"));
    m_headerIdField = new QLineEdit();
    m_headerIdField->setPlaceholderText(tr("packet_id"));
    m_headerIdField->setText("packet_id");
    m_validateHeaderButton = new QPushButton(tr("Validate"));

    m_headerIdLayout->addWidget(m_headerIdLabel);
    m_headerIdLayout->addWidget(m_headerIdField);
    m_headerIdLayout->addWidget(m_validateHeaderButton);
    m_headerIdLayout->addStretch();

    m_headerLayout->addLayout(m_headerIdLayout);

    // Preview tree
    m_headerPreviewTree = new StructurePreviewTree();
    m_headerPreviewTree->setMaximumHeight(200);
    m_headerLayout->addWidget(m_headerPreviewTree);

    // Button layout
    m_headerButtonLayout = new QHBoxLayout();
    m_saveHeaderButton = new QPushButton(tr("Save Header"));
    m_loadHeaderButton = new QPushButton(tr("Load Header"));

    m_headerButtonLayout->addWidget(m_saveHeaderButton);
    m_headerButtonLayout->addWidget(m_loadHeaderButton);
    m_headerButtonLayout->addStretch();

    m_headerLayout->addLayout(m_headerButtonLayout);

    m_tabWidget->addTab(m_headerDefineTab, tr("Header Define"));
}

void AddStructWindow::createReusableStructTab()
{
    m_reusableStructTab = new QWidget();
    m_reusableLayout = new QHBoxLayout(m_reusableStructTab);

    // Left side - input
    m_reusableLeftLayout = new QVBoxLayout();

    m_reusableInstructionLabel = new QLabel(tr("Define reusable structures that can be used as building blocks:"));
    m_reusableInstructionLabel->setWordWrap(true);
    m_reusableLeftLayout->addWidget(m_reusableInstructionLabel);

    m_reusableCodeEditor = new StructureCodeEditor();
    m_reusableCodeEditor->setPlaceholderText(tr("Enter reusable structure definitions..."));
    m_reusableLeftLayout->addWidget(m_reusableCodeEditor, 1);

    // Button layout for reusable
    m_reusableButtonLayout = new QHBoxLayout();
    m_addReusableButton = new QPushButton(tr("Add Structures"));
    m_clearReusableButton = new QPushButton(tr("Clear"));
    m_loadReusableButton = new QPushButton(tr("Load"));
    m_saveReusableButton = new QPushButton(tr("Save"));

    m_reusableButtonLayout->addWidget(m_addReusableButton);
    m_reusableButtonLayout->addWidget(m_clearReusableButton);
    m_reusableButtonLayout->addWidget(m_loadReusableButton);
    m_reusableButtonLayout->addWidget(m_saveReusableButton);

    m_reusableLeftLayout->addLayout(m_reusableButtonLayout);

    // Right side - preview
    m_reusableRightLayout = new QVBoxLayout();

    m_reusablePreviewLabel = new QLabel(tr("Structure Preview:"));
    m_reusableRightLayout->addWidget(m_reusablePreviewLabel);

    m_reusablePreviewTree = new StructurePreviewTree();
    m_reusableRightLayout->addWidget(m_reusablePreviewTree, 1);

    // Add to main layout
    m_reusableLayout->addLayout(m_reusableLeftLayout, 1);
    m_reusableLayout->addLayout(m_reusableRightLayout, 1);

    m_tabWidget->addTab(m_reusableStructTab, tr("Reusable Struct Define"));
}

void AddStructWindow::createPacketStructTab()
{
    m_packetStructTab = new QWidget();
    m_packetLayout = new QHBoxLayout(m_packetStructTab);

    // Left side - input
    m_packetLeftLayout = new QVBoxLayout();

    m_packetInstructionLabel = new QLabel(tr("Define complete packet structures with unique IDs:"));
    m_packetInstructionLabel->setWordWrap(true);
    m_packetLeftLayout->addWidget(m_packetInstructionLabel);

    m_packetCodeEditor = new StructureCodeEditor();
    m_packetCodeEditor->setPlaceholderText(tr("Enter packet structure definition..."));
    m_packetLeftLayout->addWidget(m_packetCodeEditor, 1);

    // Packet ID layout
    m_packetIdLayout = new QHBoxLayout();
    m_packetIdLabel = new QLabel(tr("Packet ID:"));
    m_packetIdSpinBox = new QSpinBox();
    m_packetIdSpinBox->setRange(1, 65535);
    m_packetIdSpinBox->setValue(1);

    m_packetIdLayout->addWidget(m_packetIdLabel);
    m_packetIdLayout->addWidget(m_packetIdSpinBox);
    m_packetIdLayout->addStretch();

    m_packetLeftLayout->addLayout(m_packetIdLayout);

    // Button layout for packet
    m_packetButtonLayout = new QHBoxLayout();
    m_addPacketButton = new QPushButton(tr("Add Packet"));
    m_clearPacketButton = new QPushButton(tr("Clear"));
    m_loadPacketButton = new QPushButton(tr("Load"));
    m_savePacketButton = new QPushButton(tr("Save"));

    m_packetButtonLayout->addWidget(m_addPacketButton);
    m_packetButtonLayout->addWidget(m_clearPacketButton);
    m_packetButtonLayout->addWidget(m_loadPacketButton);
    m_packetButtonLayout->addWidget(m_savePacketButton);

    m_packetLeftLayout->addLayout(m_packetButtonLayout);

    // Right side - preview
    m_packetRightLayout = new QVBoxLayout();

    m_packetPreviewLabel = new QLabel(tr("Packet Structure Preview:"));
    m_packetRightLayout->addWidget(m_packetPreviewLabel);

    m_packetPreviewTree = new StructurePreviewTree();
    m_packetRightLayout->addWidget(m_packetPreviewTree, 1);

    // Add to main layout
    m_packetLayout->addLayout(m_packetLeftLayout, 1);
    m_packetLayout->addLayout(m_packetRightLayout, 1);

    m_tabWidget->addTab(m_packetStructTab, tr("Packet Structs Define"));
}

void AddStructWindow::setupButtonBox()
{
    m_bottomButtonLayout = new QHBoxLayout();

    m_applyButton = new QPushButton(tr("Apply Changes"));
    m_applyButton->setDefault(true);
    m_cancelButton = new QPushButton(tr("Cancel"));
    m_resetButton = new QPushButton(tr("Reset to Defaults"));

    m_bottomButtonLayout->addStretch();
    m_bottomButtonLayout->addWidget(m_resetButton);
    m_bottomButtonLayout->addWidget(m_cancelButton);
    m_bottomButtonLayout->addWidget(m_applyButton);

    m_mainLayout->addLayout(m_bottomButtonLayout);
}

void AddStructWindow::setupConnections()
{
    // Tab widget
    connect(m_tabWidget, &QTabWidget::currentChanged, this, &AddStructWindow::onTabChanged);

    // Header tab connections
    connect(m_headerCodeEditor, &StructureCodeEditor::textChangedDelayed,
            this, &AddStructWindow::onHeaderCodeChanged);
    connect(m_headerIdField, &QLineEdit::textChanged,
            this, &AddStructWindow::onHeaderIdFieldChanged);
    connect(m_validateHeaderButton, &QPushButton::clicked,
            this, &AddStructWindow::onValidateHeaderClicked);
    connect(m_saveHeaderButton, &QPushButton::clicked,
            this, &AddStructWindow::onSaveHeaderClicked);
    connect(m_loadHeaderButton, &QPushButton::clicked,
            this, &AddStructWindow::onLoadHeaderClicked);

    // Reusable struct tab connections
    connect(m_reusableCodeEditor, &StructureCodeEditor::textChangedDelayed,
            this, &AddStructWindow::onReusableCodeChanged);
    connect(m_addReusableButton, &QPushButton::clicked,
            this, &AddStructWindow::onAddReusableStructClicked);
    connect(m_clearReusableButton, &QPushButton::clicked,
            this, &AddStructWindow::onClearReusableStructClicked);
    connect(m_loadReusableButton, &QPushButton::clicked,
            this, &AddStructWindow::onLoadReusableStructClicked);
    connect(m_saveReusableButton, &QPushButton::clicked,
            this, &AddStructWindow::onSaveReusableStructClicked);

    // Packet struct tab connections
    connect(m_packetCodeEditor, &StructureCodeEditor::textChangedDelayed,
            this, &AddStructWindow::onPacketCodeChanged);
    connect(m_packetIdSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &AddStructWindow::onPacketIdChanged);
    connect(m_addPacketButton, &QPushButton::clicked,
            this, &AddStructWindow::onAddPacketStructClicked);
    connect(m_clearPacketButton, &QPushButton::clicked,
            this, &AddStructWindow::onClearPacketStructClicked);
    connect(m_loadPacketButton, &QPushButton::clicked,
            this, &AddStructWindow::onLoadPacketStructClicked);
    connect(m_savePacketButton, &QPushButton::clicked,
            this, &AddStructWindow::onSavePacketStructClicked);

    // Bottom button connections
    connect(m_applyButton, &QPushButton::clicked,
            this, &AddStructWindow::onApplyChanges);
    connect(m_cancelButton, &QPushButton::clicked,
            this, &AddStructWindow::onCancelChanges);
    connect(m_resetButton, &QPushButton::clicked,
            this, &AddStructWindow::onResetToDefaults);
}

// ============================================================================
// Event Handlers
// ============================================================================

void AddStructWindow::onTabChanged(int index)
{
    m_currentTab = index;
    updateButtonStates();

    qCDebug(addStructWindow) << "Tab changed to:" << index;
}

void AddStructWindow::onHeaderCodeChanged()
{
    m_hasUnsavedChanges = true;
    updateHeaderPreview();
}

void AddStructWindow::onHeaderIdFieldChanged()
{
    m_hasUnsavedChanges = true;
    m_currentHeaderIdField = m_headerIdField->text();
}

void AddStructWindow::onValidateHeaderClicked()
{
    parseHeaderStructure();
}

void AddStructWindow::onSaveHeaderClicked()
{
    QString filename = QFileDialog::getSaveFileName(this,
        tr("Save Header Structure"), getDefaultSaveLocation() + "/header.h",
        tr("Header Files (*.h);;All Files (*)"));

    if (!filename.isEmpty()) {
        QFile file(filename);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&file);
            out << m_headerCodeEditor->toPlainText();
            qCInfo(addStructWindow) << "Header saved to:" << filename;
        } else {
            showError(tr("Save Error"), tr("Could not save header to file: %1").arg(filename));
        }
    }
}

void AddStructWindow::onLoadHeaderClicked()
{
    QString filename = QFileDialog::getOpenFileName(this,
        tr("Load Header Structure"), getDefaultSaveLocation(),
        tr("Header Files (*.h);;All Files (*)"));

    if (!filename.isEmpty()) {
        QFile file(filename);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&file);
            m_headerCodeEditor->setPlainText(in.readAll());
            m_hasUnsavedChanges = true;
            qCInfo(addStructWindow) << "Header loaded from:" << filename;
        } else {
            showError(tr("Load Error"), tr("Could not load header from file: %1").arg(filename));
        }
    }
}

void AddStructWindow::onReusableCodeChanged()
{
    m_hasUnsavedChanges = true;
    updateReusablePreview();
}

void AddStructWindow::onAddReusableStructClicked()
{
    parseReusableStructures();
}

void AddStructWindow::onClearReusableStructClicked()
{
    m_reusableCodeEditor->clear();
    m_reusablePreviewTree->clear();
    m_hasUnsavedChanges = true;
}

void AddStructWindow::onLoadReusableStructClicked()
{
    QString filename = QFileDialog::getOpenFileName(this,
        tr("Load Reusable Structures"), getDefaultSaveLocation(),
        tr("Header Files (*.h);;All Files (*)"));

    if (!filename.isEmpty()) {
        QFile file(filename);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&file);
            m_reusableCodeEditor->setPlainText(in.readAll());
            m_hasUnsavedChanges = true;
            qCInfo(addStructWindow) << "Reusable structures loaded from:" << filename;
        }
    }
}

void AddStructWindow::onSaveReusableStructClicked()
{
    QString filename = QFileDialog::getSaveFileName(this,
        tr("Save Reusable Structures"), getDefaultSaveLocation() + "/reusable.h",
        tr("Header Files (*.h);;All Files (*)"));

    if (!filename.isEmpty()) {
        QFile file(filename);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&file);
            out << m_reusableCodeEditor->toPlainText();
            qCInfo(addStructWindow) << "Reusable structures saved to:" << filename;
        }
    }
}

void AddStructWindow::onPacketCodeChanged()
{
    m_hasUnsavedChanges = true;
    updatePacketPreview();
}

void AddStructWindow::onPacketIdChanged()
{
    m_hasUnsavedChanges = true;
    validatePacketStructure();
}

void AddStructWindow::onAddPacketStructClicked()
{
    parsePacketStructure();
}

void AddStructWindow::onClearPacketStructClicked()
{
    m_packetCodeEditor->clear();
    m_packetPreviewTree->clear();
    m_hasUnsavedChanges = true;
}

void AddStructWindow::onLoadPacketStructClicked()
{
    QString filename = QFileDialog::getOpenFileName(this,
        tr("Load Packet Structure"), getDefaultSaveLocation(),
        tr("Header Files (*.h);;All Files (*)"));

    if (!filename.isEmpty()) {
        QFile file(filename);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&file);
            m_packetCodeEditor->setPlainText(in.readAll());
            m_hasUnsavedChanges = true;
            qCInfo(addStructWindow) << "Packet structure loaded from:" << filename;
        }
    }
}

void AddStructWindow::onSavePacketStructClicked()
{
    QString filename = QFileDialog::getSaveFileName(this,
        tr("Save Packet Structure"), getDefaultSaveLocation() + "/packet.h",
        tr("Header Files (*.h);;All Files (*)"));

    if (!filename.isEmpty()) {
        QFile file(filename);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&file);
            out << m_packetCodeEditor->toPlainText();
            qCInfo(addStructWindow) << "Packet structure saved to:" << filename;
        }
    }
}

void AddStructWindow::onApplyChanges()
{
    if (m_structureManager) {
        // Parse and apply all structures
        parseHeaderStructure();
        parseReusableStructures();
        parsePacketStructure();

        m_hasUnsavedChanges = false;
        updateButtonStates();

        QMessageBox::information(this, tr("Changes Applied"),
                                tr("All structure changes have been applied successfully."));
    } else {
        showError(tr("Apply Error"), tr("No structure manager available."));
    }
}

void AddStructWindow::onCancelChanges()
{
    if (m_hasUnsavedChanges) {
        int ret = QMessageBox::question(this, tr("Unsaved Changes"),
                                       tr("You have unsaved changes. Do you want to discard them?"),
                                       QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (ret == QMessageBox::Yes) {
            hide();
        }
    } else {
        hide();
    }
}

void AddStructWindow::onResetToDefaults()
{
    m_headerCodeEditor->setPlainText(m_defaultHeaderContent);
    m_headerIdField->setText("packet_id");
    m_reusableCodeEditor->setPlainText(m_defaultReusableContent);
    m_packetCodeEditor->setPlainText(m_defaultPacketContent);
    m_packetIdSpinBox->setValue(1);

    // Clear preview trees
    m_headerPreviewTree->clear();
    m_reusablePreviewTree->clear();
    m_packetPreviewTree->clear();

    m_hasUnsavedChanges = false;
    updateButtonStates();

    qCInfo(addStructWindow) << "Reset to default content";
}

// ============================================================================
// Parsing and Validation
// ============================================================================

void AddStructWindow::parseHeaderStructure()
{
    if (!m_structureManager) {
        showError(tr("Parse Error"), tr("No structure manager available."));
        return;
    }

    QString code = m_headerCodeEditor->toPlainText();
    if (code.trimmed().isEmpty()) {
        m_headerPreviewTree->clear();
        return;
    }

    try {
        auto result = m_structureManager->parseStructures(code.toStdString());
        m_headerPreviewTree->updatePreview(result, m_structureManager);

        if (result.success) {
            m_currentHeaderStruct = code;
            emit statusMessage(tr("Header structure parsed successfully."));
        } else {
            emit parseError(tr("Header parsing failed with %1 errors.").arg(result.errors.size()));
        }
    } catch (const std::exception &e) {
        showError(tr("Parse Error"), tr("Exception during header parsing: %1").arg(e.what()));
    }
}

void AddStructWindow::parseReusableStructures()
{
    if (!m_structureManager) {
        showError(tr("Parse Error"), tr("No structure manager available."));
        return;
    }

    QString code = m_reusableCodeEditor->toPlainText();
    if (code.trimmed().isEmpty()) {
        m_reusablePreviewTree->clear();
        return;
    }

    try {
        auto result = m_structureManager->parseStructures(code.toStdString());
        m_reusablePreviewTree->updatePreview(result, m_structureManager);

        if (result.success) {
            emit statusMessage(tr("Reusable structures parsed successfully."));
        } else {
            emit parseError(tr("Reusable structure parsing failed with %1 errors.").arg(result.errors.size()));
        }
    } catch (const std::exception &e) {
        showError(tr("Parse Error"), tr("Exception during reusable structure parsing: %1").arg(e.what()));
    }
}

void AddStructWindow::parsePacketStructure()
{
    if (!m_structureManager) {
        showError(tr("Parse Error"), tr("No structure manager available."));
        return;
    }

    QString code = m_packetCodeEditor->toPlainText();
    if (code.trimmed().isEmpty()) {
        m_packetPreviewTree->clear();
        return;
    }

    try {
        auto result = m_structureManager->parseStructures(code.toStdString());
        m_packetPreviewTree->updatePreview(result, m_structureManager);

        if (result.success) {
            int packetId = m_packetIdSpinBox->value();
            if (validatePacketId(packetId)) {
                emit statusMessage(tr("Packet structure parsed successfully with ID %1.").arg(packetId));
            }
        } else {
            emit parseError(tr("Packet structure parsing failed with %1 errors.").arg(result.errors.size()));
        }
    } catch (const std::exception &e) {
        showError(tr("Parse Error"), tr("Exception during packet structure parsing: %1").arg(e.what()));
    }
}

void AddStructWindow::validatePacketStructure()
{
    // Check if packet ID is unique
    int packetId = m_packetIdSpinBox->value();
    validatePacketId(packetId);
}

bool AddStructWindow::validatePacketId(int packetId)
{
    if (m_packetStructures.contains(packetId)) {
        showWarning(tr("Duplicate ID"), tr("Packet ID %1 is already in use.").arg(packetId));
        return false;
    }
    return true;
}

// ============================================================================
// UI Update Helpers
// ============================================================================

void AddStructWindow::updateHeaderPreview()
{
    // Auto-parse on delay to show real-time preview
    QTimer::singleShot(1000, this, &AddStructWindow::parseHeaderStructure);
}

void AddStructWindow::updateReusablePreview()
{
    // Auto-parse on delay to show real-time preview
    QTimer::singleShot(1000, this, &AddStructWindow::parseReusableStructures);
}

void AddStructWindow::updatePacketPreview()
{
    // Auto-parse on delay to show real-time preview
    QTimer::singleShot(1000, this, &AddStructWindow::parsePacketStructure);
}

void AddStructWindow::updateButtonStates()
{
    m_applyButton->setEnabled(m_hasUnsavedChanges && m_structureManager != nullptr);
}

void AddStructWindow::showError(const QString &title, const QString &message)
{
    QMessageBox::critical(this, title, message);
    qCWarning(addStructWindow) << title << ":" << message;
}

void AddStructWindow::showWarning(const QString &title, const QString &message)
{
    QMessageBox::warning(this, title, message);
    qCWarning(addStructWindow) << title << ":" << message;
}

void AddStructWindow::clearErrors()
{
    m_headerCodeEditor->clearErrorHighlights();
    m_reusableCodeEditor->clearErrorHighlights();
    m_packetCodeEditor->clearErrorHighlights();
}

QString AddStructWindow::getDefaultSaveLocation() const
{
    return QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/MonitorStructures";
}

// ============================================================================
// Event Handlers
// ============================================================================

void AddStructWindow::closeEvent(QCloseEvent *event)
{
    if (m_hasUnsavedChanges) {
        int ret = QMessageBox::question(this, tr("Unsaved Changes"),
                                       tr("You have unsaved changes. Do you want to save them?"),
                                       QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
                                       QMessageBox::Save);

        if (ret == QMessageBox::Save) {
            onApplyChanges();
            event->accept();
        } else if (ret == QMessageBox::Discard) {
            event->accept();
        } else {
            event->ignore();
            return;
        }
    }

    QDialog::closeEvent(event);
}

void AddStructWindow::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    emit windowShown();
}

void AddStructWindow::hideEvent(QHideEvent *event)
{
    QDialog::hideEvent(event);
    emit windowHidden();
}

QJsonObject AddStructWindow::saveState() const
{
    QJsonObject state;
    state["headerCode"] = m_headerCodeEditor->toPlainText();
    state["headerIdField"] = m_headerIdField->text();
    state["reusableCode"] = m_reusableCodeEditor->toPlainText();
    state["packetCode"] = m_packetCodeEditor->toPlainText();
    state["packetId"] = m_packetIdSpinBox->value();
    state["currentTab"] = m_currentTab;
    return state;
}

bool AddStructWindow::restoreState(const QJsonObject &state)
{
    if (state.contains("headerCode")) {
        m_headerCodeEditor->setPlainText(state["headerCode"].toString());
    }
    if (state.contains("headerIdField")) {
        m_headerIdField->setText(state["headerIdField"].toString());
    }
    if (state.contains("reusableCode")) {
        m_reusableCodeEditor->setPlainText(state["reusableCode"].toString());
    }
    if (state.contains("packetCode")) {
        m_packetCodeEditor->setPlainText(state["packetCode"].toString());
    }
    if (state.contains("packetId")) {
        m_packetIdSpinBox->setValue(state["packetId"].toInt());
    }
    if (state.contains("currentTab")) {
        m_tabWidget->setCurrentIndex(state["currentTab"].toInt());
    }

    m_hasUnsavedChanges = false;
    updateButtonStates();

    return true;
}

void AddStructWindow::setActiveTab(int index)
{
    if (index >= 0 && index < m_tabWidget->count()) {
        m_tabWidget->setCurrentIndex(index);
    }
}

int AddStructWindow::getActiveTab() const
{
    return m_tabWidget->currentIndex();
}

// Missing slot implementations required by Qt MOC system
void AddStructWindow::onHeaderDefineTabSelected()
{
    qCDebug(addStructWindow) << "Header Define tab selected";
    m_currentTab = 0;
}

void AddStructWindow::onReusableStructTabSelected()
{
    qCDebug(addStructWindow) << "Reusable Struct tab selected";
    m_currentTab = 1;
}

void AddStructWindow::onPacketStructTabSelected()
{
    qCDebug(addStructWindow) << "Packet Struct tab selected";
    m_currentTab = 2;
}

void AddStructWindow::onParseAndPreview()
{
    qCDebug(addStructWindow) << "Parse and preview requested";

    switch (m_currentTab) {
    case 0:
        parseHeaderStructure();
        break;
    case 1:
        parseReusableStructures();
        break;
    case 2:
        parsePacketStructure();
        break;
    default:
        qCWarning(addStructWindow) << "Unknown tab for parse and preview:" << m_currentTab;
        break;
    }
}

void AddStructWindow::onWindowClosed()
{
    qCDebug(addStructWindow) << "Window closed";
    hide();
}

// Qt's meta-object system will automatically generate the moc file