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

//---------------------------------------------------------------------------
#include "stdinc.h"
//---------------------------------------------------------------------------
#include "DcCommands.h"
//---------------------------------------------------------------------------
#include "colUsers.h"
#include "GlobalDataQueue.h"
#include "hashBanManager.h"
#include "hashRegManager.h"
#include "hashUsrManager.h"
#include "LanguageManager.h"
#include "LuaScriptManager.h"
#include "ProfileManager.h"
#include "ServerManager.h"
#include "SettingManager.h"
#include "UdpDebug.h"
#include "User.h"
#include "utility.h"
#include "ZlibUtility.h"
//---------------------------------------------------------------------------
#ifdef _WIN32
	#pragma hdrstop
#endif
//---------------------------------------------------------------------------
#include "DeFlood.h"
#include "HubCommands.h"
#include "IP2Country.h"
#include "ResNickManager.h"
#include "TextFileManager.h"
//---------------------------------------------------------------------------
#ifdef _BUILD_GUI
	#include "../gui.win/GuiUtil.h"
    #include "../gui.win/MainWindowPageUsersChat.h"
#endif
//---------------------------------------------------------------------------
clsDcCommands * clsDcCommands::mPtr = NULL;
//---------------------------------------------------------------------------

struct PassBf {
	int iCount;
	PassBf *prev, *next;
	uint8_t ui128IpHash[16];

	PassBf(const uint8_t * ui128Hash);
	~PassBf(void) { };
};
//---------------------------------------------------------------------------

PassBf::PassBf(const uint8_t * ui128Hash) {
	memcpy(ui128IpHash, ui128Hash, 16);
    iCount = 1;
    prev = NULL;
    next = NULL;
}
//---------------------------------------------------------------------------

clsDcCommands::clsDcCommands() {
	PasswdBfCheck = NULL;
    iStatChat = iStatCmdUnknown = iStatCmdTo = iStatCmdMyInfo = iStatCmdSearch = iStatCmdSR = iStatCmdRevCTM = 0;
    iStatCmdOpForceMove = iStatCmdMyPass = iStatCmdValidate = iStatCmdKey = iStatCmdGetInfo = iStatCmdGetNickList = 0;
	iStatCmdConnectToMe = iStatCmdVersion = iStatCmdKick = iStatCmdSupports = iStatBotINFO = iStatZPipe = 0;
    iStatCmdMultiSearch = iStatCmdMultiConnectToMe = iStatCmdClose = 0;
    msg[0] = '\0';
}
//---------------------------------------------------------------------------

clsDcCommands::~clsDcCommands() {
	PassBf *next = PasswdBfCheck;

    while(next != NULL) {
        PassBf *cur = next;
		next = cur->next;
		delete cur;
    }

	PasswdBfCheck = NULL;
}
//---------------------------------------------------------------------------

