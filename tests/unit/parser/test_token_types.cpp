#include <QTest>
#include <QObject>
#include "parser/lexer/token_types.h"

using namespace Monitor::Parser::Lexer;

class TestTokenTypes : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    
    void testTokenConstruction();
    void testTokenTypeQueries();
    void testTokenToString();
    void testKeywordRecognition();
    void testOperatorRecognition();
    void testTokenTypeToString();
    void testTokenTypeUtilities();
    void testInvalidTokens();

private:
    void createSampleToken();
};

void TestTokenTypes::initTestCase() {
    // Setup before all tests
}

void TestTokenTypes::cleanupTestCase() {
    // Cleanup after all tests
}

void TestTokenTypes::testTokenConstruction() {
    // Test default constructor
    Token defaultToken;
    QCOMPARE(defaultToken.type, TokenType::INVALID);
    QCOMPARE(defaultToken.value, std::string(""));
    QCOMPARE(defaultToken.line, size_t(0));
    QCOMPARE(defaultToken.column, size_t(0));
    QCOMPARE(defaultToken.position, size_t(0));
    
    // Test parameterized constructor
    Token structToken(TokenType::STRUCT, "struct", 1, 5, 10);
    QCOMPARE(structToken.type, TokenType::STRUCT);
    QCOMPARE(structToken.value, std::string("struct"));
    QCOMPARE(structToken.line, size_t(1));
    QCOMPARE(structToken.column, size_t(5));
    QCOMPARE(structToken.position, size_t(10));
}

void TestTokenTypes::testTokenTypeQueries() {
    // Test keyword detection
    Token structToken(TokenType::STRUCT, "struct", 1, 1, 1);
    QVERIFY(structToken.isKeyword());
    QVERIFY(!structToken.isOperator());
    QVERIFY(!structToken.isLiteral());
    QVERIFY(!structToken.isDelimiter());
    QVERIFY(!structToken.isType());
    
    // Test operator detection
    Token plusToken(TokenType::PLUS, "+", 1, 1, 1);
    QVERIFY(!plusToken.isKeyword());
    QVERIFY(plusToken.isOperator());
    QVERIFY(!plusToken.isLiteral());
    QVERIFY(!plusToken.isDelimiter());
    QVERIFY(!plusToken.isType());
    
    // Test literal detection
    Token identifierToken(TokenType::IDENTIFIER, "myVar", 1, 1, 1);
    QVERIFY(!identifierToken.isKeyword());
    QVERIFY(!identifierToken.isOperator());
    QVERIFY(identifierToken.isLiteral());
    QVERIFY(!identifierToken.isDelimiter());
    QVERIFY(!identifierToken.isType());
    
    // Test delimiter detection
    Token semicolonToken(TokenType::SEMICOLON, ";", 1, 1, 1);
    QVERIFY(!semicolonToken.isKeyword());
    QVERIFY(!semicolonToken.isOperator());
    QVERIFY(!semicolonToken.isLiteral());
    QVERIFY(semicolonToken.isDelimiter());
    QVERIFY(!semicolonToken.isType());
    
    // Test type detection
    Token intToken(TokenType::INT, "int", 1, 1, 1);
    QVERIFY(intToken.isKeyword()); // int is both a keyword and a type
    QVERIFY(!intToken.isOperator());
    QVERIFY(!intToken.isLiteral());
    QVERIFY(!intToken.isDelimiter());
    QVERIFY(intToken.isType());
}

void TestTokenTypes::testTokenToString() {
    Token token(TokenType::STRUCT, "struct", 5, 10, 25);
    std::string expected = "Token(STRUCT, \"struct\", 5:10)";
    QCOMPARE(token.toString(), expected);
}

