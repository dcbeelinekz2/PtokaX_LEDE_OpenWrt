bot = "ТЕХ.ПОДДЕРЖКА"
botDesc = " [МЕНЮ]"	--Описание бота
botTag = "RAFA"    	--Тэг бота
botEmail = "DCBEELINEKZ@mail.ru"    --еМайл бота

OnStartup = function()
	Core.RegBot(bot, botDesc.."<"..botTag..">", botEmail, true)
end

ToArrival = function(user, data)
	data = data:sub(1,-2)
	local sBot, nick, msg = data:match"$To:%s+(%S+)%s+From:%s+(%S+)%s+$%b<>%s+(.*)"
	if sBot == bot then
    Core.SendPmToUser(user, bot, "\n\t\t МЕНЮ ТЕХ.ПОДДЕРЖКИ [DCBEELINEKZ] \r\n\t\t RAFA")
    return true
	end
end