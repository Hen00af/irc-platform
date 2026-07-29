import assert from "node:assert/strict";
import test from "node:test";
import { ChannelHistory } from "../src/history.js";

test("keeps only the configured number of messages per channel", () => {
  const history = new ChannelHistory({ limit: 3, replayLimit: 2 });
  for (let index = 1; index <= 4; index += 1) {
    history.add("#relay", {
      nick: "tester",
      text: `message ${index}`,
      timestamp: `2026-01-01T00:00:0${index}.000Z`
    });
  }

  assert.deepEqual(
    history.recent("#relay").map(({ text }) => text),
    ["message 3", "message 4"]
  );
});

test("separates channel histories", () => {
  const history = new ChannelHistory();
  history.add("#one", { nick: "one", text: "first" });
  history.add("#two", { nick: "two", text: "second" });

  assert.equal(history.recent("#one")[0].text, "first");
  assert.equal(history.recent("#two")[0].text, "second");
  assert.deepEqual(history.recent("#missing"), []);
});