// Process DC data form User
void clsDcCommands::PreProcessData(User * curUser, char * sData, const bool &bCheck, const uint32_t &iLen) {
#ifdef _BUILD_GUI
    // Full raw data trace for better logging
    if(::SendMessage(clsMainWindowPageUsersChat::mPtr->hWndPageItems[clsMainWindowPageUsersChat::BTN_SHOW_COMMANDS], BM_GETCHECK, 0, 0) == BST_CHECKED) {
		int imsglen = sprintf(msg, "%s (%s) > ", curUser->sNick, curUser->sIP);
		if(CheckSprintf(imsglen, 1024, "clsDcCommands::PreProcessData1") == true) {
			RichEditAppendText(clsMainWindowPageUsersChat::mPtr->hWndPageItems[clsMainWindowPageUsersChat::REDT_CHAT], (msg+string(sData, iLen)).c_str());
		}
    }
#endif

    // micro spam
    if(iLen < 5 && bCheck) {
    	#ifdef _DEBUG
    	    int iret = sprintf(msg, ">>> Garbage DATA from %s (%s) -> %s", curUser->sNick, curUser->sIP, sData);
    	    if(CheckSprintf(iret, 1024, "clsDcCommands::PreProcessData2") == true) {
                Memo(msg);
            }
        #endif
        int imsgLen = sprintf(msg, "[SYS] Garbage DATA from %s (%s) -> %s", curUser->sNick, curUser->sIP, sData);
        if(CheckSprintf(imsgLen, 1024, "clsDcCommands::PreProcessData3") == true) {
            clsUdpDebug::mPtr->Broadcast(msg, imsgLen);
        }
        curUser->Close();
        return;
    }

    static const uint32_t ui32ulti = *((uint32_t *)"ulti");
    static const uint32_t ui32ick = *((uint32_t *)"ick ");

    switch(curUser->ui8State) {
        case User::STATE_KEY_OR_SUP: {
            if(sData[0] == '$') {
                if(memcmp(sData+1, "Supports ", 9) == 0) {
                    iStatCmdSupports++;
                    Supports(curUser, sData, iLen);
                    return;
                } else if(*((uint32_t *)(sData+1)) == *((uint32_t *)"Key ")) {
					iStatCmdKey++;
                    Key(curUser, sData, iLen);
                    return;
                } else if(memcmp(sData+1, "MyNick ", 7) == 0) {
                    MyNick(curUser, sData, iLen);
                    return;
                }
            }
            break;
        }
        case User::STATE_VALIDATE: {
            if(sData[0] == '$') {
                switch(sData[1]) {
                    case 'K':
                        if(memcmp(sData+2, "ey ", 3) == 0) {
                            iStatCmdKey++;
                            if(((curUser->ui32BoolBits & User::BIT_HAVE_SUPPORTS) == User::BIT_HAVE_SUPPORTS) == false) {
                                Key(curUser, sData, iLen);
                            } else {
                                curUser->FreeBuffer();
                            }

                            return;
                        }
                        break;
                    case 'V':
                        if(memcmp(sData+2, "alidateNick ", 12) == 0) {
                            iStatCmdValidate++;
                            ValidateNick(curUser, sData, iLen);
                            return;
                        }
                        break;
                    case 'M':
                        if(memcmp(sData+2, "yINFO $ALL ", 11) == 0) {
                            iStatCmdMyInfo++;
                            if(((curUser->ui32SupportBits & User::SUPPORTBIT_QUICKLIST) == User::SUPPORTBIT_QUICKLIST) == false) {
                                // bad state
                                #ifdef _DBG
                                    int iret = sprintf(msg, "%s (%s) bad state in case $MyINFO: %d", curUser->Nick, curUser->IP, curUser->iState);
                                    if(CheckSprintf(iret, 1024, "clsDcCommands::PreProcessData4") == true) {
                                        Memo(msg);
                                    }
                                #endif
                                int imsgLen = sprintf(msg, "[SYS] Bad state (%d) in $MyINFO %s (%s) - user closed.", (int)curUser->ui8State, curUser->sNick, curUser->sIP);
                                if(CheckSprintf(imsgLen, 1024, "clsDcCommands::PreProcessData5") == true) {
                                    clsUdpDebug::mPtr->Broadcast(msg, imsgLen);
                                }
                                curUser->Close();
                                return;
                            }
                            
                            if(MyINFODeflood(curUser, sData, iLen, bCheck) == false) {
                                return;
                            }
                            
                            // PPK [ Strikes back ;) ] ... get nick from MyINFO
                            char *cTemp;
                            if((cTemp = strchr(sData+13, ' ')) == NULL) {
                                if(iLen > 65000) {
                                    sData[65000] = '\0';
                                }

                                int imsgLen = sprintf(clsServerManager::sGlobalBuffer, "[SYS] Attempt to validate empty nick  from %s (%s) - user closed. (QuickList -> %s)", curUser->sNick, curUser->sIP, sData);
                                if(CheckSprintf(imsgLen, clsServerManager::szGlobalBufferSize, "clsDcCommands::PreProcessData6") == true) {
                                    clsUdpDebug::mPtr->Broadcast(clsServerManager::sGlobalBuffer, imsgLen);
                                }

                                curUser->Close();
                                return;
                            }
                            // PPK ... one null please :)
                            cTemp[0] = '\0';
                            
                            if(ValidateUserNick(curUser, sData+13, (cTemp-sData)-13, false) == false) return;
                            
                            cTemp[0] = ' ';

                            // 1st time MyINFO, user is being added to nicklist
                            if(MyINFO(curUser, sData, iLen) == false || (curUser->ui32BoolBits & User::BIT_WAITING_FOR_PASS) == User::BIT_WAITING_FOR_PASS ||
                                ((curUser->ui32BoolBits & User::BIT_PINGER) == User::BIT_PINGER) == true)
                                return;

                            curUser->AddMeOrIPv4Check();

                            return;
                        }
                        break;
                    case 'G':
                        if(iLen == 13 && memcmp(sData+2, "etNickList", 10) == 0) {
                            iStatCmdGetNickList++;
                            if(((curUser->ui32SupportBits & User::SUPPORTBIT_QUICKLIST) == User::SUPPORTBIT_QUICKLIST) == false &&
                                ((curUser->ui32BoolBits & User::BIT_PINGER) == User::BIT_PINGER) == false) {
                                // bad state
                                #ifdef _DBG
                                    int iret = sprintf(msg, "%s (%s) bad state in case $GetNickList: %d", curUser->Nick, curUser->IP, curUser->iState);
                                    if(CheckSprintf(iret, 1024, "clsDcCommands::PreProcessData8") == true) {
                                        Memo(msg);
                                    }
                                #endif
                                int imsgLen = sprintf(msg, "[SYS] Bad state (%d) in $GetNickList %s (%s) - user closed.", (int)curUser->ui8State, curUser->sNick, curUser->sIP);
                                if(CheckSprintf(imsgLen, 1024, "clsDcCommands::PreProcessData9") == true) {
                                    clsUdpDebug::mPtr->Broadcast(msg, imsgLen);
                                }
                                curUser->Close();
                                return;
                            }
                            GetNickList(curUser, sData, iLen, bCheck);
                            return;
                        }
                        break;
                    default:
                        break;
                }                                            
            }
            break;
        }
        case User::STATE_VERSION_OR_MYPASS: {
            if(sData[0] == '$') {
                switch(sData[1]) {
                    case 'V':
                        if(memcmp(sData+2, "ersion ", 7) == 0) {
                            iStatCmdVersion++;
                            Version(curUser, sData, iLen);
                            return;
                        }
                        break;
                    case 'G':
                        if(iLen == 13 && memcmp(sData+2, "etNickList", 10) == 0) {
                            iStatCmdGetNickList++;
                            if(GetNickList(curUser, sData, iLen, bCheck) == true && 
                                ((curUser->ui32SupportBits & User::SUPPORTBIT_QUICKLIST) == User::SUPPORTBIT_QUICKLIST) == false) {
                        	   curUser->ui32BoolBits |= User::BIT_GETNICKLIST;
                            }
                            return;
                        }
                        break;
                    case 'M':
                        if(sData[2] == 'y') {
                            if(memcmp(sData+3, "INFO $ALL ", 10) == 0) {
                                iStatCmdMyInfo++;
                                if(MyINFODeflood(curUser, sData, iLen, bCheck) == false) {
                                    return;
                                }
                                
                                // Am I sending MyINFO of someone other ?
                                // OR i try to fuck up hub with some chars after my nick ??? ... PPK
                                if((sData[13+curUser->ui8NickLen] != ' ') || (memcmp(curUser->sNick, sData+13, curUser->ui8NickLen) != 0)) {
                                    if(iLen > 65000) {
                                        sData[65000] = '\0';
                                    }

                                    int imsgLen = sprintf(clsServerManager::sGlobalBuffer, "[SYS] Nick spoofing in myinfo from %s (%s) - user closed. (%s)", curUser->sNick, curUser->sIP, sData);
                                    if(CheckSprintf(imsgLen, clsServerManager::szGlobalBufferSize, "clsDcCommands::PreProcessData10") == true) {
                                        clsUdpDebug::mPtr->Broadcast(clsServerManager::sGlobalBuffer, imsgLen);
                                    }

                                    curUser->Close();
                                    return;
                                }
        
                                if(MyINFO(curUser, sData, iLen) == false || (curUser->ui32BoolBits & User::BIT_WAITING_FOR_PASS) == User::BIT_WAITING_FOR_PASS ||
                                    ((curUser->ui32BoolBits & User::BIT_PINGER) == User::BIT_PINGER) == true)
                                    return;
                                
                                curUser->AddMeOrIPv4Check();

                                return;
                            } else if(memcmp(sData+3, "Pass ", 5) == 0) {
                                iStatCmdMyPass++;
                                MyPass(curUser, sData, iLen);
                                return;
                            }
                        }
                        break;
                    default:
                        break;
                }
            }
            break;
        }
        case User::STATE_GETNICKLIST_OR_MYINFO: {
            if(sData[0] == '$') {
                if(iLen == 13 && memcmp(sData+1, "GetNickList", 11) == 0) {
                    iStatCmdGetNickList++;
                    if(GetNickList(curUser, sData, iLen, bCheck) == true) {
                        curUser->ui32BoolBits |= User::BIT_GETNICKLIST;
                    }
                    return;
                } else if(memcmp(sData+1, "MyINFO $ALL ", 12) == 0) {
                    iStatCmdMyInfo++;
                    if(MyINFODeflood(curUser, sData, iLen, bCheck) == false) {
                        return;
                    }
                                
                    // Am I sending MyINFO of someone other ?
                    // OR i try to fuck up hub with some chars after my nick ??? ... PPK
                    if((sData[13+curUser->ui8NickLen] != ' ') || (memcmp(curUser->sNick, sData+13, curUser->ui8NickLen) != 0)) {
                        if(iLen > 65000) {
                            sData[65000] = '\0';
                        }

                        int imsgLen = sprintf(clsServerManager::sGlobalBuffer, "[SYS] Nick spoofing in myinfo from %s (%s) - user closed. (%s)", curUser->sNick, curUser->sIP, sData);
                        if(CheckSprintf(imsgLen, clsServerManager::szGlobalBufferSize, "clsDcCommands::PreProcessData12") == true) {
                            clsUdpDebug::mPtr->Broadcast(clsServerManager::sGlobalBuffer, imsgLen);
                        }

                        curUser->Close();
                        return;
                    }
        
                    if(MyINFO(curUser, sData, iLen) == false || (curUser->ui32BoolBits & User::BIT_WAITING_FOR_PASS) == User::BIT_WAITING_FOR_PASS ||
                        ((curUser->ui32BoolBits & User::BIT_PINGER) == User::BIT_PINGER) == true)
                        return;
                    
                    curUser->AddMeOrIPv4Check();

                    return;
                }
            }
            break;
        }
        case User::STATE_IPV4_CHECK:
        case User::STATE_ADDME:
        case User::STATE_ADDME_1LOOP: {
            if(sData[0] == '$') {
                switch(sData[1]) {
                    case 'G':
                        if(iLen == 13 && memcmp(sData+2, "etNickList", 10) == 0) {
                            iStatCmdGetNickList++;
                            if(GetNickList(curUser, sData, iLen, bCheck) == true) {
                                curUser->ui32BoolBits |= User::BIT_GETNICKLIST;
                            }
                            return;
                        }
                        break;
                    case 'M': {
                        if(memcmp(sData+2, "yINFO $ALL ", 11) == 0) {
                            iStatCmdMyInfo++;
                            if(MyINFODeflood(curUser, sData, iLen, bCheck) == false) {
                                return;
                            }

                            // Am I sending MyINFO of someone other ?
                            // OR i try to fuck up hub with some chars after my nick ??? ... PPK
                            if((sData[13+curUser->ui8NickLen] != ' ') || (memcmp(curUser->sNick, sData+13, curUser->ui8NickLen) != 0)) {
                                if(iLen > 65000) {
                                    sData[65000] = '\0';
                                }

                                int imsgLen = sprintf(clsServerManager::sGlobalBuffer, "[SYS] Nick spoofing in myinfo from %s (%s) - user closed. (%s)", curUser->sNick, curUser->sIP, sData);
                                if(CheckSprintf(imsgLen, clsServerManager::szGlobalBufferSize, "clsDcCommands::PreProcessData14") == true) {
                                    clsUdpDebug::mPtr->Broadcast(clsServerManager::sGlobalBuffer, imsgLen);
                                }

                                curUser->Close();
                                return;
                            }

                            MyINFO(curUser, sData, iLen);
                            
                            return;
                        } else if(memcmp(sData+2, "ultiSearch ", 11) == 0) {
                            iStatCmdMultiSearch++;
                            SearchDeflood(curUser, sData, iLen, bCheck, true);
                            return;
                        }
                        break;
                    }
                    case 'S':
                        if(memcmp(sData+2, "earch ", 6) == 0) {
                            iStatCmdSearch++;
                            SearchDeflood(curUser, sData, iLen, bCheck, false);
                            return;
                        }
                        break;
                    default:
                        break;
                }
            } else if(sData[0] == '<') {
                iStatChat++;
                ChatDeflood(curUser, sData, iLen, bCheck);
                return;
            }
            break;
        }
        case User::STATE_ADDED: {
            if(sData[0] == '$') {
                switch(sData[1]) {
                    case 'S': {
                        if(memcmp(sData+2, "earch ", 6) == 0) {
                            iStatCmdSearch++;
                            if(SearchDeflood(curUser, sData, iLen, bCheck, false) == true) {
                                Search(curUser, sData, iLen, bCheck, false);
                            }
                            return;
                        } else if(*((uint16_t *)(sData+2)) == *((uint16_t *)"R ")) {
                            iStatCmdSR++;
                            SR(curUser, sData, iLen, bCheck);
                            return;
                        }
                        break;
                    }
                    case 'C':
                        if(memcmp(sData+2, "onnectToMe ", 11) == 0) {
                            iStatCmdConnectToMe++;
                            ConnectToMe(curUser, sData, iLen, bCheck, false);
                            return;
                        } else if(memcmp(sData+2, "lose ", 5) == 0) {
                            iStatCmdClose++;
                            Close(curUser, sData, iLen);
                            return;
                        }
                        break;
                    case 'R':
                        if(memcmp(sData+2, "evConnectToMe ", 14) == 0) {
                            iStatCmdRevCTM++;
                            RevConnectToMe(curUser, sData, iLen, bCheck);
                            return;
                        }
                        break;
                    case 'M':
                        if(memcmp(sData+2, "yINFO $ALL ", 11) == 0) {
                            iStatCmdMyInfo++;
                            if(MyINFODeflood(curUser, sData, iLen, bCheck) == false) {
                                return;
                            }
                                    
                            // Am I sending MyINFO of someone other ?
                            // OR i try to fuck up hub with some chars after my nick ??? ... PPK
                            if((sData[13+curUser->ui8NickLen] != ' ') || (memcmp(curUser->sNick, sData+13, curUser->ui8NickLen) != 0)) {
                                if(iLen > 65000) {
                                    sData[65000] = '\0';
                                }

                                int imsgLen = sprintf(clsServerManager::sGlobalBuffer, "[SYS] Nick spoofing in myinfo from %s (%s) - user closed. (%s)", curUser->sNick, curUser->sIP, sData);
                                if(CheckSprintf(imsgLen, clsServerManager::szGlobalBufferSize, "clsDcCommands::PreProcessData16") == true) {
                                    clsUdpDebug::mPtr->Broadcast(clsServerManager::sGlobalBuffer, imsgLen);
                                }

                                curUser->Close();
                                return;
                            }
                                                  
                            if(MyINFO(curUser, sData, iLen) == true) {
                                curUser->ui32BoolBits |= User::BIT_PRCSD_MYINFO;
                            }
                            return;
                        } else if(*((uint32_t *)(sData+2)) == ui32ulti) {
                            if(memcmp(sData+6, "Search ", 7) == 0) {
                                iStatCmdMultiSearch++;
                                if(SearchDeflood(curUser, sData, iLen, bCheck, true) == true) {
                                    Search(curUser, sData, iLen, bCheck, true);
                                }
                                return;
                            } else if(memcmp(sData+6, "ConnectToMe ", 12) == 0) {
                                iStatCmdMultiConnectToMe++;
                                ConnectToMe(curUser, sData, iLen, bCheck, true);
                                return;
                            }
                        } else if(memcmp(sData+2, "yPass ", 6) == 0) {
                            iStatCmdMyPass++;
                            //MyPass(curUser, sData, iLen);
                            if((curUser->ui32BoolBits & User::BIT_WAITING_FOR_PASS) == User::BIT_WAITING_FOR_PASS) {
                                curUser->ui32BoolBits &= ~User::BIT_WAITING_FOR_PASS;

                                if(curUser->uLogInOut != NULL && curUser->uLogInOut->pBuffer != NULL) {
                                    int iProfile = clsProfileManager::mPtr->GetProfileIndex(curUser->uLogInOut->pBuffer);
                                    if(iProfile == -1) {
               	                        int iMsgLen = sprintf(msg, "<%s> %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_ERR_NO_PROFILE_GIVEN_NAME_EXIST]);
                                        if(CheckSprintf(iMsgLen, 1024, "clsDcCommands::PreProcessData::MyPass->RegUser") == true) {
                                            curUser->SendCharDelayed(msg, iMsgLen);
                                        }

                                        delete curUser->uLogInOut;
                                        curUser->uLogInOut = NULL;

                                        return;
                                    }
                                    
                                    if(iLen > 73) {
                                        int iMsgLen = sprintf(msg, "<%s> %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_MAX_ALWD_PASS_LEN_64_CHARS]);
                                        if(CheckSprintf(iMsgLen, 1024, "clsDcCommands::PreProcessData::MyPass->RegUser1") == true) {
                                            curUser->SendCharDelayed(msg, iMsgLen);
                                        }

                                        delete curUser->uLogInOut;
                                        curUser->uLogInOut = NULL;

                                        return;
                                    }

                                    sData[iLen-1] = '\0'; // cutoff pipe

                                    if(clsRegManager::mPtr->AddNew(curUser->sNick, sData+8, (uint16_t)iProfile) == false) {
                                        int iMsgLen = sprintf(msg, "<%s> %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                                            clsLanguageManager::mPtr->sTexts[LAN_SORRY_YOU_ARE_ALREADY_REGISTERED]);
                                        if(CheckSprintf(iMsgLen, 1024, "clsDcCommands::PreProcessData::MyPass->RegUser2") == true) {
                                            curUser->SendCharDelayed(msg, iMsgLen);
                                        }
                                    } else {
                                        int iMsgLen = sprintf(msg, "<%s> %s %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                                            clsLanguageManager::mPtr->sTexts[LAN_THANK_YOU_FOR_PASSWORD_YOU_ARE_NOW_REGISTERED_AS], curUser->uLogInOut->pBuffer);
                                        if(CheckSprintf(iMsgLen, 1024, "clsDcCommands::PreProcessData::MyPass->RegUser2") == true) {
                                            curUser->SendCharDelayed(msg, iMsgLen);
                                        }
                                    }

                                    delete curUser->uLogInOut;
                                    curUser->uLogInOut = NULL;

                                    curUser->iProfile = iProfile;

                                    if(((curUser->ui32BoolBits & User::BIT_OPERATOR) == User::BIT_OPERATOR) == false) {
                                        if(clsProfileManager::mPtr->IsAllowed(curUser, clsProfileManager::HASKEYICON) == false) {
                                            return;
                                        }
                                        
                                        curUser->ui32BoolBits |= User::BIT_OPERATOR;

                                        clsUsers::mPtr->Add2OpList(curUser);
                                        clsGlobalDataQueue::mPtr->OpListStore(curUser->sNick);

                                        if(clsProfileManager::mPtr->IsAllowed(curUser, clsProfileManager::ALLOWEDOPCHAT) == true) {
                                            if(clsSettingManager::mPtr->bBools[SETBOOL_REG_OP_CHAT] == true &&
                                                (clsSettingManager::mPtr->bBools[SETBOOL_REG_BOT] == false || clsSettingManager::mPtr->bBotsSameNick == false)) {
                                                if(((curUser->ui32SupportBits & User::SUPPORTBIT_NOHELLO) == User::SUPPORTBIT_NOHELLO) == false) {
                                                    curUser->SendCharDelayed(clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_OP_CHAT_HELLO],
                                                        clsSettingManager::mPtr->ui16PreTextsLens[clsSettingManager::SETPRETXT_OP_CHAT_HELLO]);
                                                }

                                                curUser->SendCharDelayed(clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_OP_CHAT_MYINFO],
                                                    clsSettingManager::mPtr->ui16PreTextsLens[clsSettingManager::SETPRETXT_OP_CHAT_MYINFO]);
                                                int imsgLen = sprintf(msg, "$OpList %s$$|", clsSettingManager::mPtr->sTexts[SETTXT_OP_CHAT_NICK]);
                                                if(CheckSprintf(imsgLen, 1024, "clsDcCommands::PreProcessData::MyPass->RegUser3") == true) {
                                                    curUser->SendCharDelayed(msg, imsgLen);
                                                }
                                            }
                                        }
                                    }
                                }

                                return;
                            }
                        }
                        break;
                    case 'G': {
                        if(iLen == 13 && memcmp(sData+2, "etNickList", 10) == 0) {
                            iStatCmdGetNickList++;
                            if(GetNickList(curUser, sData, iLen, bCheck) == true) {
                                curUser->ui32BoolBits |= User::BIT_GETNICKLIST;
                            }
                            return;
                        } else if(memcmp(sData+2, "etINFO ", 7) == 0) {
							iStatCmdGetInfo++;
                            GetINFO(curUser, sData, iLen);
                            return;
                        }
                        break;
                    }
                    case 'T':
                        if(memcmp(sData+2, "o: ", 3) == 0) {
                            iStatCmdTo++;
                            To(curUser, sData, iLen, bCheck);
                            return;
                        }
                    case 'K':
                        if(*((uint32_t *)(sData+2)) == ui32ick) {
							iStatCmdKick++;
                            Kick(curUser, sData, iLen);
                            return;
                        }
                        break;
                    case 'O':
                        if(memcmp(sData+2, "pForceMove $Who:", 16) == 0) {
                            iStatCmdOpForceMove++;
                            OpForceMove(curUser, sData, iLen);
                            return;
                        }
                    default:
                        break;
                }
            } else if(sData[0] == '<') {
                iStatChat++;
                if(ChatDeflood(curUser, sData, iLen, bCheck) == true) {
                    Chat(curUser, sData, iLen, bCheck);
                }
                
                return;
            }
            break;
        }
        case User::STATE_CLOSING:
        case User::STATE_REMME:
            return;
    }
    
    // PPK ... fallback to full command identification and disconnect on bad state or unknown command not handled by script
    switch(sData[0]) {
        case '$':
            switch(sData[1]) {
                case 'B': {
					if(memcmp(sData+2, "otINFO", 6) == 0) {
						iStatBotINFO++;
                        BotINFO(curUser, sData, iLen);
                        return;
                    }
                    break;
                }
                case 'C':
                    if(memcmp(sData+2, "onnectToMe ", 11) == 0) {
                        iStatCmdConnectToMe++;
                        #ifdef _DBG
                            int iret sprintf(msg, "%s (%s) bad state in case $ConnectToMe: %d", curUser->Nick, curUser->IP, curUser->iState);
                            if(CheckSprintf(iret, 1024, "clsDcCommands::PreProcessData18") == true) {
                                Memo(msg);
                            }
                        #endif
                        int imsgLen = sprintf(msg, "[SYS] Bad state (%d) in $ConnectToMe %s (%s) - user closed.", (int)curUser->ui8State, curUser->sNick, curUser->sIP);
                        if(CheckSprintf(imsgLen, 1024, "clsDcCommands::PreProcessData19") == true) {
                            clsUdpDebug::mPtr->Broadcast(msg, imsgLen);
                        }
                        curUser->Close();
                        return;
                    } else if(memcmp(sData+2, "lose ", 5) == 0) {
                        iStatCmdClose++;
                        #ifdef _DBG
                            int iret = sprintf(msg, "%s (%s) bad state in case $Close: %d", curUser->Nick, curUser->IP, curUser->iState);
                            if(CheckSprintf(iret, 1024, "clsDcCommands::PreProcessData20") == true) {
                                Memo(msg);
                            }
                        #endif
                        int imsgLen = sprintf(msg, "[SYS] Bad state (%d) in $Close %s (%s) - user closed.", (int)curUser->ui8State, curUser->sNick, curUser->sIP);
                        if(CheckSprintf(imsgLen, 1024, "clsDcCommands::PreProcessData21") == true) {
                            clsUdpDebug::mPtr->Broadcast(msg, imsgLen);
                        }
                        curUser->Close();
                        return;
                    }
                    break;
                case 'G': {
                    if(*((uint16_t *)(sData+2)) == *((uint16_t *)"et")) {
                        if(memcmp(sData+4, "INFO ", 5) == 0) {
                            iStatCmdGetInfo++;
                            #ifdef _DBG
                                int iret = sprintf(msg, "%s (%s) bad state in case $GetINFO: %d", curUser->Nick, curUser->IP, curUser->iState);
                                if(CheckSprintf(iret, 1024, "clsDcCommands::PreProcessData22") == true) {
                                    Memo(msg);
                                }
                            #endif
                            int imsgLen = sprintf(msg, "[SYS] Bad state (%d) in $GetINFO %s (%s) - user closed.", (int)curUser->ui8State, curUser->sNick, curUser->sIP);
                            if(CheckSprintf(imsgLen, 1024, "clsDcCommands::PreProcessData23") == true) {
                                clsUdpDebug::mPtr->Broadcast(msg, imsgLen);
                            }
                            curUser->Close();
                            return;
                        } else if(iLen == 13 && *((uint64_t *)(sData+4)) == *((uint64_t *)"NickList")) {
                            iStatCmdGetNickList++;
                            #ifdef _DBG
                                int iret = sprintf(msg, "%s (%s) bad state in case $GetNickList: %d", curUser->Nick, curUser->IP, curUser->iState);
                                if(CheckSprintf(iret, 1024, "clsDcCommands::PreProcessData24") == true) {
                                    Memo(msg);
                                }
                            #endif
                            int imsgLen = sprintf(msg, "[SYS] Bad state (%d) in $GetNickList %s (%s) - user closed.", (int)curUser->ui8State, curUser->sNick, curUser->sIP);
                            if(CheckSprintf(imsgLen, 1024, "clsDcCommands::PreProcessData25") == true) {
                                clsUdpDebug::mPtr->Broadcast(msg, imsgLen);
                            }
                            curUser->Close();
                            return;
                        }
                    }
                    break;
                }
                case 'K':
                    if(memcmp(sData+2, "ey ", 3) == 0) {
                        iStatCmdKey++;
                        #ifdef _DBG
                            int iret = sprintf(msg, "%s (%s) bad state in case $Key: %d", curUser->Nick, curUser->IP, curUser->iState);
                            if(CheckSprintf(iret, 1024, "clsDcCommands::PreProcessData26") == true) {
                                Memo(msg);
                            }
                        #endif
                        int imsgLen = sprintf(msg, "[SYS] Bad state (%d) in $Key %s (%s) - user closed.", (int)curUser->ui8State, curUser->sNick, curUser->sIP);
                        if(CheckSprintf(imsgLen, 1024, "clsDcCommands::PreProcessData27") == true) {
                            clsUdpDebug::mPtr->Broadcast(msg, imsgLen);
                        }
                        curUser->Close();
                        return;
                    } else if(*((uint32_t *)(sData+2)) == ui32ick) {
                        iStatCmdKick++;
                        #ifdef _DBG
                            int iret = sprintf(msg, "%s (%s) bad state in case $Kick: %d", curUser->Nick, curUser->IP, curUser->iState);
                            if(CheckSprintf(iret, 1024, "clsDcCommands::PreProcessData28") == true) {
                                Memo(msg);
                            }
                        #endif
                        int imsgLen = sprintf(msg, "[SYS] Bad state (%d) in $Kick %s (%s) - user closed.", (int)curUser->ui8State, curUser->sNick, curUser->sIP);
                        if(CheckSprintf(imsgLen, 1024, "clsDcCommands::PreProcessData29") == true) {
                            clsUdpDebug::mPtr->Broadcast(msg, imsgLen);
                        }
                        curUser->Close();
                        return;
                    }
                    break;
                case 'M':
                    if(*((uint32_t *)(sData+2)) == ui32ulti) {
                        if(memcmp(sData+6, "ConnectToMe ", 12) == 0) {
                            iStatCmdMultiConnectToMe++;
                            #ifdef _DBG
                                int iret = sprintf(msg, "%s (%s) bad state in case $MultiConnectToMe: %d", curUser->Nick, curUser->IP, curUser->iState);
                                if(CheckSprintf(iret, 1024, "clsDcCommands::PreProcessData30") == true) {
                                    Memo(msg);
                                }
                            #endif
                            int imsgLen = sprintf(msg, "[SYS] Bad state (%d) in $MultiConnectToMe %s (%s) - user closed.", (int)curUser->ui8State, curUser->sNick, curUser->sIP);
                            if(CheckSprintf(imsgLen, 1024, "clsDcCommands::PreProcessData31") == true) {
                                clsUdpDebug::mPtr->Broadcast(msg, imsgLen);
                            }
                            curUser->Close();
                            return;
                        } else if(memcmp(sData+6, "Search ", 7) == 0) {
                            iStatCmdMultiSearch++;
                            #ifdef _DBG
                                int iret = sprintf(msg, "%s (%s) bad state in case $MultiSearch: %d", curUser->Nick, curUser->IP, curUser->iState);
                                if(CheckSprintf(iret, 1024, "clsDcCommands::PreProcessData32") == true) {
                                    Memo(msg);
                                }
                            #endif
                            int imsgLen = sprintf(msg, "[SYS] Bad state (%d) in $MultiSearch %s (%s) - user closed.", (int)curUser->ui8State, curUser->sNick, curUser->sIP);
                            if(CheckSprintf(imsgLen, 1024, "clsDcCommands::PreProcessData33") == true) {
                                clsUdpDebug::mPtr->Broadcast(msg, imsgLen);
                            }
                            curUser->Close();
                            return;
                        }
                    } else if(sData[2] == 'y') {
                        if(memcmp(sData+3, "INFO $ALL ", 10) == 0) {
                            iStatCmdMyInfo++;
                            #ifdef _DBG
                                int iret sprintf(msg, "%s (%s) bad state in case $MyINFO: %d", curUser->Nick, curUser->IP, curUser->iState);
                                if(CheckSprintf(iret, 1024, "clsDcCommands::PreProcessData34") == true) {
                                    Memo(msg);
                                }
                            #endif
                            int imsgLen = sprintf(msg, "[SYS] Bad state (%d) in $MyINFO %s (%s) - user closed.", (int)curUser->ui8State, curUser->sNick, curUser->sIP);
                            if(CheckSprintf(imsgLen, 1024, "clsDcCommands::PreProcessData35") == true) {
                                clsUdpDebug::mPtr->Broadcast(msg, imsgLen);
                            }
                            curUser->Close();
                            return;
                        } else if(memcmp(sData+3, "Pass ", 5) == 0) {
                            iStatCmdMyPass++;
                            #ifdef _DBG
                                int iret = sprintf(msg, "%s (%s) bad state in case $MyPass: %d", curUser->Nick, curUser->IP, curUser->iState);
                                if(CheckSprintf(iret, 1024, "clsDcCommands::PreProcessData36") == true) {
                                    Memo(msg);
                                }
                            #endif
                            int imsgLen = sprintf(msg, "[SYS] Bad state (%d) in $MyPass %s (%s) - user closed.", (int)curUser->ui8State, curUser->sNick, curUser->sIP);
                            if(CheckSprintf(imsgLen, 1024, "clsDcCommands::PreProcessData37") == true) {
                                clsUdpDebug::mPtr->Broadcast(msg, imsgLen);
                            }
                            curUser->Close();
                            return;
                        }
                    }
                    break;
                case 'O':
                    if(memcmp(sData+2, "pForceMove $Who:", 16) == 0) {
                        iStatCmdOpForceMove++;
                        #ifdef _DBG
                            int iret = sprintf(msg, "%s (%s) bad state in case $OpForceMove: %d", curUser->Nick, curUser->IP, curUser->iState);
                            if(CheckSprintf(iret, 1024, "clsDcCommands::PreProcessData38") == true) {
                                Memo(msg);
                            }
                        #endif
                        int imsgLen = sprintf(msg, "[SYS] Bad state (%d) in $OpForceMove %s (%s) - user closed.", (int)curUser->ui8State, curUser->sNick, curUser->sIP);
                        if(CheckSprintf(imsgLen, 1024, "clsDcCommands::PreProcessData39") == true) {
                            clsUdpDebug::mPtr->Broadcast(msg, imsgLen);
                        }
                        curUser->Close();
                        return;
                    }
                    break;
                case 'R':
                    if(memcmp(sData+2, "evConnectToMe ", 14) == 0) {
                        iStatCmdRevCTM++;
                        #ifdef _DBG
                            int iret = sprintf(msg, "%s (%s) bad state in case $RevConnectToMe: %d", curUser->Nick, curUser->IP, curUser->iState);
                            if(CheckSprintf(iret, 1024, "clsDcCommands::PreProcessData40") == true) {
                                Memo(msg);
                            }
                        #endif
                        int imsgLen = sprintf(msg, "[SYS] Bad state (%d) in $RevConnectToMe %s (%s) - user closed.", (int)curUser->ui8State, curUser->sNick, curUser->sIP);
                        if(CheckSprintf(imsgLen, 1024, "clsDcCommands::PreProcessData41") == true) {
                            clsUdpDebug::mPtr->Broadcast(msg, imsgLen);
                        }
						curUser->Close();
                        return;
                    }
                    break;
                case 'S':
                    switch(sData[2]) {
                        case 'e': {
                            if(memcmp(sData+3, "arch ", 5) == 0) {
                                iStatCmdSearch++;
                                #ifdef _DBG
                                    int iret = sprintf(msg, "%s (%s) bad state in case $Search: %d", curUser->Nick, curUser->IP, curUser->iState);
                                    if(CheckSprintf(iret, 1024, "clsDcCommands::PreProcessData42") == true) {
                                        Memo(msg);
                                    }
                                #endif
                                int imsgLen = sprintf(msg, "[SYS] Bad state (%d) in $Search %s (%s) - user closed.", (int)curUser->ui8State, curUser->sNick, curUser->sIP);
                                if(CheckSprintf(imsgLen, 1024, "clsDcCommands::PreProcessData43") == true) {
                                    clsUdpDebug::mPtr->Broadcast(msg, imsgLen);
                                }
                                curUser->Close();
                                return;
                            }
                            break;
                        }
                        case 'R':
                            if(sData[3] == ' ') {
                                iStatCmdSR++;
                                #ifdef _DBG
                                    int iret sprintf(msg, "%s (%s) bad state in case $SR: %d", curUser->Nick, curUser->IP, curUser->iState);
                                    if(CheckSprintf(iret, 1024, "clsDcCommands::PreProcessData44") == true) {
                                        Memo(msg);
                                    }
                                #endif
                                int imsgLen = sprintf(msg, "[SYS] Bad state (%d) in $SR %s (%s) - user closed.", (int)curUser->ui8State, curUser->sNick, curUser->sIP);
                                if(CheckSprintf(imsgLen, 1024, "clsDcCommands::PreProcessData45") == true) {
                                    clsUdpDebug::mPtr->Broadcast(msg, imsgLen);
                                }
                                curUser->Close();
                                return;
                            }
                            break;
                        case 'u': {
                            if(memcmp(sData+3, "pports ", 7) == 0) {
                                iStatCmdSupports++;
                                #ifdef _DBG
                                    int iret = sprintf(msg, "%s (%s) bad state in case $Supports: %d", curUser->Nick, curUser->IP, curUser->iState);
                                    if(CheckSprintf(iret, 1024, "clsDcCommands::PreProcessData46") == true) {
                                        Memo(msg);
                                    }
                                #endif
                                int imsgLen = sprintf(msg, "[SYS] Bad state (%d) in $Supports %s (%s) - user closed.", (int)curUser->ui8State, curUser->sNick, curUser->sIP);
                                if(CheckSprintf(imsgLen, 1024, "clsDcCommands::PreProcessData47") == true) {
                                    clsUdpDebug::mPtr->Broadcast(msg, imsgLen);
                                }
                                curUser->Close();
                                return;
                            }
                            break;
                        }
                        default:
                            break;
                    }
                    break;
                case 'T':
                    if(memcmp(sData+2, "o: ", 3) == 0) {
                        iStatCmdTo++;
                        #ifdef _DBG
                            int iret sprintf(msg, "%s (%s) bad state in case $To: %d", curUser->Nick, curUser->IP, curUser->iState);
                            if(CheckSprintf(iret, 1024, "clsDcCommands::PreProcessData48") == true) {
                                Memo(msg);
                            }
                        #endif
                        int imsgLen = sprintf(msg, "[SYS] Bad state (%d) in $To %s (%s) - user closed.", (int)curUser->ui8State, curUser->sNick, curUser->sIP);
                        if(CheckSprintf(imsgLen, 1024, "clsDcCommands::PreProcessData49") == true) {
                            clsUdpDebug::mPtr->Broadcast(msg, imsgLen);
                        }
                        curUser->Close();
                        return;
                    }
                    break;
                case 'V':
                    if(memcmp(sData+2, "alidateNick ", 12) == 0) {
                        iStatCmdValidate++;
                        #ifdef _DBG
                            int iret = sprintf(msg, "%s (%s) bad state in case $ValidateNick: %d", curUser->Nick, curUser->IP, curUser->iState);
                            if(CheckSprintf(iret, 1024, "clsDcCommands::PreProcessData50") == true) {
                                Memo(msg);
                            }
                        #endif
                        int imsgLen = sprintf(msg, "[SYS] Bad state (%d) in $ValidateNick %s (%s) - user closed.", (int)curUser->ui8State, curUser->sNick, curUser->sIP);
                        if(CheckSprintf(imsgLen, 1024, "clsDcCommands::PreProcessData51") == true) {
                            clsUdpDebug::mPtr->Broadcast(msg, imsgLen);
                        }
                        curUser->Close();
                        return;
                    } else if(memcmp(sData+2, "ersion ", 7) == 0) {
                        iStatCmdVersion++;
                        #ifdef _DBG
                            int iret = sprintf(msg, "%s (%s) bad state in case $Version: %d", curUser->Nick, curUser->IP, curUser->iState);
                            if(CheckSprintf(iret, 1024, "clsDcCommands::PreProcessData52") == true) {
                                Memo(msg);
                            }
                        #endif
                        int imsgLen = sprintf(msg, "[SYS] Bad state (%d) in $Version %s (%s) - user closed.", (int)curUser->ui8State, curUser->sNick, curUser->sIP);
                        if(CheckSprintf(imsgLen, 1024, "clsDcCommands::PreProcessData53") == true) {
                            clsUdpDebug::mPtr->Broadcast(msg, imsgLen);
                        }
                        curUser->Close();
                        return;
                    }
                    break;
                default:
                    break;
            }
            break;
        case '<': {
            iStatChat++;
            #ifdef _DBG
                int iret = sprintf(msg, "%s (%s) bad state in case Chat: %d", curUser->Nick, curUser->IP, curUser->iState);
                if(CheckSprintf(iret, 1024, "clsDcCommands::PreProcessData54") == true) {
                    Memo(msg);
                }
            #endif
            int imsgLen = sprintf(msg, "[SYS] Bad state (%d) in Chat %s (%s) - user closed.", (int)curUser->ui8State, curUser->sNick, curUser->sIP);
            if(CheckSprintf(imsgLen, 1024, "clsDcCommands::PreProcessData55") == true) {
                clsUdpDebug::mPtr->Broadcast(msg, imsgLen);
            }
            curUser->Close();
            return;
        }
        default:
            break;
    }


    Unknown(curUser, sData, iLen);
}
//---------------------------------------------------------------------------

// $BotINFO pinger identification|
void clsDcCommands::BotINFO(User * curUser, char * sData, const uint32_t &iLen) {
	if(((curUser->ui32BoolBits & User::BIT_PINGER) == User::BIT_PINGER) == false || ((curUser->ui32BoolBits & User::BIT_HAVE_BOTINFO) == User::BIT_HAVE_BOTINFO) == true) {
        #ifdef _DBG
            int iret = sprintf(msg, "%s (%s) send $BotINFO and not detected as pinger.", curUser->Nick, curUser->IP);
            if(CheckSprintf(iret, 1024, "clsDcCommands::BotINFO1") == true) {
                Memo(msg);
            }
        #endif
        int imsgLen = sprintf(msg, "[SYS] Not pinger $BotINFO or $BotINFO flood from %s (%s)", curUser->sNick, curUser->sIP);
        if(CheckSprintf(imsgLen, 1024, "clsDcCommands::BotINFO2") == true) {
            clsUdpDebug::mPtr->Broadcast(msg, imsgLen);
        }
        curUser->Close();
        return;
    }

    if(iLen < 9) {
        int imsgLen = sprintf(msg, "[SYS] Bad $BotINFO (%s) from %s (%s) - user closed.", sData, curUser->sNick, curUser->sIP);
        if(CheckSprintf(imsgLen, 1024, "clsDcCommands::BotINFO3") == true) {
            clsUdpDebug::mPtr->Broadcast(msg, imsgLen);
        }
        curUser->Close();
        return;
    }

    curUser->ui32BoolBits |= User::BIT_HAVE_BOTINFO;

	if(clsScriptManager::mPtr->Arrival(curUser, sData, iLen, clsScriptManager::BOTINFO_ARRIVAL) == true ||
		curUser->ui8State >= User::STATE_CLOSING) {
		return;
	}

	int imsgLen = sprintf(msg, "$HubINFO %s$%s:%hu$%s.px.$%hd$%" PRIu64 "$%hd$%hd$PtokaX$%s|", clsSettingManager::mPtr->sTexts[SETTXT_HUB_NAME], clsSettingManager::mPtr->sTexts[SETTXT_HUB_ADDRESS],
        clsSettingManager::mPtr->iPortNumbers[0], clsSettingManager::mPtr->sTexts[SETTXT_HUB_DESCRIPTION] == NULL ? "" : clsSettingManager::mPtr->sTexts[SETTXT_HUB_DESCRIPTION],
        clsSettingManager::mPtr->iShorts[SETSHORT_MAX_USERS], clsSettingManager::mPtr->ui64MinShare, clsSettingManager::mPtr->iShorts[SETSHORT_MIN_SLOTS_LIMIT], clsSettingManager::mPtr->iShorts[SETSHORT_MAX_HUBS_LIMIT],
		clsSettingManager::mPtr->sTexts[SETTXT_HUB_OWNER_EMAIL] == NULL ? "" : clsSettingManager::mPtr->sTexts[SETTXT_HUB_OWNER_EMAIL]);
	if(CheckSprintf(imsgLen, 1024, "clsDcCommands::BotINFO4") == true) {
        curUser->SendCharDelayed(msg, imsgLen);
    }

	if(((curUser->ui32BoolBits & User::BIT_HAVE_GETNICKLIST) == User::BIT_HAVE_GETNICKLIST) == true) {
		curUser->Close();
    }
}
//---------------------------------------------------------------------------

// $ConnectToMe <nickname> <ownip>:<ownlistenport>
// $MultiConnectToMe <nick> <ownip:port> <hub[:port]>
void clsDcCommands::ConnectToMe(User * curUser, char * sData, const uint32_t &iLen, const bool &bCheck, const bool &bMulti) {
    if((bMulti == false && iLen < 23) || (bMulti == true && iLen < 28)) {
        int imsgLen = sprintf(msg, "[SYS] Bad $%sConnectToMe (%s) from %s (%s) - user closed.", bMulti == false ? "" : "Multi", sData, curUser->sNick, curUser->sIP);
        if(CheckSprintf(imsgLen, 1024, "clsDcCommands::ConnectToMe1") == true) {
            clsUdpDebug::mPtr->Broadcast(msg, imsgLen);
        }
        curUser->Close();
        return;
    }

    // PPK ... check flood ...
    if(bCheck == true && clsProfileManager::mPtr->IsAllowed(curUser, clsProfileManager::NODEFLOODCTM) == false) { 
        if(clsSettingManager::mPtr->iShorts[SETSHORT_CTM_ACTION] != 0) {
            if(DeFloodCheckForFlood(curUser, DEFLOOD_CTM, clsSettingManager::mPtr->iShorts[SETSHORT_CTM_ACTION], 
              curUser->ui16CTMs, curUser->ui64CTMsTick, clsSettingManager::mPtr->iShorts[SETSHORT_CTM_MESSAGES], 
              (uint32_t)clsSettingManager::mPtr->iShorts[SETSHORT_CTM_TIME]) == true) {
				return;
            }
        }

        if(clsSettingManager::mPtr->iShorts[SETSHORT_CTM_ACTION2] != 0) {
            if(DeFloodCheckForFlood(curUser, DEFLOOD_CTM, clsSettingManager::mPtr->iShorts[SETSHORT_CTM_ACTION2], 
              curUser->ui16CTMs2, curUser->ui64CTMsTick2, clsSettingManager::mPtr->iShorts[SETSHORT_CTM_MESSAGES2], 
			  (uint32_t)clsSettingManager::mPtr->iShorts[SETSHORT_CTM_TIME2]) == true) {
                return;
            }
        }
    }

    if(iLen > (uint32_t)clsSettingManager::mPtr->iShorts[SETSHORT_MAX_CTM_LEN]) {
        int imsgLen = sprintf(msg, "<%s> %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_CTM_TOO_LONG]);
        if(CheckSprintf(imsgLen, 1024, "clsDcCommands::ConnectToMe2") == true) {
            curUser->SendCharDelayed(msg, imsgLen);
        }

        if(iLen > 65000) {
            sData[65000] = '\0';
        }

        imsgLen = sprintf(clsServerManager::sGlobalBuffer, "[SYS] Long $ConnectToMe from %s (%s) - user closed. (%s)", curUser->sNick, curUser->sIP, sData);
        if(CheckSprintf(imsgLen, clsServerManager::szGlobalBufferSize, "clsDcCommands::ConnectToMe3") == true) {
            clsUdpDebug::mPtr->Broadcast(clsServerManager::sGlobalBuffer, imsgLen);
        }

        curUser->Close();
		return;
    }

    // PPK ... $CTM means user is active ?!? Probably yes, let set it active and use on another places ;)
    if(curUser->sTag == NULL) {
        curUser->ui32BoolBits |= User::BIT_IPV4_ACTIVE;
    }

    // full data only and allow blocking
	if(clsScriptManager::mPtr->Arrival(curUser, sData, iLen, (uint8_t)(bMulti == false ? clsScriptManager::CONNECTTOME_ARRIVAL : clsScriptManager::MULTICONNECTTOME_ARRIVAL)) == true ||
		curUser->ui8State >= User::STATE_CLOSING) {
		return;
	}

    char *towho = strchr(sData+(bMulti == false ? 13 : 18), ' ');
    if(towho == NULL) {
        return;
    }

    towho[0] = '\0';

    User * pOtherUser = clsHashManager::mPtr->FindUser(sData+(bMulti == false ? 13 : 18), towho-(sData+(bMulti == false ? 13 : 18)));
    // PPK ... no connection to yourself !!!
    if(pOtherUser == NULL || pOtherUser == curUser || pOtherUser->ui8State != User::STATE_ADDED) {
        return;
    }

    towho[0] = ' ';

    // IP check
    if(bCheck == true && clsSettingManager::mPtr->bBools[SETBOOL_CHECK_IP_IN_COMMANDS] == true && clsProfileManager::mPtr->IsAllowed(curUser, clsProfileManager::NOIPCHECK) == false) {
        if(CheckIP(curUser, towho+1) == false) {
            size_t szPortLen = 0;
            char * sPort = GetPort(towho+1, '|', szPortLen);
            if(sPort != NULL) {
                if((curUser->ui32BoolBits & User::BIT_IPV6) == User::BIT_IPV6 && (pOtherUser->ui32BoolBits & User::BIT_IPV6) == User::BIT_IPV6) {
                    int imsgLen = sprintf(clsServerManager::sGlobalBuffer, "$ConnectToMe %s [%s]:%s|", pOtherUser->sNick, curUser->sIP, sPort);
                    if(CheckSprintf(imsgLen, clsServerManager::szGlobalBufferSize, "clsDcCommands::ConnectToMe4") == true) {
                        curUser->AddPrcsdCmd(PrcsdUsrCmd::CTM_MCTM_RCTM_SR_TO, clsServerManager::sGlobalBuffer, imsgLen, pOtherUser);
                    }

                    char * sBadIP = towho+1;
                    if(sBadIP[0] == '[') {
                        sBadIP[strlen(sBadIP)-1] = '\0';
                        sBadIP++;
                    } else if(strchr(sBadIP, '.') == NULL) {
                        *(sPort-1) = ':';
                    }

                    SendIPFixedMsg(curUser, sBadIP, curUser->sIP);
                    return;
                } else if((curUser->ui32BoolBits & User::BIT_IPV4) == User::BIT_IPV4 && (pOtherUser->ui32BoolBits & User::BIT_IPV4) == User::BIT_IPV4) {
                    char * sIP = curUser->ui8IPv4Len == 0 ? curUser->sIP : curUser->sIPv4;

                    int imsgLen = sprintf(clsServerManager::sGlobalBuffer, "$ConnectToMe %s %s:%s|", pOtherUser->sNick, sIP, sPort);
                    if(CheckSprintf(imsgLen, clsServerManager::szGlobalBufferSize, "clsDcCommands::ConnectToMe5") == true) {
                        curUser->AddPrcsdCmd(PrcsdUsrCmd::CTM_MCTM_RCTM_SR_TO, clsServerManager::sGlobalBuffer, imsgLen, pOtherUser);
                    }

                    char * sBadIP = towho+1;
                    if(sBadIP[0] == '[') {
                        sBadIP[strlen(sBadIP)-1] = '\0';
                        sBadIP++;
                    } else if(strchr(sBadIP, '.') == NULL) {
                        *(sPort-1) = ':';
                    }

                    SendIPFixedMsg(curUser, sBadIP, sIP);
                    return;
                }

                *(sPort-1) = ':';
                *(sPort+szPortLen) = '|';
            }

            SendIncorrectIPMsg(curUser, towho+1, true);

            if(iLen > 65000) {
                sData[65000] = '\0';
            }

            int imsgLen = sprintf(clsServerManager::sGlobalBuffer, "[SYS] Bad IP in %sCTM from %s (%s). (%s)", bMulti == false ? "" : "M", curUser->sNick, curUser->sIP, sData);
            if(CheckSprintf(imsgLen, clsServerManager::szGlobalBufferSize, "clsDcCommands::ConnectToMe6") == true) {
                clsUdpDebug::mPtr->Broadcast(clsServerManager::sGlobalBuffer, imsgLen);
            }

            curUser->Close();
            return;
        }
    }

    if(bMulti == true) {
        sData[5] = '$';
    }

    curUser->AddPrcsdCmd(PrcsdUsrCmd::CTM_MCTM_RCTM_SR_TO, bMulti == false ? sData : sData+5, bMulti == false ? iLen : iLen-5, pOtherUser);
}
//---------------------------------------------------------------------------

// $GetINFO <nickname> <ownnickname>
void clsDcCommands::GetINFO(User * curUser, char * sData, const uint32_t &iLen) {
	if(((curUser->ui32SupportBits & User::SUPPORTBIT_NOGETINFO) == User::SUPPORTBIT_NOGETINFO) == true ||
        ((curUser->ui32SupportBits & User::SUPPORTBIT_NOHELLO) == User::SUPPORTBIT_NOHELLO) == true ||
        ((curUser->ui32SupportBits & User::SUPPORTBIT_QUICKLIST) == User::SUPPORTBIT_QUICKLIST) == true) {
        int imsgLen = sprintf(msg, "[SYS] Not allowed user %s (%s) send $GetINFO - user closed.", curUser->sNick, curUser->sIP);
        if(CheckSprintf(imsgLen, 1024, "clsDcCommands::GetINFO1") == true) {
            clsUdpDebug::mPtr->Broadcast(msg, imsgLen);
        }
        curUser->Close();
        return;
    }
    
    // PPK ... code change, added own nick and space on right place check
    if(iLen < (12u+curUser->ui8NickLen) || iLen > (75u+curUser->ui8NickLen) || sData[iLen-curUser->ui8NickLen-2] != ' ' ||
        memcmp(sData+(iLen-curUser->ui8NickLen-1), curUser->sNick, curUser->ui8NickLen) != 0) {
        if(iLen > 65000) {
            sData[65000] = '\0';
        }

        int imsgLen = sprintf(clsServerManager::sGlobalBuffer, "[SYS] Bad $GetINFO from %s (%s) - user closed. (%s)", curUser->sNick, curUser->sIP, sData);
        if(CheckSprintf(imsgLen, clsServerManager::szGlobalBufferSize, "clsDcCommands::GetINFO2") == true) {
            clsUdpDebug::mPtr->Broadcast(clsServerManager::sGlobalBuffer, imsgLen);
        }

        curUser->Close();
        return;
    }

	if(clsScriptManager::mPtr->Arrival(curUser, sData, iLen, clsScriptManager::GETINFO_ARRIVAL) == true ||
		curUser->ui8State >= User::STATE_CLOSING) {
		return;
	}

	// PPK ... for now this is disabled, used by flooders... change later
/*    sData[iLen-curUser->NickLen-2] = '\0';
    sData += 9;
    User *OtherUser = clsHashManager::mPtr->FindUser(sData);
    if(OtherUser == NULL) {
        // if the user is not here then send $Quit user| !!!
        // so the client can remove him from nicklist :)
        if(ReservedNicks->IndexOf(sData) == -1) {
            int imsgLen = sprintf(msg, "$Quit %s|", OtherUser->Nick);
            curUser->SendCharDelayed(msg, imsgLen);
        }
    }*/
}
//---------------------------------------------------------------------------

// $GetNickList
bool clsDcCommands::GetNickList(User * curUser, char * sData, const uint32_t &iLen, const bool &bCheck) {
    if(((curUser->ui32SupportBits & User::SUPPORTBIT_QUICKLIST) == User::SUPPORTBIT_QUICKLIST) == true &&
        ((curUser->ui32BoolBits & User::BIT_HAVE_GETNICKLIST) == User::BIT_HAVE_GETNICKLIST) == true) {
        // PPK ... refresh not allowed !
        #ifdef _DBG
           	int iret = sprintf(msg, "%s (%s) bad $GetNickList request.", curUser->Nick, curUser->IP);
           	if(CheckSprintf(iret, 1024, "clsDcCommands::GetNickList1") == true) {
               	Memo(msg);
            }
        #endif
        int imsgLen = sprintf(msg, "[SYS] Bad $GetNickList request %s (%s) - user closed.", curUser->sNick, curUser->sIP);
        if(CheckSprintf(imsgLen, 1024, "clsDcCommands::GetNickList2") == true) {
            clsUdpDebug::mPtr->Broadcast(msg, imsgLen);
        }
        curUser->Close();
        return false;
    } else if(((curUser->ui32BoolBits & User::BIT_PINGER) == User::BIT_PINGER) == true) {
		if(((curUser->ui32BoolBits & User::BIT_HAVE_GETNICKLIST) == User::BIT_HAVE_GETNICKLIST) == false) {
            curUser->ui32BoolBits |= User::BIT_BIG_SEND_BUFFER;
            if(((curUser->ui32SupportBits & User::SUPPORTBIT_NOHELLO) == User::SUPPORTBIT_NOHELLO) == false && clsUsers::mPtr->nickListLen > 11) {
                if(((curUser->ui32SupportBits & User::SUPPORTBIT_ZPIPE) == User::SUPPORTBIT_ZPIPE) == false) {
                    curUser->SendCharDelayed(clsUsers::mPtr->nickList, clsUsers::mPtr->nickListLen);
                } else {
                    if(clsUsers::mPtr->iZNickListLen == 0) {
                        clsUsers::mPtr->sZNickList = clsZlibUtility::mPtr->CreateZPipe(clsUsers::mPtr->nickList, clsUsers::mPtr->nickListLen, clsUsers::mPtr->sZNickList,
                            clsUsers::mPtr->iZNickListLen, clsUsers::mPtr->iZNickListSize, Allign16K);
                        if(clsUsers::mPtr->iZNickListLen == 0) {
                            curUser->SendCharDelayed(clsUsers::mPtr->nickList, clsUsers::mPtr->nickListLen);
                        } else {
                            curUser->PutInSendBuf(clsUsers::mPtr->sZNickList, clsUsers::mPtr->iZNickListLen);
                            clsServerManager::ui64BytesSentSaved += clsUsers::mPtr->nickListLen-clsUsers::mPtr->iZNickListLen;
                        }
                    } else {
                        curUser->PutInSendBuf(clsUsers::mPtr->sZNickList, clsUsers::mPtr->iZNickListLen);
                        clsServerManager::ui64BytesSentSaved += clsUsers::mPtr->nickListLen-clsUsers::mPtr->iZNickListLen;
                    }
                }
            }
            
            if(clsSettingManager::mPtr->ui8FullMyINFOOption == 2) {
                if(clsUsers::mPtr->myInfosLen != 0) {
                    if(((curUser->ui32SupportBits & User::SUPPORTBIT_ZPIPE) == User::SUPPORTBIT_ZPIPE) == false) {
                        curUser->SendCharDelayed(clsUsers::mPtr->myInfos, clsUsers::mPtr->myInfosLen);
                    } else {
                        if(clsUsers::mPtr->iZMyInfosLen == 0) {
                            clsUsers::mPtr->sZMyInfos = clsZlibUtility::mPtr->CreateZPipe(clsUsers::mPtr->myInfos, clsUsers::mPtr->myInfosLen, clsUsers::mPtr->sZMyInfos,
                                clsUsers::mPtr->iZMyInfosLen, clsUsers::mPtr->iZMyInfosSize, Allign128K);
                            if(clsUsers::mPtr->iZMyInfosLen == 0) {
                                curUser->SendCharDelayed(clsUsers::mPtr->myInfos, clsUsers::mPtr->myInfosLen);
                            } else {
                                curUser->PutInSendBuf(clsUsers::mPtr->sZMyInfos, clsUsers::mPtr->iZMyInfosLen);
                                clsServerManager::ui64BytesSentSaved += clsUsers::mPtr->myInfosLen-clsUsers::mPtr->iZMyInfosLen;
                            }
                        } else {
                            curUser->PutInSendBuf(clsUsers::mPtr->sZMyInfos, clsUsers::mPtr->iZMyInfosLen);
                            clsServerManager::ui64BytesSentSaved += clsUsers::mPtr->myInfosLen-clsUsers::mPtr->iZMyInfosLen;
                        }
                    }
                }
            } else if(clsUsers::mPtr->myInfosTagLen != 0) {
                if(((curUser->ui32SupportBits & User::SUPPORTBIT_ZPIPE) == User::SUPPORTBIT_ZPIPE) == false) {
                    curUser->SendCharDelayed(clsUsers::mPtr->myInfosTag, clsUsers::mPtr->myInfosTagLen);
                } else {
                    if(clsUsers::mPtr->iZMyInfosTagLen == 0) {
                        clsUsers::mPtr->sZMyInfosTag = clsZlibUtility::mPtr->CreateZPipe(clsUsers::mPtr->myInfosTag, clsUsers::mPtr->myInfosTagLen, clsUsers::mPtr->sZMyInfosTag,
                            clsUsers::mPtr->iZMyInfosTagLen, clsUsers::mPtr->iZMyInfosTagSize, Allign128K);
                        if(clsUsers::mPtr->iZMyInfosTagLen == 0) {
                            curUser->SendCharDelayed(clsUsers::mPtr->myInfosTag, clsUsers::mPtr->myInfosTagLen);
                        } else {
                            curUser->PutInSendBuf(clsUsers::mPtr->sZMyInfosTag, clsUsers::mPtr->iZMyInfosTagLen);
                            clsServerManager::ui64BytesSentSaved += clsUsers::mPtr->myInfosTagLen-clsUsers::mPtr->iZMyInfosTagLen;
                        }
                    } else {
                        curUser->PutInSendBuf(clsUsers::mPtr->sZMyInfosTag, clsUsers::mPtr->iZMyInfosTagLen);
                        clsServerManager::ui64BytesSentSaved += clsUsers::mPtr->myInfosTagLen-clsUsers::mPtr->iZMyInfosTagLen;
                    }  
                }
            }
            
 			if(clsUsers::mPtr->opListLen > 9) {
                if(((curUser->ui32SupportBits & User::SUPPORTBIT_ZPIPE) == User::SUPPORTBIT_ZPIPE) == false) {
                    curUser->SendCharDelayed(clsUsers::mPtr->opList, clsUsers::mPtr->opListLen);
                } else {
                    if(clsUsers::mPtr->iZOpListLen == 0) {
                        clsUsers::mPtr->sZOpList = clsZlibUtility::mPtr->CreateZPipe(clsUsers::mPtr->opList, clsUsers::mPtr->opListLen, clsUsers::mPtr->sZOpList,
                            clsUsers::mPtr->iZOpListLen, clsUsers::mPtr->iZOpListSize, Allign16K);
                        if(clsUsers::mPtr->iZOpListLen == 0) {
                            curUser->SendCharDelayed(clsUsers::mPtr->opList, clsUsers::mPtr->opListLen);
                        } else {
                            curUser->PutInSendBuf(clsUsers::mPtr->sZOpList, clsUsers::mPtr->iZOpListLen);
                            clsServerManager::ui64BytesSentSaved += clsUsers::mPtr->opListLen-clsUsers::mPtr->iZOpListLen;
                        }
                    } else {
                        curUser->PutInSendBuf(clsUsers::mPtr->sZOpList, clsUsers::mPtr->iZOpListLen);
                        clsServerManager::ui64BytesSentSaved += clsUsers::mPtr->opListLen-clsUsers::mPtr->iZOpListLen;
                    }
                }
            }
            
 			if(curUser->sbdatalen != 0) {
                curUser->Try2Send();
            }
 			
  			curUser->ui32BoolBits |= User::BIT_HAVE_GETNICKLIST;
  			
   			if(clsSettingManager::mPtr->bBools[SETBOOL_REPORT_PINGERS] == true) {
                if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES_AS_PM] == true) {
                    int imsgLen = sprintf(msg, "%s $<%s> *** %s: %s %s: %s %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                        clsLanguageManager::mPtr->sTexts[LAN_PINGER_FROM_IP], curUser->sIP, clsLanguageManager::mPtr->sTexts[LAN_WITH_NICK], curUser->sNick, clsLanguageManager::mPtr->sTexts[LAN_DETECTED_LWR]);
                    if(CheckSprintf(imsgLen, 1024, "clsDcCommands::GetNickList3") == true) {
						clsGlobalDataQueue::mPtr->SingleItemStore(msg, imsgLen, NULL, 0, clsGlobalDataQueue::SI_PM2OPS);
                    }
                    imsgLen = sprintf(msg, "<%s> *** Pinger from IP: %s with nick: %s detected.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], curUser->sIP, curUser->sNick);
                    if(CheckSprintf(imsgLen, 1024, "clsDcCommands::GetNickList4") == true) {
                        clsUdpDebug::mPtr->Broadcast(msg, imsgLen);
                    }
                } else {
                    int imsgLen = sprintf(msg, "<%s> *** %s: %s %s: %s %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_PINGER_FROM_IP], curUser->sIP,
                        clsLanguageManager::mPtr->sTexts[LAN_WITH_NICK], curUser->sNick, clsLanguageManager::mPtr->sTexts[LAN_DETECTED_LWR]);
                    if(CheckSprintf(imsgLen, 1024, "clsDcCommands::GetNickList5") == true) {
                        clsUdpDebug::mPtr->Broadcast(msg, imsgLen);
						clsGlobalDataQueue::mPtr->AddQueueItem(msg, imsgLen, NULL, 0, clsGlobalDataQueue::CMD_OPS);
                    }
                }
			}
			if(((curUser->ui32BoolBits & User::BIT_HAVE_BOTINFO) == User::BIT_HAVE_BOTINFO) == true) {
                curUser->Close();
            }
			return false;
		} else {
			int imsgLen = sprintf(msg, "[SYS] $GetNickList flood from pinger %s (%s) - user closed.", curUser->sNick, curUser->sIP);
			if(CheckSprintf(imsgLen, 1024, "clsDcCommands::GetNickList6") == true) {
                clsUdpDebug::mPtr->Broadcast(msg, imsgLen);
            }
			curUser->Close();
			return false;
		}
	}

    curUser->ui32BoolBits |= User::BIT_HAVE_GETNICKLIST;
    
     // PPK ... check flood...
    if(bCheck == true && clsProfileManager::mPtr->IsAllowed(curUser, clsProfileManager::NODEFLOODGETNICKLIST) == false && 
      clsSettingManager::mPtr->iShorts[SETSHORT_GETNICKLIST_ACTION] != 0) {
        if(DeFloodCheckForFlood(curUser, DEFLOOD_GETNICKLIST, clsSettingManager::mPtr->iShorts[SETSHORT_GETNICKLIST_ACTION], 
          curUser->ui16GetNickLists, curUser->ui64GetNickListsTick, clsSettingManager::mPtr->iShorts[SETSHORT_GETNICKLIST_MESSAGES], 
          ((uint32_t)clsSettingManager::mPtr->iShorts[SETSHORT_GETNICKLIST_TIME])*60) == true) {
            return false;
        }
    }

	if(clsScriptManager::mPtr->Arrival(curUser, sData, iLen, clsScriptManager::GETNICKLIST_ARRIVAL) == true ||
		curUser->ui8State >= User::STATE_CLOSING ||
		((curUser->ui32SupportBits & User::SUPPORTBIT_QUICKLIST) == User::SUPPORTBIT_QUICKLIST) == true) {
		return false;
	}

	return true;
}
//---------------------------------------------------------------------------

// $Key
void clsDcCommands::Key(User * curUser, char * sData, const uint32_t &iLen) {
    if(((curUser->ui32BoolBits & User::BIT_HAVE_KEY) == User::BIT_HAVE_KEY) == true) {
        int imsgLen = sprintf(msg, "[SYS] $Key flood from %s (%s) - user closed.", curUser->sNick, curUser->sIP);
        if(CheckSprintf(imsgLen, 1024, "clsDcCommands::Key1") == true) {
            clsUdpDebug::mPtr->Broadcast(msg, imsgLen);
        }
        curUser->Close();
        return;
    }
    
    curUser->ui32BoolBits |= User::BIT_HAVE_KEY;

    sData[iLen-1] = '\0'; // cutoff pipe

    if(iLen < 6 || strcmp(Lock2Key(curUser->uLogInOut->pBuffer), sData+5) != 0) {
        if(iLen > 65000) {
            sData[65000] = '\0';
        }

        int imsgLen = sprintf(clsServerManager::sGlobalBuffer, "[SYS] Bad $Key from %s (%s) - user closed. (%s)", curUser->sNick, curUser->sIP, sData);
        if(CheckSprintf(imsgLen, clsServerManager::szGlobalBufferSize, "clsDcCommands::Key2") == true) {
            clsUdpDebug::mPtr->Broadcast(clsServerManager::sGlobalBuffer, imsgLen);
        }

        curUser->Close();
        return;
    }

    curUser->FreeBuffer();

    sData[iLen-1] = '|'; // add back pipe

	clsScriptManager::mPtr->Arrival(curUser, sData, iLen, clsScriptManager::KEY_ARRIVAL);

	if(curUser->ui8State >= User::STATE_CLOSING) {
		return;
	}

    curUser->ui8State = User::STATE_VALIDATE;
}
//---------------------------------------------------------------------------

// $Kick <name>
void clsDcCommands::Kick(User * curUser, char * sData, const uint32_t &iLen) {
    if(clsProfileManager::mPtr->IsAllowed(curUser, clsProfileManager::KICK) == false) {
        int iLen = sprintf(msg, "<%s> %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_YOU_ARE_NOT_ALWD_TO_USE_THIS_CMD]);
        if(CheckSprintf(iLen, 1024, "clsDcCommands::Kick1") == true) {
            curUser->SendCharDelayed(msg, iLen);
        }
        return;
    } 

    if(iLen < 8) {
        int imsgLen = sprintf(msg, "[SYS] Bad $Kick (%s) from %s (%s) - user closed.", sData, curUser->sNick, curUser->sIP);
        if(CheckSprintf(imsgLen, 1024, "clsDcCommands::Kick2") == true) {
            clsUdpDebug::mPtr->Broadcast(msg, imsgLen);
        }
        curUser->Close();
        return;
    }

	if(clsScriptManager::mPtr->Arrival(curUser, sData, iLen, clsScriptManager::KICK_ARRIVAL) == true ||
		curUser->ui8State >= User::STATE_CLOSING) {
		return;
	}

    sData[iLen-1] = '\0'; // cutoff pipe

    User *OtherUser = clsHashManager::mPtr->FindUser(sData+6, iLen-7);
    if(OtherUser != NULL) {
        // Self-kick
        if(OtherUser == curUser) {
        	int imsgLen = sprintf(msg, "<%s> %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_YOU_CANT_KICK_YOURSELF]);
        	if(CheckSprintf(imsgLen, 1024, "clsDcCommands::Kick3") == true) {
                curUser->SendCharDelayed(msg, imsgLen);
            }
            return;
    	}
    	
        if(OtherUser->iProfile != -1 && curUser->iProfile > OtherUser->iProfile) {
        	int imsgLen = sprintf(msg, "<%s> %s %s|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_YOU_ARE_NOT_ALLOWED_TO_KICK], OtherUser->sNick);
        	if(CheckSprintf(imsgLen, 1024, "clsDcCommands::Kick4") == true) {
                curUser->SendCharDelayed(msg, imsgLen);
            }
        	return;
        }

        if(curUser->cmdToUserStrt != NULL) {
            PrcsdToUsrCmd *prev = NULL, 
                *next = curUser->cmdToUserStrt;

            while(next != NULL) {
                PrcsdToUsrCmd *cur = next;
                next = cur->next;
                                       
                if(OtherUser == cur->To) {
                    cur->To->SendChar(cur->sCommand, cur->iLen);

                    if(prev == NULL) {
                        if(cur->next == NULL) {
                            curUser->cmdToUserStrt = NULL;
                            curUser->cmdToUserEnd = NULL;
                        } else {
                            curUser->cmdToUserStrt = cur->next;
                        }
                    } else if(cur->next == NULL) {
                        prev->next = NULL;
                        curUser->cmdToUserEnd = prev;
                    } else {
                        prev->next = cur->next;
                    }

#ifdef _WIN32
					if(HeapFree(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE, (void *)cur->sCommand) == 0) {
						AppendDebugLog("%s - [MEM] Cannot deallocate cur->sCommand in clsDcCommands::Kick\n", 0);
                    }
#else
					free(cur->sCommand);
#endif
                    cur->sCommand = NULL;

#ifdef _WIN32
                    if(HeapFree(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE, (void *)cur->ToNick) == 0) {
						AppendDebugLog("%s - [MEM] Cannot deallocate cur->ToNick in clsDcCommands::Kick\n", 0);
                    }
#else
					free(cur->ToNick);
#endif
                    cur->ToNick = NULL;

					delete cur;
                    break;
                }
                prev = cur;
            }
        }

        char * sBanTime;
		if(OtherUser->uLogInOut != NULL && OtherUser->uLogInOut->pBuffer != NULL &&
			(sBanTime = stristr(OtherUser->uLogInOut->pBuffer, "_BAN_")) != NULL) {
			sBanTime[0] = '\0';

			if(sBanTime[5] == '\0' || sBanTime[5] == ' ') { // permban
				clsBanManager::mPtr->Ban(OtherUser, OtherUser->uLogInOut->pBuffer, curUser->sNick, false);
    
                if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES] == true) {
                    int imsgLen = 0;
                    if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES_AS_PM] == true) {
                        imsgLen = sprintf(msg, "%s $", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC]);
                        CheckSprintf(imsgLen, 1024, "clsDcCommands::Kick5");
                    }
                    
                    int iret = sprintf(msg+imsgLen, "<%s> *** %s %s %s %s %s %s %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], OtherUser->sNick,
                        clsLanguageManager::mPtr->sTexts[LAN_WITH_IP], OtherUser->sIP, clsLanguageManager::mPtr->sTexts[LAN_HAS_BEEN], clsLanguageManager::mPtr->sTexts[LAN_BANNED_LWR],
                        clsLanguageManager::mPtr->sTexts[LAN_BY_LWR], curUser->sNick);
					imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsDcCommands::Kick6") == true) {
                        if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES_AS_PM] == true) {
        					clsGlobalDataQueue::mPtr->SingleItemStore(msg, imsgLen, NULL, 0, clsGlobalDataQueue::SI_PM2OPS);
                        } else {
							clsGlobalDataQueue::mPtr->AddQueueItem(msg, imsgLen, NULL, 0, clsGlobalDataQueue::CMD_OPS);
                        }
                    }
                }

                if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES] == false || ((curUser->ui32BoolBits & User::BIT_OPERATOR) == User::BIT_OPERATOR) == false) {
                    int imsgLen = sprintf(msg, "<%s> *** %s %s %s %s %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], OtherUser->sNick, clsLanguageManager::mPtr->sTexts[LAN_WITH_IP],
                        OtherUser->sIP, clsLanguageManager::mPtr->sTexts[LAN_HAS_BEEN], clsLanguageManager::mPtr->sTexts[LAN_BANNED_LWR]);
					if(CheckSprintf(imsgLen, 1024, "clsDcCommands::Kick7") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
                }

        		// disconnect the user
        		int imsgLen = sprintf(msg, "[SYS] User %s (%s) kicked by %s", OtherUser->sNick, OtherUser->sIP, curUser->sNick);
                	if(CheckSprintf(imsgLen, 1024, "clsDcCommands::Kick8") == true) {
					clsUdpDebug::mPtr->Broadcast(msg, imsgLen);
				}
				OtherUser->Close();

                return;
			} else if(isdigit(sBanTime[5]) != 0) { // tempban
				uint32_t i = 6;
				while(sBanTime[i] != '\0' && isdigit(sBanTime[i]) != 0) {
                	i++;
                }

                char cTime = sBanTime[i];
                sBanTime[i] = '\0';
                int iTime = atoi(sBanTime+5);
				time_t acc_time, ban_time;

				if(cTime != '\0' && iTime > 0 && GenerateTempBanTime(cTime, iTime, acc_time, ban_time) == true) {
					clsBanManager::mPtr->TempBan(OtherUser, OtherUser->uLogInOut->pBuffer, curUser->sNick, 0, ban_time, false);

                    static char sTime[256];
                    strcpy(sTime, formatTime((ban_time-acc_time)/60));

					if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES] == true) {
                        int imsgLen = 0;
                        if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES_AS_PM] == true) {
                            imsgLen = sprintf(msg, "%s $", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC]);
							CheckSprintf(imsgLen, 1024, "clsDcCommands::Kick9");
                        }
                        
                        int iret = sprintf(msg+imsgLen, "<%s> *** %s %s %s %s %s %s %s %s: %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], OtherUser->sNick,
                            clsLanguageManager::mPtr->sTexts[LAN_WITH_IP], OtherUser->sIP, clsLanguageManager::mPtr->sTexts[LAN_HAS_BEEN], clsLanguageManager::mPtr->sTexts[LAN_TEMP_BANNED],
                            clsLanguageManager::mPtr->sTexts[LAN_BY_LWR], curUser->sNick, clsLanguageManager::mPtr->sTexts[LAN_TO_LWR], sTime);
						imsgLen += iret;
                        if(CheckSprintf1(iret, imsgLen, 1024, "clsDcCommands::Kick10") == true) {
							if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES_AS_PM] == true) {
            					clsGlobalDataQueue::mPtr->SingleItemStore(msg, imsgLen, NULL, 0, clsGlobalDataQueue::SI_PM2OPS);
                            } else {
								clsGlobalDataQueue::mPtr->AddQueueItem(msg, imsgLen, NULL, 0, clsGlobalDataQueue::CMD_OPS);
                            }
                        }
                    }
                
                    if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES] == false || ((curUser->ui32BoolBits & User::BIT_OPERATOR) == User::BIT_OPERATOR) == false) {
                        int imsgLen = sprintf(msg, "<%s> *** %s %s %s %s %s %s: %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], OtherUser->sNick,
                            clsLanguageManager::mPtr->sTexts[LAN_WITH_IP], OtherUser->sIP, clsLanguageManager::mPtr->sTexts[LAN_HAS_BEEN], clsLanguageManager::mPtr->sTexts[LAN_TEMP_BANNED],
                            clsLanguageManager::mPtr->sTexts[LAN_TO_LWR], sTime);
                        if(CheckSprintf(imsgLen, 1024, "clsDcCommands::Kick11") == true) {
                            curUser->SendCharDelayed(msg, imsgLen);
                        }
                	}
    
                    // disconnect the user
                    int imsgLen = sprintf(msg, "[SYS] User %s (%s) kicked by %s", OtherUser->sNick, OtherUser->sIP, curUser->sNick);
                    if(CheckSprintf(imsgLen, 1024, "clsDcCommands::Kick12") == true) {
                        clsUdpDebug::mPtr->Broadcast(msg, imsgLen);
                    }
					OtherUser->Close();

					return;
                }
            }
		}

        clsBanManager::mPtr->TempBan(OtherUser, OtherUser->uLogInOut != NULL ? OtherUser->uLogInOut->pBuffer : NULL, curUser->sNick, 0, 0, false);

        if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES] == true) {
            int imsgLen = 0;
            if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES_AS_PM] == true) {
                imsgLen = sprintf(msg, "%s $", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC]);
                CheckSprintf(imsgLen, 1024, "clsDcCommands::Kick13");
            }

            int iret = sprintf(msg+imsgLen, "<%s> *** %s %s %s %s %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], OtherUser->sNick, clsLanguageManager::mPtr->sTexts[LAN_WITH_IP],
                OtherUser->sIP, clsLanguageManager::mPtr->sTexts[LAN_WAS_KICKED_BY], curUser->sNick);
            imsgLen += iret;
            if(CheckSprintf1(iret, imsgLen, 1024, "clsDcCommands::Kick14") == true) {
                if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES_AS_PM] == true) {
    				clsGlobalDataQueue::mPtr->SingleItemStore(msg, imsgLen, NULL, 0, clsGlobalDataQueue::SI_PM2OPS);
                } else {
					clsGlobalDataQueue::mPtr->AddQueueItem(msg, imsgLen, NULL, 0, clsGlobalDataQueue::CMD_OPS);
                }
            }
        }

        if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES] == false || ((curUser->ui32BoolBits & User::BIT_OPERATOR) == User::BIT_OPERATOR) == false) {
            int imsgLen = sprintf(msg, "<%s> *** %s %s %s %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], OtherUser->sNick, clsLanguageManager::mPtr->sTexts[LAN_WITH_IP], OtherUser->sIP,
                clsLanguageManager::mPtr->sTexts[LAN_WAS_KICKED]);
            if(CheckSprintf(imsgLen, 1024, "clsDcCommands::Kick15") == true) {
                curUser->SendCharDelayed(msg, imsgLen);
            }
        }

        // disconnect the user
        int imsgLen = sprintf(msg, "[SYS] User %s (%s) kicked by %s", OtherUser->sNick, OtherUser->sIP, curUser->sNick);
        if(CheckSprintf(imsgLen, 1024, "clsDcCommands::Kick16") == true) {
            clsUdpDebug::mPtr->Broadcast(msg, imsgLen);
        }
        OtherUser->Close();
    }
}
//---------------------------------------------------------------------------

