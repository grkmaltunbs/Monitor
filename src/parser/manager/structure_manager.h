#pragma once

#include "../ast/ast_nodes.h"
#include "../parser/struct_parser.h"
#include "../layout/layout_calculator.h"
#include "../serialization/json_serializer.h"
#include "../serialization/json_deserializer.h"
#include "structure_cache.h"
#include "../../events/event_dispatcher.h"
#include "../../logging/logger.h"
#include "../../memory/memory_pool.h"
#include "../../profiling/profiler.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <shared_mutex>

namespace Monitor {
namespace Parser {

/**
 * @brief Main interface for C++ structure parsing and management
 * 
 * This class provides the high-level API for parsing C++ structures,
 * managing their layouts, and integrating with the Monitor Application's
 * core systems. It coordinates all parser components and provides
 * caching, persistence, and dependency management.
 */
class StructureManager : public QObject {
    Q_OBJECT
    
public:
    struct ParseResult {
        bool success = false;
        std::vector<std::string> errors;
        std::vector<std::string> warnings;
        size_t structuresParsed = 0;
        size_t unionsParsed = 0;
        size_t typedefsParsed = 0;
        std::chrono::milliseconds parseTime{0};
        std::chrono::milliseconds layoutTime{0};
        std::chrono::milliseconds totalTime{0};
        
        ParseResult() = default;
        
        bool hasErrors() const { return !errors.empty(); }
        bool hasWarnings() const { return !warnings.empty(); }
        
        size_t getTotalItems() const {
            return structuresParsed + unionsParsed + typedefsParsed;
        }
        
        std::string getSummary() const;
    };
    
    struct StructureInfo {
        std::string name;
        size_t totalSize = 0;
        size_t alignment = 0;
        size_t fieldCount = 0;
        size_t bitfieldCount = 0;
        bool isPacked = false;
        std::vector<std::string> dependencies;
        std::chrono::steady_clock::time_point lastModified;
        
        StructureInfo() = default;
        StructureInfo(const std::string& n) : name(n) {}
    };
    
    explicit StructureManager(QObject* parent = nullptr);
    ~StructureManager() override;
    
    // Main parsing interface
    ParseResult parseStructures(const std::string& sourceCode);
    ParseResult parseStructuresFromFile(const QString& filePath);
    ParseResult parseStructuresFromFiles(const QStringList& filePaths);
    
    // Structure queries
    bool hasStructure(const std::string& name) const;
    std::shared_ptr<const AST::StructDeclaration> getStructure(const std::string& name) const;
    std::vector<std::string> getStructureNames() const;
    std::vector<StructureInfo> getStructureInfos() const;
    
    // Union queries
    bool hasUnion(const std::string& name) const;
    std::shared_ptr<const AST::UnionDeclaration> getUnion(const std::string& name) const;
    std::vector<std::string> getUnionNames() const;
    
    // Typedef queries  
    bool hasTypedef(const std::string& name) const;
    std::shared_ptr<const AST::TypedefDeclaration> getTypedef(const std::string& name) const;
    std::vector<std::string> getTypedefNames() const;
    
    // Layout information
    bool hasLayout(const std::string& structName) const;
    Layout::LayoutCalculator::StructLayout getStructLayout(const std::string& name) const;
    Layout::LayoutCalculator::UnionLayout getUnionLayout(const std::string& name) const;
    
    // Field access helpers
    size_t getStructureSize(const std::string& structName) const;
    size_t getFieldOffset(const std::string& structName, const std::string& fieldPath) const;
    size_t getFieldSize(const std::string& structName, const std::string& fieldPath) const;
    bool validateFieldPath(const std::string& structName, const std::string& fieldPath) const;
    
    // Dependency management
    std::vector<std::string> getDependencies(const std::string& structName) const;
    std::vector<std::string> getDependents(const std::string& structName) const;
    bool validateDependencies() const;
    bool hasCyclicDependencies() const;
    std::vector<std::string> getTopologicalOrder() const;
    
