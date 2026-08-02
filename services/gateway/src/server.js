import http from "node:http";
import process from "node:process";
import { WebSocketServer, WebSocket } from "ws";
import {
  IrcConnection,
  validChannel,
  validNickname
} from "./irc.js";
import { ChannelHistory } from "./history.js";

const WS_PORT = Number(process.env.WS_PORT || 3001);
const IRC_HOST = process.env.IRC_HOST || "127.0.0.1";
const IRC_PORT = Number(process.env.IRC_PORT || 6667);
const IRC_PASSWORD = process.env.IRC_PASSWORD || "";
const HISTORY_LIMIT = Number(process.env.HISTORY_LIMIT || 100);
const HISTORY_REPLAY_LIMIT = Number(process.env.HISTORY_REPLAY_LIMIT || 50);
const HISTORY_CHANNEL_LIMIT = Number(process.env.HISTORY_CHANNEL_LIMIT || 200);
// Every browser socket opens a TCP connection to ft_irc, so these caps bound
// the file descriptors and memory one client can take from the whole service.
// The per-message limit below only ever bounded a single connection.
const MAX_CONNECTIONS = Number(process.env.MAX_CONNECTIONS || 200);
const MAX_CONNECTIONS_PER_IP = Number(process.env.MAX_CONNECTIONS_PER_IP || 5);
const IDLE_TIMEOUT_MS = Number(process.env.IDLE_TIMEOUT_MS || 120_000);
const ALLOWED_ORIGINS = new Set(
  (process.env.ALLOWED_ORIGINS || "")
    .split(",")
    .map((value) => value.trim())
    .filter(Boolean)
);
const history = new ChannelHistory({
  limit: HISTORY_LIMIT,
  replayLimit: HISTORY_REPLAY_LIMIT,
  channelLimit: HISTORY_CHANNEL_LIMIT
});

// Counted per source address. Render terminates TLS ahead of this process, so
// the peer address is the proxy for every client and only the forwarded header
// tells them apart.
const connectionsPerIp = new Map();

function clientAddress(request) {
  const forwarded = request.headers["x-forwarded-for"];
  if (typeof forwarded === "string" && forwarded.length) {
    return forwarded.split(",")[0].trim();
  }
  return request.socket.remoteAddress || "unknown";
}

function log(level, event, details = {}) {
  console[level](
    JSON.stringify({
      timestamp: new Date().toISOString(),
      service: "gateway",
      event,
      ...details
    })
  );
}

function json(ws, payload) {
  if (ws.readyState === WebSocket.OPEN) ws.send(JSON.stringify(payload));
}

function publicEvent(message) {
  const base = {
    command: message.command,
    nick: message.nick,
    params: message.params,
    text: message.trailing
  };

  switch (message.command) {
    case "001":
      return { type: "ready", ...base };
    case "PRIVMSG":
      return { type: "message", target: message.params[0], ...base };
    case "NOTICE":
      return { type: "notice", target: message.params[0], ...base };
    case "353":
      return {
        type: "names",
        channel: message.params[2],
        names: message.trailing
          .split(/\s+/)
          .filter(Boolean)
          .map((name) => name.replace(/^[@+]/, "")),
        ...base
      };
    case "366":
      return { type: "names_end", channel: message.params[1], ...base };
    case "422":
      return { type: "server_info", ...base };
    case "JOIN":
      return {
        type: "join",
        channel: message.params[0] || message.trailing,
        ...base
      };
    case "PART":
      return { type: "part", channel: message.params[0], ...base };
    case "KICK":
      return {
        type: "kick",
        channel: message.params[0],
        target: message.params[1],
        ...base
      };
    case "NICK":
      return {
        type: "nick",
        nickname: message.params[0] || message.trailing,
        ...base
      };
    default:
      if (/^[45]\d\d$/.test(message.command)) {
        return { type: "error", code: message.command, ...base };
      }
      return { type: "event", ...base };
  }
}

function parseClientMessage(raw) {
  if (raw.length > 8 * 1024) throw new Error("Message payload is too large");
  const value = JSON.parse(raw.toString("utf8"));
  if (!value || typeof value.type !== "string") {
    throw new Error("Message type is required");
  }
  return value;
}

const server = http.createServer((request, response) => {
  if (request.url === "/health") {
    response.writeHead(200, { "content-type": "application/json" });
    response.end(JSON.stringify({ ok: true, irc: `${IRC_HOST}:${IRC_PORT}` }));
    return;
  }
  response.writeHead(404, { "content-type": "application/json" });
  response.end(JSON.stringify({ error: "not_found" }));
});

