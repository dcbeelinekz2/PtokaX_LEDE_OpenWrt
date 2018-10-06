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
#include "stdinc.h"
//------------------------------------------------------------------------------
#include "LuaInc.h"
//------------------------------------------------------------------------------
#include "LuaScriptManager.h"
//------------------------------------------------------------------------------
#include "ServerManager.h"
#include "SettingManager.h"
#include "User.h"
#include "utility.h"
//------------------------------------------------------------------------------
#ifdef _WIN32
	#pragma hdrstop
#endif
//------------------------------------------------------------------------------
#include "LuaScript.h"
//------------------------------------------------------------------------------
#ifdef _BUILD_GUI
    #include "../gui.win/MainWindowPageScripts.h"
#endif
//------------------------------------------------------------------------------
clsScriptManager * clsScriptManager::mPtr = NULL;
//------------------------------------------------------------------------------

clsScriptManager::clsScriptManager() {
    RunningScriptS = NULL;
    RunningScriptE = NULL;

    ScriptTable = NULL;
    
    ActualUser = NULL;

    ui8ScriptCount = 0;
    ui8BotsCount = 0;

    bMoved = false;

#ifdef _WIN32
    clsServerManager::hLuaHeap = ::HeapCreate(HEAP_NO_SERIALIZE, 0x80000, 0);
#endif

	// PPK ... first start all script in order from xml file
#ifdef _WIN32
	TiXmlDocument doc((clsServerManager::sPath+"\\cfg\\Scripts.xml").c_str());
#else
	TiXmlDocument doc((clsServerManager::sPath+"/cfg/Scripts.xml").c_str());
#endif
	if(doc.LoadFile()) {
		TiXmlHandle cfg(&doc);
		TiXmlNode *scripts = cfg.FirstChild("Scripts").Node();
		if(scripts != NULL) {
			TiXmlNode *child = NULL;
			while((child = scripts->IterateChildren(child)) != NULL) {
				TiXmlNode *script = child->FirstChild("Name");
    
				if(script == NULL || (script = script->FirstChild()) == NULL) {
					continue;
				}
    
				char *name = (char *)script->Value();

				if(FileExist((clsServerManager::sScriptPath+string(name)).c_str()) == false) {
					continue;
				}

				if((script = child->FirstChild("Enabled")) == NULL ||
					(script = script->FirstChild()) == NULL) {
					continue;
				}
    
				bool enabled = atoi(script->Value()) == 0 ? false : true;

				if(FindScript(name) != NULL) {
					continue;
				}

				AddScript(name, enabled, false);
            }
        }
    }
}
//------------------------------------------------------------------------------

clsScriptManager::~clsScriptManager() {
    RunningScriptS = NULL;
    RunningScriptE = NULL;

    for(uint8_t ui8i = 0; ui8i < ui8ScriptCount; ui8i++) {
		delete ScriptTable[ui8i];
    }

#ifdef _WIN32
	if(HeapFree(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE, (void *)ScriptTable) == 0) {
		AppendDebugLog("%s - [MEM] Cannot deallocate ScriptTable in clsScriptManager::~clsScriptManager\n", 0);
	}
#else
	free(ScriptTable);
#endif

	ScriptTable = NULL;

	ui8ScriptCount = 0;

    ActualUser = NULL;

    ui8BotsCount = 0;

#ifdef _WIN32
    ::HeapDestroy(clsServerManager::hLuaHeap);
#endif
}
//------------------------------------------------------------------------------

void clsScriptManager::Start() {
    ui8BotsCount = 0;
    ActualUser = NULL;

    
    // PPK ... first look for deleted and new scripts
    CheckForDeletedScripts();
    CheckForNewScripts();

    // PPK ... second start all enabled scripts
    for(uint8_t ui8i = 0; ui8i < ui8ScriptCount; ui8i++) {
		if(ScriptTable[ui8i]->bEnabled == true) {
        	if(ScriptStart(ScriptTable[ui8i]) == true) {
				AddRunningScript(ScriptTable[ui8i]);
			} else {
                ScriptTable[ui8i]->bEnabled = false;
            }
		}
	}

#ifdef _BUILD_GUI
    clsMainWindowPageScripts::mPtr->AddScriptsToList(true);
#endif
}
//------------------------------------------------------------------------------

