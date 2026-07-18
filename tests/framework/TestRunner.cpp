#include "TestRunner.hpp"

#include <iostream>

namespace
{
    int g_passed = 0;
    int g_failed = 0;
}

void TestRunner::beginSuite(const std::string &name)
{
    std::cout << "\n=== " << name << " ===" << std::endl;
}

void TestRunner::pass(const std::string &name)
{
    ++g_passed;
    std::cout << "  [ok]   " << name << std::endl;
}

void TestRunner::fail(const std::string &name,
                      const std::string &expected,
                      const std::string &actual,
                      const char        *file,
                      int                line)
{
    ++g_failed;
    std::cout << "  [FAIL] " << name << std::endl;
    std::cout << "         expected: " << expected << std::endl;
    std::cout << "         actual:   " << actual << std::endl;
    std::cout << "         at " << file << ":" << line << std::endl;
}

int TestRunner::summary()
{
    std::cout << "\n----------------------------------------" << std::endl;
    std::cout << g_passed << " passed, " << g_failed << " failed" << std::endl;
    return g_failed == 0 ? 0 : 1;
}

std::string TestRunner::display(const std::string &value)
{
    return "\"" + value + "\"";
}

std::string TestRunner::display(const char *value)
{
    if (value == 0)
        return "NULL";
    return "\"" + std::string(value) + "\"";
}

std::string TestRunner::display(bool value)
{
    return value ? "true" : "false";
}
