#!/bin/sh

CONF_FILE="../../conf/nginx.conf"

c++ -Wall -Wextra -Werror \
  _ConfTest.cpp \
  ../persing/persing.cpp \
  ../persing/conf.cpp \
  ../logging/logging.cpp \
  -o conf_test && \
./conf_test ../../conf/nginx.conf > ../../log/conf.log  && \
./conf_test ../../conf/nginx_random.conf > ../../log/conf2.log || \
./conf_test ../../conf/nginxEmpty.conf >> ../../log/conf3.log 2>&1 && \
rm -f ./conf_test
