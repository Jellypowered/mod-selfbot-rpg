-- Persistent, circular minimap launcher.
SelfBotRPGDB = SelfBotRPGDB or {}
if type(SelfBotRPGDB.minimapPos) ~= "number" then
  SelfBotRPGDB.minimapPos = tonumber(SelfBotRPGDB.minimapAngle) or 225
end

local button = CreateFrame("Button", "SelfBotRPGMinimapButton", Minimap)
button:SetSize(32, 32)
button:SetMovable(true)
button:EnableMouse(true)
button:SetFrameStrata("MEDIUM")
button:SetFrameLevel(Minimap:GetFrameLevel() + 8)
button:RegisterForClicks("LeftButtonUp", "RightButtonUp")
button:RegisterForDrag("LeftButton")

local background = button:CreateTexture(nil, "BACKGROUND")
background:SetTexture("Interface\\Minimap\\UI-Minimap-Background")
background:SetSize(20, 20)
background:SetPoint("CENTER", 0, 1)

local icon = button:CreateTexture(nil, "ARTWORK")
icon:SetTexture("Interface\\Icons\\INV_Pick_02")
icon:SetSize(18, 18)
icon:SetPoint("CENTER", 0, 1)
icon:SetTexCoord(0.08, 0.92, 0.08, 0.92)

local border = button:CreateTexture(nil, "OVERLAY")
border:SetTexture("Interface\\Minimap\\MiniMap-TrackingBorder")
border:SetSize(54, 54)
border:SetPoint("TOPLEFT", 0, 0)

local highlight = button:CreateTexture(nil, "HIGHLIGHT")
highlight:SetTexture("Interface\\Minimap\\UI-Minimap-ZoomButton-Highlight")
highlight:SetBlendMode("ADD")
highlight:SetAllPoints(button)

local function Place()
  local angle = math.rad(tonumber(SelfBotRPGDB.minimapPos) or 225)
  button:ClearAllPoints()
  button:SetPoint("CENTER", Minimap, "CENTER", math.cos(angle) * 80, math.sin(angle) * 80)
end

local moved = false
button:SetScript("OnMouseDown", function() moved = false end)
button:SetScript("OnDragStart", function(self)
  moved = true
  self:SetScript("OnUpdate", function()
    local mx, my = Minimap:GetCenter()
    local cursorX, cursorY = GetCursorPosition()
    local scale = Minimap:GetEffectiveScale()
    SelfBotRPGDB.minimapPos = math.deg(math.atan2(cursorY / scale - my, cursorX / scale - mx))
    Place()
  end)
end)
button:SetScript("OnDragStop", function(self)
  self:SetScript("OnUpdate", nil)
  Place()
end)
button:SetScript("OnClick", function(_, mouse)
  if moved then return end
  if mouse == "LeftButton" then
    if SelfBotRPGFrame:IsShown() then SelfBotRPGFrame:Hide() else SelfBotRPGFrame:Show() end
  elseif mouse == "RightButton" then
    SelfBotRPGSettingsFrame:Show()
  end
end)
button:SetScript("OnEnter", function(self)
  GameTooltip:SetOwner(self, "ANCHOR_LEFT")
  GameTooltip:AddLine("SelfBot RPG")
  GameTooltip:AddLine("Left-click: farming panel", 1, 1, 1)
  GameTooltip:AddLine("Right-click: settings", 1, 1, 1)
  GameTooltip:AddLine("Drag: reposition", 0.7, 0.7, 0.7)
  GameTooltip:Show()
end)
button:SetScript("OnLeave", function() GameTooltip:Hide() end)
button:RegisterEvent("PLAYER_LOGIN")
button:SetScript("OnEvent", function(self)
  Place()
  self:UnregisterEvent("PLAYER_LOGIN")
end)
Place()
