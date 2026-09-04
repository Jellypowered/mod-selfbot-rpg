-- SelfBot RPG: farming controller and per-run settings.
local addon = CreateFrame("Frame", "SelfBotRPGFrame", UIParent)
addon:SetSize(450, 340); addon:SetPoint("CENTER"); addon:SetMovable(true); addon:EnableMouse(true)
addon:RegisterForDrag("LeftButton"); addon:SetScript("OnDragStart", addon.StartMoving); addon:SetScript("OnDragStop", addon.StopMovingOrSizing)
addon:SetBackdrop({bgFile="Interface/Tooltips/UI-Tooltip-Background",edgeFile="Interface/Tooltips/UI-Tooltip-Border",edgeSize=12,insets={left=3,right=3,top=3,bottom=3}}); addon:SetBackdropColor(0,0,0,.88); addon:SetAlpha(.55); addon:Hide()
-- Keep the panel readable while working, but unobtrusive when left idle.
-- Alpha is updated from the cursor bounds so child edit boxes and dropdowns do
-- not accidentally make the parent appear idle while the user is interacting.
local function FrameUnderCursor(frame)
 if not frame or not frame:IsShown() or not frame.GetLeft then return false end
 local scale=frame:GetEffectiveScale() or 1;local x,y=GetCursorPosition();x=x/scale;y=y/scale
 local left,bottom=frame:GetLeft(),frame:GetBottom();local width,height=frame:GetWidth(),frame:GetHeight()
 return left and bottom and x>=left and x<=left+width and y>=bottom and y<=bottom+height
end
local title=addon:CreateFontString(nil,"OVERLAY","GameFontNormalLarge"); title:SetPoint("TOP",0,-12); title:SetText("SelfBot RPG: Farming")
local close=CreateFrame("Button",nil,addon,"UIPanelCloseButton"); close:SetPoint("TOPRIGHT",2,2)
local help=addon:CreateFontString(nil,"OVERLAY","GameFontHighlightSmall"); help:SetPoint("TOP",0,-37); help:SetText("Node farming or material farming; choose a mode below.")
local resources={both={"Zone"},mining={"Copper","Tin","Silver","Iron","Gold","Mithril","Truesilver","Thorium","Fel Iron","Adamantite","Khorium","Cobalt","Saronite","Titanium"},herbalism={"Peacebloom","Silverleaf","Earthroot","Mageroyal","Briarthorn","Bruiseweed","Wild Steelbloom","Kingsblood","Liferoot","Fadeleaf","Goldthorn","Felweed","Goldclover","Lichbloom","Icethorn","Frost Lotus"}}
local resourceIcons={Copper="Interface\\Icons\\INV_Ore_Copper",Tin="Interface\\Icons\\INV_Ore_Tin",Silver="Interface\\Icons\\INV_Ore_Silver",Iron="Interface\\Icons\\INV_Ore_Iron",Gold="Interface\\Icons\\INV_Ore_Gold",Mithril="Interface\\Icons\\INV_Ore_Mithril",Truesilver="Interface\\Icons\\INV_Ore_Truesilver",Thorium="Interface\\Icons\\INV_Ore_Thorium",["Fel Iron"]="Interface\\Icons\\INV_Ore_FelIron",Adamantite="Interface\\Icons\\INV_Ore_Adamantium",Khorium="Interface\\Icons\\INV_Ore_Khorium",Cobalt="Interface\\Icons\\INV_Ore_Cobalt",Saronite="Interface\\Icons\\INV_Ore_Saronite",Titanium="Interface\\Icons\\INV_Ore_Titanium",Peacebloom="Interface\\Icons\\INV_Misc_Herb_01",Silverleaf="Interface\\Icons\\INV_Misc_Herb_08",Earthroot="Interface\\Icons\\INV_Misc_Herb_02",Mageroyal="Interface\\Icons\\INV_Misc_Herb_03",Briarthorn="Interface\\Icons\\INV_Misc_Herb_05",Bruiseweed="Interface\\Icons\\INV_Misc_Herb_04",["Wild Steelbloom"]="Interface\\Icons\\INV_Misc_Herb_06",Kingsblood="Interface\\Icons\\INV_Misc_Herb_04",Liferoot="Interface\\Icons\\INV_Misc_Herb_10",Fadeleaf="Interface\\Icons\\INV_Misc_Herb_11",Goldthorn="Interface\\Icons\\INV_Misc_Herb_12",Felweed="Interface\\Icons\\INV_Misc_Herb_Felweed",Goldclover="Interface\\Icons\\INV_Misc_Herb_17",Lichbloom="Interface\\Icons\\INV_Misc_Herb_15",Icethorn="Interface\\Icons\\INV_Misc_Herb_16",["Frost Lotus"]="Interface\\Icons\\INV_Misc_Herb_FrostLotus"}
local profession,resource="mining","Copper"
local material="Linen Cloth"
local materials={"Linen Cloth","Wool Cloth","Silk Cloth","Mageweave Cloth","Runecloth","Netherweave Cloth","Frostweave Cloth","Light Leather","Medium Leather","Heavy Leather","Thick Leather","Rugged Leather","Knothide Leather","Borean Leather","Mote of Earth","Mote of Fire","Mote of Air","Chunk of Boar Meat","Stringy Wolf Meat"}
local settings={}
local status
if RegisterAddonMessagePrefix then RegisterAddonMessagePrefix("JLYRPG2") end
local bridgeReady=false
local nextRequestId=0
local function SendProtocol(opcode, fields)
  nextRequestId=(nextRequestId % 999999999)+1
  local payload="1\t"..opcode.."\t"..nextRequestId
  for _,field in ipairs(fields or {}) do payload=payload.."\t"..tostring(field) end
  local channel,target="WHISPER",UnitName("player")
  if GetNumRaidMembers and GetNumRaidMembers()>0 then channel,target="RAID",nil
  elseif GetNumPartyMembers and GetNumPartyMembers()>0 then channel,target="PARTY",nil end
  SendAddonMessage("JLYRPG2",payload,channel,target)