// $Search $MultiSearch
bool clsDcCommands::SearchDeflood(User *curUser, char * sData, const uint32_t &iLen, const bool &bCheck, const bool &bMulti) {
    // search flood protection ... modified by PPK ;-)
    if(bCheck == true && clsProfileManager::mPtr->IsAllowed(curUser, clsProfileManager::NODEFLOODSEARCH) == false) {
        if(clsSettingManager::mPtr->iShorts[SETSHORT_SEARCH_ACTION] != 0) {  
            if(DeFloodCheckForFlood(curUser, DEFLOOD_SEARCH, clsSettingManager::mPtr->iShorts[SETSHORT_SEARCH_ACTION], 
              curUser->ui16Searchs, curUser->ui64SearchsTick, clsSettingManager::mPtr->iShorts[SETSHORT_SEARCH_MESSAGES], 
              (uint32_t)clsSettingManager::mPtr->iShorts[SETSHORT_SEARCH_TIME]) == true) {
                return false;
            }
        }

        if(clsSettingManager::mPtr->iShorts[SETSHORT_SEARCH_ACTION2] != 0) {  
            if(DeFloodCheckForFlood(curUser, DEFLOOD_SEARCH, clsSettingManager::mPtr->iShorts[SETSHORT_SEARCH_ACTION2], 
              curUser->ui16Searchs2, curUser->ui64SearchsTick2, clsSettingManager::mPtr->iShorts[SETSHORT_SEARCH_MESSAGES2], 
              (uint32_t)clsSettingManager::mPtr->iShorts[SETSHORT_SEARCH_TIME2]) == true) {
                return false;
            }
        }

        // 2nd check for same search flood
		if(clsSettingManager::mPtr->iShorts[SETSHORT_SAME_SEARCH_ACTION] != 0) {
			bool bNewData = false;
            if(DeFloodCheckForSameFlood(curUser, DEFLOOD_SAME_SEARCH, clsSettingManager::mPtr->iShorts[SETSHORT_SAME_SEARCH_ACTION], 
              curUser->ui16SameSearchs, curUser->ui64SameSearchsTick, 
              clsSettingManager::mPtr->iShorts[SETSHORT_SAME_SEARCH_MESSAGES], clsSettingManager::mPtr->iShorts[SETSHORT_SAME_SEARCH_TIME], 
			  sData+(bMulti == true ? 13 : 8), iLen-(bMulti == true ? 13 : 8),
			  curUser->sLastSearch, curUser->ui16LastSearchLen, bNewData) == true) {
                return false;
            }

			if(bNewData == true) {
				curUser->SetLastSearch(sData+(bMulti == true ? 13 : 8), iLen-(bMulti == true ? 13 : 8));
			}
        }
    }
    
    return true;
}
//---------------------------------------------------------------------------