// Rejected before the handshake, so a flood never reaches the point where a
// socket is tracked or an IRC connection is opened for it.
function verifyClient({ origin, req }, callback) {
  if (ALLOWED_ORIGINS.size !== 0 && !ALLOWED_ORIGINS.has(origin)) {
    return callback(false, 403, "Origin not allowed");
  }
  const address = clientAddress(req);
  if (
    wss.clients.size >= MAX_CONNECTIONS ||
    (connectionsPerIp.get(address) || 0) >= MAX_CONNECTIONS_PER_IP
  ) {
    log("warn", "connection_rejected", {
      address,
      total: wss.clients.size,
      fromAddress: connectionsPerIp.get(address) || 0
    });
    return callback(false, 429, "Too many connections");
  }
  return callback(true);
}

const wss = new WebSocketServer({
  server,
  maxPayload: 8 * 1024,
  verifyClient
});

wss.on("connection", (ws, request) => {
  let irc = null;
  let joinedChannel = null;
  const recentActions = [];

  const address = clientAddress(request);
  connectionsPerIp.set(address, (connectionsPerIp.get(address) || 0) + 1);

  // A socket that opens and then says nothing still holds a slot, so idle ones
  // are dropped. Any traffic — including the pong the browser sends back
  // automatically — counts as alive.
  let alive = true;
  const heartbeat = setInterval(() => {
    if (!alive) {
      ws.terminate();
      return;
    }
    alive = false;
    ws.ping();
  }, IDLE_TIMEOUT_MS);
  ws.on("pong", () => {
    alive = true;
  });

  json(ws, { type: "hello", message: "Gateway ready" });

  ws.on("message", (raw) => {
    alive = true;
    try {
      const now = Date.now();
      while (recentActions[0] < now - 5_000) recentActions.shift();
      if (recentActions.length >= 12) {
        throw new Error("Slow down and try again");
      }
      recentActions.push(now);

      const message = parseClientMessage(raw);

      if (message.type === "connect") {
        if (irc) throw new Error("Already connected");
        if (!validNickname(message.nickname)) {
          throw new Error("Nickname must start with a letter");
        }
        if (message.channel && !validChannel(message.channel)) {
          throw new Error("Channels must begin with #");
        }

        joinedChannel = message.channel || "#lobby";
        irc = new IrcConnection({
          host: IRC_HOST,
          port: IRC_PORT,
          password: IRC_PASSWORD,
          nickname: message.nickname,
          username: message.username
        });
        irc.on("message", (event) => {
          const payload = publicEvent(event);
          if (payload.type === "error") {
            log("warn", "irc_error", {
              code: payload.code,
              command: payload.params[1] || "",
              message: payload.text
            });
          }
          json(ws, payload);
          if (event.command === "001") {
            irc.join(joinedChannel);
          }
          if (
            event.command === "JOIN" &&
            event.nick === irc.nickname &&
            (event.params[0] || event.trailing) === joinedChannel
          ) {
            json(ws, {
              type: "history",
              channel: joinedChannel,
              messages: history.recent(joinedChannel),
              persistent: false
            });
          }
        });
        irc.on("error", (error) => {
          log("error", "irc_connection_error", { message: error.message });
          json(ws, { type: "error", message: error.message });
        });
        irc.on("close", () => {
          log("info", "irc_disconnected");
          json(ws, { type: "disconnected" });
        });
        irc.connect();
        json(ws, { type: "connecting" });
        return;
      }

      if (!irc) throw new Error("Connect before sending commands");

      switch (message.type) {
        case "message": {
          const target = message.target || joinedChannel;
          irc.privmsg(target, message.text);
          const text = String(message.text).trim();
          // Only the channel this connection actually joined is recorded.
          // ft_irc rejects a PRIVMSG to a channel the sender is not in, but
          // that rejection arrives asynchronously, so recording any target the
          // client names would let anyone plant messages in the replay of a
          // channel they never entered.
          if (target === joinedChannel) {
            history.add(target, { nick: irc.nickname, text });
          }
          json(ws, {
            type: "message",
            target,
            nick: irc.nickname,
            text,
            own: true
          });
          break;
        }
        case "join":
          irc.join(message.channel);
          joinedChannel = message.channel;
          break;
        case "part":
          irc.part(message.channel || joinedChannel);
          break;
        case "nick":
          irc.changeNick(message.nickname);
          break;
        default:
          throw new Error("Unsupported action");
      }
    } catch (error) {
      log("warn", "client_action_rejected", { message: error.message });
      json(ws, { type: "error", message: error.message });
    }
  });

  ws.on("close", () => {
    clearInterval(heartbeat);
    const remaining = (connectionsPerIp.get(address) || 1) - 1;
    if (remaining > 0) connectionsPerIp.set(address, remaining);
    else connectionsPerIp.delete(address);
    if (irc) irc.quit("Browser disconnected");
  });
});

server.listen(WS_PORT, "0.0.0.0", () => {
  console.log(`gateway: listening on 0.0.0.0:${WS_PORT}`);
});

function shutdown() {
  for (const client of wss.clients) client.close(1001, "Server stopping");
  wss.close(() => server.close(() => process.exit(0)));
  setTimeout(() => process.exit(1), 5_000).unref();
}

process.on("SIGINT", shutdown);
process.on("SIGTERM", shutdown);
