#ifndef ADD_STRUCT_WINDOW_H
#define ADD_STRUCT_WINDOW_H

#include "../../parser/manager/structure_manager.h"
#include <QDialog>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QTabWidget>
#include <QTextEdit>
#include <QPlainTextEdit>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QPushButton>
#include <QSplitter>
#include <QGroupBox>
#include <QFrame>
#include <QScrollArea>
#include <QMessageBox>
#include <QFileDialog>
#include <QJsonObject>
#include <QJsonDocument>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QRegularExpression>
#include <QTimer>
#include <memory>

// Forward declarations
namespace Monitor {
    namespace Parser {
        class StructureManager;
    }
}

/**
 * @brief C++ syntax highlighter for code editors
 */
class CppSyntaxHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT

public:
    explicit CppSyntaxHighlighter(QTextDocument *parent = nullptr);

protected:
    void highlightBlock(const QString &text) override;

private:
    struct HighlightingRule
    {
        QRegularExpression pattern;
        QTextCharFormat format;
    };
    QVector<HighlightingRule> highlightingRules;

    QTextCharFormat keywordFormat;
    QTextCharFormat classFormat;
    QTextCharFormat singleLineCommentFormat;
    QTextCharFormat multiLineCommentFormat;
    QTextCharFormat quotationFormat;
    QTextCharFormat functionFormat;
    QTextCharFormat typeFormat;
};

/**
 * @brief Custom text editor with C++ syntax highlighting
 */
class StructureCodeEditor : public QPlainTextEdit
{
    Q_OBJECT

public:
    explicit StructureCodeEditor(QWidget *parent = nullptr);

    void setPlaceholderText(const QString &text);
    QString getPlaceholderText() const { return m_placeholderText; }

    // Error highlighting
    void highlightError(int line, const QString &message);
    void clearErrorHighlights();

signals:
    void textChangedDelayed();

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void onTextChanged();
    void onDelayedTextChanged();

private:
    void updateLineNumberAreaWidth(int newBlockCount = 0);
    void updateLineNumberArea(const QRect &rect, int dy);

    CppSyntaxHighlighter *m_highlighter;
    QWidget *m_lineNumberArea;
    QString m_placeholderText;
    QTimer *m_textChangeTimer;
    QList<int> m_errorLines;
    QStringList m_errorMessages;
};

/**
 * @brief Preview tree widget for showing parsed structure
 */
class StructurePreviewTree : public QTreeWidget
{
    Q_OBJECT

public:
    explicit StructurePreviewTree(QWidget *parent = nullptr);

    void updatePreview(const Monitor::Parser::StructureManager::ParseResult &result,
                      Monitor::Parser::StructureManager *manager);
    void clear();

    void showErrors(const std::vector<std::string> &errors);
    void showWarnings(const std::vector<std::string> &warnings);

private:
    void addStructureToTree(const std::string &structName,
                           Monitor::Parser::StructureManager *manager);
    void addFieldToTree(QTreeWidgetItem *parent,
                       const std::string &fieldName,
                       const std::string &fieldType,
                       size_t offset, size_t size);

    QTreeWidgetItem *m_errorsItem;
    QTreeWidgetItem *m_warningsItem;
    QTreeWidgetItem *m_structuresItem;
};

/**
 * @brief Add Structure Window - Main dialog for defining C++ structures
 *
 * This window provides a comprehensive interface for defining and managing
 * C++ structures used in packet processing. It features three tabs:
 *
 * 1. Header Define - Define the common packet header structure
 * 2. Reusable Struct Define - Define reusable structure components
 * 3. Packet Structs Define - Define complete packet structures with IDs
 */
class AddStructWindow : public QDialog
{
    Q_OBJECT

public:
    explicit AddStructWindow(QWidget *parent = nullptr);
    ~AddStructWindow();

    // Structure manager integration
    void setStructureManager(Monitor::Parser::StructureManager *manager);
    Monitor::Parser::StructureManager* getStructureManager() const { return m_structureManager; }

    // Window management
    void showWindow();
    void hideWindow();
    bool isWindowVisible() const;

    // Data persistence
    QJsonObject saveState() const;
    bool restoreState(const QJsonObject &state);

    // Tab management
    void setActiveTab(int index);
    int getActiveTab() const;

public slots:
    // Tab switching
    void onHeaderDefineTabSelected();
    void onReusableStructTabSelected();
    void onPacketStructTabSelected();

    // Header Define tab actions
    void onHeaderCodeChanged();
    void onHeaderIdFieldChanged();
    void onSaveHeaderClicked();
    void onLoadHeaderClicked();
    void onValidateHeaderClicked();

    // Reusable Struct tab actions
    void onReusableCodeChanged();
    void onAddReusableStructClicked();
    void onClearReusableStructClicked();
    void onLoadReusableStructClicked();
    void onSaveReusableStructClicked();

    // Packet Struct tab actions
    void onPacketCodeChanged();
    void onPacketIdChanged();
    void onAddPacketStructClicked();
    void onClearPacketStructClicked();
    void onLoadPacketStructClicked();
    void onSavePacketStructClicked();