#ifdef _BUILD_GUI
bool clsScriptManager::AddScript(char * sName, const bool &bEnabled, const bool &bNew) {
#else
bool clsScriptManager::AddScript(char * sName, const bool &bEnabled, const bool &/*bNew*/) {
#endif
	if(ui8ScriptCount == 254) {
        return false;
    }

    Script ** oldbuf = ScriptTable;
#ifdef _WIN32
    if(ScriptTable == NULL) {
        ScriptTable = (Script **)HeapAlloc(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE | HEAP_ZERO_MEMORY, (ui8ScriptCount+1)*sizeof(Script *));
    } else {
		ScriptTable = (Script **)HeapReAlloc(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE, (void *)oldbuf, (ui8ScriptCount+1)*sizeof(Script *));
    }
#else
	ScriptTable = (Script **)realloc(oldbuf, (ui8ScriptCount+1)*sizeof(Script *));
#endif
	if(ScriptTable == NULL) {
        ScriptTable = oldbuf;

		AppendDebugLog("%s - [MEM] Cannot (re)allocate ScriptTable in clsScriptManager::AddScript\n", 0);
        return false;
    }

    ScriptTable[ui8ScriptCount] = Script::CreateScript(sName, bEnabled);

    if(ScriptTable[ui8ScriptCount] == NULL) {
        AppendDebugLog("%s - [MEM] Cannot allocate new Script in clsScriptManager::AddScript\n", 0);

        return false;
    }

    ui8ScriptCount++;

#ifdef _BUILD_GUI
    if(bNew == true) {
        clsMainWindowPageScripts::mPtr->ScriptToList(ui8ScriptCount-1, true, false);
    }
#endif

    return true;
}
//------------------------------------------------------------------------------

void clsScriptManager::Stop() {
	Script *next = RunningScriptS;

    RunningScriptS = NULL;
	RunningScriptE = NULL;

    while(next != NULL) {
		Script *S = next;
		next = S->next;

		ScriptStop(S);
	}

	ActualUser = NULL;

#ifdef _BUILD_GUI
    clsMainWindowPageScripts::mPtr->ClearMemUsageAll();
#endif
}
//------------------------------------------------------------------------------

void clsScriptManager::AddRunningScript(Script * curScript) {
	if(RunningScriptS == NULL) {
		RunningScriptS = curScript;
		RunningScriptE = curScript;
		return;
	}

   	curScript->prev = RunningScriptE;
    RunningScriptE->next = curScript;
    RunningScriptE = curScript;
}
//------------------------------------------------------------------------------

void clsScriptManager::RemoveRunningScript(Script * curScript) {
	// single entry
	if(curScript->prev == NULL && curScript->next == NULL) {
    	RunningScriptS = NULL;
        RunningScriptE = NULL;
        return;
    }

    // first in list
	if(curScript->prev == NULL) {
        RunningScriptS = curScript->next;
        RunningScriptS->prev = NULL;
        return;
    }

    // last in list
    if(curScript->next == NULL) {
        RunningScriptE = curScript->prev;
    	RunningScriptE->next = NULL;
        return;
    }

    // in the middle
    curScript->prev->next = curScript->next;
    curScript->next->prev = curScript->prev;
}
//------------------------------------------------------------------------------

void clsScriptManager::SaveScripts() {
#ifdef _WIN32
    TiXmlDocument doc((clsServerManager::sPath+"\\cfg\\Scripts.xml").c_str());
#else
	TiXmlDocument doc((clsServerManager::sPath+"/cfg/Scripts.xml").c_str());
#endif
    doc.InsertEndChild(TiXmlDeclaration("1.0", "windows-1252", "yes"));
    TiXmlElement scripts("Scripts");

	for(uint8_t ui8i = 0; ui8i < ui8ScriptCount; ui8i++) {
		if(FileExist((clsServerManager::sScriptPath+string(ScriptTable[ui8i]->sName)).c_str()) == false) {
			continue;
        }

        TiXmlElement name("Name");
        name.InsertEndChild(TiXmlText(ScriptTable[ui8i]->sName));
        
        TiXmlElement enabled("Enabled");
        if(ScriptTable[ui8i]->bEnabled == true) {
            enabled.InsertEndChild(TiXmlText("1"));
        } else {
            enabled.InsertEndChild(TiXmlText("0"));
        }
        
        TiXmlElement script("Script");
        script.InsertEndChild(name);
        script.InsertEndChild(enabled);
        
        scripts.InsertEndChild(script);
    }
    doc.InsertEndChild(scripts);
    doc.SaveFile();
}
//------------------------------------------------------------------------------

void clsScriptManager::CheckForDeletedScripts() {
    uint8_t ui8i = 0;

	while(ui8i < ui8ScriptCount) {
		if(FileExist((clsServerManager::sScriptPath+string(ScriptTable[ui8i]->sName)).c_str()) == true || ScriptTable[ui8i]->LUA != NULL) {
			ui8i++;
			continue;
        }

		delete ScriptTable[ui8i];

		for(uint8_t ui8j = ui8i; ui8j+1 < ui8ScriptCount; ui8j++) {
            ScriptTable[ui8j] = ScriptTable[ui8j+1];
        }

        ScriptTable[ui8ScriptCount-1] = NULL;
        ui8ScriptCount--;
    }
}
//------------------------------------------------------------------------------

void clsScriptManager::CheckForNewScripts() {
#ifdef _WIN32
    struct _finddata_t luafile;
    intptr_t hFile = _findfirst((clsServerManager::sScriptPath+"\\*.lua").c_str(), &luafile);

    if(hFile != -1) {
        do {
			if((luafile.attrib & _A_SUBDIR) != 0 ||
				stricmp(luafile.name+(strlen(luafile.name)-4), ".lua") != 0) {
				continue;
			}

			if(FindScript(luafile.name) != NULL) {
                continue;
            }

			AddScript(luafile.name, false, false);
        } while(_findnext(hFile, &luafile) == 0);

		_findclose(hFile);
	}
#else
    DIR * p_scriptdir = opendir(clsServerManager::sScriptPath.c_str());

    if(p_scriptdir == NULL) {
        return;
    }

    struct dirent * p_dirent;
    struct stat s_buf;

    while((p_dirent = readdir(p_scriptdir)) != NULL) {
        if(stat((clsServerManager::sScriptPath + p_dirent->d_name).c_str(), &s_buf) != 0 ||
            (s_buf.st_mode & S_IFDIR) != 0 || 
            strcasecmp(p_dirent->d_name + (strlen(p_dirent->d_name)-4), ".lua") != 0) {
            continue;
        }

		if(FindScript(p_dirent->d_name) != NULL) {
            continue;
        }

		AddScript(p_dirent->d_name, false, false);
    }

    closedir(p_scriptdir);
#endif
}
//------------------------------------------------------------------------------

void clsScriptManager::Restart() {
	OnExit();
	Stop();

    CheckForDeletedScripts();

	Start();
	OnStartup();

#ifdef _BUILD_GUI
    clsMainWindowPageScripts::mPtr->AddScriptsToList(true);
#endif
}
//------------------------------------------------------------------------------

Script * clsScriptManager::FindScript(char * sName) {
    for(uint8_t ui8i = 0; ui8i < ui8ScriptCount; ui8i++) {
		if(strcasecmp(ScriptTable[ui8i]->sName, sName) == 0) {
            return ScriptTable[ui8i];
        }
    }

    return NULL;
}
//------------------------------------------------------------------------------

Script * clsScriptManager::FindScript(lua_State * L) {
    Script *next = RunningScriptS;

    while(next != NULL) {
    	Script *cur = next;
        next = cur->next;

        if(cur->LUA == L) {
            return cur;
        }
    }

    return NULL;
}
//------------------------------------------------------------------------------

uint8_t clsScriptManager::FindScriptIdx(char * sName) {
    for(uint8_t ui8i = 0; ui8i < ui8ScriptCount; ui8i++) {
		if(strcasecmp(ScriptTable[ui8i]->sName, sName) == 0) {
            return ui8i;
        }
    }

    return ui8ScriptCount;
}
//------------------------------------------------------------------------------

bool clsScriptManager::StartScript(Script * curScript, const bool &bEnable) {
    uint8_t ui8dx = 255;
    for(uint8_t ui8i = 0; ui8i < ui8ScriptCount; ui8i++) {
        if(curScript == ScriptTable[ui8i]) {
            ui8dx = ui8i;
            break;
        }
    }

    if(ui8dx == 255) {
        return false;
	}

    if(bEnable == true) {
        curScript->bEnabled = true;
#ifdef _BUILD_GUI
        clsMainWindowPageScripts::mPtr->UpdateCheck(ui8dx);
#endif
    }

    if(ScriptStart(curScript) == false) {
        curScript->bEnabled = false;
#ifdef _BUILD_GUI
        clsMainWindowPageScripts::mPtr->UpdateCheck(ui8dx);
#endif
        return false;
    }

	if(RunningScriptS == NULL) {
		RunningScriptS = curScript;
		RunningScriptE = curScript;
	} else {
		// previous script
		if(ui8dx != 0) {
			for(int16_t i16i = (int16_t)(ui8dx-1); i16i > -1; i16i--) {
				if(ScriptTable[i16i]->bEnabled == true) {
					ScriptTable[i16i]->next = curScript;
					curScript->prev = ScriptTable[i16i];
					break;
				}
			}

			if(curScript->prev == NULL) {
				RunningScriptS = curScript;
			}
		} else {
			curScript->next = RunningScriptS;
			RunningScriptS->prev = curScript;
			RunningScriptS = curScript;
        }

		// next script
		if(ui8dx != ui8ScriptCount-1) {
			for(uint8_t ui8i = ui8dx+1; ui8i < ui8ScriptCount; ui8i++) {
				if(ScriptTable[ui8i]->bEnabled == true) {
					ScriptTable[ui8i]->prev = curScript;
					curScript->next = ScriptTable[ui8i];
					break;
				}
			}

			if(curScript->next == NULL) {
				RunningScriptE = curScript;
			}
		} else {
			curScript->prev = RunningScriptE;
			RunningScriptE->next = curScript;
			RunningScriptE = curScript;
        }
	}


	if(clsServerManager::bServerRunning == true) {
        ScriptOnStartup(curScript);
    }

    return true;
}
//------------------------------------------------------------------------------

void clsScriptManager::StopScript(Script * curScript, const bool &bDisable) {
    if(bDisable == true) {
		curScript->bEnabled = false;

#ifdef _BUILD_GUI
	    for(uint8_t ui8i = 0; ui8i < ui8ScriptCount; ui8i++) {
            if(curScript == ScriptTable[ui8i]) {
                clsMainWindowPageScripts::mPtr->UpdateCheck(ui8i);
                break;
            }
        }
#endif
    }

	RemoveRunningScript(curScript);

    if(clsServerManager::bServerRunning == true) {
        ScriptOnExit(curScript);
    }

	ScriptStop(curScript);
}
//------------------------------------------------------------------------------

void clsScriptManager::MoveScript(const uint8_t &ui8ScriptPosInTbl, const bool &bUp) {
    if(bUp == true) {
		if(ui8ScriptPosInTbl == 0) {
            return;
        }

        Script * cur = ScriptTable[ui8ScriptPosInTbl];
		ScriptTable[ui8ScriptPosInTbl] = ScriptTable[ui8ScriptPosInTbl-1];
		ScriptTable[ui8ScriptPosInTbl-1] = cur;

		// if one of moved scripts not running then return
		if(cur->LUA == NULL || ScriptTable[ui8ScriptPosInTbl]->LUA == NULL) {
			return;
		}

		if(cur->prev == NULL) { // first running script, nothing to move
			return;
		} else if(cur->next == NULL) { // last running script
			// set prev script as last
			RunningScriptE = cur->prev;

			// change prev prev script next
			if(RunningScriptE->prev != NULL) {
				RunningScriptE->prev->next = cur;
			} else {
				RunningScriptS = cur;
			}

			// change cur script prev and next
			cur->prev = RunningScriptE->prev;
			cur->next = RunningScriptE;

			// change prev script prev to cur and his next to NULL
			RunningScriptE->prev = cur;
			RunningScriptE->next = NULL;
		} else { // not first, not last ...
			// remember original prev and next
			Script * prev = cur->prev;
			Script * next = cur->next;

			// change cur script prev
			cur->prev = prev->prev;

			// change prev script next
			prev->next = next;

			// change cur script next
			cur->next = prev;

			// change next script prev
			next->prev = prev;

			// change prev prev script next
			if(prev->prev != NULL) {
				prev->prev->next = cur;
			} else {
				RunningScriptS = cur;
			}

			// change prev script prev
			prev->prev = cur;
		}
    } else {
		if(ui8ScriptPosInTbl == ui8ScriptCount-1) {
            return;
		}

        Script * cur = ScriptTable[ui8ScriptPosInTbl];
		ScriptTable[ui8ScriptPosInTbl] = ScriptTable[ui8ScriptPosInTbl+1];
        ScriptTable[ui8ScriptPosInTbl+1] = cur;

		// if one of moved scripts not running then return
		if(cur->LUA == NULL || ScriptTable[ui8ScriptPosInTbl]->LUA == NULL) {
			return;
		}

        if(cur->next == NULL) { // last running script, nothing to move
            return;
        } else if(cur->prev == NULL) { // first running script
            //set next running script as first
            RunningScriptS = cur->next;

            // change next next script prev
			if(RunningScriptS->next != NULL) {
				RunningScriptS->next->prev = cur;
			} else {
				RunningScriptE = cur;
			}

			// change cur script prev and next
            cur->prev = RunningScriptS;
			cur->next = RunningScriptS->next;

            // change next script next to cur and his prev to NULL
			RunningScriptS->prev = NULL;
			RunningScriptS->next = cur;
		} else { // not first, not last ...
            // remember original prev and next
            Script * prev = cur->prev;
            Script * next = cur->next;

			// change cur script next
			cur->next = next->next;

			// change next script prev
			next->prev = prev;

			// change cur script prev
			cur->prev = next;

			// change prev script next
			prev->next = next;

			// change next next script prev
            if(next->next != NULL) {
                next->next->prev = cur;
			} else {
				RunningScriptE = cur;
			}

			// change next script next
			next->next = cur;
		}
	}
}
//------------------------------------------------------------------------------

void clsScriptManager::DeleteScript(const uint8_t &ui8ScriptPosInTbl) {
    Script * cur = ScriptTable[ui8ScriptPosInTbl];

	if(cur->LUA != NULL) {
		StopScript(cur, false);
	}

	if(FileExist((clsServerManager::sScriptPath+string(cur->sName)).c_str()) == true) {
#ifdef _WIN32
        DeleteFile((clsServerManager::sScriptPath+string(cur->sName)).c_str());
#else
		unlink((clsServerManager::sScriptPath+string(cur->sName)).c_str());
#endif
    }

    delete cur;

	for(uint8_t ui8i = ui8ScriptPosInTbl; ui8i+1 < ui8ScriptCount; ui8i++) {
        ScriptTable[ui8i] = ScriptTable[ui8i+1];
    }

    ScriptTable[ui8ScriptCount-1] = NULL;
    ui8ScriptCount--;
}
//------------------------------------------------------------------------------

void clsScriptManager::OnStartup() {
    if(clsSettingManager::mPtr->bBools[SETBOOL_ENABLE_SCRIPTING] == false) {
        return;
    }

    ActualUser = NULL;
    bMoved = false;

    Script *next = RunningScriptS;
        
    while(next != NULL) {
    	Script * cur = next;
        next = cur->next;

		if(((cur->ui16Functions & Script::ONSTARTUP) == Script::ONSTARTUP) == true && (bMoved == false || cur->bProcessed == false)) {
            cur->bProcessed = true;
            ScriptOnStartup(cur);
        }
	}
}
//------------------------------------------------------------------------------

void clsScriptManager::OnExit(bool bForce/* = false*/) {
    if(bForce == false && clsSettingManager::mPtr->bBools[SETBOOL_ENABLE_SCRIPTING] == false) {
        return;
    }

    ActualUser = NULL;
    bMoved = false;

    Script *next = RunningScriptS;
        
    while(next != NULL) {
    	Script *cur = next;
        next = cur->next;

		if(((cur->ui16Functions & Script::ONEXIT) == Script::ONEXIT) == true && (bMoved == false || cur->bProcessed == false)) {
            cur->bProcessed = true;
            ScriptOnExit(cur);
        }
    }
}
//------------------------------------------------------------------------------

bool clsScriptManager::Arrival(User * u, char * sData, const size_t &szLen, const unsigned char &uiType) {
	if(clsSettingManager::mPtr->bBools[SETBOOL_ENABLE_SCRIPTING] == false) {
		return false;
	}

	static const uint32_t iLuaArrivalBits[] = {
        0x1, 
        0x2, 
        0x4, 
        0x8, 
        0x10, 
        0x20, 
        0x40, 
        0x80, 
        0x100, 
        0x200, 
        0x400, 
        0x800, 
        0x1000, 
        0x2000, 
        0x4000, 
        0x8000, 
        0x10000, 
        0x20000, 
        0x40000, 
        0x80000, 
        0x100000,
		0x400000,
	};

    bMoved = false;

    Script *next = RunningScriptS;
        
    while(next != NULL) {
    	Script *cur = next;
        next = cur->next;

        // if any of the scripts returns a nonzero value,
		// then stop for all other scripts
        if(((cur->ui32DataArrivals & iLuaArrivalBits[uiType]) == iLuaArrivalBits[uiType]) == true && (bMoved == false || cur->bProcessed == false)) {
            cur->bProcessed = true;

            lua_pushcfunction(cur->LUA, ScriptTraceback);
            int iTraceback = lua_gettop(cur->LUA);

            // PPK ... table of arrivals
            static const char* arrival[] = { "ChatArrival", "KeyArrival", "ValidateNickArrival", "PasswordArrival",
            "VersionArrival", "GetNickListArrival", "MyINFOArrival", "GetINFOArrival", "SearchArrival",
            "ToArrival", "ConnectToMeArrival", "MultiConnectToMeArrival", "RevConnectToMeArrival", "SRArrival",
            "UDPSRArrival", "KickArrival", "OpForceMoveArrival", "SupportsArrival", "BotINFOArrival", 
            "CloseArrival", "UnknownArrival","ValidateDenideArrival" };

            lua_getglobal(cur->LUA, arrival[uiType]);
            int i = lua_gettop(cur->LUA);
            if(lua_isnil(cur->LUA, i)) {
                cur->ui32DataArrivals &= ~iLuaArrivalBits[uiType];

                lua_settop(cur->LUA, 0);
                continue;
            }

            ActualUser = u;

            lua_checkstack(cur->LUA, 2); // we need 2 empty slots in stack, check it to be sure

			ScriptPushUser(cur->LUA, u); // usertable
            lua_pushlstring(cur->LUA, sData, szLen); // sData

            // two passed parameters, zero returned
            if(lua_pcall(cur->LUA, 2, LUA_MULTRET, iTraceback) != 0) {
                ScriptError(cur);

                lua_settop(cur->LUA, 0);
                continue;
            }

            ActualUser = NULL;
        
            // check the return value
            // if no return value specified, continue
            // if non-boolean value returned, continue
            // if a boolean true value dwels on the stack, return it

            int top = lua_gettop(cur->LUA);
        
            // no return value
            if(top == 0) {
                continue;
            }

			if(lua_type(cur->LUA, top) != LUA_TBOOLEAN || lua_toboolean(cur->LUA, top) == 0) {
                lua_settop(cur->LUA, 0);
                continue;
            }

            // clear the stack for sure
            lua_settop(cur->LUA, 0);

			return true; // true means DO NOT process data by the hub's core
        }
    }

    return false;
}
//------------------------------------------------------------------------------

bool clsScriptManager::UserConnected(User * u) {
	if(clsSettingManager::mPtr->bBools[SETBOOL_ENABLE_SCRIPTING] == false) {
        return false;
    }

    uint8_t ui8Type = 0; // User
    if(u->iProfile != -1) {
        if(((u->ui32BoolBits & User::BIT_OPERATOR) == User::BIT_OPERATOR) == false) {
            ui8Type = 1; // Reg
		} else {
			ui8Type = 2; // OP
		}
    }

    bMoved = false;

    Script *next = RunningScriptS;
        
    while(next != NULL) {
    	Script *cur = next;
        next = cur->next;

		static const uint32_t iConnectedBits[] = { Script::USERCONNECTED, Script::REGCONNECTED, Script::OPCONNECTED };

		if(((cur->ui16Functions & iConnectedBits[ui8Type]) == iConnectedBits[ui8Type]) == true && (bMoved == false || cur->bProcessed == false)) {
            cur->bProcessed = true;

            lua_pushcfunction(cur->LUA, ScriptTraceback);
            int iTraceback = lua_gettop(cur->LUA);

            // PPK ... table of connected functions
            static const char* ConnectedFunction[] = { "UserConnected", "RegConnected", "OpConnected" };

            lua_getglobal(cur->LUA, ConnectedFunction[ui8Type]);
            int i = lua_gettop(cur->LUA);
			if(lua_isnil(cur->LUA, i)) {
				switch(ui8Type) {
					case 0:
						cur->ui16Functions &= ~Script::USERCONNECTED;
						break;
					case 1:
						cur->ui16Functions &= ~Script::REGCONNECTED;
						break;
					case 2:
						cur->ui16Functions &= ~Script::OPCONNECTED;
						break;
				}

                lua_settop(cur->LUA, 0);
                continue;
            }

            ActualUser = u;

            lua_checkstack(cur->LUA, 1); // we need 1 empty slots in stack, check it to be sure

			ScriptPushUser(cur->LUA, u); // usertable

            // 1 passed parameters, zero returned
			if(lua_pcall(cur->LUA, 1, LUA_MULTRET, iTraceback) != 0) {
                ScriptError(cur);

                lua_settop(cur->LUA, 0);
                continue;
            }

            ActualUser = NULL;
            
            // check the return value
            // if no return value specified, continue
            // if non-boolean value returned, continue
            // if a boolean true value dwels on the stack, return
        
            int top = lua_gettop(cur->LUA);
        
            // no return value
            if(top == 0) {
                continue;
            }
        
			if(lua_type(cur->LUA, top) != LUA_TBOOLEAN || lua_toboolean(cur->LUA, top) == 0) {
                lua_settop(cur->LUA, 0);
                continue;
            }
       
            // clear the stack for sure
            lua_settop(cur->LUA, 0);

            UserDisconnected(u, cur);

            return true; // means DO NOT process by next scripts
        }
    }

    return false;
}
//------------------------------------------------------------------------------

void clsScriptManager::UserDisconnected(User * u, Script * pScript/* = NULL*/) {
	if(clsSettingManager::mPtr->bBools[SETBOOL_ENABLE_SCRIPTING] == false) {
        return;
    }

    uint8_t ui8Type = 0; // User
    if(u->iProfile != -1) {
        if(((u->ui32BoolBits & User::BIT_OPERATOR) == User::BIT_OPERATOR) == false) {
            ui8Type = 1; // Reg
		} else {
			ui8Type = 2; // OP
		}
    }

    bMoved = false;

    Script *next = RunningScriptS;
        
    while(next != NULL) {
    	Script *cur = next;
        next = cur->next;

        if(cur == pScript) {
            return;
        }

		static const uint32_t iDisconnectedBits[] = { Script::USERDISCONNECTED, Script::REGDISCONNECTED, Script::OPDISCONNECTED };

        if(((cur->ui16Functions & iDisconnectedBits[ui8Type]) == iDisconnectedBits[ui8Type]) == true && (bMoved == false || cur->bProcessed == false)) {
            cur->bProcessed = true;

            lua_pushcfunction(cur->LUA, ScriptTraceback);
            int iTraceback = lua_gettop(cur->LUA);

            // PPK ... table of disconnected functions
            static const char* DisconnectedFunction[] = { "UserDisconnected", "RegDisconnected", "OpDisconnected" };

            lua_getglobal(cur->LUA, DisconnectedFunction[ui8Type]);
            int i = lua_gettop(cur->LUA);
            if(lua_isnil(cur->LUA, i)) {
				switch(ui8Type) {
					case 0:
						cur->ui16Functions &= ~Script::USERDISCONNECTED;
						break;
					case 1:
						cur->ui16Functions &= ~Script::REGDISCONNECTED;
						break;
					case 2:
						cur->ui16Functions &= ~Script::OPDISCONNECTED;
						break;
				}

                lua_settop(cur->LUA, 0);
                continue;
            }

            ActualUser = u;

            lua_checkstack(cur->LUA, 1); // we need 1 empty slots in stack, check it to be sure

			ScriptPushUser(cur->LUA, u); // usertable

            // 1 passed parameters, zero returned
			if(lua_pcall(cur->LUA, 1, 0, iTraceback) != 0) {
                ScriptError(cur);

                lua_settop(cur->LUA, 0);
                continue;
            }

            ActualUser = NULL;

            // clear the stack for sure
            lua_settop(cur->LUA, 0);
        }
    }
}
//------------------------------------------------------------------------------

void clsScriptManager::PrepareMove(lua_State * L) {
    if(bMoved == true) {
        return;
    }

    bool bBefore = true;

    bMoved = true;

    Script *next = RunningScriptS;
        
    while(next != NULL) {
    	Script *cur = next;
        next = cur->next;

        if(bBefore == true) {
            cur->bProcessed = true;
        } else {
            cur->bProcessed = false;
        }

        if(cur->LUA == L) {
            bBefore = false;
        }
    }
}
//------------------------------------------------------------------------------
