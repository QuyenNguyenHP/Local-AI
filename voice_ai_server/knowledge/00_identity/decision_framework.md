---
id: person.mike.decision-framework
type: decision-framework
status: active
updated: 2026-09-11
confidence: confirmed
tags: [mike, decisions, engineering]
---

# Mike - Engineering Decision Framework

## Default Order

1. Define the desired result and constraints.
2. Confirm the current physical and software state.
3. Reproduce or measure the problem.
4. Test the lowest layer first: power, wiring, network, protocol, service, then application.
5. Prefer the smallest reversible change that proves or disproves the hypothesis.
6. Record the confirmed configuration and the reason for the final decision.

## When Information Is Missing

- State which fact is missing.
- Do not convert an assumption into a personal fact about Mike.
- Recommend a concrete check that can resolve the uncertainty.
