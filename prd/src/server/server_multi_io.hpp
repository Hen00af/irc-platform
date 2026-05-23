#ifndef SERVER_MULTI_IO_HPP
#define SERVER_MULTI_IO_HPP

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <vector>
#include <string>
#include <iostream>
#include <cstring>
#include <cerrno>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <poll.h>
#include <map>
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

    //　クライアントごとに送受信のバッファを持つように実装
    struct ClientState {
        std::string read_buffer;
        std::string write_buffer;
    };

private:
    int                 _listen_fd;    // ソケットディスクリプタ
    struct sockaddr_in  _addr;      // サーバーのアドレス情報
    int                 _port;         // ポート番号
    int                 _server_fd;    // サーバのfd
    int                 _client_fd;    // クライアントのfd

};

#endif
