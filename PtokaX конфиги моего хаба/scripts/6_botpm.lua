bot = "����_����"
botDesc = " [��] ������ ���� � �����"	--�������� ����
botTag = "RAFAdc++ v.333"    	--��� ����
botEmail = "DCBEELINEKZ@mail.ru"    --����� ����

OnStartup = function()
	Core.RegBot(bot, botDesc.."<"..botTag..">", botEmail, true)
end

ToArrival = function(user, data)
	data = data:sub(1,-2)
	local sBot, nick, msg = data:match"$To:%s+(%S+)%s+From:%s+(%S+)%s+$%b<>%s+(.*)"
	if sBot == bot then
		for i, v in pairs(Core.GetOnlineUsers()) do
			if v.sNick ~= nick then
				Core.SendPmToNick(v.sNick, bot, "*** �������: <"..nick..">:\n\n"..msg)
			end
		end
		Core.SendPmToUser(user, bot, "��")
		return true
	end
end
			