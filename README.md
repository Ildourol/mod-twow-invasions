<p align="center">
  <img src="banner.jpg" alt="mod-twow-invasions Banner" width="100%" />
</p>

# mod-twow-invasions: World PvP Invasions & PlayerBot Phantoms

> **Dark Souls-style asynchronous World PvP Invasions and autonomous PlayerBot Phantoms for Turtle WoW (vMaNGOS 1.12.1 / 1.18.1).**  
> *Channel the fiery rite, teleport 10 yards behind unsuspecting flagged players or roaming bots, and battle inside a high-stakes 100-yard duel arena.*

---

[![Turtle WoW](https://img.shields.io/badge/Turtle_WoW-1.18.1%20%2F%201.12.1-orange.svg)](https://turtle-wow.org/)
[![Engine](https://img.shields.io/badge/Core-vMaNGOS%20(tortoise--wow)-blue.svg)]()
[![PlayerBots](https://img.shields.io/badge/AI-TortoiseBots%20Compatible-green.svg)]()
[![License](https://img.shields.io/badge/License-GPL%20v2-red.svg)](LICENSE)

---

## ⚔️ Overview

**`mod-twow-invasions`** brings the tension and thrill of Dark Souls asynchronous world invasions to Vanilla WoW. Whether initiated by a real player or an autonomous wandering PlayerBot, combatants channel a 6-second fiery rite, lock their PvP flags, and teleport directly behind their quarry anywhere across Azeroth's open world.

The hunt is balanced, non-intrusive, and thrilling:
1. **The Stalking Warning**: The defender receives an ominous screen prompt (*"Something has come to forgive you"*) 5 seconds before the invader lands, allowing a moment to prepare.
2. **The Ambush**: The invader teleports **10 yards directly behind** the defender with a dark visual effect (`ARRIVE_KIT`), while stealth is preserved for Rogues and Druids.
3. **The 100-Yard Arena Leash**: An invisible duel boundary forms around the arrival point. Fleeing further than 100 yards triggers a 10-second countdown to return or forfeit.
4. **Asymmetric Death**: A defeated defender revives over their corpse without running from a distant graveyard; a fallen invader is carried home to their starting location dead and automatically resurrected.

---

## ✨ Key Features

- **🔥 Autolearned Spell Mode (Default)**: Automatically unlocked directly in the Spellbook (General tab) upon reaching level 10 (`Invasion.MinLevel`). No inventory clutter or bag space needed.
- **📜 Optional Physical Talisman**: Configurable option to enable the physical *Invader's Sigil* (Item `100035`) and gossip vendor NPC *Mordrin the Marked* (`100031`) for classic roleplay feel.
- **🤖 Autonomous PlayerBot Phantom Invasions**:
  - **Bot $\rightarrow$ Player**: Wandering PlayerBots can channel the rite and invade real players like Dark Souls NPC Phantoms (*Kirk*, *Mildred*).
  - **Player $\rightarrow$ Bot**: When no real human players are flagged or online, matchmaking falls back to roaming PlayerBots so the rite never fails.
  - **Bot $\rightarrow$ Bot**: Autonomous simulated world PvP encounters across factions.
  - **Balanced Pacing**: Configurable check intervals (`Invasion.BotInvasionInterval`), roll chances (`Invasion.BotInvasionChance`), personal bot cooldowns, and realm-wide active invasion caps.
- **🛡️ Zero Core Patches (The Unediting Doctrine)**:
  - 100% standalone C++ module operating entirely through `ScriptObjects.h` lifecycle callbacks (`PlayerScript`, `WorldScript`, `UnitScript`, `ItemScript`).
  - Cross-faction and sanctuary town hostility is enforced via `SetFFAPvP(true)` during the match, cleanly reverting on conclusion without modifying core `GetReactionTo`.
- **📡 Minimap Radar Tracking**: Real-time positional radar updates transmitted via the `CINV` addon channel every 250ms, rendering skull tracking dots on compatible client addons.
- **🏆 Standard PvP Rewards**: Victorious invaders and defenders receive standard PvP Honor points and Gold directly into their purse.
- **🕊️ Anti-Grief Respite Timer**: Invaded defenders receive an immunity cooldown (default: 10 minutes) preventing back-to-back harassment.

---

## ⚙️ Configuration Reference (`mod-twow-invasions.conf`)

```ini
[ModuleConf]
# Master switch
Invasion.Enable = 1
Invasion.Debug = 0

# Access Modes: 1 = Spell Only (Default), 2 = Item Only, 3 = Hybrid
Invasion.AccessMode = 1
Invasion.MinLevel = 10
Invasion.SpellId = 51942
Invasion.EnableTalisman = 0

# Matchmaking & Rules
Invasion.LevelRange = 3
Invasion.WarnSeconds = 5
Invasion.DurationSeconds = 300
Invasion.RespiteSeconds = 600
Invasion.ArriveDistance = 10
Invasion.ReturnSeconds = 5
Invasion.DeathLingerSeconds = 10
Invasion.ReviveSeconds = 3

# Rewards
Invasion.RewardHonor = 75
Invasion.RewardMoney = 5000

# PlayerBot Phantoms Matrix
Invasion.AllowPlayerToBot = 1
Invasion.AllowBotToPlayer = 1
Invasion.AllowBotToBot = 1
Invasion.BotInvasionInterval = 300
Invasion.BotInvasionChance = 30
Invasion.BotCooldownSeconds = 600
Invasion.BotMaxActiveInvasions = 3
```

---

## 🚀 Installation

1. Clone or copy `mod-twow-invasions` into your core's module directory:
   ```bash
   git clone https://github.com/Ildourol/mod-twow-invasions.git src/modules/mod-twow-invasions
   ```
2. Apply database migrations:
   - `data/sql/character/001_invasion_match_history.sql` $\rightarrow$ `characters` database.
   - `data/sql/world/001_invasion_spell_and_item.sql` $\rightarrow$ `world` database.
3. Copy `conf/mod-twow-invasions.conf.dist` to your server config folder as `mod-twow-invasions.conf` and adjust settings as desired.
4. Re-run CMake and compile your server:
   ```bash
   cmake -B build
   cmake --build build --target mangosd
   ```

---

## 📄 License & Credits

- Licensed under **GPL v2**.
- Designed for **Turtle WoW** and the **vMaNGOS** community.
- Original invasion concept inspired by Dark Souls asynchronous multiplayer.
