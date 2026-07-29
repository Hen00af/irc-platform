#!/bin/sh
# 本物 nginx で CGI を動かすための FastCGI ブリッジ (fcgiwrap) を起動する。
# nginx 公式イメージは起動前に /docker-entrypoint.d/*.sh を実行するため、
# ここで spawn-fcgi により fcgiwrap をデーモン化しておく。
set -e

spawn-fcgi -s /var/run/fcgiwrap.socket -M 766 -u www-data -g www-data /usr/sbin/fcgiwrap
