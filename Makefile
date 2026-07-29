NAME := webserv
CXX := c++
CXXFLAGS := -Wall -Wextra -Werror -std=c++98 -MMD -MP
CPPFLAGS := -Iinclude
SRC := src/main.cpp src/Config.cpp src/Http.cpp src/Router.cpp \
	src/ResponseFactory.cpp src/FileSystem.cpp src/StaticHandler.cpp \
	src/UploadHandler.cpp src/DeleteHandler.cpp src/CgiHandler.cpp \
	src/Dispatcher.cpp src/Server.cpp
OBJ := $(SRC:src/%.cpp=.obj/%.o)
DEP := $(OBJ:.o=.d)
TEST_BINS := .obj/config_test .obj/router_test .obj/http_test \
	.obj/dispatcher_test .obj/cgi_handler_test

all: $(NAME)

$(NAME): $(OBJ)
	$(CXX) $(CXXFLAGS) $(OBJ) -o $@

.obj/%.o: src/%.cpp
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf .obj

fclean: clean
	rm -f $(NAME)

re: fclean all

test: $(TEST_BINS)
	./.obj/config_test
	./.obj/router_test
	./.obj/http_test
	./.obj/dispatcher_test
	./.obj/cgi_handler_test

integration-test: all
	sh tests/CgiIntegrationTest.sh

stress-test: all
	sh tests/StressTest.sh

docker-build:
	docker compose build

docker-shell:
	docker compose run --rm webserv sh

$(TEST_BINS): | .obj

.obj:
	@mkdir -p $@

.obj/router_test: tests/RouterTest.cpp src/Router.cpp src/Config.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $^ -o $@

.obj/config_test: tests/ConfigTest.cpp src/Config.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $^ -o $@

.obj/http_test: tests/HttpTest.cpp src/Http.cpp src/Config.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $^ -o $@

.obj/dispatcher_test: tests/DispatcherTest.cpp src/Config.cpp src/Http.cpp \
	src/Router.cpp src/ResponseFactory.cpp src/FileSystem.cpp \
	src/StaticHandler.cpp src/UploadHandler.cpp src/DeleteHandler.cpp \
	src/Dispatcher.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $^ -o $@

.obj/cgi_handler_test: tests/CgiHandlerTest.cpp src/CgiHandler.cpp \
	src/ResponseFactory.cpp src/Http.cpp src/Config.cpp src/Router.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $^ -o $@

-include $(DEP)

.PHONY: all clean fclean re test integration-test stress-test docker-build \
	docker-shell
