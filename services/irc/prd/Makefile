NAME    = ircserv
CXX     = c++
CXXFLAGS = -Wall -Wextra -Werror -std=c++98

SRCS    = main.cpp \
          domain/Client.cpp \
          domain/Channel.cpp \
          util/BufferUtil.cpp \
          util/IrcUtil.cpp \
          util/Parser.cpp \
          util/Reply.cpp \
          interface/Server.cpp \
          interface/ServerRelations.cpp \
          interface/ServerDispatch.cpp \
          interface/ServerNetwork.cpp \
          handler/ServerAuthCommands.cpp \
          handler/ServerChannelCommands.cpp \
          handler/ServerMessageCommands.cpp \
          handler/ServerMode.cpp

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

all: $(NAME)

$(NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

# ヘッダ依存を張らないと、ヘッダ変更後に .o が再ビルドされず、
# 古い定義のままリンクされる
%.o: %.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS)

fclean: clean
	rm -f $(NAME)

re: fclean all

.PHONY: all clean fclean re
