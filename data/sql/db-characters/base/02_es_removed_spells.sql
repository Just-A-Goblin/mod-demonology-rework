-- Tracks the vanilla greater-demon summons (Inferno / Ritual of Doom) that Eternal
-- Servitude removed from a character's spellbook, so they can be RESTORED (only the ones
-- we removed) when the character respecs out of ES. These are quest-learned spells that
-- can't be re-trained, so we must never grant them — only give back what we took.
CREATE TABLE IF NOT EXISTS `character_demonology_es_removed` (
  `guid`  INT UNSIGNED NOT NULL,
  `spell` INT UNSIGNED NOT NULL,
  PRIMARY KEY (`guid`, `spell`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4
  COMMENT='Vanilla Inferno/Ritual of Doom removed by Eternal Servitude, restored on respec';