// $Search $MultiSearch
void clsDcCommands::Search(User *curUser, char * sData, uint32_t iLen, const bool &bCheck, const bool &bMulti) {
    uint32_t iAfterCmd;
    if(bMulti == false) {
        if(iLen < 10) {
            int imsgLen = sprintf(msg, "[SYS] Bad $Search (%s) from %s (%s) - user closed.", sData, curUser->sNick, curUser->sIP);
            if(CheckSprintf(imsgLen, 1024, "clsDcCommands::Search1") == true) {
                clsUdpDebug::mPtr->Broadcast(msg, imsgLen);
            }
            curUser->Close();
			return;
        }
        iAfterCmd = 8;
    } else {
        if(iLen < 15) {
            int imsgLen = sprintf(msg, "[SYS] Bad $MultiSearch (%s) from %s (%s) - user closed.", sData, curUser->sNick, curUser->sIP);
            if(CheckSprintf(imsgLen, 1024, "clsDcCommands::Search2") == true) {
                clsUdpDebug::mPtr->Broadcast(msg, imsgLen);
            }
            curUser->Close();
            return;
        }
        iAfterCmd = 13;
    }

    if(bCheck == true && clsProfileManager::mPtr->IsAllowed(curUser, clsProfileManager::NOSEARCHINTERVAL) == false) {
        if(DeFloodCheckInterval(curUser, INTERVAL_SEARCH, curUser->ui16SearchsInt, 
            curUser->ui64SearchsIntTick, clsSettingManager::mPtr->iShorts[SETSHORT_SEARCH_INTERVAL_MESSAGES], 
            (uint32_t)clsSettingManager::mPtr->iShorts[SETSHORT_SEARCH_INTERVAL_TIME]) == true) {
            return;
        }
    }

	if(clsScriptManager::mPtr->Arrival(curUser, sData, iLen, clsScriptManager::SEARCH_ARRIVAL) == true ||
		curUser->ui8State >= User::STATE_CLOSING) {
		return;
	}

    // send search from actives to all, from passives to actives only
    // PPK ... optimization ;o)
    if(bMulti == false && *((uint32_t *)(sData+iAfterCmd)) == *((uint32_t *)"Hub:")) {
        if(curUser->sTag == NULL) {
            curUser->ui32BoolBits &= ~User::BIT_IPV4_ACTIVE;
        }

        // PPK ... check nick !!!
        if((sData[iAfterCmd+4+curUser->ui8NickLen] != ' ') || (memcmp(sData+iAfterCmd+4, curUser->sNick, curUser->ui8NickLen) != 0)) {
            if(iLen > 65000) {
                sData[65000] = '\0';
            }

            int imsgLen = sprintf(clsServerManager::sGlobalBuffer, "[SYS] Nick spoofing in search from %s (%s) - user closed. (%s)", curUser->sNick, curUser->sIP, sData);
            if(CheckSprintf(imsgLen, clsServerManager::szGlobalBufferSize, "clsDcCommands::Search3") == true) {
                clsUdpDebug::mPtr->Broadcast(clsServerManager::sGlobalBuffer, imsgLen);
            }

            curUser->Close();
            return;
        }

        if(bCheck == true && clsProfileManager::mPtr->IsAllowed(curUser, clsProfileManager::NOSEARCHLIMITS) == false &&
            (clsSettingManager::mPtr->iShorts[SETSHORT_MIN_SEARCH_LEN] != 0 || clsSettingManager::mPtr->iShorts[SETSHORT_MAX_SEARCH_LEN] != 0)) {
            // PPK ... search string len check
            // $Search Hub:PPK F?T?0?2?test|
            uint32_t iChar = iAfterCmd+8+curUser->ui8NickLen+1;
            uint32_t iCount = 0;
            for(; iChar < iLen; iChar++) {
                if(sData[iChar] == '?') {
                    iCount++;
                    if(iCount == 2)
                        break;
                }
            }
            iCount = iLen-2-iChar;

            if(iCount < (uint32_t)clsSettingManager::mPtr->iShorts[SETSHORT_MIN_SEARCH_LEN]) {
                int imsgLen = sprintf(msg, "<%s> %s %hd.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_SORRY_MIN_SEARCH_LEN_IS],
                    clsSettingManager::mPtr->iShorts[SETSHORT_MIN_SEARCH_LEN]);
                if(CheckSprintf(imsgLen, 1024, "clsDcCommands::Search5") == true) {
                    curUser->SendCharDelayed(msg, imsgLen);
                }
                return;
            }
            if(clsSettingManager::mPtr->iShorts[SETSHORT_MAX_SEARCH_LEN] != 0 && iCount > (uint32_t)clsSettingManager::mPtr->iShorts[SETSHORT_MAX_SEARCH_LEN]) {
                int imsgLen = sprintf(msg, "<%s> %s %hd.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_SORRY_MAX_SEARCH_LEN_IS],
                    clsSettingManager::mPtr->iShorts[SETSHORT_MAX_SEARCH_LEN]);
                if(CheckSprintf(imsgLen, 1024, "clsDcCommands::Search6") == true) {
                    curUser->SendCharDelayed(msg, imsgLen);
                }
                return;
            }
        }

        curUser->iSR = 0;

        curUser->cmdPassiveSearch = AddSearch(curUser, curUser->cmdPassiveSearch, sData, iLen, false);
    } else {
        if(curUser->sTag == NULL) {
            curUser->ui32BoolBits |= User::BIT_IPV4_ACTIVE;
        }

        if(bCheck == true && clsProfileManager::mPtr->IsAllowed(curUser, clsProfileManager::NOSEARCHLIMITS) == false &&
            (clsSettingManager::mPtr->iShorts[SETSHORT_MIN_SEARCH_LEN] != 0 || clsSettingManager::mPtr->iShorts[SETSHORT_MAX_SEARCH_LEN] != 0)) {
            // PPK ... search string len check
            // $Search 1.2.3.4:1 F?F?0?2?test|
            uint32_t iChar = iAfterCmd+11;
            uint32_t iCount = 0;

            for(; iChar < iLen; iChar++) {
                if(sData[iChar] == '?') {
                    iCount++;
                    if(iCount == 4)
                        break;
                }
            }

            iCount = iLen-2-iChar;

            if(iCount < (uint32_t)clsSettingManager::mPtr->iShorts[SETSHORT_MIN_SEARCH_LEN]) {
                int imsgLen = sprintf(msg, "<%s> %s %hd.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_SORRY_MIN_SEARCH_LEN_IS],
                    clsSettingManager::mPtr->iShorts[SETSHORT_MIN_SEARCH_LEN]);
                if(CheckSprintf(imsgLen, 1024, "clsDcCommands::Search10") == true) {
                    curUser->SendCharDelayed(msg, imsgLen);
                }
                return;
            }
            if(clsSettingManager::mPtr->iShorts[SETSHORT_MAX_SEARCH_LEN] != 0 && iCount > (uint32_t)clsSettingManager::mPtr->iShorts[SETSHORT_MAX_SEARCH_LEN]) {
                int imsgLen = sprintf(msg, "<%s> %s %hd.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_SORRY_MAX_SEARCH_LEN_IS],
                    clsSettingManager::mPtr->iShorts[SETSHORT_MAX_SEARCH_LEN]);
                if(CheckSprintf(imsgLen, 1024, "clsDcCommands::Search11") == true) {
                    curUser->SendCharDelayed(msg, imsgLen);
                }
                return;
            }
        }

        // IP check
        if(bCheck == true && clsSettingManager::mPtr->bBools[SETBOOL_CHECK_IP_IN_COMMANDS] == true && clsProfileManager::mPtr->IsAllowed(curUser, clsProfileManager::NOIPCHECK) == false) {
            if(CheckIP(curUser, sData+iAfterCmd) == false) {
                size_t szPortLen = 0;
                char * sPort = GetPort(sData+iAfterCmd, ' ', szPortLen);
                if(sPort != NULL) {
                    if((curUser->ui32BoolBits & User::BIT_IPV6) == User::BIT_IPV6) {
                        if((curUser->ui32BoolBits & User::BIT_IPV6_ACTIVE) == User::BIT_IPV6_ACTIVE) {
                            int imsgLen = sprintf(clsServerManager::sGlobalBuffer, "$Search [%s]:%s %s", curUser->sIP, sPort, sPort+szPortLen+1);
                            if(CheckSprintf(imsgLen, clsServerManager::szGlobalBufferSize, "clsDcCommands::Search12") == true) {
							 curUser->cmdActive6Search = AddSearch(curUser, curUser->cmdActive6Search, clsServerManager::sGlobalBuffer, imsgLen, true);
                            }
                        } else {
                            int imsgLen = sprintf(clsServerManager::sGlobalBuffer, "$Search Hub:%s %s", curUser->sNick, sPort+szPortLen+1);
                            if(CheckSprintf(imsgLen, clsServerManager::szGlobalBufferSize, "clsDcCommands::Search12-1") == true) {
                                curUser->cmdPassiveSearch = AddSearch(curUser, curUser->cmdPassiveSearch, clsServerManager::sGlobalBuffer, imsgLen, false);
                            }
                        }

						if((curUser->ui32BoolBits & User::BIT_IPV4) == User::BIT_IPV4) {
                            if((curUser->ui32BoolBits & User::BIT_IPV4_ACTIVE) == User::BIT_IPV4_ACTIVE) {
                                int imsgLen = sprintf(clsServerManager::sGlobalBuffer, "$Search %s:%s %s", curUser->sIPv4, sPort, sPort+szPortLen+1);
                                if(CheckSprintf(imsgLen, clsServerManager::szGlobalBufferSize, "clsDcCommands::Search12-2") == true) {
                                    curUser->cmdActive4Search = AddSearch(curUser, curUser->cmdActive4Search, clsServerManager::sGlobalBuffer, imsgLen, true);
                                }
                            } else {
                                int imsgLen = sprintf(clsServerManager::sGlobalBuffer, "$Search Hub:%s %s", curUser->sNick, sPort+szPortLen+1);
                                if(CheckSprintf(imsgLen, clsServerManager::szGlobalBufferSize, "clsDcCommands::Search12-3") == true) {
                                    curUser->cmdPassiveSearch = AddSearch(curUser, curUser->cmdPassiveSearch, clsServerManager::sGlobalBuffer, imsgLen, false);
                                }
                            }
						}

                        char * sBadIP = sData+iAfterCmd;
                        if(sBadIP[0] == '[') {
                            sBadIP[strlen(sBadIP)-1] = '\0';
                            sBadIP++;
                        } else if(strchr(sBadIP, '.') == NULL) {
                            *(sPort-1) = ':';
                        }

                        SendIPFixedMsg(curUser, sBadIP, curUser->sIP);
                        return;
                    } else if((curUser->ui32BoolBits & User::BIT_IPV4) == User::BIT_IPV4) {
                        char * sIP = curUser->ui8IPv4Len == 0 ? curUser->sIP : curUser->sIPv4;

                        int imsgLen = sprintf(clsServerManager::sGlobalBuffer, "$Search %s:%s %s", sIP, sPort, sPort+szPortLen+1);
                        if(CheckSprintf(imsgLen, clsServerManager::szGlobalBufferSize, "clsDcCommands::Search13") == true) {
							curUser->cmdActive4Search = AddSearch(curUser, curUser->cmdActive4Search, clsServerManager::sGlobalBuffer, imsgLen, true);
                        }

                        char * sBadIP = sData+iAfterCmd;
                        if(sBadIP[0] == '[') {
                            sBadIP[strlen(sBadIP)-1] = '\0';
                            sBadIP++;
                        } else if(strchr(sBadIP, '.') == NULL) {
                            *(sPort-1) = ':';
                        }

                        SendIPFixedMsg(curUser, sBadIP, sIP);
                        return;
                    }

                    *(sPort-1) = ':';
                    *(sPort+szPortLen) = ' ';
                }

                SendIncorrectIPMsg(curUser, sData+iAfterCmd, false);

                if(iLen > 65000) {
                    sData[65000] = '\0';
                }

                int imsgLen = sprintf(clsServerManager::sGlobalBuffer, "[SYS] Bad IP in Search from %s (%s). (%s)", curUser->sNick, curUser->sIP, sData);
                if(CheckSprintf(imsgLen, clsServerManager::szGlobalBufferSize, "clsDcCommands::Search14") == true) {
                    clsUdpDebug::mPtr->Broadcast(clsServerManager::sGlobalBuffer, imsgLen);
                }

                curUser->Close();
                return;
            }
        }

        if(bMulti == true) {
            sData[5] = '$';
            sData += 5;
            iLen -= 5;
        }

		if((curUser->ui32BoolBits & User::BIT_IPV6) == User::BIT_IPV6) {
			if(sData[8] == '[') {
				curUser->cmdActive6Search = AddSearch(curUser, curUser->cmdActive6Search, sData, iLen, true);

				if((curUser->ui32BoolBits & User::BIT_IPV4) == User::BIT_IPV4) {
					size_t szPortLen = 0;
					char * sPort = GetPort(sData+8, ' ', szPortLen);
					if(sPort != NULL) {
                        if((curUser->ui32BoolBits & User::BIT_IPV4_ACTIVE) == User::BIT_IPV4_ACTIVE) {
                            int imsgLen = sprintf(clsServerManager::sGlobalBuffer, "$Search %s:%s %s", curUser->sIPv4, sPort, sPort+szPortLen+1);
                            if(CheckSprintf(imsgLen, clsServerManager::szGlobalBufferSize, "clsDcCommands::Search15") == true) {
                                curUser->cmdActive4Search = AddSearch(curUser, curUser->cmdActive4Search, clsServerManager::sGlobalBuffer, imsgLen, true);
                            }
						} else {
                            int imsgLen = sprintf(clsServerManager::sGlobalBuffer, "$Search Hub:%s %s", curUser->sNick, sPort+szPortLen+1);
                            if(CheckSprintf(imsgLen, clsServerManager::szGlobalBufferSize, "clsDcCommands::Search16") == true) {
                                curUser->cmdPassiveSearch = AddSearch(curUser, curUser->cmdPassiveSearch, clsServerManager::sGlobalBuffer, imsgLen, false);
                            }
                        }
					}
				}
			} else {
                curUser->cmdActive4Search = AddSearch(curUser, curUser->cmdActive4Search, sData, iLen, true);

                if(((curUser->ui32BoolBits & User::BIT_IPV6_ACTIVE) == User::BIT_IPV6_ACTIVE) == false) {
					size_t szPortLen = 0;
					char * sPort = GetPort(sData+8, ' ', szPortLen);
					if(sPort != NULL) {
                        int imsgLen = sprintf(clsServerManager::sGlobalBuffer, "$Search Hub:%s %s", curUser->sNick, sPort+szPortLen+1);
                        if(CheckSprintf(imsgLen, clsServerManager::szGlobalBufferSize, "clsDcCommands::Search17") == true) {
                            curUser->cmdPassiveSearch = AddSearch(curUser, curUser->cmdPassiveSearch, clsServerManager::sGlobalBuffer, imsgLen, false);
                        }
                    }
                }
			}
		} else {
			curUser->cmdActive4Search = AddSearch(curUser, curUser->cmdActive4Search, sData, iLen, true);
		}
    }
}
//---------------------------------------------------------------------------

