-- Table: rpg_temperaments
-- Defines fixed archetypes with stat modifiers and script hooks

CREATE TABLE IF NOT EXISTS `rpg_temperaments` (
    `id` INT UNSIGNED NOT NULL AUTO_INCREMENT,
    `name` VARCHAR(64) NOT NULL,                -- Name of the temperament (e.g., Giga-Pumper)
    `script` VARCHAR(128) DEFAULT NULL,         -- Script or spell associated with this temperament
    `comment` TEXT DEFAULT NULL,                -- Description or notes
    PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='Temperament archetypes for RPG bots';
