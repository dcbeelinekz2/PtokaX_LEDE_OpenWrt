Folder = "Log"  	-- ��� ����� ��� ���-����� (����� �������� �� �������)
name = "log_lichka.txt"	-- ��� ���-����� (����� �������� �� �������)
bot = "��"	-- ��� ����� �������
------------------------------------------------------------------------------------------------------------------------
bFlag1 = true	-- �������� ��������� ��������� ������������� ��������� ����� �� ������� ( true - ��,   false - ��� )
------------------------------------------------------------------------------------------------------------------------
bFlag2 = true	-- ���������� ��������� ��������� ������������� � ���-���� ( true - ��,   false - ��� )
------------------------------------------------------------------------------------------------------------------------
Nicks = {		-- ������� �����, ������� ������ ������ ��������� ��������� ������������� � �������� ������� (� ��������� ����)
	"DCHUB",	-- ��� ���������� ���� � �������, �� �������� ������� ������� � ����� !!!
}
------------------------------------------------------------------------------------------------------------------------
Bots = {		-- ������� ����� ����, �� �������� �� ����� ������� ���������� � �������� ������� (�� � ���-���� ��������� ���������)
	"OpChat",	-- �� �� ����� � �������� !!!
---	"DCBEELINEKZ",
}
------------------------------------------------------------------------------------------------------------------------
function OnStartup() folder=Core.GetPtokaXPath().."/scripts/"..Folder.."/" set=folder..name if loadfile(set) then else nt={} local _="\"" os.execute("mkdir ".._..folder.._) Save() end end function ToArrival(user,data) local data=data:sub(1,-2) local to,from,msg=data:match"$To:%s+(%S+)%s+From:%s+(%S+)%s+$%b<>%s+(.*)" if bFlag1 then SL(user.sNick,msg,to) end if bFlag2 then nt={} nt[os.date("%d-%m-%Y  %X")]="["..user.sIP.."] <"..from.."> "..to..": "..msg Save() end end function SL(nick,sData,too) local dat="<"..nick..">  �����  <"..too..">  -->  "..sData local flg,flg1=true,true for i,v in pairs(Nicks) do if nick==v or too==v then flg=false end end if flg then for i,v in pairs(Bots) do if too==v then flg1=false end end if flg1 then for i,v in pairs(Nicks) do Core.SendPmToNick(v,bot,dat) end end end end function Save_Serialize(tTable,sTableName,hFile,sTab) sTab=sTab or "" hFile:write(sTab..sTableName) for key,value in pairs(tTable) do local sKey=(type(key)=="string") and string.format(" %q ",key) or string.format(" %d ",key) if(type(value)=="table") then Save_Serialize(value,sKey,hFile,sTab) else local sValue=(type(value)=="string") and string.format(" %q ",value) or tostring(value) hFile:write(sTab..sKey..sValue) end hFile:write("\n") end hFile:write(sTab) end function Save() Save_file(set,nt,"") end function Save_file(file,table,tablename) local hFile=io.open(file,"a") Save_Serialize(table,tablename,hFile) hFile:close() end
------------------------------------------------------------------------------------------------------------------------