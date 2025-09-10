#include <QtTest/QtTest>
#include <QSignalSpy>
#include <memory>

#include "../../../src/parser/manager/structure_manager.h"
#include "../../../src/parser/ast/ast_nodes.h"

class TestStructureManager : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    // Basic management tests
    void testAddStructure();
    void testRemoveStructure();
    void testFindStructure();
    void testListStructures();
    void testClearStructures();

    // Dependency management tests
    void testDependencyResolution();
    void testCircularDependencyDetection();
    void testDependencyOrdering();
    void testMissingDependencies();
    void testDependencyUpdates();

    // Structure validation tests
    void testStructureValidation();
    void testNameConflicts();
    void testInvalidStructures();
    void testStructureIntegrity();

    // Cache management tests
    void testCacheOperations();
    void testCacheInvalidation();
    void testCachePerformance();
    void testCacheConcurrency();

    // Qt integration tests
    void testSignalEmission();
    void testQObjectIntegration();
    void testThreadSafety();

    // JSON serialization tests
    void testJSONSerialization();
    void testJSONDeserialization();
    void testSerializationRoundTrip();
    void testSerializationErrors();

    // Workspace management tests
    void testWorkspaceOperations();
    void testWorkspacePersistence();
    void testWorkspaceVersioning();
    void testWorkspaceMigration();

    // Performance tests
    void testLargeNumberOfStructures();
    void testComplexDependencyGraphs();
    void testMemoryUsage();
    void testLookupPerformance();

    // Error handling tests
    void testErrorRecovery();
    void testCorruptedData();
    void testResourceExhaustion();

    // Integration tests
    void testParserIntegration();
    void testLayoutCalculatorIntegration();
    void testRealWorldScenarios();

private:
    std::unique_ptr<StructureManager> manager;
    
    // Helper methods
    std::unique_ptr<StructNode> createTestStruct(const QString& name, const QStringList& fieldTypes = {});
    std::unique_ptr<StructNode> createComplexStruct();
    void verifyDependencyOrder(const QList<QString>& order, const QStringList& expectedBefore);
    void createDependencyChain(int depth);
    void setupCircularDependency();
    QJsonObject createTestJSON();
};

void TestStructureManager::initTestCase()
{
    manager = std::make_unique<StructureManager>();
}

void TestStructureManager::cleanupTestCase()
{
    manager.reset();
}

void TestStructureManager::init()
{
    // Clear any existing structures before each test
    manager->clear();
}

void TestStructureManager::cleanup()
{
    // Cleanup after each test
    manager->clear();
}

void TestStructureManager::testAddStructure()
{
    auto testStruct = createTestStruct("TestStruct", {"int", "double"});
    auto structPtr = testStruct.get();
    
    QSignalSpy addedSpy(manager.get(), &StructureManager::structureAdded);
    
    bool result = manager->addStructure(std::move(testStruct));
    
    QVERIFY(result);
    QCOMPARE(addedSpy.count(), 1);
    QCOMPARE(addedSpy.first().first().toString(), QString("TestStruct"));
    
    // Verify structure was added
    auto retrieved = manager->findStructure("TestStruct");
    QVERIFY(retrieved != nullptr);
    QCOMPARE(retrieved, structPtr);
    
    QCOMPARE(manager->getStructureCount(), 1);
    QCOMPARE(manager->listStructures().size(), 1);
}

void TestStructureManager::testRemoveStructure()
{
    // Add a structure first
    auto testStruct = createTestStruct("TestStruct");
    manager->addStructure(std::move(testStruct));
    
    QSignalSpy removedSpy(manager.get(), &StructureManager::structureRemoved);
    
    bool result = manager->removeStructure("TestStruct");
    
    QVERIFY(result);
    QCOMPARE(removedSpy.count(), 1);
    QCOMPARE(removedSpy.first().first().toString(), QString("TestStruct"));
    
    // Verify structure was removed
    auto retrieved = manager->findStructure("TestStruct");
    QVERIFY(retrieved == nullptr);
    
    QCOMPARE(manager->getStructureCount(), 0);
    
    // Test removing non-existent structure
    bool removeResult = manager->removeStructure("NonExistent");
    QVERIFY(!removeResult);
}

void TestStructureManager::testFindStructure()
{
    // Add multiple structures
    manager->addStructure(createTestStruct("Struct1"));
    manager->addStructure(createTestStruct("Struct2"));
    manager->addStructure(createTestStruct("Struct3"));
    
    // Test finding existing structures
    auto found1 = manager->findStructure("Struct1");
    QVERIFY(found1 != nullptr);
    QCOMPARE(found1->getName(), QString("Struct1"));
    
    auto found2 = manager->findStructure("Struct2");
    QVERIFY(found2 != nullptr);
    QCOMPARE(found2->getName(), QString("Struct2"));
    
    // Test finding non-existent structure
    auto notFound = manager->findStructure("NonExistent");
    QVERIFY(notFound == nullptr);
    
    // Test case sensitivity
    auto caseSensitive = manager->findStructure("struct1"); // lowercase
    QVERIFY(caseSensitive == nullptr); // Should not find uppercase version
}

