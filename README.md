# mod-rpgbots

An "Army of Alts" RPG Framework for AzerothCore

---

## Table of Contents
- [Project Overview](#project-overview)
- [Core Philosophy](#core-philosophy)
- [Technical Architecture](#technical-architecture)
  - [Data Layer (SQL)](#data-layer-sql)
  - [Integration Strategy](#integration-strategy)
- [The "2K" Stat System](#the-2k-stat-system)
- [Project Roadmap](#project-roadmap)

---

## Project Overview
mod-rpgbots transforms player alts from static bots into persistent, psychological entities. Every character on the server—whether played by a human or a bot—carries a unique profile consisting of Affixes, Temperaments, and Psychological Types.

---

## Core Philosophy
- **Consistency over Perfection:** Bots don't just "play perfectly." They have a "2K-style" shot meter for mechanics and rotations.
- **Persistent Growth:** Alts earn XP in specific fields (Mechanics, Rotation, Heroism) to improve their performance over time.
- **Psychological Depth:** Traits are active even when a human logs in, creating a "Character Identity" system that transcends AI.

---

## Technical Architecture

### Data Layer (SQL)
The system utilizes three primary tables to handle the "Mind" of the character:
- **character_rpg_data:** The "Live" table. Stores GUID-linked scores for Mechanics, Rotation, and Heroism, along with their respective XP and current IDs for Temperament/Psych.
- **rpg_temperaments:** The "Trait Library." Defines fixed archetypes (e.g., Giga-Pumper, Loudmouth, Ancient) with static stat modifiers and script hooks.
- **rpg_psychology:** The "Neuro-Layer." Defines global multipliers (e.g., Insane, Stoic) that modify how the base stats are calculated during a roll.

### Integration Strategy
- **Minimal Core Edit (Conditional):** Uses a `#ifdef MODULE_RPG_BOTS` wrapper in `WorldSession.cpp` to allow "Socketless Sessions," enabling alts to load without a real client connection.
- **Hybrid Logic:** Logic is handled via PlayerScript hooks, ensuring that personality-driven events (like "Loudmouth" chat triggers) occur regardless of whether the character is a bot or a player.

---

## The "2K" Stat System

| Primary Affix | Stat Impact | Failure State |
|--------------|-------------|--------------|
| Mechanics    | % Chance to correctly dodge AOE/Stack. | Bot stands in fire or fails to move. |
| Rotation     | % Chance to cast the optimal spell.    | Bot uses a suboptimal filler or misses a DoT. |
| Heroism      | % Chance to save a dying ally (LoH, Brez). | Bot ignores the crisis to keep DPSing. |

### The Psychological Filter
- **Neurotypical:** 1.0x Multiplier (Reliable).
- **Insane:** 0.5x - 1.5x Multiplier (High peaks of brilliance, deep valleys of failure).
- **Stoic:** Immune to "Salt" (wipes), but gains XP 20% slower.

---

## Project Roadmap

### Phase 1: Foundations (The "Empty" Shell)
- [ ] Implement `character_rpg_data` SQL schema.
- [ ] Add the "Dummy Session" core edit to allow alt loading.
- [ ] Create basic `.army spawn` command to pull account-linked alts.

### Phase 2: The Logic Engine
- [ ] Build the RollSuccess calculator (`Stat * Psych + Temp`).
- [ ] Integrate "Rotation Misfire" logic into the Bot AI loop.
- [ ] Create the "Heroism Scanner" for emergency cooldown usage.

### Phase 3: Personality & Flavour
- [ ] Implement the "Loudmouth" chat system (chirping after wipes).
- [ ] Add the "Salt" mechanic (performance degradation after repeated deaths).
- [ ] Create the "Scroll of Personality" (The Gacha Roll system for Rarity: Common -> Ancient).

---