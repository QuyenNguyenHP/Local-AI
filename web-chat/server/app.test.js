import test from "node:test";
import assert from "node:assert/strict";
import { createApp } from "./app.js";
async function server(t, fetchImpl) {
  const instance = createApp({ fetchImpl }).listen(0, "127.0.0.1");
  await new Promise((resolve) => instance.once("listening", resolve));
  t.after(() => {
    instance.closeAllConnections();
    instance.close();
  });
  return `http://127.0.0.1:${instance.address().port}`;
}
test("lists installed models", async (t) => {
  const url = await server(t, async () =>
    Response.json({ models: [{ name: "gemma3:4b" }] }),
  );
  const response = await fetch(url + "/api/models");
  assert.deepEqual((await response.json()).models, ["gemma3:4b"]);
});
test("rejects invalid and oversized conversation payloads without calling Ollama", async (t) => {
  const url = await server(t, () => {
    throw new Error("Must not call upstream");
  });
  for (const body of [
    { model: "a", messages: [] },
    { model: "a", messages: [{ role: "system", content: "x" }] },
    { model: "a", messages: [{ role: "user", content: "x".repeat(32001) }] },
  ]) {
    const response = await fetch(url + "/api/chat", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(body),
    });
    assert.equal(response.status, 400);
  }
});
test("streams model response and forwards conversation context", async (t) => {
  const expected =
    '{"message":{"content":"Xin chào"},"done":false}\n{"done":true}\n';
  const url = await server(t, async (_url, init) => {
    const body = JSON.parse(init.body);
    assert.equal(body.stream, true);
    assert.match(body.messages[0].content, /No notes matched/);
    assert.match(body.messages[0].content, /Question:\nHello$/);
    assert.match(body.messages[0].content, /Answer only in English/);
    return new Response(expected);
  });
  const response = await fetch(url + "/api/chat", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({
      model: "a",
      messages: [{ role: "user", content: "Hello" }],
    }),
  });
  assert.match(response.headers.get("content-type"), /ndjson/);
  assert.equal(await response.text(), expected);
});
test("reports an offline model service", async (t) => {
  const url = await server(t, async () => {
    throw new Error("offline");
  });
  assert.equal((await fetch(url + "/api/models")).status, 503);
  const response = await fetch(url + "/api/chat", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({
      model: "a",
      messages: [{ role: "user", content: "Hello" }],
    }),
  });
  assert.equal(response.status, 503);
  assert.match((await response.json()).error, /Ollama/);
});

test("injects matching knowledge only into the latest message", async (t) => {
  const url = await server(t, async (_url, init) => {
    const body = JSON.parse(init.body);
    assert.deepEqual(body.messages[0], {
      role: "user",
      content: "Earlier question",
    });
    assert.deepEqual(body.messages[1], {
      role: "assistant",
      content: "Earlier answer",
    });
    assert.match(body.messages[2].content, /Source: unitree_r1.md/);
    assert.match(body.messages[2].content, /Blank template fields are unknown/);
    assert.match(
      body.messages[2].content,
      /Question:\nTell me about my robot$/,
    );
    return new Response('{"done":true}\n');
  });
  const response = await fetch(url + "/api/chat", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({
      model: "a",
      messages: [
        { role: "user", content: "Earlier question" },
        { role: "assistant", content: "Earlier answer" },
        { role: "user", content: "Tell me about my robot" },
      ],
    }),
  });
  assert.equal(response.status, 200);
  await response.text();
});
test("does not call the model when knowledge lookup fails", async (t) => {
  let called = false;
  const instance = createApp({
    contextBuilder: async () => {
      throw new Error("Knowledge lookup failed");
    },
    fetchImpl: async () => {
      called = true;
    },
  }).listen(0, "127.0.0.1");
  await new Promise((resolve) => instance.once("listening", resolve));
  t.after(() => {
    instance.closeAllConnections();
    instance.close();
  });
  const response = await fetch(
    `http://127.0.0.1:${instance.address().port}/api/chat`,
    {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({
        model: "a",
        messages: [{ role: "user", content: "hello" }],
      }),
    },
  );
  assert.equal(response.status, 500);
  assert.match((await response.json()).error, /Knowledge lookup failed/);
  assert.equal(called, false);
});
