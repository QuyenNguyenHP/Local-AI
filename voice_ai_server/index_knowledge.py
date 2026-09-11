#!/usr/bin/env python3
"""Build the Qdrant collection from active Markdown files under knowledge/.

Run this after editing knowledge. It recreates only QDRANT_COLLECTION, making
the result exact and preventing deleted Markdown chunks from being retrieved.
Files under knowledge/90_archive are deliberately excluded.
"""

import argparse
import os
import re
import sys
import uuid
from pathlib import Path

import httpx
import yaml
from qdrant_client import QdrantClient
from qdrant_client.models import Distance, PayloadSchemaType, PointStruct, VectorParams

KNOWLEDGE_DIR = Path(__file__).resolve().parent / "knowledge"
ARCHIVE_DIR = "90_archive"
REQUIRED_PROPERTIES = {"id", "type", "status", "updated", "confidence", "tags"}


def load_dotenv() -> None:
    env_file = Path(__file__).with_name(".env")
    if env_file.exists():
        for line in env_file.read_text(encoding="utf-8").splitlines():
            if "=" in line and not line.lstrip().startswith("#"):
                key, value = line.split("=", 1)
                os.environ.setdefault(key.strip(), value.strip().strip("\"'"))


def normalize_property(value):
    if value is None or isinstance(value, (str, int, float, bool)):
        return value
    if hasattr(value, "isoformat"):
        return value.isoformat()
    if isinstance(value, list):
        return [normalize_property(item) for item in value]
    if isinstance(value, dict):
        return {str(key): normalize_property(item) for key, item in value.items()}
    return str(value)


def parse_document(path: Path) -> tuple[dict[str, object], str]:
    raw = path.read_text(encoding="utf-8").strip()
    match = re.fullmatch(r"---\s*\n(.*?)\n---\s*\n(.*)", raw, flags=re.DOTALL)
    if not match:
        raise ValueError(f"{path} must begin with YAML frontmatter enclosed by ---")
    loaded = yaml.safe_load(match.group(1))
    if not isinstance(loaded, dict):
        raise ValueError(f"{path} frontmatter must be a YAML object")
    missing = REQUIRED_PROPERTIES - loaded.keys()
    if missing:
        raise ValueError(f"{path} is missing required properties: {', '.join(sorted(missing))}")
    tags = loaded["tags"]
    if not isinstance(tags, list) or not all(isinstance(tag, str) and tag.strip() for tag in tags):
        raise ValueError(f"{path} property 'tags' must be a list of non-empty strings")
    metadata = {str(key): normalize_property(value) for key, value in loaded.items()}
    metadata["id"] = str(metadata["id"])
    metadata["document_id"] = metadata["id"]
    metadata["type"] = str(metadata["type"])
    metadata["status"] = str(metadata["status"]).lower()
    metadata["updated"] = str(metadata["updated"])
    metadata["confidence"] = str(metadata["confidence"]).lower()
    title_match = re.search(r"^#\s+(.+)$", match.group(2), flags=re.MULTILINE)
    metadata["title"] = title_match.group(1).strip() if title_match else path.stem.replace("_", " ").title()
    return metadata, match.group(2).strip()


def chunks(text: str, size: int, overlap: int):
    # Prefer headings, list items, and paragraph boundaries so long timelines
    # and equipment lists are not cut in the middle of an entry.
    pieces = [
        piece.strip()
        for piece in re.split(
            r"(?=^#{1,6}\s)|(?=^[-*]\s)|(?=^\d+\.\s)|\n\s*\n",
            text,
            flags=re.MULTILINE,
        )
        if piece.strip()
    ]
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


def embedding_text(metadata: dict[str, object], text: str) -> str:
    tags = ", ".join(metadata["tags"])
    return (
        f"Document: {metadata['title']}\n"
        f"Document ID: {metadata['document_id']}\n"
        f"Type: {metadata['type']}\n"
        f"Status: {metadata['status']}\n"
        f"Updated: {metadata['updated']}\n"
        f"Confidence: {metadata['confidence']}\n"
        f"Tags: {tags}\n\n{text}"
    )


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
    active_paths = [
        path
        for path in sorted(KNOWLEDGE_DIR.rglob("*.md"))
        if ARCHIVE_DIR not in path.relative_to(KNOWLEDGE_DIR).parts
    ]
    source_chunks = []
    for path in active_paths:
        source = path.relative_to(KNOWLEDGE_DIR).as_posix()
        metadata, body = parse_document(path)
        for chunk_index, text in enumerate(chunks(body, args.chunk_size, args.overlap)):
            source_chunks.append({
                "source": source,
                "chunk_index": chunk_index,
                "text": text,
                "embedding_text": embedding_text(metadata, text),
                **metadata,
            })
    if not source_chunks:
        raise RuntimeError(f"No Markdown files found in {KNOWLEDGE_DIR}")
    ollama_url = os.getenv("OLLAMA_URL", "http://127.0.0.1:11434")
    embed_model = os.getenv("OLLAMA_EMBED_MODEL", "embeddinggemma")
    qdrant = QdrantClient(url=os.getenv("QDRANT_URL", "http://127.0.0.1:6333"), api_key=os.getenv("QDRANT_API_KEY") or None)
    with httpx.Client(timeout=120) as ollama:
        first_vector = embed(ollama, ollama_url, embed_model, [source_chunks[0]["embedding_text"]])[0]
        collection = os.getenv("QDRANT_COLLECTION", "local_ai_knowledge")
        if qdrant.collection_exists(collection):
            qdrant.delete_collection(collection)
        qdrant.create_collection(collection, vectors_config=VectorParams(size=len(first_vector), distance=Distance.COSINE))
        for field_name in ("document_id", "type", "status", "confidence", "tags"):
            qdrant.create_payload_index(
                collection_name=collection,
                field_name=field_name,
                field_schema=PayloadSchemaType.KEYWORD,
                wait=True,
            )
        records = [(source_chunks[0], first_vector)]
        for offset in range(1, len(source_chunks), args.batch_size):
            batch = source_chunks[offset: offset + args.batch_size]
            vectors = embed(ollama, ollama_url, embed_model, [item["embedding_text"] for item in batch])
            records.extend(zip(batch, vectors))
    points = [
        PointStruct(
            # Qdrant accepts an unsigned integer or canonical UUID, not a raw
            # SHA-256 hex digest. UUIDv5 remains stable across re-indexes.
            id=str(uuid.uuid5(uuid.NAMESPACE_URL, f"local-ai-knowledge/{item['document_id']}/{item['chunk_index']}\0{item['text']}")),
            vector=vector,
            payload={key: value for key, value in item.items() if key != "embedding_text"},
        )
        for item, vector in records
    ]
    qdrant.upsert(collection_name=collection, points=points, wait=True)
    print(f"Indexed {len(points)} chunks from {len(set(item['source'] for item in source_chunks))} files into {collection} using {embed_model}.")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f"Indexing failed: {exc}", file=sys.stderr)
        raise SystemExit(1)
