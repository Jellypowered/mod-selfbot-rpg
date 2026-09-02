-- Persistent minimap launcher. Left-click toggles the panel; right-click opens settings.
SelfBotRPGDB = SelfBotRPGDB or { minimapAngle = 225 }
local button = CreateFrame("Button", "SelfBotRPGMinimapButton", Minimap)
button:SetSize(32, 32)
button:SetFrameStrata("MEDIUM")
button:SetNormalTexture("Interface\\Icons\\INV_Pick_02")
button:SetHighlightTexture("Interface\\Minimap\\UI-Minimap-ZoomButton-Highlight")
button:RegisterForClicks("LeftButtonUp", "RightButtonUp")
button:RegisterForDrag("LeftButton")

local function Place()
  local a = math.rad(SelfBotRPGDB.minimapAngle or 225)
  button:SetPoint("CENTER", Minimap, "CENTER", math.cos(a) * 80, math.sin(a) * 80)
end
button:SetScript("OnDragStart", function() button:SetScript("OnUpdate", function()
  local mx, my = Minimap:GetCenter(); local x, y = GetCursorPosition(); local s = Minimap:GetEffectiveScale()
  SelfBotRPGDB.minimapAngle = math.deg(math.atan2(y / s - my, x / s - mx)); Place()
end) end)
button:SetScript("OnDragStop", function() button:SetScript("OnUpdate", nil) end)
button:SetScript("OnClick", function(_, mouse)
  if mouse == "LeftButton" then
    if SelfBotRPGFrame:IsShown() then SelfBotRPGFrame:Hide() else SelfBotRPGFrame:Show() end
  else
    SelfBotRPGSettingsFrame:Show()
  end
end)
button:SetScript("OnEnter", function() GameTooltip:SetOwner(button,"ANCHOR_LEFT"); GameTooltip:AddLine("SelfBot RPG"); GameTooltip:AddLine("Left-click: farming panel"); GameTooltip:AddLine("Right-click: settings"); GameTooltip:Show() end)
button:SetScript("OnLeave", function() GameTooltip:Hide() end)
Place()
