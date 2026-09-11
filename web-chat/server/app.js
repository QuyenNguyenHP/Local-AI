import express from "express";
import { buildKnowledgeContext } from "./knowledge.js";
import path from "node:path";
import { fileURLToPath } from "node:url";
export function createApp({
  ollamaUrl = process.env.OLLAMA_URL || "http://127.0.0.1:11434",
  fetchImpl = fetch,
  contextBuilder,
} = {}) {
  const app = express();
  app.use(express.json({ limit: "256kb" }));
  app.get("/api/models", async (_req, res) => {
    try {
      const response = await fetchImpl(`${ollamaUrl}/api/tags`, {
        signal: AbortSignal.timeout(5000),
      });
      if (!response.ok) throw new Error("Model service unavailable");
      const data = await response.json();
      res.json({
        models: data.models.map((m) => m.name),
        defaultModel: process.env.OLLAMA_MODEL || "dq-assistant:latest",
      });
    } catch {
      res
        .status(503)
        .json({
          error:
            "Cannot connect to Ollama. Start it with ollama serve, then refresh.",
        });
    }
  });
  app.post("/api/chat", async (req, res) => {
    const { model, messages } = req.body;
    if (
      typeof model !== "string" ||
      !model.trim() ||
      model.length > 200 ||
      !Array.isArray(messages) ||
      !messages.length ||
      messages.length > 100 ||
      messages.some(
        (m) =>
          !m ||
          !["user", "assistant"].includes(m.role) ||
          typeof m.content !== "string" ||
          !m.content.trim() ||
          m.content.length > 32000,
      ) ||
      messages.at(-1).role !== "user"
    ) {
      return res
        .status(400)
        .json({
          error:
            "Provide a model and valid chat messages ending with a user message.",
        });
    }
    let context;
    try {
      context = contextBuilder
        ? await contextBuilder(messages.at(-1).content)
        : await buildKnowledgeContext(messages.at(-1).content, { fetchImpl, ollamaUrl });
    } catch (error) {
      return res.status(500).json({ error: error.message });
    }
    const enrichedMessages = [
      ...messages
        .slice(0, -1)
        .slice(-10)
        .map(({ role, content }) => ({ role, content })),
      { role: "user", content: context },
    ];
    const controller = new AbortController();
    const timeout = setTimeout(() => controller.abort(), 180000);
    res.on("close", () => controller.abort());
    try {
      const upstream = await fetchImpl(`${ollamaUrl}/api/chat`, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        signal: controller.signal,
        body: JSON.stringify({
          model,
          messages: enrichedMessages,
          stream: true,
          options: { temperature: 0.6, num_ctx: 8192, num_predict: 512 },
        }),
      });
      if (!upstream.ok)
        return res
          .status(502)
          .json({
            error:
              "Ollama could not load this model. Check the model is installed and try again.",
          });
      res.setHeader("Content-Type", "application/x-ndjson");
      res.setHeader("Cache-Control", "no-cache");
      res.setHeader("X-Accel-Buffering", "no");
      for await (const chunk of upstream.body) {
        if (res.destroyed) break;
        res.write(chunk);
      }
      res.end();
    } catch {
      if (!res.destroyed) {
        const error =
          "Generation interrupted. Check Ollama is running and try again.";
        if (res.headersSent) res.end(JSON.stringify({ error }) + "\n");
        else res.status(503).json({ error });
      }
    } finally {
      clearTimeout(timeout);
    }
  });
  app.use("/api", (_req, res) =>
    res.status(404).json({ error: "API route not found" }),
  );
  const dist = fileURLToPath(new URL("../dist", import.meta.url));
  app.use(express.static(dist));
  app.get("/{*path}", (_req, res) =>
    res.sendFile(path.join(dist, "index.html")),
  );
  app.use((err, _req, res, _next) =>
    res
      .status(err.status || 500)
      .json({
        error:
          err.status === 413 ? "Message is too large." : "Invalid request.",
      }),
  );
  return app;
}