void TestStructureManager::testListStructures()
{
    QCOMPARE(manager->listStructures().size(), 0);
    
    // Add structures
    manager->addStructure(createTestStruct("Alpha"));
    manager->addStructure(createTestStruct("Beta"));
    manager->addStructure(createTestStruct("Gamma"));
    
    auto structures = manager->listStructures();
    QCOMPARE(structures.size(), 3);
    
    // Verify all structures are in the list
    QStringList names;
    for (const auto& structure : structures) {
        names.append(structure->getName());
    }
    
    QVERIFY(names.contains("Alpha"));
    QVERIFY(names.contains("Beta"));
    QVERIFY(names.contains("Gamma"));
    
    // Test sorted listing (if supported)
    auto sortedStructures = manager->listStructures(true);
    QCOMPARE(sortedStructures.size(), 3);
    
    if (sortedStructures.size() == 3) {
        QCOMPARE(sortedStructures[0]->getName(), QString("Alpha"));
        QCOMPARE(sortedStructures[1]->getName(), QString("Beta"));
        QCOMPARE(sortedStructures[2]->getName(), QString("Gamma"));
    }
}

void TestStructureManager::testClearStructures()
{
    // Add structures
    manager->addStructure(createTestStruct("Struct1"));
    manager->addStructure(createTestStruct("Struct2"));
    manager->addStructure(createTestStruct("Struct3"));
    
    QCOMPARE(manager->getStructureCount(), 3);
    
    QSignalSpy clearedSpy(manager.get(), &StructureManager::structuresCleared);
    
    manager->clear();
    
    QCOMPARE(manager->getStructureCount(), 0);
    QCOMPARE(manager->listStructures().size(), 0);
    QCOMPARE(clearedSpy.count(), 1);
    
    // Verify all structures are gone
    QVERIFY(manager->findStructure("Struct1") == nullptr);
    QVERIFY(manager->findStructure("Struct2") == nullptr);
    QVERIFY(manager->findStructure("Struct3") == nullptr);
}

void TestStructureManager::testDependencyResolution()
{
    // Create structures with dependencies
    // BaseStruct (no dependencies)
    auto baseStruct = createTestStruct("BaseStruct", {"int", "double"});
    
    // DerivedStruct depends on BaseStruct
    auto derivedStruct = createTestStruct("DerivedStruct", {"BaseStruct", "float"});
    derivedStruct->addDependency("BaseStruct");
    
    // ComplexStruct depends on both
    auto complexStruct = createTestStruct("ComplexStruct", {"BaseStruct", "DerivedStruct", "char"});
    complexStruct->addDependency("BaseStruct");
    complexStruct->addDependency("DerivedStruct");
    
    // Add in reverse order to test resolution
    manager->addStructure(std::move(complexStruct));
    manager->addStructure(std::move(derivedStruct));
    manager->addStructure(std::move(baseStruct));
    
    // Test dependency resolution
    auto dependencies = manager->resolveDependencies("ComplexStruct");
    
    QVERIFY(dependencies.contains("BaseStruct"));
    QVERIFY(dependencies.contains("DerivedStruct"));
    QCOMPARE(dependencies.size(), 2);
    
    // Test dependency ordering
    auto order = manager->getDependencyOrder("ComplexStruct");
    
    // BaseStruct should come before DerivedStruct
    int baseIndex = order.indexOf("BaseStruct");
    int derivedIndex = order.indexOf("DerivedStruct");
    int complexIndex = order.indexOf("ComplexStruct");
    
    QVERIFY(baseIndex >= 0);
    QVERIFY(derivedIndex >= 0);
    QVERIFY(complexIndex >= 0);
    QVERIFY(baseIndex < derivedIndex);
    QVERIFY(derivedIndex < complexIndex);
}

void TestStructureManager::testCircularDependencyDetection()
{
    // Create circular dependency: A -> B -> C -> A
    auto structA = createTestStruct("StructA", {"StructB*"});
    structA->addDependency("StructB");
    
    auto structB = createTestStruct("StructB", {"StructC*"});
    structB->addDependency("StructC");
    
    auto structC = createTestStruct("StructC", {"StructA*"});
    structC->addDependency("StructA");
    
    manager->addStructure(std::move(structA));
    manager->addStructure(std::move(structB));
    manager->addStructure(std::move(structC));
    
    // Test circular dependency detection
    bool hasCircular = manager->hasCircularDependencies();
    QVERIFY(hasCircular);
    
    auto circularStructs = manager->findCircularDependencies();
    QVERIFY(circularStructs.contains("StructA"));
    QVERIFY(circularStructs.contains("StructB"));
    QVERIFY(circularStructs.contains("StructC"));
    
    // Test specific structure circular check
    QVERIFY(manager->hasCircularDependency("StructA"));
    QVERIFY(manager->hasCircularDependency("StructB"));
    QVERIFY(manager->hasCircularDependency("StructC"));
}

void TestStructureManager::testDependencyOrdering()
{
    // Create a complex dependency graph
    manager->addStructure(createTestStruct("Level0")); // No dependencies
    
    auto level1 = createTestStruct("Level1", {"Level0"});
    level1->addDependency("Level0");
    manager->addStructure(std::move(level1));
    
    auto level2a = createTestStruct("Level2A", {"Level1"});
    level2a->addDependency("Level1");
    manager->addStructure(std::move(level2a));
    
    auto level2b = createTestStruct("Level2B", {"Level1"});
    level2b->addDependency("Level1");
    manager->addStructure(std::move(level2b));
    
    auto level3 = createTestStruct("Level3", {"Level2A", "Level2B"});
    level3->addDependency("Level2A");
    level3->addDependency("Level2B");
    manager->addStructure(std::move(level3));
    
    // Test complete topological ordering
    auto completeOrder = manager->getTopologicalOrder();
    
    QVERIFY(completeOrder.contains("Level0"));
    QVERIFY(completeOrder.contains("Level1"));
    QVERIFY(completeOrder.contains("Level2A"));
    QVERIFY(completeOrder.contains("Level2B"));
    QVERIFY(completeOrder.contains("Level3"));
    
    // Verify ordering constraints
    verifyDependencyOrder(completeOrder, {"Level0", "Level1", "Level2A", "Level3"});
    verifyDependencyOrder(completeOrder, {"Level0", "Level1", "Level2B", "Level3"});
    
    // Level2A and Level2B can be in any order relative to each other
    int level2AIndex = completeOrder.indexOf("Level2A");
    int level2BIndex = completeOrder.indexOf("Level2B");
    int level3Index = completeOrder.indexOf("Level3");
    
    QVERIFY(level2AIndex < level3Index);
    QVERIFY(level2BIndex < level3Index);
}