void TestTokenTypes::testKeywordRecognition() {
    // Test known keywords
    QVERIFY(TokenTypeUtils::isKeyword("struct"));
    QVERIFY(TokenTypeUtils::isKeyword("union"));
    QVERIFY(TokenTypeUtils::isKeyword("typedef"));
    QVERIFY(TokenTypeUtils::isKeyword("int"));
    QVERIFY(TokenTypeUtils::isKeyword("const"));
    
    // Test non-keywords
    QVERIFY(!TokenTypeUtils::isKeyword("myVariable"));
    QVERIFY(!TokenTypeUtils::isKeyword("SomeStruct"));
    QVERIFY(!TokenTypeUtils::isKeyword("123"));
    QVERIFY(!TokenTypeUtils::isKeyword(""));
    
    // Test keyword type retrieval
    QCOMPARE(TokenTypeUtils::getKeywordType("struct"), TokenType::STRUCT);
    QCOMPARE(TokenTypeUtils::getKeywordType("union"), TokenType::UNION);
    QCOMPARE(TokenTypeUtils::getKeywordType("int"), TokenType::INT);
    QCOMPARE(TokenTypeUtils::getKeywordType("nonkeyword"), TokenType::INVALID);
}

void TestTokenTypes::testOperatorRecognition() {
    const auto& operators = TokenTypeUtils::getOperators();
    
    // Test some common operators
    QVERIFY(operators.find("+") != operators.end());
    QVERIFY(operators.find("-") != operators.end());
    QVERIFY(operators.find("*") != operators.end());
    QVERIFY(operators.find("==") != operators.end());
    QVERIFY(operators.find("&&") != operators.end());
    QVERIFY(operators.find("->") != operators.end());
    
    // Verify correct mapping
    QCOMPARE(operators.at("+"), TokenType::PLUS);
    QCOMPARE(operators.at("=="), TokenType::EQUAL);
    QCOMPARE(operators.at("&&"), TokenType::LOGICAL_AND);
}

void TestTokenTypes::testTokenTypeToString() {
    QCOMPARE(TokenTypeUtils::tokenTypeToString(TokenType::STRUCT), std::string("STRUCT"));
    QCOMPARE(TokenTypeUtils::tokenTypeToString(TokenType::UNION), std::string("UNION"));
    QCOMPARE(TokenTypeUtils::tokenTypeToString(TokenType::INT), std::string("INT"));
    QCOMPARE(TokenTypeUtils::tokenTypeToString(TokenType::SEMICOLON), std::string("SEMICOLON"));
    QCOMPARE(TokenTypeUtils::tokenTypeToString(TokenType::IDENTIFIER), std::string("IDENTIFIER"));
    QCOMPARE(TokenTypeUtils::tokenTypeToString(TokenType::INVALID), std::string("INVALID"));
}

void TestTokenTypes::testTokenTypeUtilities() {
    // Test keyword map access
    const auto& keywords = TokenTypeUtils::getKeywords();
    QVERIFY(!keywords.empty());
    QVERIFY(keywords.size() > 10); // Should have at least basic C++ keywords
    
    // Test operator map access
    const auto& operators = TokenTypeUtils::getOperators();
    QVERIFY(!operators.empty());
    QVERIFY(operators.size() > 20); // Should have plenty of operators
}

void TestTokenTypes::testInvalidTokens() {
    Token invalidToken;
    QCOMPARE(invalidToken.type, TokenType::INVALID);
    
    // Invalid tokens should not match any category
    QVERIFY(!invalidToken.isKeyword());
    QVERIFY(!invalidToken.isOperator());
    QVERIFY(!invalidToken.isLiteral());
    QVERIFY(!invalidToken.isDelimiter());
    QVERIFY(!invalidToken.isType());
}

void TestTokenTypes::createSampleToken() {
    // Helper method to create sample tokens for other tests
    Token token(TokenType::STRUCT, "struct", 1, 1, 1);
    QVERIFY(token.type == TokenType::STRUCT);
}

// Include the MOC file for Qt's meta-object system
#include "test_token_types.moc"

QTEST_MAIN(TestTokenTypes)