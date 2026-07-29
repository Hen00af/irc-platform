import net from "node:net";
import { EventEmitter } from "node:events";

const NICK_RE = /^[A-Za-z][A-Za-z0-9_\-[\]{}\\`^]{0,8}$/;
const CHANNEL_RE = /^#[A-Za-z0-9_\-]{1,49}$/;

export function validNickname(value) {
  return typeof value === "string" && NICK_RE.test(value);
}

export function validChannel(value) {
  return typeof value === "string" && CHANNEL_RE.test(value);
}

export function parseIrcLine(line) {
  let rest = line;
  let prefix = "";

  if (rest.startsWith(":")) {
    const end = rest.indexOf(" ");
    if (end === -1) return null;
    prefix = rest.slice(1, end);
    rest = rest.slice(end + 1);
  }

  let trailing = "";
  const trailingIndex = rest.indexOf(" :");
  if (trailingIndex !== -1) {
    trailing = rest.slice(trailingIndex + 2);
    rest = rest.slice(0, trailingIndex);
  }

  const parts = rest.split(/\s+/).filter(Boolean);
  if (parts.length === 0) return null;

  return {
    prefix,
    command: parts[0].toUpperCase(),
    params: parts.slice(1),
    trailing
  };
}

function nickFromPrefix(prefix) {
  const end = prefix.indexOf("!");
  return end === -1 ? prefix : prefix.slice(0, end);
}

export class IrcConnection extends EventEmitter {
  constructor(options) {
    super();
    this.host = options.host;
    this.port = options.port;
    this.password = options.password;
    this.nickname = options.nickname;
    this.username = options.username || options.nickname.toLowerCase();
    this.socket = null;
    this.buffer = "";
    this.closed = false;
  }

  connect() {
    if (this.socket) return;

    this.socket = net.createConnection(
      { host: this.host, port: this.port },
      () => {
        if (this.password) this.send("PASS", [this.password]);
        this.send("NICK", [this.nickname]);
        this.send("USER", [this.username, "0", "*"], this.nickname);
      }
    );
    this.socket.setEncoding("utf8");
    this.socket.setKeepAlive(true, 30_000);
    this.socket.on("data", (chunk) => this.#onData(chunk));
    this.socket.on("error", (error) => this.emit("error", error));
    this.socket.on("close", () => {
      this.closed = true;
      this.emit("close");
    });
  }

  #onData(chunk) {
    this.buffer += chunk;
    if (this.buffer.length > 128 * 1024) {
      this.destroy(new Error("IRC receive buffer exceeded its limit"));
      return;
    }

    const lines = this.buffer.split(/\r?\n/);
    this.buffer = lines.pop() || "";
    for (const line of lines) {
      if (!line) continue;
      const message = parseIrcLine(line);
      if (!message) continue;

      if (message.command === "PING") {
        this.send("PONG", message.params, message.trailing);
      }

      this.emit("message", {
        ...message,
        nick: nickFromPrefix(message.prefix),
        raw: line
      });
    }
  }

  send(command, params = [], trailing = "") {
    if (!this.socket || this.socket.destroyed) return;
    const cleanCommand = String(command).replace(/[\r\n ]/g, "").toUpperCase();
    const cleanParams = params.map((value) =>
      String(value).replace(/[\r\n ]/g, "")
    );
    const cleanTrailing = String(trailing).replace(/[\r\n]/g, " ");
    let line = [cleanCommand, ...cleanParams].join(" ");
    if (cleanTrailing) line += ` :${cleanTrailing}`;
    this.socket.write(`${line}\r\n`);
  }

  join(channel) {
    if (!validChannel(channel)) throw new Error("Invalid channel name");
    this.send("JOIN", [channel]);
  }

  part(channel) {
    if (!validChannel(channel)) throw new Error("Invalid channel name");
    this.send("PART", [channel]);
  }

  privmsg(target, text) {
    if (!validChannel(target) && !validNickname(target)) {
      throw new Error("Invalid message target");
    }
    const normalized = String(text).trim();
    if (!normalized || Buffer.byteLength(normalized, "utf8") > 420) {
      throw new Error("Messages must contain 1–420 bytes");
    }
    this.send("PRIVMSG", [target], normalized);
  }

  changeNick(nickname) {
    if (!validNickname(nickname)) throw new Error("Invalid nickname");
    this.send("NICK", [nickname]);
    this.nickname = nickname;
  }

  quit(reason = "Leaving irc-platform") {
    if (!this.socket || this.socket.destroyed) return;
    this.send("QUIT", [], reason);
    this.socket.end();
  }

  destroy(error) {
    if (error) this.emit("error", error);
    if (this.socket && !this.socket.destroyed) this.socket.destroy();
  }
}