// $MyINFO $ALL  $ $$$$|
bool clsDcCommands::MyINFODeflood(User * curUser, char * sData, const uint32_t &iLen, const bool &bCheck) {
    if(iLen < (22u+curUser->ui8NickLen)) {
        int imsgLen = sprintf(msg, "[SYS] Bad $MyINFO (%s) from %s (%s) - user closed.", sData, curUser->sNick, curUser->sIP);
        if(CheckSprintf(imsgLen, 1024, "clsDcCommands::MyINFODeflood1") == true) {
            clsUdpDebug::mPtr->Broadcast(msg, imsgLen);
        }
        curUser->Close();
        return false;
    }

    // PPK ... check flood ...
    if(bCheck == true && clsProfileManager::mPtr->IsAllowed(curUser, clsProfileManager::NODEFLOODMYINFO) == false) { 
        if(clsSettingManager::mPtr->iShorts[SETSHORT_MYINFO_ACTION] != 0) {
            if(DeFloodCheckForFlood(curUser, DEFLOOD_MYINFO, clsSettingManager::mPtr->iShorts[SETSHORT_MYINFO_ACTION], 
              curUser->ui16MyINFOs, curUser->ui64MyINFOsTick, clsSettingManager::mPtr->iShorts[SETSHORT_MYINFO_MESSAGES], 
              (uint32_t)clsSettingManager::mPtr->iShorts[SETSHORT_MYINFO_TIME]) == true) {
                return false;
            }
        }

        if(clsSettingManager::mPtr->iShorts[SETSHORT_MYINFO_ACTION2] != 0) {
            if(DeFloodCheckForFlood(curUser, DEFLOOD_MYINFO, clsSettingManager::mPtr->iShorts[SETSHORT_MYINFO_ACTION2], 
              curUser->ui16MyINFOs2, curUser->ui64MyINFOsTick2, clsSettingManager::mPtr->iShorts[SETSHORT_MYINFO_MESSAGES2], 
              (uint32_t)clsSettingManager::mPtr->iShorts[SETSHORT_MYINFO_TIME2]) == true) {
                return false;
            }
        }
    }

    if(iLen > (uint32_t)clsSettingManager::mPtr->iShorts[SETSHORT_MAX_MYINFO_LEN]) {
        int imsgLen = sprintf(msg, "<%s> %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_MYINFO_TOO_LONG]);
        if(CheckSprintf(imsgLen, 1024, "clsDcCommands::MyINFODeflood2") == true) {
            curUser->SendCharDelayed(msg, imsgLen);
        }

        if(iLen > 65000) {
            sData[65000] = '\0';
        }

        imsgLen = sprintf(clsServerManager::sGlobalBuffer, "[SYS] Bad $MyINFO from %s (%s) - user closed. (%s)", curUser->sNick, curUser->sIP, sData);
        if(CheckSprintf(imsgLen, clsServerManager::szGlobalBufferSize, "clsDcCommands::MyINFODeflood3") == true) {
            clsUdpDebug::mPtr->Broadcast(clsServerManager::sGlobalBuffer, imsgLen);
        }

        curUser->Close();
		return false;
    }
    
    return true;
}
//---------------------------------------------------------------------------

// $MyINFO $ALL  $ $$$$|
bool clsDcCommands::MyINFO(User * curUser, char * sData, const uint32_t &iLen) {
    // if no change, just return
    // else store MyINFO and perform all checks again
    if(curUser->sMyInfoOriginal != NULL) { // PPK ... optimizations
       	if(iLen == curUser->ui16MyInfoOriginalLen && memcmp(curUser->sMyInfoOriginal+14+curUser->ui8NickLen, sData+14+curUser->ui8NickLen, curUser->ui16MyInfoOriginalLen-14-curUser->ui8NickLen) == 0) {
           return false;
        }
    }

    curUser->SetMyInfoOriginal(sData, (uint16_t)iLen);

    if(curUser->ui8State >= User::STATE_CLOSING) {
        return false;
    }

    if(curUser->ProcessRules() == false) {
        curUser->Close();
        return false;
    }
    
    // TODO 3 -oPTA -ccheckers: Slots fetching for no tag users
    //search command for slots fetch for users without tag
    //if(curUser->Tag == NULL)
    //{
    //    curUser->SendText("$Search "+HubAddress->Text+":411 F?F?0?1?.|");
    //}

    // SEND myinfo to others (including me) only if this is
    // a refresh MyINFO event. Else dispatch it in addMe condition
    // of service loop

    // PPK ... moved lua here -> another "optimization" ;o)
	clsScriptManager::mPtr->Arrival(curUser, sData, iLen, clsScriptManager::MYINFO_ARRIVAL);

	if(curUser->ui8State >= User::STATE_CLOSING) {
		return false;
	}
    
    return true;
}
//---------------------------------------------------------------------------

// $MyPass
void clsDcCommands::MyPass(User * curUser, char * sData, const uint32_t &iLen) {
    RegUser * pReg = clsRegManager::mPtr->Find(curUser);
    if(pReg != NULL && (curUser->ui32BoolBits & User::BIT_WAITING_FOR_PASS) == User::BIT_WAITING_FOR_PASS) {
        curUser->ui32BoolBits &= ~User::BIT_WAITING_FOR_PASS;
    } else {
        // no password required
        int imsgLen = sprintf(msg, "[SYS] $MyPass without request from %s (%s) - user closed.", curUser->sNick, curUser->sIP);
        if(CheckSprintf(imsgLen, 1024, "clsDcCommands::MyPass1") == true) {
            clsUdpDebug::mPtr->Broadcast(msg, imsgLen);
        }
        curUser->Close();
        return;
    }

    if(iLen < 10|| iLen > 73) {
        if(iLen > 65000) {
            sData[65000] = '\0';
        }

        int imsgLen = sprintf(clsServerManager::sGlobalBuffer, "[SYS] Bad $MyPass from %s (%s) - user closed. (%s)", curUser->sNick, curUser->sIP, sData);
        if(CheckSprintf(imsgLen, clsServerManager::szGlobalBufferSize, "clsDcCommands::MyPass2") == true) {
            clsUdpDebug::mPtr->Broadcast(clsServerManager::sGlobalBuffer, imsgLen);
        }

        curUser->Close();
        return;
    }

    sData[iLen-1] = '\0'; // cutoff pipe

    bool bBadPass = false;

	if(pReg->bPassHash == true) {
		uint8_t ui8Hash[64];

		size_t szLen = iLen-9;

		if(HashPassword(sData+8, szLen, ui8Hash) == false || memcmp(pReg->ui8PassHash, ui8Hash, 64) != 0) {
			bBadPass = true;
		}
	} else {
		if(strcmp(pReg->sPass, sData+8) != 0) {
			bBadPass = true;
		}
	}

    // if password is wrong, close the connection
    if(bBadPass == true) {
        if(clsSettingManager::mPtr->bBools[SETBOOL_ADVANCED_PASS_PROTECTION] == true) {
            time(&pReg->tLastBadPass);
            if(pReg->ui8BadPassCount < 255)
                pReg->ui8BadPassCount++;
        }
    
        if(clsSettingManager::mPtr->iShorts[SETSHORT_BRUTE_FORCE_PASS_PROTECT_BAN_TYPE] != 0) {
            // brute force password protection
			PassBf * PassBfItem = Find(curUser->ui128IpHash);
            if(PassBfItem == NULL) {
                PassBfItem = new PassBf(curUser->ui128IpHash);
                if(PassBfItem == NULL) {
					AppendDebugLog("%s - [MEM] Cannot allocate new PassBfItem in clsDcCommands::MyPass\n", 0);
                	return;
                }

                if(PasswdBfCheck != NULL) {
                    PasswdBfCheck->prev = PassBfItem;
                    PassBfItem->next = PasswdBfCheck;
                    PasswdBfCheck = PassBfItem;
                }
                PasswdBfCheck = PassBfItem;
            } else {
                if(PassBfItem->iCount == 2) {
                    BanItem *Ban = clsBanManager::mPtr->FindFull(curUser->ui128IpHash);
                    if(Ban == NULL || ((Ban->ui8Bits & clsBanManager::FULL) == clsBanManager::FULL) == false) {
                        int iret = sprintf(msg, "3x bad password for nick %s", curUser->sNick);
                        if(CheckSprintf(iret, 1024, "clsDcCommands::MyPass4") == false) {
                            msg[0] = '\0';
                        }
                        if(clsSettingManager::mPtr->iShorts[SETSHORT_BRUTE_FORCE_PASS_PROTECT_BAN_TYPE] == 1) {
                            clsBanManager::mPtr->BanIp(curUser, NULL, msg, NULL, true);
                        } else {
                            clsBanManager::mPtr->TempBanIp(curUser, NULL, msg, NULL, clsSettingManager::mPtr->iShorts[SETSHORT_BRUTE_FORCE_PASS_PROTECT_TEMP_BAN_TIME]*60, 0, true);
                        }
                        Remove(PassBfItem);
                        int imsgLen = sprintf(msg, "<%s> %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_YOUR_IP_BANNED_BRUTE_FORCE_ATTACK]);
                        if(CheckSprintf(imsgLen, 1024, "clsDcCommands::MyPass5") == true) {
                            curUser->SendChar(msg, imsgLen);
                        }
                    } else {
                        int imsgLen = sprintf(msg, "<%s> %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_YOUR_IS_BANNED]);
                        if(CheckSprintf(imsgLen, 1024, "clsDcCommands::MyPass6") == true) {
                            curUser->SendChar(msg, imsgLen);
                        }
                    }
                    if(clsSettingManager::mPtr->bBools[SETBOOL_REPORT_3X_BAD_PASS] == true) {
                        if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES_AS_PM] == true) {
                            int imsgLen = sprintf(msg, "%s $<%s> *** %s %s %s %s|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                                clsLanguageManager::mPtr->sTexts[LAN_IP], curUser->sIP, clsLanguageManager::mPtr->sTexts[LAN_BANNED_BECAUSE_3X_BAD_PASS_FOR_NICK], curUser->sNick);
                            if(CheckSprintf(imsgLen, 1024, "clsDcCommands::MyPass7") == true) {
								clsGlobalDataQueue::mPtr->SingleItemStore(msg, imsgLen, NULL, 0, clsGlobalDataQueue::SI_PM2OPS);
                            }
                        } else {
                            int imsgLen = sprintf(msg, "<%s> *** %s %s %s %s|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_IP], curUser->sIP,
                                clsLanguageManager::mPtr->sTexts[LAN_BANNED_BECAUSE_3X_BAD_PASS_FOR_NICK], curUser->sNick);
                            if(CheckSprintf(imsgLen, 1024, "clsDcCommands::MyPass8") == true) {
								clsGlobalDataQueue::mPtr->AddQueueItem(msg, imsgLen, NULL, 0, clsGlobalDataQueue::CMD_OPS);
                            }
                        }
                    }

                    if(iLen > 65000) {
                        sData[65000] = '\0';
                    }

                    int imsgLen = sprintf(clsServerManager::sGlobalBuffer, "[SYS] Bad 3x password from %s (%s) - user banned. (%s)", curUser->sNick, curUser->sIP, sData);
                    if(CheckSprintf(imsgLen, clsServerManager::szGlobalBufferSize, "clsDcCommands::MyPass10") == true) {
                        clsUdpDebug::mPtr->Broadcast(clsServerManager::sGlobalBuffer, imsgLen);
                    }

                    curUser->Close();
                    return;
                } else {
                    PassBfItem->iCount++;
                }
            }
        }

        int imsgLen = sprintf(msg, "$BadPass|<%s> %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_INCORRECT_PASSWORD]);
        if(CheckSprintf(imsgLen, 1024, "clsDcCommands::MyPass12") == true) {
            curUser->SendChar(msg, imsgLen);
        }

        if(iLen > 65000) {
            sData[65000] = '\0';
        }

        imsgLen = sprintf(clsServerManager::sGlobalBuffer, "[SYS] Bad password from %s (%s) - user closed. (%s)", curUser->sNick, curUser->sIP, sData);
        if(CheckSprintf(imsgLen, clsServerManager::szGlobalBufferSize, "clsDcCommands::MyPass13") == true) {
            clsUdpDebug::mPtr->Broadcast(clsServerManager::sGlobalBuffer, imsgLen);
        }

        curUser->Close();
        return;
    } else {
        pReg->ui8BadPassCount = 0;

        PassBf * PassBfItem = Find(curUser->ui128IpHash);
        if(PassBfItem != NULL) {
            Remove(PassBfItem);
        }

        sData[iLen-1] = '|'; // add back pipe

        // PPK ... Lua DataArrival only if pass is ok
		clsScriptManager::mPtr->Arrival(curUser, sData, iLen, clsScriptManager::PASSWORD_ARRIVAL);

		if(curUser->ui8State >= User::STATE_CLOSING) {
    		return;
    	}

        if(clsProfileManager::mPtr->IsAllowed(curUser, clsProfileManager::HASKEYICON) == true) {
            curUser->ui32BoolBits |= User::BIT_OPERATOR;
        } else {
            curUser->ui32BoolBits &= ~User::BIT_OPERATOR;
        }
        
        // PPK ... addition for registered users, kill your own ghost >:-]
        if(((curUser->ui32BoolBits & User::BIT_HASHED) == User::BIT_HASHED) == false) {
            User *OtherUser = clsHashManager::mPtr->FindUser(curUser->sNick, curUser->ui8NickLen);
            if(OtherUser != NULL) {
                int imsgLen = sprintf(msg, "[SYS] Ghost %s (%s) closed.", OtherUser->sNick, OtherUser->sIP);
                if(CheckSprintf(imsgLen, 1024, "clsDcCommands::MyPass12") == true) {
                    clsUdpDebug::mPtr->Broadcast(msg, imsgLen);
                }
                if(((curUser->ui32SupportBits & User::SUPPORTBIT_QUICKLIST) == User::SUPPORTBIT_QUICKLIST) == false) {
           	        OtherUser->Close();
                } else {
                    OtherUser->Close(true);
                }
            }
            if(clsHashManager::mPtr->Add(curUser) == false) {
                return;
            }
            curUser->ui32BoolBits |= User::BIT_HASHED;
        }
        if(((curUser->ui32SupportBits & User::SUPPORTBIT_QUICKLIST) == User::SUPPORTBIT_QUICKLIST) == false) {
            // welcome the new user
            // PPK ... fixed bad DC protocol implementation, $LogedIn is only for OPs !!!
            // registered DC1 users have enabled OP menu :)))))))))
            int imsgLen;
            if(((curUser->ui32BoolBits & User::BIT_OPERATOR) == User::BIT_OPERATOR) == true) {
                imsgLen = sprintf(msg, "$Hello %s|$LogedIn %s|", curUser->sNick, curUser->sNick);
                if(CheckSprintf(imsgLen, 1024, "clsDcCommands::MyPass15") == false) {
                    return;
                }
            } else {
                imsgLen = sprintf(msg, "$Hello %s|", curUser->sNick);
                if(CheckSprintf(imsgLen, 1024, "clsDcCommands::MyPass16") == false) {
                    return;
                }
            }
            curUser->SendCharDelayed(msg, imsgLen);
            return;
        } else {
            if(((curUser->ui32BoolBits & User::BIT_PINGER) == User::BIT_PINGER) == false) {
                curUser->AddMeOrIPv4Check();
            }
        }
    }     
}
//---------------------------------------------------------------------------

// $OpForceMove $Who:<nickname>$Where:<iptoredirect>$Msg:<a message>
void clsDcCommands::OpForceMove(User * curUser, char * sData, const uint32_t &iLen) {
    if(clsProfileManager::mPtr->IsAllowed(curUser, clsProfileManager::REDIRECT) == false) {
        int iLen = sprintf(msg, "<%s> %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_YOU_ARE_NOT_ALWD_TO_USE_THIS_CMD]);
        if(CheckSprintf(iLen, 1024, "clsDcCommands::OpForceMove1") == true) {
            curUser->SendCharDelayed(msg, iLen);
        }
        return;
    }

    if(iLen < 31) {
        int imsgLen = sprintf(msg, "[SYS] Bad $OpForceMove (%s) from %s (%s) - user closed.", sData, curUser->sNick, curUser->sIP);
        if(CheckSprintf(imsgLen, 1024, "clsDcCommands::OpForceMove2") == true) {
            clsUdpDebug::mPtr->Broadcast(msg, imsgLen);
        }
        curUser->Close();
        return;
    }

	if(clsScriptManager::mPtr->Arrival(curUser, sData, iLen, clsScriptManager::OPFORCEMOVE_ARRIVAL) == true ||
		curUser->ui8State >= User::STATE_CLOSING) {
		return;
	}

    sData[iLen-1] = '\0'; // cutoff pipe

    char *sCmdParts[] = { NULL, NULL, NULL };
    uint16_t iCmdPartsLen[] = { 0, 0, 0 };
                
    uint8_t cPart = 0;
                
    sCmdParts[cPart] = sData+18; // nick start
                
    for(uint32_t ui32i = 18; ui32i < iLen; ui32i++) {
        if(sData[ui32i] == '$') {
            sData[ui32i] = '\0';
            iCmdPartsLen[cPart] = (uint16_t)((sData+ui32i)-sCmdParts[cPart]);
                    
            // are we on last $ ???
            if(cPart == 1) {
                sCmdParts[2] = sData+ui32i+1;
                iCmdPartsLen[2] = (uint16_t)(iLen-ui32i-1);
                break;
            }
                    
            cPart++;
            sCmdParts[cPart] = sData+ui32i+1;
        }
    }

    if(iCmdPartsLen[0] == 0 || iCmdPartsLen[1] < 7 || iCmdPartsLen[2] < 5 || iCmdPartsLen[1] > 4096 || iCmdPartsLen[2] > 16384) {
        return;
    }

    User *OtherUser = clsHashManager::mPtr->FindUser(sCmdParts[0], iCmdPartsLen[0]);
    if(OtherUser) {
   	    if(OtherUser->iProfile != -1 && curUser->iProfile > OtherUser->iProfile) {
            int imsgLen = sprintf(msg, "<%s> %s %s|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_YOU_ARE_NOT_ALLOWED_TO_REDIRECT], OtherUser->sNick);
            if(CheckSprintf(imsgLen, 1024, "clsDcCommands::OpForceMove3") == true) {
                curUser->SendCharDelayed(msg, imsgLen);
            }
            return;
        } else {
            int imsgLen = sprintf(clsServerManager::sGlobalBuffer, "<%s> %s %s %s %s. %s: %s|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_YOU_ARE_REDIRECTED_TO],
                sCmdParts[1]+6, clsLanguageManager::mPtr->sTexts[LAN_BY_LWR], curUser->sNick, clsLanguageManager::mPtr->sTexts[LAN_MESSAGE], sCmdParts[2]+4);
            if(CheckSprintf(imsgLen, clsServerManager::szGlobalBufferSize, "clsDcCommands::OpForceMove4") == true) {
                OtherUser->SendCharDelayed(clsServerManager::sGlobalBuffer, imsgLen);
            }

            imsgLen = sprintf(clsServerManager::sGlobalBuffer, "$ForceMove %s|", sCmdParts[1]+6);
            if(CheckSprintf(imsgLen, clsServerManager::szGlobalBufferSize, "clsDcCommands::OpForceMove6") == true) {
                OtherUser->SendChar(clsServerManager::sGlobalBuffer, imsgLen);
            }

            // PPK ... close user !!!
            imsgLen = sprintf(msg, "[SYS] User %s (%s) redirected by %s", OtherUser->sNick, OtherUser->sIP, curUser->sNick);
            if(CheckSprintf(imsgLen, 1024, "clsDcCommands::OpForceMove13") == true) {
                clsUdpDebug::mPtr->Broadcast(msg, imsgLen);
            }
            OtherUser->Close();

            if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES] == true) {
                if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES_AS_PM] == true) {
                    imsgLen = sprintf(clsServerManager::sGlobalBuffer, "%s $<%s> *** %s %s %s %s %s. %s: %s|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                        clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], OtherUser->sNick, clsLanguageManager::mPtr->sTexts[LAN_IS_REDIRECTED_TO], sCmdParts[1]+6,
                        clsLanguageManager::mPtr->sTexts[LAN_BY_LWR], curUser->sNick, clsLanguageManager::mPtr->sTexts[LAN_MESSAGE], sCmdParts[2]+4);
                    if(CheckSprintf(imsgLen, clsServerManager::szGlobalBufferSize, "clsDcCommands::OpForceMove7") == true) {
						clsGlobalDataQueue::mPtr->SingleItemStore(clsServerManager::sGlobalBuffer, imsgLen, NULL, 0, clsGlobalDataQueue::SI_PM2OPS);
                    }
                } else {
                    imsgLen = sprintf(clsServerManager::sGlobalBuffer, "<%s> *** %s %s %s %s %s. %s: %s|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], OtherUser->sNick,
                        clsLanguageManager::mPtr->sTexts[LAN_IS_REDIRECTED_TO], sCmdParts[1]+6, clsLanguageManager::mPtr->sTexts[LAN_BY_LWR], curUser->sNick, clsLanguageManager::mPtr->sTexts[LAN_MESSAGE], sCmdParts[2]+4);
                    if(CheckSprintf(imsgLen, clsServerManager::szGlobalBufferSize, "clsDcCommands::OpForceMove9") == true) {
						clsGlobalDataQueue::mPtr->AddQueueItem(clsServerManager::sGlobalBuffer, imsgLen, NULL, 0, clsGlobalDataQueue::CMD_OPS);
                    }
                }
            }

            if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES] == false || ((curUser->ui32BoolBits & User::BIT_OPERATOR) == User::BIT_OPERATOR) == false) {
                imsgLen = sprintf(clsServerManager::sGlobalBuffer, "<%s> *** %s %s %s. %s: %s|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], OtherUser->sNick, clsLanguageManager::mPtr->sTexts[LAN_IS_REDIRECTED_TO],
                    sCmdParts[1]+6, clsLanguageManager::mPtr->sTexts[LAN_MESSAGE], sCmdParts[2]+4);
                if(CheckSprintf(imsgLen, clsServerManager::szGlobalBufferSize, "clsDcCommands::OpForceMove11") == true) {
                    curUser->SendCharDelayed(clsServerManager::sGlobalBuffer, imsgLen);
                }
            }
        }
    }
}
//---------------------------------------------------------------------------

