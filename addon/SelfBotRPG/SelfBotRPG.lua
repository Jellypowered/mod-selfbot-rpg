-- SelfBot RPG: farming controller and per-run settings.
local addon = CreateFrame("Frame", "SelfBotRPGFrame", UIParent)
addon:SetSize(450, 190); addon:SetPoint("CENTER"); addon:SetMovable(true); addon:EnableMouse(true)
addon:RegisterForDrag("LeftButton"); addon:SetScript("OnDragStart", addon.StartMoving); addon:SetScript("OnDragStop", addon.StopMovingOrSizing)
addon:SetBackdrop({bgFile="Interface/Tooltips/UI-Tooltip-Background",edgeFile="Interface/Tooltips/UI-Tooltip-Border",edgeSize=12,insets={left=3,right=3,top=3,bottom=3}}); addon:SetBackdropColor(0,0,0,.88); addon:Hide()
local title=addon:CreateFontString(nil,"OVERLAY","GameFontNormalLarge"); title:SetPoint("TOP",0,-12); title:SetText("SelfBot RPG: Farming")
local close=CreateFrame("Button",nil,addon,"UIPanelCloseButton"); close:SetPoint("TOPRIGHT",2,2)
local help=addon:CreateFontString(nil,"OVERLAY","GameFontHighlightSmall"); help:SetPoint("TOP",0,-37); help:SetText("Select a profession and resource, then start farming.")
local resources={both={"Zone"},mining={"Copper","Tin","Silver","Iron","Gold","Mithril","Truesilver","Thorium","Fel Iron","Adamantite","Khorium","Cobalt","Saronite","Titanium"},herbalism={"Peacebloom","Silverleaf","Earthroot","Mageroyal","Briarthorn","Bruiseweed","Wild Steelbloom","Kingsblood","Liferoot","Fadeleaf","Goldthorn","Felweed","Goldclover","Lichbloom","Icethorn","Frost Lotus"}}
local profession,resource="mining","Copper"
local function Command(text)
  -- text is the legacy .sbrpg command shape; translate it to protocol v1.
  local command, rest = string.match(text, "%.sbrpg%s+(%S+)%s*(.*)")
  if not command then return end
  local payload = "1\tCMD\t" .. string.upper(command)
  for word in string.gmatch(rest, "%S+") do payload = payload .. "\t" .. word end
  local channel, target = "WHISPER", UnitName("player")
  if GetNumRaidMembers() > 0 then channel, target = "RAID", nil elseif GetNumPartyMembers() > 0 then channel, target = "PARTY", nil end
  SendAddonMessage("SBRPG", payload, channel, target)
end
local pd=CreateFrame("Frame","SelfBotRPGProfession",addon,"UIDropDownMenuTemplate"); pd:SetPoint("TOPLEFT",18,-57)
local rd=CreateFrame("Frame","SelfBotRPGResource",addon,"UIDropDownMenuTemplate"); rd:SetPoint("TOPRIGHT",-34,-57)
local function SetResource(v) resource=v; UIDropDownMenu_SetText(rd,v) end
UIDropDownMenu_Initialize(rd,function() local i=UIDropDownMenu_CreateInfo(); for _,n in ipairs(resources[profession]) do i.text=n;i.checked=n==resource;i.func=function()SetResource(n)end;UIDropDownMenu_AddButton(i) end
 if profession~="both" then i.text="Zone";i.checked=resource=="Zone";i.func=function()SetResource("Zone")end;UIDropDownMenu_AddButton(i) end end); UIDropDownMenu_SetWidth(rd,180);UIDropDownMenu_SetText(rd,resource)
