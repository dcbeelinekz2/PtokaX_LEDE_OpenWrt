local tCfg = {
	-- ник бота, оставьте "" для использования имени бота хаба
	sBot = "",
	-- отправлять меню при входе в хаб, 1 или 0
	bMenuOnEntry = 1,
	-- сохранять +me сообщения, 1 или 0
	bLogMeCmds = 0,
	-- укорачивать сообщения в истории чата до этого количества символов
	-- оставьте 0, если укорачивать не требуется
	iMaxLen = 0,
	-- максимальное количество сохраняемых в истории чата сообщений
	iMaxMsgs = 3000,
	-- количество сообщений при входе в хаб
	iMsgsOnEntry = 10,
	-- корректировка времени, секунд
	iTimeOffset = 0,
	-- интервал автосохранения истории, минут
	iAutoSave = 1,
	-- формат времени написания сообщений в истории чата
	sTimeStamp = "[%H:%M:%S] ",
	-- относительный путь к файлу сохранения истории
	sFile	= "Log/Chat.dat",
	iMenuDefaultContext = 3,
}
local tCmdsConf = { -- настройки команд: название команды; профили с доступом к команде;
-- использовать ли указанные профили при формировании пула профилей-получателей отчётов;
-- создавать меню для команды; показывать команду в справке; очерёдность в меню и в справке;
-- правила создания меню (контекст, родительское меню, название строки с переводом дополнительных параметров,
-- сырые дополнительные параметры меню, разделитель над меню); параметры команды для справки
	tScriptHelp		= { "histhelp",		{0},		0, 1, 1, 90,
						{3,"","","",1} },
	tGlobalHelp		= { "help",			{0},		0, 0, 0, 99 },

	tGetChatHistory	= { "history",		{0},		0, 1, 1, 10,
						{3, "", "sMenuShowParams"},	" <count>"},
	tDelMsgByString	= { "chdelstr",		{0},			1, 1, 1, 20,
						{3, "", "sMenuDMBSParams"},	" <text>"},
	tDelMsgFromNick	= { "chdelnick",	{0},			1, 1, 1, 30,
						{	{3,"","sMenuDMBNParams"}, {2,"",""," %[nick]"},	},	" <nick>"},
	tDelMsg			= { "chdelmsg", 	{0},			1, 1, 1, 40,
						{3, "", "sMenuDMParams"},		" <number>[ <number>]"},
	tDelChatHistory	= { "chdelete", 	{0},				1, 1, 1, 50,
						{3, "", "sMenuDCHParams"} },
}
local tExceptions = {	-- шаблоны, сообщения с которыми обрезаться не будут
	"magnet:%?xt=urn:tree:tiger:[a-zA-Z%d]+&xl=%-?%d+&dn=%S+",	-- магнет-ссылки
	"https?://%S+",	-- веб-ссылки
	"www%.%S+",		-- веб-ссылки
	"ftp[:.]%S+",	-- фтп-ссылки
	"dchub://%S+",	-- хаб-ссылки
}
-- соответствие страны пользователя языку получаемых им сообщений от скрипта
local tLangs = {
	Status	= "Russian",	-- Рассылка операторам
}

