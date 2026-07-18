#include "TestRunner.hpp"

#include <iostream>
#include <string>

/* 各 suite の実装は test_<name>.cpp にある */
void runIrcUtilTests();
void runBufferUtilTests();
void runClientTests();
void runChannelTests();
void runParserTests();
void runReplyTests();

/* ============================================================
 * 単体テストの実行本体。
 *
 * 提出物 ircserv とは別ビルドのため、この main() が通常ビルドへ
 * 混入することはない (prd/Makefile は tests/ を参照しない)。
 *
 *   ./run_tests           すべて実行
 *   ./run_tests client    Client だけ実行
 * ============================================================ */
int main(int argc, char **argv)
{
    std::string suite = (argc > 1) ? argv[1] : "all";
    bool        all   = (suite == "all");
    bool        ran   = false;

    if (all || suite == "ircutil")
    {
        runIrcUtilTests();
        ran = true;
    }
    if (all || suite == "bufferutil")
    {
        runBufferUtilTests();
        ran = true;
    }
    if (all || suite == "client")
    {
        runClientTests();
        ran = true;
    }
    if (all || suite == "channel")
    {
        runChannelTests();
        ran = true;
    }
    if (all || suite == "parser")
    {
        runParserTests();
        ran = true;
    }
    if (all || suite == "reply")
    {
        runReplyTests();
        ran = true;
    }

    if (!ran)
    {
        std::cerr << "Unknown suite: " << suite << std::endl;
        std::cerr << "Usage: " << argv[0]
                  << " [all|ircutil|bufferutil|client|channel|parser|reply]" << std::endl;
        return 1;
    }
    return TestRunner::summary();
}
