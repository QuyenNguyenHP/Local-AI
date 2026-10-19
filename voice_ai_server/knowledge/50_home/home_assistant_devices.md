---
id: home.home-assistant-devices
type: home-automation
status: active
updated: 2026-10-19
confidence: confirmed
tags: [home-assistant, lights, living-room]
---
# Home Assistant Devices

## Main Light

The user may refer to this device as:

- turn on the light
- turn off the light
- living room light
- main light
- light

Rules:

- To turn the light on, call `home_assistant_switch` with `entity_id` set to
  the matching value from the tool's allowed entity list and `state` set to `on`.
- To turn the light off, call `home_assistant_switch` with `entity_id` set to
  the matching value from the tool's allowed entity list and `state` set to `off`.
- If exactly one entity is allowed and the user asks for "the light," use that
  entity. The entity ID is supplied dynamically by `HA_ALLOWED_ENTITIES`.
- If the requested room or device is ambiguous, ask the user to clarify. Do not
  guess.

## Adding or Replacing Devices

The tool supports `light.*` and `switch.*` entity IDs. The service domain is
selected automatically from each entity ID. For example, `light.light` uses
`light.turn_on` and `light.turn_off`, while `switch.fan` uses `switch.turn_on`
and `switch.turn_off`.

Set the exact allowed entity IDs in `voice_ai_server/.env`, separated by commas:

```dotenv
HA_ALLOWED_ENTITIES=light.light,switch.bedroom_light,switch.living_room_fan
```

Restart the Voice AI service after changing `.env`. The AI can control only
entities in this allowlist. Find entity IDs in Home Assistant under **Settings
→ Devices & services → Entities**.

Changing only the allowlist is sufficient when the user's request clearly
identifies the device; no entity ID is hardcoded in this document. Add a
device-specific section only when the AI needs mappings for nicknames, rooms, or
ambiguous natural language. Re-index Qdrant after changing this document.
