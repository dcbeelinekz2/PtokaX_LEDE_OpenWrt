Menu = "ТЕХ.ПОДДЕРЖКА\\"
UserMenu = "ЮЗЕР\\"
AdminMenu = "НАСТРОЙКА\\"

ScriptEasy = false	--Упрощенное меню управления скриптами (true - да, false - нет)
TempOP = true	--Включение меню "Временный оператор" (true - да, false - нет)
RestartHub = true	--Включение меню "Перезапуск хаба" (true - да, false - нет)
Prefix = ""	--Префикс команд хаба. Если не указан, используется первый префикс из настроек хаба.
sEnable = "+" -- Символ перед скриптом в меню, обозначающий что скрипт включен.
--###################################################################################

function OnStartup()
	if Prefix == "" then
		Prefix = SetMan.GetString(29):sub(1,1)
	end
	local tTmp = SetMan.GetHubBot()
	bot = tTmp.sNick
end

function UserConnected(user)
	local t = ProfMan.GetProfilePermissions(user.iProfile)
	--Глобальные команды
	Core.SendToUser(user,"$UserCommand 1 3 "..Menu.."Помощь$<%[mynick]> "..Prefix.."help&#124;")
	Core.SendToUser(user,"$UserCommand 1 3 "..Menu.."Показать ваш IP адрес$<%[mynick]> "..Prefix.."myip&#124;")
	if t then
		--Меню юзера
		if t.bGetInfo then
			Core.SendToUser(user,"$UserCommand 1 2 "..UserMenu.."Показать информацию$<%[mynick]> "..Prefix.."getinfo %[nick]&#124;")
			if t.bDrop or t.bKick or t.bTempBan or t.bBan or t.bAddRegUser or t.bDelRegUser or t.bMassMsg then
				Core.SendToUser(user,"$UserCommand 0 2")
			end
		end
		if t.bAddRegUser then
			Core.SendToUser(user,"$UserCommand 1 2 "..UserMenu.."Регистрация\\Зарегистрировать пользователя$<%[mynick]> "..Prefix.."addreguser %[nick] %[line:Пароль] %[line:Имя профиля]&#124;")
		end
		if t.bDelRegUser then
			Core.SendToUser(user,"$UserCommand 1 2 "..UserMenu.."Регистрация\\Удалить регистрацию$<%[mynick]> "..Prefix.."delreguser %[nick]&#124;")
		end
		if t.bMassMsg then
			Core.SendToUser(user,"$UserCommand 1 2 "..UserMenu.."Сообщение от имени бота$<%[mynick]> "..Prefix.."frombot %[nick] %[line:Введите текст сообщения]&#124;")
		end
		if t.bTempOP and TempOP then
			Core.SendToUser(user,"$UserCommand 1 2 "..UserMenu.."Временный оператор$<%[mynick]> "..Prefix.."op %[nick]&#124;")
		end
		if (t.bAddRegUser or t.bDelRegUser or t.bMassMsg or (t.bTempOP and TempOP)) and (t.bDrop or t.bKick or t.bTempBan or t.bBan) then
			Core.SendToUser(user,"$UserCommand 0 2")
		end
		if t.bDrop then
			Core.SendToUser(user,"$UserCommand 1 2 "..UserMenu.."Отключить$<%[mynick]> "..Prefix.."disconnect %[nick]&#124;")
		end
		if t.bKick then
			Core.SendToUser(user,"$UserCommand 1 2 "..UserMenu.."Кикнуть$<%[mynick]> "..Prefix.."drop %[nick] %[line:Причина]&#124;")
		end
		if t.bTempBan then
			Core.SendToUser(user,"$UserCommand 1 2 "..UserMenu.."Бан 1 час$<%[mynick]> "..Prefix.."nicktempban %[nick] 1h %[line:Причина]&#124;")
			Core.SendToUser(user,"$UserCommand 1 2 "..UserMenu.."Бан 24 часа$<%[mynick]> "..Prefix.."nicktempban %[nick] 1d %[line:Причина]&#124;")
			Core.SendToUser(user,"$UserCommand 1 2 "..UserMenu.."Временный бан...$<%[mynick]> "..Prefix.."nicktempban %[nick] %[line:Время (m = минут, h = часов, d = дней, w = недель)] %[line:Причина]&#124;")
		end
		if t.bBan then
			Core.SendToUser(user,"$UserCommand 1 2 "..UserMenu.."Постоянный бан$<%[mynick]> "..Prefix.."nickban %[nick] %[line:Причина]&#124;")
		end
		--Управление хабом
		if t.bTopic then
			Core.SendToUser(user,"$UserCommand 1 3 "..AdminMenu.."Топик\\Установить топик$<%[mynick]> "..Prefix.."topic %[line:Введите текст]&#124;")
			Core.SendToUser(user,"$UserCommand 1 3 "..AdminMenu.."Топик\\Очистить топик$<%[mynick]> "..Prefix.."topic off&#124;")
		end
		if t.bRefreshTxt then
			Core.SendToUser(user,"$UserCommand 1 3 "..AdminMenu.."Текстовые файлы\\Перезап. текст. файлы$<%[mynick]> "..Prefix.."reloadtxt&#124;")
		end
		if t.bMassMsg then
			Core.SendToUser(user,"$UserCommand 1 3 "..AdminMenu.."Рассылка сообщений\\Массовая рассылка$<%[mynick]> "..Prefix.."massmsg %[line:Введите текст сообщения]&#124;")
			Core.SendToUser(user,"$UserCommand 1 3 "..AdminMenu.."Рассылка сообщений\\Рассылка ОПам$<%[mynick]> "..Prefix.."opmassmsg %[line:Введите текст сообщения]&#124;")
			Core.SendToUser(user,"$UserCommand 1 3 "..AdminMenu.."Сообщение от имени бота$<%[mynick]> "..Prefix.."frombot %[line:Ник] %[line:Введите текст сообщения]&#124;")
		end
		--Списки банов
		if t.bGetBans then
			Core.SendToUser(user,"$UserCommand 1 3 "..AdminMenu.."Баны\\Список банов$<%[mynick]> "..Prefix.."getbans&#124;")
			Core.SendToUser(user,"$UserCommand 1 3 "..AdminMenu.."Баны\\Список временных банов$<%[mynick]> "..Prefix.."gettempbans&#124;")
			Core.SendToUser(user,"$UserCommand 1 3 "..AdminMenu.."Баны\\Список постоянных банов$<%[mynick]> "..Prefix.."getpermbans&#124;")
			Core.SendToUser(user,"$UserCommand 0 3")
		end
		--Временные баны
		if t.bTempBan then
			Core.SendToUser(user,"$UserCommand 1 3 "..AdminMenu.."Баны\\Временный бан$<%[mynick]> "..Prefix.."nicktempban %[line:Ник] %[line:Время (m = минут, h = часов, d = дней, w = недель)] %[line:Причина]&#124;")
			Core.SendToUser(user,"$UserCommand 1 3 "..AdminMenu.."Баны\\Временный бан IP$<%[mynick]> "..Prefix.."tempbanip %[line:Укажите IP] %[line:Время (m = минут, h = часов, d = дней, w = недель)] %[line:Причина]&#124;")
			Core.SendToUser(user,"$UserCommand 1 3 "..AdminMenu.."Баны\\Временный бан IP (полный)$<%[mynick]> "..Prefix.."fulltempbanip %[line:Укажите IP] %[line:Время (m = минут, h = часов, d = дней, w = недель)] %[line:Причина]&#124;")
			Core.SendToUser(user,"$UserCommand 0 3")
		end
		if t.bTempUnban then
			Core.SendToUser(user,"$UserCommand 1 3 "..AdminMenu.."Баны\\Снять временный бан$<%[mynick]> "..Prefix.."tempunban %[line:IP или ник]&#124;")
			Core.SendToUser(user,"$UserCommand 0 3")
		end
		--Постоянные баны
		if t.bBan then
			Core.SendToUser(user,"$UserCommand 1 3 "..AdminMenu.."Баны\\Бан$<%[mynick]> "..Prefix.."nickban %[line:Ник] %[line:Причина]&#124;")
			Core.SendToUser(user,"$UserCommand 1 3 "..AdminMenu.."Баны\\Бан IP$<%[mynick]> "..Prefix.."banip %[line:Укажите IP] %[line:Причина]&#124;")
			Core.SendToUser(user,"$UserCommand 1 3 "..AdminMenu.."Баны\\Бан IP (полный)$<%[mynick]> "..Prefix.."fullbanip %[line:Укажите IP] %[line:Причина]&#124;")
			Core.SendToUser(user,"$UserCommand 0 3")
		end
		if t.bUnban then
			Core.SendToUser(user,"$UserCommand 1 3 "..AdminMenu.."Баны\\Снять бан$<%[mynick]> "..Prefix.."unban %[line:Ник или IP]&#124;")
			Core.SendToUser(user,"$UserCommand 0 3")
		end
		--Списки диапазонов банов
		if t.bGetRangeBans then
			Core.SendToUser(user,"$UserCommand 1 3 "..AdminMenu.."Баны диапазонов\\Список диапазонов банов $<%[mynick]> "..Prefix.."getrangebans&#124;")
			Core.SendToUser(user,"$UserCommand 1 3 "..AdminMenu.."Баны диапазонов\\Список временных банов диапазонов$<%[mynick]> "..Prefix.."getrangetempbans&#124;")
			Core.SendToUser(user,"$UserCommand 1 3 "..AdminMenu.."Баны диапазонов\\Список постоянных банов диапазонов$<%[mynick]> "..Prefix.."getrangepermbans&#124;")
			Core.SendToUser(user,"$UserCommand 0 3")
		end
		--Временные баны диапазонов
		if t.bRangeTempBan then
			Core.SendToUser(user,"$UserCommand 1 3 "..AdminMenu.."Баны диапазонов\\Временный бан диапазона$<%[mynick]> "..Prefix.."rangetempban %[line:Начальный IP диапазона] %[line:Конечный IP диапазона] %[line:Время (m = минут, h = часов, d = дней, w = недель)] %[line:Причина]&#124;")
			Core.SendToUser(user,"$UserCommand 1 3 "..AdminMenu.."Баны диапазонов\\Временный бан диапазона (полный)$<%[mynick]> "..Prefix.."fullrangetempban %[line:Начальный IP диапазона] %[line:Конечный IP диапазона] %[line:Время (m = минут, h = часов, d = дней, w = недель)] %[line:Причина]&#124;")
			Core.SendToUser(user,"$UserCommand 0 3")
		end
		if t.bRangeTempUnban then
			Core.SendToUser(user,"$UserCommand 1 3 "..AdminMenu.."Баны диапазонов\\Снять временный бан диапазона$<%[mynick]> "..Prefix.."tempunban %[line:Начальный IP диапазона] %[line:Конечный IP диапазона]&#124;")
			Core.SendToUser(user,"$UserCommand 0 3")
		end
		--Постоянные баны диапазонов
		if t.bRangeBan then
			Core.SendToUser(user,"$UserCommand 1 3 "..AdminMenu.."Баны диапазонов\\Бан диапазона$<%[mynick]> "..Prefix.."rangeban %[line:Начальный IP диапазона] %[line:Конечный IP диапазона] %[line:Причина]&#124;")
			Core.SendToUser(user,"$UserCommand 1 3 "..AdminMenu.."Баны диапазонов\\Бан диапазона (полный)$<%[mynick]> "..Prefix.."fullrangeban %[line:Начальный IP диапазона] %[line:Конечный IP диапазона] %[line:Причина]&#124;")
			Core.SendToUser(user,"$UserCommand 0 3")
		end
		if t.bRangeUnban then
			Core.SendToUser(user,"$UserCommand 1 3 "..AdminMenu.."Баны диапазонов\\Снять бан диапазона$<%[mynick]> "..Prefix.."rangepermunban %[line:Начальный IP диапазона] %[line:Конечный IP диапазона]&#124;")
			Core.SendToUser(user,"$UserCommand 0 3")
		end
		--Регистрация
		if t.bAddRegUser then
			Core.SendToUser(user,"$UserCommand 1 3 "..AdminMenu.."Регистрация\\Зарегистрировать пользователя$<%[mynick]> "..Prefix.."addreguser %[line:Ник] %[line:Пароль] %[line:Имя профиля]&#124;")
		end
		if t.bDelRegUser then
			Core.SendToUser(user,"$UserCommand 1 3 "..AdminMenu.."Регистрация\\Удалить регистрацию$<%[mynick]> "..Prefix.."delreguser %[line:Ник]&#124;")
		end
		--Управление скриптами
		if t.bRestartScripts then
			Core.SendToUser(user,"$UserCommand 1 3 "..AdminMenu.."Скрипты\\Посмотреть список$<%[mynick]> "..Prefix.."getscripts&#124;")
			Core.SendToUser(user,"$UserCommand 1 3 "..AdminMenu.."Скрипты\\Перезапустить скрипты$<%[mynick]> "..Prefix.."restartscripts&#124;")
			Core.SendToUser(user,"$UserCommand 0 3")
			if ScriptEasy then
				Core.SendToUser(user,"$UserCommand 1 3 "..AdminMenu.."Скрипты\\Перезапуск$<%[mynick]> "..Prefix.."restartscript %[line:Имя файла]&#124;")
				Core.SendToUser(user,"$UserCommand 1 3 "..AdminMenu.."Скрипты\\Старт$<%[mynick]> "..Prefix.."startscript %[line:Имя файла]&#124;")
				Core.SendToUser(user,"$UserCommand 1 3 "..AdminMenu.."Скрипты\\Стоп$<%[mynick]> "..Prefix.."stopscript %[line:Имя файла]&#124;")
				Core.SendToUser(user,"$UserCommand 1 3 "..AdminMenu.."Скрипты\\Сдвинуть вверх$<%[mynick]> "..Prefix.."scriptmoveup %[line:Имя файла]&#124;")
				Core.SendToUser(user,"$UserCommand 1 3 "..AdminMenu.."Скрипты\\Сдвинуть вниз$<%[mynick]> "..Prefix.."scriptmovedown %[line:Имя файла]&#124;")
			else
				local sEn = sEnable.." "
				local tScripts = ScriptMan.GetScripts()
				for script in pairs(tScripts) do
					local CurScript = tScripts[script].sName
					local bEnabled = tScripts[script].bEnabled or false
					if bEnabled then Scr = sEn..CurScript else Scr = "   "..CurScript end
					Core.SendToUser(user,"$UserCommand 1 3 "..AdminMenu.."Скрипты\\"..Scr.."\\Перезапуск$<%[mynick]> "..Prefix.."restartscript "..CurScript.."&#124;")
					Core.SendToUser(user,"$UserCommand 1 3 "..AdminMenu.."Скрипты\\"..Scr.."\\Старт$<%[mynick]> "..Prefix.."startscript "..CurScript.."&#124;")
					Core.SendToUser(user,"$UserCommand 1 3 "..AdminMenu.."Скрипты\\"..Scr.."\\Стоп$<%[mynick]> "..Prefix.."stopscript "..CurScript.."&#124;")
					Core.SendToUser(user,"$UserCommand 1 3 "..AdminMenu.."Скрипты\\"..Scr.."\\Сдвинуть вверх$<%[mynick]> "..Prefix.."scriptmoveup "..CurScript.."&#124;")
					Core.SendToUser(user,"$UserCommand 1 3 "..AdminMenu.."Скрипты\\"..Scr.."\\Сдвинуть вниз$<%[mynick]> "..Prefix.."scriptmovedown "..CurScript.."&#124;")
				end
			end
		end
		--Статистика
		if t.bIsOP then
			Core.SendToUser(user,"$UserCommand 1 3 "..AdminMenu.."Статистика$<%[mynick]> "..Prefix.."stats&#124;")
		end
		if t.bRestartHub and RestartHub then
			Core.SendToUser(user,"$UserCommand 1 3 "..AdminMenu.."Перезапуск хаба$<%[mynick]> "..Prefix.."restart&#124;")
		end
	end
