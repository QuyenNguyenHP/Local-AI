import express from "express";
import path from "node:path";
import { fileURLToPath } from "node:url";
export function createApp({
  ollamaUrl = process.env.OLLAMA_URL || "http://127.0.0.1:11434",
  voiceAiUrl = process.env.VOICE_AI_URL || "http://127.0.0.1:8000",
  voiceAiApiKey = process.env.VOICE_AI_API_KEY || "",
  fetchImpl = fetch,
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
    const controller = new AbortController();
    const timeout = setTimeout(() => controller.abort(), 180000);
    res.on("close", () => controller.abort());
    try {
      const headers = { "Content-Type": "application/json" };
      if (voiceAiApiKey) headers.Authorization = `Bearer ${voiceAiApiKey}`;
      const upstream = await fetchImpl(
        `${voiceAiUrl.replace(/\/$/, "")}/v1/chat/completions`,
        {
        method: "POST",
        headers,
        signal: controller.signal,
        body: JSON.stringify({
          model,
          // Voice AI owns RAG and retains only the recent turns it needs.
          messages: messages.slice(-30).map(({ role, content }) => ({ role, content })),
        }),
        },
      );
      const data = await upstream.json().catch(() => ({}));
      if (!upstream.ok) {
        return res.status(upstream.status).json({
          error: data.detail || data.error || "Voice AI chat request failed.",
        });
      }
      const answer = data.choices?.[0]?.message?.content;
      if (typeof answer !== "string" || !answer.trim()) {
        return res.status(502).json({ error: "Voice AI returned an empty response." });
      }
      res.json({ choices: [{ message: { role: "assistant", content: answer } }] });
    } catch (error) {
      if (!res.destroyed) {
        res.status(503).json({
          error: "Cannot connect to Voice AI. Start voice_ai_server and try again.",
        });
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