end
local function Command(text)
  local command,rest=string.match(text,"%.sbrpg%s+(%S+)%s*(.*)")
  if not command then return end
  if not bridgeReady then
    status:SetText("SelfBot RPG: connecting protocol…")
    SendProtocol("HELLO",{})
    return
  end
  command=string.upper(command)
  if command=="FARM" then
    local profession,resource=string.match(rest,"^(%S+)%s+(.+)$")
    if not profession or not resource then status:SetText("Select profession and resource."); return end
    SendProtocol("START",{profession,resource})
  elseif command=="SET" then
    local key,value=string.match(rest,"^(%S+)%s+(%S+)$")
    if not key or not value then status:SetText("Invalid setting."); return end
    SendProtocol("SET",{key,value})
  elseif command=="STOP" or command=="STATUS" then
    SendProtocol(command,{})
  else
    status:SetText("Unsupported addon command: "..command)
  end
end
local professionLabel=addon:CreateFontString(nil,"OVERLAY","GameFontHighlightSmall");professionLabel:SetPoint("TOPLEFT",28,-67);professionLabel:SetText("Node profession")
local resourceLabel=addon:CreateFontString(nil,"OVERLAY","GameFontHighlightSmall");resourceLabel:SetPoint("TOPRIGHT",-55,-67);resourceLabel:SetText("Node resource")
local pd=CreateFrame("Frame","SelfBotRPGProfession",addon,"UIDropDownMenuTemplate"); pd:SetPoint("TOPLEFT",18,-79)
local resourceButton=CreateFrame("Button","SelfBotRPGResourceButton",addon,"UIPanelButtonTemplate");resourceButton:SetSize(180,24);resourceButton:SetPoint("TOPRIGHT",-34,-79);resourceButton:SetText(resource)
resourceButton.icon=resourceButton:CreateTexture(nil,"ARTWORK");resourceButton.icon:SetSize(16,16);resourceButton.icon:SetPoint("LEFT",6,0);resourceButton.icon:SetTexCoord(.08,.92,.08,.92);resourceButton.icon:SetTexture(resourceIcons[resource] or "Interface\\Icons\\INV_Misc_QuestionMark")
local resourceButtonText=resourceButton:GetFontString();resourceButtonText:ClearAllPoints();resourceButtonText:SetPoint("LEFT",resourceButton.icon,"RIGHT",5,0);resourceButtonText:SetPoint("RIGHT",resourceButton,"RIGHT",-8,0);resourceButtonText:SetJustifyH("LEFT")
local resourcePopup=CreateFrame("Frame","SelfBotRPGResourcePopup",UIParent);resourcePopup:SetSize(245,195);resourcePopup:SetPoint("TOPLEFT",resourceButton,"BOTTOMLEFT",0,-3);resourcePopup:SetFrameStrata("DIALOG");resourcePopup:SetBackdrop({bgFile="Interface\\Tooltips\\UI-Tooltip-Background",edgeFile="Interface\\Tooltips\\UI-Tooltip-Border",edgeSize=12,insets={left=3,right=3,top=3,bottom=3}});resourcePopup:SetBackdropColor(0,0,0,.96);resourcePopup:Hide()
local resourceScroll=CreateFrame("ScrollFrame","SelfBotRPGResourceScroll",resourcePopup,"UIPanelScrollFrameTemplate");resourceScroll:SetPoint("TOPLEFT",8,-8);resourceScroll:SetPoint("BOTTOMRIGHT",-28,8);resourceScroll:EnableMouseWheel(true);resourceScroll:SetScript("OnMouseWheel",function(self,delta)self:SetVerticalScroll(math.max(0,math.min(self:GetVerticalScrollRange(),self:GetVerticalScroll()-delta*20)))end)
local resourceChild=CreateFrame("Frame",nil,resourceScroll);resourceChild:SetWidth(205);resourceScroll:SetScrollChild(resourceChild)
local resourceRows={}
local function SetResource(v) resource=v;resourceButton:SetText(v);resourceButton.icon:SetTexture(resourceIcons[v] or "Interface\\Icons\\INV_Misc_QuestionMark");resourcePopup:Hide() end
local function RefreshResourceMenu()
 for _,row in ipairs(resourceRows) do row:Hide() end
 local values={};for _,n in ipairs(resources[profession] or {}) do table.insert(values,n) end
 if profession~="both" then table.insert(values,"Zone") end
 for index,name in ipairs(values) do
  local row=resourceRows[index]
  if not row then
   row=CreateFrame("Button",nil,resourceChild);row:SetSize(205,22);row.icon=row:CreateTexture(nil,"ARTWORK");row.icon:SetSize(18,18);row.icon:SetPoint("LEFT",3,0);row.text=row:CreateFontString(nil,"OVERLAY","GameFontHighlight");row.text:SetPoint("LEFT",row.icon,"RIGHT",6,0);row.highlight=row:CreateTexture(nil,"HIGHLIGHT");row.highlight:SetTexture("Interface\\Buttons\\WHITE8x8");row.highlight:SetVertexColor(.25,.22,.10,.35);row.highlight:SetAllPoints();resourceRows[index]=row
  end
  row:ClearAllPoints();row:SetPoint("TOPLEFT",resourceChild,"TOPLEFT",0,-(index-1)*22);row.text:SetText(name);row.icon:SetTexture(resourceIcons[name] or "Interface\\Icons\\INV_Misc_QuestionMark");row:SetScript("OnClick",function()SetResource(name)end);row:SetScript("OnEnter",function()GameTooltip:SetOwner(row,"ANCHOR_RIGHT");GameTooltip:SetText(name,1,.82,.22,true);GameTooltip:AddLine("Select this node resource for farming.",.8,.8,.8,true);GameTooltip:Show()end);row:SetScript("OnLeave",function()GameTooltip:Hide()end);row:Show()
 end
 resourceChild:SetHeight(math.max(1,#values*22))
end
resourceButton:SetScript("OnClick",function()if resourcePopup:IsShown() then resourcePopup:Hide() else RefreshResourceMenu();resourcePopup:Show()end end)
resourceButton:SetScript("OnEnter",function()GameTooltip:SetOwner(resourceButton,"ANCHOR_RIGHT");GameTooltip:SetText("Node resource",1,.82,.22,true);GameTooltip:AddLine("Select a node type from the scrollable list.",.8,.8,.8,true);GameTooltip:Show()end);resourceButton:SetScript("OnLeave",function()GameTooltip:Hide()end)
UIDropDownMenu_Initialize(pd,function() local i=UIDropDownMenu_CreateInfo();for _,n in ipairs({"Mining","Herbalism","Both"}) do local v=string.lower(n);i.text=n;i.checked=v==profession;i.func=function()profession=v;SetResource(resources[v][1]);RefreshResourceMenu();UIDropDownMenu_SetText(pd,n)end;UIDropDownMenu_AddButton(i)end end);UIDropDownMenu_SetWidth(pd,110);UIDropDownMenu_SetText(pd,"Mining")
pd:SetScript("OnEnter",function()GameTooltip:SetOwner(pd,"ANCHOR_RIGHT");GameTooltip:SetText("Node profession",1,.82,.22,true);GameTooltip:AddLine("Select which gathering profession controls node farming.",.8,.8,.8,true);GameTooltip:Show()end);pd:SetScript("OnLeave",function()GameTooltip:Hide()end)
RefreshResourceMenu()
-- Static IDs keep familiar icons visible before the server catalog arrives.
-- The server-provided catalog replaces these with authoritative IDs when loaded.
local materialIds={
 ["Linen Cloth"]=2589,["Wool Cloth"]=2592,["Silk Cloth"]=4306,["Mageweave Cloth"]=4338,["Runecloth"]=14047,["Netherweave Cloth"]=21877,["Frostweave Cloth"]=33470,
 ["Light Leather"]=2318,["Medium Leather"]=2319,["Heavy Leather"]=4234,["Thick Leather"]=4235,["Rugged Leather"]=4304,["Knothide Leather"]=21887,["Borean Leather"]=33568,
 ["Mote of Air"]=22572,["Mote of Earth"]=22573,["Mote of Fire"]=22574,["Mote of Life"]=22575,["Mote of Mana"]=22576,["Mote of Shadow"]=22577,["Mote of Water"]=22578,
 ["Primal Fire"]=21884,["Primal Water"]=21885,["Primal Life"]=21886,["Primal Air"]=22451,["Primal Earth"]=22452,["Primal Shadow"]=22456,["Primal Mana"]=22457,
}
local materialMeta={}
local function ItemIcon(itemId)
 itemId=tonumber(itemId or 0) or 0
 if itemId>0 and GetItemIcon then
  local icon=GetItemIcon(itemId);if icon then return icon end
 end
 if itemId>0 and GetItemInfo then
  local _,_,_,_,_,_,_,_,_,texture=GetItemInfo(itemId);if texture then return texture end
 end
 return "Interface\\Icons\\INV_Misc_QuestionMark"
end
local materialSearch=CreateFrame("EditBox",nil,addon,"InputBoxTemplate");materialSearch:SetSize(125,20);materialSearch:SetPoint("TOPRIGHT",-38,-139);materialSearch:SetAutoFocus(false);materialSearch:SetText("")
local materialLabel=addon:CreateFontString(nil,"OVERLAY","GameFontHighlightSmall");materialLabel:SetPoint("TOPLEFT",28,-123);materialLabel:SetText("Material target")
local materialSearchLabel=addon:CreateFontString(nil,"OVERLAY","GameFontHighlightSmall");materialSearchLabel:SetWidth(125);materialSearchLabel:SetJustifyH("RIGHT");materialSearchLabel:SetPoint("TOPRIGHT",-38,-123);materialSearchLabel:SetText("Search catalog")
local materialButton=CreateFrame("Button","SelfBotRPGMaterialButton",addon,"UIPanelButtonTemplate");materialButton:SetSize(180,24);materialButton:SetPoint("TOPLEFT",18,-139);materialButton:SetText(material)
local materialButtonIcon=materialButton:CreateTexture(nil,"ARTWORK");materialButtonIcon:SetSize(16,16);materialButtonIcon:SetPoint("LEFT",6,0);materialButtonIcon:SetTexCoord(.08,.92,.08,.92);materialButtonIcon:SetTexture(ItemIcon(materialIds[material]))
local materialButtonText=materialButton:GetFontString();materialButtonText:ClearAllPoints();materialButtonText:SetPoint("LEFT",materialButtonIcon,"RIGHT",5,0);materialButtonText:SetPoint("RIGHT",materialButton,"RIGHT",-8,0);materialButtonText:SetJustifyH("LEFT")
local materialPopup=CreateFrame("Frame","SelfBotRPGMaterialPopup",UIParent);materialPopup:SetSize(285,195);materialPopup:SetPoint("TOPLEFT",materialButton,"BOTTOMLEFT",0,-3);materialPopup:SetFrameStrata("DIALOG");materialPopup:SetBackdrop({bgFile="Interface\\Tooltips\\UI-Tooltip-Background",edgeFile="Interface\\Tooltips\\UI-Tooltip-Border",edgeSize=12,insets={left=3,right=3,top=3,bottom=3}});materialPopup:SetBackdropColor(0,0,0,.96);materialPopup:Hide()
addon:SetScript("OnUpdate",function()
 local over=FrameUnderCursor(addon) or FrameUnderCursor(materialPopup) or FrameUnderCursor(resourcePopup)
 for i=1,3 do local list=_G["DropDownList"..i];if FrameUnderCursor(list) then over=true;break end end
 addon:SetAlpha(over and 1 or .55)
end)
local materialScroll=CreateFrame("ScrollFrame","SelfBotRPGMaterialScroll",materialPopup,"UIPanelScrollFrameTemplate");materialScroll:SetPoint("TOPLEFT",8,-8);materialScroll:SetPoint("BOTTOMRIGHT",-28,8)
local materialChild=CreateFrame("Frame",nil,materialScroll);materialChild:SetWidth(245);materialScroll:SetScrollChild(materialChild)
local materialRows={}
local function SetMaterial(v) material=v;materialButton:SetText(v);materialButtonIcon:SetTexture(ItemIcon(materialIds[v]));materialPopup:Hide() end
local function MaterialTooltip(row,name)
 row:SetScript("OnEnter",function(self)
  GameTooltip:SetOwner(self,"ANCHOR_RIGHT");GameTooltip:SetText(name,1,.82,.22,true)
  local id=materialIds[name];local meta=materialMeta[name]
  if id then GameTooltip:AddLine("Item ID: "..id,.8,.8,.8,true) end
  if meta then GameTooltip:AddLine(meta.family.." | "..meta.methods,.7,.9,1,true) end
  GameTooltip:AddLine("Click to select this farming target.",.7,.7,.7,true);GameTooltip:Show()
 end)
 row:SetScript("OnLeave",function()GameTooltip:Hide()end)
end
local function RefreshMaterialMenu()
 for _,row in ipairs(materialRows) do row:Hide() end
 local filter=string.lower(materialSearch:GetText() or "");local shown=0
 for _,name in ipairs(materials) do
  if filter=="" or string.find(string.lower(name),filter,1,true) then
   shown=shown+1;local row=materialRows[shown]
   if not row then
    row=CreateFrame("Button",nil,materialChild);row:SetSize(245,22);row.icon=row:CreateTexture(nil,"ARTWORK");row.icon:SetSize(18,18);row.icon:SetPoint("LEFT",3,0);row.text=row:CreateFontString(nil,"OVERLAY","GameFontHighlight");row.text:SetPoint("LEFT",row.icon,"RIGHT",6,0);row.text:SetJustifyH("LEFT");row.highlight=row:CreateTexture(nil,"HIGHLIGHT");row.highlight:SetTexture("Interface\\QuestFrame\\UI-QuestTitleHighlight");row.highlight:SetBlendMode("ADD");row.highlight:SetAllPoints();materialRows[shown]=row
   end
   row:ClearAllPoints();row:SetPoint("TOPLEFT",materialChild, "TOPLEFT",0,-(shown-1)*22);row.text:SetText(name);row.icon:SetTexture(ItemIcon(materialIds[name]));row:SetScript("OnClick",function()SetMaterial(name)end);MaterialTooltip(row,name);row:Show()
  end
 end
 materialChild:SetHeight(math.max(1,shown*22))
end
materialButton:SetScript("OnClick",function() if materialPopup:IsShown() then materialPopup:Hide() else RefreshMaterialMenu();materialPopup:Show() end end)
materialButton:SetScript("OnEnter",function(self)GameTooltip:SetOwner(self,"ANCHOR_RIGHT");GameTooltip:SetText("Material target",1,.82,.22,true);GameTooltip:AddLine("Select an exact catalog item to farm.",.8,.8,.8,true);GameTooltip:AddLine("Use Search to filter the scrollable list.",.8,.8,.8,true);GameTooltip:Show()end);materialButton:SetScript("OnLeave",function()GameTooltip:Hide()end)
materialSearch:SetScript("OnTextChanged",RefreshMaterialMenu);materialSearch:SetScript("OnEnter",function()GameTooltip:SetOwner(materialSearch,"ANCHOR_RIGHT");GameTooltip:SetText("Search material catalog",1,.82,.22,true);GameTooltip:AddLine("Type part of a name, then open the scrollable picker.",.8,.8,.8,true);GameTooltip:Show()end);materialSearch:SetScript("OnLeave",function()GameTooltip:Hide()end);RefreshMaterialMenu()
local durationRow=CreateFrame("Frame",nil,addon);durationRow:SetSize(340,22);durationRow:SetPoint("TOP",0,-177)
local durationLabel=durationRow:CreateFontString(nil,"OVERLAY","GameFontHighlightSmall");durationLabel:SetWidth(270);durationLabel:SetPoint("LEFT",0,0);durationLabel:SetJustifyH("RIGHT");durationLabel:SetText("Duration (minutes; 0 = unlimited)")
local durationBox=CreateFrame("EditBox",nil,durationRow,"InputBoxTemplate");durationBox:SetSize(55,20);durationBox:SetPoint("LEFT",durationLabel,"RIGHT",10,0);durationBox:SetAutoFocus(false);durationBox:SetNumeric(true);durationBox:SetMaxLetters(5);durationBox:SetText("0")
durationBox:SetScript("OnEnter",function()GameTooltip:SetOwner(durationBox,"ANCHOR_RIGHT");GameTooltip:SetText("Duration",1,.82,.22,true);GameTooltip:AddLine("0 or empty runs until quantity, bag, or manual stop.",.8,.8,.8,true);GameTooltip:Show()end);durationBox:SetScript("OnLeave",function()GameTooltip:Hide()end)
local quantityRow=CreateFrame("Frame",nil,addon);quantityRow:SetSize(300,22);quantityRow:SetPoint("TOP",0,-213)
local quantityLabel=quantityRow:CreateFontString(nil,"OVERLAY","GameFontHighlightSmall");quantityLabel:SetWidth(230);quantityLabel:SetPoint("LEFT",0,0);quantityLabel:SetJustifyH("RIGHT");quantityLabel:SetText("Quantity goal (0 = unlimited)")
local quantityBox=CreateFrame("EditBox",nil,quantityRow,"InputBoxTemplate");quantityBox:SetSize(55,20);quantityBox:SetPoint("LEFT",quantityLabel,"RIGHT",10,0);quantityBox:SetAutoFocus(false);quantityBox:SetNumeric(true);quantityBox:SetMaxLetters(6);quantityBox:SetText("0")
quantityBox:SetScript("OnEnter",function()GameTooltip:SetOwner(quantityBox,"ANCHOR_RIGHT");GameTooltip:SetText("Quantity goal",1,.82,.22,true);GameTooltip:AddLine("Stops material farming after this many requested items.",.8,.8,.8,true);GameTooltip:Show()end);quantityBox:SetScript("OnLeave",function()GameTooltip:Hide()end)
status=addon:CreateFontString(nil,"OVERLAY","GameFontHighlightSmall");status:SetPoint("TOP",0,-250);status:SetWidth(420);status:SetHeight(44);status:SetWordWrap(true);status:SetJustifyH("CENTER");status:SetJustifyV("TOP");status:SetText("SelfBot RPG: waiting for status")
local lastStatusRequest=0
local function RequestStatus()
  local now=GetTime()
  if now-lastStatusRequest<2 then return end
  lastStatusRequest=now
  Command(".sbrpg status")
  if bridgeReady then SendProtocol("MATERIAL_STATUS",{}) end
end
addon:RegisterEvent("CHAT_MSG_ADDON");addon:RegisterEvent("PLAYER_LOGIN");addon:SetScript("OnEvent",function(_,event,prefix,message)
 if event=="PLAYER_LOGIN" then
   if RegisterAddonMessagePrefix then RegisterAddonMessagePrefix("JLYRPG2") end
   bridgeReady=false
   SendProtocol("HELLO",{})
   return
 end
 if prefix~="JLYRPG2" then return end
 local parts={}
 for field in string.gmatch(message.."\t","(.-)\t") do table.insert(parts,field) end
 if parts[1]~="1" then return end
 if parts[2]=="HELLO_ACK" then
   bridgeReady=true
   status:SetText("SelfBot RPG: protocol connected")
   SendProtocol("MATERIAL_CATALOG",{})
 elseif parts[2]=="CAPABILITIES" then
   bridgeReady=true
 elseif parts[2]=="MATERIAL_CATALOG" then
   -- Catalog rows are individually framed so they remain below addon-message limits.
   local requestId,index,total,itemId,key,displayName,family,methods=unpack(parts,3)
   if tonumber(index or "0")==0 then materials={};materialIds={};materialMeta={} end
   local name=displayName or key or itemId
   table.insert(materials,name);materialIds[name]=tonumber(itemId);materialMeta[name]={family=family or "other",methods=methods or "unknown"}
   if name==material then materialButtonIcon:SetTexture(ItemIcon(materialIds[name])) end
   RefreshMaterialMenu()
 elseif parts[2]=="ACK" then
   -- Commands are request-id correlated; STATUS carries the resulting state.
   bridgeReady=true
 elseif parts[2]=="MATERIAL_STATUS" then
   local requestId,active,itemId,gathered,goal,kills,remaining,skinning=unpack(parts,3)
   if active=="0" then
     status:SetText("SelfBot RPG: no material run active")
   else
     local goalText=(tonumber(goal or "0") or 0)>0 and (" / "..goal) or ""
     status:SetText("Material "..itemId..": "..gathered..goalText.." items | "..kills.." kills | "..remaining.."s remaining")
   end
 elseif parts[2]=="STATUS" then
   local active,phase,reason,profession,nodes,gathers,items,perMin,perSec,target,durationSec,remainingSec=unpack(parts,3)
   local reasonText=(reason and reason~="") and (" — "..reason) or ""
   if active=="0" then status:SetText("SelfBot RPG: "..(reason~="" and reason or "idle"))
   else
     local targetText=(target and target~="0") and (" | target "..target) or ""
     local timerText=""
     if tonumber(durationSec or "0")>0 then
       local remain=tonumber(remainingSec or "0") or 0
       timerText=string.format(" | %d:%02d left",math.floor(remain/60),remain % 60)
     end
     status:SetText(phase..reasonText.." | "..profession..": "..nodes.." nodes | "..gathers.." gathers / "..items.." items | "..perMin.."/min "..perSec.."/sec"..timerText..targetText)
   end
 elseif parts[2]=="SETTING" then
   if settings[parts[3]] then settings[parts[3]]:SetText(parts[4]) end
 elseif parts[2]=="ERROR" then status:SetText("Error: "..(parts[4] or parts[3] or "unknown"))
 elseif parts[2]=="DEBUG" then status:SetText(parts[3] or "") end
end)
addon:SetScript("OnShow",function() addon:SetAlpha(1);RequestStatus() end)
-- Status requests are deliberately limited to panel opens; periodic requests
-- are not emitted by the fallback UI.
SelfBotRPGSettingsFrame=CreateFrame("Frame","SelfBotRPGSettingsFrame",UIParent)
local settingsPanel=SelfBotRPGSettingsFrame;settingsPanel:SetSize(330,230);settingsPanel:SetPoint("CENTER",addon,"CENTER",0,0);settingsPanel:SetMovable(true);settingsPanel:EnableMouse(true);settingsPanel:SetScript("OnShow", RequestStatus);settingsPanel:SetBackdrop({bgFile="Interface/Tooltips/UI-Tooltip-Background",edgeFile="Interface/Tooltips/UI-Tooltip-Border",edgeSize=12,insets={left=3,right=3,top=3,bottom=3}});settingsPanel:SetBackdropColor(0,0,0,.92);settingsPanel:Hide()
local st=settingsPanel:CreateFontString(nil,"OVERLAY","GameFontNormalLarge");st:SetPoint("TOP",0,-12);st:SetText("SelfBot RPG Settings")
local sc=CreateFrame("Button",nil,settingsPanel,"UIPanelCloseButton");sc:SetPoint("TOPRIGHT",2,2)
local function Setting(label,key,value,y)
 local text=settingsPanel:CreateFontString(nil,"OVERLAY","GameFontNormalSmall");text:SetPoint("TOPLEFT",20,y);text:SetText(label)
 local box=CreateFrame("EditBox",nil,settingsPanel,"InputBoxTemplate");box:SetSize(55,20);box:SetPoint("TOPRIGHT",-35,y+4);box:SetAutoFocus(false);box:SetText(value)
 settings[key]=box
 box:SetScript("OnEnterPressed",function(self)Command(".sbrpg set "..key.." "..self:GetText());self:ClearFocus()end)
end
Setting("Attempts before blacklist", "attempts", "3", -48);Setting("Failed-node blacklist seconds", "failedblacklist", "120", -78);Setting("Empty-node blacklist seconds", "emptyblacklist", "120", -108);Setting("Stay in starting zone (1/0)", "zone", "1", -138);Setting("Gather settle delay (ms)", "settledelay", "500", -168)
local start=CreateFrame("Button",nil,addon,"UIPanelButtonTemplate");start:SetSize(120,24);start:SetPoint("BOTTOMLEFT",12,15);start:SetText("Start Farming");start:SetScript("OnClick",function()
 if not bridgeReady then status:SetText("SelfBot RPG: connecting protocol…");SendProtocol("HELLO",{});return end
 local duration=durationBox:GetText() or ""
 if duration~="" and (not tonumber(duration) or tonumber(duration)<0 or tonumber(duration)>10080) then status:SetText("Duration must be 0–10080 minutes.");return end
 if duration=="" then duration="0" end
 if resource=="Zone" and profession~="both" then SendProtocol("START",{"zone",profession,duration}) else SendProtocol("START",{profession,resource,duration}) end
 -- A setting issued before farming is deliberately rejected by the server;
 -- send the complete override set immediately after starting the run instead.
 for key,box in pairs(settings) do Command(".sbrpg set "..key.." "..box:GetText()) end
 Command(".sbrpg status")
end)
local materialStart=CreateFrame("Button",nil,addon,"UIPanelButtonTemplate");materialStart:SetSize(120,24);materialStart:SetPoint("BOTTOM",0,15);materialStart:SetText("Start Material");materialStart:SetScript("OnClick",function()
 if not bridgeReady then status:SetText("SelfBot RPG: connecting protocol…");SendProtocol("HELLO",{});return end
 local duration=tonumber(durationBox:GetText() or "0") or 0
 local quantity=tonumber(quantityBox:GetText() or "0") or 0
 if duration<0 or duration>10080 or quantity<0 then status:SetText("Invalid duration or quantity.");return end
 SendProtocol("START_MATERIAL",{"material",material,tostring(duration),tostring(quantity)})
end)
local stop=CreateFrame("Button",nil,addon,"UIPanelButtonTemplate");stop:SetSize(100,24);stop:SetPoint("BOTTOMRIGHT",-12,15);stop:SetText("Stop");stop:SetScript("OnClick",function()Command(".sbrpg stop")end)
local function ButtonTooltip(button,titleText,bodyText)
 button:SetScript("OnEnter",function(self)GameTooltip:SetOwner(self,"ANCHOR_TOP");GameTooltip:SetText(titleText,1,.82,.22,true);GameTooltip:AddLine(bodyText,.8,.8,.8,true);GameTooltip:Show()end);button:SetScript("OnLeave",function()GameTooltip:Hide()end)
end
local function ButtonIcon(button,texture)
 local icon=button:CreateTexture(nil,"ARTWORK");icon:SetTexture(texture);icon:SetSize(16,16);icon:SetPoint("LEFT",button,"LEFT",8,0);icon:SetTexCoord(.08,.92,.08,.92)
end
ButtonIcon(start,"Interface\\Icons\\INV_Misc_Herb_07")
ButtonIcon(materialStart,"Interface\\Icons\\INV_Misc_LeatherScrap")
ButtonIcon(stop,"Interface\\Icons\\Spell_Shadow_Teleport")
ButtonTooltip(start,"Start node farming","Moves between mining/herbalism nodes using the selected profession and resource.")
ButtonTooltip(materialStart,"Start material farming","Farms the selected exact item from eligible creature sources. Duration and quantity are optional goals.")
ButtonTooltip(stop,"Stop current run","Stops farming/material activity and restores normal playerbot strategies.")
-- Manual-only emergency fallback. It is never selected automatically, so
-- addon protocol traffic cannot collide with playerbot chat handlers.
SLASH_SELFBOTRPG1="/sbrpg";SlashCmdList.SELFBOTRPG=function(text)
 if text and string.lower(text)=="material status" then
   if not bridgeReady then status:SetText("SelfBot RPG: connecting protocol…");SendProtocol("HELLO",{})
   else SendProtocol("MATERIAL_STATUS",{}) end
   if not addon:IsShown() then addon:Show() end
   return
 end
 if addon:IsShown() then addon:Hide() else addon:Show() end
end
SLASH_SELFBOTRPGCHAT1="/sbrpgchat";SlashCmdList.SELFBOTRPGCHAT=function(text)
 SendChatMessage(".sbrpg "..text,"SAY")
end
