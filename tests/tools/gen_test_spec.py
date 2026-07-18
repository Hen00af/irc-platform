#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""テストコードからテスト項目一覧を生成する。

各 test_<suite>.cpp の隣へ test_<suite>.txt を出力する。アサーション名を
そのまま拾うため、コードとずれない。手で書き写した一覧は必ず腐るので、
生成する方針にしている。

  cd tests && make spec

生成物は編集しないこと。項目を直したいときはテストコード側の
アサーション名を直して再生成する。

このスクリプトは開発用のツールであり、提出物 ircserv のビルドには含まれない。
"""

import os
import re
import subprocess
import sys

TOOLS_DIR = os.path.dirname(os.path.abspath(__file__))
TESTS_DIR = os.path.dirname(TOOLS_DIR)

SUITES = [
    ("ircutil", "util/ircutil", "IrcUtil", "IRC の文字列規則。case mapping、Nickname と "
                           "Channel 名の検証、行長、整形"),
    ("bufferutil", "util/bufferutil", "BufferUtil", "受信バッファからの行切り出し。分割受信の"
                                 "復元と行長境界"),
    ("client", "domain/client", "Client", "1 接続分の状態。登録フロー、所属 Channel、"
                         "送受信バッファ"),
    ("channel", "domain/channel", "Channel", "1 チャンネル分の状態。Member / Operator / "
                           "Invite、Topic、MODE i t k l"),
    ("parser", "util/parser", "Parser", "文字列から Message への変換"),
    ("reply", "util/reply", "Reply", "送信行の組み立て。Prefix、Numeric、CRLF injection の防御"),
]

SECTION = re.compile(r"/\*\s*──\s*(.+?)\s*─+\s*\*/")
ASSERT = re.compile(r"\bASSERT_(EQ|TRUE|FALSE)\s*\(")


def unescape(text):
    return (text.replace("\\r", "\\r").replace("\\n", "\\n")
                .replace('\\"', '"').replace("\\\\", "\\"))


def first_arg(text, start):
    """'(' の直後から、深さ 0 のカンマ手前までを返す。"""
    depth = 0
    i = start
    in_str = False
    esc = False
    while i < len(text):
        c = text[i]
        if in_str:
            if esc:
                esc = False
            elif c == "\\":
                esc = True
            elif c == '"':
                in_str = False
        elif c == '"':
            in_str = True
        elif c in "([{":
            depth += 1
        elif c in ")]}":
            if depth == 0:
                break
            depth -= 1
        elif c == "," and depth == 0:
            break
        i += 1
    return text[start:i]


def arg_to_name(arg):
    """引数から項目名を組み立てる。

    隣接する文字列リテラルは連結する。実行時に決まる部分 (ループ変数など)
    は <値> と表示する。
    """
    out = []
    i = 0
    dynamic = False
    while i < len(arg):
        c = arg[i]
        if c == '"':
            j = i + 1
            buf = []
            while j < len(arg):
                if arg[j] == "\\":
                    buf.append(arg[j:j + 2])
                    j += 2
                    continue
                if arg[j] == '"':
                    break
                buf.append(arg[j])
                j += 1
            if dynamic:
                out.append("<値>")
                dynamic = False
            out.append(unescape("".join(buf)))
            i = j + 1
        elif c in " \t\r\n+":
            i += 1
        else:
            dynamic = True
            i += 1
    if dynamic:
        out.append("<値>")
    return "".join(out)


def parse(path):
    """(セクション名, [項目名...]) の一覧を返す。"""
    with open(path, encoding="utf-8") as f:
        source = f.read()

    marks = []
    for m in SECTION.finditer(source):
        marks.append((m.start(), "section", m.group(1).strip()))
    for m in ASSERT.finditer(source):
        marks.append((m.start(), "assert", arg_to_name(first_arg(source,
                                                                 m.end()))))
    marks.sort()

    sections = []
    current = None
    for _, kind, value in marks:
        if kind == "section":
            current = (value, [])
            sections.append(current)
        else:
            if current is None:
                current = ("その他", [])
                sections.append(current)
            current[1].append(value)
    return sections


def runtime_count(suite):
    res = subprocess.run("./run_tests %s" % suite, cwd=TESTS_DIR, shell=True,
                         stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    m = re.search(r"(\d+) passed, (\d+) failed",
                  res.stdout.decode("utf-8", "replace"))
    return (int(m.group(1)) + int(m.group(2))) if m else None


def main():
    if not os.path.exists(os.path.join(TESTS_DIR, "run_tests")):
        print("run_tests がありません。先に make test を実行してください。")
        return 1

    total_items = 0
    total_runtime = 0

    for suite, subdir, title, summary in SUITES:
        src = os.path.join(TESTS_DIR, subdir, "test_%s.cpp" % suite)
        dst = os.path.join(TESTS_DIR, subdir, "test_%s.txt" % suite)
        sections = parse(src)
        items = sum(len(a) for _, a in sections)
        runtime = runtime_count(suite)
        total_items += items
        total_runtime += runtime or 0

        lines = []
        lines.append("%s テスト項目" % title)
        lines.append("=" * 70)
        lines.append("")
        lines.append(summary)
        lines.append("")
        lines.append("  実行:     cd tests && make test-%s" % suite)
        lines.append("  生成元:   tests/%s/test_%s.cpp" % (subdir, suite))
        lines.append("  再生成:   cd tests && make spec")
        lines.append("")
        lines.append("このファイルは自動生成される。編集しても再生成で消える。")
        lines.append("項目を直すときはテストコードのアサーション名を直すこと。")
        lines.append("")
        lines.append("  項目数        %d (テストコード上)" % items)
        if runtime is not None:
            lines.append("  実行時        %d 件" % runtime)
            if runtime != items:
                lines.append("                ループで同じ項目を複数の値へ"
                             "適用するため、実行時の方が多い")
        lines.append("")
        lines.append("=" * 70)
        lines.append("")

        number = 0
        for name, asserts in sections:
            if not asserts:
                continue
            lines.append("[ %s ]  %d 項目" % (name, len(asserts)))
            lines.append("")
            for a in asserts:
                number += 1
                lines.append("  %3d. %s" % (number, a))
            lines.append("")

        with open(dst, "w", encoding="utf-8") as f:
            f.write("\n".join(lines))
        print("  %-24s %3d 項目 (実行時 %s 件)"
              % (subdir + "/test_" + suite + ".txt", items, runtime))

    print("")
    print("  合計 %d 項目 / 実行時 %d 件" % (total_items, total_runtime))
    return 0


if __name__ == "__main__":
    sys.exit(main())