local tMsgs = {}
tMsgs["Russian"] = {
	sBadBotName	= "Имя бота содержит запрещённые символы (&#36; или &#124; или пробел). Они были заменены на их коды.",
	sCHEmpty	= "История чата пуста.",
	sCHLong		= "Последние [COUNT] сообщений в чате:\n [MSGS]\n",
	sCHShort	= "ЧАТ:",
	sCHCleared	= "%s очистил историю чата.",
	sDelMsg		= "%s удалил из истории чата %s сообщений: %s",
	sDelMsgNick	= "%s удалил из истории чата %s сообщений от ника '%s'.",
	sDelMsgPat	= "%s удалил из истории чата %s сообщений, содержащих строку '%s'.",
	sHelpHeader	= "Вам доступны следующие команды:\n",
	sMsgNotFound	= "%s\n\tСообщение #%s не найдено.",
	sNickNotFound	= "В истории чата не найдено сообщений от ника '%s'.",
	sNoArg		= "Не найден необходимый параметр команды.",
	sFileError	= "Не удалось открыть файл",
	sPatternNotFound = "В истории чата не найдено сообщений, содержащих строку '%s'.",
	sRegBotFail		= "Не удалось зарегистрировать бота '%s'.",
	sSubMenu		= "ИСТОРИЯ ЧАТА",
	sMenuDCHParams		= "Вы действительно хотите удалить все сообщения в истории чата?",
	sMenuDMBNParams		= "Введите ник",
	sMenuDMBSParams		= "Введите образец строки для удаления",
	sMenuDMParams		= "Введите номера удаляемых сообщений (через пробел)",
	sMenuShowParams		= "Сколько сообщений показать?",

	sHelpDelChatHistory	= "очистить историю чата",
	sHelpDelMsg			= "удалить сообщения с указанными номерами (нумерация от новейшего сообщения)",
	sHelpDelMsgByString = "удалить сообщения, содержащие указанный образец текста",
	sHelpDelMsgFromNick = "удалить сообщения от указанного ника",
	sHelpGetChatHistory = "показать последние <count> сообщений в истории чата (без параметра - все сообщения)",
	sHelpScriptHelp		= "эта справка по командам",
	sMenuDelChatHistory	= "Очистить",
	sMenuDelMsg			= "Удалить по номеру",
	sMenuDelMsgByString = "Удалить по образцу",
	sMenuDelMsgFromNick = "Удалить сообщения ника",
	sMenuDelMsgFromNick2 = "Удалить сообщения этого юзера",
	sMenuGetChatHistory	= "Показать",
	sMenuScriptHelp		= "Справка",
}


------------------------------------------------------------

local tCmds, tChat, sChatHistory
local tUC, tHelp = {}, {}
local bToSave, bChanges = true, true

function OnError(sErrMsg)
	Core.SendToOps("<"..Core.GetHubSecAlias().."> Error "..sErrMsg)
	return true
end

