#ifndef PULSE_PARSER_TESTS_PARSER_TEST_UTIL_H
#define PULSE_PARSER_TESTS_PARSER_TEST_UTIL_H

// Helpers shared by the parser suites, on top of the compiler-wide TestUtil helpers.

#include <gtest/gtest.h>

#include <iostream>
#include <sstream>
#include <string>

#include "test_helpers.h"

namespace ParserTest
{
    using namespace Pulse::Parser;
    using namespace TestUtil;

    /// The error a source fails with: message and location. `thrown` is false when it parsed.
    struct SyntaxError
    {
        bool thrown = false;
        std::string message;
        size_t line = 0;
        size_t column = 0;
    };

    inline SyntaxError syntaxError(const std::string& source)
    {
        SyntaxError error;
        try
        {
            (void)parseSource(source);
        }
        catch (const compiler_error& e)
        {
            error = { true, e.what(), e.location().line, e.location().column };
        }
        return error;
    }

    /// `node` as a T; fails the test (and returns null) when it is something else.
    template <typename T>
    const T* as(const ASTNode* node)
    {
        const T* typed = dynamic_cast<const T*>(node);
        EXPECT_NE(typed, nullptr) << "node is not a " << typeid(T).name();
        return typed;
    }

    /// The body statements of the first architecture.
    inline const std::vector<StatementPtr>& architectureBody(const ASTRoot& root)
    {
        return firstArchitecture(root)->body;
    }

    /// The first statement of the first process of the first architecture.
    inline const std::vector<StatementPtr>& processBody(const ASTRoot& root)
    {
        return as<ProcessStatement>(architectureBody(root).at(0).get())->body;
    }

    /// Output of node.print(), captured by redirecting std::cout.
    inline std::string dump(const ASTNode& node)
    {
        std::ostringstream captured;
        std::streambuf* previous = std::cout.rdbuf(captured.rdbuf());
        node.print(0);
        std::cout.rdbuf(previous);
        return captured.str();
    }

} // namespace ParserTest

#endif // PULSE_PARSER_TESTS_PARSER_TEST_UTIL_H
