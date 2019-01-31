Menu = "���.���������\\"
UserMenu = "����\\"
AdminMenu = "���������\\"

ScriptEasy = false	--���������� ���� ���������� ��������� (true - ��, false - ���)
TempOP = true	--��������� ���� "��������� ��������" (true - ��, false - ���)
RestartHub = true	--��������� ���� "���������� ����" (true - ��, false - ���)
Prefix = ""	--������� ������ ����. ���� �� ������, ������������ ������ ������� �� �������� ����.
sEnable = "+" -- ������ ����� �������� � ����, ������������ ��� ������ �������.
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
	--���������� �������
	Core.SendToUser(user,"$UserCommand 1 3 "..Menu.."������$<%[mynick]> "..Prefix.."help&#124;")
	Core.SendToUser(user,"$UserCommand 1 3 "..Menu.."�������� ��� IP �����$<%[mynick]> "..Prefix.."myip&#124;")
	if t then
		--���� �����
		if t.bGetInfo then
			Core.SendToUser(user,"$UserCommand 1 2 "..UserMenu.."�������� ����������$<%[mynick]> "..Prefix.."getinfo %[nick]&#124;")
			if t.bDrop or t.bKick or t.bTempBan or t.bBan or t.bAddRegUser or t.bDelRegUser or t.bMassMsg then
				Core.SendToUser(user,"$UserCommand 0 2")
			end
		end
		if t.bAddRegUser then
			Core.SendToUser(user,"$UserCommand 1 2 "..UserMenu.."�����������\\���������������� ������������$<%[mynick]> "..Prefix.."addreguser %[nick] %[line:������] %[line:��� �������]&#124;")
		end
		if t.bDelRegUser then
			Core.SendToUser(user,"$UserCommand 1 2 "..UserMenu.."�����������\\������� �����������$<%[mynick]> "..Prefix.."delreguser %[nick]&#124;")
		end
		if t.bMassMsg then
			Core.SendToUser(user,"$UserCommand 1 2 "..UserMenu.."��������� �� ����� ����$<%[mynick]> "..Prefix.."frombot %[nick] %[line:������� ����� ���������]&#124;")
		end
		if t.bTempOP and TempOP then
			Core.SendToUser(user,"$UserCommand 1 2 "..UserMenu.."��������� ��������$<%[mynick]> "..Prefix.."op %[nick]&#124;")
		end
		if (t.bAddRegUser or t.bDelRegUser or t.bMassMsg or (t.bTempOP and TempOP)) and (t.bDrop or t.bKick or t.bTempBan or t.bBan) then
			Core.SendToUser(user,"$UserCommand 0 2")
		end
		if t.bDrop then
			Core.SendToUser(user,"$UserCommand 1 2 "..UserMenu.."���������$<%[mynick]> "..Prefix.."disconnect %[nick]&#124;")
		end
		if t.bKick then
			Core.SendToUser(user,"$UserCommand 1 2 "..UserMenu.."�������$<%[mynick]> "..Prefix.."drop %[nick] %[line:�������]&#124;")
		end
		if t.bTempBan then
			Core.SendToUser(user,"$UserCommand 1 2 "..UserMenu.."��� 1 ���$<%[mynick]> "..Prefix.."nicktempban %[nick] 1h %[line:�������]&#124;")
			Core.SendToUser(user,"$UserCommand 1 2 "..UserMenu.."��� 24 ����$<%[mynick]> "..Prefix.."nicktempban %[nick] 1d %[line:�������]&#124;")
			Core.SendToUser(user,"$UserCommand 1 2 "..UserMenu.."��������� ���...$<%[mynick]> "..Prefix.."nicktempban %[nick] %[line:����� (m = �����, h = �����, d = ����, w = ������)] %[line:�������]&#124;")
		end
		if t.bBan then
			Core.SendToUser(user,"$UserCommand 1 2 "..UserMenu.."���������� ���$<%[mynick]> "..Prefix.."nickban %[nick] %[line:�������]&#124;")
		end
		--���������� �����
		if t.bTopic then
			Core.SendToUser(user,"$UserCommand 1 3 "..AdminMenu.."�����\\���������� �����$<%[mynick]> "..Prefix.."topic %[line:������� �����]&#124;")
			Core.SendToUser(user,"$UserCommand 1 3 "..AdminMenu.."�����\\�������� �����$<%[mynick]> "..Prefix.."topic off&#124;")
		end
		if t.bRefreshTxt then
			Core.SendToUser(user,"$UserCommand 1 3 "..AdminMenu.."��������� �����\\�������. �����. �����$<%[mynick]> "..Prefix.."reloadtxt&#124;")
		end
		if t.bMassMsg then
			Core.SendToUser(user,"$UserCommand 1 3 "..AdminMenu.."�������� ���������\\�������� ��������$<%[mynick]> "..Prefix.."massmsg %[line:������� ����� ���������]&#124;")
			Core.SendToUser(user,"$UserCommand 1 3 "..AdminMenu.."�������� ���������\\�������� ����$<%[mynick]> "..Prefix.."opmassmsg %[line:������� ����� ���������]&#124;")
			Core.SendToUser(user,"$UserCommand 1 3 "..AdminMenu.."��������� �� ����� ����$<%[mynick]> "..Prefix.."frombot %[line:���] %[line:������� ����� ���������]&#124;")
		end
		--������ �����
		if t.bGetBans then
			Core.SendToUser(user,"$UserCommand 1 3 "..AdminMenu.."����\\������ �����$<%[mynick]> "..Prefix.."getbans&#124;")
			Core.SendToUser(user,"$UserCommand 1 3 "..AdminMenu.."����\\������ ��������� �����$<%[mynick]> "..Prefix.."gettempbans&#124;")
			Core.SendToUser(user,"$UserCommand 1 3 "..AdminMenu.."����\\������ ���������� �����$<%[mynick]> "..Prefix.."getpermbans&#124;")
			Core.SendToUser(user,"$UserCommand 0 3")
		end
		--��������� ����
		if t.bTempBan then
			Core.SendToUser(user,"$UserCommand 1 3 "..AdminMenu.."����\\��������� ���$<%[mynick]> "..Prefix.."nicktempban %[line:���] %[line:����� (m = �����, h = �����, d = ����, w = ������)] %[line:�������]&#124;")
			Core.SendToUser(user,"$UserCommand 1 3 "..AdminMenu.."����\\��������� ��� IP$<%[mynick]> "..Prefix.."tempbanip %[line:������� IP] %[line:����� (m = �����, h = �����, d = ����, w = ������)] %[line:�������]&#124;")
			Core.SendToUser(user,"$UserCommand 1 3 "..AdminMenu.."����\\��������� ��� IP (������)$<%[mynick]> "..Prefix.."fulltempbanip %[line:������� IP] %[line:����� (m = �����, h = �����, d = ����, w = ������)] %[line:�������]&#124;")
			Core.SendToUser(user,"$UserCommand 0 3")
		end
		if t.bTempUnban then
			Core.SendToUser(user,"$UserCommand 1 3 "..AdminMenu.."����\\����� ��������� ���$<%[mynick]> "..Prefix.."tempunban %[line:IP ��� ���]&#124;")
			Core.SendToUser(user,"$UserCommand 0 3")
		end
		--���������� ����
		if t.bBan then
			Core.SendToUser(user,"$UserCommand 1 3 "..AdminMenu.."����\\���$<%[mynick]> "..Prefix.."nickban %[line:���] %[line:�������]&#124;")
			Core.SendToUser(user,"$UserCommand 1 3 "..AdminMenu.."����\\��� IP$<%[mynick]> "..Prefix.."banip %[line:������� IP] %[line:�������]&#124;")
			Core.SendToUser(user,"$UserCommand 1 3 "..AdminMenu.."����\\��� IP (������)$<%[mynick]> "..Prefix.."fullbanip %[line:������� IP] %[line:�������]&#124;")
			Core.SendToUser(user,"$UserCommand 0 3")
		end
		if t.bUnban then
			Core.SendToUser(user,"$UserCommand 1 3 "..AdminMenu.."����\\����� ���$<%[mynick]> "..Prefix.."unban %[line:��� ��� IP]&#124;")
			Core.SendToUser(user,"$UserCommand 0 3")
		end
		--������ ���������� �����
		if t.bGetRangeBans then
			Core.SendToUser(user,"$UserCommand 1 3 "..AdminMenu.."���� ����������\\������ ���������� ����� $<%[mynick]> "..Prefix.."getrangebans&#124;")
			Core.SendToUser(user,"$UserCommand 1 3 "..AdminMenu.."���� ����������\\������ ��������� ����� ����������$<%[mynick]> "..Prefix.."getrangetempbans&#124;")
			Core.SendToUser(user,"$UserCommand 1 3 "..AdminMenu.."���� ����������\\������ ���������� ����� ����������$<%[mynick]> "..Prefix.."getrangepermbans&#124;")
			Core.SendToUser(user,"$UserCommand 0 3")
		end
		--��������� ���� ����������
		if t.bRangeTempBan then
			Core.SendToUser(user,"$UserCommand 1 3 "..AdminMenu.."���� ����������\\��������� ��� ���������$<%[mynick]> "..Prefix.."rangetempban %[line:��������� IP ���������] %[line:�������� IP ���������] %[line:����� (m = �����, h = �����, d = ����, w = ������)] %[line:�������]&#124;")
			Core.SendToUser(user,"$UserCommand 1 3 "..AdminMenu.."���� ����������\\��������� ��� ��������� (������)$<%[mynick]> "..Prefix.."fullrangetempban %[line:��������� IP ���������] %[line:�������� IP ���������] %[line:����� (m = �����, h = �����, d = ����, w = ������)] %[line:�������]&#124;")
			Core.SendToUser(user,"$UserCommand 0 3")
		end
		if t.bRangeTempUnban then
			Core.SendToUser(user,"$UserCommand 1 3 "..AdminMenu.."���� ����������\\����� ��������� ��� ���������$<%[mynick]> "..Prefix.."tempunban %[line:��������� IP ���������] %[line:�������� IP ���������]&#124;")
			Core.SendToUser(user,"$UserCommand 0 3")
		end
		--���������� ���� ����������
		if t.bRangeBan then
			Core.SendToUser(user,"$UserCommand 1 3 "..AdminMenu.."���� ����������\\��� ���������$<%[mynick]> "..Prefix.."rangeban %[line:��������� IP ���������] %[line:�������� IP ���������] %[line:�������]&#124;")
			Core.SendToUser(user,"$UserCommand 1 3 "..AdminMenu.."���� ����������\\��� ��������� (������)$<%[mynick]> "..Prefix.."fullrangeban %[line:��������� IP ���������] %[line:�������� IP ���������] %[line:�������]&#124;")
			Core.SendToUser(user,"$UserCommand 0 3")
		end
		if t.bRangeUnban then
			Core.SendToUser(user,"$UserCommand 1 3 "..AdminMenu.."���� ����������\\����� ��� ���������$<%[mynick]> "..Prefix.."rangepermunban %[line:��������� IP ���������] %[line:�������� IP ���������]&#124;")
			Core.SendToUser(user,"$UserCommand 0 3")
		end
		--�����������
		if t.bAddRegUser then
			Core.SendToUser(user,"$UserCommand 1 3 "..AdminMenu.."�����������\\���������������� ������������$<%[mynick]> "..Prefix.."addreguser %[line:���] %[line:������] %[line:��� �������]&#124;")
		end
		if t.bDelRegUser then
			Core.SendToUser(user,"$UserCommand 1 3 "..AdminMenu.."�����������\\������� �����������$<%[mynick]> "..Prefix.."delreguser %[line:���]&#124;")
		end
		--���������� ���������
		if t.bRestartScripts then
			Core.SendToUser(user,"$UserCommand 1 3 "..AdminMenu.."�������\\���������� ������$<%[mynick]> "..Prefix.."getscripts&#124;")
			Core.SendToUser(user,"$UserCommand 1 3 "..AdminMenu.."�������\\������������� �������$<%[mynick]> "..Prefix.."restartscripts&#124;")
			Core.SendToUser(user,"$UserCommand 0 3")
			if ScriptEasy then
				Core.SendToUser(user,"$UserCommand 1 3 "..AdminMenu.."�������\\����������$<%[mynick]> "..Prefix.."restartscript %[line:��� �����]&#124;")
				Core.SendToUser(user,"$UserCommand 1 3 "..AdminMenu.."�������\\�����$<%[mynick]> "..Prefix.."startscript %[line:��� �����]&#124;")
				Core.SendToUser(user,"$UserCommand 1 3 "..AdminMenu.."�������\\����$<%[mynick]> "..Prefix.."stopscript %[line:��� �����]&#124;")
				Core.SendToUser(user,"$UserCommand 1 3 "..AdminMenu.."�������\\�������� �����$<%[mynick]> "..Prefix.."scriptmoveup %[line:��� �����]&#124;")
				Core.SendToUser(user,"$UserCommand 1 3 "..AdminMenu.."�������\\�������� ����$<%[mynick]> "..Prefix.."scriptmovedown %[line:��� �����]&#124;")
			else
				local sEn = sEnable.." "
				local tScripts = ScriptMan.GetScripts()
				for script in pairs(tScripts) do
					local CurScript = tScripts[script].sName
					local bEnabled = tScripts[script].bEnabled or false
					if bEnabled then Scr = sEn..CurScript else Scr = "   "..CurScript end
					Core.SendToUser(user,"$UserCommand 1 3 "..AdminMenu.."�������\\"..Scr.."\\����������$<%[mynick]> "..Prefix.."restartscript "..CurScript.."&#124;")
					Core.SendToUser(user,"$UserCommand 1 3 "..AdminMenu.."�������\\"..Scr.."\\�����$<%[mynick]> "..Prefix.."startscript "..CurScript.."&#124;")
					Core.SendToUser(user,"$UserCommand 1 3 "..AdminMenu.."�������\\"..Scr.."\\����$<%[mynick]> "..Prefix.."stopscript "..CurScript.."&#124;")
					Core.SendToUser(user,"$UserCommand 1 3 "..AdminMenu.."�������\\"..Scr.."\\�������� �����$<%[mynick]> "..Prefix.."scriptmoveup "..CurScript.."&#124;")
					Core.SendToUser(user,"$UserCommand 1 3 "..AdminMenu.."�������\\"..Scr.."\\�������� ����$<%[mynick]> "..Prefix.."scriptmovedown "..CurScript.."&#124;")
				end
			end
		end
		--����������
		if t.bIsOP then
			Core.SendToUser(user,"$UserCommand 1 3 "..AdminMenu.."����������$<%[mynick]> "..Prefix.."stats&#124;")
		end
		if t.bRestartHub and RestartHub then
			Core.SendToUser(user,"$UserCommand 1 3 "..AdminMenu.."���������� ����$<%[mynick]> "..Prefix.."restart&#124;")
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
							ToOps(user.sNick..": ������ "..param.." ��������� ����� �� ���� �������.")
						else
							ToUser(user,"������: ������ "..param.." ����������� �� �������.")
						end
					else
						ToUser(user,"<"..bot.."> ������. �� ������ ������� ��� �����.")
					end
					return true
				elseif cmd and cmd == "scriptmovedown" then
					if param then
						result = ScriptMan.MoveDown(param)
						if result then
							ToOps(user.sNick..": ������ "..param.." ��������� ���� �� ���� �������.")
						else
							ToUser(user,"������: ������ "..param.." ����������� �� �������.")
						end
					else
						ToUser(user,"<"..bot.."> ������. �� ������ ������� ��� �����.")
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
							ToOps(user.sNick.." �������� ����� "..param)
						else
							ToUser(user,"������: ���� "..param.." �� ������ �� ����")
						end
					else
						ToUser(user,"������: �� ������ ������� ���.")
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
							ToUser(user,"��������� ����������")
							ToOps(user.sNick.." �������� ��������� �� ����� ���� ����� "..nick.." :"..msg)
						else
							ToUser(user,"������: ���� "..nick.." �� ������ �� ����")
						end
					else
						ToUser(user,"������ ����������. ���������: "..Prefix.."frombot <���> <����� ���������>")
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