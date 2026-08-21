-- playerbot_talent_check.sql — Phase 1 safety check for the reworked Demonology bot spec.
--
-- After spec'ing ONE demo warlock bot (see docs/BOT_INTEGRATION_ANALYSIS.md / the Phase 1
-- dry-run steps), run this against the CHARACTERS database. It proves the talents the bot
-- actually learned are all valid rows in the shipped Talent.dbc — i.e. no orphaned/renamed
-- talent that would trigger the character_talent crash-loop on the next server load.
--
-- Set @botname to your test bot. The valid-ID list is generated from dist/dbc/Talent.dbc
-- (tab 303, all ranks) by tools/gen_playerbot_spec_links.py's DBC reader — regenerate if the
-- tree changes.  EXPECT: 'INVALID' query returns 0 rows; 'SUMMARY' shows a plausible point total.

SET @botname := 'YOURBOT';

-- (A) CRASH-LOOP GUARD: any learned talent spell NOT in the shipped Demonology tab is an orphan.
SELECT ct.guid, ct.spell AS orphan_spell, '<-- NOT in shipped Talent.dbc tab 303' AS problem
FROM character_talent ct
JOIN characters c ON c.guid = ct.guid
WHERE c.name = @botname
  AND ct.spell NOT IN (
    290010,290011,290012,290902,290903,290904,290905,290906,290907,290908,290909,290910,
    290911,290912,290913,290914,290915,290916,290917,290918,290919,290920,290921,290922,
    290923,290924,290925,290926,290927,290928,290929,290930,290931,290932,290933,290934,
    290935,290936,290937,290938,290939,290940,290941,290942,290943,290944,290945,290946,
    290947,290948,290949,290950,290951,290952,290953,290954,290955,290956,290957,290958,
    290959,290960,290961,290962,290963,290964,290965,290966,290967,290968,290969,290970,
    290971,290972,290973,290974,290975,290976
  );

-- (B) SUMMARY: how many demo talents the bot learned (rows), for a sanity glance vs. expected.
SELECT c.name, COUNT(*) AS demo_talent_rows
FROM character_talent ct
JOIN characters c ON c.guid = ct.guid
WHERE c.name = @botname
GROUP BY c.name;
