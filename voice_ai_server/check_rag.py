#!/usr/bin/env python3
"""Diagnose the semantic RAG index and show unfiltered similarity scores."""

import argparse
import sys

import httpx
from qdrant_client import QdrantClient
from qdrant_client.models import FieldCondition, Filter, MatchValue

from app.config import get_settings


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "question",
        nargs="?",
        default="What tools and technologies does Mike work with?",
        help="Question to embed and search for",
    )
    parser.add_argument("--limit", type=int, default=5)
    args = parser.parse_args()
    if args.limit < 1:
        parser.error("--limit must be at least 1")

    settings = get_settings()
    qdrant = QdrantClient(
        url=settings.qdrant_url,
        api_key=settings.qdrant_api_key or None,
    )
    collection = qdrant.get_collection(settings.qdrant_collection)
    active_filter = Filter(
        must=[FieldCondition(key="status", match=MatchValue(value="active"))]
    )
    active_count = qdrant.count(
        collection_name=settings.qdrant_collection,
        count_filter=active_filter,
        exact=True,
    ).count

    with httpx.Client(timeout=settings.ollama_timeout_seconds) as client:
        response = client.post(
            settings.ollama_url.rstrip("/") + "/api/embed",
            json={
                "model": settings.ollama_embed_model,
                "input": args.question,
                "truncate": True,
            },
        )
        response.raise_for_status()
    embeddings = response.json().get("embeddings")
    if not isinstance(embeddings, list) or not embeddings or not isinstance(embeddings[0], list):
        raise RuntimeError("Ollama returned an invalid embedding response")
    vector = embeddings[0]

    points = qdrant.query_points(
        collection_name=settings.qdrant_collection,
        query=vector,
        query_filter=active_filter,
        limit=args.limit,
        with_payload=True,
        with_vectors=False,
    ).points

    print(f"Collection: {settings.qdrant_collection}")
    print(f"Points: {collection.points_count}; active points: {active_count}")
    print(f"Embedding model: {settings.ollama_embed_model}; query dimensions: {len(vector)}")
    print(f"Configured threshold: {settings.rag_score_threshold:.3f}")
    print(f"Question: {args.question}")
    if not points:
        print("No active points exist or Qdrant returned no candidates.")
        return 2

    print("\nTop active candidates (queried without a score threshold):")
    passing = 0
    for position, point in enumerate(points, start=1):
        payload = point.payload or {}
        passes = point.score >= settings.rag_score_threshold
        passing += int(passes)
        text = " ".join(str(payload.get("text", "")).split())[:180]
        print(
            f"{position}. score={point.score:.3f} "
            f"passes_threshold={'yes' if passes else 'no'} "
            f"source={payload.get('source', 'unknown')}"
        )
        print(f"   {text}")

    print(f"\nCandidates that the server would return: {passing}")
    return 0 if passing else 3


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f"RAG check failed: {exc}", file=sys.stderr)
        raise SystemExit(1)
