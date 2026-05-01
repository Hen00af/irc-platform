CONF_FILE=/Users/hattorinarimakoto/Documents/codes/Webserv/prd/nginx.conf


c++ -Wall -Wextra -Werror _conf_test.cpp ../persing/persing.cpp ../persing/conf.cpp ../logging/logging.cpp  -o conf_test && \
./conf_test ../../nginx.conf >../../log/log.conf && \
rm ./conf_test