void TestStructureManager::testMissingDependencies()
{
    // Create structure with missing dependency
    auto structWithMissingDep = createTestStruct("Dependent", {"MissingStruct"});
    structWithMissingDep->addDependency("MissingStruct");
    
    manager->addStructure(std::move(structWithMissingDep));
    
    // Test missing dependency detection
    auto missingDeps = manager->findMissingDependencies();
    QVERIFY(missingDeps.contains("MissingStruct"));
    
    auto dependentMissing = manager->getMissingDependencies("Dependent");
    QVERIFY(dependentMissing.contains("MissingStruct"));
    
    // Test validation with missing dependencies
    bool isValid = manager->validateDependencies();
    QVERIFY(!isValid);
    
    // Add the missing dependency
    manager->addStructure(createTestStruct("MissingStruct"));
    
    // Should now be valid
    isValid = manager->validateDependencies();
    QVERIFY(isValid);
    
    auto missingAfter = manager->findMissingDependencies();
    QVERIFY(missingAfter.isEmpty());
}

void TestStructureManager::testDependencyUpdates()
{
    // Add initial structures
    manager->addStructure(createTestStruct("BaseStruct"));
    
    auto derivedStruct = createTestStruct("DerivedStruct");
    derivedStruct->addDependency("BaseStruct");
    manager->addStructure(std::move(derivedStruct));
    
    QSignalSpy dependencyChangedSpy(manager.get(), &StructureManager::dependenciesChanged);
    
    // Update dependencies
    auto updatedStruct = manager->findStructure("DerivedStruct");
    updatedStruct->addDependency("NewDependency");
    
    manager->updateDependencies("DerivedStruct");
    
    QCOMPARE(dependencyChangedSpy.count(), 1);
    
    // Verify updated dependencies
    auto deps = manager->resolveDependencies("DerivedStruct");
    QVERIFY(deps.contains("BaseStruct"));
    QVERIFY(deps.contains("NewDependency")); // Should be in missing dependencies
    
    auto missing = manager->getMissingDependencies("DerivedStruct");
    QVERIFY(missing.contains("NewDependency"));
}

void TestStructureManager::testStructureValidation()
{
    // Test valid structure
    auto validStruct = createTestStruct("ValidStruct", {"int", "double"});
    QVERIFY(manager->validateStructure(validStruct.get()));
    
    manager->addStructure(std::move(validStruct));
    
    // Test structure with missing dependency
    auto invalidStruct = createTestStruct("InvalidStruct", {"MissingType"});
    invalidStruct->addDependency("MissingType");
    
    // Validation should fail due to missing dependency
    QVERIFY(!manager->validateStructure(invalidStruct.get()));
    
    // Test overall validation
    QVERIFY(manager->validateAllStructures());
    
    manager->addStructure(std::move(invalidStruct));
    
    QVERIFY(!manager->validateAllStructures());
}

void TestStructureManager::testNameConflicts()
{
    // Add first structure
    auto struct1 = createTestStruct("ConflictName");
    QVERIFY(manager->addStructure(std::move(struct1)));
    
    // Try to add structure with same name
    auto struct2 = createTestStruct("ConflictName");
    
    QSignalSpy errorSpy(manager.get(), &StructureManager::errorOccurred);
    
    QVERIFY(!manager->addStructure(std::move(struct2)));
    QCOMPARE(errorSpy.count(), 1);
    
    // Verify only one structure exists
    QCOMPARE(manager->getStructureCount(), 1);
    
    // Test conflict detection
    QVERIFY(manager->hasNameConflict("ConflictName"));
    QVERIFY(!manager->hasNameConflict("UniqueNasme"));
}

void TestStructureManager::testInvalidStructures()
{
    // Test null structure
    QVERIFY(!manager->addStructure(nullptr));
    
    // Test structure with empty name
    auto emptyNameStruct = std::make_unique<StructNode>("");
    QVERIFY(!manager->addStructure(std::move(emptyNameStruct)));
    
    // Test structure with invalid characters in name
    auto invalidNameStruct = std::make_unique<StructNode>("Invalid Name!");
    QVERIFY(!manager->validateStructure(invalidNameStruct.get()));
}

void TestStructureManager::testStructureIntegrity()
{
    // Add structures and test integrity
    manager->addStructure(createTestStruct("Struct1"));
    manager->addStructure(createTestStruct("Struct2"));
    
    QVERIFY(manager->checkIntegrity());
    
    // Manually corrupt internal state (if possible)
    // This would depend on the internal implementation
    
    // Test recovery from corruption
    bool recovered = manager->repairIntegrity();
    QVERIFY(recovered);
    QVERIFY(manager->checkIntegrity());
}