end

OpConnected = UserConnected
RegConnected = UserConnected

function ChatArrival(user,data)
	data = data:sub(1,-2)
	local t = ProfMan.GetProfilePermissions(user.iProfile)
	local pre,cmd = data:match("^%b<>%s+(%p)(%S+)")
	local param = data:match("^%b<>%s+%p%S+%s+(.+)")
	if pre == Prefix then
		if t then
			if t.bRestartScripts then
				if cmd and cmd == "scriptmoveup" then
					if param then
						result = ScriptMan.MoveUp(param)
						if result then
							ToOps(user.sNick..": Скрипт "..param.." перемещён вверх на одну позицию.")
						else
							ToUser(user,"Ошибка: скрипт "..param.." переместить не удалось.")
						end
					else
						ToUser(user,"<"..bot.."> Ошибка. Вы должны указать имя файла.")
					end
					return true
				elseif cmd and cmd == "scriptmovedown" then
					if param then
						result = ScriptMan.MoveDown(param)
						if result then
							ToOps(user.sNick..": Скрипт "..param.." перемещён вниз на одну позицию.")
						else
							ToUser(user,"Ошибка: скрипт "..param.." переместить не удалось.")
						end
					else
						ToUser(user,"<"..bot.."> Ошибка. Вы должны указать имя файла.")
					end
					return true
				end
			end
			if t.bDrop then
				if cmd and cmd == "disconnect" then
					if param then
						local CurUser = Core.GetUser(param)
						if CurUser then
							Core.Disconnect(CurUser)
							ToOps(user.sNick.." отключил юзера "..param)
						else
							ToUser(user,"Ошибка: юзер "..param.." не найден на хабе")
						end
					else
						ToUser(user,"Ошибка: Вы должны указать ник.")
					end
					return true
				end
			end
			if t.bMassMsg then
				if cmd and cmd == "frombot" then
					local s,e,nick,msg = string.find(param, "^(%S+)%s+(.+)$")
					if nick and msg then
						local CurUser = Core.GetUser(nick)
						if CurUser then
							FromBot(CurUser,msg)
							ToUser(user,"Сообщение отправлено")
							ToOps(user.sNick.." отправил сообщение от имени бота юзеру "..nick.." :"..msg)
						else
							ToUser(user,"Ошибка: юзер "..nick.." не найден на хабе")
						end
					else
						ToUser(user,"Ошибка синтаксиса. Синтаксис: "..Prefix.."frombot <ник> <текст сообщения>")
					end
					return true
				end
			end
		end
	end
end

function ToUser(user,msg)
	Core.SendToUser(user,"<"..bot.."> "..msg)
end

function ToOps(msg)
	Core.SendToOps("<"..bot.."> "..msg)
end

function FromBot(user,msg)
	Core.SendPmToUser(user,bot,msg)
end