    // Common actions
    void onParseAndPreview();
    void onApplyChanges();
    void onCancelChanges();
    void onResetToDefaults();

signals:
    // Structure management signals
    void headerStructureAdded(const QString &structName);
    void reusableStructureAdded(const QString &structName);
    void packetStructureAdded(const QString &structName, int packetId);

    // Window events
    void windowShown();
    void windowHidden();

    // Error/status signals
    void parseError(const QString &message);
    void parseWarning(const QString &message);
    void statusMessage(const QString &message);

protected:
    void closeEvent(QCloseEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private slots:
    void onTabChanged(int index);
    void onWindowClosed();

private:
    // UI setup methods
    void setupUI();
    void setupTabWidget();
    void setupHeaderDefineTab();
    void setupReusableStructTab();
    void setupPacketStructTab();
    void setupButtonBox();
    void setupConnections();

    // Header Define tab components
    void createHeaderDefineTab();
    QWidget* createHeaderDefineWidget();

    // Reusable Struct tab components
    void createReusableStructTab();
    QWidget* createReusableStructWidget();

    // Packet Struct tab components
    void createPacketStructTab();
    QWidget* createPacketStructWidget();

    // Parsing and validation helpers
    void parseHeaderStructure();
    void parseReusableStructures();
    void parsePacketStructure();
    void validatePacketStructure();
    bool validatePacketId(int packetId);

    // UI update helpers
    void updateHeaderPreview();
    void updateReusablePreview();
    void updatePacketPreview();
    void updateButtonStates();
    void showParseResults(const Monitor::Parser::StructureManager::ParseResult &result);

    // Error handling
    void showError(const QString &title, const QString &message);
    void showWarning(const QString &title, const QString &message);
    void clearErrors();

    // File operations
    bool saveStructuresToFile(const QString &filePath);
    bool loadStructuresFromFile(const QString &filePath);
    QString getDefaultSaveLocation() const;

    // Main layout
    QVBoxLayout *m_mainLayout;
    QTabWidget *m_tabWidget;
    QHBoxLayout *m_buttonLayout;

    // Tab widgets
    QWidget *m_headerDefineTab;
    QWidget *m_reusableStructTab;
    QWidget *m_packetStructTab;

    // Header Define tab components
    QVBoxLayout *m_headerLayout;
    QLabel *m_headerInstructionLabel;
    StructureCodeEditor *m_headerCodeEditor;
    QHBoxLayout *m_headerIdLayout;
    QLabel *m_headerIdLabel;
    QLineEdit *m_headerIdField;
    QPushButton *m_validateHeaderButton;
    StructurePreviewTree *m_headerPreviewTree;
    QHBoxLayout *m_headerButtonLayout;
    QPushButton *m_saveHeaderButton;
    QPushButton *m_loadHeaderButton;

    // Reusable Struct tab components
    QHBoxLayout *m_reusableLayout;
    QVBoxLayout *m_reusableLeftLayout;
    QLabel *m_reusableInstructionLabel;
    StructureCodeEditor *m_reusableCodeEditor;
    QHBoxLayout *m_reusableButtonLayout;
    QPushButton *m_addReusableButton;
    QPushButton *m_clearReusableButton;
    QPushButton *m_loadReusableButton;
    QPushButton *m_saveReusableButton;
    QVBoxLayout *m_reusableRightLayout;
    QLabel *m_reusablePreviewLabel;
    StructurePreviewTree *m_reusablePreviewTree;

    // Packet Struct tab components
    QHBoxLayout *m_packetLayout;
    QVBoxLayout *m_packetLeftLayout;
    QLabel *m_packetInstructionLabel;
    StructureCodeEditor *m_packetCodeEditor;
    QHBoxLayout *m_packetIdLayout;
    QLabel *m_packetIdLabel;
    QSpinBox *m_packetIdSpinBox;
    QHBoxLayout *m_packetButtonLayout;
    QPushButton *m_addPacketButton;
    QPushButton *m_clearPacketButton;
    QPushButton *m_loadPacketButton;
    QPushButton *m_savePacketButton;
    QVBoxLayout *m_packetRightLayout;
    QLabel *m_packetPreviewLabel;
    StructurePreviewTree *m_packetPreviewTree;

    // Bottom button box
    QHBoxLayout *m_bottomButtonLayout;
    QPushButton *m_applyButton;
    QPushButton *m_cancelButton;
    QPushButton *m_resetButton;

    // Data members
    Monitor::Parser::StructureManager *m_structureManager;
    QString m_currentHeaderStruct;
    QString m_currentHeaderIdField;
    QStringList m_reusableStructures;
    QMap<int, QString> m_packetStructures;

    // State tracking
    bool m_hasUnsavedChanges;
    int m_currentTab;

    // Default content
    QString m_defaultHeaderContent;
    QString m_defaultReusableContent;
    QString m_defaultPacketContent;
};

#endif // ADD_STRUCT_WINDOW_H