# x64dbg recipe — find & patch the per-tier talent cap (Wow.exe build 12340)

Goal: confirm whether the Demonology tab's talent **count** is capped (27) or the **count is 36 but 9 array
slots are stale**, then find the exact instruction that enforces the cap and NOP/widen it.

All addresses are for **ImageBase 0x400000** (this exe normally loads there; 3.3.5 Wow.exe has no ASLR).
If x64dbg shows a different module base for `wow.exe`, add `(base - 0x400000)` to every VA — the RVAs are
listed so you can also use x64dbg's `wow.exe:$<RVA>` form.

| Symbol | VA | RVA |
|---|---|---|
| GetNumTalents | 0x5C5D40 | 0x1C5D40 |
| getTalentTabStruct(tab,inspect,pet) | 0x5C5C60 | 0x1C5C60 |
| GetTalentInfo | 0x5C7800 | 0x1C7800 |
| talent-array index sites (imul *0x5C) | 0x5C78E9 / 0x5C7DC6 / 0x5C7FB8 | 0x1C78E9 / … |
| player tab-array ptr (global) | [0xC21020] | 0x821020 |
| player tab-count (global) | [0xC2101C] | 0x82101C |

**Tab struct layout** (returned by getTalentTabStruct): `[+0]`=capacity, `[+4]`=talent count,
`[+8]`=pointer to talent array; each talent entry is **0x5C (92) bytes = one Talent.dbc record**
(entry offsets: `+0` ID, `+4` TabID, `+8` TierID, `+0xC` ColumnIndex, `+0x10..` SpellRank[1..9]).

---

## Step 0 — attach
1. Open `Wow.exe` in x64dbg (or run it and File → Attach). Log in to a warlock and get to a spot where you
   can open the talent window, but DON'T open it yet.
2. Confirm the module base: Symbols tab → `wow.exe` → note "Base". If it's `00400000`, use the VAs as-is.

## Step 1 — is the COUNT capped (27) or is it 36 with stale slots?
Set a **logging** breakpoint on the count read inside GetNumTalents (do not pause — just log):

In the command bar:
```
bp 0x5C5DBD
SetBreakpointCommandCondition 0x5C5DBD, 1
SetBreakpointLog 0x5C5DBD, "TAB struct={p:eax} cap={d:[eax]} count={d:[eax+4]} array={p:[eax+8]}"
SetBreakpointFastResume 0x5C5DBD, 1
```
(At 0x5C5DBD, `eax` = the tab struct just returned by getTalentTabStruct; the next instruction reads `[eax+4]`.)

Now in-game: open the talent window and click **each** of the 3 warlock tabs once (Affliction, Demonology,
Destruction). The Log tab prints one line per tab. Record the **count** and **struct** address for each.

- **Decision A — count for Demonology is 27** → the cap is in the *builder* that fills the struct. Go to Step 3.
- **Decision B — count is 36** → the count is fine but 9 array entries are stale. Go to Step 2 to confirm, then
  the cap is in the copy/build of `[struct+8]`. Go to Step 3 either way (same builder).

## Step 2 — inspect the Demonology talent array (confirm stale slots)
1. Do: select **Destruction** spec, then **Demonology** (to reproduce the Destruction-bleed).
2. Pause on the Demonology GetNumTalents hit (temporarily flip the bp to pause: uncheck FastResume, or
   `SetBreakpointFastResume 0x5C5DBD, 0`). `eax` = Demo struct.
3. In the CPU dump, `Follow in Dump` → `[eax+8]` (the array). Set dump to display and walk entries at stride
   0x5C. For each entry read `+4` (TabID) and `+8` (Tier):
   - Entries whose **TabID == 303** = our talents (good).
   - Entries whose **TabID == 301/302** = STALE (bleed) — note the FIRST index where TabID != 303 per tier.
   This tells you exactly how many real entries got written before the cap kicked in.

## Step 3 — find the instruction that enforces the cap (hardware breakpoint on the write)
You now have the Demonology **struct** address (call it `S`) from Step 1, and `S+4` = count field.
1. Set a **hardware breakpoint on WRITE** to the count field:
   ```
   bphws S+4, w, 4
   ```
   (replace `S+4` with the literal address, e.g. `bphws 0x12AB4C4, w, 4`). Also optionally HW-bp the array
   pointer `S+8` and a mid-array slot.
2. Trigger a rebuild: switch specs (Affliction ↔ Demonology) or reopen the talent frame / relog the talent
   data. The HW bp fires **inside the builder** the moment it writes the count/array.
3. When it breaks: look at the **Call Stack** and the surrounding code. You're looking for a loop that:
   - reads a per-tier or per-tab limit (a `cmp`/`jae`/`jge` against a count), and
   - either stops appending or overwrites — that compare is the cap.
   Common shapes: `cmp <index>, <limit>; jae skip_append`, or `inc count; cmp count, <maxPerTier>; jb loop`.
   Note the **address** of that conditional jump and what `<limit>` is (immediate? a memory read of the
   original per-tier count?).

## Step 4 — capture what I need to write the patch
Paste me:
- The 3 log lines from Step 1 (cap/count/array per tab).
- The array TabID scan from Step 2 (where stale begins).
- A **disassembly dump of ~40 instructions around the builder** where the HW bp fired (the loop + the cap
  compare/jump), plus the call-stack. Easiest: in the CPU view at the fired location, right-click → "Copy →
  Selection (No Bytes)" over ~40 lines, and the Call Stack (right-click → Copy).

With that I can give you the exact byte patch (usually a one/two-byte edit: NOP the cap jump, or bump the
limit immediate). x64dbg applies it via right-click → Assemble / Binary → Edit, then Patches → "Patch file"
to write a new Wow.exe (keep your backup).

## Notes
- If Step 1 shows **count = 36** and the frame still bleeds, the cap is purely in the array-fill (Step 3 HW bp
  on `S+8`/a mid slot is the key). If **count = 27**, the same builder loop sets both — Step 3 still applies.
- The per-tier hypothesis: the loop likely reads the ORIGINAL per-tier counts `[4,3,4,2,2,2,3,2,3,1,1]` from a
  table or the DBC-derived structure; the patch is to remove that ceiling (NOP the `jae`/`jge`, or replace the
  limit with a large constant / our per-tier count).