// $RevConnectToMe <ownnick> <nickname>
void clsDcCommands::RevConnectToMe(User * curUser, char * sData, const uint32_t &iLen, const bool &bCheck) {
    if(iLen < 19) {
        int imsgLen = sprintf(msg, "[SYS] Bad $RevConnectToMe (%s) from %s (%s) - user closed.", sData, curUser->sNick, curUser->sIP);
        if(CheckSprintf(imsgLen, 1024, "clsDcCommands::RevConnectToMe1") == true) {
            clsUdpDebug::mPtr->Broadcast(msg, imsgLen);
        }
        curUser->Close();
        return;
    }

    // PPK ... optimizations
    if((sData[16+curUser->ui8NickLen] != ' ') || (memcmp(sData+16, curUser->sNick, curUser->ui8NickLen) != 0)) {
        if(iLen > 65000) {
            sData[65000] = '\0';
        }

        int imsgLen = sprintf(clsServerManager::sGlobalBuffer, "[SYS] Nick spoofing in RCTM from %s (%s) - user closed. (%s)", curUser->sNick, curUser->sIP, sData);
        if(CheckSprintf(imsgLen, clsServerManager::szGlobalBufferSize, "clsDcCommands::RevConnectToMe5") == true) {
            clsUdpDebug::mPtr->Broadcast(clsServerManager::sGlobalBuffer, imsgLen);
        }

        curUser->Close();
        return;
    }

    // PPK ... check flood ...
    if(bCheck == true && clsProfileManager::mPtr->IsAllowed(curUser, clsProfileManager::NODEFLOODRCTM) == false) { 
        if(clsSettingManager::mPtr->iShorts[SETSHORT_RCTM_ACTION] != 0) {
            if(DeFloodCheckForFlood(curUser, DEFLOOD_RCTM, clsSettingManager::mPtr->iShorts[SETSHORT_RCTM_ACTION], 
              curUser->ui16RCTMs, curUser->ui64RCTMsTick, clsSettingManager::mPtr->iShorts[SETSHORT_RCTM_MESSAGES], 
              (uint32_t)clsSettingManager::mPtr->iShorts[SETSHORT_RCTM_TIME]) == true) {
				return;
            }
        }

        if(clsSettingManager::mPtr->iShorts[SETSHORT_RCTM_ACTION2] != 0) {
            if(DeFloodCheckForFlood(curUser, DEFLOOD_RCTM, clsSettingManager::mPtr->iShorts[SETSHORT_RCTM_ACTION2], 
              curUser->ui16RCTMs2, curUser->ui64RCTMsTick2, clsSettingManager::mPtr->iShorts[SETSHORT_RCTM_MESSAGES2], 
			  (uint32_t)clsSettingManager::mPtr->iShorts[SETSHORT_RCTM_TIME2]) == true) {
                return;
            }
        }
    }

    if(iLen > (uint32_t)clsSettingManager::mPtr->iShorts[SETSHORT_MAX_RCTM_LEN]) {
        int imsgLen = sprintf(msg, "<%s> %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_RCTM_TOO_LONG]);
        if(CheckSprintf(imsgLen, 1024, "clsDcCommands::RevConnectToMe2") == true) {
            curUser->SendCharDelayed(msg, imsgLen);
        }

        if(iLen > 65000) {
            sData[65000] = '\0';
        }

        imsgLen = sprintf(clsServerManager::sGlobalBuffer, "[SYS] Long $RevConnectToMe from %s (%s) - user closed. (%s)", curUser->sNick, curUser->sIP, sData);
        if(CheckSprintf(imsgLen, clsServerManager::szGlobalBufferSize, "clsDcCommands::RevConnectToMe3") == true) {
            clsUdpDebug::mPtr->Broadcast(clsServerManager::sGlobalBuffer, imsgLen);
        }

        curUser->Close();
        return;
    }

    // PPK ... $RCTM means user is pasive ?!? Probably yes, let set it not active and use on another places ;)
    if(curUser->sTag == NULL) {
        curUser->ui32BoolBits &= ~User::BIT_IPV4_ACTIVE;
    }

	if(clsScriptManager::mPtr->Arrival(curUser, sData, iLen, clsScriptManager::REVCONNECTTOME_ARRIVAL) == true ||
		curUser->ui8State >= User::STATE_CLOSING) {
		return;
	}

    sData[iLen-1] = '\0'; // cutoff pipe
       
    User *OtherUser = clsHashManager::mPtr->FindUser(sData+17+curUser->ui8NickLen, iLen-(18+curUser->ui8NickLen));
    // PPK ... no connection to yourself !!!
    if(OtherUser != NULL && OtherUser != curUser && OtherUser->ui8State == User::STATE_ADDED) {
        sData[iLen-1] = '|'; // add back pipe
        curUser->AddPrcsdCmd(PrcsdUsrCmd::CTM_MCTM_RCTM_SR_TO, sData, iLen, OtherUser);
    }   
}
//---------------------------------------------------------------------------

// $SR <nickname> - Search Respond for passive users
void clsDcCommands::SR(User * curUser, char * sData, const uint32_t &iLen, const bool &bCheck) {
    if(iLen < 6u+curUser->ui8NickLen) {
        int imsgLen = sprintf(msg, "[SYS] Bad $SR (%s) from %s (%s) - user closed.", sData, curUser->sNick, curUser->sIP);
        if(CheckSprintf(imsgLen, 1024, "clsDcCommands::SR1") == true) {
            clsUdpDebug::mPtr->Broadcast(msg, imsgLen);
        }
        curUser->Close();
        return;
    }

    // PPK ... check flood ...
    if(bCheck == true && clsProfileManager::mPtr->IsAllowed(curUser, clsProfileManager::NODEFLOODSR) == false) { 
        if(clsSettingManager::mPtr->iShorts[SETSHORT_SR_ACTION] != 0) {
            if(DeFloodCheckForFlood(curUser, DEFLOOD_SR, clsSettingManager::mPtr->iShorts[SETSHORT_SR_ACTION], 
              curUser->ui16SRs, curUser->ui64SRsTick, clsSettingManager::mPtr->iShorts[SETSHORT_SR_MESSAGES], 
              (uint32_t)clsSettingManager::mPtr->iShorts[SETSHORT_SR_TIME]) == true) {
				return;
            }
        }

        if(clsSettingManager::mPtr->iShorts[SETSHORT_SR_ACTION2] != 0) {
            if(DeFloodCheckForFlood(curUser, DEFLOOD_SR, clsSettingManager::mPtr->iShorts[SETSHORT_SR_ACTION2], 
              curUser->ui16SRs2, curUser->ui64SRsTick2, clsSettingManager::mPtr->iShorts[SETSHORT_SR_MESSAGES2], 
			  (uint32_t)clsSettingManager::mPtr->iShorts[SETSHORT_SR_TIME2]) == true) {
                return;
            }
        }
    }

    if(iLen > (uint32_t)clsSettingManager::mPtr->iShorts[SETSHORT_MAX_SR_LEN]) {
        int imsgLen = sprintf(msg, "<%s> %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_SR_TOO_LONG]);
        if(CheckSprintf(imsgLen, 1024, "clsDcCommands::SR2") == true) {
            curUser->SendCharDelayed(msg, imsgLen);
        }

        if(iLen > 65000) {
            sData[65000] = '\0';
        }

        imsgLen = sprintf(clsServerManager::sGlobalBuffer, "[SYS] Long $SR from %s (%s) - user closed. (%s)", curUser->sNick, curUser->sIP, sData);
        if(CheckSprintf(imsgLen, clsServerManager::szGlobalBufferSize, "clsDcCommands::SR3") == true) {
            clsUdpDebug::mPtr->Broadcast(clsServerManager::sGlobalBuffer, imsgLen);
        }

        curUser->Close();
		return;
    }

    // check $SR spoofing (thanx Fusbar)
    // PPK... added checking for empty space after nick
    if(sData[4+curUser->ui8NickLen] != ' ' || memcmp(sData+4, curUser->sNick, curUser->ui8NickLen) != 0) {
        if(iLen > 65000) {
            sData[65000] = '\0';
        }

        int imsgLen = sprintf(clsServerManager::sGlobalBuffer, "[SYS] Nick spoofing in SR from %s (%s) - user closed. (%s)", curUser->sNick, curUser->sIP, sData);
        if(CheckSprintf(imsgLen, clsServerManager::szGlobalBufferSize, "clsDcCommands::SR5") == true) {
            clsUdpDebug::mPtr->Broadcast(clsServerManager::sGlobalBuffer, imsgLen);
        }

        curUser->Close();
        return;
    }

    // is this response on SlotFetch request ?
    /*if(strcmp(towho, "SlotFetch") == 0) {
        int s,c=0;
        char *slash;
        slash = strchr(sData, '/');
        if(slash == NULL) return;

        s = atoi(slash);
        curUser->Slots = s;
        if(s < hubForm->UpDownMinSlots->Position) {
            if(clsProfileManager::mPtr->IsAllowed(curUser, ProfileMan::NOSLOTCHECK) == true) return; // Applies for OPs ?
            int imsgLen = sprintf(msg, "$To: %s From: %s $<%s> %s|", curUser->Nick, HubBotName->Text.c_str(), HubBotName->Text.c_str(), MinSlotsCheckMessage->Text.c_str());
            UserSendChar(curUser, msg, imsgLen);
            if(hubForm->MinShareRedir->Checked) {
                imsgLen = sprintf(msg, "<%s> %s|$ForceMove %s|", hubForm->HubBotName->Text.c_str(), hubForm->ShareLimitMessage->Text.c_str(), hubForm->MinShareRedirAddr->Text.c_str());
               	UserSendChar(curUser, msg, imsgLen);
            }
            int imsgLen = sprintf(msg, "[SYS] SlotFetch %s (%s) - user closed.", curUser->Nick, curUser->IP);
            clsUdpDebug::mPtr->Broadcast(msg, imsgLen);
            curUser->Close();
        }
        return;
    }*/

    // past SR to script only if it's not a data for SlotFetcher
	if(clsScriptManager::mPtr->Arrival(curUser, sData, iLen, clsScriptManager::SR_ARRIVAL) == true ||
		curUser->ui8State >= User::STATE_CLOSING) {
		return;
	}

    sData[iLen-1] = '\0'; // cutoff pipe

    char *towho = strrchr(sData, '\5');
    if(towho == NULL) return;

    User *OtherUser = clsHashManager::mPtr->FindUser(towho+1, iLen-2-(towho-sData));
    // PPK ... no $SR to yourself !!!
    if(OtherUser != NULL && OtherUser != curUser && OtherUser->ui8State == User::STATE_ADDED) {
        // PPK ... search replies limiting
        if(clsSettingManager::mPtr->iShorts[SETSHORT_MAX_PASIVE_SR] != 0) {
			if(OtherUser->iSR >= (uint32_t)clsSettingManager::mPtr->iShorts[SETSHORT_MAX_PASIVE_SR])
                return;
                        
            OtherUser->iSR++;
        }

        // cutoff the last part // PPK ... and do it fast ;)
        towho[0] = '|';
        towho[1] = '\0';
        curUser->AddPrcsdCmd(PrcsdUsrCmd::CTM_MCTM_RCTM_SR_TO, sData, iLen-OtherUser->ui8NickLen-1, OtherUser);
    }   
}

//---------------------------------------------------------------------------

// $SR <nickname> - Search Respond for active users from UDP
void clsDcCommands::SRFromUDP(User * curUser, char * sData, const size_t &szLen) {
	if(clsScriptManager::mPtr->Arrival(curUser, sData, szLen, clsScriptManager::UDP_SR_ARRIVAL) == true ||
		curUser->ui8State >= User::STATE_CLOSING) {
		return;
	}
}
//---------------------------------------------------------------------------

// $Supports item item item... PPK $Supports UserCommand NoGetINFO NoHello UserIP2 QuickList|
void clsDcCommands::Supports(User * curUser, char * sData, const uint32_t &iLen) {
    if(((curUser->ui32BoolBits & User::BIT_HAVE_SUPPORTS) == User::BIT_HAVE_SUPPORTS) == true) {
        int imsgLen = sprintf(msg, "[SYS] $Supports flood from %s (%s) - user closed.", curUser->sNick, curUser->sIP);
        if(CheckSprintf(imsgLen, 1024, "clsDcCommands::Supports1") == true) {
            clsUdpDebug::mPtr->Broadcast(msg, imsgLen);
        }
        curUser->Close();
        return;
    }

    curUser->ui32BoolBits |= User::BIT_HAVE_SUPPORTS;
    
    if(iLen < 13) {
        int imsgLen = sprintf(msg, "[SYS] Bad $Supports (%s) from %s (%s) - user closed.", sData, curUser->sNick, curUser->sIP);
        if(CheckSprintf(imsgLen, 1024, "clsDcCommands::Supports2") == true) {
            clsUdpDebug::mPtr->Broadcast(msg, imsgLen);
        }
        curUser->Close();
        return;
    }

	// alex82 ... Удалили бессмысленную опцию "Отключать клиенты, отправляющие $Supports с ошибками", поскольку "ошибками" считается только лишний пробел в конце команды.

	clsScriptManager::mPtr->Arrival(curUser, sData, iLen, clsScriptManager::SUPPORTS_ARRIVAL);

	if(curUser->ui8State >= User::STATE_CLOSING) {
    	return;
    }

    char *sSupport = sData+10;
	size_t szDataLen;
                    
    for(uint32_t ui32i = 10; ui32i < iLen-1; ui32i++) {
        if(sData[ui32i] == ' ') {
            sData[ui32i] = '\0';
        } else if(ui32i != iLen-2) {
            continue;
        } else {
            ui32i++;
        }

        szDataLen = (sData+ui32i)-sSupport;

        switch(sSupport[0]) {
            case 'N':
                if(sSupport[1] == 'o') {
                    if(((curUser->ui32SupportBits & User::SUPPORTBIT_NOHELLO) == User::SUPPORTBIT_NOHELLO) == false && szDataLen == 7 && memcmp(sSupport+2, "Hello", 5) == 0) {
                        curUser->ui32SupportBits |= User::SUPPORTBIT_NOHELLO;
                    } else if(((curUser->ui32SupportBits & User::SUPPORTBIT_NOGETINFO) == User::SUPPORTBIT_NOGETINFO) == false && szDataLen == 9 && memcmp(sSupport+2, "GetINFO", 7) == 0) {
                        curUser->ui32SupportBits |= User::SUPPORTBIT_NOGETINFO;
                    }
                }
                break;
            case 'Q': {
                if(((curUser->ui32SupportBits & User::SUPPORTBIT_QUICKLIST) == User::SUPPORTBIT_QUICKLIST) == false && szDataLen == 9 && *((uint64_t *)(sSupport+1)) == *((uint64_t *)"uickList")) {
                    curUser->ui32SupportBits |= User::SUPPORTBIT_QUICKLIST;
                    // PPK ... in fact NoHello is only not fully implemented Quicklist (without diferent login sequency)
                    // That's why i overide NoHello here and use bQuicklist only for login, on other places is same as NoHello ;)
                    curUser->ui32SupportBits |= User::SUPPORTBIT_NOHELLO;
                }
                break;
            }
            case 'U': {
                if(((curUser->ui32SupportBits & User::SUPPORTBIT_USERCOMMAND) == User::SUPPORTBIT_USERCOMMAND) == false && szDataLen == 11 && memcmp(sSupport+1, "serCommand", 10) == 0) {
                    curUser->ui32SupportBits |= User::SUPPORTBIT_USERCOMMAND;
                } else if(((curUser->ui32SupportBits & User::SUPPORTBIT_USERIP2) == User::SUPPORTBIT_USERIP2) == false && szDataLen == 7 && memcmp(sSupport+1, "serIP2", 6) == 0) {
                    curUser->ui32SupportBits |= User::SUPPORTBIT_USERIP2;
                }
                break;
            }
            case 'B': {
                if(((curUser->ui32BoolBits & User::BIT_PINGER) == User::BIT_PINGER) == false && szDataLen == 7 && memcmp(sSupport+1, "otINFO", 6) == 0) {
                    if(clsSettingManager::mPtr->bBools[SETBOOL_DONT_ALLOW_PINGERS] == true) {
                        int imsgLen = sprintf(msg, "<%s> %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_SORRY_THIS_HUB_NOT_ALLOW_PINGERS]);
                        if(CheckSprintf(imsgLen, 1024, "clsDcCommands::Supports6") == true) {
                            curUser->SendChar(msg, imsgLen);
                        }
                        curUser->Close();
                        return;
                    } else if(clsSettingManager::mPtr->bBools[SETBOOL_CHECK_IP_IN_COMMANDS] == false) {
                        int imsgLen = sprintf(msg, "<%s> Sorry, this hub banned yourself from hublist because allow CTM exploit.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC]);
                        if(CheckSprintf(imsgLen, 1024, "clsDcCommands::Supports7") == true) {
                            curUser->SendChar(msg, imsgLen);
                        }
                        curUser->Close();
                        return;
                    } else {
                        curUser->ui32BoolBits |= User::BIT_PINGER;
                        int imsgLen = GetWlcmMsg(msg);
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
                }
                break;
            }
            case 'Z': {
                if(((curUser->ui32SupportBits & User::SUPPORTBIT_ZPIPE) == User::SUPPORTBIT_ZPIPE) == false && ((szDataLen == 5 && *((uint32_t *)(sSupport+1)) == *((uint32_t *)"Pipe")) ||
                    (szDataLen == 6 && memcmp(sSupport+1, "Pipe0", 5) == 0))) {
                    curUser->ui32SupportBits |= User::SUPPORTBIT_ZPIPE;
                    iStatZPipe++;
                }
                break;
            }
            case 'I': {
                if(szDataLen == 4) {
                    if(((curUser->ui32SupportBits & User::SUPPORTBIT_IP64) == User::SUPPORTBIT_IP64) == false && *((uint32_t *)sSupport) == *((uint32_t *)"IP64")) {
                        curUser->ui32SupportBits |= User::SUPPORTBIT_IP64;
                    } else if(((curUser->ui32SupportBits & User::SUPPORTBIT_IPV4) == User::SUPPORTBIT_IPV4) == false && *((uint32_t *)sSupport) == *((uint32_t *)"IPv4")) {
                        curUser->ui32SupportBits |= User::SUPPORTBIT_IPV4;
                    }
                }
                break;
            }
            case '\0': {
                // PPK ... corrupted $Supports ???
                int imsgLen = sprintf(msg, "[SYS] Bad $Supports from %s (%s) - user closed.", curUser->sNick, curUser->sIP);
                if(CheckSprintf(imsgLen, 1024, "clsDcCommands::Supports8") == true) {
                    clsUdpDebug::mPtr->Broadcast(msg, imsgLen);
                }
                curUser->Close();
                return;
            }
            default:
                // PPK ... unknown supports
                break;
        }
                
        sSupport = sData+ui32i+1;
    }
    
    curUser->ui8State = User::STATE_VALIDATE;
    
    curUser->AddPrcsdCmd(PrcsdUsrCmd::SUPPORTS, NULL, 0, NULL);
}
//---------------------------------------------------------------------------

// $To: nickname From: ownnickname $<ownnickname> <message>
void clsDcCommands::To(User * curUser, char * sData, const uint32_t &iLen, const bool &bCheck) {
    char *cTemp = strchr(sData+5, ' ');

    if(iLen < 19 || cTemp == NULL) {
        if(iLen > 65000) {
            sData[65000] = '\0';
        }

        int imsgLen = sprintf(clsServerManager::sGlobalBuffer, "[SYS] Bad To from %s (%s) - user closed. (%s)", curUser->sNick, curUser->sIP, sData);
        if(CheckSprintf(imsgLen, clsServerManager::szGlobalBufferSize, "clsDcCommands::To1") == true) {
            clsUdpDebug::mPtr->Broadcast(clsServerManager::sGlobalBuffer, imsgLen);
        }

        curUser->Close();
        return;
    }
    
    size_t szNickLen = cTemp-(sData+5);

    if(szNickLen > 64) {
        int imsgLen = sprintf(msg, "<%s> *** %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_MAX_ALWD_NICK_LEN_64_CHARS]);
        if(CheckSprintf(imsgLen, 1024, "clsDcCommands::To3") == true) {
            curUser->SendCharDelayed(msg, imsgLen);
        }
        return;
    }

    // is the mesg really from us ?
    // PPK ... replaced by better and faster code ;)
    int imsgLen = sprintf(msg, "From: %s $<%s> ", curUser->sNick, curUser->sNick);
    if(CheckSprintf(imsgLen, 1024, "clsDcCommands::To4") == true) {
        if(strncmp(cTemp+1, msg, imsgLen) != 0) {
            if(iLen > 65000) {
                sData[65000] = '\0';
            }

            int imsgLen = sprintf(clsServerManager::sGlobalBuffer, "[SYS] Nick spoofing in To from %s (%s) - user closed. (%s)", curUser->sNick, curUser->sIP, sData);
            if(CheckSprintf(imsgLen, clsServerManager::szGlobalBufferSize, "clsDcCommands::To5") == true) {
                clsUdpDebug::mPtr->Broadcast(clsServerManager::sGlobalBuffer, imsgLen);
            }

            curUser->Close();
            return;
        }
    }

    //FloodCheck
    if(bCheck == true && clsProfileManager::mPtr->IsAllowed(curUser, clsProfileManager::NODEFLOODPM) == false) {
        // PPK ... pm antiflood
        if(clsSettingManager::mPtr->iShorts[SETSHORT_PM_ACTION] != 0) {
            cTemp[0] = '\0';
            if(DeFloodCheckForFlood(curUser, DEFLOOD_PM, clsSettingManager::mPtr->iShorts[SETSHORT_PM_ACTION], 
                curUser->ui16PMs, curUser->ui64PMsTick, clsSettingManager::mPtr->iShorts[SETSHORT_PM_MESSAGES], 
                (uint32_t)clsSettingManager::mPtr->iShorts[SETSHORT_PM_TIME], sData+5) == true) {
                return;
            }
            cTemp[0] = ' ';
        }

        if(clsSettingManager::mPtr->iShorts[SETSHORT_PM_ACTION2] != 0) {
            cTemp[0] = '\0';
            if(DeFloodCheckForFlood(curUser, DEFLOOD_PM, clsSettingManager::mPtr->iShorts[SETSHORT_PM_ACTION2], 
                curUser->ui16PMs2, curUser->ui64PMsTick2, clsSettingManager::mPtr->iShorts[SETSHORT_PM_MESSAGES2], 
                (uint32_t)clsSettingManager::mPtr->iShorts[SETSHORT_PM_TIME2], sData+5) == true) {
                return;
            }
            cTemp[0] = ' ';
        }

        // 2nd check for PM flooding
		if(clsSettingManager::mPtr->iShorts[SETSHORT_SAME_PM_ACTION] != 0) {
			bool bNewData = false;
			cTemp[0] = '\0';
			if(DeFloodCheckForSameFlood(curUser, DEFLOOD_SAME_PM, clsSettingManager::mPtr->iShorts[SETSHORT_SAME_PM_ACTION],
                curUser->ui16SamePMs, curUser->ui64SamePMsTick, 
                clsSettingManager::mPtr->iShorts[SETSHORT_SAME_PM_MESSAGES], clsSettingManager::mPtr->iShorts[SETSHORT_SAME_PM_TIME], 
                cTemp+(12+(2*curUser->ui8NickLen)), (iLen-(cTemp-sData))-(12+(2*curUser->ui8NickLen)), 
				curUser->sLastPM, curUser->ui16LastPMLen, bNewData, sData+5) == true) {
                return;
            }
            cTemp[0] = ' ';

			if(bNewData == true) {
				curUser->SetLastPM(cTemp+(12+(2*curUser->ui8NickLen)), (iLen-(cTemp-sData))-(12+(2*curUser->ui8NickLen)));
			}
        }
    }

    if(clsProfileManager::mPtr->IsAllowed(curUser, clsProfileManager::NOCHATLIMITS) == false) {
        // 1st check for length limit for PM message
        size_t szMessLen = iLen-(2*curUser->ui8NickLen)-(cTemp-sData)-13;
        if(clsSettingManager::mPtr->iShorts[SETSHORT_MAX_PM_LEN] != 0 && szMessLen > (size_t)clsSettingManager::mPtr->iShorts[SETSHORT_MAX_PM_LEN]) {
       	    // PPK ... hubsec alias
       	    cTemp[0] = '\0';
       	   	int imsgLen = sprintf(msg, "$To: %s From: %s $<%s> %s!|", curUser->sNick, sData+5, clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                clsLanguageManager::mPtr->sTexts[LAN_THE_MESSAGE_WAS_TOO_LONG]);
            if(CheckSprintf(imsgLen, 1024, "clsDcCommands::To14") == true) {
               	curUser->SendCharDelayed(msg, imsgLen);
            }
            return;
        }

        // PPK ... check for message lines limit
        if(clsSettingManager::mPtr->iShorts[SETSHORT_MAX_PM_LINES] != 0 || clsSettingManager::mPtr->iShorts[SETSHORT_SAME_MULTI_PM_ACTION] != 0) {
			if(curUser->ui16SamePMs < 2) {
				uint16_t iLines = 1;
                for(uint32_t ui32i = 9+curUser->ui8NickLen; ui32i < iLen-(cTemp-sData)-1; ui32i++) {
                    if(cTemp[ui32i] == '\n') {
                        iLines++;
                    }
                }
                curUser->ui16LastPmLines = iLines;
                if(curUser->ui16LastPmLines > 1) {
                    curUser->ui16SameMultiPms++;
                }
            } else if(curUser->ui16LastPmLines > 1) {
                curUser->ui16SameMultiPms++;
            }

			if(clsProfileManager::mPtr->IsAllowed(curUser, clsProfileManager::NODEFLOODPM) == false && clsSettingManager::mPtr->iShorts[SETSHORT_SAME_MULTI_PM_ACTION] != 0) {
				if(curUser->ui16SameMultiPms > clsSettingManager::mPtr->iShorts[SETSHORT_SAME_MULTI_PM_MESSAGES] &&
                    curUser->ui16LastPmLines >= clsSettingManager::mPtr->iShorts[SETSHORT_SAME_MULTI_PM_LINES]) {
					cTemp[0] = '\0';
					uint16_t lines = 0;
					DeFloodDoAction(curUser, DEFLOOD_SAME_MULTI_PM, clsSettingManager::mPtr->iShorts[SETSHORT_SAME_MULTI_PM_ACTION], lines, sData+5);
                    return;
                }
            }

            if(clsSettingManager::mPtr->iShorts[SETSHORT_MAX_PM_LINES] != 0 && curUser->ui16LastPmLines > clsSettingManager::mPtr->iShorts[SETSHORT_MAX_PM_LINES]) {
                cTemp[0] = '\0';
                int imsgLen = sprintf(msg, "$To: %s From: %s $<%s> %s!|", curUser->sNick, sData+5, clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                    clsLanguageManager::mPtr->sTexts[LAN_THE_MESSAGE_WAS_TOO_LONG]);
                if(CheckSprintf(imsgLen, 1024, "clsDcCommands::To15") == true) {
                    curUser->SendCharDelayed(msg, imsgLen);
                }
                return;
            }
        }
    }

    if(bCheck == true && clsProfileManager::mPtr->IsAllowed(curUser, clsProfileManager::NOPMINTERVAL) == false) {
        cTemp[0] = '\0';
        if(DeFloodCheckInterval(curUser, INTERVAL_PM, curUser->ui16PMsInt, 
            curUser->ui64PMsIntTick, clsSettingManager::mPtr->iShorts[SETSHORT_PM_INTERVAL_MESSAGES], 
            (uint32_t)clsSettingManager::mPtr->iShorts[SETSHORT_PM_INTERVAL_TIME], sData+5) == true) {
            return;
        }
        cTemp[0] = ' ';
    }

	if(clsScriptManager::mPtr->Arrival(curUser, sData, iLen, clsScriptManager::TO_ARRIVAL) == true ||
		curUser->ui8State >= User::STATE_CLOSING) {
		return;
	}

    cTemp[0] = '\0';

    // ignore the silly debug messages !!!
    if(memcmp(sData+5, "Vandel\\Debug", 12) == 0) {
        return;
    }

    // Everything's ok lets chat
    // if this is a PM to OpChat or Hub bot, process the message
    if(clsSettingManager::mPtr->bBools[SETBOOL_REG_BOT] == true && strcmp(sData+5, clsSettingManager::mPtr->sTexts[SETTXT_BOT_NICK]) == 0) {
        cTemp += 9+curUser->ui8NickLen;
        // PPK ... check message length, return if no mess found
        uint32_t iLen1 = (uint32_t)((iLen-(cTemp-sData))+1);
        if(iLen1 <= curUser->ui8NickLen+4u)
            return;

        // find chat message data
        char *sBuff = cTemp+curUser->ui8NickLen+3;

        // non-command chat msg
        for(uint8_t ui8i = 0; ui8i < (uint8_t)clsSettingManager::mPtr->ui16TextsLens[SETTXT_CHAT_COMMANDS_PREFIXES]; ui8i++) {
            if(sBuff[0] == clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][ui8i]) {
                sData[iLen-1] = '\0'; // cutoff pipe
                // built-in commands
                if(clsSettingManager::mPtr->bBools[SETBOOL_ENABLE_TEXT_FILES] == true && 
                    clsTextFilesManager::mPtr->ProcessTextFilesCmd(curUser, sBuff+1, true)) {
                    return;
                }
           
                // HubCommands
                if(iLen1-curUser->ui8NickLen >= 10) {
                    if(clsHubCommands::DoCommand(curUser, sBuff, iLen1, true)) return;
                }
                        
                sData[iLen-1] = '|'; // add back pipe
                break;
            }
        }
        // PPK ... if i am here is not textfile request or hub command, try opchat
        if(clsSettingManager::mPtr->bBools[SETBOOL_REG_OP_CHAT] == true && clsProfileManager::mPtr->IsAllowed(curUser, clsProfileManager::ALLOWEDOPCHAT) == true && 
            clsSettingManager::mPtr->bBotsSameNick == true) {
            uint32_t iOpChatLen = clsSettingManager::mPtr->ui16TextsLens[SETTXT_OP_CHAT_NICK];
            memcpy(cTemp-iOpChatLen-2, clsSettingManager::mPtr->sTexts[SETTXT_OP_CHAT_NICK], iOpChatLen);
            curUser->AddPrcsdCmd(PrcsdUsrCmd::TO_OP_CHAT, cTemp-iOpChatLen-2, iLen-((cTemp-iOpChatLen-2)-sData), NULL);
        }
    } else if(clsSettingManager::mPtr->bBools[SETBOOL_REG_OP_CHAT] == true && 
        clsProfileManager::mPtr->IsAllowed(curUser, clsProfileManager::ALLOWEDOPCHAT) == true && 
        strcmp(sData+5, clsSettingManager::mPtr->sTexts[SETTXT_OP_CHAT_NICK]) == 0) {
        cTemp += 9+curUser->ui8NickLen;
        uint32_t iOpChatLen = clsSettingManager::mPtr->ui16TextsLens[SETTXT_OP_CHAT_NICK];
        memcpy(cTemp-iOpChatLen-2, clsSettingManager::mPtr->sTexts[SETTXT_OP_CHAT_NICK], iOpChatLen);
        curUser->AddPrcsdCmd(PrcsdUsrCmd::TO_OP_CHAT, cTemp-iOpChatLen-2, (iLen-(cTemp-sData))+iOpChatLen+2, NULL);
    } else {       
        User *OtherUser = clsHashManager::mPtr->FindUser(sData+5, szNickLen);
        // PPK ... pm to yourself ?!? NO!
        if(OtherUser != NULL && OtherUser != curUser && OtherUser->ui8State == User::STATE_ADDED) {
            cTemp[0] = ' ';
            curUser->AddPrcsdCmd(PrcsdUsrCmd::CTM_MCTM_RCTM_SR_TO, sData, iLen, OtherUser, true);
        }
    }
}
//---------------------------------------------------------------------------

// $ValidateNick
void clsDcCommands::ValidateNick(User * curUser, char * sData, const uint32_t &iLen) {
    if(((curUser->ui32SupportBits & User::SUPPORTBIT_QUICKLIST) == User::SUPPORTBIT_QUICKLIST) == true) {
        int imsgLen = sprintf(msg, "[SYS] $ValidateNick with QuickList support from %s (%s) - user closed.", curUser->sNick, curUser->sIP);
        if(CheckSprintf(imsgLen, 1024, "clsDcCommands::ValidateNick1") == true) {
            clsUdpDebug::mPtr->Broadcast(msg, imsgLen);
        }
        curUser->Close();
        return;
    }

    if(iLen < 16) {
        int imsgLen = sprintf(msg, "[SYS] Attempt to Validate empty nick (%s) from %s (%s) - user closed.", sData, curUser->sNick, curUser->sIP);
        if(CheckSprintf(imsgLen, 1024, "clsDcCommands::ValidateNick3") == true) {
            clsUdpDebug::mPtr->Broadcast(msg, imsgLen);
        }
        curUser->Close();
        return;
    }

    sData[iLen-1] = '\0'; // cutoff pipe
    if(ValidateUserNick(curUser, sData+14, iLen-15, true) == false) return;

	sData[iLen-1] = '|'; // add back pipe

	clsScriptManager::mPtr->Arrival(curUser, sData, iLen, clsScriptManager::VALIDATENICK_ARRIVAL);
}
//---------------------------------------------------------------------------

// $Version
void clsDcCommands::Version(User * curUser, char * sData, const uint32_t &iLen) {
    if(iLen < 11) {
        int imsgLen = sprintf(msg, "[SYS] Bad $Version (%s) from %s (%s) - user closed.", sData, curUser->sNick, curUser->sIP);
        if(CheckSprintf(imsgLen, 1024, "clsDcCommands::Version3") == true) {
            clsUdpDebug::mPtr->Broadcast(msg, imsgLen);
        }
        curUser->Close();
        return;
    }

	curUser->ui8State = User::STATE_GETNICKLIST_OR_MYINFO;

	clsScriptManager::mPtr->Arrival(curUser, sData, iLen, clsScriptManager::VERSION_ARRIVAL);

	if(curUser->ui8State >= User::STATE_CLOSING) {
    	return;
    }
    
    sData[iLen-1] = '\0'; // cutoff pipe
    curUser->SetVersion(sData+9);
}
//---------------------------------------------------------------------------

// Chat message
bool clsDcCommands::ChatDeflood(User * curUser, char * sData, const uint32_t &iLen, const bool &bCheck) {
#ifdef _BUILD_GUI
    if(::SendMessage(clsMainWindowPageUsersChat::mPtr->hWndPageItems[clsMainWindowPageUsersChat::BTN_SHOW_CHAT], BM_GETCHECK, 0, 0) == BST_CHECKED) {
        sData[iLen - 1] = '\0';
        RichEditAppendText(clsMainWindowPageUsersChat::mPtr->hWndPageItems[clsMainWindowPageUsersChat::REDT_CHAT], sData);
        sData[iLen - 1] = '|';
    }
#endif
    
	// if the user is sending chat as other user, kick him
	if(sData[1+curUser->ui8NickLen] != '>' || sData[2+curUser->ui8NickLen] != ' ' || memcmp(curUser->sNick, sData+1, curUser->ui8NickLen) != 0) {
        if(iLen > 65000) {
            sData[65000] = '\0';
        }

        int imsgLen = sprintf(clsServerManager::sGlobalBuffer, "[SYS] Nick spoofing in chat from %s (%s) - user closed. (%s)", curUser->sNick, curUser->sIP, sData);
        if(CheckSprintf(imsgLen, clsServerManager::szGlobalBufferSize, "clsDcCommands::ChatDeflood1") == true) {
            clsUdpDebug::mPtr->Broadcast(clsServerManager::sGlobalBuffer, imsgLen);
        }

		curUser->Close();
		return false;
	}
        
	// PPK ... check flood...
	if(bCheck == true && clsProfileManager::mPtr->IsAllowed(curUser, clsProfileManager::NODEFLOODMAINCHAT) == false) {
		if(clsSettingManager::mPtr->iShorts[SETSHORT_MAIN_CHAT_ACTION] != 0) {
            if(DeFloodCheckForFlood(curUser, DEFLOOD_CHAT, clsSettingManager::mPtr->iShorts[SETSHORT_MAIN_CHAT_ACTION], 
                curUser->ui16ChatMsgs, curUser->ui64ChatMsgsTick, clsSettingManager::mPtr->iShorts[SETSHORT_MAIN_CHAT_MESSAGES], 
                (uint32_t)clsSettingManager::mPtr->iShorts[SETSHORT_MAIN_CHAT_TIME]) == true) {
                return false;
            }
		}

		if(clsSettingManager::mPtr->iShorts[SETSHORT_MAIN_CHAT_ACTION2] != 0) {
            if(DeFloodCheckForFlood(curUser, DEFLOOD_CHAT, clsSettingManager::mPtr->iShorts[SETSHORT_MAIN_CHAT_ACTION2], 
                curUser->ui16ChatMsgs2, curUser->ui64ChatMsgsTick2, clsSettingManager::mPtr->iShorts[SETSHORT_MAIN_CHAT_MESSAGES2], 
                (uint32_t)clsSettingManager::mPtr->iShorts[SETSHORT_MAIN_CHAT_TIME2]) == true) {
                return false;
            }
		}

		// 2nd check for chatmessage flood
		if(clsSettingManager::mPtr->iShorts[SETSHORT_SAME_MAIN_CHAT_ACTION] != 0) {
			bool bNewData = false;
			if(DeFloodCheckForSameFlood(curUser, DEFLOOD_SAME_CHAT, clsSettingManager::mPtr->iShorts[SETSHORT_SAME_MAIN_CHAT_ACTION],
			  curUser->ui16SameChatMsgs, curUser->ui64SameChatsTick,
              clsSettingManager::mPtr->iShorts[SETSHORT_SAME_MAIN_CHAT_MESSAGES], clsSettingManager::mPtr->iShorts[SETSHORT_SAME_MAIN_CHAT_TIME], 
			  sData+curUser->ui8NickLen+3, iLen-(curUser->ui8NickLen+3), curUser->sLastChat, curUser->ui16LastChatLen, bNewData) == true) {
				return false;
			}

			if(bNewData == true) {
				curUser->SetLastChat(sData+curUser->ui8NickLen+3, iLen-(curUser->ui8NickLen+3));
			}
        }
    }

    // PPK ... ignore empty chat ;)
    if(iLen < curUser->ui8NickLen+5u) {
        return false;
    }

    return true;
}
//---------------------------------------------------------------------------

// Chat message
void clsDcCommands::Chat(User * curUser, char * sData, const uint32_t &iLen, const bool &bCheck) {
    if(bCheck == true && clsProfileManager::mPtr->IsAllowed(curUser, clsProfileManager::NOCHATLIMITS) == false) {
    	// PPK ... check for message limit length
 	    if(clsSettingManager::mPtr->iShorts[SETSHORT_MAX_CHAT_LEN] != 0 && (iLen-curUser->ui8NickLen-4) > (uint32_t)clsSettingManager::mPtr->iShorts[SETSHORT_MAX_CHAT_LEN]) {
     		// PPK ... hubsec alias
	   		int imsgLen = sprintf(msg, "<%s> %s !|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_THE_MESSAGE_WAS_TOO_LONG]);
	   		if(CheckSprintf(imsgLen, 1024, "clsDcCommands::Chat1") == true) {
                curUser->SendCharDelayed(msg, imsgLen);
            }
	        return;
 	    }

        // PPK ... check for message lines limit
        if(clsSettingManager::mPtr->iShorts[SETSHORT_MAX_CHAT_LINES] != 0 || clsSettingManager::mPtr->iShorts[SETSHORT_SAME_MULTI_MAIN_CHAT_ACTION] != 0) {
            if(curUser->ui16SameChatMsgs < 2) {
                uint16_t iLines = 1;

                for(uint32_t ui32i = curUser->ui8NickLen+3; ui32i < iLen-1; ui32i++) {
                    if(sData[ui32i] == '\n') {
                        iLines++;
                    }
                }

                curUser->ui16LastChatLines = iLines;

                if(curUser->ui16LastChatLines > 1) {
                    curUser->ui16SameMultiChats++;
                }
			} else if(curUser->ui16LastChatLines > 1) {
                curUser->ui16SameMultiChats++;
            }

			if(clsProfileManager::mPtr->IsAllowed(curUser, clsProfileManager::NODEFLOODMAINCHAT) == false && clsSettingManager::mPtr->iShorts[SETSHORT_SAME_MULTI_MAIN_CHAT_ACTION] != 0) {
                if(curUser->ui16SameMultiChats > clsSettingManager::mPtr->iShorts[SETSHORT_SAME_MULTI_MAIN_CHAT_MESSAGES] && 
                    curUser->ui16LastChatLines >= clsSettingManager::mPtr->iShorts[SETSHORT_SAME_MULTI_MAIN_CHAT_LINES]) {
                    uint16_t lines = 0;
					DeFloodDoAction(curUser, DEFLOOD_SAME_MULTI_CHAT, clsSettingManager::mPtr->iShorts[SETSHORT_SAME_MULTI_MAIN_CHAT_ACTION], lines, NULL);
					return;
				}
			}

			if(clsSettingManager::mPtr->iShorts[SETSHORT_MAX_CHAT_LINES] != 0 && curUser->ui16LastChatLines > clsSettingManager::mPtr->iShorts[SETSHORT_MAX_CHAT_LINES]) {
                int imsgLen = sprintf(msg, "<%s> %s !|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_THE_MESSAGE_WAS_TOO_LONG]);
                if(CheckSprintf(imsgLen, 1024, "clsDcCommands::Chat2") == true) {
                    curUser->SendCharDelayed(msg, imsgLen);
                }
                return;
			}
		}
	}

    if(bCheck == true && clsProfileManager::mPtr->IsAllowed(curUser, clsProfileManager::NOCHATINTERVAL) == false) {
        if(DeFloodCheckInterval(curUser, INTERVAL_CHAT, curUser->ui16ChatIntMsgs, 
            curUser->ui64ChatIntMsgsTick, clsSettingManager::mPtr->iShorts[SETSHORT_CHAT_INTERVAL_MESSAGES], 
            (uint32_t)clsSettingManager::mPtr->iShorts[SETSHORT_CHAT_INTERVAL_TIME]) == true) {
            return;
        }
    }

    if(((curUser->ui32BoolBits & User::BIT_GAGGED) == User::BIT_GAGGED) == true)
        return;

    void * pQueueItem1 = clsGlobalDataQueue::mPtr->GetLastQueueItem();

	if(clsScriptManager::mPtr->Arrival(curUser, sData, iLen, clsScriptManager::CHAT_ARRIVAL) == true ||
		curUser->ui8State >= User::STATE_CLOSING) {
		return;
	}

    User * pToUser = NULL;
    void * pQueueItem2 = clsGlobalDataQueue::mPtr->GetLastQueueItem();

    if(pQueueItem1 != pQueueItem2) {
        if(pQueueItem1 == NULL) {
            pToUser = (User *)clsGlobalDataQueue::mPtr->InsertBlankQueueItem(clsGlobalDataQueue::mPtr->GetFirstQueueItem(), clsGlobalDataQueue::CMD_CHAT);
        } else {
            pToUser = (User *)clsGlobalDataQueue::mPtr->InsertBlankQueueItem(pQueueItem1, clsGlobalDataQueue::CMD_CHAT);
        }

        if(pToUser != NULL) {
            curUser->ui32BoolBits |= User::BIT_CHAT_INSERT;
        }
    } else if((curUser->ui32BoolBits & User::BIT_CHAT_INSERT) == User::BIT_CHAT_INSERT) {
        pToUser = (User *)clsGlobalDataQueue::mPtr->InsertBlankQueueItem(pQueueItem1, clsGlobalDataQueue::CMD_CHAT);
    }

	// PPK ... filtering kick messages
	if(clsProfileManager::mPtr->IsAllowed(curUser, clsProfileManager::KICK) == true) {
    	if(iLen > curUser->ui8NickLen+21u) {
            char * cTemp = strchr(sData+curUser->ui8NickLen+3, '\n');
            if(cTemp != NULL) {
            	cTemp[0] = '\0';
            }

            char *temp, *temp1;
            if((temp = stristr(sData+curUser->ui8NickLen+3, "is kicking ")) != NULL &&
            	(temp1 = stristr(temp+12, " because: ")) != NULL) {
                // PPK ... catch kick message and store for later use in $Kick for tempban reason
                temp1[0] = '\0';               
                User * KickedUser = clsHashManager::mPtr->FindUser(temp+11, temp1-(temp+11));
                temp1[0] = ' ';
                if(KickedUser != NULL) {
                    // PPK ... valid kick messages for existing user, remove this message from deflood ;)
                    if(curUser->ui16ChatMsgs != 0) {
                        curUser->ui16ChatMsgs--;
                        curUser->ui16ChatMsgs2--;
                    }
                    if(temp1[10] != '|') {
                        sData[iLen-1] = '\0'; // get rid of the pipe
                        KickedUser->SetBuffer(temp1+10, iLen-(temp1-sData)-11);
                        sData[iLen-1] = '|'; // add back pipe
                    }
                }

            	if(cTemp != NULL) {
                    cTemp[0] = '\n';
                }

                // PPK ... kick messages filtering
                if(clsSettingManager::mPtr->bBools[SETBOOL_FILTER_KICK_MESSAGES] == true) {
                	if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_KICK_MESSAGES_TO_OPS] == true) {
               			if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES_AS_PM] == true) {
                            if(iLen > 65000) {
                                sData[65000] = '\0';
                            }

                            int imsgLen = sprintf(clsServerManager::sGlobalBuffer, "%s $%s", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], sData);
                            if(CheckSprintf(imsgLen, clsServerManager::szGlobalBufferSize, "clsDcCommands::Chat3") == true) {
								clsGlobalDataQueue::mPtr->SingleItemStore(clsServerManager::sGlobalBuffer, imsgLen, NULL, 0, clsGlobalDataQueue::SI_PM2OPS);
                            }
                        } else {
							clsGlobalDataQueue::mPtr->AddQueueItem(sData, iLen, NULL, 0, clsGlobalDataQueue::CMD_OPS);
                        }
		    		} else {
                        curUser->SendCharDelayed(sData, iLen);
                    }
                    return;
                }
            }

            if(cTemp != NULL) {
                cTemp[0] = '\n';
            }
        }        
	}

    curUser->AddPrcsdCmd(PrcsdUsrCmd::CHAT, sData, iLen, pToUser);
}
//---------------------------------------------------------------------------

// $Close nick|
void clsDcCommands::Close(User * curUser, char * sData, const uint32_t &iLen) {
    if(clsProfileManager::mPtr->IsAllowed(curUser, clsProfileManager::CLOSE) == false) {
        int iLen = sprintf(msg, "<%s> %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_YOU_ARE_NOT_ALWD_TO_USE_THIS_CMD]);
        if(CheckSprintf(iLen, 1024, "clsDcCommands::Close1") == true) {
            curUser->SendCharDelayed(msg, iLen);
        }
        return;
    } 

    if(iLen < 9) {
        int imsgLen = sprintf(msg, "[SYS] Bad $Close (%s) from %s (%s) - user closed.", sData, curUser->sNick, curUser->sIP);
        if(CheckSprintf(imsgLen, 1024, "clsDcCommands::Close2") == true) {
            clsUdpDebug::mPtr->Broadcast(msg, imsgLen);
        }
        curUser->Close();
        return;
    }

	if(clsScriptManager::mPtr->Arrival(curUser, sData, iLen, clsScriptManager::CLOSE_ARRIVAL) == true ||
		curUser->ui8State >= User::STATE_CLOSING) {
		return;
	}

    sData[iLen-1] = '\0'; // cutoff pipe

    User *OtherUser = clsHashManager::mPtr->FindUser(sData+7, iLen-8);
    if(OtherUser != NULL) {
        // Self-kick
        if(OtherUser == curUser) {
        	int imsgLen = sprintf(msg, "<%s> %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_YOU_CANT_CLOSE_YOURSELF]);
        	if(CheckSprintf(imsgLen, 1024, "clsDcCommands::Close3") == true) {
                curUser->SendCharDelayed(msg, imsgLen);
            }
            return;
    	}
    	
        if(OtherUser->iProfile != -1 && curUser->iProfile > OtherUser->iProfile) {
        	int imsgLen = sprintf(msg, "<%s> %s %s|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_YOU_ARE_NOT_ALLOWED_TO_CLOSE], OtherUser->sNick);
        	if(CheckSprintf(imsgLen, 1024, "clsDcCommands::Close4") == true) {
                curUser->SendCharDelayed(msg, imsgLen);
            }
        	return;
        }

        // disconnect the user
        int imsgLen = sprintf(msg, "[SYS] User %s (%s) closed by %s", OtherUser->sNick, OtherUser->sIP, curUser->sNick);
        if(CheckSprintf(imsgLen, 1024, "clsDcCommands::Close5") == true) {
            clsUdpDebug::mPtr->Broadcast(msg, imsgLen);
        }
        OtherUser->Close();
        
        if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES] == true) {
            if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES_AS_PM] == true) {
                int imsgLen = sprintf(msg, "%s $<%s> *** %s %s %s %s %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                    OtherUser->sNick, clsLanguageManager::mPtr->sTexts[LAN_WITH_IP], OtherUser->sIP, clsLanguageManager::mPtr->sTexts[LAN_WAS_CLOSED_BY], curUser->sNick);
                if(CheckSprintf(imsgLen, 1024, "clsDcCommands::Close6") == true) {
					clsGlobalDataQueue::mPtr->SingleItemStore(msg, imsgLen, NULL, 0, clsGlobalDataQueue::SI_PM2OPS);
                }
            } else {
                int imsgLen = sprintf(msg, "<%s> *** %s %s %s %s %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], OtherUser->sNick, clsLanguageManager::mPtr->sTexts[LAN_WITH_IP],
                    OtherUser->sIP, clsLanguageManager::mPtr->sTexts[LAN_WAS_CLOSED_BY], curUser->sNick);
                if(CheckSprintf(imsgLen, 1024, "clsDcCommands::Close7") == true) {
					clsGlobalDataQueue::mPtr->AddQueueItem(msg, imsgLen, NULL, 0, clsGlobalDataQueue::CMD_OPS);
                }
            }
        }
        
        if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES] == false || ((curUser->ui32BoolBits & User::BIT_OPERATOR) == User::BIT_OPERATOR) == false) {
        	int imsgLen = sprintf(msg, "<%s> *** %s %s %s %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], OtherUser->sNick, clsLanguageManager::mPtr->sTexts[LAN_WITH_IP], OtherUser->sIP,
                clsLanguageManager::mPtr->sTexts[LAN_WAS_CLOSED]);
            if(CheckSprintf(imsgLen, 1024, "clsDcCommands::Close8") == true) {
                curUser->SendCharDelayed(msg, imsgLen);
            }
        }
    }
}
//---------------------------------------------------------------------------

