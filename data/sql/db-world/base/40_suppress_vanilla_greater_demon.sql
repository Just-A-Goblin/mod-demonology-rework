-- Eternal Servitude suppresses the vanilla temporary greater-demon summons so an ES
-- warlock doesn't pair a permanent Infernal/Doomguard with a fresh temporary one. Binds
-- our CheckCast-only SpellScript to Inferno (1122) and Ritual of Doom (18540); it only
-- blocks the cast when the caster has the ES talent, so non-ES warlocks are unaffected.

DELETE FROM `spell_script_names`
 WHERE `spell_id` IN (1122, 18540) AND `ScriptName` = 'spell_demonology_suppress_vanilla_greater_demon';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
 (1122,  'spell_demonology_suppress_vanilla_greater_demon'),
 (18540, 'spell_demonology_suppress_vanilla_greater_demon');
