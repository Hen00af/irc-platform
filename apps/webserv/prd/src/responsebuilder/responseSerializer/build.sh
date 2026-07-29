#!/bin/sh
set -e

cd "$(dirname "$0")"

c++ -Wall -Wextra -std=c++98 \
    main.cpp \
    responseSerializer.cpp \
    ../responsebuilder/responseBuilder.cpp \
    ../../../mock/conf.cpp \
    ../../persing/persing_conf.cpp \
    ../../persing/persing_conf_util.cpp \
    -o serializer_mock

echo "built: ./serializer_mock"