void clsDcCommands::Unknown(User * curUser, char * sData, const uint32_t &iLen) {
    iStatCmdUnknown++;

    #ifdef _DBG
        Memo(">>> Unimplemented Cmd "+curUser->Nick+" [" + curUser->IP + "]: " + sData);
    #endif

    // if we got unknown command sooner than full login finished
    // PPK ... fixed posibility to send (or flood !!!) hub with unknown command before full login
    // Give him chance with script...
    // if this is unkncwn command and script dont clarify that it's ok, disconnect the user
    if(clsScriptManager::mPtr->Arrival(curUser, sData, iLen, clsScriptManager::UNKNOWN_ARRIVAL) == false) {
        if(iLen > 65000) {
            sData[65000] = '\0';
        }

        int imsgLen = sprintf(clsServerManager::sGlobalBuffer, "[SYS] Unknown command from %s (%s) - user closed. (%s)", curUser->sNick, curUser->sIP, sData);
        if(CheckSprintf(imsgLen, clsServerManager::szGlobalBufferSize, "clsDcCommands::Unknown1") == true) {
            clsUdpDebug::mPtr->Broadcast(clsServerManager::sGlobalBuffer, imsgLen);
        }

        curUser->Close();
    }
}
//---------------------------------------------------------------------------

bool clsDcCommands::ValidateUserNick(User * curUser, char * Nick, const size_t &szNickLen, const bool &ValidateNick) {
    // illegal characters in nick?
    for(uint32_t ui32i = 0; ui32i < szNickLen; ui32i++) {
        switch(Nick[ui32i]) {
            case ' ':
            case '$':
            case '|': {
           	    int imsgLen = sprintf(msg, "<%s> %s '%c' ! %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_YOUR_NICK_CONTAINS_ILLEGAL_CHARACTER],
                    Nick[ui32i], clsLanguageManager::mPtr->sTexts[LAN_PLS_CORRECT_IT_AND_GET_BACK_AGAIN]);
                if(CheckSprintf(imsgLen, 1024, "clsDcCommands::ValidateUserNick1") == true) {
                    curUser->SendChar(msg, imsgLen);
                }
//                int imsgLen = sprintf(msg, "[SYS] Nick with bad chars (%s) from %s (%s) - user closed.", Nick, curUser->Nick, curUser->IP);
//                clsUdpDebug::mPtr->Broadcast(msg, imsgLen);
                curUser->Close();
                return false;
            }
            default:
                if((unsigned char)Nick[ui32i] < 32) {
           	        int imsgLen = sprintf(msg, "<%s> %s! %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_YOUR_NICK_CONTAINS_ILLEGAL_WHITE_CHARACTER],
                        clsLanguageManager::mPtr->sTexts[LAN_PLS_CORRECT_IT_AND_GET_BACK_AGAIN]);
                    if(CheckSprintf(imsgLen, 1024, "clsDcCommands::ValidateUserNick1-1") == true) {
                        curUser->SendChar(msg, imsgLen);
                    }
//                    int imsgLen = sprintf(msg, "[SYS] Nick with white chars (%s) from %s (%s) - user closed.", Nick, curUser->Nick, curUser->IP);
//                    clsUdpDebug::mPtr->Broadcast(msg, imsgLen);
                    curUser->Close();
                    return false;
                }

                continue;
        }
    }

    curUser->SetNick(Nick, (uint8_t)szNickLen);
    
    // check for reserved nicks
    if(clsReservedNicksManager::mPtr->CheckReserved(curUser->sNick, curUser->ui32NickHash) == true) {
   	    int imsgLen = sprintf(msg, "<%s> %s. %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_THE_NICK_IS_RESERVED_FOR_SOMEONE_OTHER],
            clsLanguageManager::mPtr->sTexts[LAN_CHANGE_YOUR_NICK_AND_GET_BACK_AGAIN]);
        if(CheckSprintf(imsgLen, 1024, "clsDcCommands::ValidateUserNick2") == true) {
       	    curUser->SendChar(msg, imsgLen);
        }
//       	int imsgLen = sprintf(msg, "[SYS] Reserved nick (%s) from %s (%s) - user closed.", Nick, curUser->Nick, curUser->IP);
//       	clsUdpDebug::mPtr->Broadcast(msg, imsgLen);
        curUser->Close();
        return false;
    }

    time_t acc_time;
    time(&acc_time);

    // PPK ... check if we already have ban for this user
    if(curUser->uLogInOut->uBan != NULL && curUser->ui32NickHash == curUser->uLogInOut->uBan->ui32NickHash) {
        curUser->SendChar(curUser->uLogInOut->uBan->sMessage, curUser->uLogInOut->uBan->ui32Len);
        int imsgLen = sprintf(msg, "[SYS] Banned user %s (%s) - user closed.", curUser->sNick, curUser->sIP);
        if(CheckSprintf(imsgLen, 1024, "clsDcCommands::ValidateUserNick3") == true) {
            clsUdpDebug::mPtr->Broadcast(msg, imsgLen);
        }
        curUser->Close();
        return false;
    }
    
    // check for banned nicks
    BanItem * pBan = clsBanManager::mPtr->FindNick(curUser);
    if(pBan != NULL) {
        int imsgLen;
        char *messg = GenerateBanMessage(pBan, imsgLen, acc_time);
        curUser->SendChar(messg, imsgLen);
        imsgLen = sprintf(msg, "[SYS] Banned user %s (%s) - user closed.", curUser->sNick, curUser->sIP);
        if(CheckSprintf(imsgLen, 1024, "clsDcCommands::ValidateUserNick4") == true) {
            clsUdpDebug::mPtr->Broadcast(msg, imsgLen);
        }
        curUser->Close();
        return false;
    }

    // Nick is ok, check for registered nick
    RegUser *Reg = clsRegManager::mPtr->Find(curUser);
    if(Reg != NULL) {
        if(clsSettingManager::mPtr->bBools[SETBOOL_ADVANCED_PASS_PROTECTION] == true && Reg->ui8BadPassCount != 0) {
            time_t acc_time;
            time(&acc_time);

#ifdef _WIN32
			uint32_t iMinutes2Wait = (uint32_t)pow((float)2, Reg->ui8BadPassCount-1);
#else
			uint32_t iMinutes2Wait = (uint32_t)pow(2, Reg->ui8BadPassCount-1);
#endif
            if(acc_time < (time_t)(Reg->tLastBadPass+(60*iMinutes2Wait))) {
                int imsgLen = sprintf(msg, "<%s> %s %s %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_LAST_PASS_WAS_WRONG_YOU_NEED_WAIT],
                    formatSecTime((Reg->tLastBadPass+(60*iMinutes2Wait))-acc_time), clsLanguageManager::mPtr->sTexts[LAN_BEFORE_YOU_TRY_AGAIN]);
                if(CheckSprintf(imsgLen, 1024, "clsDcCommands::ValidateUserNick5") == true) {
                    curUser->SendChar(msg, imsgLen);
                }

				imsgLen = sprintf(msg, "[SYS] User %s (%s) not allowed to send password (%" PRIu64 ") - user closed.", curUser->sNick, curUser->sIP, 
                    (uint64_t)((Reg->tLastBadPass+(60*iMinutes2Wait))-acc_time));
                if(CheckSprintf(imsgLen, 1024, "clsDcCommands::ValidateUserNick6") == true) {
                    clsUdpDebug::mPtr->Broadcast(msg, imsgLen);
                }
                curUser->Close();
                return false;
            }
        }
            
        curUser->iProfile = (int32_t)Reg->ui16Profile;
    }

    // PPK ... moved IP ban check here, we need to allow registered users on shared IP to log in if not have banned nick, but only IP.
    if(clsProfileManager::mPtr->IsAllowed(curUser, clsProfileManager::ENTERIFIPBAN) == false) {
        // PPK ... check if we already have ban for this user
        if(curUser->uLogInOut->uBan != NULL) {
            curUser->SendChar(curUser->uLogInOut->uBan->sMessage, curUser->uLogInOut->uBan->ui32Len);
            int imsgLen = sprintf(msg, "[SYS] uBanned user %s (%s) - user closed.", curUser->sNick, curUser->sIP);
            if(CheckSprintf(imsgLen, 1024, "clsDcCommands::ValidateUserNick7") == true) {
                clsUdpDebug::mPtr->Broadcast(msg, imsgLen);
            }
            curUser->Close();
            return false;
        }
    }

    // PPK ... delete user ban if we have it
    if(curUser->uLogInOut->uBan != NULL) {
        delete curUser->uLogInOut->uBan;
        curUser->uLogInOut->uBan = NULL;
    }

    // first check for user limit ! PPK ... allow hublist pinger to check hub any time ;)
    if(clsProfileManager::mPtr->IsAllowed(curUser, clsProfileManager::ENTERFULLHUB) == false && ((curUser->ui32BoolBits & User::BIT_PINGER) == User::BIT_PINGER) == false) {
        // user is NOT allowed enter full hub, check for maxClients
        if(clsServerManager::ui32Joins-clsServerManager::ui32Parts > (uint32_t)clsSettingManager::mPtr->iShorts[SETSHORT_MAX_USERS]) {
			int imsgLen = sprintf(msg, "$HubIsFull|<%s> %s. %u %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_THIS_HUB_IS_FULL], clsServerManager::ui32Logged,
                clsLanguageManager::mPtr->sTexts[LAN_USERS_ONLINE_LWR]);
            if(CheckSprintf(imsgLen, 1024, "clsDcCommands::ValidateUserNick8") == false) {
                return false;
            }
            if(clsSettingManager::mPtr->bBools[SETBOOL_REDIRECT_WHEN_HUB_FULL] == true &&
                clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_REDIRECT_ADDRESS] != NULL) {
                memcpy(msg+imsgLen, clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_REDIRECT_ADDRESS],
                    (size_t)clsSettingManager::mPtr->ui16PreTextsLens[clsSettingManager::SETPRETXT_REDIRECT_ADDRESS]);
                imsgLen += clsSettingManager::mPtr->ui16PreTextsLens[clsSettingManager::SETPRETXT_REDIRECT_ADDRESS];
                msg[imsgLen] = '\0';
            }
            curUser->SendChar(msg, imsgLen);
//            int imsgLen = sprintf(msg, "[SYS] Hub full for %s (%s) - user closed.", curUser->Nick, curUser->IP);
//            clsUdpDebug::mPtr->Broadcast(msg, imsgLen);
            curUser->Close();
            return false;
        }
    }

    // Check for maximum connections from same IP
    if(clsProfileManager::mPtr->IsAllowed(curUser, clsProfileManager::NOUSRSAMEIP) == false) {
        uint32_t ui32Count = clsHashManager::mPtr->GetUserIpCount(curUser);
        if(ui32Count >= (uint32_t)clsSettingManager::mPtr->iShorts[SETSHORT_MAX_CONN_SAME_IP]) {
            int imsgLen = sprintf(msg, "<%s> %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_SORRY_ALREADY_MAX_IP_CONNS]);
            if(CheckSprintf(imsgLen, 1024, "clsDcCommands::ValidateUserNick9") == false) {
                return false;
            }
            if(clsSettingManager::mPtr->bBools[SETBOOL_REDIRECT_WHEN_HUB_FULL] == true &&
                clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_REDIRECT_ADDRESS] != NULL) {
                   memcpy(msg+imsgLen, clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_REDIRECT_ADDRESS],
                    (size_t)clsSettingManager::mPtr->ui16PreTextsLens[clsSettingManager::SETPRETXT_REDIRECT_ADDRESS]);
                imsgLen += clsSettingManager::mPtr->ui16PreTextsLens[clsSettingManager::SETPRETXT_REDIRECT_ADDRESS];
                msg[imsgLen] = '\0';
            }
            curUser->SendChar(msg, imsgLen);
			imsgLen = sprintf(msg, "[SYS] Max connections from same IP (%u) for %s (%s) - user closed. ", ui32Count, curUser->sNick, curUser->sIP);
            string tmp;
            if(CheckSprintf(imsgLen, 1024, "clsDcCommands::ValidateUserNick10") == true) {
                //clsUdpDebug::mPtr->Broadcast(msg, imsgLen);
                tmp = msg;
            }

            User *nxt = clsHashManager::mPtr->FindUser(curUser->ui128IpHash);

            while(nxt != NULL) {
        		User *cur = nxt;
                nxt = cur->hashiptablenext;

                tmp += " "+string(cur->sNick, cur->ui8NickLen);
            }

            clsUdpDebug::mPtr->Broadcast(tmp);

            curUser->Close();
            return false;
        }
    }

    // Check for reconnect time
    if(clsProfileManager::mPtr->IsAllowed(curUser, clsProfileManager::NORECONNTIME) == false &&
        clsUsers::mPtr->CheckRecTime(curUser) == true) {
        int imsgLen = sprintf(msg, "[SYS] Fast reconnect from %s (%s) - user closed.", curUser->sNick, curUser->sIP);
        if(CheckSprintf(imsgLen, 1024, "clsDcCommands::ValidateUserNick11") == true) {
            clsUdpDebug::mPtr->Broadcast(msg, imsgLen);
        }
        curUser->Close();
        return false;
    }

    curUser->ui8Country = clsIpP2Country::mPtr->Find(curUser->ui128IpHash);

    // check for nick in userlist. If taken, check for dupe's socket state
    // if still active, send $ValidateDenide and close()
    User *OtherUser = clsHashManager::mPtr->FindUser(curUser);

    if(OtherUser != NULL) {
   	    if(OtherUser->ui8State < User::STATE_CLOSING) {
            // check for socket error, or if user closed connection
            int iRet = recv(OtherUser->Sck, msg, 16, MSG_PEEK);

            // if socket error or user closed connection then allow new user to log in
#ifdef _WIN32
            if((iRet == SOCKET_ERROR && WSAGetLastError() != WSAEWOULDBLOCK) || iRet == 0) {
#else
			if((iRet == -1 && errno != EAGAIN) || iRet == 0) {
#endif
                OtherUser->ui32BoolBits |= User::BIT_ERROR;
                int imsgLen = sprintf(msg, "[SYS] Ghost in validate nick %s (%s) - user closed.", OtherUser->sNick, OtherUser->sIP);
                if(CheckSprintf(imsgLen, 1024, "clsDcCommands::ValidateUserNick13") == true) {
                    clsUdpDebug::mPtr->Broadcast(msg, imsgLen);
                }
                OtherUser->Close();
                return false;
            } else {
                if(Reg == NULL) {
 					// alex82 ... добавили ValidateDenideArrival
					clsScriptManager::mPtr->Arrival(curUser, Nick, szNickLen, clsScriptManager::VALIDATE_DENIDE_ARRIVAL);
           	        int imsgLen = sprintf(msg, "$ValidateDenide %s|", Nick);
           	        if(CheckSprintf(imsgLen, 1024, "clsDcCommands::ValidateUserNick14") == true) {
                        curUser->SendChar(msg, imsgLen);
                    }

                    if(strcmp(OtherUser->sIP, curUser->sIP) != 0 || strcmp(OtherUser->sNick, curUser->sNick) != 0) {
                        imsgLen = sprintf(msg, "[SYS] Nick taken [%s (%s)] %s (%s) - user closed.", OtherUser->sNick, OtherUser->sIP, curUser->sNick, curUser->sIP);
                        if(CheckSprintf(imsgLen, 1024, "clsDcCommands::ValidateUserNick15") == true) {
                            clsUdpDebug::mPtr->Broadcast(msg, imsgLen);
                        }
                    }

                    curUser->Close();
                    return false;
                } else {
                    // PPK ... addition for registered users, kill your own ghost >:-]
                    curUser->ui8State = User::STATE_VERSION_OR_MYPASS;
                    curUser->ui32BoolBits |= User::BIT_WAITING_FOR_PASS;
                    curUser->AddPrcsdCmd(PrcsdUsrCmd::GETPASS, NULL, 0, NULL);
                    return true;
                }
            }
        }
    }
        
    if(Reg == NULL) {
        // user is NOT registered
        
        // nick length check
        if((clsSettingManager::mPtr->iShorts[SETSHORT_MIN_NICK_LEN] != 0 && szNickLen < (uint32_t)clsSettingManager::mPtr->iShorts[SETSHORT_MIN_NICK_LEN]) ||
            (clsSettingManager::mPtr->iShorts[SETSHORT_MAX_NICK_LEN] != 0 && szNickLen > (uint32_t)clsSettingManager::mPtr->iShorts[SETSHORT_MAX_NICK_LEN])) {
            curUser->SendChar(clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_NICK_LIMIT_MSG],
                clsSettingManager::mPtr->ui16PreTextsLens[clsSettingManager::SETPRETXT_NICK_LIMIT_MSG]);
            int imsgLen = sprintf(msg, "[SYS] Bad nick length (%d) from %s (%s) - user closed.", (int)szNickLen, curUser->sNick, curUser->sIP);
            if(CheckSprintf(imsgLen, 1024, "clsDcCommands::ValidateUserNick16") == true) {
                clsUdpDebug::mPtr->Broadcast(msg, imsgLen);
            }
            curUser->Close();
            return false;
        }

        if(clsSettingManager::mPtr->bBools[SETBOOL_REG_ONLY] == true && ((curUser->ui32BoolBits & User::BIT_PINGER) == User::BIT_PINGER) == false) {
            curUser->SendChar(clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_REG_ONLY_MSG],
                clsSettingManager::mPtr->ui16PreTextsLens[clsSettingManager::SETPRETXT_REG_ONLY_MSG]);
//            imsgLen = sprintf(msg, "[SYS] Hub for reg only %s (%s) - user closed.", curUser->Nick, curUser->IP);
//            clsUdpDebug::mPtr->Broadcast(msg, imsgLen);
            curUser->Close();
            return false;
        } else {
       	    // hub is public, proceed to Hello
            if(clsHashManager::mPtr->Add(curUser) == false) {
                return false;
            }

            curUser->ui32BoolBits |= User::BIT_HASHED;

            if(ValidateNick == true) {
                curUser->ui8State = User::STATE_VERSION_OR_MYPASS; // waiting for $Version
                curUser->AddPrcsdCmd(PrcsdUsrCmd::LOGINHELLO, NULL, 0, NULL);
            }
            return true;
        }
    } else {
        // user is registered, wait for password
        if(clsHashManager::mPtr->Add(curUser) == false) {
            return false;
        }

        curUser->ui32BoolBits |= User::BIT_HASHED;
        curUser->ui8State = User::STATE_VERSION_OR_MYPASS;
        curUser->ui32BoolBits |= User::BIT_WAITING_FOR_PASS;
        curUser->AddPrcsdCmd(PrcsdUsrCmd::GETPASS, NULL, 0, NULL);
        return true;
    }
}
//---------------------------------------------------------------------------

