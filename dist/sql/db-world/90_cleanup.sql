-- spellforge generated world cleanup for retired spells
-- generated: 2026-08-19T14:23:00Z  commit: n/a
-- DO NOT EDIT — regenerate with `sf build`

DELETE FROM `spell_bonus_data`    WHERE `entry`    IN (200000);
DELETE FROM `trainer_spell`       WHERE `SpellId`  IN (200000);
DELETE FROM `spell_proc`          WHERE `SpellId`  IN (200000);
DELETE FROM `spell_script_names`  WHERE `spell_id` IN (200000);
DELETE FROM `spell_ranks`         WHERE `spell_id` IN (200000) OR `first_spell_id` IN (200000);
DELETE FROM `spell_dbc`           WHERE `Id`       IN (200000);
