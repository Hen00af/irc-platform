SRCS_DIR = source
OBJS_DIR = .objs

SRCS	=	$(shell find $(SRCS_DIR) -name "*.cpp")
OBJS	=	$(patsubst $(SRCS_DIR)%.cpp, $(OBJS_DIR)%.o, $(SRCS))

CXX = c++
CXXFLAGS = c++ -g -O0 


NAME = webserv

all: $(NAME)
CXXFLAGS