    // Serialization and persistence
    nlohmann::json exportToJSON() const;
    nlohmann::json exportStructureToJSON(const std::string& structName) const;
    bool importFromJSON(const nlohmann::json& json);
    
    bool saveToFile(const QString& filePath) const;
    bool loadFromFile(const QString& filePath);
    bool saveStructureToFile(const std::string& structName, const QString& filePath) const;
    
    // Configuration
    void setCompilerType(Layout::AlignmentRules::CompilerType compiler);
    Layout::AlignmentRules::CompilerType getCompilerType() const;
    
    void setArchitecture(Layout::AlignmentRules::Architecture arch);
    Layout::AlignmentRules::Architecture getArchitecture() const;
    
    void setParserOptions(const StructParser::ParserOptions& options);
    StructParser::ParserOptions getParserOptions() const;
    
    void setCacheSize(size_t maxSize);
    size_t getCacheSize() const;
    
    void enableCaching(bool enabled);
    bool isCachingEnabled() const;
    
    // Cache management
    void invalidateStructure(const std::string& structName);
    void invalidateAll();
    void compactCache();
    
    // Statistics and diagnostics
    struct Statistics {
        size_t totalStructures = 0;
        size_t totalUnions = 0;
        size_t totalTypedefs = 0;
        size_t totalFields = 0;
        size_t totalBitfields = 0;
        size_t cacheHitCount = 0;
        size_t cacheMissCount = 0;
        std::chrono::milliseconds averageParseTime{0};
        std::chrono::milliseconds totalParseTime{0};
        size_t memoryUsage = 0;
        
        double getCacheHitRatio() const {
            size_t total = cacheHitCount + cacheMissCount;
            return total > 0 ? static_cast<double>(cacheHitCount) / total : 0.0;
        }
        
        size_t getTotalDeclarations() const {
            return totalStructures + totalUnions + totalTypedefs;
        }
        
        size_t getTotalElements() const {
            return totalFields + totalBitfields;
        }
    };
    
    Statistics getStatistics() const;
    void resetStatistics();
    
    // Performance and diagnostics
    std::string generateReport() const;
    void printDiagnostics() const;
    
    // Error handling
    struct StructureError {
        std::string message;
        std::string structureName;
        std::string fieldName;
        size_t line = 0;
        size_t column = 0;
        
        StructureError(const std::string& msg, const std::string& structure = "",
                      const std::string& field = "", size_t l = 0, size_t c = 0)
            : message(msg), structureName(structure), fieldName(field), line(l), column(c) {}
        
        std::string toString() const;
    };
    
    const std::vector<StructureError>& getErrors() const { return m_errors; }
    bool hasErrors() const { return !m_errors.empty(); }
    void clearErrors();
    
    // Integration with Phase 1 components
    void setEventDispatcher(Events::EventDispatcher* dispatcher) { m_eventDispatcher = dispatcher; }
    void setMemoryManager(Memory::MemoryPoolManager* manager) { m_memoryManager = manager; }
    void setLogger(Logging::Logger* logger) { m_logger = logger; }
    void setProfiler(Profiling::Profiler* profiler) { m_profiler = profiler; }

signals:
    void structureParsed(const QString& name);
    void structureInvalidated(const QString& name);
    void parseCompleted(const ParseResult& result);
    void errorOccurred(const QString& error);
    void warningIssued(const QString& warning);
    void dependencyChanged(const QString& structureName);
    void cacheUpdated();

public slots:
    void onMemoryPressure();
    void onPerformanceAlert();
    
private slots:
    void onCacheEviction(const QString& structureName);
    
private:
    // Core components
    std::unique_ptr<StructParser> m_parser;
    std::unique_ptr<Layout::LayoutCalculator> m_layoutCalculator;
    std::unique_ptr<StructureCache> m_cache;
    std::unique_ptr<Serialization::JSONSerializer> m_serializer;
    std::unique_ptr<Serialization::JSONDeserializer> m_deserializer;
    
