-- Table: rpg_psychology
-- Defines global psychological multipliers and their effects

CREATE TABLE IF NOT EXISTS `rpg_psychology` (
    `id` INT UNSIGNED NOT NULL AUTO_INCREMENT,
    `name` VARCHAR(64) NOT NULL,                -- Name of the psychological type (e.g., Insane, Stoic)
    `spell` VARCHAR(128) DEFAULT NULL,          -- Script or spell associated with this psychology
    `comment` TEXT DEFAULT NULL,                -- Description or notes
    PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='Psychological types and multipliers for RPG bots';
