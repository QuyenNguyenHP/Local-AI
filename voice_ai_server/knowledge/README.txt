PERSONAL KNOWLEDGE ORGANIZATION

00_identity   Stable profile, communication style, values, and decision rules.
10_biography  Career timeline, roles, and important experiences.
20_expertise  Reusable domain knowledge and personal technical strategies.
30_projects   One folder per project; split overview, configuration, decisions, and troubleshooting as each project grows.
40_assets     Hardware, servers, software, and other owned or operated assets.
50_memory     Dated decisions, events, and lessons learned.
60_procedures Repeatable checklists and operating procedures.
90_archive    Superseded facts kept only for history; the indexer skips this directory.

Only factual, completed Markdown documents belong in active directories because their .md files are indexed into Qdrant.
Every active Markdown file must have id, type, status, updated, confidence, and tags properties in YAML frontmatter.
Blank templates live in ../knowledge_templates and are deliberately excluded from indexing.
Never store passwords, API keys, access tokens, private keys, or other secrets here.
