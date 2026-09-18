"""Allowlisted Home Assistant switch control."""

import json
from typing import Any

import httpx

from ..config import Settings
from ..progress import log


class HomeAssistantTool:
    name = "home_assistant_switch"

    def __init__(self, settings: Settings):
        self.settings = settings

    def definition(self) -> dict[str, Any]:
        return {
            "type": "function",
            "function": {
                "name": self.name,
                "description": "Turn an explicitly configured Home Assistant switch on or off. Use only when the user clearly asks to control that switch.",
                "parameters": {
                    "type": "object",
                    "properties": {
                        "entity_id": {
                            "type": "string",
                            "enum": sorted(self.settings.ha_allowed_entities),
                            "description": "The configured Home Assistant switch entity ID.",
                        },
                        "state": {
                            "type": "string",
                            "enum": ["on", "off"],
                            "description": "Requested switch state.",
                        },
                    },
                    "required": ["entity_id", "state"],
                },
            },
        }

    async def execute(self, arguments: dict[str, Any] | str | None) -> str:
        if not self.settings.ha_enabled:
            raise ValueError("Home Assistant tool is disabled")
        if not self.settings.ha_url or not self.settings.ha_token:
            raise ValueError("Home Assistant is not configured")
        if isinstance(arguments, str):
            try:
                arguments = json.loads(arguments)
            except json.JSONDecodeError as exc:
                raise ValueError("Tool arguments are not valid JSON") from exc
        arguments = arguments or {}
        entity_id = arguments.get("entity_id")
        state = arguments.get("state")
        if not isinstance(entity_id, str) or entity_id not in self.settings.ha_allowed_entities:
            raise ValueError("That Home Assistant entity is not allowed")
        if state not in {"on", "off"}:
            raise ValueError("Switch state must be on or off")

        endpoint = f"{self.settings.ha_url.rstrip('/')}/api/services/switch/turn_{state}"
        try:
            async with httpx.AsyncClient(timeout=self.settings.ha_timeout_seconds) as client:
                response = await client.post(
                    endpoint,
                    headers={"Authorization": f"Bearer {self.settings.ha_token}"},
                    json={"entity_id": entity_id},
                )
                response.raise_for_status()
        except httpx.HTTPError as exc:
            raise ValueError("Home Assistant request failed") from exc
        log("Tool | home_assistant_switch entity=%s state=%s", entity_id, state)
        return f"Switch {entity_id} was turned {state}."
