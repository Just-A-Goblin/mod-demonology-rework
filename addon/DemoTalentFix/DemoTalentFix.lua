-- DemoTalentFix — GENERAL talent-frame crash fix (not class-specific).
--
-- Blizzard's TalentFrame_DrawLines (FrameXML/TalentFrameBase.lua ~line 404)
-- hard-crashes with "attempt to concatenate field 'id' (a nil value)" whenever a
-- prerequisite arrow would run through an OCCUPIED cell ("blocked vertically" /
-- "undrawable layout"): it builds an error string by concatenating a
-- TALENT_BRANCH_ARRAY[..][..].id that is nil. That error aborts the whole
-- TalentFrame_Update redraw, so talents past the crash never draw and the
-- previous tab's buttons "bleed" through.
--
-- This happens for ANY talent tree (any class/spec) that routes a prereq past
-- another talent in the same column, or a long diagonal past a blocker — which
-- custom/expanded trees do routinely. Stock Blizzard trees never do, which is
-- why retail never trips it.
--
-- Fix: redefine the GLOBAL TalentFrame_DrawLines with the identical stock logic,
-- but on the three "undrawable" cases silently `return` instead of the crashing
-- message()+nil-concat. Result: all talents render; any arrow that genuinely
-- can't be drawn is simply omitted (the prereq itself still gates normally —
-- gating is driven by DBC PrereqTalent / GetTalentPrereqs, not by this drawing).
--
-- TalentFrame_SetPrereqs calls TalentFrame_DrawLines as a global, so reassigning
-- the global here (our addon loads after FrameXML) takes effect everywhere.

function TalentFrame_DrawLines(buttonTier, buttonColumn, tier, column, requirementsMet, TalentFrame)
  if ( requirementsMet ) then requirementsMet = 1 else requirementsMet = -1 end
  local BA = TalentFrame.TALENT_BRANCH_ARRAY

  -- prereq is in the same column
  if ( buttonColumn == column ) then
    if ( (buttonTier - tier) > 1 ) then
      for i = tier + 1, buttonTier - 1 do
        if ( BA[i][buttonColumn].id ) then return end   -- blocked vertically: skip line
      end
    end
    for i = tier, buttonTier - 1 do
      BA[i][buttonColumn].down = requirementsMet
      if ( (i + 1) <= (buttonTier - 1) ) then
        BA[i + 1][buttonColumn].up = requirementsMet
      end
    end
    BA[buttonTier][buttonColumn].topArrow = requirementsMet
    return
  end

  -- prereq is in the same tier
  if ( buttonTier == tier ) then
    local left = min(buttonColumn, column)
    local right = max(buttonColumn, column)
    if ( (right - left) > 1 ) then
      for i = left + 1, right - 1 do
        if ( BA[tier][i].id ) then return end   -- horizontal blocker: skip line
      end
    end
    for i = left, right - 1 do
      BA[tier][i].right = requirementsMet
      BA[tier][i + 1].left = requirementsMet
    end
    if ( buttonColumn < column ) then
      BA[buttonTier][buttonColumn].rightArrow = requirementsMet
    else
      BA[buttonTier][buttonColumn].leftArrow = requirementsMet
    end
    return
  end

  -- prereq is diagonal
  local left = min(buttonColumn, column)
  local right = max(buttonColumn, column)
  if ( left == column ) then left = left + 1 else right = right - 1 end
  local blocked = nil
  for i = left, right do
    if ( BA[tier][i].id ) then blocked = 1 end
  end
  left = min(buttonColumn, column)
  right = max(buttonColumn, column)
  if ( not blocked ) then
    BA[tier][buttonColumn].down = requirementsMet
    BA[buttonTier][buttonColumn].up = requirementsMet
    for i = tier, buttonTier - 1 do
      BA[i][buttonColumn].down = requirementsMet
      BA[i + 1][buttonColumn].up = requirementsMet
    end
    for i = left, right - 1 do
      BA[tier][i].right = requirementsMet
      BA[tier][i + 1].left = requirementsMet
    end
    BA[buttonTier][buttonColumn].topArrow = requirementsMet
    return
  end
  -- blocked going up first: try over-then-up
  if ( left == buttonColumn ) then left = left + 1 else right = right - 1 end
  for i = left, right do
    if ( BA[buttonTier][i].id ) then return end   -- undrawable layout: skip line
  end
  left = min(buttonColumn, column)
  right = max(buttonColumn, column)
  for i = tier, buttonTier - 1 do
    BA[i][column].up = requirementsMet
    BA[i + 1][column].down = requirementsMet
  end
  if ( buttonColumn < column ) then
    BA[buttonTier][buttonColumn].rightArrow = requirementsMet
  else
    BA[buttonTier][buttonColumn].leftArrow = requirementsMet
  end
end

-- ---------------------------------------------------------------------------
-- Optional debug helpers (harmless; handy when authoring new trees).
--   /tsel   — list the 3 tab names + GetNumTalents for each
--   /tprobe — force-select tab 2, report state, force a refresh, report shown count
-- ---------------------------------------------------------------------------
local function shownCount()
  local n = 0
  for i = 1, 40 do
    local b = _G["PlayerTalentFrameTalent"..i]
    if b and b:IsShown() then n = n + 1 end
  end
  return n
end

SLASH_TSEL1 = "/tsel"
SlashCmdList["TSEL"] = function()
  for t = 1, GetNumTalentTabs() do
    DEFAULT_CHAT_FRAME:AddMessage("tab "..t.." = "..tostring((GetTalentTabInfo(t))).." num="..tostring(GetNumTalents(t)))
  end
end

SLASH_TPROBE1 = "/tprobe"
SlashCmdList["TPROBE"] = function()
  local f = PlayerTalentFrame
  if not f then DEFAULT_CHAT_FRAME:AddMessage("open the talent window first"); return end
  local sel = PanelTemplates_GetSelectedTab(f)
  DEFAULT_CHAT_FRAME:AddMessage("before shown="..shownCount().." sel="..tostring(sel).." num="..tostring(GetNumTalents(sel)))
  local r = PlayerTalentFrame_Refresh or PlayerTalentFrame_Update
  local ok, err = pcall(function() if r then r() else TalentFrame_Update(f) end end)
  DEFAULT_CHAT_FRAME:AddMessage("refresh ok="..tostring(ok)..(ok and "" or (" ERR:"..tostring(err))).." after shown="..shownCount())
end
