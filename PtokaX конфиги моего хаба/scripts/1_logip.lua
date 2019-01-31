local Folder = "Log"  -- Имя папки для лог-файла (можно изменить по желанию)

function Start() _ = {} end

function OnStartup()
	folder = Core.GetPtokaXPath().."/scripts/"..Folder.."/"
	set = folder.."log_users.txt"	-- имя лог-файла (можно изменить по желанию)
	if loadfile(set) then
		dofile(set)
	else
		local _ = "\""
		os.execute("mkdir ".._..folder.._)
		Start()
	end
	if loadfile(set) then dofile(set) end
	Save()
end

function UserConnected(user)
	_ = {}
	_[os.date("%d.%m.%Y  %X")] = "Вход пользователя <"..user.sNick.."> с IP-адреса: ["..user.sIP.."]"
	Save()
end
OpConnected, RegConnected = UserConnected, UserConnected

function Save_Serialize(tTable, sTableName, hFile, sTab)
	sTab = sTab or "";
	hFile:write(sTab..sTableName);
	for key, value in pairs(tTable) do
		local sKey = (type(key) == "string") and string.format(" %q ",key) or string.format(" %d ",key);
		if(type(value) == "table") then
			Save_Serialize(value, sKey, hFile, sTab);
		else
			local sValue = (type(value) == "string") and string.format(" %q ",value) or tostring(value);
			hFile:write( sTab..sKey..sValue);
		end
		hFile:write( "\n");
	end
	hFile:write(sTab);
end

function Save() Save_file(set, _, "-") end

function Save_file(file, table, tablename)
	local hFile = io.open (file, "a")
	Save_Serialize(table, tablename, hFile);
	hFile:close()
end