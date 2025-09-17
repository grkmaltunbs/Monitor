#include "structure_manager.h"
#include "../parser/struct_parser.h"
#include <QDebug>
#include <QFileInfo>
#include <QTextStream>
#include <QFile>

namespace Monitor {
namespace Parser {

StructureManager::StructureManager(QObject* parent)
    : QObject(parent)
    , m_compilerType(Layout::AlignmentRules::CompilerType::AUTO_DETECT)
    , m_architecture(Layout::AlignmentRules::Architecture::AUTO_DETECT)
{
    // Initialize the parser
    m_parser = std::make_unique<StructParser>();
    qDebug() << "StructureManager initialized with real parser";
}

StructureManager::~StructureManager() {
    // Cleanup will be implemented later
}

StructureManager::ParseResult StructureManager::parseStructures(const std::string& sourceCode) {
    clearErrors();

    ParseResult result;

    try {
        // Use the real parser
        auto parseResult = m_parser->parse(sourceCode);

        result.success = parseResult.success;
        result.errors = parseResult.errors;
        result.warnings = parseResult.warnings;
        result.structuresParsed = parseResult.structures.size();
        result.unionsParsed = parseResult.unions.size();
        result.typedefsParsed = parseResult.typedefs.size();
        result.parseTime = parseResult.parseTime;

        if (parseResult.success) {
            // Store parsed structures
            for (auto& structDecl : parseResult.structures) {
                if (structDecl) {
                    std::string name = structDecl->getName();
                    m_structures[name] = std::move(structDecl);
                    qDebug() << "Stored structure:" << QString::fromStdString(name);
                }
            }

            // Store parsed unions
            for (auto& unionDecl : parseResult.unions) {
                if (unionDecl) {
                    std::string name = unionDecl->getName();
                    m_unions[name] = std::move(unionDecl);
                    qDebug() << "Stored union:" << QString::fromStdString(name);
                }
            }

            // Store parsed typedefs
            for (auto& typedefDecl : parseResult.typedefs) {
                if (typedefDecl) {
                    std::string name = typedefDecl->getName();
                    m_typedefs[name] = std::move(typedefDecl);
                    qDebug() << "Stored typedef:" << QString::fromStdString(name);
                }
            }

            // Emit signal for successful parsing
            emit parseCompleted(result);
        } else {
            // Copy errors to member variable for later retrieval
            for (const auto& error : parseResult.errors) {
                StructureError structError(error, "", "", 0, 0);
                m_errors.push_back(structError);
            }
        }

    } catch (const std::exception& e) {
        result.success = false;
        result.errors.push_back("Exception during parsing: " + std::string(e.what()));

        StructureError structError(e.what(), "", "", 0, 0);
        m_errors.push_back(structError);
    }

    return result;
}

StructureManager::ParseResult StructureManager::parseStructuresFromFile(const QString& filePath) {
    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists()) {
        ParseResult result;
        result.success = false;
        result.errors.push_back("File does not exist: " + filePath.toStdString());
        return result;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        ParseResult result;
        result.success = false;
        result.errors.push_back("Cannot open file: " + filePath.toStdString());
        return result;
    }

    QTextStream in(&file);
    QString content = in.readAll();

