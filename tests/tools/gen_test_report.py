#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""単体テストを実行し、1 行 1 項目の結果ファイルを出力する。

  cd tests && make report        全 suite
  cd tests && make report SUITE=channel

出力: tests/test_results.txt

run_tests の端末出力は失敗時に 4 行へ広がる (期待値・実際の値・位置)。
このスクリプトはそれを 1 行へ畳んで、grep や diff で扱える形にする。

失敗が 1 件でもあれば終了コード 1 を返す。

このスクリプトは開発用のツールであり、提出物 ircserv のビルドには含まれない。
"""

import os
import re
import subprocess
import sys

TOOLS_DIR = os.path.dirname(os.path.abspath(__file__))
TESTS_DIR = os.path.dirname(TOOLS_DIR)
REPORT = os.path.join(TESTS_DIR, "test_results.txt")

SUITE_LINE = re.compile(r"^=== (.+) ===$")
OK_LINE = re.compile(r"^\s+\[ok\]\s+(.*)$")
FAIL_LINE = re.compile(r"^\s+\[FAIL\]\s+(.*)$")
DETAIL = re.compile(r"^\s+(expected|actual|at):?\s*(.*)$")
SUMMARY = re.compile(r"^(\d+) passed, (\d+) failed$")


def main():
    suite = sys.argv[1] if len(sys.argv) > 1 else "all"

    res = subprocess.run("./run_tests %s" % suite, cwd=TESTS_DIR, shell=True,
                         stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    raw = res.stdout.decode("utf-8", "replace").splitlines()

    rows = []          # (suite, 結果, 項目名, 補足)
    current = "?"
    passed = failed = 0
    i = 0
    while i < len(raw):
        line = raw[i]

        m = SUITE_LINE.match(line)
        if m:
            current = m.group(1)
            i += 1
            continue

        m = OK_LINE.match(line)
        if m:
            rows.append((current, "ok", m.group(1).rstrip(), ""))
            i += 1
            continue

        m = FAIL_LINE.match(line)
        if m:
            name = m.group(1).rstrip()
            detail = []
            i += 1
            while i < len(raw):
                d = DETAIL.match(raw[i])
                if not d:
                    break
                key, value = d.group(1), d.group(2).rstrip()
                # 期待値・実際の値が複数行に渡る場合 (改行を含む文字列) は
                # 1 行へ畳む
                while (i + 1 < len(raw) and not DETAIL.match(raw[i + 1])
                       and not OK_LINE.match(raw[i + 1])
                       and not FAIL_LINE.match(raw[i + 1])
                       and not SUITE_LINE.match(raw[i + 1])
                       and raw[i + 1].strip()
                       and not raw[i + 1].startswith("---")
                       and not SUMMARY.match(raw[i + 1].strip())):
                    i += 1
                    value += "\\n" + raw[i].strip()
                detail.append("%s: %s" % (key, value))
                i += 1
            rows.append((current, "FAIL", name, " / ".join(detail)))
            continue

        m = SUMMARY.match(line.strip())
        if m:
            passed, failed = int(m.group(1)), int(m.group(2))

        i += 1

    if not rows:
        print("テスト結果を読み取れませんでした。make test を確認してください。")
        print("\n".join(raw[-20:]))
        return 1

    width = max(len(name) for _, _, name, _ in rows)
    width = min(width, 64)

    out = []
    out.append("ft_irc 単体テスト結果  (1 行 1 項目)")
    out.append("=" * 78)
    out.append("")
    out.append("  実行:   cd tests && make report")
    out.append("  対象:   %s" % ("全 suite" if suite == "all" else suite))
    out.append("")
    out.append("  合計 %d 件   成功 %d   失敗 %d" % (passed + failed, passed,
                                                     failed))
    out.append("")
    out.append("このファイルは実行のたびに上書きされる。")
    out.append("")
    out.append("=" * 78)
    out.append("")

    last = None
    for suite_name, verdict, name, detail in rows:
        if suite_name != last:
            out.append("")
            out.append("--- %s ---" % suite_name)
            last = suite_name
        mark = "ok  " if verdict == "ok" else "FAIL"
        if detail:
            out.append("[%s] %-*s  %s" % (mark, width, name, detail))
        else:
            out.append("[%s] %s" % (mark, name))
    out.append("")

    if failed:
        out.append("=" * 78)
        out.append("")
        out.append("失敗 %d 件:" % failed)
        out.append("")
        for suite_name, verdict, name, detail in rows:
            if verdict == "FAIL":
                out.append("  [%s] %s" % (suite_name, name))
                if detail:
                    out.append("        %s" % detail)
        out.append("")

    with open(REPORT, "w", encoding="utf-8") as f:
        f.write("\n".join(out))

    print("  合計 %d 件   成功 %d   失敗 %d" % (passed + failed, passed,
                                                failed))
    print("  出力: %s" % REPORT)
    if failed:
        print("")
        for suite_name, verdict, name, _ in rows:
            if verdict == "FAIL":
                print("  [FAIL] %s: %s" % (suite_name, name))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
