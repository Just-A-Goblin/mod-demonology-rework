-- Vital Conduit (vc): bind our SpellScript to the Life Tap chain so Life Tap also heals the
-- commanded demons. The script only acts for a warlock with the vc talent, so other Life Taps
-- are unaffected. Idempotent. Ranks: 1454,1455,1456,11687,11688,11689,27222,57946.

DELETE FROM `spell_script_names`
 WHERE `spell_id` IN (1454,1455,1456,11687,11688,11689,27222,57946)
   AND `ScriptName` = 'spell_demonology_life_tap_conduit';

INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
 (1454,  'spell_demonology_life_tap_conduit'),
 (1455,  'spell_demonology_life_tap_conduit'),
 (1456,  'spell_demonology_life_tap_conduit'),
 (11687, 'spell_demonology_life_tap_conduit'),
 (11688, 'spell_demonology_life_tap_conduit'),
 (11689, 'spell_demonology_life_tap_conduit'),
 (27222, 'spell_demonology_life_tap_conduit'),
 (57946, 'spell_demonology_life_tap_conduit');
