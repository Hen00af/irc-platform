#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""担当クラスのミューテーションテスト。

単体テストが「通る」ことと、単体テストが「バグを捕まえられる」ことは別である。
アサーションが恒真だったり、期待値を実装から逆算していたりすると、全部
パスしていてもバグを 1 つも検出できない。

このスクリプトは実装へわざとバグを注入し、単体テストが失敗することを確認する。

  検出  = バグを注入したらテストが落ちた      → テストが効いている
  生存  = バグを注入してもテストが通った      → テストの穴。要追加
  compile error = 注入したコードがコンパイル不能 → ミューテーションとして無効

各バグは、設計書の判断や過去に実際やらかした間違いから採っている。

このスクリプトは開発用のツールであり、提出物 ircserv のビルドには含まれない。
実行後、対象ファイルは必ず元へ戻す (中断・失敗時も finally で復元する)。
"""

import os
import re
import subprocess
import sys

TOOLS_DIR = os.path.dirname(os.path.abspath(__file__))
TESTS_DIR = os.path.dirname(TOOLS_DIR)
ROOT = os.path.dirname(TESTS_DIR)
REPORT = os.path.join(TESTS_DIR, "mutation_report.md")

# id, ファイル, 落ちるべき suite, 何を壊すか, 検索文字列, 置換文字列
MUTATIONS = [
    (
        "行長閾値を設計書が元々書いていた誤った値へ",
        "prd/util/BufferUtil.cpp",
        "bufferutil",
        "LF なしの切断閾値を 512 から 510 超へ。本文 510+CRLF の正当な"
        "最大長行が分割受信されたとき誤って切断する",
        "buffer.size() >= IrcUtil::IRC_MAX_LINE",
        "buffer.size() > IrcUtil::IRC_MAX_CONTENT",
    ),
    (
        "行長判定を LF 込みの長さで行う",
        "prd/util/BufferUtil.cpp",
        "bufferutil",
        "本文ではなく行全体を 512 と比較。本文 511+LF を誤って受理する",
        "if (raw.size() > IrcUtil::IRC_MAX_CONTENT)",
        "if (raw.size() > IrcUtil::IRC_MAX_LINE)",
    ),
    (
        "超過行でも out を書き込む",
        "prd/util/BufferUtil.cpp",
        "bufferutil",
        "長さ検査の前に out へ代入。Server が超過行を処理してしまう",
        "    if (raw.size() > IrcUtil::IRC_MAX_CONTENT)\n        return LINE_TOO_LONG;\n\n    out = raw;",
        "    out = raw;\n\n    if (raw.size() > IrcUtil::IRC_MAX_CONTENT)\n        return LINE_TOO_LONG;",
    ),
    (
        "新規 Channel の初期 Mode を -t にする",
        "prd/domain/Channel.cpp",
        "channel",
        "設計書 02 §6.2 は新規 Channel で t を有効にする",
        "_topicRestricted(true)",
        "_topicRestricted(false)",
    ),
    (
        "非 Member を Operator にできるようにする",
        "prd/domain/Channel.cpp",
        "channel",
        "不変条件「Operator は必ず Member」を破る",
        "    if (!hasMember(fd))\n        return false;\n    return _operators.insert(fd).second;",
        "    return _operators.insert(fd).second;",
    ),
    (
        "removeMember が Operator 集合を残す",
        "prd/domain/Channel.cpp",
        "channel",
        "退出した FD が Operator に残り、不変条件を破る",
        "    /* 不変条件「Operator FD は必ず Member FD でもある」を保つ */\n    _operators.erase(fd);\n    return removed;",
        "    return removed;",
    ),
    (
        "isFull の境界を 1 ずらす",
        "prd/domain/Channel.cpp",
        "channel",
        "Limit ちょうどで満員にならず、上限を 1 人超える",
        "return _members.size() >= _userLimit;",
        "return _members.size() > _userLimit;",
    ),
    (
        "Key 未設定の Channel へ参加できなくする",
        "prd/domain/Channel.cpp",
        "channel",
        "matchesKey が Key 未設定で false を返す",
        "    if (!_keyEnabled)\n        return true;",
        "    if (!_keyEnabled)\n        return false;",
    ),
    (
        "buildModeParameters を挿入順で返す",
        "prd/domain/Channel.cpp",
        "channel",
        "Key より先に Limit を積む。itkl 順が崩れ 324 の Parameter が入れ替わる",
        "    if (_keyEnabled)\n        parameters.push_back(_channelKey);\n\n    if (_limitEnabled)\n    {\n        std::ostringstream oss;\n\n        oss << _userLimit;\n        parameters.push_back(oss.str());\n    }\n    return parameters;",
        "    if (_limitEnabled)\n    {\n        std::ostringstream oss;\n\n        oss << _userLimit;\n        parameters.push_back(oss.str());\n    }\n\n    if (_keyEnabled)\n        parameters.push_back(_channelKey);\n    return parameters;",
    ),
    (
        "PASS なしで登録完了できるようにする",
        "prd/domain/Client.cpp",
        "client",
        "設計書 04 §5「登録完了は PASS 成功後だけ」を破る。認証を回避できる",
        "if (_passwordAccepted && hasNickname() && _userReceived)",
        "if (hasNickname() && _userReceived)",
    ),
    (
        "tryCompleteRegistration が毎回 true を返す",
        "prd/domain/Client.cpp",
        "client",
        "登録済み判定を外す。Welcome Reply が二重送信される",
        "    if (_registered)\n        return false;\n\n    if (_passwordAccepted",
        "    if (false)\n        return false;\n\n    if (_passwordAccepted",
    ),
    (
        "eraseSendPrefix が毎回全消しする",
        "prd/domain/Client.cpp",
        "client",
        "部分送信で未送信データを捨てる。メッセージが欠落する",
        "void Client::eraseSendPrefix(std::size_t length)\n{\n    if (length >= _sendBuffer.size())\n        _sendBuffer.clear();\n    else\n        _sendBuffer.erase(0, length);\n}",
        "void Client::eraseSendPrefix(std::size_t length)\n{\n    (void)length;\n    _sendBuffer.clear();\n}",
    ),
    (
        "Parser の NUL 検査を外す",
        "prd/util/Parser.cpp",
        "parser",
        "設計書 06 §12.1「NUL を含む行を破棄」を破る",
        "    if (line.find('\\0') != std::string::npos)\n        return false;",
        "",
    ),
    (
        "Parameter 数上限を 1 ずらす",
        "prd/util/Parser.cpp",
        "parser",
        "16 個の Parameter を受理してしまう",
        "if (parsed.params.size() >= MAX_PARAMS)",
        "if (parsed.params.size() > MAX_PARAMS)",
    ),
    (
        "ircCaseFold の ^ 変換を落とす",
        "prd/util/IrcUtil.cpp",
        "ircutil",
        "RFC 2812 の case mapping から ^ が漏れる。Nickname 重複判定が甘くなる",
        "        else if (c == '^')\n            result[i] = '~';",
        "",
    ),
    (
        "overflow ガードを素朴な式へ",
        "prd/util/IrcUtil.cpp",
        "ircutil",
        "digit を無視した判定。size_t の max 付近を誤って拒否する",
        "if (parsed > (maximum - digit) / 10)",
        "if (parsed > maximum / 10)",
    ),
    (
        "sanitizeMessageText が CR を残す",
        "prd/util/IrcUtil.cpp",
        "ircutil",
        "設計書 06 §21 の CRLF injection 対策が破れる。最重要",
        "if (c != '\\r' && c != '\\n' && c != '\\0')",
        "if (c != '\\n' && c != '\\0')",
    ),
    (
        "isSafeToken の先頭 colon 検査を外す",
        "prd/util/IrcUtil.cpp",
        "ircutil",
        "Nickname \":x\" で Numeric の target が Trailing として解釈される",
        "    if (value[0] == ':')\n        return false;",
        "",
    ),
    (
        "Nickname の長さ上限を緩める",
        "prd/util/IrcUtil.cpp",
        "ircutil",
        "RFC 2812 の 9 文字を超える Nickname を通す",
        "if (value.empty() || value.size() > NICKNAME_MAX_LENGTH)",
        "if (value.empty() || value.size() > NICKNAME_MAX_LENGTH + 1)",
    ),
    (
        "Reply の target 検査を外す",
        "prd/util/Reply.cpp",
        "reply",
        "CR を含む Nickname が Numeric の target へ入り、行が壊れる",
        "    if (!IrcUtil::isSafeToken(targetName))\n        return std::string();",
        "",
    ),
]


def run(cmd):
    return subprocess.run(cmd, cwd=TESTS_DIR, shell=True,
                          stdout=subprocess.PIPE, stderr=subprocess.STDOUT)


def force_rebuild(relpath):
    """対象の .o とテストバイナリを消し、make に再コンパイルと再リンクを強制する。

    ファイルを書き換えて即 make すると mtime が同じ秒に収まり、make が
    「更新なし」と判断して再ビルドしないことがある。.o だけ消しても、今度は
    .o とバイナリの mtime が同じ秒になりリンクがスキップされる。
    どちらの場合も古いバイナリでテストしてしまい、注入したバグを見逃したのか
    単にビルドされていないだけなのか区別できない。成果物を消すのが確実。
    """
    obj = os.path.join(TESTS_DIR, "obj",
                       os.path.basename(relpath).replace(".cpp", ".o"))
    for path in (obj, os.path.join(TESTS_DIR, "run_tests")):
        if os.path.exists(path):
            os.remove(path)


def failed_count(output):
    m = re.search(r"(\d+) passed, (\d+) failed", output)
    return (int(m.group(1)), int(m.group(2))) if m else (0, 0)


def main():
    print("ベースラインを確認します...")
    base = run("make -s re 2>&1 && ./run_tests")
    if base.returncode != 0:
        print("ベースラインが失敗しています。先に make test を通してください。")
        print(base.stdout.decode("utf-8", "replace")[-2000:])
        return 1
    passed, _ = failed_count(base.stdout.decode("utf-8", "replace"))
    print("ベースライン: %d 件パス\n" % passed)

    results = []
    for i, (name, relpath, suite, why, find, repl) in enumerate(MUTATIONS, 1):
        path = os.path.join(ROOT, relpath)
        with open(path, encoding="utf-8") as f:
            original = f.read()

        if find not in original:
            results.append((name, relpath, suite, why, "対象なし", 0))
            print("[%2d/%d] %-44s 対象文字列が見つかりません"
                  % (i, len(MUTATIONS), name))
            continue

        try:
            with open(path, "w", encoding="utf-8") as f:
                f.write(original.replace(find, repl, 1))
            force_rebuild(relpath)

            build = run("make -s run_tests 2>&1")
            if build.returncode != 0:
                verdict, caught = "compile error", 0
            else:
                res = run("./run_tests %s" % suite)
                out = res.stdout.decode("utf-8", "replace")
                _, caught = failed_count(out)
                verdict = "検出" if res.returncode != 0 else "生存"
        finally:
            with open(path, "w", encoding="utf-8") as f:
                f.write(original)
            force_rebuild(relpath)

        results.append((name, relpath, suite, why, verdict, caught))
        mark = {"検出": "OK", "生存": "!! 穴", "compile error": "-- 無効"}
        print("[%2d/%d] %-44s %-8s %s"
              % (i, len(MUTATIONS), name, mark.get(verdict, "?"),
                 ("%d 件が検出" % caught) if caught else ""))

    run("make -s re >/dev/null 2>&1")

    detected = sum(1 for r in results if r[4] == "検出")
    survived = [r for r in results if r[4] == "生存"]
    invalid = [r for r in results if r[4] not in ("検出", "生存")]
    valid = len(results) - len(invalid)

    lines = []
    lines.append("# ミューテーションテスト結果\n")
    lines.append("実装へわざとバグを注入し、単体テストが検出できるかを確認する。\n")
    lines.append("- **検出** … バグを注入したらテストが落ちた。テストが効いている")
    lines.append("- **生存** … 注入してもテストが通った。**テストの穴**")
    lines.append("- **無効** … 注入したコードがコンパイル不能。判定対象外\n")
    lines.append("## サマリ\n")
    lines.append("| 項目 | 値 |")
    lines.append("|---|---|")
    lines.append("| ベースライン | %d 件パス |" % passed)
    lines.append("| 注入したバグ | %d 件 |" % len(results))
    lines.append("| 検出 | %d 件 |" % detected)
    lines.append("| 生存 (テストの穴) | %d 件 |" % len(survived))
    if invalid:
        lines.append("| 判定対象外 | %d 件 |" % len(invalid))
    if valid:
        lines.append("| **検出率** | **%d / %d (%.0f%%)** |"
                     % (detected, valid, 100.0 * detected / valid))
    lines.append("")
    lines.append("## 明細\n")
    lines.append("| # | 注入したバグ | ファイル | suite | 結果 | 落ちたテスト |")
    lines.append("|---|---|---|---|---|---|")
    for i, (name, relpath, suite, why, verdict, caught) in enumerate(results, 1):
        mark = {"検出": "検出", "生存": "**生存**"}.get(verdict, verdict)
        lines.append("| %d | %s | `%s` | %s | %s | %s |"
                     % (i, name, os.path.basename(relpath), suite, mark,
                        ("%d 件" % caught) if caught else "-"))
    lines.append("")
    lines.append("## 各バグが破る仕様\n")
    for i, (name, _, _, why, _, _) in enumerate(results, 1):
        lines.append("%d. **%s** — %s" % (i, name, why))
    lines.append("")
    if survived:
        lines.append("## 生存したバグ (テストを追加すべき箇所)\n")
        for name, relpath, suite, why, _, _ in survived:
            lines.append("- **%s** (`%s`, suite: %s) — %s"
                         % (name, os.path.basename(relpath), suite, why))
        lines.append("")

    report = "\n".join(lines)
    with open(REPORT, "w", encoding="utf-8") as f:
        f.write(report)

    print("\n" + "=" * 60)
    print("検出 %d / 判定対象 %d" % (detected, valid))
    if survived:
        print("生存 %d 件。テストに穴があります:" % len(survived))
        for name, _, _, _, _, _ in survived:
            print("  - %s" % name)
    print("レポート: %s" % REPORT)
    return 1 if survived else 0


if __name__ == "__main__":
    sys.exit(main())