function OnStartup()
	tCfg.sBot = tCfg.sBot and tCfg.sBot ~= "" and tCfg.sBot
		or SetMan.GetString( SetMan.tStrings and SetMan.tStrings.HubBotNick or 21 )
	for i,v in pairs(tCfg) do
		if i:byte(1) == 98 then -- b
			tCfg[i] = v == 1
		end
	end
	tCfg.bTruncateMsgs = tCfg.iMaxLen ~= 0

	local sPrefixes = SetMan.GetString( SetMan.tStrings and SetMan.tStrings.ChatCommandsPrefixes or 29 )
	if not sPrefixes or #sPrefixes == 0 then sPrefixes = "!" end
	local sPrefix = sPrefixes:sub(1,1)
	sPrefixes = sPrefixes:gsub("[%^%%%.%[%]%-]","%%%1")
	tCfg.sMcPattern = "^<[^ |$]+> %s*(["..sPrefixes.."]%S+)"
	tCfg.sPmPattern = "^$To:%s+([^ ]+)%s+From:%s+[^ ]+%s+$<[^ |$]+>( %s*["..sPrefixes.."](%S+).*)|$"
	local sUC = "$UserCommand 1 [CONTEXT] [MENU]$<%[mynick]> [COMMAND][PARAMS]&#124;|"

	if not tLangs.Status	then tLangs.Status	= next(tMsgs) end
	if not tLangs.Other		then tLangs.Other	= tLangs.Status end
	if not tLangs.NoIP2C	then tLangs.NoIP2C	= tLangs.Status end
	local t = {}
	for i,v in pairs(tLangs) do
		if not tMsgs[v] then
			table.insert(t, i)
		end
	end
	for i,v in ipairs(t) do tLangs[v] = nil end

	for sLangName, tLangMsgs in pairs(tMsgs) do
		t = {}
		for i,v in pairs(tLangMsgs) do
			if (i:sub(1,5) == "sMenu" or i:sub(1,5) == "sHelp") and #v == 0 then
				table.insert(t, i)
			end
		end
		for i,v in ipairs(t) do tLangMsgs[v] = nil end
	end

	local tTempStrings, tTempUC, tTempHelp, tReportToProfs = {}, {}, {}, {}
	for k,tComConf in pairs(tCmdsConf) do
		tTempStrings[k] = {}
		for sLangName, tLangMsgs in pairs(tMsgs) do
			tTempStrings[k][sLangName] = {}
			local tTempStringsLang = tTempStrings[k][sLangName]
			if tComConf[4] == 1 then
				if tComConf[7] and tComConf[7][1] and type(tComConf[7][1]) == 'table' then
					for i,v in ipairs(tComConf[7]) do
						local sSubMenu = tLangMsgs['sSubMenu'..(v[2] or "")]
						if v[5] then
							tTempStringsLang[1] = (tTempStringsLang[1] or "").."$UserCommand 0 "..(v[1] or tCfg.iMenuDefaultContext).." |"
						end
						tTempStringsLang[1] = (tTempStringsLang[1] or "")..(sUC:gsub("%[([A-Z]+)%]", {
							CONTEXT	= v[1] or tCfg.iMenuDefaultContext,
							MENU	= (sSubMenu and #sSubMenu ~= 0 and sSubMenu.."\\" or "")
								..(tLangMsgs["sMenu"..k:sub(2)..i] or tLangMsgs["sMenu"..k:sub(2)] or k:sub(2)),
							COMMAND = sPrefix..tComConf[1],
							PARAMS	=
								  (v[3] and #v[3] ~= 0 and (" %%[line:%s]"):format(tLangMsgs[ v[3] ]) or "")
								..(v[4] and #v[4] ~= 0 and v[4]  or ""),
						}))
					end
				else
					local sSubMenu = tLangMsgs['sSubMenu'..(tComConf[7] and tComConf[7][2] or "")]
					if tComConf[7] and tComConf[7][5] then
						tTempStringsLang[1] = "$UserCommand 0 "..(tComConf[7][1] or tCfg.iMenuDefaultContext).." |"
					end
					tTempStringsLang[1] = (tTempStringsLang[1] or "")..(sUC:gsub("%[([A-Z]+)%]", {
						CONTEXT	= tComConf[7] and tComConf[7][1] or tCfg.iMenuDefaultContext,
						MENU	= (sSubMenu and #sSubMenu ~= 0 and sSubMenu.."\\" or "")
							..(tLangMsgs["sMenu"..k:sub(2)] or k:sub(2)),
						COMMAND = sPrefix..tComConf[1],
						PARAMS	= (
							tComConf[7] and tComConf[7][3] and #tComConf[7][3] ~= 0
							and (" %%[line:%s]"):format(tLangMsgs[ tComConf[7][3] ]) or ""
						) .. (
							tComConf[7] and tComConf[7][4] and #tComConf[7][4] ~= 0
							and tComConf[7][4] or ""
						),
					}))
				end
			else
				tTempStringsLang[1] = ""
			end
			table.insert(tTempStringsLang, tComConf[5] == 1 and ("\n\t%s%s%s - %s."):format(
				sPrefix, tComConf[1], tComConf[8] or "", tLangMsgs["sHelp"..k:sub(2)] or "not described yet") or "")
		end

		local tPerms = {}
		for _,iProf in ipairs(tComConf[2]) do
			tPerms[iProf] = true

			if tComConf[3] == 1 then
				tReportToProfs[iProf] = true
			end

			if tComConf[4] == 1 then
				if not tTempUC[iProf] then tTempUC[iProf] = {} end
				table.insert(tTempUC[iProf], k)
			end
			if tComConf[5] == 1 then
				if not tTempHelp[iProf] then tTempHelp[iProf] = {} end
				table.insert(tTempHelp[iProf], k)
			end
		end
		if tCmds[tComConf[1]] then
			table.insert(tCmds[tComConf[1]], tPerms)
		end
	end

	for iProf,tComs in pairs(tTempUC) do
		table.sort(tComs, function(a,b) return tCmdsConf[a][6] < tCmdsConf[b][6] end)
		for _,sCmdsConfIdx in ipairs(tComs) do
			for sLangName, tLangUC in pairs(tTempStrings[sCmdsConfIdx]) do
				if not tUC[sLangName] then tUC[sLangName] = {} end
				tUC[sLangName][iProf] = (tUC[sLangName][iProf] or "")..tLangUC[1]
			end
		end
	end
	for iProf,tComs in pairs(tTempHelp) do
		table.sort(tComs, function(a,b) return tCmdsConf[a][6] < tCmdsConf[b][6] end)
		for _,sCmdsConfIdx in ipairs(tComs) do
			for sLangName, tLangHelp in pairs(tTempStrings[sCmdsConfIdx]) do
				if not tHelp[sLangName] then tHelp[sLangName] = {} end
				tHelp[sLangName][iProf] = (tHelp[sLangName][iProf] or "")..tLangHelp[2]
			end
		end
	end

	local tReportTo = {}
	for i in pairs(tReportToProfs) do
		table.insert(tReportTo, i)
	end
	if #tReportTo == 0 then table.insert(tReportTo, 0) end
	Report = function(sMsg)
		if SetMan.GetBool( SetMan.tBooleans and SetMan.tBooleans.SendStatusMessagesAsPm or 30 ) then
			local sMsg = "*** "..tostring(sMsg)
			for i,v in ipairs(tReportTo) do
				Core.SendPmToProfile(v, tCfg.sBot, sMsg)
			end
		else
			local sMsg = "<"..tCfg.sBot.."> *** "..tostring(sMsg)
			for i,v in ipairs(tReportTo) do
				Core.SendToProfile(v, sMsg)
			end
		end
	end

	local bBotExists
	if tCfg.sBot == SetMan.GetString( SetMan.tStrings and SetMan.tStrings.HubBotNick or 21 ) then
		bBotExists = true
	else
		for i,v in ipairs(Core.GetBots()) do
			if v.sName == tCfg.sBot then
				bBotExists = true
				break
			end
		end
	end
	if not bBotExists then
		if tCfg.sBot:find"[|$ ]" then
			tCfg.sBot = tCfg.sBot:gsub("[|$ ]", function(s) return('\\'..string.byte(s)) end)
			Report(tMsgs[tLangs.Status].sBadBotName)
		end
		if not Core.RegBot(tCfg.sBot,"","",true) then
			Report(tMsgs[tLangs.Status].sRegBotFail:format(tCfg.sBot))
		end
	end

	tCfg.sFile = Core.GetPtokaXPath().."scripts/"..tCfg.sFile
	local Ret = loadfile(tCfg.sFile)
	if Ret then Ret() end

	tChat = _G.tChat or {}
	while #tChat > tCfg.iMaxMsgs do
		table.remove(tChat, 1)
	end
	tCfg.iAutoSave = tCfg.iAutoSave * 60 * 1000
	TmrMan.AddTimer(tCfg.iAutoSave, 'OnExit')
	OnExit()
end

function Serialize(tTable, sTableName, hFile, sTab)
	sTab = sTab or ''
	hFile:write(sTab..sTableName.." = {\n")
	for k, v in pairs(tTable) do
		if type(v) ~= "function" then
			local sKey = type(k) == "string" and ("[%q]"):format(k) or ("[%d]"):format(k)
			if type(v) == "table" then
				Serialize(v, sKey, hFile, sTab..'\t')
			else
				local sValue = type(v) == "string" and ("%q"):format(v) or tostring(v)
				hFile:write(sTab..'\t'..sKey.." = "..sValue)
			end
			hFile:write(",\n")
		end
	end
	hFile:write(sTab.."}")
end

function SaveTable(sFile, tTable, sTableName)
	local hFile, sErr = io.open(sFile, "w+")
	if hFile then
		Serialize(tTable, sTableName, hFile)
		hFile:flush()
		hFile:close()
		return true
	end
	error( ("%s %s\n%s"):format(tMsgs[tLangs.Status].sFileError, sFile, sErr), 0 )
end

function OnExit()
	if bToSave then
		bToSave = false
		SaveTable(tCfg.sFile, tChat, "tChat")
	end
end

function ChatArrival(tUser, sData)
	sData = sData:sub(1,-2)
	local sCmdOrNick = sData:match(tCfg.sMcPattern)
	if sCmdOrNick then
		local sCmd = sCmdOrNick:sub(2):lower()
		if tCmds[sCmd] and tCmds[sCmd][2][tUser.iProfile] then
--			local sLang = tLangs[IP2Country.GetCountryCode(tUser.sIP) or "NoIP2C"] or tLangs.Other
			local sLang = tLangs[Core.GetUserValue(tUser, 26) or "NoIP2C"] or tLangs.Other
			local bRet, sResult = tCmds[sCmd][1](tUser, sLang, sData:sub(#tUser.sNick + 3))
			if sResult and type(sResult) == "string" then
				Core.SendToUser(tUser, "<"..tCfg.sBot.."> "..sResult)
			end
			return bRet
		elseif Core.GetUser(sCmdOrNick) or Core.GetUser(sCmdOrNick:sub(1,-2)) then
			sCmdOrNick = nil
		else
			sCmdOrNick = sCmdOrNick:match("^.(%a%w+)$")
		end
	end
	if not sData:find"is kicking" and (not sCmdOrNick or (tCfg.bLogMeCmds and sCmdOrNick == "me")) then
		if sCmdOrNick then
			sData = "* "..tUser.sNick..sData:sub(#tUser.sNick + 7)
		end
		if tCfg.bTruncateMsgs then
			local bTrimMsg = true
			local iLen = #sData - #tUser.sNick - 3
			for i,v in ipairs(tExceptions) do
				if sData:find(v) then
					bTrimMsg = false
					break
				end
			end
			if bTrimMsg and iLen > tCfg.iMaxLen then
				sData = sData:sub(1, tCfg.iMaxLen - iLen - 1).."[…]"
			end
		end
		if not bToSave then bToSave = true end
		bChanges = true
		table.insert(tChat, os.date(tCfg.sTimeStamp, os.time() + tCfg.iTimeOffset)..sData)
		while #tChat > tCfg.iMaxMsgs do
			table.remove(tChat, 1)
		end
	end
end

function ToArrival(tUser, sData)
	local sTo, sMsg, sCmd = sData:match(tCfg.sPmPattern)
	if sTo and sTo == tCfg.sBot and tCmds[sCmd:lower()] and tCmds[sCmd:lower()][2][tUser.iProfile] then
--		local sLang = tLangs[IP2Country.GetCountryCode(tUser.sIP) or "NoIP2C"] or tLangs.Other
		local sLang = tLangs[Core.GetUserValue(tUser, 26) or "NoIP2C"] or tLangs.Other
		local bRet, sResult = tCmds[sCmd:lower()][1](tUser, sLang, sMsg, true)
		if sResult and type(sResult) == "string" then
			Core.SendPmToUser(tUser, tCfg.sBot, sResult)
		end
		return bRet
	end
end

function UserConnected(tUser)
--	local sLang = tLangs[IP2Country.GetCountryCode(tUser.sIP) or "NoIP2C"] or tLangs.Other
	local sLang = tLangs[Core.GetUserValue(tUser, 26) or "NoIP2C"] or tLangs.Other
	if tCfg.bMenuOnEntry and Core.GetUserValue(tUser, 12) and tUC[sLang] and tUC[sLang][tUser.iProfile] then
		Core.SendToUser(tUser, tUC[sLang][tUser.iProfile])
	end
	if bChanges then
		if #tChat ~= 0 then
			bChanges = false
			sChatHistory = ("\n\173%s\n"):format(table.concat(
				tChat, "\n\173", #tChat - math.min(tCfg.iMsgsOnEntry, #tChat) + 1))
		else
			Core.SendToUser(tUser, "<"..tCfg.sBot.."> "..tMsgs[sLang].sCHEmpty)
			return
		end
	end
	Core.SendToUser(tUser, ("<%s> %s %s"):format(tCfg.sBot, tMsgs[sLang].sCHShort, sChatHistory) )
end
RegConnected, OpConnected = UserConnected, UserConnected

tCmds = {
	[tCmdsConf.tScriptHelp[1]] = {function(tUser, sLang)
		return true, tMsgs[sLang].sHelpHeader..tHelp[sLang][tUser.iProfile]
	end},
	[tCmdsConf.tGlobalHelp[1]] = {function(tUser, sLang, _, bPM)
		if not bPM and SetMan.GetBool( SetMan.tBooleans and SetMan.tBooleans.ReplyToHubCommandsAsPm or 36 ) then
			Core.SendPmToUser(tUser, tCfg.sBot, tMsgs[sLang].sHelpHeader..tHelp[sLang][tUser.iProfile])
			return false
		end
		return false, tMsgs[sLang].sHelpHeader..tHelp[sLang][tUser.iProfile]
	end},

	[tCmdsConf.tGetChatHistory[1]] = {function(tUser, sLang, sData)
		if #tChat == 0 then
			return true, tMsgs[sLang].sCHEmpty
		end

		local iMsgs = sData:match"^%s+[^ ]+%s+(%d+)"
		iMsgs = math.min(tonumber(iMsgs) or #tChat, #tChat)
		return true, tMsgs[sLang].sCHLong:gsub("%[([A-Z]+)%]", {
			COUNT = iMsgs,
			MSGS  = table.concat(tChat, "\n ", #tChat - iMsgs + 1),
		})
	end},
	[tCmdsConf.tDelChatHistory[1]] = {function(tUser)
		tChat = {}
		bToSave = true
		bChanges = true
		Report(tMsgs[tLangs.Status].sCHCleared:format(tUser.sNick))
		return true
	end},
	[tCmdsConf.tDelMsgByString[1]] = {function(tUser, sLang, sData)
		local sPattern = sData:match"^%s+[^ ]+%s+(.*)"
		if not sPattern or sPattern == "" then
			return true, tMsgs[sLang].sNoArg
		end

		local t = {}
		for i,v in ipairs(tChat) do
			if v:find(sPattern,1,true) then
				table.insert(t,i)
			end
		end
		if #t == 0 then
			return true, tMsgs[sLang].sPatternNotFound:format(sPattern)
		end

		table.sort(t)
		for i=#t,1,-1 do
			table.remove(tChat, t[i])
		end
		bToSave = true
		bChanges = true
		Report(tMsgs[tLangs.Status].sDelMsgPat:format(tUser.sNick, #t, sPattern))
		return true
	end},
	[tCmdsConf.tDelMsgFromNick[1]] = {function(tUser, sLang, sData)
		local sNick = sData:match"^%s+[^ ]+%s+([^ ]+)"
		if not sNick then
			return true, tMsgs[sLang].sNoArg
		end

		local sTS = (tCfg.sTimeStamp:gsub("[%^%$%(%)%%%.%[%]%*%+%-%?]","%%%1")):gsub("%%%%[a-zA-Z]","%[%%da-zA-Z%]+")
		local sNickP = sNick:gsub("[%^%$%(%)%%%.%[%]%*%+%-%?]","%%%1")
		local sPattern1 = "^"..sTS.."<"	..sNickP..">"
		local sPattern2 = "^"..sTS.."%* "..sNickP
		local t = {}
		for i,v in ipairs(tChat) do
			if v:find(sPattern1) or v:find(sPattern2) then
				table.insert(t,i)
			end
		end
		if #t == 0 then
			return true, tMsgs[sLang].sNickNotFound:format(sNick)
		end

		table.sort(t)
		for i=#t,1,-1 do
			table.remove(tChat, t[i])
		end
		bToSave = true
		bChanges = true
		Report(tMsgs[tLangs.Status].sDelMsgNick:format(tUser.sNick, #t, sNick))
		return true
	end},
	[tCmdsConf.tDelMsg[1]] = {function(tUser, sLang, sData)
		local t = {}
		for n in sData:gmatch" (%d+)" do
			table.insert(t, tonumber(n))
		end
		if #t == 0 then
			return true, tMsgs[sLang].sNoArg
		end

		local iChat, iCounter, sRet = #tChat, 0, ""
		table.sort(t)
		for i=1,#t do
			local sMsg = table.remove(tChat, iChat - t[i] + 1)
			if sMsg then
				iCounter = iCounter + 1
				sRet = ("%s\n\t%s: %s"):format(sRet, t[i], sMsg)
			else
				sRet = tMsgs[sLang].sMsgNotFound:format(sRet, t[i])
			end
		end
		if iCounter == 0 then
			return true, sRet
		end

		bToSave = true
		bChanges = true
		Report(tMsgs[tLangs.Status].sDelMsg:format(tUser.sNick, iCounter, sRet))
		return true
	end},
}