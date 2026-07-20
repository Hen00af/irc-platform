# ============================================================
# 担当クラスの単体テスト
#
# 提出物 ircserv とは独立してビルド・実行する。テスト用 main() が
# 通常ビルドへ混入しないよう、prd/Makefile とは分離している。
#
# 構成は prd/ を鏡写しにし、さらにクラスごとへ分ける。
#
#   tests/
#     test_main.cpp        suite の振り分け
#     framework/           自前のアサーション (外部ライブラリ禁止のため)
#     domain/<class>/      test_<class>.cpp  テストコード
#                          test_<class>.txt  項目一覧 (make spec が生成)
#     util/<class>/        同上
#     tools/               開発用スクリプト。提出物には含まれない
#
# ── 実行 ────────────────────────────────
#   make            すべてのテストをビルドして実行
#   make test       同上
#   make test-client / test-channel / test-ircutil / test-bufferutil
#   make test-parser / test-reply
#                   クラスごとの単体テストだけを実行
#   make test-asan  AddressSanitizer / UBSanitizer 付きで実行
#                   (-fsanitize はコンパイラのフラグであり、禁止されている
#                    外部ライブラリのリンクにはあたらない)
#
# ── 検証・出力 ──────────────────────────
#   make mutation   実装へわざとバグを注入し、テストが検出できるか確認する
#                   (テストが「通る」ことと「バグを捕まえる」ことは別のため)
#   make spec       テストコードから test_<class>.txt (項目一覧) を生成する
#   make report     テストを実行し、1 行 1 項目の結果を test_results.txt へ
#                   出力する。make report SUITE=channel で suite を絞れる
#
# ── 後始末 ──────────────────────────────
#   make clean      オブジェクトを削除
#   make fclean     オブジェクト・バイナリ・生成レポートを削除
#                   (test_<class>.txt は資料なので残す)
#   make re         再ビルド
# ============================================================

NAME     = run_tests
CXX      = c++

# -I framework : test_*.cpp から "TestRunner.hpp" で参照する
# -I ..        : "prd/domain/Client.hpp" のようにルート相対で参照する。
#                ディレクトリの深さが変わっても include が壊れない
CXXFLAGS = -Wall -Wextra -Werror -std=c++98 -I framework -I ..

OBJDIR   = obj

# テスト対象 (prd 配下の担当分)
TARGET_SRCS = ../prd/domain/Client.cpp \
              ../prd/domain/Channel.cpp \
              ../prd/util/IrcUtil.cpp \
              ../prd/util/BufferUtil.cpp \
              ../prd/util/Parser.cpp \
              ../prd/util/Reply.cpp \
              ../prd/interface/Server.cpp \
              ../prd/interface/ServerRelations.cpp

TEST_SRCS   = test_main.cpp \
              framework/TestRunner.cpp \
              domain/client/test_client.cpp \
              domain/channel/test_channel.cpp \
              util/ircutil/test_ircutil.cpp \
              util/bufferutil/test_bufferutil.cpp \
              util/parser/test_parser.cpp \
              util/reply/test_reply.cpp \
              interface/dispatch/test_dispatch.cpp

SRCS        = $(TEST_SRCS) $(TARGET_SRCS)
OBJS        = $(addprefix $(OBJDIR)/, $(notdir $(SRCS:.cpp=.o)))

HEADERS     = framework/TestRunner.hpp \
              ../prd/domain/Client.hpp \
              ../prd/domain/Channel.hpp \
              ../prd/domain/Message.hpp \
              ../prd/util/IrcUtil.hpp \
              ../prd/util/BufferUtil.hpp \
              ../prd/util/Parser.hpp \
              ../prd/util/Numerics.hpp \
              ../prd/util/Reply.hpp \
              ../prd/interface/Server.hpp

vpath %.cpp . framework \
             domain/client domain/channel \
             util/ircutil util/bufferutil util/parser util/reply \
             interface/dispatch \
             ../prd/domain ../prd/util ../prd/interface

all: test

$(NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS)

$(OBJDIR)/%.o: %.cpp $(HEADERS) | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(OBJDIR):
	mkdir -p $(OBJDIR)

test: $(NAME)
	./$(NAME)

test-ircutil: $(NAME)
	./$(NAME) ircutil

test-bufferutil: $(NAME)
	./$(NAME) bufferutil

test-client: $(NAME)
	./$(NAME) client

test-channel: $(NAME)
	./$(NAME) channel

test-parser: $(NAME)
	./$(NAME) parser

test-reply: $(NAME)
	./$(NAME) reply

test-dispatch: $(NAME)
	./$(NAME) dispatch
# アサーション失敗後も実行が続く作りのため、範囲外アクセスが
# 潜んでいても静かに通ってしまう。それを検出する
test-asan: fclean
	$(MAKE) test \
	    CXXFLAGS="$(CXXFLAGS) -g -fsanitize=address,undefined \
	              -fno-omit-frame-pointer"

# 単体テストが「通る」ことと「バグを捕まえられる」ことは別である。
# 実装へわざとバグを注入し、テストが落ちることを確認する
mutation: $(NAME)
	python3 tools/mutation_test.py

# テスト項目一覧をテストコードから生成する。手で書き写すと必ず腐るため
spec: $(NAME)
	python3 tools/gen_test_spec.py

# 実行結果を 1 行 1 項目で出力する。端末出力は失敗時に 4 行へ広がるため、
# grep や diff で扱えるよう 1 行へ畳む
SUITE = all
report: $(NAME)
	python3 tools/gen_test_report.py $(SUITE)

clean:
	rm -rf $(OBJDIR)

# test_<class>.txt は消さない。テスト項目一覧として読むためのもので、
# バイナリではなく資料として追跡している
fclean: clean
	rm -f $(NAME) mutation_report.md test_results.txt

re: fclean all

.PHONY: all test test-ircutil test-bufferutil test-client test-channel \
        test-parser test-reply test-asan mutation spec report \
        clean fclean re

