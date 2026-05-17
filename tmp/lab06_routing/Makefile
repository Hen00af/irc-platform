NAME      = lab06

CXX       = c++
CXXFLAGS  = -Wall -Wextra -Werror -std=c++98 -MMD -MP

SRC_DIR   = .
OBJ_DIR   = obj

SRCS      = main.cpp routing.cpp
OBJS      = $(SRCS:%.cpp=$(OBJ_DIR)/%.o)
DEPS      = $(OBJS:.o=.d)

all: $(NAME)

$(NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(OBJ_DIR):
	mkdir -p $@

clean:
	rm -rf $(OBJ_DIR)

fclean: clean
	rm -f $(NAME)

re: fclean all

-include $(DEPS)

.PHONY: all clean fclean re
