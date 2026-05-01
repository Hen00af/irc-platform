CONF_FILE=/Users/hattorinarimakoto/Documents/codes/Webserv/prd/nginx.conf


c++ -Wall -Wextra -Werror _server_test.cpp ../persing/persing.cpp ../persing/conf.cpp ../logging/logging.cpp ../server/server.cpp  -o server_test && \
./server_test ../../nginx.conf >../../log/log.conf && \
rm ./server_test