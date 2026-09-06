-- SelfBot RPG: farming controller and per-run settings.
SelfBotRPGDB=SelfBotRPGDB or {}
SelfBotRPGDB.Panel=SelfBotRPGDB.Panel or {}
local panelDB=SelfBotRPGDB.Panel
local addon = CreateFrame("Frame", "SelfBotRPGFrame", UIParent)
addon:SetSize(450, 400)
if panelDB.x and panelDB.y then addon:SetPoint("CENTER",UIParent,"BOTTOMLEFT",panelDB.x,panelDB.y) else addon:SetPoint("CENTER") end
addon:SetMovable(true); addon:EnableMouse(true)
addon:RegisterForDrag("LeftButton"); addon:SetScript("OnDragStart", addon.StartMoving); addon:SetScript("OnDragStop",function(self)self:StopMovingOrSizing();local x,y=self:GetCenter();local scale=UIParent:GetEffectiveScale() or 1;panelDB.x=x*scale;panelDB.y=y*scale end)
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
local resourceItemIds={Copper=2770,Tin=2771,Silver=2775,Iron=2772,Gold=2776,Mithril=3858,Truesilver=7911,Thorium=10620,["Fel Iron"]=23424,Adamantite=23425,Khorium=23426,Cobalt=36909,Saronite=36912,Titanium=36910,Peacebloom=2447,Silverleaf=765,Earthroot=2449,Mageroyal=785,Briarthorn=2450,Bruiseweed=2453,["Wild Steelbloom"]=3355,Kingsblood=3356,Liferoot=3357,Fadeleaf=3818,Goldthorn=3821,Felweed=22785,Goldclover=36901,Lichbloom=36905,Icethorn=36906,["Frost Lotus"]=36907}
local profession=panelDB.profession or "mining"
local resource=panelDB.resource or ((profession=="herbalism" and "Peacebloom") or "Copper")
local material=panelDB.material or "Select material"
local fishingTarget=panelDB.fishingTarget or ""
local materials={"Linen Cloth","Wool Cloth","Silk Cloth","Mageweave Cloth","Runecloth","Netherweave Cloth","Frostweave Cloth","Light Leather","Medium Leather","Heavy Leather","Thick Leather","Rugged Leather","Knothide Leather","Borean Leather","Mote of Earth","Mote of Fire","Mote of Air","Chunk of Boar Meat","Stringy Wolf Meat","Oily Blackmouth","Firefin Snapper","Deviate Fish","Raw Loch Frenzy","Rainbow Fin Albacore","Winter Squid","Raw Summer Bass","Lightning Eel","Raw Redgill","Raw Nightfin Snapper","Raw Sunscale Salmon","Barbed Gill Trout","Spotted Feltail","Zangarian Sporefish","Golden Darter","Furious Crawdad","Deep Sea Monsterbelly","Moonglow Cuttlefish","Imperial Manta Ray","Rockfin Grouper","Borean Man O' War"}
local settings={}
local status
if RegisterAddonMessagePrefix then RegisterAddonMessagePrefix("JLYRPG2") end
local bridgeReady=false
local capabilities={}
local materialSourceCount=0
local materialSourceTotal=0
local materialSourceRequestId=nil
local start,materialStart,fishingZoneStart,stop
local durationBox,quantityBox,materialSearch,fishingTargetButton
local prioritizePoolsCheck,openWaterOnlyCheck
local SendProtocol
local ApplySavedSettings
local nextRequestId=0
local function HasCapability(name) return capabilities[name] == true end
local function FormatDuration(seconds)
 seconds=math.max(0,tonumber(seconds) or 0)
 local hours=math.floor(seconds/3600);local minutes=math.floor((seconds%3600)/60);local remainder=seconds%60
 if hours>0 then return string.format("%dh %dm %ds",hours,minutes,remainder) end
 if minutes>0 then return string.format("%dm %ds",minutes,remainder) end
 return string.format("%ds",remainder)
end
local function SavePanelFields()
 panelDB.profession=profession;panelDB.resource=resource;panelDB.material=material
 if durationBox then panelDB.duration=durationBox:GetText() end
 if quantityBox then panelDB.quantity=quantityBox:GetText() end
 if materialSearch then panelDB.search=materialSearch:GetText() end
end
local function UpdateProtocolControls()
 if start then if HasCapability("START") then start:Enable() else start:Disable() end end
 if materialStart then if HasCapability("START_MATERIAL") or HasCapability("START_FISHING") then materialStart:Enable() else materialStart:Disable() end end
 if fishingZoneStart then if HasCapability("START_FISHING") then fishingZoneStart:Enable() else fishingZoneStart:Disable() end end
 if stop then if bridgeReady and HasCapability("STOP") then stop:Enable() else stop:Disable() end end
end
local function RequestMaterialSources(name)
 if not bridgeReady or not HasCapability("MATERIAL_SOURCES") then return end
 materialSourceCount=0;materialSourceTotal=0
 local requestId=SendProtocol("MATERIAL_SOURCES",{"material",name})
 materialSourceRequestId=requestId
