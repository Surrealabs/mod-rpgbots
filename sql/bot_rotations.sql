-- ============================================================================
-- RPGBots: Flat Rotation Table — one row per spec, 20 spell columns
-- ============================================================================
-- Dead simple.  Each spec gets ONE row with four buckets of 5 spells each.
-- The C++ AI is role-aware: it knows who to target based on the role column.
--
--   ability_1..5  — Core rotation.  Healer: cast on lowest-HP ally (<90%).
--                   DPS/Tank: cast on master's target (priority order).
--   buff_1..5     — Self-buffs.  Cast on self if the aura is missing.
--   defensive_1..5 — Emergency.  Cast on self when HP drops below 35%.
--   mobility_1..5 — Positioning. Cast when out of preferred range.
--
-- To change a rotation: UPDATE one row, then `.rpg reload` in-game.
-- To add a new class: INSERT one row per spec.  No recompile.
-- ============================================================================

DROP TABLE IF EXISTS `rpgbots`.`bot_rotations`;
CREATE TABLE `rpgbots`.`bot_rotations` (
    `class_id`        TINYINT UNSIGNED NOT NULL  COMMENT '1=War 2=Pal 3=Hunt 4=Rog 5=Pri 6=DK 7=Sha 8=Mag 9=Lock 11=Dru',
    `spec_index`      TINYINT UNSIGNED NOT NULL  COMMENT 'Talent tree 0/1/2',
    `spec_name`       VARCHAR(32)  NOT NULL DEFAULT '',
    `role`            ENUM('tank','healer','melee_dps','ranged_dps') NOT NULL DEFAULT 'melee_dps',
    `preferred_range` FLOAT        NOT NULL DEFAULT 0  COMMENT '0 = melee, 30 = caster',

    -- Bucket 1: Core abilities (priority 1 = highest)
    `ability_1`       MEDIUMINT UNSIGNED NOT NULL DEFAULT 0,
    `ability_2`       MEDIUMINT UNSIGNED NOT NULL DEFAULT 0,
    `ability_3`       MEDIUMINT UNSIGNED NOT NULL DEFAULT 0,
    `ability_4`       MEDIUMINT UNSIGNED NOT NULL DEFAULT 0,
    `ability_5`       MEDIUMINT UNSIGNED NOT NULL DEFAULT 0,

    -- Bucket 2: Self-buffs (cast if aura missing)
    `buff_1`          MEDIUMINT UNSIGNED NOT NULL DEFAULT 0,
    `buff_2`          MEDIUMINT UNSIGNED NOT NULL DEFAULT 0,
    `buff_3`          MEDIUMINT UNSIGNED NOT NULL DEFAULT 0,
    `buff_4`          MEDIUMINT UNSIGNED NOT NULL DEFAULT 0,
    `buff_5`          MEDIUMINT UNSIGNED NOT NULL DEFAULT 0,

    -- Bucket 3: Defensives (cast when HP < 35%)
    `defensive_1`     MEDIUMINT UNSIGNED NOT NULL DEFAULT 0,
    `defensive_2`     MEDIUMINT UNSIGNED NOT NULL DEFAULT 0,
    `defensive_3`     MEDIUMINT UNSIGNED NOT NULL DEFAULT 0,
    `defensive_4`     MEDIUMINT UNSIGNED NOT NULL DEFAULT 0,
    `defensive_5`     MEDIUMINT UNSIGNED NOT NULL DEFAULT 0,

    -- Bucket 4: Mobility (cast when out of range)
    `mobility_1`      MEDIUMINT UNSIGNED NOT NULL DEFAULT 0,
    `mobility_2`      MEDIUMINT UNSIGNED NOT NULL DEFAULT 0,
    `mobility_3`      MEDIUMINT UNSIGNED NOT NULL DEFAULT 0,
    `mobility_4`      MEDIUMINT UNSIGNED NOT NULL DEFAULT 0,
    `mobility_5`      MEDIUMINT UNSIGNED NOT NULL DEFAULT 0,

    PRIMARY KEY (`class_id`, `spec_index`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='One row per spec — 20 spells, 4 buckets';


-- ============================================================================
-- PALADIN (class_id = 2)
-- ============================================================================

INSERT INTO `rpgbots`.`bot_rotations` VALUES
-- Holy Paladin (healer, 30yd range)
-- abilities: heal spells cast on lowest-HP party member under 90%
-- buffs: self-buffs kept rolling
-- defensives: oh-shit buttons when bot HP < 35%
-- mobility: none for holy (0 = skip)
(2, 0, 'Holy', 'healer', 30,
    48825, 48785, 48782, 53563, 53601,   -- Holy Shock, Flash of Light, Holy Light, Beacon, Sacred Shield
    19746, 20166, 20217, 0, 0,           -- Concentration Aura, Seal of Wisdom, Blessing of Kings
    48788, 498, 54428, 0, 0,             -- Lay on Hands, Divine Protection, Divine Plea
    0, 0, 0, 0, 0),

-- Protection Paladin (tank, melee)
-- abilities: threat spells cast on enemy target
(2, 1, 'Protection', 'tank', 0,
    61411, 53595, 48827, 48819, 53408,   -- Shield of Right, Hammer of Right, Avenger Shield, Consecration, Judge
    25780, 48942, 31801, 48952, 0,       -- Righteous Fury, Devotion Aura, Seal of Vengeance, Holy Shield
    498, 642, 54428, 0, 0,               -- Divine Protection, Divine Shield, Divine Plea
    62124, 0, 0, 0, 0),                  -- Hand of Reckoning (taunt / gap closer)

-- Retribution Paladin (melee DPS)
-- abilities: FCFS priority on enemy
(2, 2, 'Retribution', 'melee_dps', 0,
    35395, 53385, 53408, 48806, 48801,   -- Crusader Strike, Divine Storm, Judgement, Hammer of Wrath, Exorcism
    54043, 20375, 48934, 0, 0,           -- Retribution Aura, Seal of Command, Blessing of Might
    498, 54428, 0, 0, 0,                 -- Divine Protection, Divine Plea
    0, 0, 0, 0, 0);


-- ============================================================================
-- WARLOCK (class_id = 9)
-- ============================================================================

INSERT INTO `rpgbots`.`bot_rotations` VALUES
-- Affliction (ranged DPS, 30yd)
-- abilities: DoTs then filler, cast on master's target
(9, 0, 'Affliction', 'ranged_dps', 30,
    59164, 47813, 47843, 47864, 47809,   -- Haunt, Corruption, Unstable Aff, Curse of Agony, Shadow Bolt
    47893, 0, 0, 0, 0,                   -- Fel Armor
    47860, 29858, 57946, 0, 0,           -- Death Coil, Soul Shatter, Life Tap
    0, 0, 0, 0, 0),

-- Demonology (ranged DPS, 30yd)
(9, 1, 'Demonology', 'ranged_dps', 30,
    47241, 47193, 47813, 47811, 47809,   -- Metamorphosis, Demonic Emp, Corruption, Immolate, Shadow Bolt
    47893, 0, 0, 0, 0,                   -- Fel Armor
    47860, 57946, 59092, 0, 0,           -- Death Coil, Life Tap, Dark Pact
    0, 0, 0, 0, 0),

-- Destruction (ranged DPS, 30yd)
(9, 2, 'Destruction', 'ranged_dps', 30,
    47811, 17962, 59172, 47838, 47813,   -- Immolate, Conflagrate, Chaos Bolt, Incinerate, Corruption
    47893, 0, 0, 0, 0,                   -- Fel Armor
    47847, 47860, 57946, 0, 0,           -- Shadowfury, Death Coil, Life Tap
    0, 0, 0, 0, 0);
