#!/usr/bin/env python3
"""Build the Qdrant collection from ../knowledge Markdown files.

Run this after editing knowledge. It recreates only QDRANT_COLLECTION, making
the result exact and preventing deleted Markdown chunks from being retrieved.
"""

import argparse
import os
import re
import sys
import uuid
from pathlib import Path

import httpx
from qdrant_client import QdrantClient
from qdrant_client.models import Distance, PointStruct, VectorParams

KNOWLEDGE_DIR = Path(__file__).resolve().parent / "knowledge"


def load_dotenv() -> None:
    env_file = Path(__file__).with_name(".env")
    if env_file.exists():
        for line in env_file.read_text(encoding="utf-8").splitlines():
            if "=" in line and not line.lstrip().startswith("#"):
                key, value = line.split("=", 1)
                os.environ.setdefault(key.strip(), value.strip().strip("\"'"))


def chunks(path: Path, size: int, overlap: int):
    text = path.read_text(encoding="utf-8").strip()
    # Prefer headings and paragraph boundaries; then retain a small overlap.
    pieces = [p.strip() for p in re.split(r"(?=^#{1,6}\s)|\n\s*\n", text, flags=re.MULTILINE) if p.strip()]
    buffer = ""
    for piece in pieces:
        candidate = f"{buffer}\n\n{piece}".strip() if buffer else piece
        if len(candidate) <= size:
            buffer = candidate
            continue
        if buffer:
            yield buffer
        while len(piece) > size:
            yield piece[:size]
            piece = piece[size - overlap:]
        buffer = piece
    if buffer:
        yield buffer


def embed(client: httpx.Client, url: str, model: str, texts: list[str]) -> list[list[float]]:
    response = client.post(url.rstrip("/") + "/api/embed", json={"model": model, "input": texts, "truncate": True})
    response.raise_for_status()
    vectors = response.json().get("embeddings")
    if not isinstance(vectors, list) or len(vectors) != len(texts):
        raise RuntimeError("Ollama returned an unexpected embedding batch")
    return vectors


def main() -> int:
    load_dotenv()
    parser = argparse.ArgumentParser(description="Index Markdown knowledge into Qdrant")
    parser.add_argument("--chunk-size", type=int, default=1200)
    parser.add_argument("--overlap", type=int, default=160)
    parser.add_argument("--batch-size", type=int, default=32)
    args = parser.parse_args()
    if args.chunk_size < 100 or not 0 <= args.overlap < args.chunk_size:
        parser.error("chunk-size must be >= 100 and overlap must be smaller")
    source_chunks = [(path.relative_to(KNOWLEDGE_DIR).as_posix(), text) for path in sorted(KNOWLEDGE_DIR.rglob("*.md")) for text in chunks(path, args.chunk_size, args.overlap)]
    if not source_chunks:
        raise RuntimeError(f"No Markdown files found in {KNOWLEDGE_DIR}")
    ollama_url = os.getenv("OLLAMA_URL", "http://127.0.0.1:11434")
    embed_model = os.getenv("OLLAMA_EMBED_MODEL", "embeddinggemma")
    qdrant = QdrantClient(url=os.getenv("QDRANT_URL", "http://127.0.0.1:6333"), api_key=os.getenv("QDRANT_API_KEY") or None)
    with httpx.Client(timeout=120) as ollama:
        first_vector = embed(ollama, ollama_url, embed_model, [source_chunks[0][1]])[0]
        collection = os.getenv("QDRANT_COLLECTION", "local_ai_knowledge")
        if qdrant.collection_exists(collection):
            qdrant.delete_collection(collection)
        qdrant.create_collection(collection, vectors_config=VectorParams(size=len(first_vector), distance=Distance.COSINE))
        records = [(source_chunks[0][0], source_chunks[0][1], first_vector)]
        for offset in range(1, len(source_chunks), args.batch_size):
            batch = source_chunks[offset: offset + args.batch_size]
            vectors = embed(ollama, ollama_url, embed_model, [text for _, text in batch])
            records.extend((source, text, vector) for (source, text), vector in zip(batch, vectors))
    points = [
        PointStruct(
            # Qdrant accepts an unsigned integer or canonical UUID, not a raw
            # SHA-256 hex digest. UUIDv5 remains stable across re-indexes.
            id=str(uuid.uuid5(uuid.NAMESPACE_URL, f"local-ai-knowledge/{source}\0{text}")),
            vector=vector,
            payload={"source": source, "text": text},
        )
        for source, text, vector in records
    ]
    qdrant.upsert(collection_name=collection, points=points, wait=True)
    print(f"Indexed {len(points)} chunks from {len(set(source for source, _ in source_chunks))} files into {collection} using {embed_model}.")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f"Indexing failed: {exc}", file=sys.stderr)
        raise SystemExit(1)
