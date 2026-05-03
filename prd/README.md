# Webserv

42Tokyoのカリキュラム向けに作成した、C++98製のNginx風のWebサーバプロジェクトです。


# 概要
このサーバはシンプルなステートマシンをベースにしています。
各サーバは locationのリストを持ちます。　各locationはdirectiveのリストを持ちます。魯クエストを受け取ると、サーバはリクエストURIがどの location に一致するかを探します。
一致する location　が見つかると、その location　に定義された directive をリクエストに通用します。

