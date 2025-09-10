#include <gtest/gtest.h>
#include <QApplication>
#include <QTimer>
#include <QEventLoop>
#include <memory>

#include "../../src/packet/sources/simulation_source.h"
#include "../../src/packet/core/packet_factory.h"
#include "../../src/memory/memory_pool_manager.h"
#include "../../src/events/event_dispatcher.h"
#include "../../src/logging/logger.h"
#include "../../src/test_framework/core/test_definition.h"
#include "../../src/test_framework/core/test_result.h"
#include "../../src/test_framework/parser/expression_lexer.h"
#include "../../src/test_framework/parser/expression_parser.h"
#include "../../src/test_framework/execution/expression_evaluator.h"

using namespace Monitor;

class SimpleTestFrameworkIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!QApplication::instance()) {
            int argc = 1;
            char* argv[] = {const_cast<char*>("test")};
            app = new QApplication(argc, argv);
        }

        memoryManager = std::make_unique<Memory::MemoryPoolManager>();
        eventDispatcher = std::make_unique<Events::EventDispatcher>();
        logger = Logging::Logger::instance();
        packetFactory = std::make_unique<Packet::PacketFactory>(memoryManager.get());
    }

    void TearDown() override {
        simulationSource.reset();
        packetFactory.reset();
        eventDispatcher.reset();
        memoryManager.reset();
    }

    std::unique_ptr<Memory::MemoryPoolManager> memoryManager;
    std::unique_ptr<Events::EventDispatcher> eventDispatcher;
    std::unique_ptr<Packet::PacketFactory> packetFactory;
    std::shared_ptr<Packet::SimulationSource> simulationSource;
    Logging::Logger* logger;
    QApplication* app = nullptr;
};

TEST_F(SimpleTestFrameworkIntegrationTest, BasicTestExecution) {
    // Create a simple test definition
    TestFramework::TestDefinition test;
    test.setId("simple_001");
    test.setName("Basic Packet Validation");
    test.setExpression("packet_id > 0");
    test.setSeverity(TestFramework::TestDefinition::Severity::ERROR);
    test.setEnabled(true);

    // Create simulation source
    auto config = Packet::SimulationSource::createDefaultConfig();
    simulationSource = std::make_shared<Packet::SimulationSource>(config);
    simulationSource->setPacketFactory(packetFactory.get());
    simulationSource->setEventDispatcher(eventDispatcher.get());

    // Set up test evaluation
    TestFramework::ExpressionEvaluator evaluator;
    TestFramework::ExpressionLexer lexer;
    TestFramework::ExpressionParser parser;

    auto tokens = lexer.tokenize(test.getExpression());
    auto expr = parser.parse(tokens);
    ASSERT_TRUE(expr != nullptr);

    // Track test results
    int testCount = 0;
    int passedCount = 0;

    connect(simulationSource.get(), &Packet::PacketSource::packetReady,
            [&](PacketPtr packet) {
                TestFramework::EvaluationContext context;
                context.setVariable("packet_id", static_cast<double>(packet->getHeader().packet_id));
                
                auto result = evaluator.evaluate(expr, context);
                testCount++;
                
                if (result.isValid() && result.asBool()) {
                    passedCount++;
                }
            });

    // Run test
    ASSERT_TRUE(simulationSource->start());

    QEventLoop loop;
    QTimer::singleShot(1000, &loop, &QEventLoop::quit);
    loop.exec();

    simulationSource->stop();

    // Verify results
    EXPECT_GT(testCount, 0);
    EXPECT_GT(passedCount, 0);
    EXPECT_EQ(passedCount, testCount) << "All packet IDs should be > 0";

    logger->info("SimpleTestFrameworkIntegration",
                QString("Basic test: %1/%2 passed").arg(passedCount).arg(testCount));
}

TEST_F(SimpleTestFrameworkIntegrationTest, ExpressionEvaluation) {
    // Test basic expression evaluation without packets
    TestFramework::ExpressionEvaluator evaluator;
    TestFramework::ExpressionLexer lexer;
    TestFramework::ExpressionParser parser;
    TestFramework::EvaluationContext context;

    // Test simple expressions
    struct TestCase {
        std::string expression;
        double expectedValue;
        bool shouldPass;
    };

    std::vector<TestCase> testCases = {
        {"5 + 3", 8.0, true},
        {"10 - 4", 6.0, true},
        {"3 * 7", 21.0, true},
        {"15 / 3", 5.0, true},
        {"2 > 1", 1.0, true},
        {"1 > 2", 0.0, true},
        {"5 == 5", 1.0, true},
        {"5 != 3", 1.0, true}
    };

    for (const auto& testCase : testCases) {
        auto tokens = lexer.tokenize(testCase.expression);
        auto expr = parser.parse(tokens);
        
        if (testCase.shouldPass) {
            ASSERT_TRUE(expr != nullptr) << "Failed to parse: " << testCase.expression;
            
            auto result = evaluator.evaluate(expr, context);
            EXPECT_TRUE(result.isValid()) << "Evaluation failed for: " << testCase.expression;
            EXPECT_DOUBLE_EQ(result.asDouble(), testCase.expectedValue) 
                << "Wrong result for: " << testCase.expression;
        }
    }
}

TEST_F(SimpleTestFrameworkIntegrationTest, TestResultCreation) {
    // Test TestResult functionality
    TestFramework::TestResult result;
    result.setTestId("result_001");
    result.setTimestamp(std::chrono::system_clock::now());
    result.setPacketId(1001);
    result.setSequenceNumber(42);
    result.setPassed(true);

    EXPECT_EQ(result.getTestId(), "result_001");
    EXPECT_EQ(result.getPacketId(), 1001u);
    EXPECT_EQ(result.getSequenceNumber(), 42u);
    EXPECT_TRUE(result.isPassed());

    // Test failure case
    TestFramework::TestResult failureResult;
    failureResult.setTestId("failure_001");
    failureResult.setPassed(false);
    failureResult.setFailureMessage("Test failed");

    EXPECT_FALSE(failureResult.isPassed());
    EXPECT_EQ(failureResult.getFailureMessage(), "Test failed");
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    
    QApplication app(argc, argv);
    
    return RUN_ALL_TESTS();
}