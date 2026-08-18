--[[
  Master of the Legion — DISPLAY ONLY (PLAN §3.3).

  Shows health + buff frames for legion demons in slots 1-3 (the anchor uses the
  stock pet frame). Reads client-visible state only; sends NOTHING to the server.
  No trust boundary, no rate limiting.

  Phase 0: skeleton — the frames exist and can be shown/hidden. Wiring the frames
  to the actual guardian units (Feed the Pit stacks, Demonic Empowerment uptime)
  lands with Phase 1 (the Command Pool) once those units exist client-side.
--]]

local ADDON = "MasterOfTheLegion"
local NUM_SLOTS = 3

MasterOfTheLegionDB = MasterOfTheLegionDB or {}

local Legion = CreateFrame("Frame", "MasterOfTheLegionFrame", UIParent)
Legion.slots = {}

local function InitSlots()
    for i = 1, NUM_SLOTS do
        Legion.slots[i] = _G["MasterOfTheLegionSlot" .. i]
    end
end

Legion:RegisterEvent("PLAYER_LOGIN")
Legion:SetScript("OnEvent", function(self, event)
    if event == "PLAYER_LOGIN" then
        InitSlots()
        -- TODO(Phase 1): update loop reading demon health/buffs from client state.
        self:Hide()  -- hidden until there are demons to show
    end
end)
