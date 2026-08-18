-- Gate the base Summon Felguard PET (core spell 30146) behind the Summon Felguard
-- talent. Binds our CheckCast-only SpellScript to 30146; the script aborts the cast
-- (with a message) unless the player has trained the talent. Idempotent.
-- The matching Felguard LEGIONNAIRE (290003) is gated in C++ (CheckCast), no row needed.

DELETE FROM `spell_script_names`
 WHERE `spell_id` = 30146 AND `ScriptName` = 'spell_demonology_gate_summon_felguard';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`)
VALUES (30146, 'spell_demonology_gate_summon_felguard');
