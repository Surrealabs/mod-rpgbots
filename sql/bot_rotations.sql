-- ============================================================================
-- RPGBots: Data-Driven Rotation System
-- ============================================================================
-- All bot combat behavior is defined here. No recompile needed to change
-- rotations — just edit these rows and `.rpg reload` in-game.
-- ============================================================================

-- ─── Rotation Entries Table ────────────────────────────────────────────────────
-- Each row is one spell in a class/spec's priority list.
-- The bot scans entries top-down (lowest priority_order first) and casts the
-- first spell whose conditions are met.

DROP TABLE IF EXISTS `rpgbots`.`bot_rotation_entries`;
CREATE TABLE `rpgbots`.`bot_rotation_entries` (
    `id`              INT UNSIGNED NOT NULL AUTO_INCREMENT,
    `class_id`        TINYINT UNSIGNED NOT NULL COMMENT 'Player class (1=Warrior … 9=Warlock)',
    `spec_index`      TINYINT UNSIGNED NOT NULL COMMENT 'Talent tree index (0/1/2)',
    `category`        ENUM('rotation','maintenance','defensive','utility') NOT NULL DEFAULT 'rotation'
        COMMENT 'Which priority bucket this spell belongs to',
    `priority_order`  SMALLINT UNSIGNED NOT NULL DEFAULT 0
        COMMENT 'Lower = higher priority. Scanned top-down within each category.',
    `spell_id`        MEDIUMINT UNSIGNED NOT NULL COMMENT 'The spell ID to cast',
    `spell_name`      VARCHAR(64) NOT NULL DEFAULT '' COMMENT 'Human-readable label (for editors)',
    `target_type`     ENUM('enemy','self','friendly_lowest_hp','friendly_tank','pet') NOT NULL DEFAULT 'enemy'
        COMMENT 'Who the bot casts this spell on',
    `condition_type`  ENUM(
        'none',
        'health_below',
        'health_above',
        'mana_below',
        'mana_above',
        'target_health_below',
        'target_health_above',
        'has_aura',
        'missing_aura',
        'target_has_aura',
        'target_missing_aura',
        'in_melee_range',
        'not_in_melee_range'
    ) NOT NULL DEFAULT 'none'
        COMMENT 'Optional condition that must be true for this entry to fire',
    `condition_value`  INT NOT NULL DEFAULT 0
        COMMENT 'Threshold for the condition (pct for health/mana, spell_id for aura checks)',
    `enabled`         TINYINT UNSIGNED NOT NULL DEFAULT 1 COMMENT '0 = skip this row',
    PRIMARY KEY (`id`),
    INDEX `idx_class_spec` (`class_id`, `spec_index`, `category`, `priority_order`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='Data-driven bot rotation priorities';

-- ─── Rotation Metadata Table ──────────────────────────────────────────────────
-- One row per class/spec. Defines the spec name, role, preferred range, etc.

DROP TABLE IF EXISTS `rpgbots`.`bot_rotation_specs`;
CREATE TABLE `rpgbots`.`bot_rotation_specs` (
    `class_id`        TINYINT UNSIGNED NOT NULL,
    `spec_index`      TINYINT UNSIGNED NOT NULL,
    `spec_name`       VARCHAR(32) NOT NULL DEFAULT '',
    `role`            ENUM('tank','healer','melee_dps','ranged_dps') NOT NULL DEFAULT 'melee_dps',
    `preferred_range` FLOAT NOT NULL DEFAULT 0 COMMENT 'Yards: 0 = melee, 30 = caster',
    `description`     TEXT COMMENT 'AI behavior notes for this spec',
    PRIMARY KEY (`class_id`, `spec_index`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='Spec metadata for bot rotation system';


-- ============================================================================
-- PALADIN (class_id = 2)
-- ============================================================================

-- ── Spec Metadata ──────────────────────────────────────────────────────────────

INSERT INTO `rpgbots`.`bot_rotation_specs` VALUES
(2, 0, 'Holy',        'healer',     30, 'Beacon tank, triage heals, mana management'),
(2, 1, 'Protection',  'tank',        0, 'AoE threat, maintain Holy Shield + Righteous Fury'),
(2, 2, 'Retribution', 'melee_dps',   0, 'FCFS priority, Crusader Strike + Divine Storm core');

-- ── Holy Paladin (2,0) ─────────────────────────────────────────────────────────

INSERT INTO `rpgbots`.`bot_rotation_entries`
    (`class_id`,`spec_index`,`category`,`priority_order`,`spell_id`,`spell_name`,`target_type`,`condition_type`,`condition_value`) VALUES
-- maintenance: keep buffs rolling
(2, 0, 'maintenance', 10, 19746,  'Concentration Aura',     'self', 'missing_aura', 19746),
(2, 0, 'maintenance', 20, 20166,  'Seal of Wisdom',         'self', 'missing_aura', 20166),
(2, 0, 'maintenance', 30, 53563,  'Beacon of Light',        'friendly_tank', 'target_missing_aura', 53563),
(2, 0, 'maintenance', 40, 53601,  'Sacred Shield',          'friendly_tank', 'target_missing_aura', 53601),
(2, 0, 'maintenance', 50, 20217,  'Blessing of Kings',      'self', 'missing_aura', 20217),
-- defensive: emergency
(2, 0, 'defensive',   10, 48788,  'Lay on Hands',           'friendly_lowest_hp', 'target_health_below', 15),
(2, 0, 'defensive',   20, 54428,  'Divine Plea',            'self', 'mana_below', 30),
(2, 0, 'defensive',   30, 31842,  'Divine Illumination',    'self', 'mana_below', 40),
-- rotation: healing priority
(2, 0, 'rotation',    10, 48825,  'Holy Shock',             'friendly_lowest_hp', 'target_health_below', 80),
(2, 0, 'rotation',    20, 48785,  'Flash of Light',         'friendly_lowest_hp', 'target_health_below', 75),
(2, 0, 'rotation',    30, 48782,  'Holy Light',             'friendly_lowest_hp', 'target_health_below', 50),
(2, 0, 'rotation',    40, 20216,  'Divine Favor',           'self', 'none', 0),
(2, 0, 'rotation',    50, 53408,  'Judgement of Wisdom',    'enemy', 'none', 0),
-- utility
(2, 0, 'utility',     10, 4987,   'Cleanse',                'friendly_lowest_hp', 'none', 0);

-- ── Protection Paladin (2,1) ───────────────────────────────────────────────────

INSERT INTO `rpgbots`.`bot_rotation_entries`
    (`class_id`,`spec_index`,`category`,`priority_order`,`spell_id`,`spell_name`,`target_type`,`condition_type`,`condition_value`) VALUES
-- maintenance
(2, 1, 'maintenance', 10, 25780,  'Righteous Fury',         'self', 'missing_aura', 25780),
(2, 1, 'maintenance', 20, 48942,  'Devotion Aura',          'self', 'missing_aura', 48942),
(2, 1, 'maintenance', 30, 31801,  'Seal of Vengeance',      'self', 'missing_aura', 31801),
(2, 1, 'maintenance', 40, 20911,  'Blessing of Sanctuary',  'self', 'missing_aura', 20911),
-- defensive
(2, 1, 'defensive',   10, 498,    'Divine Protection',      'self', 'health_below', 30),
(2, 1, 'defensive',   20, 54428,  'Divine Plea',            'self', 'mana_below', 25),
-- rotation: threat
(2, 1, 'rotation',    10, 62124,  'Hand of Reckoning',      'enemy', 'none', 0),
(2, 1, 'rotation',    20, 61411,  'Shield of Righteousness', 'enemy', 'none', 0),
(2, 1, 'rotation',    30, 53595,  'Hammer of the Righteous', 'enemy', 'none', 0),
(2, 1, 'rotation',    40, 48952,  'Holy Shield',            'self', 'missing_aura', 48952),
(2, 1, 'rotation',    50, 48827,  'Avenger\'s Shield',      'enemy', 'none', 0),
(2, 1, 'rotation',    60, 48819,  'Consecration',           'self', 'none', 0),
(2, 1, 'rotation',    70, 53408,  'Judgement of Wisdom',    'enemy', 'none', 0),
-- utility
(2, 1, 'utility',     10, 10308,  'Hammer of Justice',      'enemy', 'none', 0);

-- ── Retribution Paladin (2,2) ──────────────────────────────────────────────────

INSERT INTO `rpgbots`.`bot_rotation_entries`
    (`class_id`,`spec_index`,`category`,`priority_order`,`spell_id`,`spell_name`,`target_type`,`condition_type`,`condition_value`) VALUES
-- maintenance
(2, 2, 'maintenance', 10, 54043,  'Retribution Aura',       'self', 'missing_aura', 54043),
(2, 2, 'maintenance', 20, 20375,  'Seal of Command',        'self', 'missing_aura', 20375),
(2, 2, 'maintenance', 30, 48934,  'Blessing of Might',      'self', 'missing_aura', 48934),
-- rotation: FCFS
(2, 2, 'rotation',    10, 48806,  'Hammer of Wrath',        'enemy', 'target_health_below', 20),
(2, 2, 'rotation',    20, 35395,  'Crusader Strike',        'enemy', 'none', 0),
(2, 2, 'rotation',    30, 53385,  'Divine Storm',           'enemy', 'none', 0),
(2, 2, 'rotation',    40, 53408,  'Judgement of Wisdom',    'enemy', 'none', 0),
(2, 2, 'rotation',    50, 48819,  'Consecration',           'self', 'none', 0),
(2, 2, 'rotation',    60, 48801,  'Exorcism',               'enemy', 'none', 0),
(2, 2, 'rotation',    70, 31884,  'Avenging Wrath',         'self', 'none', 0);


-- ============================================================================
-- WARLOCK (class_id = 9)
-- ============================================================================

INSERT INTO `rpgbots`.`bot_rotation_specs` VALUES
(9, 0, 'Affliction',   'ranged_dps', 30, 'Multi-DoT, Haunt windows, Drain Soul execute'),
(9, 1, 'Demonology',   'ranged_dps', 30, 'Pet-centric, Metamorphosis burst, Shadow Bolt filler'),
(9, 2, 'Destruction',  'ranged_dps', 30, 'Immolate uptime, Conflagrate procs, Chaos Bolt on CD');

-- ── Affliction Warlock (9,0) ───────────────────────────────────────────────────

INSERT INTO `rpgbots`.`bot_rotation_entries`
    (`class_id`,`spec_index`,`category`,`priority_order`,`spell_id`,`spell_name`,`target_type`,`condition_type`,`condition_value`) VALUES
-- maintenance
(9, 0, 'maintenance', 10, 47893,  'Fel Armor',              'self', 'missing_aura', 47893),
-- defensive
(9, 0, 'defensive',   10, 47860,  'Death Coil',             'enemy', 'health_below', 25),
(9, 0, 'defensive',   20, 29858,  'Soul Shatter',           'self', 'none', 0),
(9, 0, 'defensive',   30, 57946,  'Life Tap',               'self', 'mana_below', 20),
-- rotation: DoT priority
(9, 0, 'rotation',    10, 59164,  'Haunt',                  'enemy', 'none', 0),
(9, 0, 'rotation',    20, 47813,  'Corruption',             'enemy', 'target_missing_aura', 47813),
(9, 0, 'rotation',    30, 47843,  'Unstable Affliction',    'enemy', 'target_missing_aura', 47843),
(9, 0, 'rotation',    40, 47864,  'Curse of Agony',         'enemy', 'target_missing_aura', 47864),
(9, 0, 'rotation',    50, 47855,  'Drain Soul',             'enemy', 'target_health_below', 25),
(9, 0, 'rotation',    60, 47809,  'Shadow Bolt',            'enemy', 'none', 0),
(9, 0, 'rotation',    70, 47836,  'Seed of Corruption',     'enemy', 'none', 0);

-- ── Demonology Warlock (9,1) ───────────────────────────────────────────────────

INSERT INTO `rpgbots`.`bot_rotation_entries`
    (`class_id`,`spec_index`,`category`,`priority_order`,`spell_id`,`spell_name`,`target_type`,`condition_type`,`condition_value`) VALUES
-- maintenance
(9, 1, 'maintenance', 10, 47893,  'Fel Armor',              'self', 'missing_aura', 47893),
-- defensive
(9, 1, 'defensive',   10, 57946,  'Life Tap',               'self', 'mana_below', 25),
(9, 1, 'defensive',   20, 59092,  'Dark Pact',              'self', 'mana_below', 35),
-- rotation
(9, 1, 'rotation',    10, 47241,  'Metamorphosis',          'self', 'none', 0),
(9, 1, 'rotation',    20, 50589,  'Immolation Aura',        'self', 'has_aura', 47241),
(9, 1, 'rotation',    30, 47193,  'Demonic Empowerment',    'pet', 'none', 0),
(9, 1, 'rotation',    40, 47813,  'Corruption',             'enemy', 'target_missing_aura', 47813),
(9, 1, 'rotation',    50, 47811,  'Immolate',               'enemy', 'target_missing_aura', 47811),
(9, 1, 'rotation',    60, 47867,  'Curse of Doom',          'enemy', 'target_missing_aura', 47867),
(9, 1, 'rotation',    70, 47825,  'Soul Fire',              'enemy', 'target_health_below', 35),
(9, 1, 'rotation',    80, 47809,  'Shadow Bolt',            'enemy', 'none', 0);

-- ── Destruction Warlock (9,2) ──────────────────────────────────────────────────

INSERT INTO `rpgbots`.`bot_rotation_entries`
    (`class_id`,`spec_index`,`category`,`priority_order`,`spell_id`,`spell_name`,`target_type`,`condition_type`,`condition_value`) VALUES
-- maintenance
(9, 2, 'maintenance', 10, 47893,  'Fel Armor',              'self', 'missing_aura', 47893),
-- defensive
(9, 2, 'defensive',   10, 47847,  'Shadowfury',             'enemy', 'health_below', 30),
(9, 2, 'defensive',   20, 57946,  'Life Tap',               'self', 'mana_below', 20),
-- rotation
(9, 2, 'rotation',    10, 47811,  'Immolate',               'enemy', 'target_missing_aura', 47811),
(9, 2, 'rotation',    20, 17962,  'Conflagrate',            'enemy', 'target_has_aura', 47811),
(9, 2, 'rotation',    30, 59172,  'Chaos Bolt',             'enemy', 'none', 0),
(9, 2, 'rotation',    40, 47865,  'Curse of the Elements',  'enemy', 'target_missing_aura', 47865),
(9, 2, 'rotation',    50, 47813,  'Corruption',             'enemy', 'target_missing_aura', 47813),
(9, 2, 'rotation',    60, 47827,  'Shadowburn',             'enemy', 'target_health_below', 20),
(9, 2, 'rotation',    70, 47838,  'Incinerate',             'enemy', 'none', 0),
(9, 2, 'rotation',    80, 47820,  'Rain of Fire',           'enemy', 'none', 0);
