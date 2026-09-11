# Personal knowledge with semantic RAG

The voice server and web chat use Ollama embeddings plus Qdrant for every
knowledge lookup. There is no keyword router or non-RAG fallback.

## Knowledge layout

```text
voice_ai_server/
├── knowledge/                 # Every Markdown file here is indexed
│   ├── 00_identity/           # Profile, style, values, decision framework
│   ├── 10_biography/          # Career and personal history
│   ├── 20_expertise/          # Reusable domain knowledge
│   ├── 30_projects/           # One directory per project
│   ├── 40_assets/             # Hardware, servers, and software inventory
│   ├── 50_memory/             # Dated decisions, events, and lessons
│   ├── 60_procedures/         # Repeatable checklists
│   └── 90_archive/            # Superseded information; excluded from indexing
└── knowledge_templates/       # Blank templates are not indexed
```

Keep one subject per file. Make each heading and paragraph understandable when
retrieved without the rest of the document. Use ISO dates (`YYYY-MM-DD`), state
whether facts are confirmed or planned, and avoid duplicate sources of truth.
Never store credentials, tokens, private keys, or passwords under `knowledge/`.

## Required document properties

Every indexed Markdown file must begin with YAML frontmatter:

```yaml
---
id: project.unitree-r1.platform-configuration
type: project-configuration
status: active
updated: 2026-09-11
confidence: confirmed
tags: [unitree-r1, jetson, ros2, dds]
---
```

The indexer removes the frontmatter from the document body, validates the required
properties, and stores all properties in every Qdrant point. It also prefixes every
embedding input with the document title, ID, type, status, update date, confidence,
and tags. This gives every chunk its own document context without showing raw YAML
as part of the retrieved text.

Qdrant keyword payload indexes are created for `document_id`, `type`, `status`,
`confidence`, and `tags`, allowing efficient metadata filters as the collection grows.

Queries filter for `status: active`. Use `planned` inside an active project's body
for individual future capabilities. Set the whole document to `deprecated` or move
it under `90_archive/` when it should no longer be returned. Additional properties
such as `owner` or `aliases` are preserved automatically in the Qdrant payload.

## Build or refresh the index

Start Qdrant and ensure the embedding model is installed:

```bash
cd /home/daikai/Local-AI/voice_ai_server
docker compose -f docker-compose.qdrant.yml up -d qdrant
ollama pull embeddinggemma
```

Index all Markdown documents:

```bash
cd /home/daikai/Local-AI
.venv/bin/python voice_ai_server/index_knowledge.py
```

For the Compose deployment:

```bash
cd /home/daikai/Local-AI/voice_ai_server
sudo docker compose -f docker-compose.qdrant.yml run --rm voice-ai python index_knowledge.py
```

Rerun the indexer after adding, editing, moving, archiving, or deleting knowledge.
It recreates only the configured collection, preventing stale chunks. A full
re-index is required after upgrading from the old text-only payload format.

## Configuration

```dotenv
QDRANT_URL=http://127.0.0.1:6333
QDRANT_API_KEY=
QDRANT_COLLECTION=local_ai_knowledge
OLLAMA_EMBED_MODEL=embeddinggemma
RAG_TOP_K=5
RAG_SCORE_THRESHOLD=0.35
RAG_MAX_CHARS=9000
```

The indexing process and every query must use the same embedding model. Changing
`OLLAMA_EMBED_MODEL` requires a complete re-index.

## Troubleshooting

- No relevant answer: rerun the indexer, lower `RAG_SCORE_THRESHOLD` carefully,
  and make the document section more self-contained.
- Wrong project retrieved: use the full project and entity names in headings and
  paragraphs instead of ambiguous pronouns.
- Qdrant error: verify the service, URL, collection name, and API key.
- Embedding error: verify Ollama is running and `OLLAMA_EMBED_MODEL` is installed.
- Old information appears: update or archive the original source, then re-index.
