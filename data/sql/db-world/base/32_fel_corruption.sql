-- Fel Corruption (rc): bind our AuraScript to the Corruption chain so its periodic ticks feed
-- Soul Harvest + Doombrand (via LegionEconomy::QualifyPlayerPeriodic) at rank effectiveness. The
-- script only acts for a warlock with the talent, so other Corruptions are unaffected. Seed of
-- Corruption is deliberately NOT bound. Idempotent. Corruption ranks (172 chain).

DELETE FROM `spell_script_names`
 WHERE `spell_id` IN (172,6222,6223,7648,11671,11672,25311,27216,47812,47813)
   AND `ScriptName` = 'spell_demonology_fel_corruption';

INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
 (172,   'spell_demonology_fel_corruption'),
 (6222,  'spell_demonology_fel_corruption'),
 (6223,  'spell_demonology_fel_corruption'),
 (7648,  'spell_demonology_fel_corruption'),
 (11671, 'spell_demonology_fel_corruption'),
 (11672, 'spell_demonology_fel_corruption'),
 (25311, 'spell_demonology_fel_corruption'),
 (27216, 'spell_demonology_fel_corruption'),
 (47812, 'spell_demonology_fel_corruption'),
 (47813, 'spell_demonology_fel_corruption');
