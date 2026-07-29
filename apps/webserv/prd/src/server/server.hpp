#ifndef SERVER_HPP
#define SERVER_HPP

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <vector>
#include <string>
#include "../persing/persing_conf.hpp"

struct ServerConfig;

class Conf;

/*
    このクラスはconfigからとってきた情報を元に、
    tcp/ip接続を行うクラス。

    HTTPのパースコンポーネント・バッファは持たない。
*/

class Server {
public:
    Server();
    ~Server();
    // サーバーの初期化（socket, bind, listenまで行う）
    void    initServer(int port);
    // Getter（後のselect/poll/kqueueで使用）
    int     getListenFd() const;
    void    boot_server(Conf &conf);
    void    build_connection(const std::vector<ServerConfig> &servers);

private:
    int                 _listen_fd;    // ソケットディスクリプタ
    struct sockaddr_in  _addr;      // サーバーのアドレス情報
    int                 _port;         // ポート番号
    int                 _server_fd;    // サーバのfd
    int                 _client_fd;    // クライアントのfd

};

#endif