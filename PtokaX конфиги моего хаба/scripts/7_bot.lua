tUserToBot = {
["1_[ÀŒ ¿À‹Õ€…]"] = {
["sDescription"] = " [‘Œ–”Ã]",
["sNeedClient"] = "FlylinkDC++ ",
["sNeedVersion"] = "333",
["sNeedConnection"] = "100",
["sNeedHubs"] = "3/0/0",
["sIP"] = "10.33.33.33",
["sMode"] = "A",
["sEmail"] = "DCBEELINEKZ@mail.ru",
["iShare"] = 3033,
},
-----------------------------------

["2_[¡≈—œ≈–≈¡Œ…Õ€…]"] = {
["sDescription"] = " [dcbeelinekz.1bb.ru]",
["sNeedClient"] = "FlylinkDC++ ",
["sNeedVersion"] = "333",
["sNeedConnection"] = "100",
["sNeedHubs"] = "3/0/0",
["sIP"] = "10.33.33.33",
["sMode"] = "A",
["sEmail"] = "DCBEELINEKZ@mail.ru",
["iShare"] = 3303,
},


["3_[¬]"] = {
["sDescription"] = " [—¿…“]",
["sNeedClient"] = "FlylinkDC++ ",
["sNeedVersion"] = "333",
["sNeedConnection"] = "100",
["sNeedHubs"] = "3/0/0",
["sIP"] = "10.33.33.33",
["sMode"] = "A",
["sEmail"] = "DCBEELINEKZ@mail.ru",
["iShare"] = 30303,
},

["4_[–≈∆»Ã≈]"] = {
["sDescription"] = " [dcbeelinekz.do.am]",
["sNeedClient"] = "FlylinkDC++ ",
["sNeedVersion"] = "333",
["sNeedConnection"] = "100",
["sNeedHubs"] = "3/0/0",
["sIP"] = "10.33.33.33",
["sMode"] = "A",
["sEmail"] = "DCBEELINEKZ@mail.ru",
["iShare"] = 3330,
},

["5_[¬≈◊ÕŒ√Œ]"] = {
["sDescription"] = " [√–”œœ¿ ¬ ]",
["sNeedClient"] = "FlylinkDC++ ",
["sNeedVersion"] = "333",
["sNeedConnection"] = "100",
["sNeedHubs"] = "3/0/0",
["sIP"] = "10.33.33.33",
["sMode"] = "A",
["sEmail"] = "DCBEELINEKZ@mail.ru",
["iShare"] = 303030,
},

["6_[ŒÕÀ¿…Õ¿]"] = {
["sDescription"] = " [vk.com/dchub_router]",
["sNeedClient"] = "FlylinkDC++ ",
["sNeedVersion"] = "333",
["sNeedConnection"] = "100",
["sNeedHubs"] = "3/0/0",
["sIP"] = "10.33.33.33",
["sMode"] = "A",
["sEmail"] = "DCBEELINEKZ@mail.ru",
["iShare"] = 33030,
},

["7_[—≈–¬≈–]"] = {
["sDescription"] = " [vk.com/dcbeelinekz]",
["sNeedClient"] = "FlylinkDC++ ",
["sNeedVersion"] = "333",
["sNeedConnection"] = "100",
["sNeedHubs"] = "3/0/0",
["sIP"] = "10.33.33.33",
["sMode"] = "A",
["sEmail"] = "DCBEELINEKZ@mail.ru",
["iShare"] = 330303,
},
}

------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
function OnStartup()
tNeedToSendAll = {}
DoNeedDescription()
SendFakeDescription()
TmrMan.AddTimer(1000, "OnTimer1")
end

function GetNickListArrival(curUser,sData)
tNeedToSendAll[curUser.sNick] = 1
end

function OnTimer1()
for sName in pairs(tNeedToSendAll) do
local tNeedUser = Core.GetUser(sName)
if tNeedUser and Core.GetUserValue(tNeedUser, 9) then
SendFakeDescription(tNeedUser)
tNeedToSendAll[sName] = nil
end
end
collectgarbage("collect")
end

function SendFakeDescription(curUser)
for i, v in pairs(tUserToBot) do
local sNewMyINFO = v.sMyINFO
local sIPInfo = "$UserIP "..i.." "..v["sIP"]
if not curUser then
Core.SendToAll(sNewMyINFO)
Core.SendToAll(sIPInfo)
else
Core.SendToUser(curUser, sNewMyINFO)
Core.SendToUser(curUser, sIPInfo)
end
end
end

function DoNeedDescription()
for i, v in pairs(tUserToBot) do
tUserToBot[i].sMyINFO = "$MyINFO $ALL "..i.." "..v.sDescription.."<"..(v["sNeedClient"] or "Bot'sDC++").." V:"..(v["sNeedVersion"] or "1.00")..",M:"..(v.sMode or "A")..",H:"..(v["sNeedHubs"] or "0/0/1")..",S:"..(v.iSlots or 0)..">$ $"..(v["sNeedConnection"] or "BOT").."$"..(v.sEmail or "").."$"..(v["iShare"] or 0).."$"
end
end
