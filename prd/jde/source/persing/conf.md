## flowの確認

①_コンストラクタにて、ホワイトリストを作成
        |
        |
②_引数に入っているファイルの存在・開けるかをチェック
        |
        |
③_引数のチェック
        |
        |
④_ホワイトリストを使用して、nginx.confのバリデートを行う
        |
        |
⑤_バリデートしたファイルの中身をメモリ内に格納

## flow毎のconponentの説明

### ①_コンストラクタにて、ホワイトリストを作成

想定しているnginxのconfファイルは以下の通りである
```conf
server {
    listen 8080;
    server_name localhost;

    root ./www;
    index index.html;
    error_page 404 ./errors/404.html;
    client_max_body_size 1000000;

    location / {
        allowed_methods GET POST;
        dir_listing off;
    }

    location /old {
        redir 301 /new;
    }
}
```

これを説明すると、
server：１つのサーバーの設定ブロック。
listen：どのIP・PORTで待ち受けるかの設定。
server_name：どのホストネームで設定するか。
location：URLパス毎の設定
root：ファイル探索のディレクトリ
index：ディレクトリに来たときのデフォルトファイル


```
    _directives.push_back("server");
    _directives.push_back("listen");
    _directives.push_back("server_name");
    _directives.push_back("allowed_methods");
    _directives.push_back("root");
    _directives.push_back("error_page");
    _directives.push_back("index");
    _directives.push_back("client_max_body_size");
    _directives.push_back("location");
    _directives.push_back("dir_listing");
    _directives.push_back("redir");
```


```cpp
void parse_basic(int argc, char **argv) 
```

引数のチェックを行う。
argc != 2
また、ファイル名のprexの.conf確認

### ②_引数に入っているファイルの存在・開けるかをチェック
```cpp
Conf::Conf() {
}
```


コンストラクタにて、ホワイトリスト（必要なデータ群をまとめたリスト）のインスタンスを作成する。
これをパースに使用する

```cpp
~Conf() {

}
```


```cpp
Conf::check_data() {

}
```