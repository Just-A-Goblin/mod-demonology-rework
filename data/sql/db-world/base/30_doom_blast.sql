-- Doom Blast (core spell 40878) is the AOE hit our Doomguard's Doom Bolt triggers. Bind
-- our SpellScript so we can cap its damage (it stock-hits ~1.5k). The script only alters
-- it when cast by a warlock-owned demon, so other sources of 40878 are unaffected. Idempotent.

DELETE FROM `spell_script_names`
 WHERE `spell_id` = 40878 AND `ScriptName` = 'spell_demonology_doom_blast';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`)
VALUES (40878, 'spell_demonology_doom_blast');
