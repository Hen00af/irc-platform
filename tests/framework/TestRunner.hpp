#pragma once

#include <cstddef>
#include <sstream>
#include <string>

/* ============================================================
 * TestRunner
 *
 * 外部ライブラリが禁止されているため、自前の軽量アサーション。
 * C++98 のみを使用する。
 *
 * 使い方:
 *   ASSERT_EQ("Client.getFd", client.getFd(), 3);
 *   ASSERT_TRUE("Channel.isEmpty", channel.isEmpty());
 *   ASSERT_FALSE("Channel.hasKey", channel.hasKey());
 *
 * 比較する 2 つの値は型を揃えること。符号あり / なしの比較は
 * -Wsign-compare で警告になる。
 *
 * ASSERT_EQ は関数テンプレートへ委譲しており、実引数は成否に
 * かかわらず呼び出し側で 1 回だけ評価される。マクロ内で
 * actual / expected を複数回展開すると、
 *   ASSERT_FALSE("重複追加は false", channel.addMember(3))
 * のような副作用のある式が失敗時に再実行され、診断が嘘をつく。
 * ============================================================ */
namespace TestRunner
{
    void beginSuite(const std::string &name);
    void pass(const std::string &name);
    void fail(const std::string &name,
              const std::string &expected,
              const std::string &actual,
              const char        *file,
              int                line);
    /* 失敗が 0 なら 0、そうでなければ 1 を返す。main の戻り値に使う */
    int summary();

    /* 失敗時の表示用。文字列は引用符で囲む。
       非テンプレート版は文字列リテラルに対してもテンプレート版に
       優先する (lvalue 変換は順位比較から除外され、同順位なら
       非テンプレートが勝つ)。const char * 以外のポインタ版を
       足すとこの解決が変わるので注意 */
    std::string display(const std::string &value);
    std::string display(const char *value);
    std::string display(bool value);

    template <typename T>
    std::string display(const T &value)
    {
        std::ostringstream oss;

        oss << value;
        return oss.str();
    }

    /* 実引数は呼び出し側で 1 回だけ評価される */
    template <typename A, typename E>
    void assertEq(const std::string &name,
                  const A           &actual,
                  const E           &expected,
                  const char        *file,
                  int                line)
    {
        if (actual == expected)
            pass(name);
        else
            fail(name, display(expected), display(actual), file, line);
    }
}

#define ASSERT_EQ(name, actual, expected)                                     \
    TestRunner::assertEq((name), (actual), (expected), __FILE__, __LINE__)

#define ASSERT_TRUE(name, expr)  ASSERT_EQ(name, static_cast<bool>(expr), true)
#define ASSERT_FALSE(name, expr) ASSERT_EQ(name, static_cast<bool>(expr), false)
