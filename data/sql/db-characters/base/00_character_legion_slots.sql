-- mod-demonology-rework — persistent command-pool occupants (PLAN §3.4).
-- Restored on login via a short delayed event; cleared on talent reset when
-- max slots drop.
CREATE TABLE IF NOT EXISTS `character_legion_slots` (
  `guid`            INT UNSIGNED   NOT NULL,
  `slot`            TINYINT UNSIGNED NOT NULL,
  `creature_entry`  INT UNSIGNED   NOT NULL,
  `saved_health_pct` FLOAT         NOT NULL DEFAULT 1,
  PRIMARY KEY (`guid`, `slot`)
);
