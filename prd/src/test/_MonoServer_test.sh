CONF_FILE=/Users/hattorinarimakoto/Documents/codes/Webserv/prd/conf/nginx.conf


c++ -Wall -Wextra -Werror _MonoServer_Test.cpp ../persing/persing.cpp ../persing/persing_conf.cpp ../logging/logging.cpp ../server/server.cpp  -o server_test && \
./server_test ../../conf/nginx.conf >../../log/server_.log && \
rm ./server_test


# nc 127.0.0.1 4040 
#　このコマンドでサーバーに接続