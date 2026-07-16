## ft_irc

## ircとは
IRC(Internet Relay Chat)とは、サーバを通じてクライアントとクライアントが会話をする枠組みの総称


## 実装するべき機能

- TCP/ICを利用してプロセス間で通信を行う。
- 実行コマンドの形式は ./ircserv <port> <password>

コマンド解説
./ircserv -実行ファイル
<port> -IRCサーバが着信するIRC接続を待機するポート番号
<password> -接続用のパスワード。サーバへの接続を試みる全てのIRCクライアントで必要

*注意*
    IRCクライアント・サーバー間の通信は実装してはならない。
    あくまでも実行ファイルを通じてIRCサーバーに対して接続を行う機構が必要。



- 出典
https://ja.wikipedia.org/wiki/Internet_Relay_Chat - IRCについて書かれたWikiの記事

