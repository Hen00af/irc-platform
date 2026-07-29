# 現在のリクエスト処理フロー

この文書は、現在のルート直下の実装がHTTPリクエストを受け取ってから、
レスポンスを返すまでの流れを示す。

## 全体像

```mermaid
flowchart TD
    Start([webserv起動]) --> Config[Config::parse<br/>tokenize・構文解析・検証]
    Config --> Server[Server生成<br/>listen socketを開く]
    Server --> Poll{{pollでイベント待機}}

    Poll -->|listen fd: POLLIN| Accept[acceptClient<br/>Clientを生成]
    Accept --> Poll

    Poll -->|client fd: POLLIN| Read[readClient<br/>recv結果をinputへ追加]
    Read --> Parse[parseRequest]

    Parse -->|REQUEST_INCOMPLETE| Poll
    Parse -->|REQUEST_BAD| Bad[400 Response]
    Parse -->|REQUEST_TOO_LARGE| Large[413 Response]
    Parse -->|REQUEST_OK| Dispatch[Dispatcher::dispatch]

    Dispatch --> Route[Router::resolve]
    Route --> RouteResult{RouteStatus}

    RouteResult -->|METHOD_NOT_ALLOWED| MethodError[405 Response<br/>Allow header]
    RouteResult -->|REDIRECT| Redirect[301 Response<br/>Location header]
    RouteResult -->|READY| Method{HTTP method}

    Method -->|GET| Static[StaticHandler<br/>静的ファイル・index・autoindex]
    Method -->|POST| Upload[UploadHandler<br/>upload_dirへ保存]
    Method -->|DELETE| Delete[DeleteHandler<br/>対象ファイルを削除]
    RouteResult -->|READY + CGI拡張子| CgiStart[CgiHandler<br/>fork・execve]
    CgiStart --> CgiPoll[stdin/stdout pipeを<br/>pollへ登録]
    CgiPoll --> CgiResponse[CGI出力をResponseへ変換]

    Static --> Response[Response]
    Upload --> Response
    Delete --> Response
    CgiResponse --> Response
    MethodError --> Response
    Redirect --> Response
    Bad --> ParseErrorBody[既定のエラーbodyを生成]
    Large --> ParseErrorBody
    ParseErrorBody --> Response

    Response --> Serialize[Response::serialize<br/>HTTP文字列へ変換]
    Serialize --> WriteMode[pollイベントをPOLLOUTへ変更]
    WriteMode --> Poll

    Poll -->|client fd: POLLOUT| Write[writeClient<br/>send]
    Write --> Complete{全て送信したか}
    Complete -->|no| Poll
    Complete -->|yes| Close[closeClient]
    Close --> Poll
```

## Routerの内部

`Router` は判断だけを行い、ファイル操作やレスポンス生成は行わない。

```mermaid
flowchart TD
    Input[Request + ServerConfig] --> Match[request.pathに対する<br/>最長prefix locationを選択]
    Match --> Methods[locationまたはserverの<br/>methodsを選択]
    Methods --> Allowed{methodは許可済みか}

    Allowed -->|no| NotAllowed[ROUTE_METHOD_NOT_ALLOWED]
    Allowed -->|yes| HasRedirect{redirect設定があるか}
    HasRedirect -->|yes| Redirect[ROUTE_REDIRECT<br/>redirect先を返す]
    HasRedirect -->|no| DiskPath[location prefixを除去し<br/>rootと結合]
    DiskPath --> Ready[ROUTE_READY<br/>diskPathを返す]
```

Routerの出力である`RouteResult`には次の情報が入る。

| フィールド | 内容 |
|---|---|
| `status` | 続行、405、redirectのいずれか |
| `location` | 選択されたlocation。server直下なら`NULL` |
| `methods` | 適用された許可method一覧 |
| `diskPath` | 静的ファイルなどが利用する実パス |
| `redirect` | redirect先 |

## DispatcherとStaticHandler

現在の`Dispatcher`はRouterの判断結果を受けてhandlerを選ぶ。
GET、POST、DELETEのfilesystem処理は各handlerへ分離済み。

