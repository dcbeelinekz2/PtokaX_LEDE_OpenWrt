---------------------------------------------------------------------------------
--[[

                  ><(((*>  dchub://10.18.50.50:50  <*)))><
         ÏĞÎÂÅĞÈË ĞÀÁÎÒÓ ÑÊĞÈÏÒÀ ÍÀ 0.4.1.2 - 0.4.2.0 ÏĞÎÁËÅÌ ÍÅ ÍÀØÅË!
		                            ÏÎÌÅÒÊÀ:Â ÎÏÈÑÀÍÈÈ ÃÎĞÎÄÀ ÒÅĞÅ ÍÅ ÑÒÀÂÈÒÜ!
]]--
----------------------------------------------------------------------------------

Minutes =  1
tProfileTag = {
[1] = "",
[3] = "",
[2] = "",
[0] = "",
[-1] = ""
}

--               ÁËßËÀÉÍ
RangeDesc = {
	"127.0.0.1-127.0.0.255-[LOCALHOST]",
	"192.168.0.1-192.168.0.255-[LAN]",
	"10.10.0.1-10.25.255.255-[APPLE ÀÒÀ]", 
	
	"10.30.0.1-10.31.255.255-[ÓÑÒÜÊÀÌÀÍ]",
	"10.35.0.1-10.35.255.255-[ÑÅÌÅÉ]",
	"10.40.0.1-10.41.255.255-[ÏÀÂËÎÄÀĞ]",
	
	"10.45.0.1-10.47.255.255-[ÊÀĞÀÃÀÍÄÀ]",
	"10.50.0.1-10.51.255.255-[ÓĞÀËÜÑÊ]",
	"10.55.0.1-10.57.255.255-[ÏİÒĞÎ Ï]",
	
	"10.60.0.1-10.61.255.255-[ÀÊÒÎÁÅ]",
	"10.65.0.1-10.67.255.255-[ÒÅÌÈĞÒÀÓ]",
	"10.70.0.1-10.71.255.255-[ÀÒÛĞÀÓ]",
	
	"10.75.0.1-10.76.255.255-[ÀÊÒÀÓ]",
	"10.80.0.1-10.81.255.255-[ÊÎÑÒÀÍÀÉ]",
	"10.85.0.1-10.87.255.255-[ÀÑÒÀÍÀ]",
	
	"10.90.0.1-10.91.255.255-[ÆÅÇÊÀÇÃÀÍ]",
	"10.95.0.1-10.97.255.255-[ÒÀĞÀÇ]",
	"10.100.0.1-10.102.255.255-[ØÛÌÊÅÍÒ]",
	
	"10.105.0.1-10.106.255.255-[ÊÎÊØÅÒÀÓ]",
	"10.110.0.1-10.111.255.255-[İÊÈÁÀÑÒÓÇ]",
	"10.115.0.1-10.116.255.255-[ÒÀËÄÛÊÓĞÃÀÍ]",
	
	"10.120.0.1-10.121.255.255-[ÊÀÏ×ÅÃ]",
    "10.125.0.1-10.126.255.255-[ÀÊÑÀÉ]",
  --"10.130.0.1-10.31.255.255-[???]",
}

-- ÍÅ ÊĞÓÒÈ ÒÓÒ!!!

OnStartup = function() 

tmr = TmrMan.AddTimer(10000 * Minutes) 
end 

OnTimer = function(tmr)
for id,user in pairs(Core.GetOnlineUsers(true)) do
	Userover = user
	if user.sMyInfoString ~= nil then
	local descript,userip = nil,calcip(Userover.sIP)
	if userip ~= 0 then
	for index,descIP in pairs(RangeDesc) do
	local _,_,startRange,endRange,RangeDescript = string.find(descIP, "(.*)-(.*)-(.*)")
	startRange = calcip(startRange)
	endRange = calcip(endRange)
	if userip>=startRange and userip<=endRange then
	descript = RangeDescript
	end
	end
	if descript == nil then descript = "[Î ß ÅÁÓ]" 
	end
	end
local s,e,name,desc,speed,email,share = string.find(user.sMyInfoString, "$MyINFO $ALL (%S+)%s+([^$]*)$ $([^$]*)$([^$]*)$([^$]+)")
Core.SendToAll("$MyINFO $ALL "..name.." "..tProfileTag[user.iProfile].." "..descript.." "..desc.."$ $"..speed.."$"..email.."$"..share.."$")
end
end
end

-- ôóíêöèÿ ĞÀÑ×¨ÒÀ àäğåñà IP
function calcip(ipcalc)
local _,_,a,b,c,d = string.find(ipcalc, "(%d+).(%d+).(%d+).(%d+)")
local calc = 0
if (tonumber(a) and tonumber(b) and tonumber(c) and tonumber(d)) then
calc = a*16777216 + b*65536 + c*256 + d 
return calc
else return 0
end
end

--        _
--       |©|
--       |R|àôà
--      ğ|A|ôà
--     ğà|F|à
--       |A|
--       |_|_
--         | |
--         |B|
--         |I|
--         |G|
--         |Z|
--         |O|
--         |N|
--         |E|
--         |®|
--         |B|
--         |K|
--         |•|
--         |R|
--         |U|
--         |_|