void TestStructureManager::testCacheOperations()
{
    // Test cache miss and population
    QVERIFY(manager->findStructure("CachedStruct") == nullptr);
    
    auto cachedStruct = createTestStruct("CachedStruct");
    auto structPtr = cachedStruct.get();
    manager->addStructure(std::move(cachedStruct));
    
    // First access should populate cache
    auto retrieved1 = manager->findStructure("CachedStruct");
    QCOMPARE(retrieved1, structPtr);
    
    // Second access should hit cache
    auto retrieved2 = manager->findStructure("CachedStruct");
    QCOMPARE(retrieved2, structPtr);
    QCOMPARE(retrieved1, retrieved2);
    
    // Test cache statistics (if available)
    auto cacheStats = manager->getCacheStatistics();
    QVERIFY(cacheStats.hits >= 1);
    QVERIFY(cacheStats.misses >= 1);
}

void TestStructureManager::testCacheInvalidation()
{
    auto testStruct = createTestStruct("CacheTestStruct");
    manager->addStructure(std::move(testStruct));
    
    // Access to populate cache
    auto retrieved1 = manager->findStructure("CacheTestStruct");
    QVERIFY(retrieved1 != nullptr);
    
    QSignalSpy cacheInvalidatedSpy(manager.get(), &StructureManager::cacheInvalidated);
    
    // Modify structure to invalidate cache
    manager->updateStructure("CacheTestStruct");
    
    QCOMPARE(cacheInvalidatedSpy.count(), 1);
    
    // Test manual cache clearing
    manager->clearCache();
    
    // Should still be able to find the structure
    auto retrieved2 = manager->findStructure("CacheTestStruct");
    QVERIFY(retrieved2 != nullptr);
    QCOMPARE(retrieved1, retrieved2);
}

void TestStructureManager::testCachePerformance()
{
    const int numStructs = 1000;
    
    // Add many structures
    for (int i = 0; i < numStructs; ++i) {
        manager->addStructure(createTestStruct(QString("Struct%1").arg(i)));
    }
    
    QElapsedTimer timer;
    
    // Time cold lookups
    timer.start();
    for (int i = 0; i < 100; ++i) {
        manager->findStructure(QString("Struct%1").arg(i));
    }
    qint64 coldTime = timer.elapsed();
    
    // Time warm lookups (cached)
    timer.start();
    for (int i = 0; i < 100; ++i) {
        manager->findStructure(QString("Struct%1").arg(i));
    }
    qint64 warmTime = timer.elapsed();
    
    // Cached lookups should be faster
    QVERIFY(warmTime <= coldTime);
    
    qDebug() << "Cold lookup time:" << coldTime << "ms";
    qDebug() << "Warm lookup time:" << warmTime << "ms";
}

void TestStructureManager::testCacheConcurrency()
{
    // Add test structure
    manager->addStructure(createTestStruct("ConcurrentStruct"));
    
    QSignalSpy errorSpy(manager.get(), &StructureManager::errorOccurred);
    
    // Simulate concurrent access
    QFuture<void> future1 = QtConcurrent::run([this]() {
        for (int i = 0; i < 100; ++i) {
            manager->findStructure("ConcurrentStruct");
        }
    });
    
    QFuture<void> future2 = QtConcurrent::run([this]() {
        for (int i = 0; i < 100; ++i) {
            manager->findStructure("ConcurrentStruct");
        }
    });
    
    future1.waitForFinished();
    future2.waitForFinished();
    
    // No errors should occur from concurrent access
    QCOMPARE(errorSpy.count(), 0);
    
    // Structure should still be findable
    QVERIFY(manager->findStructure("ConcurrentStruct") != nullptr);
}

void TestStructureManager::testSignalEmission()
{
    QSignalSpy addedSpy(manager.get(), &StructureManager::structureAdded);
    QSignalSpy removedSpy(manager.get(), &StructureManager::structureRemoved);
    QSignalSpy updatedSpy(manager.get(), &StructureManager::structureUpdated);
    QSignalSpy clearedSpy(manager.get(), &StructureManager::structuresCleared);
    
    // Test structure added signal
    manager->addStructure(createTestStruct("SignalStruct"));
    QCOMPARE(addedSpy.count(), 1);
    QCOMPARE(addedSpy.first().first().toString(), QString("SignalStruct"));
    
    // Test structure updated signal
    manager->updateStructure("SignalStruct");
    QCOMPARE(updatedSpy.count(), 1);
    QCOMPARE(updatedSpy.first().first().toString(), QString("SignalStruct"));
    
    // Test structure removed signal
    manager->removeStructure("SignalStruct");
    QCOMPARE(removedSpy.count(), 1);
    QCOMPARE(removedSpy.first().first().toString(), QString("SignalStruct"));
    
    // Test structures cleared signal
    manager->addStructure(createTestStruct("TempStruct"));
    manager->clear();
    QCOMPARE(clearedSpy.count(), 1);
}

void TestStructureManager::testQObjectIntegration()
{
    // Test that manager is a proper QObject
    QVERIFY(qobject_cast<QObject*>(manager.get()) != nullptr);
    
    // Test object name
    manager->setObjectName("TestStructureManager");
    QCOMPARE(manager->objectName(), QString("TestStructureManager"));
    
    // Test property system (if properties are defined)
    int structCount = manager->property("structureCount").toInt();
    QCOMPARE(structCount, manager->getStructureCount());
    
    // Test parent-child relationship
    auto childManager = std::make_unique<StructureManager>(manager.get());
    QCOMPARE(childManager->parent(), manager.get());
}