UIDropDownMenu_Initialize(pd,function() local i=UIDropDownMenu_CreateInfo();for _,n in ipairs({"Mining","Herbalism","Both"}) do local v=string.lower(n);i.text=n;i.checked=v==profession;i.func=function()profession=v;SetResource(resources[v][1]);UIDropDownMenu_SetText(pd,n)end;UIDropDownMenu_AddButton(i)end end);UIDropDownMenu_SetWidth(pd,110);UIDropDownMenu_SetText(pd,"Mining")
local status=addon:CreateFontString(nil,"OVERLAY","GameFontHighlightSmall");status:SetPoint("TOP",0,-120);status:SetWidth(420);status:SetText("Gathers: 0 | Estimated: 0.00/min, 0.000/sec")
addon:RegisterEvent("CHAT_MSG_ADDON");addon:SetScript("OnEvent",function(_,_,prefix,message)
 if prefix~="SBRPG" then return end
 local parts={} for field in string.gmatch(message,"[^\t]+") do table.insert(parts,field) end
 if parts[1]~="1" then return end
 if parts[2]=="STATUS" then
   local active,mode,nodes,gathers,items,perMin,perSec=unpack(parts,3)
   if active=="0" then status:SetText("SelfBot RPG: idle")
   else status:SetText(mode..": "..nodes.." route nodes | "..gathers.." gathers / "..items.." items | "..perMin.."/min "..perSec.."/sec") end
 elseif parts[2]=="SETTING" then
   if settings[parts[3]] then settings[parts[3]]:SetText(parts[4]) end
 elseif parts[2]=="ERROR" then status:SetText("Error: "..(parts[3] or "unknown"))
 elseif parts[2]=="DEBUG" then status:SetText(parts[3] or "") end
end)
SelfBotRPGSettingsFrame=CreateFrame("Frame","SelfBotRPGSettingsFrame",UIParent)
local settingsPanel=SelfBotRPGSettingsFrame;settingsPanel:SetSize(330,230);settingsPanel:SetPoint("CENTER",addon,"CENTER",0,0);settingsPanel:SetMovable(true);settingsPanel:EnableMouse(true);settingsPanel:SetBackdrop({bgFile="Interface/Tooltips/UI-Tooltip-Background",edgeFile="Interface/Tooltips/UI-Tooltip-Border",edgeSize=12,insets={left=3,right=3,top=3,bottom=3}});settingsPanel:SetBackdropColor(0,0,0,.92);settingsPanel:Hide()
local st=settingsPanel:CreateFontString(nil,"OVERLAY","GameFontNormalLarge");st:SetPoint("TOP",0,-12);st:SetText("SelfBot RPG Settings")
local sc=CreateFrame("Button",nil,settingsPanel,"UIPanelCloseButton");sc:SetPoint("TOPRIGHT",2,2)
local settings={}
local function Setting(label,key,value,y)
 local text=settingsPanel:CreateFontString(nil,"OVERLAY","GameFontNormalSmall");text:SetPoint("TOPLEFT",20,y);text:SetText(label)
 local box=CreateFrame("EditBox",nil,settingsPanel,"InputBoxTemplate");box:SetSize(55,20);box:SetPoint("TOPRIGHT",-35,y+4);box:SetAutoFocus(false);box:SetText(value)
 settings[key]=box
 box:SetScript("OnEnterPressed",function(self)Command(".sbrpg set "..key.." "..self:GetText());self:ClearFocus()end)
end
Setting("Attempts before blacklist", "attempts", "3", -48);Setting("Failed-node blacklist seconds", "failedblacklist", "120", -78);Setting("Empty-node blacklist seconds", "emptyblacklist", "120", -108);Setting("Stay in starting zone (1/0)", "zone", "1", -138);Setting("Gather settle delay (ms)", "settledelay", "1000", -168)
local start=CreateFrame("Button",nil,addon,"UIPanelButtonTemplate");start:SetSize(130,24);start:SetPoint("BOTTOMLEFT",24,15);start:SetText("Start Farming");start:SetScript("OnClick",function()
 if resource=="Zone" and profession~="both" then Command(".sbrpg farm zone "..profession) else Command(".sbrpg farm "..profession.." "..resource) end
 -- A setting issued before farming is deliberately rejected by the server;
 -- send the complete override set immediately after starting the run instead.
 for key,box in pairs(settings) do Command(".sbrpg set "..key.." "..box:GetText()) end
 Command(".sbrpg status")
end)
local stop=CreateFrame("Button",nil,addon,"UIPanelButtonTemplate");stop:SetSize(110,24);stop:SetPoint("BOTTOMRIGHT",-24,15);stop:SetText("Stop");stop:SetScript("OnClick",function()Command(".sbrpg stop")end)
SLASH_SELFBOTRPG1="/sbrpg";SlashCmdList.SELFBOTRPG=function()if addon:IsShown()then addon:Hide()else addon:Show()end end
