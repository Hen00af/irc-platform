## TCP/IPにおける、send関数に渡すオブジェクトを作成する箇所です。

```
raw request
   ↓
RequestParser
   ↓
HttpRequest
   ↓
Router / RequestHandler
   ↓
Response
   ↓
ResponseBuilder
   ↓
send() 
```

HttpResponse handle_request(const HttpRequest& req, const Conf& cond);