    return parseStructures(content.toStdString());
}

StructureManager::ParseResult StructureManager::parseStructuresFromFiles(const QStringList& filePaths) {
    ParseResult combinedResult;
    combinedResult.success = true;

    for (const QString& filePath : filePaths) {
        auto result = parseStructuresFromFile(filePath);

        // Combine results
        combinedResult.errors.insert(combinedResult.errors.end(),
                                   result.errors.begin(), result.errors.end());
        combinedResult.warnings.insert(combinedResult.warnings.end(),
                                     result.warnings.begin(), result.warnings.end());
        combinedResult.structuresParsed += result.structuresParsed;
        combinedResult.unionsParsed += result.unionsParsed;
        combinedResult.typedefsParsed += result.typedefsParsed;
        combinedResult.parseTime += result.parseTime;

        if (!result.success) {
            combinedResult.success = false;
        }
    }

    return combinedResult;
}

bool StructureManager::hasStructure(const std::string& name) const {
    return m_structures.find(name) != m_structures.end();
}

std::shared_ptr<const AST::StructDeclaration> StructureManager::getStructure(const std::string& name) const {
    auto it = m_structures.find(name);
    if (it != m_structures.end()) {
        return it->second;
    }
    return nullptr;
}

std::vector<std::string> StructureManager::getStructureNames() const {
    std::vector<std::string> names;
    for (const auto& pair : m_structures) {
        names.push_back(pair.first);
    }
    return names;
}

std::vector<StructureManager::StructureInfo> StructureManager::getStructureInfos() const {
    std::vector<StructureInfo> infos;
    for (const auto& pair : m_structures) {
        StructureInfo info;
        info.name = pair.first;
        if (pair.second) {
            info.fieldCount = pair.second->getFieldCount();
            info.totalSize = pair.second->getTotalSize();
            info.alignment = pair.second->getAlignment();
            info.isPacked = pair.second->isPacked();
            info.dependencies = pair.second->getDependencies();
        }
        infos.push_back(info);
    }
    return infos;
}

bool StructureManager::hasUnion(const std::string& name) const {
    return m_unions.find(name) != m_unions.end();
}

std::shared_ptr<const AST::UnionDeclaration> StructureManager::getUnion(const std::string& name) const {
    auto it = m_unions.find(name);
    if (it != m_unions.end()) {
        return it->second;
    }
    return nullptr;
}

std::vector<std::string> StructureManager::getUnionNames() const {
    std::vector<std::string> names;
    for (const auto& pair : m_unions) {
        names.push_back(pair.first);
    }
    return names;
}

bool StructureManager::hasTypedef(const std::string& name) const {
    return m_typedefs.find(name) != m_typedefs.end();
}

std::shared_ptr<const AST::TypedefDeclaration> StructureManager::getTypedef(const std::string& name) const {
    auto it = m_typedefs.find(name);
    if (it != m_typedefs.end()) {
        return it->second;
    }
    return nullptr;
}

std::vector<std::string> StructureManager::getTypedefNames() const {
    std::vector<std::string> names;
    for (const auto& pair : m_typedefs) {
        names.push_back(pair.first);
    }
    return names;
}

bool StructureManager::hasLayout(const std::string& structName) const {
    Q_UNUSED(structName);
    return false;
}

Layout::LayoutCalculator::StructLayout StructureManager::getStructLayout(const std::string& name) const {
    Q_UNUSED(name);
    return Layout::LayoutCalculator::StructLayout();
}

Layout::LayoutCalculator::UnionLayout StructureManager::getUnionLayout(const std::string& name) const {
    Q_UNUSED(name);
    return Layout::LayoutCalculator::UnionLayout();
}

size_t StructureManager::getStructureSize(const std::string& structName) const {
    Q_UNUSED(structName);
    return 0;
}

size_t StructureManager::getFieldOffset(const std::string& structName, const std::string& fieldPath) const {
    Q_UNUSED(structName);
    Q_UNUSED(fieldPath);
    return 0;
}

size_t StructureManager::getFieldSize(const std::string& structName, const std::string& fieldPath) const {
    Q_UNUSED(structName);
    Q_UNUSED(fieldPath);
    return 0;
}

bool StructureManager::validateFieldPath(const std::string& structName, const std::string& fieldPath) const {
    Q_UNUSED(structName);
    Q_UNUSED(fieldPath);
    return false;
}

std::vector<std::string> StructureManager::getDependencies(const std::string& structName) const {
    Q_UNUSED(structName);
    return {};
}

std::vector<std::string> StructureManager::getDependents(const std::string& structName) const {
    Q_UNUSED(structName);
    return {};
}

bool StructureManager::validateDependencies() const {
    return true;
}

bool StructureManager::hasCyclicDependencies() const {
    return false;
}

std::vector<std::string> StructureManager::getTopologicalOrder() const {
    return {};
}

nlohmann::json StructureManager::exportToJSON() const {
    return nlohmann::json();
}

nlohmann::json StructureManager::exportStructureToJSON(const std::string& structName) const {
    Q_UNUSED(structName);
    return nlohmann::json();
}

bool StructureManager::importFromJSON(const nlohmann::json& json) {
    Q_UNUSED(json);
    return false;
}

bool StructureManager::saveToFile(const QString& filePath) const {
    Q_UNUSED(filePath);
    return false;
}

bool StructureManager::loadFromFile(const QString& filePath) {
    Q_UNUSED(filePath);
    return false;
}

bool StructureManager::saveStructureToFile(const std::string& structName, const QString& filePath) const {
    Q_UNUSED(structName);
    Q_UNUSED(filePath);
    return false;
}

void StructureManager::setCompilerType(Layout::AlignmentRules::CompilerType compiler) {
    m_compilerType = compiler;
}

Layout::AlignmentRules::CompilerType StructureManager::getCompilerType() const {
    return m_compilerType;
}

void StructureManager::setArchitecture(Layout::AlignmentRules::Architecture arch) {
    m_architecture = arch;
}

Layout::AlignmentRules::Architecture StructureManager::getArchitecture() const {
    return m_architecture;
}

void StructureManager::setParserOptions(const StructParser::ParserOptions& options) {
    Q_UNUSED(options);
}

StructParser::ParserOptions StructureManager::getParserOptions() const {
    return StructParser::ParserOptions();
}

void StructureManager::setCacheSize(size_t maxSize) {
    Q_UNUSED(maxSize);
}

size_t StructureManager::getCacheSize() const {
    return 0;
}

void StructureManager::enableCaching(bool enabled) {
    Q_UNUSED(enabled);
}

bool StructureManager::isCachingEnabled() const {
    return false;
}

void StructureManager::invalidateStructure(const std::string& structName) {
    Q_UNUSED(structName);
}

void StructureManager::invalidateAll() {
}

void StructureManager::compactCache() {
}

StructureManager::Statistics StructureManager::getStatistics() const {
    return Statistics();
}

void StructureManager::resetStatistics() {
}

std::string StructureManager::generateReport() const {
    return "StructureManager stub implementation - no data available";
}

void StructureManager::printDiagnostics() const {
    qDebug() << "StructureManager: Stub implementation";
}

void StructureManager::clearErrors() {
    m_errors.clear();
}

// Qt slots
void StructureManager::onMemoryPressure() {
    qDebug() << "StructureManager: Memory pressure detected";
}

void StructureManager::onPerformanceAlert() {
    qDebug() << "StructureManager: Performance alert";
}

void StructureManager::onCacheEviction(const QString& structureName) {
    qDebug() << "StructureManager: Cache eviction for" << structureName;
}

std::string StructureManager::ParseResult::getSummary() const {
    return "ParseResult stub";
}

std::string StructureManager::StructureError::toString() const {
    return message + " (struct: " + structureName + ", field: " + fieldName + ")";
}

} // namespace Parser
} // namespace Monitor