end
SendProtocol=function(opcode, fields)
  nextRequestId=(nextRequestId % 999999999)+1
  local requestId=nextRequestId
  local payload="1\t"..opcode.."\t"..requestId
  for _,field in ipairs(fields or {}) do payload=payload.."\t"..tostring(field) end
  local channel,target="WHISPER",UnitName("player")
  if GetNumRaidMembers and GetNumRaidMembers()>0 then channel,target="RAID",nil
  elseif GetNumPartyMembers and GetNumPartyMembers()>0 then channel,target="PARTY",nil end
  SendAddonMessage("JLYRPG2",payload,channel,target)
  return requestId
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
local function ResourceIcon(name) return ItemIcon(resourceItemIds[name]) end
local function StyleInputBox(box)
 box:SetFontObject(GameFontHighlightSmall)
 box:SetBackdrop({bgFile="Interface\\Tooltips\\UI-Tooltip-Background",edgeFile="Interface\\Tooltips\\UI-Tooltip-Border",edgeSize=8,insets={left=3,right=3,top=3,bottom=3}})
 box:SetBackdropColor(0,0,0,.88);box:SetBackdropBorderColor(.42,.42,.42,1);box:SetTextInsets(4,4,0,0);box:SetJustifyH("CENTER")
end
local resourceButton=CreateFrame("Button","SelfBotRPGResourceButton",addon,"UIPanelButtonTemplate");resourceButton:SetSize(180,24);resourceButton:SetPoint("TOPRIGHT",-34,-79);resourceButton:SetText(resource)
resourceButton.icon=resourceButton:CreateTexture(nil,"ARTWORK");resourceButton.icon:SetSize(16,16);resourceButton.icon:SetPoint("LEFT",6,0);resourceButton.icon:SetTexCoord(.08,.92,.08,.92);resourceButton.icon:SetTexture(ResourceIcon(resource))
local resourceButtonText=resourceButton:GetFontString();resourceButtonText:ClearAllPoints();resourceButtonText:SetPoint("LEFT",resourceButton.icon,"RIGHT",5,0);resourceButtonText:SetPoint("RIGHT",resourceButton,"RIGHT",-8,0);resourceButtonText:SetJustifyH("LEFT")
local resourcePopup=CreateFrame("Frame","SelfBotRPGResourcePopup",UIParent);resourcePopup:SetSize(245,195);resourcePopup:SetPoint("TOPLEFT",resourceButton,"BOTTOMLEFT",0,-3);resourcePopup:SetFrameStrata("DIALOG");resourcePopup:SetBackdrop({bgFile="Interface\\Tooltips\\UI-Tooltip-Background",edgeFile="Interface\\Tooltips\\UI-Tooltip-Border",edgeSize=12,insets={left=3,right=3,top=3,bottom=3}});resourcePopup:SetBackdropColor(0,0,0,.96);resourcePopup:Hide()
local resourceScroll=CreateFrame("ScrollFrame","SelfBotRPGResourceScroll",resourcePopup,"UIPanelScrollFrameTemplate");resourceScroll:SetPoint("TOPLEFT",8,-8);resourceScroll:SetPoint("BOTTOMRIGHT",-28,8);resourceScroll:EnableMouseWheel(true);resourceScroll:SetScript("OnMouseWheel",function(self,delta)self:SetVerticalScroll(math.max(0,math.min(self:GetVerticalScrollRange(),self:GetVerticalScroll()-delta*20)))end)
local resourceChild=CreateFrame("Frame",nil,resourceScroll);resourceChild:SetWidth(205);resourceScroll:SetScrollChild(resourceChild)
local resourceRows={}
local function SetResource(v) resource=v;resourceButton:SetText(v);resourceButton.icon:SetTexture(ResourceIcon(v));panelDB.resource=resource;resourcePopup:Hide() end
local function RefreshResourceMenu()
 for _,row in ipairs(resourceRows) do row:Hide() end
 local values={};for _,n in ipairs(resources[profession] or {}) do table.insert(values,n) end
 if profession~="both" then table.insert(values,"Zone") end
 for index,name in ipairs(values) do
  local row=resourceRows[index]
  if not row then
   row=CreateFrame("Button",nil,resourceChild);row:SetSize(205,22);row.icon=row:CreateTexture(nil,"ARTWORK");row.icon:SetSize(18,18);row.icon:SetPoint("LEFT",3,0);row.text=row:CreateFontString(nil,"OVERLAY","GameFontHighlight");row.text:SetPoint("LEFT",row.icon,"RIGHT",6,0);row.highlight=row:CreateTexture(nil,"HIGHLIGHT");row.highlight:SetTexture("Interface\\Buttons\\WHITE8x8");row.highlight:SetVertexColor(.25,.22,.10,.35);row.highlight:SetAllPoints();resourceRows[index]=row
  end
  row:ClearAllPoints();row:SetPoint("TOPLEFT",resourceChild,"TOPLEFT",0,-(index-1)*22);row.text:SetText(name);row.icon:SetTexture(ResourceIcon(name));row:SetScript("OnClick",function()SetResource(name)end);row:SetScript("OnEnter",function()GameTooltip:SetOwner(row,"ANCHOR_RIGHT");GameTooltip:SetText(name,1,.82,.22,true);GameTooltip:AddLine("Select this node resource for farming.",.8,.8,.8,true);GameTooltip:Show()end);row:SetScript("OnLeave",function()GameTooltip:Hide()end);row:Show()
 end
 resourceChild:SetHeight(math.max(1,#values*22))
end
resourceButton:SetScript("OnClick",function()if resourcePopup:IsShown() then resourcePopup:Hide() else RefreshResourceMenu();resourcePopup:Show()end end)
resourceButton:SetScript("OnEnter",function()GameTooltip:SetOwner(resourceButton,"ANCHOR_RIGHT");GameTooltip:SetText("Node resource",1,.82,.22,true);GameTooltip:AddLine("Select a node type from the scrollable list.",.8,.8,.8,true);GameTooltip:Show()end);resourceButton:SetScript("OnLeave",function()GameTooltip:Hide()end)
UIDropDownMenu_Initialize(pd,function() local i=UIDropDownMenu_CreateInfo();for _,n in ipairs({"Mining","Herbalism","Both"}) do local v=string.lower(n);i.text=n;i.checked=v==profession;i.func=function()profession=v;panelDB.profession=profession;SetResource(resources[v][1]);RefreshResourceMenu();UIDropDownMenu_SetText(pd,n)end;UIDropDownMenu_AddButton(i)end end);UIDropDownMenu_SetWidth(pd,110);UIDropDownMenu_SetText(pd,"Mining")
pd:SetScript("OnEnter",function()GameTooltip:SetOwner(pd,"ANCHOR_RIGHT");GameTooltip:SetText("Node profession",1,.82,.22,true);GameTooltip:AddLine("Select which gathering profession controls node farming.",.8,.8,.8,true);GameTooltip:Show()end);pd:SetScript("OnLeave",function()GameTooltip:Hide()end)
RefreshResourceMenu()
-- Static IDs keep familiar icons visible before the server catalog arrives.
-- The server-provided catalog replaces these with authoritative IDs when loaded.
local materialIds={
 ["Oily Blackmouth"]=ReagentData and ReagentData.OilyBlackmouth or  UserData and UserData.OilyBlackmouth or 6358,["Firefin Snapper"]=6359,["Deviate Fish"]=6522,["Raw Loch Frenzy"]=7756,["Rainbow Fin Albacore"]=8364,["Winter Squid"]=13755,["Raw Summer Bass"]=13756,["Lightning Eel"]=13757,["Raw Redgill"]=13758,["Raw Nightfin Snapper"]=13759,["Raw Sunscale Salmon"]=13760,["Barbed Gill Trout"]=27422,["Spotted Feltail"]=27425,["Zangarian Sporefish"]=27429,["Golden Darter"]=27438,["Furious Crawdad"]=27439,["Deep Sea Monsterbelly"]=41800,["Moonglow Cuttlefish"]=41801,["Imperial Manta Ray"]=41802,["Rockfin Grouper"]=41803,["Borean Man O' War"]=34760,
 ["Linen Cloth"]=2589,["Wool Cloth"]=2592,["Silk Cloth"]=4306,["Mageweave Cloth"]=4338,["Runecloth"]=14047,["Netherweave Cloth"]=21877,["Frostweave Cloth"]=33470,
 ["Light Leather"]=2318,["Medium Leather"]=2319,["Heavy Leather"]=4234,["Thick Leather"]=4235,["Rugged Leather"]=4304,["Knothide Leather"]=21887,["Borean Leather"]=33568,
 ["Mote of Air"]=22572,["Mote of Earth"]=22573,["Mote of Fire"]=22574,["Mote of Life"]=22575,["Mote of Mana"]=22576,["Mote of Shadow"]=22577,["Mote of Water"]=22578,
 ["Primal Fire"]=21884,["Primal Water"]=21885,["Primal Life"]=21886,["Primal Air"]=22451,["Primal Earth"]=22452,["Primal Shadow"]=22456,["Primal Mana"]=22457,
}
local materialMeta={}
local fishingPicker=false
local fishingMaterialIds={
 [6358]=true,[6359]=true,[6522]=true,[7756]=true,[8364]=true,
 [13755]=true,[13756]=true,[13757]=true,[13758]=true,[13759]=true,[13760]=true,
 [27422]=true,[27425]=true,[27429]=true,[27438]=true,[27439]=true,
 [41800]=true,[41801]=true,[41802]=true,[41803]=true,[34760]=true
}
local function IsFishingMaterial(name)
 local meta=materialMeta[name]
 if meta and string.lower(meta.family or "")=="fishing" then return true end
 return fishingMaterialIds[tonumber(materialIds[name] or 0)] == true
end
materialSearch=CreateFrame("EditBox",nil,addon);materialSearch:SetWidth(125);materialSearch:SetHeight(22);materialSearch:SetPoint("TOPRIGHT",-38,-139);materialSearch:SetAutoFocus(false);materialSearch:SetText("");StyleInputBox(materialSearch);materialSearch:SetJustifyH("LEFT");materialSearch:SetTextInsets(6,6,0,0)
local materialLabel=addon:CreateFontString(nil,"OVERLAY","GameFontHighlightSmall");materialLabel:SetPoint("TOPLEFT",28,-123);materialLabel:SetText("Material target")
local materialSearchLabel=addon:CreateFontString(nil,"OVERLAY","GameFontHighlightSmall");materialSearchLabel:SetWidth(125);materialSearchLabel:SetJustifyH("RIGHT");materialSearchLabel:SetPoint("TOPRIGHT",-38,-123);materialSearchLabel:SetText("Search Material Catalog")
local materialButton=CreateFrame("Button","SelfBotRPGMaterialButton",addon,"UIPanelButtonTemplate");materialButton:SetSize(180,24);materialButton:SetPoint("TOPLEFT",18,-139);materialButton:SetText(material)
local materialButtonIcon=materialButton:CreateTexture(nil,"ARTWORK");materialButtonIcon:SetSize(16,16);materialButtonIcon:SetPoint("LEFT",6,0);materialButtonIcon:SetTexCoord(.08,.92,.08,.92);materialButtonIcon:SetTexture(ItemIcon(materialIds[material]))
local materialButtonText=materialButton:GetFontString();materialButtonText:ClearAllPoints();materialButtonText:SetPoint("LEFT",materialButtonIcon,"RIGHT",5,0);materialButtonText:SetPoint("RIGHT",materialButton,"RIGHT",-8,0);materialButtonText:SetJustifyH("LEFT")
local materialPopup=CreateFrame("Frame","SelfBotRPGMaterialPopup",UIParent);materialPopup:SetSize(285,195);materialPopup:SetPoint("TOPLEFT",materialButton,"BOTTOMLEFT",0,-3);materialPopup:SetFrameStrata("DIALOG");materialPopup:SetBackdrop({bgFile="Interface\\Tooltips\\UI-Tooltip-Background",edgeFile="Interface\\Tooltips\\UI-Tooltip-Border",edgeSize=12,insets={left=3,right=3,top=3,bottom=3}});materialPopup:SetBackdropColor(0,0,0,.96);materialPopup:Hide()
addon:SetScript("OnUpdate",function()
 local over=addon:IsShown() and (materialPopup:IsShown() or resourcePopup:IsShown() or FrameUnderCursor(addon) or FrameUnderCursor(materialPopup) or FrameUnderCursor(resourcePopup))
 for i=1,3 do local list=_G["DropDownList"..i];if FrameUnderCursor(list) then over=true;break end end
 addon:SetAlpha(over and 1 or .55)
end)
local materialScroll=CreateFrame("ScrollFrame","SelfBotRPGMaterialScroll",materialPopup,"UIPanelScrollFrameTemplate");materialScroll:SetPoint("TOPLEFT",8,-8);materialScroll:SetPoint("BOTTOMRIGHT",-28,8)
local materialChild=CreateFrame("Frame",nil,materialScroll);materialChild:SetWidth(245);materialScroll:SetScrollChild(materialChild)
local materialRows={}
local function SetMaterial(v)
 if fishingPicker then fishingTarget=v;fishingTargetButton:SetText(v);panelDB.fishingTarget=v;RequestMaterialSources(v);materialPopup:Hide();return end
 material=v;materialButton:SetText(v);materialButtonIcon:SetTexture(ItemIcon(materialIds[v]));panelDB.material=material;RequestMaterialSources(v);materialPopup:Hide()
end
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
  local allowed
  if fishingPicker then allowed=IsFishingMaterial(name) else allowed=not IsFishingMaterial(name) end
  if allowed and (filter=="" or string.find(string.lower(name),filter,1,true)) then
   shown=shown+1;local row=materialRows[shown]
   if not row then
    row=CreateFrame("Button",nil,materialChild);row:SetSize(245,22);row.icon=row:CreateTexture(nil,"ARTWORK");row.icon:SetSize(18,18);row.icon:SetPoint("LEFT",3,0);row.text=row:CreateFontString(nil,"OVERLAY","GameFontHighlight");row.text:SetPoint("LEFT",row.icon,"RIGHT",6,0);row.text:SetJustifyH("LEFT");row.highlight=row:CreateTexture(nil,"HIGHLIGHT");row.highlight:SetTexture("Interface\\QuestFrame\\UI-QuestTitleHighlight");row.highlight:SetBlendMode("ADD");row.highlight:SetAllPoints();materialRows[shown]=row
   end
   row:ClearAllPoints();row:SetPoint("TOPLEFT",materialChild, "TOPLEFT",0,-(shown-1)*22);row.text:SetText(name);row.icon:SetTexture(ItemIcon(materialIds[name]));row:SetScript("OnClick",function()SetMaterial(name)end);MaterialTooltip(row,name);row:Show()
  end
 end
 materialChild:SetHeight(math.max(1,shown*22))
end
materialButton:SetScript("OnClick",function()
 fishingPicker=false
 materialSearchLabel:SetText("Search Material Catalog")
 materialPopup:ClearAllPoints();materialPopup:SetPoint("TOPLEFT",materialButton,"BOTTOMLEFT",0,-3)
 if materialPopup:IsShown() then materialPopup:Hide() else RefreshMaterialMenu();materialPopup:Show() end
end)
local fishingTargetLabel=addon:CreateFontString(nil,"OVERLAY","GameFontHighlightSmall");fishingTargetLabel:SetPoint("TOPLEFT",28,-165);fishingTargetLabel:SetText("Fishing target")
fishingTargetButton=CreateFrame("Button","SelfBotRPGFishingTargetButton",addon,"UIPanelButtonTemplate");fishingTargetButton:SetSize(180,24);fishingTargetButton:SetPoint("TOPLEFT",18,-181);fishingTargetButton:SetText(fishingTarget~="" and fishingTarget or "Select fish")
fishingTargetButton:SetScript("OnClick",function()
 fishingPicker=true
 materialSearchLabel:SetText("Search Fish Catalog")
 materialPopup:ClearAllPoints();materialPopup:SetPoint("TOPLEFT",fishingTargetButton,"BOTTOMLEFT",0,-3)
 if materialPopup:IsShown() then materialPopup:Hide() else RefreshMaterialMenu();materialPopup:Show() end
end)
fishingTargetButton:SetScript("OnEnter",function(self)GameTooltip:SetOwner(self,"ANCHOR_RIGHT");GameTooltip:SetText("Fishing target",1,.82,.22,true);GameTooltip:AddLine("Select a fish for targeted fishing, or use Fish This Zone.",.8,.8,.8,true);GameTooltip:Show()end);fishingTargetButton:SetScript("OnLeave",function()GameTooltip:Hide()end)
materialButton:SetScript("OnEnter",function(self)GameTooltip:SetOwner(self,"ANCHOR_RIGHT");GameTooltip:SetText("Material target",1,.82,.22,true);GameTooltip:AddLine("Select an exact catalog item to farm.",.8,.8,.8,true);GameTooltip:AddLine("Use Search to filter the scrollable list.",.8,.8,.8,true);GameTooltip:Show()end);materialButton:SetScript("OnLeave",function()GameTooltip:Hide()end)
materialSearch:SetScript("OnTextChanged",function()panelDB.search=materialSearch:GetText() or "";RefreshMaterialMenu()end);materialSearch:SetScript("OnEnter",function()GameTooltip:SetOwner(materialSearch,"ANCHOR_RIGHT");GameTooltip:SetText("Search material catalog",1,.82,.22,true);GameTooltip:AddLine("Type part of a name, then open the scrollable picker.",.8,.8,.8,true);GameTooltip:Show()end);materialSearch:SetScript("OnLeave",function()GameTooltip:Hide()end);RefreshMaterialMenu()
local durationRow=CreateFrame("Frame",nil,addon);durationRow:SetSize(340,22);durationRow:SetPoint("TOP",0,-218)
local durationLabel=durationRow:CreateFontString(nil,"OVERLAY","GameFontHighlightSmall");durationLabel:SetWidth(270);durationLabel:SetPoint("LEFT",0,0);durationLabel:SetJustifyH("RIGHT");durationLabel:SetText("Duration (minutes; 0 = unlimited)")
durationBox=CreateFrame("EditBox",nil,durationRow);durationBox:SetWidth(75);durationBox:SetHeight(22);durationBox:SetPoint("LEFT",durationLabel,"RIGHT",10,0);durationBox:SetAutoFocus(false);durationBox:SetNumeric(true);durationBox:SetMaxLetters(5);durationBox:SetText("0");StyleInputBox(durationBox)
durationBox:SetScript("OnEnter",function()GameTooltip:SetOwner(durationBox,"ANCHOR_RIGHT");GameTooltip:SetText("Duration",1,.82,.22,true);GameTooltip:AddLine("0 or empty runs until quantity, bag, or manual stop.",.8,.8,.8,true);GameTooltip:Show()end);durationBox:SetScript("OnLeave",function()GameTooltip:Hide()end);durationBox:HookScript("OnTextChanged",SavePanelFields)
local quantityRow=CreateFrame("Frame",nil,addon);quantityRow:SetSize(300,22);quantityRow:SetPoint("TOP",0,-248)
local quantityLabel=quantityRow:CreateFontString(nil,"OVERLAY","GameFontHighlightSmall");quantityLabel:SetWidth(230);quantityLabel:SetPoint("LEFT",0,0);quantityLabel:SetJustifyH("RIGHT");quantityLabel:SetText("Quantity goal  (0 = unlimited)")
quantityBox=CreateFrame("EditBox",nil,quantityRow);quantityBox:SetWidth(75);quantityBox:SetHeight(22);quantityBox:SetPoint("LEFT",quantityLabel,"RIGHT",10,0);quantityBox:SetAutoFocus(false);quantityBox:SetNumeric(true);quantityBox:SetMaxLetters(6);quantityBox:SetText("0");StyleInputBox(quantityBox)
quantityBox:SetScript("OnEnter",function()GameTooltip:SetOwner(quantityBox,"ANCHOR_RIGHT");GameTooltip:SetText("Quantity goal",1,.82,.22,true);GameTooltip:AddLine("Stops material farming after this many requested items.",.8,.8,.8,true);GameTooltip:Show()end);quantityBox:SetScript("OnLeave",function()GameTooltip:Hide()end);quantityBox:HookScript("OnTextChanged",SavePanelFields)
if panelDB.duration then durationBox:SetText(panelDB.duration) end
if panelDB.quantity then quantityBox:SetText(panelDB.quantity) end
if panelDB.search then materialSearch:SetText(panelDB.search) end
SavePanelFields()
status=addon:CreateFontString(nil,"OVERLAY","GameFontHighlightSmall");status:SetPoint("TOP",0,-292);status:SetWidth(420);status:SetHeight(44);status:SetWordWrap(true);status:SetJustifyH("CENTER");status:SetJustifyV("TOP");status:SetText("SelfBot RPG: waiting for status")
local lastStatusRequest=0
local function RequestStatus()
  local now=GetTime()
  if now-lastStatusRequest<2 then return end
  lastStatusRequest=now
  Command(".sbrpg status")
  if bridgeReady then SendProtocol("MATERIAL_STATUS",{}) end
end
addon:RegisterEvent("CHAT_MSG_ADDON");addon:RegisterEvent("PLAYER_LOGIN");addon:RegisterEvent("PLAYER_LOGOUT");addon:SetScript("OnEvent",function(_,event,prefix,message)
 if event=="PLAYER_LOGOUT" then
  SelfBotRPGDB.Settings=SelfBotRPGDB.Settings or {}
  for key,box in pairs(settings) do SelfBotRPGDB.Settings[key]=box:GetText() end
  return
 end
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
   status:SetText("SelfBot RPG: protocol connected; waiting for capabilities")
 elseif parts[2]=="CAPABILITIES" then
   bridgeReady=true
   capabilities={}
   for capability in string.gmatch(parts[5] or "","[^,]+") do capabilities[capability]=true end
   UpdateProtocolControls()
   if ApplySavedSettings then ApplySavedSettings() end
   if HasCapability("MATERIAL_CATALOG") then SendProtocol("MATERIAL_CATALOG",{}) end
   if HasCapability("MATERIAL_SOURCES") then RequestMaterialSources(material) end
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
 elseif parts[2]=="MATERIAL_SOURCE" then
   local requestId,index,total,creatureEntry,method,chance,kind=unpack(parts,3)
   if tostring(requestId)==tostring(materialSourceRequestId) then
    materialSourceCount=materialSourceCount+1;materialSourceTotal=tonumber(total or "0") or 0
   end
 elseif parts[2]=="MATERIAL_SOURCES_END" then
   local requestId,total=unpack(parts,3)
   if tostring(requestId)==tostring(materialSourceRequestId) then
    materialSourceTotal=tonumber(total or "0") or materialSourceTotal
    if materialSourceTotal==0 then status:SetText("Material "..material..": no indexed sources") end
   end
 elseif parts[2]=="MATERIAL_STATUS" then
   local requestId,active,itemId,gathered,goal,kills,remaining,harvest,phase=unpack(parts,3)
   if active=="0" then
     status:SetText("SelfBot RPG: no material run active")
   else
     local goalText=(tonumber(goal or "0") or 0)>0 and (" / "..goal) or ""
     local phaseText=(phase and phase~="") and (" | "..phase) or ""
     status:SetText("Material "..itemId..": "..gathered..goalText.." items | "..kills.." kills | "..FormatDuration(remaining).." remaining"..phaseText)
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
       timerText=" | "..FormatDuration(remain).." left"
     end
     status:SetText(phase..reasonText.." | "..profession..": "..nodes.." nodes | "..gathers.." gathers / "..items.." items | "..perMin.."/min "..perSec.."/sec"..timerText..targetText)
   end
 elseif parts[2]=="SETTING" then
   if settings[parts[3]] then settings[parts[3]]:SetText(parts[4]) end
   status:SetText("SelfBot RPG: applied "..(parts[3] or "setting").." = "..(parts[4] or ""))
 elseif parts[2]=="ERROR" then status:SetText("Error: "..(parts[4] or parts[3] or "unknown"))
 elseif parts[2]=="DEBUG" then status:SetText(parts[3] or "") end
end)
addon:SetScript("OnShow",function() addon:SetAlpha(1);RequestStatus() end)
-- Status requests are deliberately limited to panel opens; periodic requests
-- are not emitted by the fallback UI.
SelfBotRPGSettingsFrame=CreateFrame("Frame","SelfBotRPGSettingsFrame",UIParent)
local settingsPanel=SelfBotRPGSettingsFrame
settingsPanel:SetSize(390,485);settingsPanel:SetFrameStrata("DIALOG");settingsPanel:SetMovable(true);settingsPanel:EnableMouse(true)
local settingsDB=SelfBotRPGDB.Settings or {};SelfBotRPGDB.Settings=settingsDB
if settingsDB.x and settingsDB.y then settingsPanel:SetPoint("CENTER",UIParent,"BOTTOMLEFT",settingsDB.x,settingsDB.y) else settingsPanel:SetPoint("CENTER",addon,"CENTER",0,0) end
settingsPanel:RegisterForDrag("LeftButton");settingsPanel:SetScript("OnDragStart",settingsPanel.StartMoving);settingsPanel:SetScript("OnDragStop",function(self)self:StopMovingOrSizing();local x,y=self:GetCenter();local scale=UIParent:GetEffectiveScale() or 1;settingsDB.x=x*scale;settingsDB.y=y*scale end)
settingsPanel:SetBackdrop({bgFile="Interface/Tooltips/UI-Tooltip-Background",edgeFile="Interface/Tooltips/UI-Tooltip-Border",edgeSize=12,insets={left=3,right=3,top=3,bottom=3}});settingsPanel:SetBackdropColor(0,0,0,.94);settingsPanel:Hide()
local st=settingsPanel:CreateFontString(nil,"OVERLAY","GameFontNormalLarge");st:SetPoint("TOP",0,-12);st:SetText("SelfBot RPG Settings")
local sc=CreateFrame("Button",nil,settingsPanel,"UIPanelCloseButton");sc:SetPoint("TOPRIGHT",2,2)
local settingsHint=settingsPanel:CreateFontString(nil,"OVERLAY","GameFontHighlightSmall");settingsHint:SetPoint("TOP",0,-36);settingsHint:SetText("Module configuration; Apply updates the current session and future runs.")
local settingDefaults={enable="1",debug="0",minchance="1.0",bagreserve="0",attempts="3",failedblacklist="120",emptyblacklist="120",zone="1",settledelay="1000",actiondelay="1000",bobbers="35591",uselures="1",prioritizepools="0",openwateronly="1",searchdistance="500.0",castdistance="12.0"}
local configKeys={"enable","debug","minchance","bagreserve","attempts","failedblacklist","emptyblacklist","zone","settledelay","actiondelay","bobbers","uselures","prioritizepools","openwateronly","searchdistance","castdistance"}
local configLabels={enable="Module enabled (1/0)",debug="Debug logging (1/0)",minchance="Minimum loot chance (%)",bagreserve="Reserved bag space (%)",attempts="Attempts before blacklist (1–10)",failedblacklist="Failed-node blacklist seconds",emptyblacklist="Empty-node blacklist seconds",zone="Stay in starting zone (1/0)",settledelay="Gather settle delay (ms)",actiondelay="Action delay (ms)",bobbers="Fishing bobber entries",uselures="Use fishing lures (1/0)",prioritizepools="Prioritize fishing pools (1/0)",openwateronly="Open water only (1/0)",searchdistance="Fishing search distance",castdistance="Fishing cast distance"}
local function SaveSettings()
 for key,box in pairs(settings) do settingsDB[key]=box:GetText() end
end
local function ApplySettings()
 SaveSettings()
 if not bridgeReady then status:SetText("SelfBot RPG: connecting protocol…");SendProtocol("HELLO",{});return end
 if not HasCapability("SET_CONFIG") then status:SetText("SelfBot RPG: server does not support settings.");return end
 for _,key in ipairs(configKeys) do
  local value=settings[key] and settings[key]:GetText() or ""
  if value~="" then SendProtocol("SET_CONFIG",{key,value}) end
 end
 -- Preserve the existing per-run protocol for active node-farming state.
 for _,key in ipairs({"attempts","failedblacklist","emptyblacklist","zone","settledelay"}) do
  local value=settings[key] and settings[key]:GetText() or ""
  if value~="" then SendProtocol("SET",{key,value}) end
 end
 status:SetText("SelfBot RPG: settings applied")
end
ApplySavedSettings=function()
 if not bridgeReady or not HasCapability("SET_CONFIG") then return end
 for _,key in ipairs(configKeys) do
  local value=settingsDB[key] or settingDefaults[key]
  if value~="" then SendProtocol("SET_CONFIG",{key,value}) end
 end
end
local function ResetSettings()
 for key,defaultValue in pairs(settingDefaults) do
  if settings[key] then settings[key]:SetText(defaultValue) end
 end
 SaveSettings()
 status:SetText("SelfBot RPG: settings reset to defaults")
end
local function Setting(label,key,y,numeric)
 local text=settingsPanel:CreateFontString(nil,"OVERLAY","GameFontNormalSmall");text:SetPoint("TOPLEFT",20,y);text:SetText(label)
 local box=CreateFrame("EditBox",nil,settingsPanel);box:SetSize(105,22);box:SetPoint("TOPRIGHT",-32,y+3);box:SetAutoFocus(false);if numeric then box:SetNumeric(true) end;box:SetMaxLetters(32);box:SetText(settingsDB[key] or settingDefaults[key]);StyleInputBox(box)
 settings[key]=box
 box:SetScript("OnEnterPressed",function(self)self:ClearFocus();SaveSettings()end)
 box:SetScript("OnTextChanged",function()SaveSettings()end)
 box:SetScript("OnEnter",function(self)GameTooltip:SetOwner(self,"ANCHOR_RIGHT");GameTooltip:SetText(label,1,.82,.22,true);GameTooltip:AddLine("Saved locally; press Apply to send it to the active run.",.8,.8,.8,true);GameTooltip:Show()end)
 box:SetScript("OnLeave",function()GameTooltip:Hide()end)
end
local settingOrder={"enable","debug","minchance","bagreserve","attempts","failedblacklist","emptyblacklist","zone","settledelay","actiondelay","bobbers","uselures","prioritizepools","openwateronly","searchdistance","castdistance"}
for index,key in ipairs(settingOrder) do
 Setting(configLabels[key],key,-58-(index-1)*24,key~="bobbers" and key~="minchance" and key~="searchdistance" and key~="castdistance")
end
local applySettings=CreateFrame("Button",nil,settingsPanel,"UIPanelButtonTemplate");applySettings:SetSize(105,24);applySettings:SetPoint("BOTTOMLEFT",18,15);applySettings:SetText("Apply");applySettings:SetScript("OnClick",ApplySettings)
local resetSettings=CreateFrame("Button",nil,settingsPanel,"UIPanelButtonTemplate");resetSettings:SetSize(105,24);resetSettings:SetPoint("BOTTOM",0,15);resetSettings:SetText("Reset Defaults");resetSettings:SetScript("OnClick",ResetSettings)
local closeSettings=CreateFrame("Button",nil,settingsPanel,"UIPanelButtonTemplate");closeSettings:SetSize(85,24);closeSettings:SetPoint("BOTTOMRIGHT",-18,15);closeSettings:SetText("Close");closeSettings:SetScript("OnClick",CloseSettingsWindow)
settingsPanel:SetScript("OnShow",function()RequestStatus();for key,box in pairs(settings) do box:SetText(settingsDB[key] or settingDefaults[key]) end end)
local function CloseMainWindow()
 materialPopup:Hide();addon:Hide()
end
local function CloseSettingsWindow()
 ApplySettings();settingsPanel:Hide()
end
local function HandleInputKey(self,key,saveFields)
 if key=="ENTER" then
  self:ClearFocus()
  if saveFields then SavePanelFields() else SaveSettings() end
  if self.SetPropagateKeyboardInput then self:SetPropagateKeyboardInput(false) end
 elseif key=="ESCAPE" then
  self:ClearFocus()
  if settingsPanel:IsShown() then CloseSettingsWindow() else CloseMainWindow() end
  if self.SetPropagateKeyboardInput then self:SetPropagateKeyboardInput(false) end
 end
end
addon:EnableKeyboard(true);addon:SetScript("OnKeyDown",function(self,key)if key=="ESCAPE" then CloseMainWindow();if self.SetPropagateKeyboardInput then self:SetPropagateKeyboardInput(false) end end end)
settingsPanel:EnableKeyboard(true);settingsPanel:SetScript("OnKeyDown",function(self,key)if key=="ESCAPE" then CloseSettingsWindow();if self.SetPropagateKeyboardInput then self:SetPropagateKeyboardInput(false) end end end)
for _,box in ipairs({materialSearch,durationBox,quantityBox}) do box:SetScript("OnEnterPressed",function(self)HandleInputKey(self,"ENTER",true)end);box:SetScript("OnKeyDown",function(self,key)HandleInputKey(self,key,true)end) end
for _,box in pairs(settings) do box:SetScript("OnKeyDown",function(self,key)HandleInputKey(self,key,false)end) end
local gear=CreateFrame("Button",nil,addon);gear:SetSize(22,22);gear:SetPoint("RIGHT",close,"LEFT",-2,0);gear:SetNormalTexture("Interface\\Buttons\\UI-OptionsButton");gear:SetHighlightTexture("Interface\\Buttons\\UI-OptionsButton");gear:SetScript("OnClick",function()settingsPanel:Show();settingsPanel:SetToplevel(true);settingsPanel:Raise()end)
sc:HookScript("OnClick",CloseSettingsWindow)
start=CreateFrame("Button",nil,addon,"UIPanelButtonTemplate");start:SetSize(100,24);start:SetPoint("BOTTOMLEFT",12,15);start:SetText("Start Farming");start:SetScript("OnClick",function()
 if not bridgeReady then status:SetText("SelfBot RPG: connecting protocol…");SendProtocol("HELLO",{});return end
 local duration=durationBox:GetText() or ""
 if duration~="" and (not tonumber(duration) or tonumber(duration)<0 or tonumber(duration)>10080) then status:SetText("Duration must be 0–10080 minutes.");return end
 if duration=="" then duration="0" end
 if resource=="Zone" and profession~="both" then SendProtocol("START",{"zone",profession,duration}) else SendProtocol("START",{profession,resource,duration}) end
 -- A setting issued before farming is deliberately rejected by the server;
 -- send the complete override set immediately after starting the run instead.
 for _,key in ipairs({"attempts","failedblacklist","emptyblacklist","zone","settledelay"}) do
  if settings[key] then SendProtocol("SET",{key,settings[key]:GetText()}) end
 end
 SendProtocol("STATUS",{})
end)
materialStart=CreateFrame("Button",nil,addon,"UIPanelButtonTemplate");materialStart:SetSize(100,24);materialStart:SetPoint("BOTTOMLEFT",117,15);materialStart:SetText("Start Selected");materialStart:SetScript("OnClick",function()
 if not bridgeReady then status:SetText("SelfBot RPG: connecting protocol…");SendProtocol("HELLO",{});return end
 local duration=tonumber(durationBox:GetText() or "0") or 0
 local quantity=tonumber(quantityBox:GetText() or "0") or 0
 if duration<0 or duration>10080 or quantity<0 then status:SetText("Invalid duration or quantity.");return end
 local meta=materialMeta[material]
 if fishingTarget~="" then
  if not HasCapability("START_FISHING") then status:SetText("Fishing is unavailable on the server.");return end
  SendProtocol("START_FISHING",{"fish",fishingTarget,tostring(duration),tostring(quantity),settings.prioritizepools:GetText(),settings.openwateronly:GetText()})
 else
  if material=="" or material=="Select material" then status:SetText("Select a material before starting.");return end
  if not HasCapability("START_MATERIAL") then status:SetText("Material farming is unavailable on the server.");return end
  SendProtocol("START_MATERIAL",{"material",material,tostring(duration),tostring(quantity)})
 end
end)
fishingZoneStart=CreateFrame("Button",nil,addon,"UIPanelButtonTemplate");fishingZoneStart:SetSize(100,24);fishingZoneStart:SetPoint("BOTTOMLEFT",222,15);fishingZoneStart:SetText("Fish This Zone");fishingZoneStart:SetScript("OnClick",function()
 if not bridgeReady or not HasCapability("START_FISHING") then status:SetText("Zone fishing is unavailable on the server.");return end
 local duration=tonumber(durationBox:GetText() or "0") or 0
 if duration<0 or duration>10080 then status:SetText("Duration must be 0–10080 minutes.");return end
 SendProtocol("START_FISHING",{"zone","current",tostring(duration),"0",settings.prioritizepools:GetText(),settings.openwateronly:GetText()})
end)
stop=CreateFrame("Button",nil,addon,"UIPanelButtonTemplate");stop:SetSize(100,24);stop:SetPoint("BOTTOMLEFT",327,15);stop:SetText("Stop");stop:SetScript("OnClick",function()Command(".sbrpg stop")end)
UpdateProtocolControls()
local function ButtonTooltip(button,titleText,bodyText)
 button:SetScript("OnEnter",function(self)GameTooltip:SetOwner(self,"ANCHOR_TOP");GameTooltip:SetText(titleText,1,.82,.22,true);GameTooltip:AddLine(bodyText,.8,.8,.8,true);GameTooltip:Show()end);button:SetScript("OnLeave",function()GameTooltip:Hide()end)
end
ButtonTooltip(start,"Start node farming","Moves between mining/herbalism nodes using the selected profession and resource.")
ButtonTooltip(materialStart,"Start selected target","Starts material farming, or targeted fishing when the selected catalog entry is a fish.")
ButtonTooltip(fishingZoneStart,"Fish current zone","Fishes the current zone using open water by default, with optional pool prioritization.")
ButtonTooltip(stop,"Stop current run","Stops farming/material activity and restores normal playerbot strategies.")
ButtonTooltip(gear,"Open settings","Configure node-farming behavior. Apply sends settings to the active run; Reset Defaults restores local defaults.")
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
