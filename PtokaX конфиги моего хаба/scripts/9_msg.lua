sBot = SetMan.GetString(21)
local sMsg=[[ �������������� ������������� ���, ������������� � ������ TP-LINK.
- ��� �� �������� �� ������. ��� ������������ ������ ���������� ����� ��������������.
 �� ������� �� ���������� ����������� ���������� �������������!
- ���� ��������� ������ (������� ����������) ������ ����� ������� ������� ������ � �������
 *����:������� ��.������� ���� �� ������ ����� - ���.���������,��� �� ����� �������� � ���: !prob
  
 ���� ���� � ��� WI-FI �������, � ������ ��� ����� ����� ������� �������� � �����. ������ � ����]]

function UserConnected(tUser)
  Core.SendToUser(tUser,"<"..sBot.."> "..sMsg)
end
RegConnected=UserConnected
OpConnected=UserConnected