void TestStructureManager::testThreadSafety()
{
    const int numThreads = 4;
    const int operationsPerThread = 100;
    
    QSignalSpy errorSpy(manager.get(), &StructureManager::errorOccurred);
    
    // Run concurrent operations
    QList<QFuture<void>> futures;
    
    for (int t = 0; t < numThreads; ++t) {
        futures.append(QtConcurrent::run([this, t, operationsPerThread]() {
            for (int i = 0; i < operationsPerThread; ++i) {
                QString structName = QString("Thread%1_Struct%2").arg(t).arg(i);
                
                // Add structure
                manager->addStructure(createTestStruct(structName));
                
                // Find structure
                auto found = manager->findStructure(structName);
                Q_UNUSED(found);
                
                // Remove structure
                manager->removeStructure(structName);
            }
        }));
    }
    
    // Wait for all threads to complete
    for (auto& future : futures) {
        future.waitForFinished();
    }
    
    // Check for errors
    QCOMPARE(errorSpy.count(), 0);
    
    // Manager should be in consistent state
    QVERIFY(manager->checkIntegrity());
}

void TestStructureManager::testJSONSerialization()
{
    // Add test structures
    manager->addStructure(createTestStruct("SimpleStruct", {"int", "double"}));
    manager->addStructure(createComplexStruct());
    
    // Serialize to JSON
    QJsonObject json = manager->toJson();
    
    QVERIFY(!json.isEmpty());
    QVERIFY(json.contains("structures"));
    QVERIFY(json.contains("version"));
    QVERIFY(json.contains("dependencies"));
    
    auto structures = json["structures"].toObject();
    QVERIFY(structures.contains("SimpleStruct"));
    
    // Verify structure data
    auto simpleStruct = structures["SimpleStruct"].toObject();
    QVERIFY(simpleStruct.contains("name"));
    QVERIFY(simpleStruct.contains("fields"));
    QCOMPARE(simpleStruct["name"].toString(), QString("SimpleStruct"));
}

void TestStructureManager::testJSONDeserialization()
{
    // Create test JSON
    QJsonObject testJson = createTestJSON();
    
    // Clear manager and load from JSON
    manager->clear();
    
    QSignalSpy loadedSpy(manager.get(), &StructureManager::structuresLoaded);
    
    bool result = manager->fromJson(testJson);
    QVERIFY(result);
    QCOMPARE(loadedSpy.count(), 1);
    
    // Verify loaded structures
    QVERIFY(manager->findStructure("TestStruct1") != nullptr);
    QVERIFY(manager->findStructure("TestStruct2") != nullptr);
    
    auto struct1 = manager->findStructure("TestStruct1");
    QCOMPARE(struct1->getFields().size(), 2);
}

void TestStructureManager::testSerializationRoundTrip()
{
    // Add test structures
    manager->addStructure(createTestStruct("RoundTrip1", {"int", "char"}));
    manager->addStructure(createTestStruct("RoundTrip2", {"double", "RoundTrip1"}));
    
    auto originalStruct1 = manager->findStructure("RoundTrip1");
    auto originalStruct2 = manager->findStructure("RoundTrip2");
    
    // Serialize
    QJsonObject json = manager->toJson();
    
    // Clear and deserialize
    manager->clear();
    QVERIFY(manager->fromJson(json));
    
    // Verify round-trip integrity
    auto restoredStruct1 = manager->findStructure("RoundTrip1");
    auto restoredStruct2 = manager->findStructure("RoundTrip2");
    
    QVERIFY(restoredStruct1 != nullptr);
    QVERIFY(restoredStruct2 != nullptr);
    
    // Compare structures (assuming equality operator)
    QVERIFY(*restoredStruct1 == *originalStruct1);
    QVERIFY(*restoredStruct2 == *originalStruct2);
}

void TestStructureManager::testSerializationErrors()
{
    // Test invalid JSON
    QJsonObject invalidJson;
    invalidJson["invalid"] = "data";
    
    QSignalSpy errorSpy(manager.get(), &StructureManager::errorOccurred);
    
    bool result = manager->fromJson(invalidJson);
    QVERIFY(!result);
    QCOMPARE(errorSpy.count(), 1);
    
    // Test corrupted JSON
    QJsonObject corruptedJson;
    corruptedJson["version"] = "1.0";
    corruptedJson["structures"] = QJsonValue(); // Null value
    
    result = manager->fromJson(corruptedJson);
    QVERIFY(!result);
    
    // Test version mismatch
    QJsonObject versionMismatch;
    versionMismatch["version"] = "999.0";
    versionMismatch["structures"] = QJsonObject();
    
    result = manager->fromJson(versionMismatch);
    // Might succeed with migration or fail - depends on implementation
}

void TestStructureManager::testWorkspaceOperations()
{
    // Add test structures
    manager->addStructure(createTestStruct("WorkspaceStruct1"));
    manager->addStructure(createTestStruct("WorkspaceStruct2"));
    
    // Save workspace
    QString workspacePath = QDir::temp().filePath("test_workspace.json");
    
    QSignalSpy savedSpy(manager.get(), &StructureManager::workspaceSaved);
    
    bool saveResult = manager->saveWorkspace(workspacePath);
    QVERIFY(saveResult);
    QCOMPARE(savedSpy.count(), 1);
    
    // Verify file exists
    QVERIFY(QFile::exists(workspacePath));
    
    // Clear and load workspace
    manager->clear();
    
    QSignalSpy loadedSpy(manager.get(), &StructureManager::workspaceLoaded);
    
    bool loadResult = manager->loadWorkspace(workspacePath);
    QVERIFY(loadResult);
    QCOMPARE(loadedSpy.count(), 1);
    
    // Verify structures restored
    QVERIFY(manager->findStructure("WorkspaceStruct1") != nullptr);
    QVERIFY(manager->findStructure("WorkspaceStruct2") != nullptr);
    
    // Cleanup
    QFile::remove(workspacePath);
}

