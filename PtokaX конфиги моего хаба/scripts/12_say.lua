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
				Core.SendToUser(user,"<"..sBot.."> ����������, ������� ���, �� �������� ����� ������� ���������")
			elseif sMsg == "" then
				Core.SendToUser(user,"<"..sBot.."> ����������, ������� ��������� ��� "..sNick.." , ������� �� ����������.")
			else
				Core.SendToAll("<"..sNick.."> ".. sMsg)
					Core.SendPmToOps(SetMan.GetString(24), "*** "..user.sNick.." ������ �� ���� "..sNick.." ���������: \""..sMsg.."\"")
			end 
			return true
		else
			Core.SendToUser(user,"<"..sBot.."> � ��� ��� ������� � ������ �������!")
				Core.SendPmToOps(SetMan.GetString(24), "*** "..user.sNick.." ��������� ������� �� ���� "..sNick.." ���������: \""..sMsg.."\", �� � ���� ������ �� �����.")
		end
	return true
	end
	if cmd == "!sysay" then
	local _,_,sMsg = string.find(sData, "%b<>%s+%S+%s+(.*)")
		if tProfile[user.iProfile] == 1 or tVIP[user.sNick] == 1 then
			if sMsg == "" then
				Core.SendToUser(user,"<"..sBot.."> ����������, ������� ���� ��������!")
				return true
			else
				Core.SendToAll(sMsg)
					Core.SendPmToOps(SetMan.GetString(24), "*** "..user.sNick.." ������ ��� ���� ���������: \""..sMsg.."\"")
			end 
			return true
		else
			Core.SendToUser(user,"<"..sBot.."> � ��� ��� ������� � ������ �������!")
				Core.SendPmToOps(SetMan.GetString(24), "*** "..user.sNick.." ��������� ������� ��� ���� ���������: \""..sMsg.."\", �� � ���� ������ �� �����")

		end
	return true
	end
	if cmd == "!pm_say" then
	local _,_, to, from, say = string.find(sData, "%b<>%s+%S+%s+(%S*)%s*(%S*)%s*(.*)")
		if tProfile[user.iProfile] == 1 or tVIP[user.sNick] == 1 then
			if say == "" then
				Core.SendToUser(user,"<"..sBot.."> ����������, ������� ���� ��������!")
				return true
			else
				Core.SendPmToNick(to,from,say)
					Core.SendPmToOps(SetMan.GetString(24), "*** "..user.sNick.." ������� � PM �� ���� "..from.." ��� ���� "..to.." ���������: \""..say.."\"")
			end 
		else
			Core.SendToUser(user,"<"..sBot.."> � ��� ��� ������� � ������ �������!")
				Core.SendPmToOps(SetMan.GetString(24), "*** "..user.sNick.." ��������� �������� � PM �� ���� "..from.." ��� ���� "..to.." ���������: \""..say.."\", �� � ���� ������ �� �����")
		end
	return true
	end
end

function UserConnected(user)
	if tProfile[user.iProfile] == 1 or tVIP[user.sNick] == 1 then
		Core.SendToUser(user,"$UserCommand 1 3 ���������\\������� �� ����..$<%[mynick]> !say %[line:������� ���] %[line:������� �����]&#124;")
		Core.SendToUser(user,"$UserCommand 1 3 ���������\\������� ��� ����..$<%[mynick]> !sysay %[line:������� �����]&#124;")
		Core.SendToUser(user,"$UserCommand 1 3 ���������\\������� �� ���� � PM..$<%[mynick]> !pm_say %[line:����] %[line:�� ����] %[line:������� ����� ���������]&#124;")
	end
end
OpConnected = UserConnected
RegConnected = UserConnected