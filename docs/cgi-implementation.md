# CGI実装の読み方

## 入口

HTTP requestのparse完了後、`Server::startCgi()`がRouterの結果を確認する。

```text
Request
  → Router::resolve
  → document root内のscriptか検証
  → CgiHandler::matches
  → CgiHandler::start
```

`CgiHandler::matches()`はscriptの拡張子をlocationの`cgiHandlers` mapと
比較する。

```text
.py  → /usr/bin/python3
.php → /usr/bin/php-cgi
```

設定例：

```nginx
location /cgi-bin {
    root www/cgi-bin;
    allow_methods GET POST;
    cgi_handler .py /usr/bin/python3;
    cgi_handler .php /usr/bin/php-cgi;
    cgi_timeout 3;
}
```

従来の`cgi_extension`と`cgi_path`も1組だけ設定する形式として利用できる。

## process生成

`CgiHandler::start()`はstdin用とstdout用に2本のpipeを作り、`fork()`する。

```text
webserv ──request body──> CGI stdin
webserv <──CGI response── CGI stdout
```

child processでは次を行う。

1. pipeを標準入力・標準出力へ`dup2`
2. 不要なfile descriptorを閉じる
3. CPU、memory、出力file、process数へ`setrlimit`
4. scriptのdirectoryへ`chdir`
5. 拡張子に対応するinterpreterを`execve`

parent processはpipeをnon-blockingにして`poll()`へ登録する。
`waitpid()`で同期的に待たないため、CGI実行中も別clientを処理できる。

## 状態遷移

```text
CLIENT_READING
  → CLIENT_CGI_WRITING
  → CLIENT_CGI_READING
  → CLIENT_WRITING
  → close
```

- `CLIENT_CGI_WRITING`: request bodyをCGI stdinへpartial write
- `CLIENT_CGI_READING`: CGI stdoutをpartial read
- `CLIENT_WRITING`: CGI出力から作ったHTTP responseをclientへ送る

CGI stdoutがEOFになった後、`waitpid(..., WNOHANG)`でchildを回収する。

## CGI環境変数

requestとserver情報から主に次を生成する。

```text
REQUEST_METHOD
REQUEST_URI
QUERY_STRING
CONTENT_LENGTH
CONTENT_TYPE
SCRIPT_NAME
SCRIPT_FILENAME
DOCUMENT_ROOT
PATH_INFO
PATH_TRANSLATED
SERVER_NAME
SERVER_PORT
REMOTE_ADDR
REMOTE_PORT
HTTP_*
```

request headerは`HTTP_HOST`や`HTTP_COOKIE`の形式でCGIへ渡す。

## CGI response

CGIのstdoutは次の形式を期待する。

```text
Status: 201 Created
Content-Type: text/plain
X-Custom: value

body
```

`CgiHandler::makeResponse()`がheaderとbodyを分離し、`Response`へ変換する。
CGIの`Content-Length`と`Connection`は採用せず、Webserv側で再生成する。

異常終了、不正なCGI header、出力上限超過は500、timeoutは504になる。

## Cookieとsession

Cookieは特別なserver内保存ではなく、通常のHTTP headerとしてCGIとclientの間を
往復する。

```text
CGI
  → Set-Cookie: webserv_session=<id>
  → client
  → Cookie: webserv_session=<id>
  → HTTP_COOKIE環境変数
  → CGI
```

`session.py`は初回にsession IDを発行し、2回目以降はclientが返したCookieから
同じIDを復元する最小例である。server-side session data storeは実装していない。

## テスト

```sh
make test
make integration-test
make stress-test
```

統合テストではPython CGI、PHP-CGI、GET/POST、custom status、redirect、
Cookie/session、異常終了、並行実行、timeoutを確認する。
