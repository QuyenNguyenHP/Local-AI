"""Single registry for tools that may be exposed to the language model."""

from typing import Any

from ..config import Settings
from .home_assistant import HomeAssistantTool


class ToolRegistry:
    """Expose only enabled tools and route a model call to its implementation."""

    def __init__(self, settings: Settings):
        self._tools: dict[str, Any] = {}
        if settings.ha_enabled:
            home_assistant = HomeAssistantTool(settings)
            self._tools[home_assistant.name] = home_assistant

    def definitions(self) -> list[dict[str, Any]]:
        return [tool.definition() for tool in self._tools.values()]

    async def execute(self, name: str, arguments: dict[str, Any] | str | None) -> str:
        tool = self._tools.get(name)
        if tool is None:
            raise ValueError("Tool is not allowed")
        return await tool.execute(arguments)