    // Data storage
    std::unordered_map<std::string, std::shared_ptr<AST::StructDeclaration>> m_structures;
    std::unordered_map<std::string, std::shared_ptr<AST::UnionDeclaration>> m_unions;
    std::unordered_map<std::string, std::shared_ptr<AST::TypedefDeclaration>> m_typedefs;
    std::unordered_map<std::string, Layout::LayoutCalculator::StructLayout> m_structLayouts;
    std::unordered_map<std::string, Layout::LayoutCalculator::UnionLayout> m_unionLayouts;
    
    // Dependency graph
    std::unordered_map<std::string, std::unordered_set<std::string>> m_dependencies;
    std::unordered_map<std::string, std::unordered_set<std::string>> m_dependents;
    
    // Configuration
    Layout::AlignmentRules::CompilerType m_compilerType;
    Layout::AlignmentRules::Architecture m_architecture;
    std::shared_ptr<Layout::AlignmentRules> m_alignmentRules;
    
    // Statistics and errors
    mutable Statistics m_statistics;
    std::vector<StructureError> m_errors;
    
    // Thread safety
    mutable std::shared_mutex m_structuresMutex;
    mutable std::shared_mutex m_layoutsMutex;
    mutable std::shared_mutex m_dependenciesMutex;
    
    // Integration with Phase 1 components
    Events::EventDispatcher* m_eventDispatcher = nullptr;
    Memory::MemoryPoolManager* m_memoryManager = nullptr;
    Logging::Logger* m_logger = nullptr;
    Profiling::Profiler* m_profiler = nullptr;
    
    // Initialization and cleanup
    void initializeComponents();
    void cleanupComponents();
    void validateConfiguration();
    
    // Parsing helpers
    ParseResult parseInternal(const std::string& source, const std::string& context);
    void processParseResult(const StructParser::ParseResult& parserResult, ParseResult& result);
    void calculateLayouts(ParseResult& result);
    void updateDependencyGraph();
    void updateCache();
    
    // Layout calculation helpers
    bool calculateStructureLayout(const std::string& structName);
    bool calculateUnionLayout(const std::string& unionName);
    void invalidateLayoutCache(const std::string& structName);
    
    // Dependency management helpers
    void buildDependencyGraph();
    bool validateDependencyGraph() const;
    void updateStructureDependencies(const std::string& structName);
    std::vector<std::string> getTopologicalOrderInternal() const;
    bool hasCyclicDependencyInternal(const std::string& start, 
                                   std::unordered_set<std::string>& visited,
                                   std::unordered_set<std::string>& recursionStack) const;
    
    // Event emission helpers
    void emitStructureParsed(const std::string& structName);
    void emitParseCompleted(const ParseResult& result);
    void emitError(const std::string& error);
    void emitWarning(const std::string& warning);
    
    // Error handling helpers
    void addError(const std::string& message, const std::string& structureName = "",
                  const std::string& fieldName = "", size_t line = 0, size_t column = 0);
    void processParserErrors(const std::vector<StructParser::ParseError>& parserErrors);
    void processLayoutErrors(const std::vector<Layout::LayoutCalculator::LayoutError>& layoutErrors);
    
    // Statistics helpers
    void updateStatistics(const ParseResult& result);
    void updateCacheStatistics();
    
    // Validation helpers
    bool validateStructureName(const std::string& name) const;
    bool validateFieldPath(const std::string& fieldPath) const;
    bool isValidIdentifier(const std::string& identifier) const;
    
    // Memory management
    void optimizeMemoryUsage();
    void handleMemoryPressure();
    
    // Profiling integration
    void profileOperation(const std::string& operationName, 
                         std::function<void()> operation);
    
    // Logging integration
    void logInfo(const std::string& message) const;
    void logWarning(const std::string& message) const;
    void logError(const std::string& message) const;
    void logDebug(const std::string& message) const;
};

} // namespace Parser
} // namespace Monitor