void TestStructureManager::testWorkspacePersistence()
{
    QString workspacePath = QDir::temp().filePath("persistence_test.json");
    
    // Create and save workspace
    {
        auto tempManager = std::make_unique<StructureManager>();
        tempManager->addStructure(createTestStruct("PersistentStruct"));
        tempManager->saveWorkspace(workspacePath);
    }
    
    // Load in new manager instance
    auto newManager = std::make_unique<StructureManager>();
    bool loaded = newManager->loadWorkspace(workspacePath);
    
    QVERIFY(loaded);
    QVERIFY(newManager->findStructure("PersistentStruct") != nullptr);
    
    // Cleanup
    QFile::remove(workspacePath);
}

void TestStructureManager::testWorkspaceVersioning()
{
    // Test workspace with version information
    QString workspacePath = QDir::temp().filePath("version_test.json");
    
    // Save current version
    manager->addStructure(createTestStruct("VersionedStruct"));
    manager->saveWorkspace(workspacePath);
    
    // Modify saved file to simulate older version
    QFile file(workspacePath);
    QVERIFY(file.open(QIODevice::ReadOnly));
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    
    QJsonObject root = doc.object();
    root["version"] = "0.9"; // Older version
    
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(QJsonDocument(root).toJson());
    file.close();
    
    // Load should handle version upgrade
    manager->clear();
    bool loaded = manager->loadWorkspace(workspacePath);
    
    // Should either succeed with migration or fail gracefully
    if (loaded) {
        QVERIFY(manager->findStructure("VersionedStruct") != nullptr);
    }
    
    // Cleanup
    QFile::remove(workspacePath);
}

void TestStructureManager::testWorkspaceMigration()
{
    // Test migration from older workspace format
    QString migrationPath = QDir::temp().filePath("migration_test.json");
    
    // Create old format workspace file
    QJsonObject oldFormat;
    oldFormat["version"] = "0.5";
    oldFormat["data"] = QJsonObject(); // Old structure format
    
    QFile file(migrationPath);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(QJsonDocument(oldFormat).toJson());
    file.close();
    
    QSignalSpy migrationSpy(manager.get(), &StructureManager::workspaceMigrated);
    
    // Load should trigger migration
    bool loaded = manager->loadWorkspace(migrationPath);
    
    if (loaded) {
        QCOMPARE(migrationSpy.count(), 1);
    }
    
    // Cleanup
    QFile::remove(migrationPath);
}

void TestStructureManager::testLargeNumberOfStructures()
{
    const int numStructures = 10000;
    
    QElapsedTimer timer;
    timer.start();
    
    // Add many structures
    for (int i = 0; i < numStructures; ++i) {
        manager->addStructure(createTestStruct(QString("LargeTest%1").arg(i)));
    }
    
    qint64 addTime = timer.elapsed();
    
    QCOMPARE(manager->getStructureCount(), numStructures);
    
    // Test lookup performance
    timer.start();
    for (int i = 0; i < 1000; ++i) {
        auto found = manager->findStructure(QString("LargeTest%1").arg(i));
        QVERIFY(found != nullptr);
    }
    qint64 lookupTime = timer.elapsed();
    
    qDebug() << "Added" << numStructures << "structures in" << addTime << "ms";
    qDebug() << "1000 lookups took" << lookupTime << "ms";
    
    // Performance requirements
    QVERIFY(addTime < 5000); // Should add 10k structures in < 5 seconds
    QVERIFY(lookupTime < 100); // Should lookup 1k structures in < 100ms
}

void TestStructureManager::testComplexDependencyGraphs()
{
    const int numLevels = 10;
    const int structsPerLevel = 5;
    
    // Create complex dependency graph
    for (int level = 0; level < numLevels; ++level) {
        for (int i = 0; i < structsPerLevel; ++i) {
            QString structName = QString("L%1S%2").arg(level).arg(i);
            auto structure = createTestStruct(structName);
            
            // Add dependencies to previous level
            if (level > 0) {
                for (int j = 0; j < structsPerLevel; ++j) {
                    QString depName = QString("L%1S%2").arg(level - 1).arg(j);
                    structure->addDependency(depName);
                }
            }
            
            manager->addStructure(std::move(structure));
        }
    }
    
    // Test dependency resolution performance
    QElapsedTimer timer;
    timer.start();
    
    auto topologicalOrder = manager->getTopologicalOrder();
    
    qint64 resolutionTime = timer.elapsed();
    
    QCOMPARE(topologicalOrder.size(), numLevels * structsPerLevel);
    QVERIFY(resolutionTime < 1000); // Should resolve complex graph in < 1 second
    
    qDebug() << "Resolved" << (numLevels * structsPerLevel) << "structures in" << resolutionTime << "ms";
}

void TestStructureManager::testMemoryUsage()
{
    // Measure memory usage with many structures
    const int numStructures = 1000;
    
    // Get baseline memory usage
    size_t baselineMemory = manager->getMemoryUsage();
    
    // Add many structures
    for (int i = 0; i < numStructures; ++i) {
        manager->addStructure(createTestStruct(QString("MemTest%1").arg(i)));
    }
    
    size_t afterAddMemory = manager->getMemoryUsage();
    
    // Clear cache to test memory efficiency
    manager->clearCache();
    
    size_t afterClearMemory = manager->getMemoryUsage();
    
    qDebug() << "Baseline memory:" << baselineMemory << "bytes";
    qDebug() << "After adding" << numStructures << "structures:" << afterAddMemory << "bytes";
    qDebug() << "After clearing cache:" << afterClearMemory << "bytes";
    
    // Verify reasonable memory usage
    size_t memoryPerStruct = (afterAddMemory - baselineMemory) / numStructures;
    QVERIFY(memoryPerStruct < 10000); // Should use < 10KB per simple structure
    
    // Cache clearing should reduce memory usage
    QVERIFY(afterClearMemory <= afterAddMemory);
}

