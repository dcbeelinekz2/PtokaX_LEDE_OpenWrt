sBot = SetMan.GetString(21)
tProfile = {
	[0] = 1, [1] = 0,
}
tVIP = {
	[""] = 1,
}

function ChatArrival(user,sData)
	local sData = string.sub(sData,1,-2)
	local _,_,cmd = string.find(sData, "%b<>%s+(%S*)")
	if cmd == "!say" then
	local _,_,sNick,sMsg = string.find(sData, "%b<>%s+%S*%s*(%S*)%s*(.*)")
		if tProfile[user.iProfile] == 1 or tVIP[user.sNick] == 1 then
			if sNick == "" then
				Core.SendToUser(user,"<"..sBot.."> Пожалуйста, введите ник, от которого будет послано сообщение")
			elseif sMsg == "" then
				Core.SendToUser(user,"<"..sBot.."> Пожалуйста, введите сообщение для "..sNick.." , которое он произнесет.")
			else
				Core.SendToAll("<"..sNick.."> ".. sMsg)
					Core.SendPmToOps(SetMan.GetString(24), "*** "..user.sNick.." сказал от ника "..sNick.." сообщение: \""..sMsg.."\"")
			end 
			return true
		else
			Core.SendToUser(user,"<"..sBot.."> У Вас нет доступа к данной команде!")
				Core.SendPmToOps(SetMan.GetString(24), "*** "..user.sNick.." попытался сказать от ника "..sNick.." сообщение: \""..sMsg.."\", но у него ничего не вышло.")
		end
	return true
	end
	if cmd == "!sysay" then
	local _,_,sMsg = string.find(sData, "%b<>%s+%S+%s+(.*)")
		if tProfile[user.iProfile] == 1 or tVIP[user.sNick] == 1 then
			if sMsg == "" then
				Core.SendToUser(user,"<"..sBot.."> Пожалуйста, введите само сообщени!")
				return true
			else
				Core.SendToAll(sMsg)
					Core.SendPmToOps(SetMan.GetString(24), "*** "..user.sNick.." сказал без ника сообщение: \""..sMsg.."\"")
			end 
			return true
		else
			Core.SendToUser(user,"<"..sBot.."> У Вас нет доступа к данной команде!")
				Core.SendPmToOps(SetMan.GetString(24), "*** "..user.sNick.." попытался сказать без ника сообщение: \""..sMsg.."\", но у него ничего не вышло")

		end
	return true
	end
	if cmd == "!pm_say" then
	local _,_, to, from, say = string.find(sData, "%b<>%s+%S+%s+(%S*)%s*(%S*)%s*(.*)")
		if tProfile[user.iProfile] == 1 or tVIP[user.sNick] == 1 then
			if say == "" then
				Core.SendToUser(user,"<"..sBot.."> Пожалуйста, введите само сообщени!")
				return true
			else
				Core.SendPmToNick(to,from,say)
					Core.SendPmToOps(SetMan.GetString(24), "*** "..user.sNick.." написал в PM от ника "..from.." для ника "..to.." сообщение: \""..say.."\"")
			end 
		else
			Core.SendToUser(user,"<"..sBot.."> У Вас нет доступа к данной команде!")
				Core.SendPmToOps(SetMan.GetString(24), "*** "..user.sNick.." попытался написать в PM от ника "..from.." для ника "..to.." сообщение: \""..say.."\", но у него ничего не вышло")
		end
	return true
	end
end

function UserConnected(user)
	if tProfile[user.iProfile] == 1 or tVIP[user.sNick] == 1 then
		Core.SendToUser(user,"$UserCommand 1 3 СООБЩЕНИЯ\\Сказать от ника..$<%[mynick]> !say %[line:Введите ник] %[line:Введите текст]&#124;")
		Core.SendToUser(user,"$UserCommand 1 3 СООБЩЕНИЯ\\Сказать без ника..$<%[mynick]> !sysay %[line:Введите текст]&#124;")
		Core.SendToUser(user,"$UserCommand 1 3 СООБЩЕНИЯ\\Сказать от ника в PM..$<%[mynick]> !pm_say %[line:Кому] %[line:От кого] %[line:Введите текст сообщения]&#124;")
	end
end
OpConnected = UserConnected
RegConnected = UserConnected