PassBf * clsDcCommands::Find(const uint8_t * ui128IpHash) {
	PassBf *next = PasswdBfCheck;

    while(next != NULL) {
        PassBf *cur = next;
        next = cur->next;
		if(memcmp(cur->ui128IpHash, ui128IpHash, 16) == 0) {
            return cur;
        }
    }
    return NULL;
}
//---------------------------------------------------------------------------

void clsDcCommands::Remove(PassBf * PassBfItem) {
    if(PassBfItem->prev == NULL) {
        if(PassBfItem->next == NULL) {
            PasswdBfCheck = NULL;
        } else {
            PassBfItem->next->prev = NULL;
            PasswdBfCheck = PassBfItem->next;
        }
    } else if(PassBfItem->next == NULL) {
        PassBfItem->prev->next = NULL;
    } else {
        PassBfItem->prev->next = PassBfItem->next;
        PassBfItem->next->prev = PassBfItem->prev;
    }
	delete PassBfItem;
}
//---------------------------------------------------------------------------

void clsDcCommands::ProcessCmds(User * curUser) {
    curUser->ui32BoolBits &= ~User::BIT_CHAT_INSERT;

    PrcsdUsrCmd *next = curUser->cmdStrt;
    
    while(next != NULL) {
        PrcsdUsrCmd *cur = next;
        next = cur->next;
        switch(cur->cType) {
            case PrcsdUsrCmd::SUPPORTS: {
                memcpy(msg, "$Supports", 9);
                uint32_t iSupportsLen = 9;
                
                // PPK ... why to send it if client don't need it =)
                /*if(((curUser->ui32SupportBits & User::SUPPORTBIT_ZPIPE) == User::SUPPORTBIT_ZPIPE) == true) {
                    memcpy(msg+iSupportsLen, " ZPipe", 6);
                    iSupportsLen += 6;
                }*/
                
                // PPK ... yes yes yes finally QuickList support in PtokaX !!! ;))
                if((curUser->ui32SupportBits & User::SUPPORTBIT_QUICKLIST) == User::SUPPORTBIT_QUICKLIST) {
                    memcpy(msg+iSupportsLen, " QuickList", 10);
                    iSupportsLen += 10;
                } else if((curUser->ui32SupportBits & User::SUPPORTBIT_NOHELLO) == User::SUPPORTBIT_NOHELLO) {
                    // PPK ... Hmmm Client not really need it, but for now send it ;-)
                    memcpy(msg+iSupportsLen, " NoHello", 8);
                    iSupportsLen += 8;
                } else if((curUser->ui32SupportBits & User::SUPPORTBIT_NOGETINFO) == User::SUPPORTBIT_NOGETINFO) {
                    // PPK ... if client support NoHello automatically supports NoGetINFO another badwith wasting !
                    memcpy(msg+iSupportsLen, " NoGetINFO", 10);
                    iSupportsLen += 10;
                }
            
                if((curUser->ui32BoolBits & User::BIT_PINGER) == User::BIT_PINGER) {
                    memcpy(msg+iSupportsLen, " BotINFO HubINFO", 16);
                    iSupportsLen += 16;
                }

                if((curUser->ui32SupportBits & User::SUPPORTBIT_IP64) == User::SUPPORTBIT_IP64) {
                    memcpy(msg+iSupportsLen, " IP64", 5);
                    iSupportsLen += 5;
                }

                if(((curUser->ui32SupportBits & User::SUPPORTBIT_IPV4) == User::SUPPORTBIT_IPV4) && ((curUser->ui32BoolBits & User::BIT_IPV6) == User::BIT_IPV6)) {
                    // Only client connected with IPv6 sending this, so only that client is getting reply
                    memcpy(msg+iSupportsLen, " IPv4", 5);
                    iSupportsLen += 5;
                }

                memcpy(msg+iSupportsLen, "|\0", 2);
                curUser->SendCharDelayed(msg, iSupportsLen+1);
                break;
            }
            case PrcsdUsrCmd::LOGINHELLO: {
                int imsgLen = sprintf(msg, "$Hello %s|", curUser->sNick);
                if(CheckSprintf(imsgLen, 1024, "clsDcCommands::ProcessCmds1") == true) {
                    curUser->SendCharDelayed(msg, imsgLen);
                }
                break;
            }
            case PrcsdUsrCmd::GETPASS: {
                uint32_t iLen = 9;
                curUser->SendCharDelayed("$GetPass|", iLen); // query user for password
                break;
            }
            case PrcsdUsrCmd::CHAT: {
            	// find chat message data
                char *sBuff = cur->sCommand+curUser->ui8NickLen+3;

            	// non-command chat msg
                bool bNonChat = false;
            	for(uint8_t ui8i = 0; ui8i < (uint8_t)clsSettingManager::mPtr->ui16TextsLens[SETTXT_CHAT_COMMANDS_PREFIXES]; ui8i++) {
            	    if(sBuff[0] == clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][ui8i] ) {
                        bNonChat = true;
                	    break;
                    }
            	}

                if(bNonChat == true) {
                    // text files...
                    if(clsSettingManager::mPtr->bBools[SETBOOL_ENABLE_TEXT_FILES] == true) {
                        cur->sCommand[cur->iLen-1] = '\0'; // get rid of the pipe
                        
                        if(clsTextFilesManager::mPtr->ProcessTextFilesCmd(curUser, sBuff+1)) {
                            break;
                        }
    
                        cur->sCommand[cur->iLen-1] = '|'; // add back pipe
                    }
    
                    // built-in commands
                    if(cur->iLen-curUser->ui8NickLen >= 9) {
                        if(clsHubCommands::DoCommand(curUser, sBuff-(curUser->ui8NickLen-1), cur->iLen)) break;
                        
                        cur->sCommand[cur->iLen-1] = '|'; // add back pipe
                    }
                }
           
            	// everything's ok, let's chat
            	clsUsers::mPtr->SendChat2All(curUser, cur->sCommand, cur->iLen, cur->ptr);
            
                break;
            }
            case PrcsdUsrCmd::TO_OP_CHAT: {
                clsGlobalDataQueue::mPtr->SingleItemStore(cur->sCommand, cur->iLen, curUser, 0, clsGlobalDataQueue::SI_OPCHAT);
                break;
            }
        }

#ifdef _WIN32
        if(HeapFree(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE, (void *)cur->sCommand) == 0) {
            AppendDebugLog("%s - [MEM] Cannot deallocate cur->sCommand in clsDcCommands::ProcessCmds\n", 0);
        }
#else
		free(cur->sCommand);
#endif
        cur->sCommand = NULL;

        delete cur;
    }
    
    curUser->cmdStrt = NULL;
    curUser->cmdEnd = NULL;

    if((curUser->ui32BoolBits & User::BIT_PRCSD_MYINFO) == User::BIT_PRCSD_MYINFO) {
        curUser->ui32BoolBits &= ~User::BIT_PRCSD_MYINFO;

        if(((curUser->ui32BoolBits & User::BIT_HAVE_BADTAG) == User::BIT_HAVE_BADTAG) == true) {
            curUser->HasSuspiciousTag();
        }

        if(clsSettingManager::mPtr->ui8FullMyINFOOption == 0) {
            if(curUser->GenerateMyInfoLong() == false) {
                return;
            }

            clsUsers::mPtr->Add2MyInfosTag(curUser);

            if(clsSettingManager::mPtr->iShorts[SETSHORT_MYINFO_DELAY] == 0 || clsServerManager::ui64ActualTick > ((60*clsSettingManager::mPtr->iShorts[SETSHORT_MYINFO_DELAY])+curUser->iLastMyINFOSendTick)) {
				clsGlobalDataQueue::mPtr->AddQueueItem(curUser->sMyInfoLong, curUser->ui16MyInfoLongLen, NULL, 0, clsGlobalDataQueue::CMD_MYINFO);
                curUser->iLastMyINFOSendTick = clsServerManager::ui64ActualTick;
            } else {
				clsGlobalDataQueue::mPtr->AddQueueItem(curUser->sMyInfoLong, curUser->ui16MyInfoLongLen, NULL, 0, clsGlobalDataQueue::CMD_OPS);
            }

            return;
        } else if(clsSettingManager::mPtr->ui8FullMyINFOOption == 2) {
            if(curUser->GenerateMyInfoShort() == false) {
                return;
            }

            clsUsers::mPtr->Add2MyInfos(curUser);

            if(clsSettingManager::mPtr->iShorts[SETSHORT_MYINFO_DELAY] == 0 || clsServerManager::ui64ActualTick > ((60*clsSettingManager::mPtr->iShorts[SETSHORT_MYINFO_DELAY])+curUser->iLastMyINFOSendTick)) {
				clsGlobalDataQueue::mPtr->AddQueueItem(curUser->sMyInfoShort, curUser->ui16MyInfoShortLen, NULL, 0, clsGlobalDataQueue::CMD_MYINFO);
                curUser->iLastMyINFOSendTick = clsServerManager::ui64ActualTick;
            } else {
				clsGlobalDataQueue::mPtr->AddQueueItem(curUser->sMyInfoShort, curUser->ui16MyInfoShortLen, NULL, 0, clsGlobalDataQueue::CMD_OPS);
            }

            return;
		}

		if(curUser->GenerateMyInfoLong() == false) {
			return;
		}

		clsUsers::mPtr->Add2MyInfosTag(curUser);

		char * sShortMyINFO = NULL;
		size_t szShortMyINFOLen = 0;

		if(curUser->GenerateMyInfoShort() == true) {
			clsUsers::mPtr->Add2MyInfos(curUser);

			if(clsSettingManager::mPtr->iShorts[SETSHORT_MYINFO_DELAY] == 0 || clsServerManager::ui64ActualTick > ((60*clsSettingManager::mPtr->iShorts[SETSHORT_MYINFO_DELAY])+curUser->iLastMyINFOSendTick)) {
				sShortMyINFO = curUser->sMyInfoShort;
				szShortMyINFOLen = curUser->ui16MyInfoShortLen;
				curUser->iLastMyINFOSendTick = clsServerManager::ui64ActualTick;
			}
		}

		clsGlobalDataQueue::mPtr->AddQueueItem(sShortMyINFO, szShortMyINFOLen, curUser->sMyInfoLong, curUser->ui16MyInfoLongLen, clsGlobalDataQueue::CMD_MYINFO);
    }
}
//---------------------------------------------------------------------------

bool clsDcCommands::CheckIP(const User * curUser, const char * sIP) {
    if((curUser->ui32BoolBits & User::BIT_IPV6) == User::BIT_IPV6) {
        if(sIP[0] == '[' && sIP[1+curUser->ui8IpLen] == ']' && sIP[2+curUser->ui8IpLen] == ':' && strncmp(sIP+1, curUser->sIP, curUser->ui8IpLen) == 0) {
            return true;
        } else if(((curUser->ui32BoolBits & User::BIT_IPV4) == User::BIT_IPV4) && sIP[curUser->ui8IPv4Len] == ':' && strncmp(sIP, curUser->sIPv4, curUser->ui8IPv4Len) == 0) {
            return true;
        }
    } else if(sIP[curUser->ui8IpLen] == ':' && strncmp(sIP, curUser->sIP, curUser->ui8IpLen) == 0) {
        return true;
    }

    return false;
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

char * clsDcCommands::GetPort(char * sData, char cPortEnd, size_t &szPortLen) {
    char * sPortEnd = strchr(sData, cPortEnd);
    if(sPortEnd == NULL) {
        return NULL;
    }

    sPortEnd[0] = '\0';

    char * sPortStart = strrchr(sData, ':');
    if(sPortStart == NULL || sPortStart[1] == '\0') {
        sPortEnd[0] = cPortEnd;
        return NULL;
    }

    sPortStart[0] = '\0';
    sPortStart++;
    szPortLen = (sPortEnd-sPortStart);

    int iPort = atoi(sPortStart);
    if(iPort < 1 || iPort > 65535) {
        *(sPortStart-1) = ':';
        sPortEnd[0] = cPortEnd;
        return NULL;
    }

    return sPortStart;
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

void clsDcCommands::SendIncorrectIPMsg(User * curUser, char * sBadIP, const bool &bCTM) {
	int imsgLen = sprintf(msg, "<%s> %s ", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_YOUR_CLIENT_SEND_INCORRECT_IP]);
	if(CheckSprintf(imsgLen, 1024, "SendIncorrectIPMsg1") == false) {
		return;
	}

    if((curUser->ui32BoolBits & User::BIT_IPV6) == User::BIT_IPV6) {
        uint8_t ui8i = 1;
        while(sBadIP[ui8i] != '\0') {
            if(isxdigit(sBadIP[ui8i]) == false && sBadIP[ui8i] != ':') {
                if(ui8i == 0) {
                    imsgLen--;
                }

                break;
            }

            msg[imsgLen] = sBadIP[ui8i];
            imsgLen++;

            ui8i++;
        }
    } else {
        uint8_t ui8i = 0;
        while(sBadIP[ui8i] != '\0') {
            if(isdigit(sBadIP[ui8i]) == false && sBadIP[ui8i] != '.') {
                if(ui8i == 0) {
                    imsgLen--;
                }

                break;
            }

            msg[imsgLen] = sBadIP[ui8i];
            imsgLen++;

            ui8i++;
        }
    }

	int iret = sprintf(msg+imsgLen, " %s %s !|", bCTM == false ? clsLanguageManager::mPtr->sTexts[LAN_IN_CTM_REQ_REAL_IP_IS] : clsLanguageManager::mPtr->sTexts[LAN_IN_SEARCH_REQ_REAL_IP_IS], curUser->sIP);
	imsgLen += iret;
	if(CheckSprintf1(iret, imsgLen, 1024, "SendIncorrectIPMsg2") == false) {
		curUser->SendCharDelayed(msg, imsgLen);
    }
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

void clsDcCommands::SendIPFixedMsg(User * pUser, char * sBadIP, char * sRealIP) {
    if((pUser->ui32BoolBits & User::BIT_WARNED_WRONG_IP) == User::BIT_WARNED_WRONG_IP) {
        return;
    }

    int imsgLen = sprintf(clsServerManager::sGlobalBuffer, "<%s> %s %s %s %s !|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_YOUR_CLIENT_SEND_INCORRECT_IP], sBadIP,
        clsLanguageManager::mPtr->sTexts[LAN_IN_COMMAND_HUB_REPLACED_IT_WITH_YOUR_REAL_IP], sRealIP);
    if(CheckSprintf(imsgLen, clsServerManager::szGlobalBufferSize, "SendIncorrectIPMsg1") == false) {
        pUser->SendCharDelayed(clsServerManager::sGlobalBuffer, imsgLen);
    }

    pUser->ui32BoolBits |= User::BIT_WARNED_WRONG_IP;
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

void clsDcCommands::MyNick(User * pUser, char * sData, const uint32_t &ui32Len) {
    if((pUser->ui32BoolBits & User::BIT_IPV6) == User::BIT_IPV6) {
        int imsgLen = sprintf(msg, "[SYS] IPv6 $MyNick (%s) from %s (%s) - user closed.", sData, pUser->sNick, pUser->sIP);
        if(CheckSprintf(imsgLen, 1024, "clsDcCommands::MyNick") == true) {
            clsUdpDebug::mPtr->Broadcast(msg, imsgLen);
        }

        Unknown(pUser, sData, ui32Len);
        return;
    }

    if(ui32Len < 10) {
        int imsgLen = sprintf(msg, "[SYS] Short $MyNick (%s) from %s (%s) - user closed.", sData, pUser->sNick, pUser->sIP);
        if(CheckSprintf(imsgLen, 1024, "clsDcCommands::MyNick1") == true) {
            clsUdpDebug::mPtr->Broadcast(msg, imsgLen);
        }

        Unknown(pUser, sData, ui32Len);
        return;
    }

    sData[ui32Len-1] = '\0'; // cutoff pipe

    User * pOtherUser = clsHashManager::mPtr->FindUser(sData+8, ui32Len-9);

    if(pOtherUser == NULL || pOtherUser->ui8State != User::STATE_IPV4_CHECK) {
        int imsgLen = sprintf(msg, "[SYS] Bad $MyNick (%s) from %s (%s) - user closed.", sData, pUser->sNick, pUser->sIP);
        if(CheckSprintf(imsgLen, 1024, "clsDcCommands::MyNick2") == true) {
            clsUdpDebug::mPtr->Broadcast(msg, imsgLen);
        }

        Unknown(pUser, sData, ui32Len);
        return;
    }

    strcpy(pOtherUser->sIPv4, pUser->sIP);
    pOtherUser->ui8IPv4Len = pUser->ui8IpLen;
    pOtherUser->ui32BoolBits |= User::BIT_IPV4;

    pOtherUser->ui8State = User::STATE_ADDME;

    pUser->Close();
/*
	int imsgLen = sprintf(msg, "<%s> Found IPv4: %s =)|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], pOtherUser->sIPv4);
	if(CheckSprintf(imsgLen, 1024, "clsDcCommands::MyNick3") == true) {
        UserSendCharDelayed(pOtherUser, msg, imsgLen);
	}
*/
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

PrcsdUsrCmd * clsDcCommands::AddSearch(User * pUser, PrcsdUsrCmd * cmdSearch, char * sSearch, const size_t &szLen, const bool &bActive) const {
    if(cmdSearch != NULL) {
        char * pOldBuf = cmdSearch->sCommand;
#ifdef _WIN32
        cmdSearch->sCommand = (char *)HeapReAlloc(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE, (void *)pOldBuf, cmdSearch->iLen+szLen+1);
#else
		cmdSearch->sCommand = (char *)realloc(pOldBuf, cmdSearch->iLen+szLen+1);
#endif
        if(cmdSearch->sCommand == NULL) {
            cmdSearch->sCommand = pOldBuf;
            pUser->ui32BoolBits |= User::BIT_ERROR;
            pUser->Close();

			AppendDebugLog("%s - [MEM] Cannot reallocate %" PRIu64 " bytes for clsDcCommands::AddSearch1\n", (uint64_t)(cmdSearch->iLen+szLen+1));

            return cmdSearch;
        }
        memcpy(cmdSearch->sCommand+cmdSearch->iLen, sSearch, szLen);
        cmdSearch->iLen += (uint32_t)szLen;
        cmdSearch->sCommand[cmdSearch->iLen] = '\0';

        if(bActive == true) {
            clsUsers::mPtr->ui16ActSearchs++;
        } else {
            clsUsers::mPtr->ui16PasSearchs++;
        }
    } else {
        cmdSearch = new PrcsdUsrCmd();
        if(cmdSearch == NULL) {
            pUser->ui32BoolBits |= User::BIT_ERROR;
            pUser->Close();

			AppendDebugLog("%s - [MEM] Cannot allocate new cmdSearch in clsDcCommands::AddSearch1\n", 0);
            return cmdSearch;
        }

#ifdef _WIN32
        cmdSearch->sCommand = (char *)HeapAlloc(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE, szLen+1);
#else
		cmdSearch->sCommand = (char *)malloc(szLen+1);
#endif
		if(cmdSearch->sCommand == NULL) {
            delete cmdSearch;
            cmdSearch = NULL;

            pUser->ui32BoolBits |= User::BIT_ERROR;
            pUser->Close();

			AppendDebugLog("%s - [MEM] Cannot allocate %" PRIu64 " bytes for DcCommands::Search5\n", (uint64_t)(szLen+1));

            return cmdSearch;
        }

        memcpy(cmdSearch->sCommand, sSearch, szLen);
        cmdSearch->sCommand[szLen] = '\0';

        cmdSearch->iLen = (uint32_t)szLen;

        if(bActive == true) {
            clsUsers::mPtr->ui16ActSearchs++;
        } else {
            clsUsers::mPtr->ui16PasSearchs++;
        }
    }

    return cmdSearch;
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
