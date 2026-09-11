---
id: expertise.personal-ai-strategy
type: expertise
status: active
updated: 2026-09-11
confidence: confirmed
tags: [local-ai, rag, knowledge-management]
---

# Personal AI Knowledge Strategy

## AI Knowledge Strategy
For a local personal AI, separate information into four categories.

### 1. System Prompt
Use for:
- Assistant identity
- Behavior
- Tone
- Basic permanent instructions

### 2. User Profile
Use for:
- Name
- Skills
- Preferences
- Current role
- Common tools

### 3. RAG Knowledge Base
Use for:
- Technical notes
- Project documentation
- Manuals
- Procedures
- Company knowledge
- Hardware configuration
- Troubleshooting history

### 4. Conversation Memory
Use for:
- Recent project decisions
- User preferences learned over time
- Temporary project state
- Previously selected hardware or software

## Fine-Tuning
Fine-tuning should not be the first choice for storing facts.

Use RAG when information:
- Changes over time
- Comes from documents
- Needs to be editable
- Needs to be traceable

Use fine-tuning mainly when changing:
- Response style
- Output format
- Specialized behavior
- Repeated reasoning patterns

## Current AI Learning Priorities
Recommended order:
1. Ollama API
2. Modelfile
3. FastAPI
4. Embeddings
5. RAG
6. ChromaDB
7. Conversation memory
8. Tool / function calling
9. Fine-tuning / LoRA

## Knowledge Base Maintenance
Good practice:
- Keep one subject per Markdown file
- Use descriptive headings
- Avoid duplicate facts
- Add dates when information may change
- Clearly mark uncertain information
- Keep passwords and API keys outside the knowledge base
- Do not place credentials in Markdown files used for RAG

## Suggested Future Files
Possible additions:
- ai_models.md
- blender_vr.md
- marine_engineering.md
- linux_commands.md
- network_config.md
- troubleshooting.md
- projects.md
- hardware_inventory.md
