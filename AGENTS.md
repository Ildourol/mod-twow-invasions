# Embedded AGENTS.md: mod-twow-invasions Specialist

This file defines operating instructions and architectural guidelines for autonomous agents operating inside [`mod-twow-invasions`](file:///C:/Users/Admin/AntigravityProfiles/Projects%20Twow/Turtle%20wow%20modules/mod-twow-invasions).

---

## 1. Domain Mandate

You are the dedicated specialist for **mod-twow-invasions** (World PvP Invasions & PlayerBot Phantoms). Your responsibilities include:
1. Developing and maintaining gameplay features for Dark Souls-style world PvP invasions on Turtle WoW (vMaNGOS 1.12.1 / 1.18.1.7272 client).
2. Adhering strictly to **The Unediting Doctrine**: zero edits to `tortoise-wow` or `TortoiseBots`.
3. Managing the dual access modes: Autolearned Spell at level 10 (Mode 1, default), Optional physical Talisman (Mode 2), or Hybrid (Mode 3).
4. Managing bidirectional PlayerBot phantom invasions (`Invasion.AllowBotToPlayer`, `Invasion.AllowPlayerToBot`, `Invasion.AllowBotToBot`) with pacing limits, individual cooldowns, and active caps.
5. Preserving the 100-yard arena leash with 10s countdown forfeit, 250ms minimap radar updates via `CINV` addon channel, asymmetric death handling, and mutual hostility via non-intrusive `SetFFAPvP(true)`.
6. Consulting and following the [**Self-Optimizing Knowledge Engine**](file:///C:/Users/Admin/AntigravityProfiles/Projects%20Twow/Turtle%20wow%20modules/.agents/SELF_OPTIMIZING_ORCHESTRATOR.md) for zero-search API mappings, hook signatures, and non-intrusive modding patterns.
7. Logging any architectural defects, parity gaps, or performance issues in the centralized workspace registry: [`docs/ISSUES.md`](file:///C:/Users/Admin/AntigravityProfiles/Projects%20Twow/Turtle%20wow%20modules/docs/ISSUES.md). No local issue folders are permitted.
8. Logging completed feature milestones, UI ports, and improvements in [`docs/HISTORY.md`](file:///C:/Users/Admin/AntigravityProfiles/Projects%20Twow/Turtle%20wow%20modules/docs/HISTORY.md).
9. Updating configuration options in [`docs/CUSTOM_CONFIGS.md`](file:///C:/Users/Admin/AntigravityProfiles/Projects%20Twow/Turtle%20wow%20modules/docs/CUSTOM_CONFIGS.md) and recompiling the dashboard via `python docs/generate_dashboard.py`.

---

## 2. Invariant Rules

* **Core Immutability**: Never create core patches. Use `ScriptObjects.h` non-intrusive hooks exclusively (`PlayerScript`, `WorldScript`, `UnitScript`, `ItemScript`).
* **Headless Bot Detection**: Always identify bots using `player->GetSession() && player->GetSession()->IsHeadless()`. Suppress chat and addon packets to headless sessions.
* **Compilation**: Handled exclusively by the user (Compilation Default OFF). Do not invoke build tools directly.
* **Self-Optimization**: Append newly discovered patterns and platform seams to [`.agents/SELF_OPTIMIZING_ORCHESTRATOR.md`](file:///C:/Users/Admin/AntigravityProfiles/Projects%20Twow/Turtle%20wow%20modules/.agents/SELF_OPTIMIZING_ORCHESTRATOR.md).

---

## 3. Module Architecture & File Map

* `src/`:
  * `mod-twow-invasions.cpp`: Module startup script & script loader entrypoint (`Addmod_twow_invasionsScripts()`).
  * `InvasionMgr.h`: Class definition, phase enum, `Invasion` struct, and tracking state.
  * `InvasionMgr.cpp`: Matchmaking, target sweep, arrival geometry, arena leash, death lifecycle, rewards, and debug logging.
  * `InvasionScripts.cpp`: `PlayerScript`, `WorldScript`, `UnitScript`, and `ItemScript` non-intrusive hooks.
* `conf/mod-twow-invasions.conf.dist`: Distributed config file under `[ModuleConf]`.
* `data/sql/character/001_invasion_match_history.sql`: Table `character_invasion_match`.
* `data/sql/world/001_invasion_spell_and_item.sql`: Item 100035, spell 51942, and NPC 100031.
* `docs/ARCHITECTURE.md`: Technical architectural guide and wire protocols.
