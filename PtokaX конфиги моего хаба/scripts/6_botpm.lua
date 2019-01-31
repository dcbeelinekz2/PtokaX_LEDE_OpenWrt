bot = "СООБ_ВСЕМ"
botDesc = " [ЛС] Напиши всем в личку"	--Описание бота
botTag = "RAFAdc++ v.333"    	--Тэг бота
botEmail = "DCBEELINEKZ@mail.ru"    --еМайл бота

OnStartup = function()
	Core.RegBot(bot, botDesc.."<"..botTag..">", botEmail, true)
end

ToArrival = function(user, data)
	data = data:sub(1,-2)
	local sBot, nick, msg = data:match"$To:%s+(%S+)%s+From:%s+(%S+)%s+$%b<>%s+(.*)"
	if sBot == bot then
		for i, v in pairs(Core.GetOnlineUsers()) do
			if v.sNick ~= nick then
				Core.SendPmToNick(v.sNick, bot, "*** Написал: <"..nick..">:\n\n"..msg)
			end
		end
		Core.SendPmToUser(user, bot, "ок")
		return true
	end
end
			