void TestStructureManager::testLookupPerformance()
{
    const int numStructures = 1000;
    const int numLookups = 10000;
    
    // Add structures
    for (int i = 0; i < numStructures; ++i) {
        manager->addStructure(createTestStruct(QString("PerfTest%1").arg(i)));
    }
    
    // Random lookup test
    QElapsedTimer timer;
    timer.start();
    
    for (int i = 0; i < numLookups; ++i) {
        int randomIndex = QRandomGenerator::global()->bounded(numStructures);
        QString structName = QString("PerfTest%1").arg(randomIndex);
        auto found = manager->findStructure(structName);
        Q_UNUSED(found);
    }
    
    qint64 lookupTime = timer.elapsed();
    
    double lookupsPerMs = double(numLookups) / lookupTime;
    
    qDebug() << numLookups << "random lookups took" << lookupTime << "ms";
    qDebug() << "Lookups per ms:" << lookupsPerMs;
    
    // Performance requirement: > 1000 lookups per second
    QVERIFY(lookupsPerMs > 1.0);
}

void TestStructureManager::testErrorRecovery()
{
    // Add valid structures
    manager->addStructure(createTestStruct("ValidStruct1"));
    manager->addStructure(createTestStruct("ValidStruct2"));
    
    QSignalSpy errorSpy(manager.get(), &StructureManager::errorOccurred);
    
    // Cause various errors
    manager->addStructure(nullptr); // Null structure
    manager->removeStructure("NonExistent"); // Non-existent structure
    
    // Manager should continue to function
    QVERIFY(manager->findStructure("ValidStruct1") != nullptr);
    QVERIFY(manager->findStructure("ValidStruct2") != nullptr);
    
    // Should be able to add more structures
    QVERIFY(manager->addStructure(createTestStruct("ValidStruct3")));
    
    QVERIFY(errorSpy.count() >= 1);
}

void TestStructureManager::testCorruptedData()
{
    // Test loading corrupted workspace
    QString corruptedPath = QDir::temp().filePath("corrupted.json");
    
    QFile file(corruptedPath);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("{ corrupted json data }");
    file.close();
    
    QSignalSpy errorSpy(manager.get(), &StructureManager::errorOccurred);
    
    bool loaded = manager->loadWorkspace(corruptedPath);
    QVERIFY(!loaded);
    QCOMPARE(errorSpy.count(), 1);
    
    // Manager should remain functional
    QVERIFY(manager->addStructure(createTestStruct("RecoveryTest")));
    
    // Cleanup
    QFile::remove(corruptedPath);
}

void TestStructureManager::testResourceExhaustion()
{
    // Simulate resource exhaustion scenarios
    
    // Test with extremely large number of structures
    const int extremeNumber = 100000;
    
    QElapsedTimer timer;
    timer.start();
    
    int successCount = 0;
    for (int i = 0; i < extremeNumber && timer.elapsed() < 10000; ++i) {
        if (manager->addStructure(createTestStruct(QString("Extreme%1").arg(i)))) {
            successCount++;
        } else {
            break; // Stop on first failure
        }
    }
    
    qDebug() << "Successfully added" << successCount << "structures before exhaustion";
    
    // Should handle gracefully and remain functional
    QVERIFY(manager->findStructure("Extreme0") != nullptr);
    QVERIFY(manager->checkIntegrity());
}

void TestStructureManager::testParserIntegration()
{
    // This test would require actual parser integration
    // For now, test the interface that would be used by parser
    
    // Simulate parser results
    auto parsedStruct1 = createTestStruct("ParsedStruct1", {"int", "char"});
    auto parsedStruct2 = createTestStruct("ParsedStruct2", {"double", "ParsedStruct1"});
    parsedStruct2->addDependency("ParsedStruct1");
    
    // Batch add from parser
    QList<std::unique_ptr<StructNode>> parsedStructures;
    parsedStructures.append(std::move(parsedStruct1));
    parsedStructures.append(std::move(parsedStruct2));
    
    QSignalSpy batchAddedSpy(manager.get(), &StructureManager::structuresBatchAdded);
    
    bool result = manager->addStructures(std::move(parsedStructures));
    QVERIFY(result);
    QCOMPARE(batchAddedSpy.count(), 1);
    
    // Verify dependency resolution
    auto deps = manager->resolveDependencies("ParsedStruct2");
    QVERIFY(deps.contains("ParsedStruct1"));
}

void TestStructureManager::testLayoutCalculatorIntegration()
{
    // Test integration with layout calculator
    auto structForLayout = createTestStruct("LayoutStruct", {"char", "int", "double"});
    manager->addStructure(std::move(structForLayout));
    
    // Trigger layout calculation
    QSignalSpy layoutCalculatedSpy(manager.get(), &StructureManager::layoutCalculated);
    
    bool calculated = manager->calculateLayout("LayoutStruct");
    QVERIFY(calculated);
    QCOMPARE(layoutCalculatedSpy.count(), 1);
    
    // Verify layout information was updated
    auto structure = manager->findStructure("LayoutStruct");
    QVERIFY(structure->getSize() > 0);
    QVERIFY(structure->getAlignment() > 0);
    
    // Test batch layout calculation
    manager->addStructure(createTestStruct("LayoutStruct2", {"short", "long"}));
    
    bool batchCalculated = manager->calculateAllLayouts();
    QVERIFY(batchCalculated);
}

