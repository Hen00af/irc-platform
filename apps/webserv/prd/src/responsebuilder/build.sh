#!/bin/sh
set -e

cd "$(dirname "$0")"

c++ -Wall -Wextra -std=c++98 \
    response_test.cpp \
    responseBuilder.cpp \
    responseSerialiezer.cpp \
    ../persing/persing_conf.cpp \
    ../persing/persing_conf_util.cpp \
    -o response_test

./response_test