```mermaid
flowchart TD
    Start[Dispatcher::dispatch] --> Resolve[Router::resolve]
    Resolve --> Status{RouteStatus}

    Status -->|METHOD_NOT_ALLOWED| R405[errorResponse 405]
    Status -->|REDIRECT| R301[Response 301]
    Status -->|READY| Method{request.method}

    Method -->|DELETE| DeleteHandler[DeleteHandler::handle]
    DeleteHandler --> Exists{対象が通常ファイルか}
    Exists -->|no| DeleteError[404または403]
    Exists -->|yes| Unlink[unlink]
    Unlink --> R204[Response 204]

    Method -->|POST| UploadHandler[UploadHandler::handle]
    UploadHandler --> UploadConfig{upload_dirがあるか}
    UploadConfig -->|no| UploadError[403]
    UploadConfig -->|yes| ValidateName[ファイル名を検証]
    ValidateName --> Save[ofstreamで保存]
    Save --> R201[Response 201]

    Method -->|GET| StaticHandler[StaticHandler::handle]
    StaticHandler --> Stat[stat]
    Stat -->|存在しない| R404[404]
    Stat -->|通常ファイル| ReadFile[ファイルを読む]
    Stat -->|directory| Index{indexが存在するか}
    Index -->|yes| ReadFile
    Index -->|no| Autoindex{autoindex有効か}
    Autoindex -->|no| R403[403]
    Autoindex -->|yes| Listing[directory listing生成]

    ReadFile --> R200[Response 200]
    Listing --> R200
```

## クラス間の責務

```mermaid
flowchart LR
    Server[Server<br/>socket・poll・Client状態] --> Http[HTTP<br/>parse・serialize]
    Http --> Router[Router<br/>純粋な経路判断]
    Router --> Dispatcher[Dispatcher<br/>処理の振り分け]
    Dispatcher --> StaticHandler[StaticHandler]
    Dispatcher --> UploadHandler[UploadHandler]
    Dispatcher --> DeleteHandler[DeleteHandler]
    Server --> CgiHandler[CgiHandler]
    StaticHandler --> FileSystem
    UploadHandler --> FileSystem
    DeleteHandler --> FileSystem
    CgiHandler --> CgiProcess[(CGI process)]
    Dispatcher --> Http
```

| モジュール | 現在の責務 | I/O |
|---|---|---|
| `Config` | nginx風block構文のtokenize、解析、検証、継承 | 設定ファイルを読む |
| `Server` | socket、poll、Client、timeout | socket I/O |
| `Http` | request解析、response直列化 | なし |
| `Router` | location、method、redirect、実パスの決定 | なし |
| `Dispatcher` | routing結果からhandlerまたはredirect/errorを選択 | なし |
| `StaticHandler` | GET、index、autoindex、MIME type | filesystem I/O |
| `UploadHandler` | POST bodyをupload directoryへ保存 | filesystem I/O |
| `DeleteHandler` | DELETE対象の検証と削除 | filesystem I/O |
| `CgiHandler` | CGI環境変数、fork/execve、CGI出力変換 | pipe I/OはServerのpollで管理 |
| `ResponseFactory` | custom error pageと既定error bodyの生成 | error pageを読む |

## 次のリファクタ後の目標

GET、POST、DELETEとCGIは各handlerへ分離済み。

```mermaid
flowchart LR
    Request[Request] --> Router
    Router --> Dispatcher
    Dispatcher --> StaticHandler[StaticHandler<br/>GET・index・autoindex<br/>実装済み]
    Dispatcher --> UploadHandler[UploadHandler<br/>POST<br/>実装済み]
    Dispatcher --> DeleteHandler[DeleteHandler<br/>DELETE<br/>実装済み]
    Dispatcher -. CGI判定はServer .-> CgiHandler[CGIHandler<br/>実装済み]

    StaticHandler --> Response
    UploadHandler --> Response
    DeleteHandler --> Response
    CgiHandler --> Response
```

Dispatcherは最終的に「どのhandlerを呼ぶか」だけを担当し、
各handlerが実際の処理と`Response`生成を担当する形を目指す。

## 現時点の重要な制約

- 1接続につき1リクエストを処理し、送信後に接続を閉じる。
- socketはnon-blockingだが、Dispatcher内のfilesystem処理は同期処理。
- CGI stdin/stdoutはnon-blocking pipeとしてsocketと同じpoll loopで管理する。
- CGIはlocationの`cgi_timeout`（既定30秒）でtimeoutし、504を返す。
- CGI出力は16 MiBを上限とする。
- Hostによるvirtual server選択はまだ行っていない。
- parse errorの400/413はDispatcherを通らず、Serverが生成する。

## 自動テスト

`make test`で次の4層を検証する。

| テスト | 固定する動作 |
|---|---|
| `ConfigTest` | block構文、継承、数値、重複、listen、redirect、CGI設定 |
| `RouterTest` | 最長prefix、実パス、method、redirect |
| `HttpTest` | incomplete、Host、body上限、chunked、serialize |
| `DispatcherTest` | static、autoindex、custom 404、upload、delete、405、redirect |
| `CgiHandlerTest` | 拡張子判定、CGI header/body変換、不正出力の500 |

`DispatcherTest`は`/tmp`にプロセス専用fixtureを作り、終了時に削除する。
プロジェクトの`www/`は変更しない。

`make integration-test`はCGIのGET・POSTと並行リクエストを実サーバーで検証する。