void TestStructureManager::testRealWorldScenarios()
{
    // Test scenario: Network protocol parsing
    auto ipHeader = createTestStruct("IPHeader");
    // Add typical IP header fields with bitfields
    
    auto tcpHeader = createTestStruct("TCPHeader");
    // Add typical TCP header fields
    
    auto packet = createTestStruct("NetworkPacket", {"IPHeader", "TCPHeader", "char"});
    packet->addDependency("IPHeader");
    packet->addDependency("TCPHeader");
    
    manager->addStructure(std::move(ipHeader));
    manager->addStructure(std::move(tcpHeader));
    manager->addStructure(std::move(packet));
    
    // Verify dependency resolution for complex packet
    auto packetDeps = manager->resolveDependencies("NetworkPacket");
    QCOMPARE(packetDeps.size(), 2);
    QVERIFY(packetDeps.contains("IPHeader"));
    QVERIFY(packetDeps.contains("TCPHeader"));
    
    // Test scenario: Embedded register map
    auto peripheralRegs = createComplexStruct();
    manager->addStructure(std::move(peripheralRegs));
    
    // Verify structure was added successfully
    QVERIFY(manager->findStructure("ComplexStruct") != nullptr);
    
    // Test scenario: Large codebase with many interdependent structures
    createDependencyChain(20);
    
    auto chainOrder = manager->getTopologicalOrder();
    QVERIFY(chainOrder.size() >= 20);
    
    // Verify no circular dependencies
    QVERIFY(!manager->hasCircularDependencies());
}

// Helper method implementations
std::unique_ptr<StructNode> TestStructureManager::createTestStruct(const QString& name, const QStringList& fieldTypes)
{
    auto structure = std::make_unique<StructNode>(name);
    
    for (int i = 0; i < fieldTypes.size(); ++i) {
        QString fieldName = QString("field%1").arg(i);
        structure->addField(std::make_unique<FieldNode>(fieldName, fieldTypes[i]));
    }
    
    return structure;
}

std::unique_ptr<StructNode> TestStructureManager::createComplexStruct()
{
    auto structure = std::make_unique<StructNode>("ComplexStruct");
    
    // Add various field types
    structure->addField(std::make_unique<FieldNode>("id", "uint32_t"));
    structure->addField(std::make_unique<FieldNode>("name", "char"));
    structure->getFields().back()->setArraySize(32);
    
    // Add bitfield
    auto flags = std::make_unique<FieldNode>("flags", "uint16_t");
    flags->setBitField(true, 12);
    structure->addField(std::move(flags));
    
    // Add nested structure
    auto nested = std::make_unique<StructNode>("NestedStruct");
    nested->addField(std::make_unique<FieldNode>("x", "double"));
    nested->addField(std::make_unique<FieldNode>("y", "double"));
    
    auto nestedField = std::make_unique<FieldNode>("position", "NestedStruct");
    nestedField->setNestedStruct(std::move(nested));
    structure->addField(std::move(nestedField));
    
    return structure;
}

void TestStructureManager::verifyDependencyOrder(const QList<QString>& order, const QStringList& expectedBefore)
{
    for (int i = 0; i < expectedBefore.size() - 1; ++i) {
        int index1 = order.indexOf(expectedBefore[i]);
        int index2 = order.indexOf(expectedBefore[i + 1]);
        
        QVERIFY(index1 >= 0);
        QVERIFY(index2 >= 0);
        QVERIFY(index1 < index2);
    }
}

void TestStructureManager::createDependencyChain(int depth)
{
    for (int i = 0; i < depth; ++i) {
        QString structName = QString("ChainStruct%1").arg(i);
        auto structure = createTestStruct(structName);
        
        if (i > 0) {
            QString prevName = QString("ChainStruct%1").arg(i - 1);
            structure->addDependency(prevName);
        }
        
        manager->addStructure(std::move(structure));
    }
}

void TestStructureManager::setupCircularDependency()
{
    auto structA = createTestStruct("CircularA", {"CircularB*"});
    structA->addDependency("CircularB");
    
    auto structB = createTestStruct("CircularB", {"CircularC*"});
    structB->addDependency("CircularC");
    
    auto structC = createTestStruct("CircularC", {"CircularA*"});
    structC->addDependency("CircularA");
    
    manager->addStructure(std::move(structA));
    manager->addStructure(std::move(structB));
    manager->addStructure(std::move(structC));
}

QJsonObject TestStructureManager::createTestJSON()
{
    QJsonObject root;
    root["version"] = "1.0";
    
    QJsonObject structures;
    
    // TestStruct1
    QJsonObject struct1;
    struct1["name"] = "TestStruct1";
    struct1["size"] = 12;
    struct1["alignment"] = 4;
    
    QJsonArray fields1;
    QJsonObject field1;
    field1["name"] = "field0";
    field1["type"] = "int";
    field1["offset"] = 0;
    field1["size"] = 4;
    fields1.append(field1);
    
    QJsonObject field2;
    field2["name"] = "field1";
    field2["type"] = "double";
    field2["offset"] = 4;
    field2["size"] = 8;
    fields1.append(field2);
    
    struct1["fields"] = fields1;
    structures["TestStruct1"] = struct1;
    
    // TestStruct2
    QJsonObject struct2;
    struct2["name"] = "TestStruct2";
    struct2["dependencies"] = QJsonArray({"TestStruct1"});
    structures["TestStruct2"] = struct2;
    
    root["structures"] = structures;
    
    QJsonObject dependencies;
    dependencies["TestStruct2"] = QJsonArray({"TestStruct1"});
    root["dependencies"] = dependencies;
    
    return root;
}

QTEST_MAIN(TestStructureManager)
#include "test_structure_manager.moc"