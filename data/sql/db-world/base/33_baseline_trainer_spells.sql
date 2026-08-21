-- Baseline Demonology abilities are TRAINER-taught (per user directive 2026-08-20; NOT hybrid-learn,
-- NOT level-auto). Warlock trainers on this server = TrainerId 31 (main, 23 NPCs) + 32 (secondary).
--   Summon Wild Imps (290001)  @ level 10 — the Path B core active.
--   Demonic Empowerment (290000) @ level 50 — matches live WoW's mid-50s.
-- Tab placement is handled separately by their SkillLineAbility (skill 354). Idempotent.
DELETE FROM `trainer_spell` WHERE `SpellId` IN (290000, 290001) AND `TrainerId` IN (31, 32);
INSERT INTO `trainer_spell`
 (`TrainerId`,`SpellId`,`MoneyCost`,`ReqSkillLine`,`ReqSkillRank`,`ReqAbility1`,`ReqAbility2`,`ReqAbility3`,`ReqLevel`,`VerifiedBuild`) VALUES
 (31, 290001,   800, 0, 0, 0, 0, 0, 10, 0),
 (32, 290001,   800, 0, 0, 0, 0, 0, 10, 0),
 (31, 290000, 60000, 0, 0, 0, 0, 0, 50, 0),
 (32, 290000, 60000, 0, 0, 0, 0, 0, 50, 0);
