import test from "node:test";
import assert from "node:assert/strict";
import { createApp } from "./app.js";

async function server(t, fetchImpl) {
  const instance = createApp({
    fetchImpl,
    voiceAiUrl: "http://voice-ai.test:8000",
  }).listen(0, "127.0.0.1");
  await new Promise((resolve) => instance.once("listening", resolve));
  t.after(() => {
    instance.closeAllConnections();
    instance.close();
  });
  return `http://127.0.0.1:${instance.address().port}`;
}

test("lists installed models through Ollama", async (t) => {
  const url = await server(t, async () =>
    Response.json({ models: [{ name: "gemma3:4b" }] }),
  );
  const response = await fetch(url + "/api/models");
  assert.deepEqual((await response.json()).models, ["gemma3:4b"]);
});

test("rejects invalid conversation payloads without calling Voice AI", async (t) => {
  const url = await server(t, () => {
    throw new Error("Must not call upstream");
  });
  const response = await fetch(url + "/api/chat", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ model: "a", messages: [] }),
  });
  assert.equal(response.status, 400);
});

test("forwards text chat to Voice AI without STT or TTS", async (t) => {
  const url = await server(t, async (upstreamUrl, init) => {
    assert.equal(upstreamUrl, "http://voice-ai.test:8000/v1/chat/completions");
    const body = JSON.parse(init.body);
    assert.equal(body.model, "dq-assistant:latest");
    assert.deepEqual(body.messages, [{ role: "user", content: "Xin chào" }]);
    assert.equal(body.stream, undefined);
    return Response.json({
      choices: [{ message: { role: "assistant", content: "Chào bạn!" } }],
    });
  });
  const response = await fetch(url + "/api/chat", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({
      model: "dq-assistant:latest",
      messages: [{ role: "user", content: "Xin chào" }],
    }),
  });
  assert.equal(response.status, 200);
  assert.equal(
    (await response.json()).choices[0].message.content,
    "Chào bạn!",
  );
});

test("reports an unavailable Voice AI service", async (t) => {
  const url = await server(t, async () => {
    throw new Error("offline");
  });
  const response = await fetch(url + "/api/chat", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({
      model: "a",
      messages: [{ role: "user", content: "Hello" }],
    }),
  });
  assert.equal(response.status, 503);
  assert.match((await response.json()).error, /Voice AI/);
});
