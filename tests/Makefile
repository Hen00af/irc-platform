NAME     = unit_tests
CXX      = c++
CXXFLAGS = -Wall -Wextra -Werror -std=c++98

PRD_DIR  = ../prd

SRCS     = unit/test_main.cpp \
           $(PRD_DIR)/domain/Client.cpp \
           $(PRD_DIR)/domain/Channel.cpp

all: $(NAME)

$(NAME): $(SRCS)
	$(CXX) $(CXXFLAGS) -I$(PRD_DIR)/domain -o $@ $(SRCS)

run: $(NAME)
	./$(NAME)

clean:
	rm -f $(NAME)

fclean: clean

re: fclean all

.PHONY: all run clean fclean re
