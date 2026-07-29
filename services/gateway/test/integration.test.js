import test from "node:test";
import assert from "node:assert/strict";
import { spawn } from "node:child_process";
import { once } from "node:events";
import net from "node:net";
import { WebSocket } from "ws";

const ROOT = new URL("../../..", import.meta.url).pathname;
const IRC_BINARY = `${ROOT}/services/irc/prd/ircserv`;
const GATEWAY = `${ROOT}/services/gateway/src/server.js`;
const IRC_PORT = 17667;
const WS_PORT = 13001;
const PASSWORD = "integration-secret";

async function waitForHealth(url) {
  for (let attempt = 0; attempt < 50; attempt += 1) {
    try {
      const response = await fetch(url);
      if (response.ok) return;
    } catch {
      // The service is still starting.
    }
    await new Promise((resolve) => setTimeout(resolve, 100));
  }
  throw new Error(`Service did not become healthy: ${url}`);
}

async function waitForPort(port) {
  for (let attempt = 0; attempt < 50; attempt += 1) {
    const connected = await new Promise((resolve) => {
      const socket = net.createConnection({ host: "127.0.0.1", port });
      socket.once("connect", () => {
        socket.destroy();
        resolve(true);
      });
      socket.once("error", () => resolve(false));
    });
    if (connected) return;
    await new Promise((resolve) => setTimeout(resolve, 100));
  }
  throw new Error(`Service did not open port ${port}`);
}

function openClient(nickname) {
  return new Promise((resolve, reject) => {
    const ws = new WebSocket(`ws://127.0.0.1:${WS_PORT}`);
    const messages = [];
    const timeout = setTimeout(() => reject(new Error("Client timed out")), 5000);

    ws.on("message", (raw) => {
      const message = JSON.parse(raw.toString());
      messages.push(message);
      if (message.type === "hello") {
        ws.send(
          JSON.stringify({ type: "connect", nickname, channel: "#lobby" })
        );
      }
      if (message.type === "ready") {
        clearTimeout(timeout);
        resolve({ ws, messages });
      }
    });
    ws.on("error", reject);
  });
}

function waitForMessage(client, predicate) {
  const existing = client.messages.find(predicate);
  if (existing) return Promise.resolve(existing);

  return new Promise((resolve, reject) => {
    const timeout = setTimeout(() => reject(new Error("Message timed out")), 5000);
    const listener = (raw) => {
      const message = JSON.parse(raw.toString());
      client.messages.push(message);
      if (predicate(message)) {
        clearTimeout(timeout);
        client.ws.off("message", listener);
        resolve(message);
      }
    };
    client.ws.on("message", listener);
  });
}

test("bridges browser messages through the real IRC server", async (t) => {
  const irc = spawn(IRC_BINARY, [String(IRC_PORT)], {
    env: { ...process.env, IRC_PASSWORD: PASSWORD },
    stdio: "ignore"
  });
  const gateway = spawn(process.execPath, [GATEWAY], {
    env: {
      ...process.env,
      IRC_PASSWORD: PASSWORD,
      IRC_HOST: "127.0.0.1",
      IRC_PORT: String(IRC_PORT),
      WS_PORT: String(WS_PORT)
    },
    stdio: "ignore"
  });

  t.after(() => {
    irc.kill("SIGTERM");
    gateway.kill("SIGTERM");
  });

  await waitForPort(IRC_PORT);
  await waitForHealth(`http://127.0.0.1:${WS_PORT}/health`);
  const alice = await openClient("Alice");
  const aliceNames = await waitForMessage(
    alice,
    (message) =>
      message.type === "names" &&
      message.channel === "#lobby" &&
      message.names.includes("Alice")
  );
  assert.deepEqual(aliceNames.names, ["Alice"]);
  assert.equal(
    alice.messages.some(
      (message) =>
        message.type === "error" &&
        (message.code === "421" || message.code === "422")
    ),
    false
  );
  const bob = await openClient("Bob");
  t.after(() => {
    alice.ws.close();
    bob.ws.close();
  });

  await waitForMessage(alice, (message) =>
    message.type === "join" && message.nick === "Bob"
  );
  alice.ws.send(
    JSON.stringify({ type: "message", target: "#lobby", text: "hello Bob" })
  );
  const delivered = await waitForMessage(
    bob,
    (message) => message.type === "message" && message.text === "hello Bob"
  );

  assert.equal(delivered.nick, "Alice");
  assert.equal(delivered.target, "#lobby");

  const charlie = await openClient("Charlie");
  t.after(() => charlie.ws.close());
  const replay = await waitForMessage(
    charlie,
    (message) =>
      message.type === "history" &&
      message.messages.some(({ text }) => text === "hello Bob")
  );
  assert.equal(replay.persistent, false);
  assert.equal(replay.messages.at(-1).nick, "Alice");

  gateway.kill("SIGTERM");
  await once(gateway, "exit");
});
