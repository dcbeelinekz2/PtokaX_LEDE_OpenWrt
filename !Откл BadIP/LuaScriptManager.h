/*
 * PtokaX - hub server for Direct Connect peer to peer network.

 * Copyright (C) 2002-2005  Ptaczek, Ptaczek at PtokaX dot org
 * Copyright (C) 2004-2013  Petr Kozelka, PPK at PtokaX dot org

 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3
 * as published by the Free Software Foundation.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

//------------------------------------------------------------------------------
#ifndef LuaScriptManagerH
#define LuaScriptManagerH
//------------------------------------------------------------------------------
struct lua_State;
struct Script;
struct ScriptTimer;
struct User;
//------------------------------------------------------------------------------

class clsScriptManager {
private:
	Script *RunningScriptE;

	void AddRunningScript(Script * curScript);
	void RemoveRunningScript(Script * curScript);
public:
    static clsScriptManager * mPtr;

    enum LuaArrivals {
        CHAT_ARRIVAL,
        KEY_ARRIVAL,
        VALIDATENICK_ARRIVAL,
        PASSWORD_ARRIVAL,
        VERSION_ARRIVAL,
        GETNICKLIST_ARRIVAL,
        MYINFO_ARRIVAL,
        GETINFO_ARRIVAL,
        SEARCH_ARRIVAL,
        TO_ARRIVAL,
        CONNECTTOME_ARRIVAL,
        MULTICONNECTTOME_ARRIVAL,
        REVCONNECTTOME_ARRIVAL,
        SR_ARRIVAL,
        UDP_SR_ARRIVAL,
        KICK_ARRIVAL,
        OPFORCEMOVE_ARRIVAL,
        SUPPORTS_ARRIVAL,
        BOTINFO_ARRIVAL,
        CLOSE_ARRIVAL, 
        UNKNOWN_ARRIVAL,
		VALIDATE_DENIDE_ARRIVAL,
    };

    Script *RunningScriptS;

    Script **ScriptTable;
	User *ActualUser;

    uint8_t ui8ScriptCount, ui8BotsCount;

    bool bMoved;
    
    clsScriptManager();
    ~clsScriptManager();
    
    void Start();
    void Stop();

    void SaveScripts();

    void CheckForDeletedScripts();
    void CheckForNewScripts();

    void Restart();

    Script * FindScript(char * sName);
    Script * FindScript(lua_State * L);
    uint8_t FindScriptIdx(char * sName);

	bool AddScript(char * sName, const bool &bEnabled, const bool &bNew);

	bool StartScript(Script * curScript, const bool &bEnable);
	void StopScript(Script * curScript, const bool &bDisable);

	void MoveScript(const uint8_t &ui8ScriptPosInTbl, const bool &bUp);

    void DeleteScript(const uint8_t &ui8ScriptPosInTbl);

    void OnStartup();
    void OnExit(bool bForce = false);
    bool Arrival(User * u, char * sData, const size_t &szLen, const unsigned char &uiType);
    bool UserConnected(User * u);
	void UserDisconnected(User * u, Script * pScript = NULL);

    void PrepareMove(lua_State * L);
};
//------------------------------------------------------------------------------

#endif
