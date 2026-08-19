-- spellforge generated character cleanup (characters DB)
-- generated: 2026-08-19T14:23:00Z  commit: n/a
-- DO NOT EDIT — regenerate with `sf build`

DELETE FROM `character_spell`  WHERE `spell` IN (200000);
DELETE FROM `character_action` WHERE `action` IN (200000) AND `type` = 0;
