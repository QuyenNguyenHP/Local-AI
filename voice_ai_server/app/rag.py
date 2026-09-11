"""Semantic retrieval from Qdrant using embeddings served by local Ollama."""

import asyncio
from time import perf_counter

import httpx
from qdrant_client import QdrantClient

from .config import Settings
from .progress import log


class SemanticKnowledge:
    """Read-only RAG client. Indexing is deliberately kept out of API requests."""

    def __init__(self, settings: Settings):
        self.settings = settings
        self._client: QdrantClient | None = None

    def _qdrant(self) -> QdrantClient:
        if self._client is None:
            self._client = QdrantClient(url=self.settings.qdrant_url, api_key=self.settings.qdrant_api_key or None)
        return self._client

    async def _embed(self, text: str) -> list[float]:
        timeout = httpx.Timeout(self.settings.ollama_timeout_seconds)
        async with httpx.AsyncClient(timeout=timeout) as client:
            response = await client.post(
                self.settings.ollama_url.rstrip("/") + "/api/embed",
                json={"model": self.settings.ollama_embed_model, "input": text, "truncate": True},
            )
            response.raise_for_status()
        embeddings = response.json().get("embeddings")
        if not isinstance(embeddings, list) or not embeddings or not isinstance(embeddings[0], list):
            raise ValueError("Ollama did not return an embedding")
        return embeddings[0]

    async def retrieve(self, question: str) -> str:
        """Return bounded, attributed excerpts suitable for putting in an LLM prompt."""
        started = perf_counter()
        vector = await self._embed(question)

        def query():
            return self._qdrant().query_points(
                collection_name=self.settings.qdrant_collection,
                query=vector,
                limit=self.settings.rag_top_k,
                score_threshold=self.settings.rag_score_threshold,
                with_payload=True,
                with_vectors=False,
            ).points

        try:
            points = await asyncio.to_thread(query)
        except Exception as exc:
            # Keep an unavailable vector store as an upstream-service error (503),
            # not an unhandled API exception. Do not silently answer without RAG.
            raise ValueError(f"Qdrant retrieval failed: {exc}") from exc
        remaining = self.settings.rag_max_chars
        excerpts = []
        for point in points:
            payload = point.payload or {}
            chunk = payload.get("text")
            source = payload.get("source", "unknown")
            if not isinstance(chunk, str) or remaining <= 0:
                continue
            clipped = chunk[:remaining]
            remaining -= len(clipped)
            excerpts.append(f"Source: {source} (similarity {point.score:.3f})\n{clipped}")
        log("RAG | model=%s hits=%d context=%d characters in %.2fs", self.settings.ollama_embed_model, len(excerpts), self.settings.rag_max_chars - remaining, perf_counter() - started)
        return "\n\n".join(excerpts)
