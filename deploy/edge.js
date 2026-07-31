// Single-port edge proxy.
//
// Platforms such as Render publish exactly one port per service, so browsers
// cannot reach Webserv (8080) and the gateway (3001) on separate ports there.
// This proxy listens on PORT and routes by path:
//
//   /gateway/...  -> gateway  (prefix stripped; /gateway/ws carries the WebSocket)
//   everything else -> webserv
//
// Local Docker Compose and Fly.io still expose the ports directly and do not
// need this process.

import http from "node:http";
import net from "node:net";
import process from "node:process";

const PORT = Number(process.env.PORT || 10000);
const WEBSERV_HOST = process.env.WEBSERV_HOST || "127.0.0.1";
const WEBSERV_PORT = Number(process.env.WEBSERV_PORT || 8080);
const GATEWAY_HOST = process.env.IRC_GATEWAY_HOST || "127.0.0.1";
const GATEWAY_PORT = Number(process.env.WS_PORT || 3001);
const GATEWAY_PREFIX = "/gateway";

function log(level, event, details = {}) {
  console[level](
    JSON.stringify({
      timestamp: new Date().toISOString(),
      service: "edge",
      event,
      ...details
    })
  );
}

// Returns the upstream and the path to request there, or null for webserv.
function route(url) {
  if (url === GATEWAY_PREFIX || url.startsWith(`${GATEWAY_PREFIX}/`)) {
    return {
      host: GATEWAY_HOST,
      port: GATEWAY_PORT,
      path: url.slice(GATEWAY_PREFIX.length) || "/"
    };
  }
  return { host: WEBSERV_HOST, port: WEBSERV_PORT, path: url };
}

const server = http.createServer((request, response) => {
  const target = route(request.url);
  const upstream = http.request(
    {
      host: target.host,
      port: target.port,
      path: target.path,
      method: request.method,
      headers: request.headers,
      agent: false
    },
    (upstreamResponse) => {
      response.writeHead(
        upstreamResponse.statusCode || 502,
        upstreamResponse.headers
      );
      upstreamResponse.pipe(response);
    }
  );

  upstream.on("error", (error) => {
    log("error", "upstream_error", {
      upstream: `${target.host}:${target.port}`,
      message: error.message
    });
    if (!response.headersSent) {
      response.writeHead(502, { "content-type": "application/json" });
    }
    response.end(JSON.stringify({ error: "bad_gateway" }));
  });

  request.on("error", () => upstream.destroy());
  request.pipe(upstream);
});

// WebSocket upgrades are replayed verbatim onto a raw socket so the gateway
// performs the handshake itself, including its Origin check.
server.on("upgrade", (request, socket, head) => {
  const target = route(request.url);
  socket.on("error", () => socket.destroy());

  const upstream = net.connect(target.port, target.host, () => {
    const lines = [`${request.method} ${target.path} HTTP/1.1`];
    for (let i = 0; i < request.rawHeaders.length; i += 2) {
      lines.push(`${request.rawHeaders[i]}: ${request.rawHeaders[i + 1]}`);
    }
    upstream.write(`${lines.join("\r\n")}\r\n\r\n`);
    if (head && head.length) upstream.write(head);
    upstream.pipe(socket);
    socket.pipe(upstream);
  });

  upstream.on("error", (error) => {
    log("error", "upgrade_upstream_error", {
      upstream: `${target.host}:${target.port}`,
      message: error.message
    });
    socket.destroy();
  });
});

server.listen(PORT, "0.0.0.0", () => {
  log("info", "listening", {
    port: PORT,
    webserv: `${WEBSERV_HOST}:${WEBSERV_PORT}`,
    gateway: `${GATEWAY_HOST}:${GATEWAY_PORT}`
  });
});

function shutdown() {
  server.close(() => process.exit(0));
  setTimeout(() => process.exit(1), 5_000).unref();
}

process.on("SIGINT", shutdown);
process.on("SIGTERM", shutdown);
