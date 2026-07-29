import test from "node:test";
import assert from "node:assert/strict";
import {
  parseIrcLine,
  validChannel,
  validNickname
} from "../src/irc.js";

test("parses prefixed IRC messages", () => {
  assert.deepEqual(
    parseIrcLine(":alice!a@127.0.0.1 PRIVMSG #lobby :hello there"),
    {
      prefix: "alice!a@127.0.0.1",
      command: "PRIVMSG",
      params: ["#lobby"],
      trailing: "hello there"
    }
  );
});

test("parses server numerics", () => {
  assert.deepEqual(parseIrcLine(":irc.local 001 alice :Welcome"), {
    prefix: "irc.local",
    command: "001",
    params: ["alice"],
    trailing: "Welcome"
  });
});

test("validates nicknames and channels", () => {
  assert.equal(validNickname("Alice_42"), true);
  assert.equal(validNickname("42alice"), false);
  assert.equal(validNickname("bad nick"), false);
  assert.equal(validChannel("#lobby"), true);
  assert.equal(validChannel("lobby"), false);
});
