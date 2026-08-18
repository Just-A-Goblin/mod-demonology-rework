-- Wild Imp (600000) — the guardian summoned by Summon Wild Imps.
-- Cloned from the classic Imp (416) so every NOT NULL column is satisfied
-- regardless of schema drift; then a few fields are overridden. Model rows live
-- in creature_template_model on modern AzerothCore, so they are cloned too.

DELETE FROM `creature_template` WHERE `entry` = 600000;
CREATE TEMPORARY TABLE `_wild_imp` AS SELECT * FROM `creature_template` WHERE `entry` = 416;
-- Note: modern AzerothCore creature_template has no spell1..spell8 columns
-- (creature spells live elsewhere). Slice Wild Imps melee; Firebolt casting via
-- 290900 is wired in Phase 3.
UPDATE `_wild_imp`
   SET `entry` = 600000,
       `name` = 'Wild Imp',
       `subname` = NULL,
       `AIName` = '',
       `ScriptName` = '';
INSERT INTO `creature_template` SELECT * FROM `_wild_imp`;
DROP TEMPORARY TABLE `_wild_imp`;

DELETE FROM `creature_template_model` WHERE `CreatureID` = 600000;
INSERT INTO `creature_template_model`
    (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`, `VerifiedBuild`)
SELECT 600000, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`, `VerifiedBuild`
  FROM `creature_template_model` WHERE `CreatureID` = 416;
