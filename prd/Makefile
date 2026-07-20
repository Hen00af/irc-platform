NAME    = ircserv
CXX     = c++
CXXFLAGS = -Wall -Wextra -Werror -std=c++98

# 移行中:
#   interface/ の Server.cpp / ServerRelations.cpp / ServerDispatch.cpp は
#   設計書 02 §12 に基づく設計準拠の Server 骨格と Dispatcher。
#   ネットワーク担当が main.cpp (エントリポイント) と ServerNetwork.cpp
#   (poll ループ、ネットワーク処理) を実装したら、all を $(NAME) へ戻す。
#   (コメント行末に \ を置くとコメントが次行へ継続し、SRCS の定義ごと
#    無効化されるため、除外中のファイルは行継続なしで記載する)
SRCS    = domain/Client.cpp \
          domain/Channel.cpp \
          util/BufferUtil.cpp \
          util/IrcUtil.cpp \
          util/Parser.cpp \
          util/Reply.cpp \
          interface/Server.cpp \
          interface/ServerRelations.cpp \
          interface/ServerDispatch.cpp \
          handler/ServerAuthCommands.cpp \
          handler/ServerChannelCommands.cpp

HEADERS = domain/Client.hpp \
          domain/Channel.hpp \
          domain/Message.hpp \
          util/BufferUtil.hpp \
          util/IrcUtil.hpp \
          util/Parser.hpp \
          util/Numerics.hpp \
          util/Reply.hpp \
          interface/Server.hpp

OBJS    = $(SRCS:.cpp=.o)

# SRCS から外している間に取り残される .o。clean で消し損ねないよう
# 明示する (.gitignore に隠れて git status にも現れないため)
MIGRATION_OBJS = main.o

# 移行中は main() がないためリンクできない。all はコンパイルまでとする
all: $(OBJS)

$(NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

# ヘッダ依存を張らないと、ヘッダ変更後に .o が再ビルドされず、
# 古い定義のままリンクされる
%.o: %.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(MIGRATION_OBJS)

fclean: clean
	rm -f $(NAME)

re: fclean all

.PHONY: all clean fclean re
