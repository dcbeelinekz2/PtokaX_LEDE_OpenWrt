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
#include "colUsers.h"
#include "DcCommands.h"
#include "eventqueue.h"
#include "GlobalDataQueue.h"
#include "hashBanManager.h"
#include "hashRegManager.h"
#include "hashUsrManager.h"
#include "LanguageManager.h"
#include "LuaScriptManager.h"
#include "ProfileManager.h"
#include "ServerManager.h"
#include "serviceLoop.h"
#include "SettingManager.h"
#include "UdpDebug.h"
#include "User.h"
#include "utility.h"
//#include "ZlibUtility.h"
//---------------------------------------------------------------------------
#ifdef _WIN32
	#pragma hdrstop
#endif
//---------------------------------------------------------------------------
#include "HubCommands.h"
//---------------------------------------------------------------------------
#include "IP2Country.h"
#include "LuaScript.h"
#include "TextFileManager.h"
//---------------------------------------------------------------------------
#ifdef _BUILD_GUI
    #include "../gui.win/RegisteredUsersDialog.h"
#endif
//---------------------------------------------------------------------------
char clsHubCommands::msg[1024];
//---------------------------------------------------------------------------

bool clsHubCommands::DoCommand(User * curUser, char * sCommand, const size_t &szCmdLen, bool fromPM/* = false*/) {
	size_t dlen;

    if(fromPM == false) {
    	// Hub commands: !me
		if(strncasecmp(sCommand+curUser->ui8NickLen, "me ", 3) == 0) {
	    	// %me command
    	    if(szCmdLen - (curUser->ui8NickLen + 4) > 4) {
                sCommand[0] = '*';
    			sCommand[1] = ' ';
    			memcpy(sCommand+2, curUser->sNick, curUser->ui8NickLen);
                clsUsers::mPtr->SendChat2All(curUser, sCommand, szCmdLen-4, NULL);
                return true;
    	    }
    	    return false;
        }

        // PPK ... optional reply commands in chat to PM
        fromPM = clsSettingManager::mPtr->bBools[SETBOOL_REPLY_TO_HUB_COMMANDS_AS_PM];

        sCommand[szCmdLen-5] = '\0'; // get rid of the pipe
        sCommand += curUser->ui8NickLen;
        dlen = szCmdLen - (curUser->ui8NickLen + 5);
    } else {
        sCommand++;
        dlen = szCmdLen - (curUser->ui8NickLen + 6);
    }

    switch(tolower(sCommand[0])) {
        case 'g':
            // Hub commands: !getbans
			if(dlen == 7 && strncasecmp(sCommand+1, "etbans", 6) == 0) {
                if(clsProfileManager::mPtr->IsAllowed(curUser, clsProfileManager::GETBANLIST) == false) {
                    SendNoPermission(curUser, fromPM);
                    return true;
                }

                UncountDeflood(curUser, fromPM);

                int imsglen = CheckFromPm(curUser, fromPM);

                int iret = sprintf(msg+imsglen, "<%s> ", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC]);
                imsglen += iret;
                if(CheckSprintf1(iret, imsglen, 1024, "clsHubCommands::DoCommand2") == false) {
                    return true;
                }

				string BanList(msg, imsglen);
                bool bIsEmpty = true;

                if(clsBanManager::mPtr->TempBanListS != NULL) {
                    uint32_t iBanNum = 0;
                    time_t acc_time;
                    time(&acc_time);

                    BanItem *nextBan = clsBanManager::mPtr->TempBanListS;

                    while(nextBan != NULL) {
                        BanItem *curBan = nextBan;
    		            nextBan = curBan->next;

                        if(acc_time > curBan->tempbanexpire) {
							clsBanManager::mPtr->Rem(curBan);
                            delete curBan;

							continue;
                        }

                        if(iBanNum == 0) {
							BanList += string(clsLanguageManager::mPtr->sTexts[LAN_TEMP_BANS], (size_t)clsLanguageManager::mPtr->ui16TextsLens[LAN_TEMP_BANS]) + ":\n\n";
                        }

                        iBanNum++;
						BanList += "[ " + string(iBanNum) + " ]";

                        if(curBan->sIp[0] != '\0') {
                            if(((curBan->ui8Bits & clsBanManager::IP) == clsBanManager::IP) == true) {
								BanList += " " + string(clsLanguageManager::mPtr->sTexts[LAN_BANNED], (size_t)clsLanguageManager::mPtr->ui16TextsLens[LAN_BANNED]);
                            }
							BanList += " " + string(clsLanguageManager::mPtr->sTexts[LAN_IP], (size_t)clsLanguageManager::mPtr->ui16TextsLens[LAN_IP]) + ": " + string(curBan->sIp);
                            if(((curBan->ui8Bits & clsBanManager::FULL) == clsBanManager::FULL) == true) {
								BanList += " (" + string(clsLanguageManager::mPtr->sTexts[LAN_FULL], (size_t)clsLanguageManager::mPtr->ui16TextsLens[LAN_FULL]) + ")";
                            }
                        }

                        if(curBan->sNick != NULL) {
                            if(((curBan->ui8Bits & clsBanManager::NICK) == clsBanManager::NICK) == true) {
								BanList += " " + string(clsLanguageManager::mPtr->sTexts[LAN_BANNED], (size_t)clsLanguageManager::mPtr->ui16TextsLens[LAN_BANNED]);
                            }
							BanList += " " + string(clsLanguageManager::mPtr->sTexts[LAN_NICK], (size_t)clsLanguageManager::mPtr->ui16TextsLens[LAN_NICK]) + ": " + string(curBan->sNick);
                        }

                        if(curBan->sBy != NULL) {
							BanList += " " + string(clsLanguageManager::mPtr->sTexts[LAN_BY], (size_t)clsLanguageManager::mPtr->ui16TextsLens[LAN_BY]) + ": " + string(curBan->sBy);
                        }

                        if(curBan->sReason != NULL) {
							BanList += " " + string(clsLanguageManager::mPtr->sTexts[LAN_REASON], (size_t)clsLanguageManager::mPtr->ui16TextsLens[LAN_REASON]) + ": " + string(curBan->sReason);
                        }

                        struct tm *tm = localtime(&curBan->tempbanexpire);
                        strftime(msg, 256, "%c\n", tm);

						BanList += " " + string(clsLanguageManager::mPtr->sTexts[LAN_EXPIRE], (size_t)clsLanguageManager::mPtr->ui16TextsLens[LAN_EXPIRE]) + ": " + string(msg);
                    }

                    if(iBanNum > 0) {
                        bIsEmpty = false;
                        BanList += "\n\n";
                    }
                }

                if(clsBanManager::mPtr->PermBanListS != NULL) {
                    bIsEmpty = false;

					BanList += string(clsLanguageManager::mPtr->sTexts[LAN_PERM_BANS], (size_t)clsLanguageManager::mPtr->ui16TextsLens[LAN_PERM_BANS]) + ":\n\n";

                    uint32_t iBanNum = 0;
                    BanItem *nextBan = clsBanManager::mPtr->PermBanListS;

                    while(nextBan != NULL) {
                        BanItem *curBan = nextBan;
    		            nextBan = curBan->next;

                        iBanNum++;
						BanList += "[ " + string(iBanNum) + " ]";
                        
                        if(curBan->sIp[0] != '\0') {
                            if(((curBan->ui8Bits & clsBanManager::IP) == clsBanManager::IP) == true) {
								BanList += " " + string(clsLanguageManager::mPtr->sTexts[LAN_BANNED], (size_t)clsLanguageManager::mPtr->ui16TextsLens[LAN_BANNED]);
							}
							BanList += " " + string(clsLanguageManager::mPtr->sTexts[LAN_IP], (size_t)clsLanguageManager::mPtr->ui16TextsLens[LAN_IP]) + ": " + string(curBan->sIp);
                            if(((curBan->ui8Bits & clsBanManager::FULL) == clsBanManager::FULL) == true) {
								BanList += " (" + string(clsLanguageManager::mPtr->sTexts[LAN_FULL], (size_t)clsLanguageManager::mPtr->ui16TextsLens[LAN_FULL]) + ")";
                            }
                        }

                        if(curBan->sNick != NULL) {
							if(((curBan->ui8Bits & clsBanManager::NICK) == clsBanManager::NICK) == true) {
								   BanList += " " + string(clsLanguageManager::mPtr->sTexts[LAN_BANNED], (size_t)clsLanguageManager::mPtr->ui16TextsLens[LAN_BANNED]);
                            }
							BanList += " " + string(clsLanguageManager::mPtr->sTexts[LAN_NICK], (size_t)clsLanguageManager::mPtr->ui16TextsLens[LAN_NICK]) + ": " + string(curBan->sNick);
                        }

                        if(curBan->sBy != NULL) {
							BanList += " " + string(clsLanguageManager::mPtr->sTexts[LAN_BY], (size_t)clsLanguageManager::mPtr->ui16TextsLens[LAN_BY]) + ": " + string(curBan->sBy);
                        }

                        if(curBan->sReason != NULL) {
							BanList += " " + string(clsLanguageManager::mPtr->sTexts[LAN_REASON], (size_t)clsLanguageManager::mPtr->ui16TextsLens[LAN_REASON]) + ": " + string(curBan->sReason);
                        }

                        BanList += "\n";
                    }
                }

                if(bIsEmpty == true) {
					BanList += string(clsLanguageManager::mPtr->sTexts[LAN_NO_BANS_FOUND], (size_t)clsLanguageManager::mPtr->ui16TextsLens[LAN_NO_BANS_FOUND]) + "...|";
                } else {
                    BanList += "|";
                }

                curUser->SendTextDelayed(BanList);
                return true;
            }

            // Hub commands: !gag
			if(strncasecmp(sCommand+1, "ag ", 3) == 0) {
                if(clsProfileManager::mPtr->IsAllowed(curUser, clsProfileManager::GAG) == false) {
                    SendNoPermission(curUser, fromPM);
                    return true;
                }

               	if(dlen < 5 || sCommand[4] == '\0') {
                    int imsgLen = CheckFromPm(curUser, fromPM);

                    int iret = sprintf(msg+imsgLen, "<%s> *** %s %cgag <%s>. %s!|", 
                        clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_SNTX_ERR_IN_CMD],
                        clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0], clsLanguageManager::mPtr->sTexts[LAN_NICK_LWR],
                        clsLanguageManager::mPtr->sTexts[LAN_NO_PARAM_GIVEN]);
                    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand4") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
                    return true;
                }

                sCommand += 4;

                // Self-gag ?
				if(strcasecmp(sCommand, curUser->sNick) == 0) {
                    int imsgLen = CheckFromPm(curUser, fromPM);

                    int iret = sprintf(msg+imsgLen, "<%s> %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                        clsLanguageManager::mPtr->sTexts[LAN_YOU_CANT_GAG_YOURSELF]);
                    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand6") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
                    return true;
           	    }

                if(dlen > 100) {
                    int imsgLen = CheckFromPm(curUser, fromPM);

                    int iret = sprintf(msg+imsgLen, "<%s> *** %s %cgag <%s>. %s!|",
                        clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_SNTX_ERR_IN_CMD],
                        clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0], clsLanguageManager::mPtr->sTexts[LAN_NICK_LWR],
                        clsLanguageManager::mPtr->sTexts[LAN_MAX_ALWD_NICK_LEN_64_CHARS]);
                    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand7") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
                    return true;
                }

                User *user = clsHashManager::mPtr->FindUser(sCommand, dlen-4);
        		if(user == NULL) {
                    int imsgLen = CheckFromPm(curUser, fromPM);

        			int iret = sprintf(msg+imsgLen, "<%s> *** %s: %s %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                        clsLanguageManager::mPtr->sTexts[LAN_ERROR], sCommand, clsLanguageManager::mPtr->sTexts[LAN_IS_NOT_IN_USERLIST]);
                    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand8") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
        			return true;
                }

                if(((user->ui32BoolBits & User::BIT_GAGGED) == User::BIT_GAGGED) == true) {
                    int imsgLen = CheckFromPm(curUser, fromPM);

        			int iret = sprintf(msg+imsgLen, "<%s> *** %s: %s %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                        clsLanguageManager::mPtr->sTexts[LAN_ERROR], user->sNick, clsLanguageManager::mPtr->sTexts[LAN_IS_ALREDY_GAGGED]);
                    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand10") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
        			return true;
                }

        		// PPK don't gag user with higher profile
        		if(user->iProfile != -1 && curUser->iProfile > user->iProfile) {
                    int imsgLen = CheckFromPm(curUser, fromPM);

                    int iret = sprintf(msg+imsgLen, "<%s> %s %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                        clsLanguageManager::mPtr->sTexts[LAN_NOT_ALW_TO_GAG], user->sNick);
                    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand12") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
        			return true;
                }

				UncountDeflood(curUser, fromPM);

                user->ui32BoolBits |= User::BIT_GAGGED;
                int imsgLen = sprintf(msg, "<%s> %s %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                    clsLanguageManager::mPtr->sTexts[LAN_YOU_GAGGED_BY], curUser->sNick);
                if(CheckSprintf(imsgLen, 1024, "clsHubCommands::DoCommand13") == true) {
                    user->SendCharDelayed(msg, imsgLen);
                }

                if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES] == true) {
                    if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES_AS_PM] == true) {
                        imsgLen = sprintf(msg, "%s $<%s> *** %s %s %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                            clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], curUser->sNick,
                            clsLanguageManager::mPtr->sTexts[LAN_HAS_GAGGED], user->sNick);
                        if(CheckSprintf(imsgLen, 1024, "clsHubCommands::DoCommand14") == true) {
							clsGlobalDataQueue::mPtr->SingleItemStore(msg, imsgLen, NULL, 0, clsGlobalDataQueue::SI_PM2OPS);
                        }
                    } else {
                        imsgLen = sprintf(msg, "<%s> *** %s %s %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], curUser->sNick,
                            clsLanguageManager::mPtr->sTexts[LAN_HAS_GAGGED], user->sNick);
                        if(CheckSprintf(imsgLen, 1024, "clsHubCommands::DoCommand15") == true) {
                            clsGlobalDataQueue::mPtr->AddQueueItem(msg, imsgLen, NULL, 0, clsGlobalDataQueue::CMD_OPS);
                        }
                    }
                } 
                        
                if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES] == false || ((curUser->ui32BoolBits & User::BIT_OPERATOR) == User::BIT_OPERATOR) == false) {
                    imsgLen = CheckFromPm(curUser, fromPM);

        			int iret = sprintf(msg+imsgLen, "<%s> %s %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], user->sNick,
                        clsLanguageManager::mPtr->sTexts[LAN_HAS_GAGGED]);
        			imsgLen += iret;
        			if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand17") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
                }
                return true;
            }

            // Hub commands: !getinfo
			if(strncasecmp(sCommand+1, "etinfo ", 7) == 0) {
                if(clsProfileManager::mPtr->IsAllowed(curUser, clsProfileManager::GETINFO) == false) {
                    SendNoPermission(curUser, fromPM);
                    return true;
                }

                if(dlen < 9 || sCommand[8] == 0) {
                    int imsgLen = CheckFromPm(curUser, fromPM);

                    int iret = sprintf(msg+imsgLen, "<%s> *** %s %cgetinfo <%s>. %s.|", 
                        clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_SNTX_ERR_IN_CMD],
                        clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0], clsLanguageManager::mPtr->sTexts[LAN_NICK_LWR],
                        clsLanguageManager::mPtr->sTexts[LAN_NO_PARAM_GIVEN]);
                    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand19") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
                    return true;
                }

                if(dlen > 100) {
                    int imsgLen = CheckFromPm(curUser, fromPM);

                    int iret = sprintf(msg+imsgLen, "<%s> *** %s %cgetinfo <%s>. %s!|",
                        clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_SNTX_ERR_IN_CMD],
                        clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0], clsLanguageManager::mPtr->sTexts[LAN_NICK_LWR],
                        clsLanguageManager::mPtr->sTexts[LAN_MAX_ALWD_NICK_LEN_64_CHARS]);
                    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand20") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
                    return true;
                }

                sCommand += 8;

                User *user = clsHashManager::mPtr->FindUser(sCommand, dlen-8);
                if(user == NULL) {
                    int imsgLen = CheckFromPm(curUser, fromPM);

                    int iret = sprintf(msg+imsgLen, "<%s> *** %s: %s %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                        clsLanguageManager::mPtr->sTexts[LAN_ERROR], sCommand, clsLanguageManager::mPtr->sTexts[LAN_IS_NOT_IN_USERLIST]);
                    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand21") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
                    return true;
                }
                
				UncountDeflood(curUser, fromPM);

                int imsgLen = CheckFromPm(curUser, fromPM);

                int iret = sprintf(msg+imsgLen, "<%s> " "\n%s: %s", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                    clsLanguageManager::mPtr->sTexts[LAN_NICK], user->sNick);
                imsgLen += iret;
                if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand23") == false) {
                    return true;
                }

                if(user->iProfile != -1) {
                    int iret = sprintf(msg+imsgLen, "\n%s: %s", clsLanguageManager::mPtr->sTexts[LAN_PROFILE], clsProfileManager::mPtr->ProfilesTable[user->iProfile]->sName);
                    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand24") == false) {
                        return true;
                    }
                }

                iret = sprintf(msg+imsgLen, "\n%s: %s ", clsLanguageManager::mPtr->sTexts[LAN_STATUS], clsLanguageManager::mPtr->sTexts[LAN_ONLINE_FROM]);
                imsgLen += iret;
                if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand25") == false) {
                    return true;
                }

                struct tm *tm = localtime(&user->LoginTime);
                iret = (int)strftime(msg+imsgLen, 256, "%c", tm);
                imsgLen += iret;
                if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand26") == false) {
                    return true;
                }

				if(user->sIPv4[0] != '\0') {
					iret = sprintf(msg+imsgLen, "\n%s: %s / %s\n%s: %0.02f %s", clsLanguageManager::mPtr->sTexts[LAN_IP], user->sIP, user->sIPv4,
						clsLanguageManager::mPtr->sTexts[LAN_SHARE_SIZE], (double)user->ui64SharedSize/1073741824, clsLanguageManager::mPtr->sTexts[LAN_GIGA_BYTES]);
					imsgLen += iret;
					if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand27-1") == false) {
						return true;
					}
				} else {
					iret = sprintf(msg+imsgLen, "\n%s: %s\n%s: %0.02f %s", clsLanguageManager::mPtr->sTexts[LAN_IP], user->sIP,
						clsLanguageManager::mPtr->sTexts[LAN_SHARE_SIZE], (double)user->ui64SharedSize/1073741824, clsLanguageManager::mPtr->sTexts[LAN_GIGA_BYTES]);
					imsgLen += iret;
					if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand27-2") == false) {
						return true;
					}
				}

                if(user->sDescription != NULL) {
                    int iret = sprintf(msg+imsgLen, "\n%s: ", clsLanguageManager::mPtr->sTexts[LAN_DESCRIPTION]);
                    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand28") == false) {
                        return true;
                    }
                    memcpy(msg+imsgLen, user->sDescription, user->ui8DescriptionLen);
                    imsgLen += user->ui8DescriptionLen;
                }

                if(user->sTag != NULL) {
                    int iret = sprintf(msg+imsgLen, "\n%s: ", clsLanguageManager::mPtr->sTexts[LAN_TAG]);
                    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand29") == false) {
                        return true;
                    }
                    memcpy(msg+imsgLen, user->sTag, user->ui8TagLen);
                    imsgLen += (int)user->ui8TagLen;
                }
                    
                if(user->sConnection != NULL) {
                    int iret = sprintf(msg+imsgLen, "\n%s: ", clsLanguageManager::mPtr->sTexts[LAN_CONNECTION]);
                    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand30") == false) {
                        return true;
                    }
                    memcpy(msg+imsgLen, user->sConnection, user->ui8ConnectionLen);
                    imsgLen += user->ui8ConnectionLen;
                }

                if(user->sEmail != NULL) {
                    int iret = sprintf(msg+imsgLen, "\n%s: ", clsLanguageManager::mPtr->sTexts[LAN_EMAIL]);
                    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand31") == false) {
                        return true;
                    }
                    memcpy(msg+imsgLen, user->sEmail, user->ui8EmailLen);
                    imsgLen += user->ui8EmailLen;
                }

                if(clsIpP2Country::mPtr->ui32Count != 0) {
                    iret = sprintf(msg+imsgLen, "\n%s: ", clsLanguageManager::mPtr->sTexts[LAN_COUNTRY]);
                    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand32") == false) {
                        return true;
                    }
                    memcpy(msg+imsgLen, clsIpP2Country::mPtr->GetCountry(user->ui8Country, false), 2);
                    imsgLen += 2;
                }

                msg[imsgLen] = '|';
                msg[imsgLen+1] = '\0';  
                curUser->SendCharDelayed(msg, imsgLen+1);
                return true;
            }

            // Hub commands: !gettempbans
			if(dlen == 11 && strncasecmp(sCommand+1, "ettempbans", 10) == 0) {
                if(clsProfileManager::mPtr->IsAllowed(curUser, clsProfileManager::GETBANLIST) == false) {
                    SendNoPermission(curUser, fromPM);
                    return true;
                }

				UncountDeflood(curUser, fromPM);

                int imsglen = CheckFromPm(curUser, fromPM);

                int iret = sprintf(msg+imsglen, "<%s> ", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC]);
                imsglen += iret;
                if(CheckSprintf1(iret, imsglen, 1024, "clsHubCommands::DoCommand33") == false) {
                    return true;
                }

				string BanList = string(msg, imsglen);

                if(clsBanManager::mPtr->TempBanListS != NULL) {
                    uint32_t iBanNum = 0;

                    time_t acc_time;
                    time(&acc_time);

                    BanItem *nextBan = clsBanManager::mPtr->TempBanListS;

                    while(nextBan != NULL) {
                        BanItem *curBan = nextBan;
    		            nextBan = curBan->next;

                        if(acc_time > curBan->tempbanexpire) {
							clsBanManager::mPtr->Rem(curBan);
                            delete curBan;

							continue;
                        }

						if(iBanNum == 0) {
							BanList += string(clsLanguageManager::mPtr->sTexts[LAN_TEMP_BANS], (size_t)clsLanguageManager::mPtr->ui16TextsLens[LAN_TEMP_BANS]) + ":\n\n";
                        }

                        iBanNum++;
						BanList += "[ " + string(iBanNum) + " ]";

                        if(curBan->sIp[0] != '\0') {
                            if(((curBan->ui8Bits & clsBanManager::IP) == clsBanManager::IP) == true) {
								BanList += " " + string(clsLanguageManager::mPtr->sTexts[LAN_BANNED], (size_t)clsLanguageManager::mPtr->ui16TextsLens[LAN_BANNED]);
                            }
							BanList += " " + string(clsLanguageManager::mPtr->sTexts[LAN_IP], (size_t)clsLanguageManager::mPtr->ui16TextsLens[LAN_IP]) + ": " + string(curBan->sIp);
                            if(((curBan->ui8Bits & clsBanManager::FULL) == clsBanManager::FULL) == true) {
								BanList += " (" + string(clsLanguageManager::mPtr->sTexts[LAN_FULL], (size_t)clsLanguageManager::mPtr->ui16TextsLens[LAN_FULL]) + ")";
                            }
                        }

                        if(curBan->sNick != NULL) {
                            if(((curBan->ui8Bits & clsBanManager::NICK) == clsBanManager::NICK) == true) {
								BanList += " " + string(clsLanguageManager::mPtr->sTexts[LAN_BANNED], (size_t)clsLanguageManager::mPtr->ui16TextsLens[LAN_BANNED]);
                            }
							BanList += " " + string(clsLanguageManager::mPtr->sTexts[LAN_NICK], (size_t)clsLanguageManager::mPtr->ui16TextsLens[LAN_NICK]) + ": " + string(curBan->sNick);
                        }

						if(curBan->sBy != NULL) {
							BanList += " " + string(clsLanguageManager::mPtr->sTexts[LAN_BY], (size_t)clsLanguageManager::mPtr->ui16TextsLens[LAN_BY]) + ": " + string(curBan->sBy);
                        }

                        if(curBan->sReason != NULL) {
							BanList += " " + string(clsLanguageManager::mPtr->sTexts[LAN_REASON], (size_t)clsLanguageManager::mPtr->ui16TextsLens[LAN_REASON]) + ": " + string(curBan->sReason);
                        }

                        struct tm *tm = localtime(&curBan->tempbanexpire);
                        strftime(msg, 256, "%c\n", tm);

						BanList += " " + string(clsLanguageManager::mPtr->sTexts[LAN_EXPIRE], (size_t)clsLanguageManager::mPtr->ui16TextsLens[LAN_EXPIRE]) + ": " + string(msg);
                    }

                    if(iBanNum > 0) {
                        BanList += "|";
                    } else {
						BanList += string(clsLanguageManager::mPtr->sTexts[LAN_NO_TEMP_BANS_FOUND], (size_t)clsLanguageManager::mPtr->ui16TextsLens[LAN_NO_TEMP_BANS_FOUND]) + "...|";
                    }
                } else {
					BanList += string(clsLanguageManager::mPtr->sTexts[LAN_NO_TEMP_BANS_FOUND], (size_t)clsLanguageManager::mPtr->ui16TextsLens[LAN_NO_TEMP_BANS_FOUND]) + "...|";
                }

				curUser->SendTextDelayed(BanList);
                return true;
            }

            // Hub commands: !getscripts
			if(dlen == 10 && strncasecmp(sCommand+1, "etscripts", 9) == 0) {
                if(clsProfileManager::mPtr->IsAllowed(curUser, clsProfileManager::RSTSCRIPTS) == false) {
                    SendNoPermission(curUser, fromPM);
                    return true;
                }

				UncountDeflood(curUser, fromPM);

                int imsglen = CheckFromPm(curUser, fromPM);

                int iret = sprintf(msg+imsglen, "<%s> ", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC]);
                imsglen += iret;
                if(CheckSprintf1(iret, imsglen, 1024, "clsHubCommands::DoCommand35") == false) {
                    return true;
                }

				string ScriptList(msg, imsglen);

				ScriptList += string(clsLanguageManager::mPtr->sTexts[LAN_SCRIPTS], (size_t)clsLanguageManager::mPtr->ui16TextsLens[LAN_SCRIPTS]) + ":\n\n";

				for(uint8_t ui8i = 0; ui8i < clsScriptManager::mPtr->ui8ScriptCount; ui8i++) {
					ScriptList += "[ " + string(clsScriptManager::mPtr->ScriptTable[ui8i]->bEnabled == true ? "1" : "0") +
						" ] " + string(clsScriptManager::mPtr->ScriptTable[ui8i]->sName);

					if(clsScriptManager::mPtr->ScriptTable[ui8i]->bEnabled == true) {
                        ScriptList += " ("+string(ScriptGetGC(clsScriptManager::mPtr->ScriptTable[ui8i]))+" kB)\n";
                    } else {
                        ScriptList += "\n";
                    }
				}

                ScriptList += "|";
				curUser->SendTextDelayed(ScriptList);
                return true;
            }

            // Hub commands: !getpermbans
			if(dlen == 11 && strncasecmp(sCommand+1, "etpermbans", 10) == 0) {
                if(clsProfileManager::mPtr->IsAllowed(curUser, clsProfileManager::GETBANLIST) == false) {
                    SendNoPermission(curUser, fromPM);
                    return true;
                }

				UncountDeflood(curUser, fromPM);

                int imsglen = CheckFromPm(curUser, fromPM);

                int iret = sprintf(msg+imsglen, "<%s> ", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC]);
                imsglen += iret;
                if(CheckSprintf1(iret, imsglen, 1024, "clsHubCommands::DoCommand37") == false) {
                    return true;
                }

				string BanList(msg, imsglen);

                if(clsBanManager::mPtr->PermBanListS != NULL) {
					BanList += string(clsLanguageManager::mPtr->sTexts[LAN_PERM_BANS], (size_t)clsLanguageManager::mPtr->ui16TextsLens[LAN_PERM_BANS]) + ":\n\n";

                    uint32_t iBanNum = 0;
                    BanItem *nextBan = clsBanManager::mPtr->PermBanListS;

                    while(nextBan != NULL) {
                        BanItem *curBan = nextBan;
    		            nextBan = curBan->next;

						iBanNum++;
						BanList += "[ " + string(iBanNum) + " ]";

                        if(curBan->sIp[0] != '\0') {
                            if(((curBan->ui8Bits & clsBanManager::IP) == clsBanManager::IP) == true) {
								BanList += " " + string(clsLanguageManager::mPtr->sTexts[LAN_BANNED], (size_t)clsLanguageManager::mPtr->ui16TextsLens[LAN_BANNED]);
                            }
							BanList += " " + string(clsLanguageManager::mPtr->sTexts[LAN_IP], (size_t)clsLanguageManager::mPtr->ui16TextsLens[LAN_IP]) + ": " + string(curBan->sIp);
                            if(((curBan->ui8Bits & clsBanManager::FULL) == clsBanManager::FULL) == true) {
								BanList += " (" + string(clsLanguageManager::mPtr->sTexts[LAN_FULL], (size_t)clsLanguageManager::mPtr->ui16TextsLens[LAN_FULL]) + ")";
                            }
                        }

                        if(curBan->sNick != NULL) {
                            if(((curBan->ui8Bits & clsBanManager::NICK) == clsBanManager::NICK) == true) {
								BanList += " " + string(clsLanguageManager::mPtr->sTexts[LAN_BANNED], (size_t)clsLanguageManager::mPtr->ui16TextsLens[LAN_BANNED]);
                            }
							BanList += " " + string(clsLanguageManager::mPtr->sTexts[LAN_NICK], (size_t)clsLanguageManager::mPtr->ui16TextsLens[LAN_NICK]) + ": " + string(curBan->sNick);
                        }

                        if(curBan->sBy != NULL) {
							BanList += " " + string(clsLanguageManager::mPtr->sTexts[LAN_BY], (size_t)clsLanguageManager::mPtr->ui16TextsLens[LAN_BY]) + ": " + string(curBan->sBy);
                        }

                        if(curBan->sReason != NULL) {
							BanList += " " + string(clsLanguageManager::mPtr->sTexts[LAN_REASON], (size_t)clsLanguageManager::mPtr->ui16TextsLens[LAN_REASON]) + ": " + string(curBan->sReason);
                        }

                        BanList += "\n";
                    }

					BanList += "|";
                } else {
					BanList += string(clsLanguageManager::mPtr->sTexts[LAN_NO_PERM_BANS_FOUND], (size_t)clsLanguageManager::mPtr->ui16TextsLens[LAN_NO_PERM_BANS_FOUND]) + "...|";
                }

				curUser->SendTextDelayed(BanList);
                return true;
            }

            // Hub commands: !getrangebans
			if(dlen == 12 && strncasecmp(sCommand+1, "etrangebans", 11) == 0) {
                if(clsProfileManager::mPtr->IsAllowed(curUser, clsProfileManager::GET_RANGE_BANS) == false) {
                    SendNoPermission(curUser, fromPM);
                    return true;
                }

				UncountDeflood(curUser, fromPM);

                int imsglen = CheckFromPm(curUser, fromPM);

                int iret = sprintf(msg+imsglen, "<%s> ", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC]);
                imsglen += iret;
                if(CheckSprintf1(iret, imsglen, 1024, "clsHubCommands::DoCommand39") == false) {
                    return true;
                }

				string BanList(msg, imsglen);
                bool bIsEmpty = true;

                if(clsBanManager::mPtr->RangeBanListS != NULL) {
                    uint32_t iBanNum = 0;

                    time_t acc_time;
                    time(&acc_time);

                    RangeBanItem *nextBan = clsBanManager::mPtr->RangeBanListS;

                    while(nextBan != NULL) {
                        RangeBanItem *curBan = nextBan;
    		            nextBan = curBan->next;

    		            if(((curBan->ui8Bits & clsBanManager::TEMP) == clsBanManager::TEMP) == false)
                            continue;

                        if(acc_time > curBan->tempbanexpire) {
							clsBanManager::mPtr->RemRange(curBan);
                            delete curBan;

							continue;
                        }

                        if(iBanNum == 0) {
							BanList += string(clsLanguageManager::mPtr->sTexts[LAN_TEMP_RANGE_BANS], (size_t)clsLanguageManager::mPtr->ui16TextsLens[LAN_TEMP_RANGE_BANS]) + ":\n\n";
                        }

                        iBanNum++;
						BanList += "[ " + string(iBanNum) + " ]";
						BanList += " " + string(clsLanguageManager::mPtr->sTexts[LAN_RANGE], (size_t)clsLanguageManager::mPtr->ui16TextsLens[LAN_RANGE]) +
							": " + string(curBan->sIpFrom) + "-" + string(curBan->sIpTo);

                        if(((curBan->ui8Bits & clsBanManager::FULL) == clsBanManager::FULL) == true) {
							BanList += " (" + string(clsLanguageManager::mPtr->sTexts[LAN_FULL], (size_t)clsLanguageManager::mPtr->ui16TextsLens[LAN_FULL]) + ")";
                        }

                        if(curBan->sBy != NULL) {
							BanList += " " + string(clsLanguageManager::mPtr->sTexts[LAN_BY], (size_t)clsLanguageManager::mPtr->ui16TextsLens[LAN_BY]) + ": " + string(curBan->sBy);
                        }

                        if(curBan->sReason != NULL) {
							BanList += " " + string(clsLanguageManager::mPtr->sTexts[LAN_REASON], (size_t)clsLanguageManager::mPtr->ui16TextsLens[LAN_REASON]) +
								": " + string(curBan->sReason);
                        }

                        struct tm *tm = localtime(&curBan->tempbanexpire);
                        strftime(msg, 256, "%c\n", tm);

						BanList += " " + string(clsLanguageManager::mPtr->sTexts[LAN_EXPIRE], (size_t)clsLanguageManager::mPtr->ui16TextsLens[LAN_EXPIRE]) + ": " + string(msg);
                    }

                    if(iBanNum > 0) {
                        bIsEmpty = false;
                        BanList += "\n\n";
                    }

                    iBanNum = 0;
                    nextBan = clsBanManager::mPtr->RangeBanListS;

                    while(nextBan != NULL) {
                        RangeBanItem *curBan = nextBan;
    		            nextBan = curBan->next;

    		            if(((curBan->ui8Bits & clsBanManager::PERM) == clsBanManager::PERM) == false)
                            continue;

                        if(iBanNum == 0) {
                            bIsEmpty = false;
							BanList += string(clsLanguageManager::mPtr->sTexts[LAN_PERM_RANGE_BANS], (size_t)clsLanguageManager::mPtr->ui16TextsLens[LAN_PERM_RANGE_BANS]) + ":\n\n";
                        }

                        iBanNum++;
						BanList += "[ " + string(iBanNum) + " ]";
						BanList += " " + string(clsLanguageManager::mPtr->sTexts[LAN_RANGE], (size_t)clsLanguageManager::mPtr->ui16TextsLens[LAN_RANGE]) +
							": " + string(curBan->sIpFrom) + "-" + string(curBan->sIpTo);

                        if(((curBan->ui8Bits & clsBanManager::FULL) == clsBanManager::FULL) == true) {
							BanList += " (" + string(clsLanguageManager::mPtr->sTexts[LAN_FULL], (size_t)clsLanguageManager::mPtr->ui16TextsLens[LAN_FULL]) + ")";
                        }

                        if(curBan->sBy != NULL) {
							BanList += " " + string(clsLanguageManager::mPtr->sTexts[LAN_BY], (size_t)clsLanguageManager::mPtr->ui16TextsLens[LAN_BY]) + ": " + string(curBan->sBy);
                        }

                        if(curBan->sReason != NULL) {
							BanList += " " + string(clsLanguageManager::mPtr->sTexts[LAN_REASON], (size_t)clsLanguageManager::mPtr->ui16TextsLens[LAN_REASON]) +
								": " + string(curBan->sReason);
                        }

                        BanList += "\n";
                    }
                }

                if(bIsEmpty == true) {
					BanList += string(clsLanguageManager::mPtr->sTexts[LAN_NO_RANGE_BANS_FOUND], (size_t)clsLanguageManager::mPtr->ui16TextsLens[LAN_NO_RANGE_BANS_FOUND]) + "...|";
                } else {
                    BanList += "|";
                }

				curUser->SendTextDelayed(BanList);
                return true;
            }

            // Hub commands: !getrangepermbans
			if(dlen == 16 && strncasecmp(sCommand+1, "etrangepermbans", 15) == 0) {
                if(clsProfileManager::mPtr->IsAllowed(curUser, clsProfileManager::GET_RANGE_BANS) == false) {
                    SendNoPermission(curUser, fromPM);
                    return true;
                }

				UncountDeflood(curUser, fromPM);

                int imsglen = CheckFromPm(curUser, fromPM);

                int iret = sprintf(msg+imsglen, "<%s> ", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC]);
                imsglen += iret;
                if(CheckSprintf1(iret, imsglen, 1024, "clsHubCommands::DoCommand41") == false) {
                    return true;
                }

				string BanList(msg, imsglen);
                bool bIsEmpty = true;

                if(clsBanManager::mPtr->RangeBanListS != NULL) {
                    uint32_t iBanNum = 0;
                    RangeBanItem *nextBan = clsBanManager::mPtr->RangeBanListS;

                    while(nextBan != NULL) {
                        RangeBanItem *curBan = nextBan;
    		            nextBan = curBan->next;

    		            if(((curBan->ui8Bits & clsBanManager::PERM) == clsBanManager::PERM) == false)
                            continue;

                        if(iBanNum == 0) {
                            bIsEmpty = false;

							BanList += string(clsLanguageManager::mPtr->sTexts[LAN_PERM_RANGE_BANS], (size_t)clsLanguageManager::mPtr->ui16TextsLens[LAN_PERM_RANGE_BANS]) + ":\n\n";
                        }

                        iBanNum++;
						BanList += "[ " + string(iBanNum) + " ]";
						BanList += " " + string(clsLanguageManager::mPtr->sTexts[LAN_RANGE], (size_t)clsLanguageManager::mPtr->ui16TextsLens[LAN_RANGE]) +
							": " + string(curBan->sIpFrom) + "-" + string(curBan->sIpTo);

                        if(((curBan->ui8Bits & clsBanManager::FULL) == clsBanManager::FULL) == true) {
							BanList += " (" + string(clsLanguageManager::mPtr->sTexts[LAN_FULL], (size_t)clsLanguageManager::mPtr->ui16TextsLens[LAN_FULL]) + ")";
                        }

                        if(curBan->sBy != NULL) {
							BanList += " " + string(clsLanguageManager::mPtr->sTexts[LAN_BY], (size_t)clsLanguageManager::mPtr->ui16TextsLens[LAN_BY]) + ": " + string(curBan->sBy);
                        }

                        if(curBan->sReason != NULL) {
							BanList += " " + string(clsLanguageManager::mPtr->sTexts[LAN_REASON], (size_t)clsLanguageManager::mPtr->ui16TextsLens[LAN_REASON]) +
								": " + string(curBan->sReason);
                        }

                        BanList += "\n";
                    }
                }

                if(bIsEmpty == true) {
					BanList += string(clsLanguageManager::mPtr->sTexts[LAN_NO_RANGE_PERM_BANS_FOUND], (size_t)clsLanguageManager::mPtr->ui16TextsLens[LAN_NO_RANGE_PERM_BANS_FOUND]) + "...|";
                } else {
                    BanList += "|";
                }

				curUser->SendTextDelayed(BanList);
                return true;
            }

            // Hub commands: !getrangetempbans
			if(dlen == 16 && strncasecmp(sCommand+1, "etrangetempbans", 15) == 0) {
                if(clsProfileManager::mPtr->IsAllowed(curUser, clsProfileManager::GET_RANGE_BANS) == false) {
                    SendNoPermission(curUser, fromPM);
                    return true;
                }

				UncountDeflood(curUser, fromPM);

                int imsglen = CheckFromPm(curUser, fromPM);

                int iret = sprintf(msg+imsglen, "<%s> ", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC]);
                imsglen += iret;
                if(CheckSprintf1(iret, imsglen, 1024, "clsHubCommands::DoCommand43") == false) {
                    return true;
                }

				string BanList(msg, imsglen);
                bool bIsEmpty = true;

                if(clsBanManager::mPtr->RangeBanListS != NULL) {
                    uint32_t iBanNum = 0;

                    time_t acc_time;
                    time(&acc_time);

                    RangeBanItem *nextBan = clsBanManager::mPtr->RangeBanListS;

                    while(nextBan != NULL) {
                        RangeBanItem *curBan = nextBan;
    		            nextBan = curBan->next;

    		            if(((curBan->ui8Bits & clsBanManager::TEMP) == clsBanManager::TEMP) == false)
                            continue;

                        if(acc_time > curBan->tempbanexpire) {
							clsBanManager::mPtr->RemRange(curBan);
                            delete curBan;

							continue;
                        }

                        if(iBanNum == 0) {
							BanList += string(clsLanguageManager::mPtr->sTexts[LAN_TEMP_RANGE_BANS], (size_t)clsLanguageManager::mPtr->ui16TextsLens[LAN_TEMP_RANGE_BANS]) + ":\n\n";
                        }

                        iBanNum++;
						BanList += "[ " + string(iBanNum) + " ]";
						BanList += " " + string(clsLanguageManager::mPtr->sTexts[LAN_RANGE], (size_t)clsLanguageManager::mPtr->ui16TextsLens[LAN_RANGE]) +
							": " + string(curBan->sIpFrom) + "-" + string(curBan->sIpTo);

                        if(((curBan->ui8Bits & clsBanManager::FULL) == clsBanManager::FULL) == true) {
							BanList += " (" + string(clsLanguageManager::mPtr->sTexts[LAN_FULL], (size_t)clsLanguageManager::mPtr->ui16TextsLens[LAN_FULL]) + ")";
                        }

						if(curBan->sBy != NULL) {
							BanList += " " + string(clsLanguageManager::mPtr->sTexts[LAN_BY], (size_t)clsLanguageManager::mPtr->ui16TextsLens[LAN_BY]) + ": " + string(curBan->sBy);
                        }

                        if(curBan->sReason != NULL) {
							BanList += " " + string(clsLanguageManager::mPtr->sTexts[LAN_REASON], (size_t)clsLanguageManager::mPtr->ui16TextsLens[LAN_REASON]) +
								": " + string(curBan->sReason);
                        }

                        struct tm *tm = localtime(&curBan->tempbanexpire);
                        strftime(msg, 256, "%c\n", tm);

						BanList += " " + string(clsLanguageManager::mPtr->sTexts[LAN_EXPIRE], (size_t)clsLanguageManager::mPtr->ui16TextsLens[LAN_EXPIRE]) + ": " + string(msg);
                    }

                    if(iBanNum > 0) {
                        bIsEmpty = false;
                    }
                }

                if(bIsEmpty == true) {
					BanList += string(clsLanguageManager::mPtr->sTexts[LAN_NO_RANGE_TEMP_BANS_FOUND], (size_t)clsLanguageManager::mPtr->ui16TextsLens[LAN_NO_RANGE_TEMP_BANS_FOUND]) + "...|";
                } else {
                    BanList += "|";
                }

				curUser->SendTextDelayed(BanList);
                return true;
            }

            return false;

        case 'n':
            // Hub commands: !nickban nick reason
			if(strncasecmp(sCommand+1, "ickban ", 7) == 0) {
                if(clsProfileManager::mPtr->IsAllowed(curUser, clsProfileManager::BAN) == false) {
                    SendNoPermission(curUser, fromPM);
                    return true;
                }

                if(dlen < 9) {
                    int imsgLen = CheckFromPm(curUser, fromPM);

               	    int iret = sprintf(msg+imsgLen, "<%s> *** %s %cnickban <%s> <%s>. %s.|", 
                       clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_SNTX_ERR_IN_CMD],
                       clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0], clsLanguageManager::mPtr->sTexts[LAN_NICK_LWR],
                       clsLanguageManager::mPtr->sTexts[LAN_REASON_LWR], clsLanguageManager::mPtr->sTexts[LAN_NO_PARAM_GIVEN]);
                    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand45") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
                    return true;
                }

                sCommand += 8;
                size_t szNickLen;
                char *reason = strchr(sCommand, ' ');

                if(reason != NULL) {
                    szNickLen = reason - sCommand;
                    reason[0] = '\0';
                    if(reason[1] == '\0') {
                        reason = NULL;
                    } else {
                        reason++;
                    }
                } else {
                    szNickLen = dlen-8;
                }

                if(sCommand[0] == '\0') {
                    int imsgLen = CheckFromPm(curUser, fromPM);

                    int iret = sprintf(msg+imsgLen, "<%s> %s %cnickban <%s> <%s>. %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                        clsLanguageManager::mPtr->sTexts[LAN_SNTX_ERR_IN_CMD], clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0],
                        clsLanguageManager::mPtr->sTexts[LAN_NICK_LWR], clsLanguageManager::mPtr->sTexts[LAN_REASON_LWR], 
                        clsLanguageManager::mPtr->sTexts[LAN_NO_NICK_SPECIFIED]);
                    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand47") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
                    return true;
                }

                if(szNickLen > 100) {
                    int imsgLen = CheckFromPm(curUser, fromPM);

                    int iret = sprintf(msg+imsgLen, "<%s> %s %cnickban <%s> <%s>. %s!|",
                        clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_SNTX_ERR_IN_CMD],
                        clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0], clsLanguageManager::mPtr->sTexts[LAN_NICK_LWR],
                        clsLanguageManager::mPtr->sTexts[LAN_REASON_LWR], clsLanguageManager::mPtr->sTexts[LAN_MAX_ALWD_NICK_LEN_64_CHARS]);
                    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand48") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
                    return true;
                }

                // Self-ban ?
				if(strcasecmp(sCommand, curUser->sNick) == 0) {
                    int imsgLen = CheckFromPm(curUser, fromPM);

                    int iret = sprintf(msg+imsgLen, "<%s> %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                        clsLanguageManager::mPtr->sTexts[LAN_YOU_CANT_BAN_YOURSELF]);
                    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand49") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
                    return true;
           	    }


                User *OtherUser = clsHashManager::mPtr->FindUser(sCommand, szNickLen);
                if(OtherUser != NULL) {
                    // PPK don't nickban user with higher profile
                    if(OtherUser->iProfile != -1 && curUser->iProfile > OtherUser->iProfile) {
                        int imsgLen = CheckFromPm(curUser, fromPM);

                        int iret = sprintf(msg+imsgLen, "<%s> %s %s %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                            clsLanguageManager::mPtr->sTexts[LAN_YOU_NOT_ALLOWED_TO], clsLanguageManager::mPtr->sTexts[LAN_BAN_LWR], OtherUser->sNick);
                        imsgLen += iret;
                        if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand51") == true) {
                            curUser->SendCharDelayed(msg, imsgLen);
                        }
                        return true;
                    }

                    UncountDeflood(curUser, fromPM);

                    int imsgLen;
                    if(reason != NULL && strlen(reason) > 512) {
                        imsgLen = sprintf(msg, "<%s> %s: ", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                            clsLanguageManager::mPtr->sTexts[LAN_YOU_HAD_BEEN_BANNED_BCS]);
                        if(CheckSprintf(imsgLen, 1024, "clsHubCommands::DoCommand52-1") == false) {
                   	        return true;
                        }
                        memcpy(msg+imsgLen, reason, 512);
                        imsgLen += (int)strlen(reason) + 2;
                        msg[imsgLen-2] = '.';
                        msg[imsgLen-1] = '|';
                        msg[imsgLen] = '\0';
                    } else {
                        imsgLen = sprintf(msg, "<%s> %s: %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                            clsLanguageManager::mPtr->sTexts[LAN_YOU_HAD_BEEN_BANNED_BCS],
                            reason == NULL ? clsLanguageManager::mPtr->sTexts[LAN_NO_REASON_SPECIFIED] : reason);
                        if(CheckSprintf(imsgLen, 1024, "clsHubCommands::DoCommand52-2") == false) {
                   	        return true;
                        }
                    }

                   	OtherUser->SendChar(msg, imsgLen);

                    if(clsBanManager::mPtr->NickBan(OtherUser, NULL, reason, curUser->sNick) == true) {
                        imsgLen = sprintf(msg, "[SYS] User %s (%s) nickbanned by %s", OtherUser->sNick, OtherUser->sIP, curUser->sNick);
                        if(CheckSprintf(imsgLen, 1024, "clsHubCommands::DoCommand53") == true) {
                            clsUdpDebug::mPtr->Broadcast(msg, imsgLen);
                        }
                        OtherUser->Close();
                    } else {
                        OtherUser->Close();
                        imsgLen = CheckFromPm(curUser, fromPM);

                        int iret = sprintf(msg+imsgLen, "<%s> *** %s %s %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                            clsLanguageManager::mPtr->sTexts[LAN_NICK], OtherUser->sNick, clsLanguageManager::mPtr->sTexts[LAN_IS_ALREDY_BANNED_DISCONNECT]);
                        imsgLen += iret;
                        if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand55") == true) {
                            curUser->SendCharDelayed(msg, imsgLen);
                        }
                        return true;
                    }
                } else {
                    return NickBan(curUser, sCommand, reason, fromPM);
                }

                if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES] == true) {
                    if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES_AS_PM] == true) {
                        int imsgLen;
                        if(reason == NULL) {
                            imsgLen = sprintf(msg, "%s $<%s> *** %s %s %s %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                                clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], curUser->sNick,
                                clsLanguageManager::mPtr->sTexts[LAN_ADDED_LWR], sCommand, clsLanguageManager::mPtr->sTexts[LAN_TO_BANS]);
                            if(CheckSprintf(imsgLen, 1024, "clsHubCommands::DoCommand59") == false) {
                                return true;
                            }
                        } else {
                            if(strlen(reason) > 512) {
                                imsgLen = sprintf(msg, "%s $<%s> *** %s %s %s %s: ", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                                    clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], sCommand, clsLanguageManager::mPtr->sTexts[LAN_HAS_BEEN_BANNED_BY],
                                    curUser->sNick, clsLanguageManager::mPtr->sTexts[LAN_BECAUSE_LWR]);
                                if(CheckSprintf(imsgLen, 1024, "clsHubCommands::DoCommand60-1") == false) {
                   	                return true;
                                }
                                memcpy(msg+imsgLen, reason, 512);
                                imsgLen += (int)strlen(reason) + 2;
                                msg[imsgLen-2] = '.';
                                msg[imsgLen-1] = '|';
                                msg[imsgLen] = '\0';
                            } else {
                                imsgLen = sprintf(msg, "%s $<%s> *** %s %s %s %s: %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                                    clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], sCommand,
                                    clsLanguageManager::mPtr->sTexts[LAN_HAS_BEEN_BANNED_BY], curUser->sNick, clsLanguageManager::mPtr->sTexts[LAN_BECAUSE_LWR], reason);
                                if(CheckSprintf(imsgLen, 1024, "clsHubCommands::DoCommand60-2") == false) {
                   	                return true;
                                }
                            }
                        }
						clsGlobalDataQueue::mPtr->SingleItemStore(msg, imsgLen, NULL, 0, clsGlobalDataQueue::SI_PM2OPS);
        	        } else {
                        int imsgLen;
                        if(reason == NULL) {
                            imsgLen = sprintf(msg, "<%s> *** %s %s %s %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], curUser->sNick,
                                clsLanguageManager::mPtr->sTexts[LAN_ADDED_LWR], sCommand, clsLanguageManager::mPtr->sTexts[LAN_TO_BANS]);
                            if(CheckSprintf(imsgLen, 1024, "clsHubCommands::DoCommand61") == false) {
                                return true;
                            }
                        } else {
                            if(strlen(reason) > 512) {
                                imsgLen = sprintf(msg, "<%s> *** %s %s %s %s: ", 
                                    clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], sCommand, clsLanguageManager::mPtr->sTexts[LAN_HAS_BEEN_BANNED_BY],
                                    curUser->sNick, clsLanguageManager::mPtr->sTexts[LAN_BECAUSE_LWR]);
                                if(CheckSprintf(imsgLen, 1024, "clsHubCommands::DoCommand62-1") == false) {
                   	                return true;
                                }
                                memcpy(msg+imsgLen, reason, 512);
                                imsgLen += (int)strlen(reason) + 2;
                                msg[imsgLen-2] = '.';
                                msg[imsgLen-1] = '|';
                                msg[imsgLen] = '\0';
                            } else {
                                imsgLen = sprintf(msg, "<%s> *** %s %s %s %s: %s.|",
                                    clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], sCommand,
                                    clsLanguageManager::mPtr->sTexts[LAN_HAS_BEEN_BANNED_BY], curUser->sNick, clsLanguageManager::mPtr->sTexts[LAN_BECAUSE_LWR], reason);
                                if(CheckSprintf(imsgLen, 1024, "clsHubCommands::DoCommand62-2") == false) {
                   	                return true;
                                }
                            }
                        }

            	        clsGlobalDataQueue::mPtr->AddQueueItem(msg, imsgLen, NULL, 0, clsGlobalDataQueue::CMD_OPS);
                  	}
        		}

        		if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES] == false || ((curUser->ui32BoolBits & User::BIT_OPERATOR) == User::BIT_OPERATOR) == false) {
                    int imsgLen = CheckFromPm(curUser, fromPM);

            	   	int iret = sprintf(msg+imsgLen, "<%s> %s %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], sCommand,
                       clsLanguageManager::mPtr->sTexts[LAN_ADDED_TO_BANS]);
                    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand64") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
        		}
                return true;
            }

            // Hub commands: !nicktempban nick time reason ...
            // PPK m = minutes, h = hours, d = day, w = weeks, M = months (30 day per month), Y = years (365 day per year)
			if(strncasecmp(sCommand+1, "icktempban ", 11) == 0) {
                if(clsProfileManager::mPtr->IsAllowed(curUser, clsProfileManager::TEMP_BAN) == false) {
                    SendNoPermission(curUser, fromPM);
                    return true;
                }

                if(dlen < 15) {
                    int imsgLen = CheckFromPm(curUser, fromPM);

               	    int iret = sprintf(msg+imsgLen, "<%s> *** %s %cnicktempban <%s> <%s> <%s>. %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                        clsLanguageManager::mPtr->sTexts[LAN_SNTX_ERR_IN_CMD], clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0],
                        clsLanguageManager::mPtr->sTexts[LAN_NICK_LWR], clsLanguageManager::mPtr->sTexts[LAN_TIME_LWR], clsLanguageManager::mPtr->sTexts[LAN_REASON_LWR], 
                        clsLanguageManager::mPtr->sTexts[LAN_NO_PARAM_GIVEN]);
                    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand66") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
                    return true;
                }

                // Now in sCommand we have nick, time and maybe reason
                char *sCmdParts[] = { NULL, NULL, NULL };
                uint16_t iCmdPartsLen[] = { 0, 0, 0 };

                uint8_t cPart = 0;

                sCmdParts[cPart] = sCommand+12; // nick start

                for(size_t szi = 12; szi < dlen; szi++) {
                    if(sCommand[szi] == ' ') {
                        sCommand[szi] = '\0';
                        iCmdPartsLen[cPart] = (uint16_t)((sCommand+szi)-sCmdParts[cPart]);

                        // are we on last space ???
                        if(cPart == 1) {
                            sCmdParts[2] = sCommand+szi+1;
                            iCmdPartsLen[2] = (uint16_t)(dlen-szi-1);
                            break;
                        }

                        cPart++;
                        sCmdParts[cPart] = sCommand+szi+1;
                    }
                }

                if(sCmdParts[2] == NULL && iCmdPartsLen[1] == 0 && sCmdParts[1] != NULL) {
                    iCmdPartsLen[1] = (uint16_t)(dlen-(sCmdParts[1]-sCommand));
                }

                if(sCmdParts[2] != NULL && iCmdPartsLen[2] == 0) {
                    sCmdParts[2] = NULL;
                }

                if(iCmdPartsLen[0] == 0 || iCmdPartsLen[1] == 0) {
                    int imsgLen = CheckFromPm(curUser, fromPM);

                   	int iret = sprintf(msg+imsgLen, "<%s> *** %s %cnicktempban <%s> <%s> <%s>. %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                        clsLanguageManager::mPtr->sTexts[LAN_SNTX_ERR_IN_CMD], clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0],
                        clsLanguageManager::mPtr->sTexts[LAN_NICK_LWR], clsLanguageManager::mPtr->sTexts[LAN_TIME_LWR], clsLanguageManager::mPtr->sTexts[LAN_REASON_LWR], 
                        clsLanguageManager::mPtr->sTexts[LAN_BAD_PARAMS_GIVEN]);
                    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand68") == false) {
                   	    curUser->SendCharDelayed(msg, imsgLen);
                    }
                    return true;
                }

                if(iCmdPartsLen[0] > 100) {
                    int imsgLen = CheckFromPm(curUser, fromPM);

                    int iret = sprintf(msg+imsgLen, "<%s> *** %s %cnicktempban <%s> <%s> <%s>. %s!|",
                        clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_SNTX_ERR_IN_CMD],
                        clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0], clsLanguageManager::mPtr->sTexts[LAN_NICK_LWR],
                        clsLanguageManager::mPtr->sTexts[LAN_TIME_LWR], clsLanguageManager::mPtr->sTexts[LAN_REASON_LWR],
                        clsLanguageManager::mPtr->sTexts[LAN_MAX_ALWD_NICK_LEN_64_CHARS]);
                    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand69") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
                    return true;
                }

                // Self-ban ?
				if(strcasecmp(sCmdParts[0], curUser->sNick) == 0) {
                    int imsgLen = CheckFromPm(curUser, fromPM);

                    int iret = sprintf(msg+imsgLen, "<%s> %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                        clsLanguageManager::mPtr->sTexts[LAN_YOU_CANT_BAN_YOURSELF]);
                    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand70") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
                    return true;
           	    }
           	    
                User *utempban = clsHashManager::mPtr->FindUser(sCmdParts[0], iCmdPartsLen[0]);
                if(utempban != NULL) {
                    // PPK don't tempban user with higher profile
                    if(utempban->iProfile != -1 && curUser->iProfile > utempban->iProfile) {
                        int imsgLen = CheckFromPm(curUser, fromPM);
 
                        int iret = sprintf(msg+imsgLen, "<%s> %s %s %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                            clsLanguageManager::mPtr->sTexts[LAN_YOU_NOT_ALLOWED_TO], clsLanguageManager::mPtr->sTexts[LAN_TEMP_BAN_NICK], utempban->sNick);
                        imsgLen += iret;
                        if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand72") == true) {
                            curUser->SendCharDelayed(msg, imsgLen);
                        }
                        return true;
                    }
                } else {
                    return TempNickBan(curUser, sCmdParts[0], sCmdParts[1], iCmdPartsLen[1], sCmdParts[2], fromPM);
                }

                char cTime = sCmdParts[1][iCmdPartsLen[1]-1];
                sCmdParts[1][iCmdPartsLen[1]-1] = '\0';
                int iTime = atoi(sCmdParts[1]);
                time_t acc_time, ban_time;

                if(iTime <= 0 || GenerateTempBanTime(cTime, (uint32_t)iTime, acc_time, ban_time) == false) {
                    int imsgLen = CheckFromPm(curUser, fromPM);

                    int iret = sprintf(msg+imsgLen, "<%s> *** %s %cnicktempban <%s> <%s> <%s>. %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                        clsLanguageManager::mPtr->sTexts[LAN_SNTX_ERR_IN_CMD], clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0],
                        clsLanguageManager::mPtr->sTexts[LAN_NICK_LWR], clsLanguageManager::mPtr->sTexts[LAN_TIME_LWR], clsLanguageManager::mPtr->sTexts[LAN_REASON_LWR], 
                        clsLanguageManager::mPtr->sTexts[LAN_BAD_TIME_SPECIFIED]);
                        imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand74") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
                    return true;
                }

                if(clsBanManager::mPtr->NickTempBan(utempban, NULL, sCmdParts[2], curUser->sNick, 0, ban_time) == false) {
                    int imsgLen = CheckFromPm(curUser, fromPM);

                    int iret = sprintf(msg+imsgLen, "<%s> *** %s %s %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_NICK], utempban->sNick,
                        clsLanguageManager::mPtr->sTexts[LAN_ALRD_BND_LNGR_TIME_DISCONNECTED]);
                    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand77") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }

                    imsgLen = sprintf(msg, "[SYS] Already temp banned user %s (%s) disconnected by %s", utempban->sNick, utempban->sIP, curUser->sNick);
                    if(CheckSprintf(imsgLen, 1024, "clsHubCommands::DoCommand78") == true) {
                        clsUdpDebug::mPtr->Broadcast(msg, imsgLen);
                    }

                    // Kick user
                    utempban->Close();
                    return true;
                }

				UncountDeflood(curUser, fromPM);

                char sTime[256];
                strcpy(sTime, formatTime((ban_time-acc_time)/60));

                // Send user a message that he has been tempbanned
                int imsgLen;
                if(sCmdParts[2] != NULL && iCmdPartsLen[2] > 512) {
                    imsgLen = sprintf(msg, "<%s> %s: %s %s: ", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_YOU_HAD_BEEN_TEMP_BANNED_TO], sTime,
                        clsLanguageManager::mPtr->sTexts[LAN_BECAUSE_LWR]);
                    if(CheckSprintf(imsgLen, 1024, "clsHubCommands::DoCommand81-1") == false) {
                   	    return true;
                    }
                    memcpy(msg+imsgLen, sCmdParts[2], 512);
                    imsgLen += iCmdPartsLen[2] + 2;
                    msg[imsgLen-2] = '.';
                    msg[imsgLen-1] = '|';
                    msg[imsgLen] = '\0';
                } else {
                    imsgLen = sprintf(msg, "<%s> %s: %s %s: %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_YOU_HAD_BEEN_TEMP_BANNED_TO], sTime,
                        clsLanguageManager::mPtr->sTexts[LAN_BECAUSE_LWR], sCmdParts[2] == NULL ? clsLanguageManager::mPtr->sTexts[LAN_NO_REASON_SPECIFIED] : sCmdParts[2]);
                    if(CheckSprintf(imsgLen, 1024, "clsHubCommands::DoCommand81-2") == false) {
                        return true;
                    }
                }

                utempban->SendChar(msg, imsgLen);

                if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES] == true) {
                    if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES_AS_PM] == true) {
                        int imsgLen;
                        if(sCmdParts[2] != NULL && iCmdPartsLen[2] > 512) {
                            imsgLen = sprintf(msg, "%s $<%s> *** %s %s %s %s: %s %s: ", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                                clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], sCmdParts[0],
                                clsLanguageManager::mPtr->sTexts[LAN_HAS_BEEN_TMPBND_BY], curUser->sNick, clsLanguageManager::mPtr->sTexts[LAN_TO_LWR],
                                sTime, clsLanguageManager::mPtr->sTexts[LAN_BECAUSE_LWR]);
                            if(CheckSprintf(imsgLen, 1024, "clsHubCommands::DoCommand82-1") == false) {
                       	        return true;
                            }
                            memcpy(msg+imsgLen, sCmdParts[2], 512);
                            imsgLen += iCmdPartsLen[2] + 2;
                            msg[imsgLen-2] = '.';
                            msg[imsgLen-1] = '|';
                            msg[imsgLen] = '\0';
                        } else {
                       	    imsgLen = sprintf(msg, "%s $<%s> *** %s %s %s %s: %s %s: %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                                clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], sCmdParts[0],
                                clsLanguageManager::mPtr->sTexts[LAN_HAS_BEEN_TMPBND_BY], curUser->sNick, clsLanguageManager::mPtr->sTexts[LAN_TO_LWR],
                                sTime, clsLanguageManager::mPtr->sTexts[LAN_BECAUSE_LWR],
                                sCmdParts[2] == NULL ? clsLanguageManager::mPtr->sTexts[LAN_NO_REASON_SPECIFIED] : sCmdParts[2]);
                            if(CheckSprintf(imsgLen, 1024, "clsHubCommands::DoCommand82-2") == false) {
                   	            return true;
                            }
                        }

						clsGlobalDataQueue::mPtr->SingleItemStore(msg, imsgLen, NULL, 0, clsGlobalDataQueue::SI_PM2OPS);
                   	} else {
                        int imsgLen;
                        if(sCmdParts[2] != NULL && iCmdPartsLen[2] > 512) {
                            imsgLen = sprintf(msg, "<%s> *** %s %s %s %s: %s %s: ", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], sCmdParts[0],
                                clsLanguageManager::mPtr->sTexts[LAN_HAS_BEEN_TMPBND_BY], curUser->sNick, clsLanguageManager::mPtr->sTexts[LAN_TO_LWR],
                                sTime, clsLanguageManager::mPtr->sTexts[LAN_BECAUSE_LWR]);
                            if(CheckSprintf(imsgLen, 1024, "clsHubCommands::DoCommand83-1") == false) {
                       	        return true;
                            }
                            memcpy(msg+imsgLen, sCmdParts[2], 512);
                            imsgLen += iCmdPartsLen[2] + 2;
                            msg[imsgLen-2] = '.';
                            msg[imsgLen-1] = '|';
                            msg[imsgLen] = '\0';
                        } else {
                       	    imsgLen = sprintf(msg, "<%s> *** %s %s %s %s: %s %s: %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], sCmdParts[0],
                                clsLanguageManager::mPtr->sTexts[LAN_HAS_BEEN_TMPBND_BY], curUser->sNick, clsLanguageManager::mPtr->sTexts[LAN_TO_LWR],
                                sTime, clsLanguageManager::mPtr->sTexts[LAN_BECAUSE_LWR],
                                sCmdParts[2] == NULL ? clsLanguageManager::mPtr->sTexts[LAN_NO_REASON_SPECIFIED] : sCmdParts[2]);
                            if(CheckSprintf(imsgLen, 1024, "clsHubCommands::DoCommand83-2") == false) {
                   	            return true;
                            }
                        }

                        clsGlobalDataQueue::mPtr->AddQueueItem(msg, imsgLen, NULL, 0, clsGlobalDataQueue::CMD_OPS);
                    }
                }

                if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES] == false || ((curUser->ui32BoolBits & User::BIT_OPERATOR) == User::BIT_OPERATOR) == false) {
                    int imsgLen = CheckFromPm(curUser, fromPM);

                    if(sCmdParts[2] != NULL && iCmdPartsLen[2] > 512) {
                   	    int iret = sprintf(msg+imsgLen, "<%s> %s %s: %s %s: ", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], sCmdParts[0],
                            clsLanguageManager::mPtr->sTexts[LAN_BEEN_TEMP_BANNED_TO], sTime, clsLanguageManager::mPtr->sTexts[LAN_BECAUSE_LWR]);
                        imsgLen += iret;
                        if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand85-1") == false) {
                            return true;
                        }

                        memcpy(msg+imsgLen, sCmdParts[2], 512);
                        imsgLen += iCmdPartsLen[2] + 2;
                        msg[imsgLen-2] = '.';
                        msg[imsgLen-1] = '|';
                        msg[imsgLen] = '\0';
                    } else {
                   	    int iret = sprintf(msg+imsgLen, "<%s> %s %s: %s %s: %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], sCmdParts[0],
                            clsLanguageManager::mPtr->sTexts[LAN_BEEN_TEMP_BANNED_TO], sTime, clsLanguageManager::mPtr->sTexts[LAN_BECAUSE_LWR],
                            sCmdParts[2] == NULL ? clsLanguageManager::mPtr->sTexts[LAN_NO_REASON_SPECIFIED] : sCmdParts[2]);
                        imsgLen += iret;
                        if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand85-2") == false) {
                            return true;
                        }
                    }

                    curUser->SendCharDelayed(msg, imsgLen);
                 }

                imsgLen = sprintf(msg, "[SYS] User %s (%s) tempbanned by %s", utempban->sNick, utempban->sIP, curUser->sNick);
                utempban->Close();
                if(CheckSprintf(imsgLen, 1024, "clsHubCommands::DoCommand86") == false) {
                    return true;
                }

                clsUdpDebug::mPtr->Broadcast(msg, imsgLen);
                return true;
            }

            return false;

        case 'b':
            // Hub commands: !banip ip reason
			if(strncasecmp(sCommand+1, "anip ", 5) == 0) {
                if(clsProfileManager::mPtr->IsAllowed(curUser, clsProfileManager::BAN) == false) {
                    SendNoPermission(curUser, fromPM);
                    return true;
                }

                if(dlen < 12) {
                    int imsgLen = CheckFromPm(curUser, fromPM);

               	    int iret = sprintf(msg+imsgLen, "<%s> %s %cbanip <%s> <%s>. %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                        clsLanguageManager::mPtr->sTexts[LAN_SNTX_ERR_IN_CMD],  clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0],
                        clsLanguageManager::mPtr->sTexts[LAN_IP], clsLanguageManager::mPtr->sTexts[LAN_REASON_LWR], 
                        clsLanguageManager::mPtr->sTexts[LAN_NO_PARAM_GIVEN]);
                    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand89") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
                    return true;
                }

                return BanIp(curUser, sCommand+6, fromPM, false);
            }

			if(strncasecmp(sCommand+1, "an ", 3) == 0) {
                if(clsProfileManager::mPtr->IsAllowed(curUser, clsProfileManager::BAN) == false) {
                    SendNoPermission(curUser, fromPM);
                    return true;
                }

                if(dlen < 5) {
                    int imsgLen = CheckFromPm(curUser, fromPM);

               	    int iret = sprintf(msg+imsgLen, "<%s> *** %s %cban <%s> <%s>. %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                        clsLanguageManager::mPtr->sTexts[LAN_SNTX_ERR_IN_CMD], clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0],
                        clsLanguageManager::mPtr->sTexts[LAN_NICK_LWR], clsLanguageManager::mPtr->sTexts[LAN_REASON_LWR], clsLanguageManager::mPtr->sTexts[LAN_NO_PARAM_GIVEN]);
                    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand91") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
                    return true;
                }
                
                return Ban(curUser, sCommand+4, fromPM, false);
            }

            return false;

        case 't':
            // Hub commands: !tempban nick time reason ... PPK m = minutes, h = hours, d = day, w = weeks, M = months (30 day per month), Y = years (365 day per year)
			if(strncasecmp(sCommand+1, "empban ", 7) == 0) {
                if(clsProfileManager::mPtr->IsAllowed(curUser, clsProfileManager::TEMP_BAN) == false) {
                    SendNoPermission(curUser, fromPM);
                    return true;
                }

                if(dlen < 11) {
                    int imsgLen = CheckFromPm(curUser, fromPM);

               	    int iret = sprintf(msg+imsgLen, "<%s> *** %s %ctempban <%s> <%s>. %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                        clsLanguageManager::mPtr->sTexts[LAN_SNTX_ERR_IN_CMD], clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0],
                        clsLanguageManager::mPtr->sTexts[LAN_NICK_LWR], clsLanguageManager::mPtr->sTexts[LAN_TIME_LWR], clsLanguageManager::mPtr->sTexts[LAN_NO_PARAM_GIVEN]);
                    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand93") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
                    return true;
                }

                return TempBan(curUser, sCommand+8, dlen-8, fromPM, false);
            }

            // !tempbanip nick time reason ... PPK m = minutes, h = hours, d = day, w = weeks, M = months (30 day per month), Y = years (365 day per year)
			if(strncasecmp(sCommand+1, "empbanip ", 9) == 0) {
                if(clsProfileManager::mPtr->IsAllowed(curUser, clsProfileManager::TEMP_BAN) == false) {
                    SendNoPermission(curUser, fromPM);
                    return true;
                }

                if(dlen < 14) {
                    int imsgLen = CheckFromPm(curUser, fromPM);

               	    int iret = sprintf(msg+imsgLen, "<%s> *** %s %ctempbanip <%s> <%s>. %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                        clsLanguageManager::mPtr->sTexts[LAN_SNTX_ERR_IN_CMD], clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0],
                        clsLanguageManager::mPtr->sTexts[LAN_IP], clsLanguageManager::mPtr->sTexts[LAN_TIME_LWR], clsLanguageManager::mPtr->sTexts[LAN_NO_PARAM_GIVEN]);
                    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand95") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
                    return true;
                }

                return TempBanIp(curUser, sCommand+10, dlen-10, fromPM, false);
            }

            // Hub commands: !tempunban
			if(strncasecmp(sCommand+1, "empunban ", 9) == 0) {
                if(clsProfileManager::mPtr->IsAllowed(curUser, clsProfileManager::TEMP_UNBAN) == false) {
                    SendNoPermission(curUser, fromPM);
                    return true;
                }

                if(dlen < 11 || sCommand[10] == '\0') {
                    int imsgLen = CheckFromPm(curUser, fromPM);

                    int iret = sprintf(msg+imsgLen, "<%s> *** %s %ctempunban <%s>. %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                        clsLanguageManager::mPtr->sTexts[LAN_SNTX_ERR_IN_CMD], clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0],
                        clsLanguageManager::mPtr->sTexts[LAN_IP_OR_NICK], clsLanguageManager::mPtr->sTexts[LAN_NO_PARAM_GIVEN]);
                    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand97") == false) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
                    return true;
                }

                sCommand += 10;

                if(dlen > 100) {
                    int imsgLen = CheckFromPm(curUser, fromPM);

                    int iret = sprintf(msg+imsgLen, "<%s> *** %s %ctempunban <%s>. %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                        clsLanguageManager::mPtr->sTexts[LAN_SNTX_ERR_IN_CMD], clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0],
                        clsLanguageManager::mPtr->sTexts[LAN_IP_OR_NICK], clsLanguageManager::mPtr->sTexts[LAN_MAX_ALWD_NICK_LEN_64_CHARS]);
                    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand98") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
                    return true;
                }

                if(clsBanManager::mPtr->TempUnban(sCommand) == false) {
                    int imsgLen = CheckFromPm(curUser, fromPM);

                    int iret = sprintf(msg+imsgLen, "<%s> *** %s %s %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                        clsLanguageManager::mPtr->sTexts[LAN_SORRY], sCommand, clsLanguageManager::mPtr->sTexts[LAN_IS_NOT_IN_MY_TEMP_BANS]);
                    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand99") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
                    return true;
                }

				UncountDeflood(curUser, fromPM);

                if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES] == true) {
                   	if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES_AS_PM] == true) {
           	            int imsgLen = sprintf(msg, "%s $<%s> *** %s %s %s %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                            clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], curUser->sNick,
                            clsLanguageManager::mPtr->sTexts[LAN_REMOVED_LWR], sCommand, clsLanguageManager::mPtr->sTexts[LAN_FROM_TEMP_BANS]);
                        if(CheckSprintf(imsgLen, 1024, "clsHubCommands::DoCommand100") == true) {
                            clsGlobalDataQueue::mPtr->SingleItemStore(msg, imsgLen, NULL, 0, clsGlobalDataQueue::SI_PM2OPS);
                        }
              	    } else {
                   	    int imsgLen = sprintf(msg, "<%s> *** %s %s %s %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], curUser->sNick,
                            clsLanguageManager::mPtr->sTexts[LAN_REMOVED_LWR], sCommand, clsLanguageManager::mPtr->sTexts[LAN_FROM_TEMP_BANS]);
                        if(CheckSprintf(imsgLen, 1024, "clsHubCommands::DoCommand101") == true) {
          	        	    clsGlobalDataQueue::mPtr->AddQueueItem(msg, imsgLen, NULL, 0, clsGlobalDataQueue::CMD_OPS);
                        }
          	    	}
                }

                if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES] == false || ((curUser->ui32BoolBits & User::BIT_OPERATOR) == User::BIT_OPERATOR) == false) {
                    int imsgLen = CheckFromPm(curUser, fromPM);

                    int iret = sprintf(msg+imsgLen, "<%s> %s %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], sCommand,
                        clsLanguageManager::mPtr->sTexts[LAN_REMOVED_FROM_TEMP_BANS]);
                    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand103") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
                }
                return true;
            }

            //Hub-Commands: !topic
			if(strncasecmp(sCommand+1, "opic ", 5) == 0 ) {
           	    if(clsProfileManager::mPtr->IsAllowed(curUser, clsProfileManager::TOPIC) == false) {
                    SendNoPermission(curUser, fromPM);
                    return true;
                }

                if(dlen < 7 || sCommand[6] == '\0') {
                    int imsgLen = CheckFromPm(curUser, fromPM);

                   	int iret = sprintf(msg+imsgLen, "<%s> *** %s %ctopic <%s>, %ctopic <off>. %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                        clsLanguageManager::mPtr->sTexts[LAN_SNTX_ERR_IN_CMD], clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0],
                        clsLanguageManager::mPtr->sTexts[LAN_NEW_TOPIC], clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0],
                        clsLanguageManager::mPtr->sTexts[LAN_NO_PARAM_GIVEN]);
                    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand105") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
                    return true;
                }

                sCommand += 6;

				UncountDeflood(curUser, fromPM);

                if(dlen-6 > 256) {
                    int imsgLen = CheckFromPm(curUser, fromPM);

                   	int iret = sprintf(msg+imsgLen, "<%s> *** %s %ctopic <%s>, %ctopic <off>. %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                        clsLanguageManager::mPtr->sTexts[LAN_SNTX_ERR_IN_CMD], clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0],
                        clsLanguageManager::mPtr->sTexts[LAN_NEW_TOPIC], clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0],
                        clsLanguageManager::mPtr->sTexts[LAN_MAX_ALWD_TOPIC_LEN_256_CHARS]);
                    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand106") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
                    return true;
                }

				if(strcasecmp(sCommand, "off") == 0) {
                    clsSettingManager::mPtr->SetText(SETTXT_HUB_TOPIC, "", 0);

                    clsGlobalDataQueue::mPtr->AddQueueItem(clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_NAME], clsSettingManager::mPtr->ui16PreTextsLens[clsSettingManager::SETPRETXT_HUB_NAME], NULL, 0,
                        clsGlobalDataQueue::CMD_HUBNAME);

                    int imsgLen = CheckFromPm(curUser, fromPM);

        			int iret = sprintf(msg+imsgLen, "<%s> *** %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                        clsLanguageManager::mPtr->sTexts[LAN_TOPIC_HAS_BEEN_CLEARED]);
                    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand108") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
                } else {
                    clsSettingManager::mPtr->SetText(SETTXT_HUB_TOPIC, sCommand, dlen-6);

                    clsGlobalDataQueue::mPtr->AddQueueItem(clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_NAME], clsSettingManager::mPtr->ui16PreTextsLens[clsSettingManager::SETPRETXT_HUB_NAME], NULL, 0,
                        clsGlobalDataQueue::CMD_HUBNAME);

                    int imsgLen = CheckFromPm(curUser, fromPM);

                    int iret = sprintf(msg+imsgLen, "<%s> *** %s: %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                        clsLanguageManager::mPtr->sTexts[LAN_TOPIC_HAS_BEEN_SET_TO], sCommand);
                    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand111") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
                }

                return true;
            }

            return false;

        case 'm':          
            //Hub-Commands: !myip
			if(dlen == 4 && strncasecmp(sCommand+1, "yip", 3) == 0) {
                int imsgLen = CheckFromPm(curUser, fromPM), iret = 0;

                if(curUser->sIPv4[0] != '\0') {
                    iret = sprintf(msg+imsgLen, "<%s> *** %s: %s / %s|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                        clsLanguageManager::mPtr->sTexts[LAN_YOUR_IP_IS], curUser->sIP, curUser->sIPv4);
                } else {
                    iret = sprintf(msg+imsgLen, "<%s> *** %s: %s|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                        clsLanguageManager::mPtr->sTexts[LAN_YOUR_IP_IS], curUser->sIP);
                }

                imsgLen += iret;
                if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand113") == true) {
                    curUser->SendCharDelayed(msg, imsgLen);
                }

                return true;
            }

            // Hub commands: !massmsg text
			if(strncasecmp(sCommand+1, "assmsg ", 7) == 0) {
                if(clsProfileManager::mPtr->IsAllowed(curUser, clsProfileManager::MASSMSG) == false) {
                    SendNoPermission(curUser, fromPM);
                    return true;
                }

                if(dlen < 9) {
                    int imsgLen = CheckFromPm(curUser, fromPM);

                    int iret = sprintf(msg+imsgLen, "<%s> *** %s %cmassmsg <%s>. %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                        clsLanguageManager::mPtr->sTexts[LAN_SNTX_ERR_IN_CMD], clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0],
                        clsLanguageManager::mPtr->sTexts[LAN_MESSAGE_LWR], clsLanguageManager::mPtr->sTexts[LAN_NO_PARAM_GIVEN]);
                    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand115") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
                    return true;
                }

				UncountDeflood(curUser, fromPM);

                if(dlen > 64000) {
                    sCommand[64000] = '\0';
                }

                int imsgLen = sprintf(clsServerManager::sGlobalBuffer, "%s $<%s> %s|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], curUser->sNick, sCommand+8);
                if(CheckSprintf(imsgLen, clsServerManager::szGlobalBufferSize, "clsHubCommands::DoCommand117") == true) {
					clsGlobalDataQueue::mPtr->SingleItemStore(clsServerManager::sGlobalBuffer, imsgLen, curUser, 0, clsGlobalDataQueue::SI_PM2ALL);
                }

                imsgLen = CheckFromPm(curUser, fromPM);

                int iret = sprintf(msg+imsgLen, "<%s> *** %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                    clsLanguageManager::mPtr->sTexts[LAN_MASSMSG_TO_ALL_SENT]);
                imsgLen += iret;
                if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand119") == true) {
                    curUser->SendCharDelayed(msg, imsgLen);
                }
                return true;
            }

            return false;

        case 'r':
			if(dlen == 14 && strncasecmp(sCommand+1, "estartscripts", 13) == 0) {
                if(clsProfileManager::mPtr->IsAllowed(curUser, clsProfileManager::RSTSCRIPTS) == false) {
                    SendNoPermission(curUser, fromPM);
                    return true;
                }

                if(clsSettingManager::mPtr->bBools[SETBOOL_ENABLE_SCRIPTING] == false) {
                    int imsgLen = CheckFromPm(curUser, fromPM);

                    int iret = sprintf(msg+imsgLen, "<%s> *** %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                        clsLanguageManager::mPtr->sTexts[LAN_ERR_SCRIPTS_DISABLED]);
                    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand121") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
                    return true;
                }

				UncountDeflood(curUser, fromPM);

                // post restart message
                if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES] == true) {
                    if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES_AS_PM] == true) {
                        int imsgLen = sprintf(msg, "%s $<%s> *** %s %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                            clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], curUser->sNick, clsLanguageManager::mPtr->sTexts[LAN_RESTARTED_SCRIPTS]);
                        if(CheckSprintf(imsgLen, 1024, "clsHubCommands::DoCommand122") == true) {
							clsGlobalDataQueue::mPtr->SingleItemStore(msg, imsgLen, NULL, 0, clsGlobalDataQueue::SI_PM2OPS);
                        }
           	        } else {
                        int imsgLen = sprintf(msg, "<%s> *** %s %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], curUser->sNick,
                            clsLanguageManager::mPtr->sTexts[LAN_RESTARTED_SCRIPTS]);
                        if(CheckSprintf(imsgLen, 1024, "clsHubCommands::DoCommand123") == true) {
               	            clsGlobalDataQueue::mPtr->AddQueueItem(msg, imsgLen, NULL, 0, clsGlobalDataQueue::CMD_OPS);
                        }
               	    }
                }

                if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES] == false || ((curUser->ui32BoolBits & User::BIT_OPERATOR) == User::BIT_OPERATOR) == false) {
                    int imsgLen = CheckFromPm(curUser, fromPM);

                    int iret = sprintf(msg+imsgLen, "<%s> %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                        clsLanguageManager::mPtr->sTexts[LAN_SCRIPTS_RESTARTED]);
                    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand125") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
                }

				clsScriptManager::mPtr->Restart();

                return true;
            }

			if(dlen == 7 && strncasecmp(sCommand+1, "estart", 6) == 0) {
                if(clsProfileManager::mPtr->IsAllowed(curUser, clsProfileManager::RSTHUB) == false) {
                    SendNoPermission(curUser, fromPM);
                    return true;
                }

				UncountDeflood(curUser, fromPM);

                // Send message to all that we are going to restart the hub
                int imsgLen = sprintf(msg,"<%s> %s. %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                    clsLanguageManager::mPtr->sTexts[LAN_HUB_WILL_BE_RESTARTED], clsLanguageManager::mPtr->sTexts[LAN_BACK_IN_FEW_MINUTES]);
                if(CheckSprintf(imsgLen, 1024, "clsHubCommands::DoCommand126") == true) {
                    clsUsers::mPtr->SendChat2All(curUser, msg, imsgLen, NULL);
                }

                // post a restart hub message
                clsEventQueue::mPtr->AddNormal(clsEventQueue::EVENT_RESTART, NULL);
                return true;
            }

            //Hub-Commands: !reloadtxt
			if(dlen == 9 && strncasecmp(sCommand+1, "eloadtxt", 8) == 0) {
           	    if(clsProfileManager::mPtr->IsAllowed(curUser, clsProfileManager::REFRESHTXT) == false) {
                    SendNoPermission(curUser, fromPM);
                    return true;
                }

               	if(clsSettingManager::mPtr->bBools[SETBOOL_ENABLE_TEXT_FILES] == false) {
                    int imsgLen = CheckFromPm(curUser, fromPM);

                  	int iret = sprintf(msg+imsgLen, "<%s> %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                        clsLanguageManager::mPtr->sTexts[LAN_TXT_SUP_NOT_ENABLED]);
                  	imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand128") == true) {
        	           curUser->SendCharDelayed(msg, imsgLen);
                    }
        	        return true;
                }

				UncountDeflood(curUser, fromPM);

               	clsTextFilesManager::mPtr->RefreshTextFiles();

               	if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES] == true) {
                    if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES_AS_PM] == true) {
              	      	int imsgLen = sprintf(msg, "%s $<%s> *** %s %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                            clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], curUser->sNick, clsLanguageManager::mPtr->sTexts[LAN_RELOAD_TXT_FILES_LWR]);
                        if(CheckSprintf(imsgLen, 1024, "clsHubCommands::DoCommand129") == true) {
							clsGlobalDataQueue::mPtr->SingleItemStore(msg, imsgLen, NULL, 0, clsGlobalDataQueue::SI_PM2OPS);
                        }
        	        } else {
            	      	int imsgLen = sprintf(msg, "<%s> *** %s %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], curUser->sNick,
                            clsLanguageManager::mPtr->sTexts[LAN_RELOAD_TXT_FILES_LWR]);
            	      	if(CheckSprintf(imsgLen, 1024, "clsHubCommands::DoCommand130") == true) {
                            clsGlobalDataQueue::mPtr->AddQueueItem(msg, imsgLen, NULL, 0, clsGlobalDataQueue::CMD_OPS);
                        }
                    }
                }

                if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES] == false || ((curUser->ui32BoolBits & User::BIT_OPERATOR) == User::BIT_OPERATOR) == false) {
                    int imsgLen = CheckFromPm(curUser, fromPM);

                  	int iret = sprintf(msg+imsgLen, "<%s> %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                        clsLanguageManager::mPtr->sTexts[LAN_TXT_FILES_RELOADED]);
                  	imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand132") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
                }
                return true;
            }

			// Hub commands: !restartscript scriptname
			if(strncasecmp(sCommand+1, "estartscript ", 13) == 0) {
                if(clsProfileManager::mPtr->IsAllowed(curUser, clsProfileManager::RSTSCRIPTS) == false) {
                    SendNoPermission(curUser, fromPM);
                    return true;
                }

                if(clsSettingManager::mPtr->bBools[SETBOOL_ENABLE_SCRIPTING] == false) {
                    int imsgLen = CheckFromPm(curUser, fromPM);

                    int iret = sprintf(msg+imsgLen, "<%s> *** %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                        clsLanguageManager::mPtr->sTexts[LAN_ERR_SCRIPTS_DISABLED]);
                  	imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand134") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
                    return true;
                }

                if(dlen < 15 || sCommand[14] == '\0') {
                    int imsgLen = CheckFromPm(curUser, fromPM);

                    int iret = sprintf(msg+imsgLen, "<%s> *** %s %crestartscript <%s>. %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                        clsLanguageManager::mPtr->sTexts[LAN_SNTX_ERR_IN_CMD], clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0],
                        clsLanguageManager::mPtr->sTexts[LAN_SCRIPTNAME_LWR], clsLanguageManager::mPtr->sTexts[LAN_NO_PARAM_GIVEN]);
                  	imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand136") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
                    return true;
                }

                sCommand += 14;

                if(dlen-14 > 256) {
                    int imsgLen = CheckFromPm(curUser, fromPM);

                    int iret = sprintf(msg+imsgLen, "<%s> *** %s %crestartscript <%s>. %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                        clsLanguageManager::mPtr->sTexts[LAN_SNTX_ERR_IN_CMD], clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0],
                        clsLanguageManager::mPtr->sTexts[LAN_SCRIPTNAME_LWR], clsLanguageManager::mPtr->sTexts[LAN_MAX_ALWD_SCRIPT_NAME_LEN_256_CHARS]);
                    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand137") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
                    return true;
                }

                Script * curScript = clsScriptManager::mPtr->FindScript(sCommand);
                if(curScript == NULL || curScript->bEnabled == false || curScript->LUA == NULL) {
                    int imsgLen = CheckFromPm(curUser, fromPM);

                    int iret = sprintf(msg+imsgLen, "<%s> *** %s %s %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                        clsLanguageManager::mPtr->sTexts[LAN_ERROR_SCRIPT], sCommand, clsLanguageManager::mPtr->sTexts[LAN_NOT_RUNNING]);
                    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand138") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
                    return true;
				}

				UncountDeflood(curUser, fromPM);

				// stop script
				clsScriptManager::mPtr->StopScript(curScript, false);

				// try to start script
				if(clsScriptManager::mPtr->StartScript(curScript, false) == true) {
                    if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES] == true) {
                        if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES_AS_PM] == true) {
                            int imsgLen = sprintf(msg, "%s $<%s> *** %s %s: %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                                clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], curUser->sNick,
                                clsLanguageManager::mPtr->sTexts[LAN_RESTARTED_SCRIPT], sCommand);
                            if(CheckSprintf(imsgLen, 1024, "clsHubCommands::DoCommand139") == true) {
								clsGlobalDataQueue::mPtr->SingleItemStore(msg, imsgLen, NULL, 0, clsGlobalDataQueue::SI_PM2OPS);
                            }
                        } else {
                            int imsgLen = sprintf(msg, "<%s> *** %s %s: %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], curUser->sNick,
                                clsLanguageManager::mPtr->sTexts[LAN_RESTARTED_SCRIPT], sCommand);
                            if(CheckSprintf(imsgLen, 1024, "clsHubCommands::DoCommand140") == true) {
                                clsGlobalDataQueue::mPtr->AddQueueItem(msg, imsgLen, NULL, 0, clsGlobalDataQueue::CMD_OPS);
                            }
                        }
                    }

                    if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES] == false ||
                        ((curUser->ui32BoolBits & User::BIT_OPERATOR) == User::BIT_OPERATOR) == false) {
                        int imsgLen = CheckFromPm(curUser, fromPM);

                        int iret = sprintf(msg+imsgLen, "<%s> %s %s %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                            clsLanguageManager::mPtr->sTexts[LAN_SCRIPT], sCommand, clsLanguageManager::mPtr->sTexts[LAN_RESTARTED_LWR]);
                        imsgLen += iret;
                        if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand142") == true) {
                            curUser->SendCharDelayed(msg, imsgLen);
                        }
                    } 
                    return true;
				} else {
                    int imsgLen = CheckFromPm(curUser, fromPM);

                    int iret = sprintf(msg+imsgLen, "<%s> *** %s %s %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                        clsLanguageManager::mPtr->sTexts[LAN_ERROR_SCRIPT], sCommand, clsLanguageManager::mPtr->sTexts[LAN_RESTART_FAILED]);
                    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand144") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
                    return true;
                }
            }

            // Hub commands: !rangeban fromip toip reason
			if(strncasecmp(sCommand+1, "angeban ", 8) == 0) {
                if(clsProfileManager::mPtr->IsAllowed(curUser, clsProfileManager::RANGE_BAN) == false) {
                    SendNoPermission(curUser, fromPM);
                    return true;
                }

                if(dlen < 24) {
                    int imsgLen = CheckFromPm(curUser, fromPM);

               	    int iret = sprintf(msg+imsgLen, "<%s> *** %s %crangeban <%s> <%s> <%s>. %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                        clsLanguageManager::mPtr->sTexts[LAN_SNTX_ERR_IN_CMD], clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0], clsLanguageManager::mPtr->sTexts[LAN_FROMIP],
                        clsLanguageManager::mPtr->sTexts[LAN_TOIP], clsLanguageManager::mPtr->sTexts[LAN_REASON_LWR], clsLanguageManager::mPtr->sTexts[LAN_NO_PARAM_GIVEN]);
                    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand148") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
                    return true;
                }

                return RangeBan(curUser, sCommand+9, dlen-9, fromPM, false);
            }

            // Hub commands: !rangetempban fromip toip time reason
			if(strncasecmp(sCommand+1, "angetempban ", 12) == 0) {
                if(clsProfileManager::mPtr->IsAllowed(curUser, clsProfileManager::RANGE_TBAN) == false) {
                    SendNoPermission(curUser, fromPM);
                    return true;
                }

                if(dlen < 31) {
                    int imsgLen = CheckFromPm(curUser, fromPM);

               	    int iret = sprintf(msg+imsgLen, "<%s> *** %s %crangetempban <%s> <%s> <%s> <%s>. %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                       clsLanguageManager::mPtr->sTexts[LAN_SNTX_ERR_IN_CMD], clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0], clsLanguageManager::mPtr->sTexts[LAN_FROMIP],
                       clsLanguageManager::mPtr->sTexts[LAN_TOIP],  clsLanguageManager::mPtr->sTexts[LAN_TIME_LWR], clsLanguageManager::mPtr->sTexts[LAN_REASON_LWR], 
                       clsLanguageManager::mPtr->sTexts[LAN_NO_PARAM_GIVEN]);
                    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand150") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
                    return true;
                }

                return RangeTempBan(curUser, sCommand+13, dlen-13, fromPM, false);
            }

            // Hub commands: !rangeunban fromip toip
			if(strncasecmp(sCommand+1, "angeunban ", 10) == 0) {
                if(dlen < 26) {
                    if(clsProfileManager::mPtr->IsAllowed(curUser, clsProfileManager::RANGE_UNBAN) == false && clsProfileManager::mPtr->IsAllowed(curUser, clsProfileManager::RANGE_TUNBAN) == false) {
                        SendNoPermission(curUser, fromPM);
                        return true;
                    }

                    int imsgLen = CheckFromPm(curUser, fromPM);

               	    int iret = sprintf(msg+imsgLen, "<%s> *** %s %crangeunban <%s> <%s>. %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                       clsLanguageManager::mPtr->sTexts[LAN_SNTX_ERR_IN_CMD], clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0],
                       clsLanguageManager::mPtr->sTexts[LAN_FROMIP], clsLanguageManager::mPtr->sTexts[LAN_TOIP], clsLanguageManager::mPtr->sTexts[LAN_NO_PARAM_GIVEN]);
                    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand152") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
                    return true;
                }

                if(clsProfileManager::mPtr->IsAllowed(curUser, clsProfileManager::RANGE_UNBAN) == true) {
                    return RangeUnban(curUser, sCommand+11, fromPM);
                } else if(clsProfileManager::mPtr->IsAllowed(curUser, clsProfileManager::RANGE_TUNBAN) == true) {
                    return RangeUnban(curUser, sCommand+11, fromPM, clsBanManager::TEMP);
                } else {
                    SendNoPermission(curUser, fromPM);
                    return true;
                }
            }

            // Hub commands: !rangetempunban fromip toip
			if(strncasecmp(sCommand+1, "angetempunban ", 14) == 0) {
                if(clsProfileManager::mPtr->IsAllowed(curUser, clsProfileManager::RANGE_TUNBAN) == false) {
                    SendNoPermission(curUser, fromPM);
                    return true;
                }

                if(dlen < 30) {
                    int imsgLen = CheckFromPm(curUser, fromPM);

               	    int iret = sprintf(msg+imsgLen, "<%s> *** %s %crangetempunban <%s> <%s>. %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                       clsLanguageManager::mPtr->sTexts[LAN_SNTX_ERR_IN_CMD], clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0],
                       clsLanguageManager::mPtr->sTexts[LAN_FROMIP], clsLanguageManager::mPtr->sTexts[LAN_TOIP], clsLanguageManager::mPtr->sTexts[LAN_NO_PARAM_GIVEN]);
                    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand154") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
                    return true;
                }

                return RangeUnban(curUser, sCommand+15, fromPM, clsBanManager::TEMP);
            }

            // Hub commands: !rangepermunban fromip toip
			if(strncasecmp(sCommand+1, "angepermunban ", 14) == 0) {
                if(clsProfileManager::mPtr->IsAllowed(curUser, clsProfileManager::RANGE_UNBAN) == false) {
                    SendNoPermission(curUser, fromPM);
                    return true;
                }

                if(dlen < 30) {
                    int imsgLen = CheckFromPm(curUser, fromPM);

               	    int iret = sprintf(msg+imsgLen, "<%s> *** %s %crangepermunban <%s> <%s>. %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                       clsLanguageManager::mPtr->sTexts[LAN_SNTX_ERR_IN_CMD], clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0],
                       clsLanguageManager::mPtr->sTexts[LAN_FROMIP], clsLanguageManager::mPtr->sTexts[LAN_TOIP], clsLanguageManager::mPtr->sTexts[LAN_NO_PARAM_GIVEN]);
                    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand156") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
                    return true;
                }

                return RangeUnban(curUser, sCommand+15, fromPM, clsBanManager::PERM);
            }

            // !reguser <nick> <profile_name>
			if(strncasecmp(sCommand+1, "eguser ", 7) == 0) {
                if(clsProfileManager::mPtr->IsAllowed(curUser, clsProfileManager::ADDREGUSER) == false) {
                    SendNoPermission(curUser, fromPM);
                    return true;
                }

                if(dlen > 255) {
                    int imsgLen = CheckFromPm(curUser, fromPM);

                    int iret = sprintf(msg+imsgLen, "<%s> *** %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_CMD_TOO_LONG]);
                    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand::RegUser1") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
                    return true;
                }

                char * sNick = sCommand+8; // nick start

                char * sProfile = strchr(sCommand+8, ' ');
                if(sProfile == NULL) {
                    int imsgLen = CheckFromPm(curUser, fromPM);

               	    int iret = sprintf(msg+imsgLen, "<%s> *** %s %creguser <%s> <%s>. %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                        clsLanguageManager::mPtr->sTexts[LAN_SNTX_ERR_IN_CMD], clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0], clsLanguageManager::mPtr->sTexts[LAN_NICK_LWR],
                        clsLanguageManager::mPtr->sTexts[LAN_PROFILENAME_LWR], clsLanguageManager::mPtr->sTexts[LAN_BAD_PARAMS_GIVEN]);
                    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand::RegUser2") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
                    return true;
                }

                sProfile[0] = '\0';
                sProfile++;

                int iProfile = clsProfileManager::mPtr->GetProfileIndex(sProfile);
                if(iProfile == -1) {
                    int imsgLen = CheckFromPm(curUser, fromPM);

               	    int iret = sprintf(msg+imsgLen, "<%s> *** %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_ERR_NO_PROFILE_GIVEN_NAME_EXIST]);
               	    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand::RegUser3") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
                    return true;
                }

                // check hierarchy
                // deny if curUser is not Master and tries add equal or higher profile
                if(curUser->iProfile > 0 && iProfile <= curUser->iProfile) {
                    int imsgLen = CheckFromPm(curUser, fromPM);

               	    int iret = sprintf(msg+imsgLen, "<%s> *** %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_YOU_NOT_ALLOWED_TO_ADD_USER_THIS_PROFILE]);
               	    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand::RegUser4") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
                    return true;
                }

                size_t szNickLen = strlen(sNick);

                // check if user is registered
                if(clsRegManager::mPtr->Find(sNick, szNickLen) != NULL) {
                    int imsgLen = CheckFromPm(curUser, fromPM);

                    int iret = sprintf(msg+imsgLen, "<%s> *** %s %s %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_USER], sNick,
                        clsLanguageManager::mPtr->sTexts[LAN_IS_ALREDY_REGISTERED]);
               	    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand::RegUser5") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
                    return true;
                }

                User * pUser = clsHashManager::mPtr->FindUser(sNick, szNickLen);
                if(pUser == NULL) {
                    int imsgLen = CheckFromPm(curUser, fromPM);

                    int iret = sprintf(msg+imsgLen, "<%s> *** %s %s %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_ERROR], sNick,
                        clsLanguageManager::mPtr->sTexts[LAN_IS_NOT_ONLINE]);
               	    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand::RegUser6") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
                    return true;
                }

				UncountDeflood(curUser, fromPM);

                if(pUser->uLogInOut == NULL) {
                    pUser->uLogInOut = new LoginLogout();
                    if(pUser->uLogInOut == NULL) {
                        pUser->ui32BoolBits |= User::BIT_ERROR;
                        pUser->Close();

                        AppendDebugLog("%s - [MEM] Cannot allocate new pUser->uLogInOut in clsHubCommands::DoCommand::RegUser\n", 0);
                        return true;
                    }
                }

                pUser->SetBuffer(sProfile);
                pUser->ui32BoolBits |= User::BIT_WAITING_FOR_PASS;

                int iMsgLen = sprintf(msg, "<%s> %s.|$GetPass|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_YOU_WERE_REGISTERED_PLEASE_ENTER_YOUR_PASSWORD]);
                if(CheckSprintf(iMsgLen, 1024, "clsHubCommands::DoCommand::RegUser7") == true) {
                    pUser->SendCharDelayed(msg, iMsgLen);
                }

                if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES] == true) {
                    if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES_AS_PM] == true) {
                        int imsgLen = sprintf(msg, "%s $<%s> *** %s %s %s %s %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],  clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                            curUser->sNick, clsLanguageManager::mPtr->sTexts[LAN_REGISTERED], sNick, clsLanguageManager::mPtr->sTexts[LAN_AS], sProfile);
                        if(CheckSprintf(imsgLen, 1024, "clsHubCommands::DoCommand::RegUser8") == true) {
							clsGlobalDataQueue::mPtr->SingleItemStore(msg, imsgLen, NULL, 0, clsGlobalDataQueue::SI_PM2OPS);
                        }
                    } else {
                        int imsgLen = sprintf(msg, "<%s> *** %s %s %s %s %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], curUser->sNick,
                            clsLanguageManager::mPtr->sTexts[LAN_REGISTERED], sNick, clsLanguageManager::mPtr->sTexts[LAN_AS], sProfile);
                        if(CheckSprintf(imsgLen, 1024, "clsHubCommands::DoCommand::RegUser9") == true) {
                            clsGlobalDataQueue::mPtr->AddQueueItem(msg, imsgLen, NULL, 0, clsGlobalDataQueue::CMD_OPS);
                        }
                    }
                }

                if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES] == false || ((curUser->ui32BoolBits & User::BIT_OPERATOR) == User::BIT_OPERATOR) == false) {
                    int imsgLen = CheckFromPm(curUser, fromPM);

                    int iret = sprintf(msg+imsgLen, "<%s> %s %s %s %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], sNick, clsLanguageManager::mPtr->sTexts[LAN_REGISTERED],
                        clsLanguageManager::mPtr->sTexts[LAN_AS], sProfile);
               	    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand::RegUser10") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
                }

                return true;
            }

            return false;

        case 'i':
            if(((curUser->ui32BoolBits & User::BIT_OPERATOR) == User::BIT_OPERATOR) == true) {
/*                // !ipinfo
                if(strnicmp(sCommand+1, "pinfo ", 6) == 0) {
               	    if(sqldb == NULL) {
                        int imsgLen = CheckFromPm(curUser, fromPM);

                        imsgLen += sprintf(msg+imsgLen, "<%s> *** UserStatistics are not running.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC]);
                        curUser->SendCharDelayed(msg, imsgLen);
                        return true;
                    }

                    if(dlen < 8) {
                        int imsgLen = CheckFromPm(curUser, fromPM);

                        imsgLen += sprintf(msg+imsgLen, "<%s> *** Syntax error in command %cipinfo <ip>: No parameter given.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0]);
                        curUser->SendCharDelayed(msg, imsgLen);
                        return true;
                   	}
                   	curUser->iChatMsgs--;
                    sCommand += 7; // move to command parameter

                    uint32_t a, b, c, d;
                    if(sCommand[0] != 0 && GetIpParts(sCommand, a, b, c, d) == true) {
                        sqldb->GetUsersByIp(sCommand, curUser);
                        return true;
                    }
                }

                if(strnicmp(sCommand+1, "prangeinfo ", 11) == 0) {
                    if(hubForm->UserStatistics == NULL) {
                        int imsgLen = CheckFromPm(curUser, fromPM);

                        imsgLen += sprintf(msg+imsgLen, "<%s> *** UserStatistics are not running.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC]);
                        curUser->SendCharDelayed(msg, imsgLen);
                        return true;
                    }

                    if(dlen < 13) {
                        int imsgLen = CheckFromPm(curUser, fromPM);

                        imsgLen += sprintf(msg+imsgLen, "<%s> *** Syntax error in command %ciprangeinfo <ip_part>: No parameter given.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0]);
                        curUser->SendCharDelayed(msg, imsgLen);
                        return true;
                   	}
                    curUser->iChatMsgs--;
                    sCommand += 12;

                    if(sCommand[0] != 0) {
                        TStringList *ReturnList=new TStringList();
                        hubForm->UserStatistics->GetUsersByIPRange(ReturnList, sCommand);
                        String IpInfo;
                        if(fromPM == true) {
                            int imsglen = sprintf(msg, "$To: %s From: %s $<%s> IP-info:\n", curUser->Nick, clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                                clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC]);
                            IpInfo += String(msg, imsglen);
                        }
                        for(uint32_t actPos=0; actPos<ReturnList->Count; actPos++) {
                            IpInfo += ReturnList->Strings[actPos]+ "\n";
                        }

                        IpInfo += "|";

                        curUser->SendTextDelayed(IpInfo);
                        ReturnList->Clear();
                        delete ReturnList;
                        return true;
                    }
                }*/
            }

            return false;

        case 'u':
/*            if(((curUser->ui32BoolBits & User::BIT_OPERATOR) == User::BIT_OPERATOR) == true) {
                // !userinfo
                if(strnicmp(sCommand+1, "serinfo ", 8) == 0) {
               	    if(hubForm->UserStatistics == NULL) {
                        int imsgLen = CheckFromPm(curUser, fromPM);

                        imsgLen += sprintf(msg+imsgLen, "<%s> *** UserStatistics are not running.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC]);
                        curUser->SendCharDelayed(msg, imsgLen);
                        return true;
                    }

                    if(dlen < 10) {
                        int imsgLen = CheckFromPm(curUser, fromPM);

                        imsgLen += sprintf(msg+imsgLen, "<%s> *** Syntax error in command %cuserinfo <username>: No parameter given.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0]);
                        curUser->SendCharDelayed(msg, imsgLen);
                        return true;
                   	}
                    curUser->iChatMsgs--;
                    sCommand += 9;

                    if(sCommand[0] != 0) {
               	        TStringList *ReturnList=new TStringList();
                   	    hubForm->UserStatistics->GetIPsByUsername(ReturnList, sCommand);
                   	    String UserInfo;
                        if(fromPM == true) {
                            int imsglen = sprintf(msg, "$To: %s From: %s $<%s> User-info:\n", curUser->Nick, clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                                clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC]);
                            UserInfo += String(msg, imsglen);
                        }
                        for(uint32_t actPos=0; actPos<ReturnList->Count; actPos++) {
           	                UserInfo += ReturnList->Strings[actPos]+ "\n";
                        }

                        UserInfo += "|";

                        curUser->SendTextDelayed(UserInfo);
           	            ReturnList->Clear();
                        delete ReturnList;
                        return true;
                    }
                }
            }*/

            // Hub commands: !unban
			if(strncasecmp(sCommand+1, "nban ", 5) == 0) {
                if(clsProfileManager::mPtr->IsAllowed(curUser, clsProfileManager::UNBAN) == false) {
                    SendNoPermission(curUser, fromPM);
                    return true;
                }

                if(dlen < 7 || sCommand[6] == '\0') {
                    int imsgLen = CheckFromPm(curUser, fromPM);

                    int iret = sprintf(msg+imsgLen, "<%s> *** %s %cunban <%s>. %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                        clsLanguageManager::mPtr->sTexts[LAN_SNTX_ERR_IN_CMD], clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0],
                        clsLanguageManager::mPtr->sTexts[LAN_IP_OR_NICK], clsLanguageManager::mPtr->sTexts[LAN_NO_PARAM_GIVEN]);
                    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand158") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
                    return true;
                }

                sCommand += 6;

                if(dlen > 100) {
                    int imsgLen = CheckFromPm(curUser, fromPM);

                    int iret = sprintf(msg+imsgLen, "<%s> *** %s %cunban <%s>. %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                        clsLanguageManager::mPtr->sTexts[LAN_SNTX_ERR_IN_CMD], clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0],
                        clsLanguageManager::mPtr->sTexts[LAN_IP_OR_NICK], clsLanguageManager::mPtr->sTexts[LAN_MAX_ALWD_NICK_LEN_64_CHARS]);
                    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand159") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
                    return true;
                }

                if(clsBanManager::mPtr->Unban(sCommand) == false) {
                    int imsgLen = CheckFromPm(curUser, fromPM);

                    int iret = sprintf(msg+imsgLen, "<%s> *** %s %s %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                        clsLanguageManager::mPtr->sTexts[LAN_SORRY], sCommand, clsLanguageManager::mPtr->sTexts[LAN_IS_NOT_IN_BANS]);
                    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand160") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
                    return true;
                }

				UncountDeflood(curUser, fromPM);

                if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES] == true) {
                   	if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES_AS_PM] == true) {
           	            int imsgLen = sprintf(msg, "%s $<%s> *** %s %s %s %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                            clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], curUser->sNick, clsLanguageManager::mPtr->sTexts[LAN_REMOVED_LWR],
                            sCommand, clsLanguageManager::mPtr->sTexts[LAN_FROM_BANS]);
                        if(CheckSprintf(imsgLen, 1024, "clsHubCommands::DoCommand161") == true) {
                            clsGlobalDataQueue::mPtr->SingleItemStore(msg, imsgLen, NULL, 0, clsGlobalDataQueue::SI_PM2OPS);
                        }
              	    } else {
                   	    int imsgLen = sprintf(msg, "<%s> *** %s %s %s %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], curUser->sNick,
                            clsLanguageManager::mPtr->sTexts[LAN_REMOVED_LWR], sCommand, clsLanguageManager::mPtr->sTexts[LAN_FROM_BANS]);
                        if(CheckSprintf(imsgLen, 1024, "clsHubCommands::DoCommand162") == true) {
          	        	    clsGlobalDataQueue::mPtr->AddQueueItem(msg, imsgLen, NULL, 0, clsGlobalDataQueue::CMD_OPS);
                        }
          	    	}
                }

                if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES] == false ||
                    ((curUser->ui32BoolBits & User::BIT_OPERATOR) == User::BIT_OPERATOR) == false) {
                    int imsgLen = CheckFromPm(curUser, fromPM);

                    int iret = sprintf(msg+imsgLen, "<%s> %s %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], sCommand,
                        clsLanguageManager::mPtr->sTexts[LAN_REMOVED_FROM_BANS]);
                    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand164") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
                }

                return true;
            }

            // Hub commands: !ungag
			if(strncasecmp(sCommand+1, "ngag ", 5) == 0) {
                if(clsProfileManager::mPtr->IsAllowed(curUser, clsProfileManager::GAG) == false) {
                    SendNoPermission(curUser, fromPM);
                    return true;
                }

               	if(dlen < 7 || sCommand[6] == '\0') {
                    int imsgLen = CheckFromPm(curUser, fromPM);

                   	int iret = sprintf(msg+imsgLen, "<%s> *** %s %cungag <%s>. %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                        clsLanguageManager::mPtr->sTexts[LAN_SNTX_ERR_IN_CMD], clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0],
                        clsLanguageManager::mPtr->sTexts[LAN_NICK_LWR], clsLanguageManager::mPtr->sTexts[LAN_NO_PARAM_GIVEN]);
                    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand166") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
                    return true;
                }

                sCommand += 6;

                if(dlen > 100) {
                    int imsgLen = CheckFromPm(curUser, fromPM);

                    int iret = sprintf(msg+imsgLen, "<%s> *** %s %cungag <%s>. %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                        clsLanguageManager::mPtr->sTexts[LAN_SNTX_ERR_IN_CMD], clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0],
                        clsLanguageManager::mPtr->sTexts[LAN_NICK_LWR], clsLanguageManager::mPtr->sTexts[LAN_MAX_ALWD_NICK_LEN_64_CHARS]);
                    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand167") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
                    return true;
                }

                User *user = clsHashManager::mPtr->FindUser(sCommand, dlen-6);
                if(user == NULL) {
                    int imsgLen = CheckFromPm(curUser, fromPM);

                    int iret = sprintf(msg+imsgLen, "<%s> *** %s %s %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                        clsLanguageManager::mPtr->sTexts[LAN_ERROR], sCommand, clsLanguageManager::mPtr->sTexts[LAN_IS_NOT_IN_USERLIST]);
                    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand168") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
                    return true;
                }

                if(((user->ui32BoolBits & User::BIT_GAGGED) == User::BIT_GAGGED) == false) {
                    int imsgLen = CheckFromPm(curUser, fromPM);

        			int iret = sprintf(msg+imsgLen, "<%s> *** %s %s %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                        clsLanguageManager::mPtr->sTexts[LAN_ERROR], sCommand, clsLanguageManager::mPtr->sTexts[LAN_IS_NOT_GAGGED]);
                    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand170") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
        			return true;
                }

				UncountDeflood(curUser, fromPM);

                user->ui32BoolBits &= ~User::BIT_GAGGED;
                int imsgLen = sprintf(msg, "<%s> %s %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                    clsLanguageManager::mPtr->sTexts[LAN_YOU_ARE_UNGAGGED_BY], curUser->sNick);
                if(CheckSprintf(imsgLen, 1024, "clsHubCommands::DoCommand171") == true) {
                    user->SendCharDelayed(msg, imsgLen);
                }

                if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES] == true) {
         	    	if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES_AS_PM] == true) {
          	    	    imsgLen = sprintf(msg, "%s $<%s> *** %s %s %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                            clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], curUser->sNick,
                            clsLanguageManager::mPtr->sTexts[LAN_HAS_UNGAGGED], user->sNick);
                        if(CheckSprintf(imsgLen, 1024, "clsHubCommands::DoCommand172") == true) {
							clsGlobalDataQueue::mPtr->SingleItemStore(msg, imsgLen, NULL, 0, clsGlobalDataQueue::SI_PM2OPS);
                        }
         	        } else {
                  		imsgLen = sprintf(msg, "<%s> *** %s %s %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], curUser->sNick,
                            clsLanguageManager::mPtr->sTexts[LAN_HAS_UNGAGGED], user->sNick);
                  		if(CheckSprintf(imsgLen, 1024, "clsHubCommands::DoCommand173") == true) {
                            clsGlobalDataQueue::mPtr->AddQueueItem(msg, imsgLen, NULL, 0, clsGlobalDataQueue::CMD_OPS);
                        }
          		    }
                }

                if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES] == false ||
                    ((curUser->ui32BoolBits & User::BIT_OPERATOR) == User::BIT_OPERATOR) == false) {
                    imsgLen = CheckFromPm(curUser, fromPM);

        			int iret = sprintf(msg+imsgLen, "<%s> %s %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], user->sNick,
                        clsLanguageManager::mPtr->sTexts[LAN_HAS_UNGAGGED]);
                    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand175") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
                }

                return true;
            }

            return false;

        case 'o':
            // Hub commands: !op nick
			if(strncasecmp(sCommand+1, "p ", 2) == 0) {
                if(clsProfileManager::mPtr->IsAllowed(curUser, clsProfileManager::TEMPOP) == false || ((curUser->ui32BoolBits & User::BIT_TEMP_OPERATOR) == User::BIT_TEMP_OPERATOR) == true) {
                    SendNoPermission(curUser, fromPM);
                    return true;
                }

                if(dlen < 4 || sCommand[3] == '\0') {
                    int imsgLen = CheckFromPm(curUser, fromPM);

               	    int iret = sprintf(msg+imsgLen, "<%s> *** %s %cop <%s>. %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                        clsLanguageManager::mPtr->sTexts[LAN_SNTX_ERR_IN_CMD], clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0],
                        clsLanguageManager::mPtr->sTexts[LAN_NICK_LWR], clsLanguageManager::mPtr->sTexts[LAN_NO_PARAM_GIVEN]);
                    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand177") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
                    return true;
                }

                sCommand += 3;

                if(dlen > 100) {
                    int imsgLen = CheckFromPm(curUser, fromPM);

                    int iret = sprintf(msg+imsgLen, "<%s> *** %s %cop <%s>. %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                        clsLanguageManager::mPtr->sTexts[LAN_SNTX_ERR_IN_CMD], clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0],
                        clsLanguageManager::mPtr->sTexts[LAN_NICK_LWR], clsLanguageManager::mPtr->sTexts[LAN_MAX_ALWD_NICK_LEN_64_CHARS]);
                    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand178") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
                    return true;
                }

                User *user = clsHashManager::mPtr->FindUser(sCommand, dlen-3);
                if(user == NULL) {
                    int imsgLen = CheckFromPm(curUser, fromPM);

                   	int iret = sprintf(msg+imsgLen, "<%s> *** %s %s %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                        clsLanguageManager::mPtr->sTexts[LAN_ERROR], sCommand, clsLanguageManager::mPtr->sTexts[LAN_IS_NOT_IN_USERLIST]);
                    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand179") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
                    return true;
                }

                if(((user->ui32BoolBits & User::BIT_OPERATOR) == User::BIT_OPERATOR) == true) {
                    int imsgLen = CheckFromPm(curUser, fromPM);

                   	int iret = sprintf(msg+imsgLen, "<%s> *** %s %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], user->sNick,
                        clsLanguageManager::mPtr->sTexts[LAN_ALREDY_IS_OP]);
                    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand181") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
                    return true;
                }

				UncountDeflood(curUser, fromPM);

                int pidx = clsProfileManager::mPtr->GetProfileIndex("Operator");
                if(pidx == -1) {
                    int imsgLen = CheckFromPm(curUser, fromPM);

                   	int iret = sprintf(msg+imsgLen, "<%s> *** %s. %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                        clsLanguageManager::mPtr->sTexts[LAN_ERROR], clsLanguageManager::mPtr->sTexts[LAN_OPERATOR_PROFILE_MISSING]);
                    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand183") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
                    return true;
                }

                user->ui32BoolBits |= User::BIT_OPERATOR;
                bool bAllowedOpChat = clsProfileManager::mPtr->IsAllowed(user, clsProfileManager::ALLOWEDOPCHAT);
                user->iProfile = pidx;
                user->ui32BoolBits |= User::BIT_TEMP_OPERATOR; // PPK ... to disallow adding more tempop by tempop user ;)
                clsUsers::mPtr->Add2OpList(user);

                int imsgLen = 0;
                if(((user->ui32SupportBits & User::SUPPORTBIT_QUICKLIST) == User::SUPPORTBIT_QUICKLIST) == false) {
                    imsgLen = sprintf(msg, "$LogedIn %s|", user->sNick);
                    if(CheckSprintf(imsgLen, 1024, "clsHubCommands::DoCommand184") == false) {
                        return true;
                    }
                }
                int iret = sprintf(msg+imsgLen, "<%s> *** %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_YOU_GOT_TEMP_OP]);
                imsgLen += iret;
                if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand185") == true) {
                    user->SendCharDelayed(msg, imsgLen);
                }
                clsGlobalDataQueue::mPtr->OpListStore(user->sNick);

                if(bAllowedOpChat != clsProfileManager::mPtr->IsAllowed(user, clsProfileManager::ALLOWEDOPCHAT)) {
                    if(clsSettingManager::mPtr->bBools[SETBOOL_REG_OP_CHAT] == true &&
                        (clsSettingManager::mPtr->bBools[SETBOOL_REG_BOT] == false || clsSettingManager::mPtr->bBotsSameNick == false)) {
                        if(((user->ui32SupportBits & User::SUPPORTBIT_NOHELLO) == User::SUPPORTBIT_NOHELLO) == false) {
                            user->SendCharDelayed(clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_OP_CHAT_HELLO],
                                clsSettingManager::mPtr->ui16PreTextsLens[clsSettingManager::SETPRETXT_OP_CHAT_HELLO]);
                        }
                        user->SendCharDelayed(clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_OP_CHAT_MYINFO],
                            clsSettingManager::mPtr->ui16PreTextsLens[clsSettingManager::SETPRETXT_OP_CHAT_MYINFO]);
                        imsgLen = sprintf(msg, "$OpList %s$$|", clsSettingManager::mPtr->sTexts[SETTXT_OP_CHAT_NICK]);
                        if(CheckSprintf(imsgLen, 1024, "clsHubCommands::DoCommand186") == true) {
                            user->SendCharDelayed(msg, imsgLen);
                        }
                    }
                }

                if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES] == true) {
                    if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES_AS_PM] == true) {
                        imsgLen = sprintf(msg, "%s $<%s> *** %s %s %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                            clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], curUser->sNick,
                            clsLanguageManager::mPtr->sTexts[LAN_SETS_OP_MODE_TO], user->sNick);
                        if(CheckSprintf(imsgLen, 1024, "clsHubCommands::DoCommand187") == true) {
							clsGlobalDataQueue::mPtr->SingleItemStore(msg, imsgLen, NULL, 0, clsGlobalDataQueue::SI_PM2OPS);
                        }
                    } else {
                        imsgLen = sprintf(msg, "<%s> *** %s %s %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], curUser->sNick,
                            clsLanguageManager::mPtr->sTexts[LAN_SETS_OP_MODE_TO], user->sNick);
                        if(CheckSprintf(imsgLen, 1024, "clsHubCommands::DoCommand188") == true) {
                            clsGlobalDataQueue::mPtr->AddQueueItem(msg, imsgLen, NULL, 0, clsGlobalDataQueue::CMD_OPS);
                        }
                    }
                }

                if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES] == false ||
                    ((curUser->ui32BoolBits & User::BIT_OPERATOR) == User::BIT_OPERATOR) == false) {
                    imsgLen = CheckFromPm(curUser, fromPM);

                   	int iret = sprintf(msg+imsgLen, "<%s> *** %s %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], user->sNick,
                        clsLanguageManager::mPtr->sTexts[LAN_GOT_OP_STATUS]);
                    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand190") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
                }

                return true;
            }

            // Hub commands: !opmassmsg text
			if(strncasecmp(sCommand+1, "pmassmsg ", 9) == 0) {
                if(clsProfileManager::mPtr->IsAllowed(curUser, clsProfileManager::MASSMSG) == false) {
                    SendNoPermission(curUser, fromPM);
                    return true;
                }

                if(dlen < 11) {
                    int imsgLen = CheckFromPm(curUser, fromPM);

                    int iret = sprintf(msg+imsgLen, "<%s> *** %s %copmassmsg <%s>. %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                        clsLanguageManager::mPtr->sTexts[LAN_SNTX_ERR_IN_CMD], clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0],
                        clsLanguageManager::mPtr->sTexts[LAN_MESSAGE_LWR], clsLanguageManager::mPtr->sTexts[LAN_NO_PARAM_GIVEN]);
                    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand192") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
                    return true;
                }

				UncountDeflood(curUser, fromPM);

                if(dlen > 64000) {
                    sCommand[64000] = '\0';
                }

                int imsgLen = sprintf(clsServerManager::sGlobalBuffer, "%s $<%s> %s|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], curUser->sNick, sCommand+10);
                if(CheckSprintf(imsgLen, clsServerManager::szGlobalBufferSize, "clsHubCommands::DoCommand193") == true) {
					clsGlobalDataQueue::mPtr->SingleItemStore(clsServerManager::sGlobalBuffer, imsgLen, curUser, 0, clsGlobalDataQueue::SI_PM2OPS);
                }

                imsgLen = CheckFromPm(curUser, fromPM);

                int iret = sprintf(msg+imsgLen, "<%s> *** %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                    clsLanguageManager::mPtr->sTexts[LAN_MASSMSG_TO_OPS_SND]);
                imsgLen += iret;
                if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand196") == true) {
                    curUser->SendCharDelayed(msg, imsgLen);
                }
                return true;
            }

            return false;

        case 'd':
            // Hub commands: !drop
			if(strncasecmp(sCommand+1, "rop ", 4) == 0) {
                if(clsProfileManager::mPtr->IsAllowed(curUser, clsProfileManager::DROP) == false) {
                    SendNoPermission(curUser, fromPM);
                    return true;
                }

                if(dlen < 6) {
                    int imsgLen = CheckFromPm(curUser, fromPM);

               	    int iret = sprintf(msg+imsgLen, "<%s> *** %s %cdrop <%s>. %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                        clsLanguageManager::mPtr->sTexts[LAN_SNTX_ERR_IN_CMD], clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0],
                        clsLanguageManager::mPtr->sTexts[LAN_NICK_LWR], clsLanguageManager::mPtr->sTexts[LAN_NO_PARAM_GIVEN]);
                    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand198") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
                    return true;
                }

                sCommand += 5;
                size_t szNickLen;

                char *reason = strchr(sCommand, ' ');
                if(reason != NULL) {
                    szNickLen = reason-sCommand;
                    reason[0] = '\0';
                    if(reason[1] == '\0') {
                        reason = NULL;
                    } else {
                        reason++;
                    }
                } else {
                    szNickLen = dlen-5;
                }

                if(szNickLen > 100) {
                    int imsgLen = CheckFromPm(curUser, fromPM);

                    int iret = sprintf(msg+imsgLen, "<%s> *** %s %cdrop <%s>. %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                        clsLanguageManager::mPtr->sTexts[LAN_SNTX_ERR_IN_CMD], clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0],
                        clsLanguageManager::mPtr->sTexts[LAN_NICK_LWR], clsLanguageManager::mPtr->sTexts[LAN_MAX_ALWD_NICK_LEN_64_CHARS]);
                    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand199") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
                    return true;
                }

                if(sCommand[0] == '\0') {
                    int imsgLen = CheckFromPm(curUser, fromPM);

               	    int iret = sprintf(msg+imsgLen, "<%s> *** %s %cdrop <%s>. %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                       clsLanguageManager::mPtr->sTexts[LAN_SNTX_ERR_IN_CMD], clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0],
                       clsLanguageManager::mPtr->sTexts[LAN_NICK_LWR], clsLanguageManager::mPtr->sTexts[LAN_NO_PARAM_GIVEN]);
                    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand200") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
                    return true;
                }
                
                // Self-drop ?
				if(strcasecmp(sCommand, curUser->sNick) == 0) {
                    int imsgLen = CheckFromPm(curUser, fromPM);

                    int iret= sprintf(msg+imsgLen, "<%s> %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                        clsLanguageManager::mPtr->sTexts[LAN_YOU_CANT_DROP_YOURSELF]);
                    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand202") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
                    return true;
           	    }
           	    
                User *user = clsHashManager::mPtr->FindUser(sCommand, szNickLen);
                if(user == NULL) {
                    int imsgLen = CheckFromPm(curUser, fromPM);

                   	int iret = sprintf(msg+imsgLen, "<%s> *** %s %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], sCommand,
                        clsLanguageManager::mPtr->sTexts[LAN_IS_NOT_IN_USERLIST]);
                    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand204") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
                    return true;
                }
                
        		// PPK don't drop user with higher profile
        		if(user->iProfile != -1 && curUser->iProfile > user->iProfile) {
                    int imsgLen = CheckFromPm(curUser, fromPM);

                    int iret = sprintf(msg+imsgLen, "<%s> %s %s %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                        clsLanguageManager::mPtr->sTexts[LAN_YOU_NOT_ALLOWED_TO], clsLanguageManager::mPtr->sTexts[LAN_DROP_LWR], user->sNick);
                    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand206") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
        			return true;
        		}

				UncountDeflood(curUser, fromPM);

                clsBanManager::mPtr->TempBan(user, reason, curUser->sNick, 0, 0, false);

                if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES] == true) {
               	    if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES_AS_PM] == true) {
                        int imsgLen;
                        if(reason != NULL && strlen(reason) > 512) {
                            imsgLen = sprintf(msg, "%s $<%s> *** %s %s %s %s %s %s: |", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                            clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], sCommand, clsLanguageManager::mPtr->sTexts[LAN_WITH_IP], user->sIP,
                            clsLanguageManager::mPtr->sTexts[LAN_DROP_ADDED_TEMPBAN_BY], curUser->sNick, clsLanguageManager::mPtr->sTexts[LAN_BECAUSE_LWR]);
                            if(CheckSprintf(imsgLen, 1024, "clsHubCommands::DoCommand207-1") == false) {
                       	        return true;
                            }
                            memcpy(msg+imsgLen, reason, 512);
                            imsgLen += (int)strlen(reason) + 2;
                            msg[imsgLen-2] = '.';
                            msg[imsgLen-1] = '|';
                            msg[imsgLen] = '\0';
                        } else {
        	       		    imsgLen = sprintf(msg, "%s $<%s> *** %s %s %s %s %s %s: %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                                clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], sCommand, clsLanguageManager::mPtr->sTexts[LAN_WITH_IP], user->sIP,
                                clsLanguageManager::mPtr->sTexts[LAN_DROP_ADDED_TEMPBAN_BY], curUser->sNick, clsLanguageManager::mPtr->sTexts[LAN_BECAUSE_LWR],
                                reason == NULL ? clsLanguageManager::mPtr->sTexts[LAN_NO_REASON_SPECIFIED] : reason);
                            if(CheckSprintf(imsgLen, 1024, "clsHubCommands::DoCommand207-2") == false) {
                       	        return true;
                            }
                        }

						clsGlobalDataQueue::mPtr->SingleItemStore(msg, imsgLen, NULL, 0, clsGlobalDataQueue::SI_PM2OPS);
         	    	} else {
                        int imsgLen;
                        if(reason != NULL && strlen(reason) > 512) {
                            imsgLen = sprintf(msg, "<%s> *** %s %s %s %s %s %s: |", 
                            clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], sCommand, clsLanguageManager::mPtr->sTexts[LAN_WITH_IP], user->sIP,
                            clsLanguageManager::mPtr->sTexts[LAN_DROP_ADDED_TEMPBAN_BY], curUser->sNick, clsLanguageManager::mPtr->sTexts[LAN_BECAUSE_LWR]);
                            if(CheckSprintf(imsgLen, 1024, "clsHubCommands::DoCommand207-1") == false) {
                       	        return true;
                            }
                            memcpy(msg+imsgLen, reason, 512);
                            imsgLen += (int)strlen(reason) + 2;
                            msg[imsgLen-2] = '.';
                            msg[imsgLen-1] = '|';
                            msg[imsgLen] = '\0';
                        } else {
        	       		    imsgLen = sprintf(msg, "<%s> *** %s %s %s %s %s %s: %s.|", 
                                clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], sCommand, clsLanguageManager::mPtr->sTexts[LAN_WITH_IP], user->sIP,
                                clsLanguageManager::mPtr->sTexts[LAN_DROP_ADDED_TEMPBAN_BY], curUser->sNick, clsLanguageManager::mPtr->sTexts[LAN_BECAUSE_LWR],
                                reason == NULL ? clsLanguageManager::mPtr->sTexts[LAN_NO_REASON_SPECIFIED] : reason);
                            if(CheckSprintf(imsgLen, 1024, "clsHubCommands::DoCommand207-2") == false) {
                       	        return true;
                            }
                        }

                        clsGlobalDataQueue::mPtr->AddQueueItem(msg, imsgLen, NULL, 0, clsGlobalDataQueue::CMD_OPS);
         			}
                }

                if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES] == false || ((curUser->ui32BoolBits & User::BIT_OPERATOR) == User::BIT_OPERATOR) == false) {
                    int imsgLen = CheckFromPm(curUser, fromPM);

                    if(reason != NULL && strlen(reason) > 512) {
                       	int iret = sprintf(msg+imsgLen, "<%s> %s %s %s %s: ", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], sCommand,
                            clsLanguageManager::mPtr->sTexts[LAN_WITH_IP], user->sIP, clsLanguageManager::mPtr->sTexts[LAN_DROP_ADDED_TEMPBAN_BCS]);
                        imsgLen += iret;
                        if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand210-1") == false) {
                            return true;
                        }

                        memcpy(msg+imsgLen, reason, 512);
                        imsgLen += (int)strlen(reason) + 2;
                        msg[imsgLen-2] = '.';
                        msg[imsgLen-1] = '|';
                        msg[imsgLen] = '\0';
                    } else {
                       	int iret = sprintf(msg+imsgLen, "<%s> %s %s %s %s: %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], sCommand,
                            clsLanguageManager::mPtr->sTexts[LAN_WITH_IP], user->sIP, clsLanguageManager::mPtr->sTexts[LAN_DROP_ADDED_TEMPBAN_BCS],
                            reason == NULL ? clsLanguageManager::mPtr->sTexts[LAN_NO_REASON_SPECIFIED] : reason);
                        imsgLen += iret;
                        if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand210-2") == false) {
                            return true;
                        }
                    }

                    curUser->SendCharDelayed(msg, imsgLen);
                }
                user->Close();
                return true;
            }

            // !DelRegUser <nick>   
			if(strncasecmp(sCommand+1, "elreguser ", 10) == 0) {
                if(clsProfileManager::mPtr->IsAllowed(curUser, clsProfileManager::DELREGUSER) == false) {
                    SendNoPermission(curUser, fromPM);
                    return true;
                }

                if(dlen > 255) {
                    int imsgLen = CheckFromPm(curUser, fromPM);

                    int iret = sprintf(msg+imsgLen, "<%s> *** %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                        clsLanguageManager::mPtr->sTexts[LAN_CMD_TOO_LONG]);
                    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand212") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
                    return true;
                } else if(dlen < 13) {
                    int imsgLen = CheckFromPm(curUser, fromPM);

               	    int iret = sprintf(msg+imsgLen, "<%s> *** %s %cdelreguser <%s>. %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                        clsLanguageManager::mPtr->sTexts[LAN_SNTX_ERR_IN_CMD], clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0],
                        clsLanguageManager::mPtr->sTexts[LAN_NICK_LWR], clsLanguageManager::mPtr->sTexts[LAN_NO_PARAM_GIVEN]);
                    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand214") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
                    return true;
                }

                sCommand += 11;
                size_t szNickLen = dlen-11;

                // find him
				RegUser *reg = clsRegManager::mPtr->Find(sCommand, szNickLen);
                if(reg == NULL) {
                    int imsgLen = CheckFromPm(curUser, fromPM);

               	    int iret = sprintf(msg+imsgLen, "<%s> *** %s %s %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                        clsLanguageManager::mPtr->sTexts[LAN_ERROR], sCommand, clsLanguageManager::mPtr->sTexts[LAN_IS_NOT_IN_REGS]);
                    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand216") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
                    return true;
                }

                // check hierarchy
                // deny if curUser is not Master and tries delete equal or higher profile
                if(curUser->iProfile > 0 && reg->ui16Profile <= curUser->iProfile) {
                    int imsgLen = CheckFromPm(curUser, fromPM);

               	    int iret = sprintf(msg+imsgLen, "<%s> *** %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                        clsLanguageManager::mPtr->sTexts[LAN_YOURE_NOT_ALWD_TO_DLT_USER_THIS_PRFL]);
                    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand218") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
                    return true;
                }

				UncountDeflood(curUser, fromPM);

                clsRegManager::mPtr->Delete(reg);

                if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES] == true) {
                    if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES_AS_PM] == true) {
                        int imsgLen = sprintf(msg, "%s $<%s> *** %s %s %s %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                            clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], curUser->sNick, clsLanguageManager::mPtr->sTexts[LAN_REMOVED_LWR],
                            sCommand, clsLanguageManager::mPtr->sTexts[LAN_FROM_REGS]);
                        if(CheckSprintf(imsgLen, 1024, "clsHubCommands::DoCommand219") == true) {
							clsGlobalDataQueue::mPtr->SingleItemStore(msg, imsgLen, NULL, 0, clsGlobalDataQueue::SI_PM2OPS);
                        }
                    } else {
                        int imsgLen = sprintf(msg, "<%s> *** %s %s %s %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], curUser->sNick,
                            clsLanguageManager::mPtr->sTexts[LAN_REMOVED_LWR], sCommand, clsLanguageManager::mPtr->sTexts[LAN_FROM_REGS]);
                        if(CheckSprintf(imsgLen, 1024, "clsHubCommands::DoCommand220") == true) {
                            clsGlobalDataQueue::mPtr->AddQueueItem(msg, imsgLen, NULL, 0, clsGlobalDataQueue::CMD_OPS);
                        }
                    }
                }

                if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES] == false || ((curUser->ui32BoolBits & User::BIT_OPERATOR) == User::BIT_OPERATOR) == false) {
                    int imsgLen = CheckFromPm(curUser, fromPM);

               	    int iret = sprintf(msg+imsgLen, "<%s> %s %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], sCommand,
                        clsLanguageManager::mPtr->sTexts[LAN_REMOVED_FROM_REGS]);
                    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand222") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
                }

                return true;
            }

            //Hub-Commands: !debug <port>|<list>|<off>
			if(strncasecmp(sCommand+1, "ebug ", 5) == 0) {
           	    if(((curUser->ui32BoolBits & User::BIT_OPERATOR) == User::BIT_OPERATOR) == false) {
                    SendNoPermission(curUser, fromPM);
                    return true;
                }

                if(dlen < 7) {
                    int imsgLen = CheckFromPm(curUser, fromPM);

                   	int iret = sprintf(msg+imsgLen, "<%s> *** %s %cdebug <%s>, %cdebug <off>. %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                        clsLanguageManager::mPtr->sTexts[LAN_SNTX_ERR_IN_CMD], clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0],
                        clsLanguageManager::mPtr->sTexts[LAN_PORT_LWR], clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0],
                        clsLanguageManager::mPtr->sTexts[LAN_NO_PARAM_GIVEN]);
                    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand225") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
                }

                sCommand += 6;

				if(strcasecmp(sCommand, "off") == 0) {
                    if(clsUdpDebug::mPtr->CheckUdpSub(curUser) == true) {
                        if(clsUdpDebug::mPtr->Remove(curUser) == true) {
                            UncountDeflood(curUser, fromPM);

                            int imsgLen = CheckFromPm(curUser, fromPM);

                            int iret = sprintf(msg+imsgLen, "<%s> *** %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                                clsLanguageManager::mPtr->sTexts[LAN_UNSUB_FROM_UDP_DBG]);
                            imsgLen += iret;
                            if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand227") == true) {
        				        curUser->SendCharDelayed(msg, imsgLen);
                            }

                            imsgLen = sprintf(msg, "[SYS] Debug subscription cancelled: %s (%s)", curUser->sNick, curUser->sIP);
                            if(CheckSprintf(imsgLen, 1024, "clsHubCommands::DoCommand228") == true) {
                                clsUdpDebug::mPtr->Broadcast(msg, imsgLen);
                            }
                            return true;
                        } else {
                            int imsgLen = CheckFromPm(curUser, fromPM);

                            int iret = sprintf(msg+imsgLen, "<%s> *** %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                                clsLanguageManager::mPtr->sTexts[LAN_UNABLE_FIND_UDP_DBG_INTER]);
                            imsgLen += iret;
                            if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand230") == true) {
        				        curUser->SendCharDelayed(msg, imsgLen);
                            }
                            return true;
                        }
                    } else {
                        int imsgLen = CheckFromPm(curUser, fromPM);

                        int iret = sprintf(msg+imsgLen, "<%s> *** %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                            clsLanguageManager::mPtr->sTexts[LAN_NOT_UDP_DEBUG_SUBSCRIB]);
                        imsgLen += iret;
                        if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand232") == true) {
        				    curUser->SendCharDelayed(msg, imsgLen);
                        }
                        return true;
                    }
                } else {
                    if(clsUdpDebug::mPtr->CheckUdpSub(curUser) == true) {
                        int imsgLen = CheckFromPm(curUser, fromPM);

                        int iret = sprintf(msg+imsgLen, "<%s> *** %s %cdebug <off> %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                            clsLanguageManager::mPtr->sTexts[LAN_ALRD_UDP_SUBSCRIP_TO_UNSUB], clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0],
                            clsLanguageManager::mPtr->sTexts[LAN_IN_MAINCHAT]);
                        imsgLen += iret;
                        if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand234") == true) {
        				    curUser->SendCharDelayed(msg, imsgLen);
                        }
                        return true;
                    }

                    int dbg_port = atoi(sCommand);
                    if(dbg_port <= 0) {
                        int imsgLen = CheckFromPm(curUser, fromPM);

                        int iret = sprintf(msg+imsgLen, "<%s> *** %s %cdebug <%s>, %cdebug <off>.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                            clsLanguageManager::mPtr->sTexts[LAN_SNTX_ERR_IN_CMD], clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0],
                            clsLanguageManager::mPtr->sTexts[LAN_PORT_LWR], clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0]);
                        imsgLen += iret;
                        if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand236") == true) {
        				    curUser->SendCharDelayed(msg, imsgLen);
                        }
                        return true;
                    }

                    if(clsUdpDebug::mPtr->New(curUser, dbg_port) == true) {
						UncountDeflood(curUser, fromPM);

                        int imsgLen = CheckFromPm(curUser, fromPM);

                        int iret = sprintf(msg+imsgLen, "<%s> *** %s %d. %s %cdebug <off> %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                            clsLanguageManager::mPtr->sTexts[LAN_SUBSCIB_UDP_DEBUG_ON_PORT], dbg_port, clsLanguageManager::mPtr->sTexts[LAN_TO_UNSUB_TYPE],
                            clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0], clsLanguageManager::mPtr->sTexts[LAN_IN_MAINCHAT]);
                        imsgLen += iret;
                        if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand238") == true) {
        				    curUser->SendCharDelayed(msg, imsgLen);
                        }

        				imsgLen = sprintf(msg, "[SYS] New Debug subscriber: %s (%s)", curUser->sNick, curUser->sIP);
        				if(CheckSprintf(imsgLen, 1024, "clsHubCommands::DoCommand239") == true) {
                            clsUdpDebug::mPtr->Broadcast(msg, imsgLen);
                        }
                        return true;
                    } else {
                        int imsgLen = CheckFromPm(curUser, fromPM);

                        int iret = sprintf(msg+imsgLen, "<%s> *** %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                            clsLanguageManager::mPtr->sTexts[LAN_UDP_DEBUG_SUBSCRIB_FAILED]);
                        imsgLen += iret;
                        if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand241") == true) {
        				    curUser->SendCharDelayed(msg, imsgLen);
                        }
                        return true;
                    }
                }
            }

            return false;

        case 'c':
/*            // !crash
			if(strncasecmp(sCommand+1, "rash", 4) == 0) {
                char * sTmp = (char*)calloc(392*1024*1024, 1);

                uint32_t iLen = 0;
                char *sData = ZlibUtility->CreateZPipe(sTmp, 392*1024*1024, iLen);

                if(iLen == 0) {
                    printf("0!");
                } else {
                    printf("LEN %u", iLen);
                    if(UserPutInSendBuf(curUser, sData, iLen)) {
                        UserTry2Send(curUser);
                    }
                }

                free(sTmp);

                return true;
            }
*/
            // !clrtempbans
			if(dlen == 11 && strncasecmp(sCommand+1, "lrtempbans", 10) == 0) {
                if(clsProfileManager::mPtr->IsAllowed(curUser, clsProfileManager::CLRTEMPBAN) == false) {
                    SendNoPermission(curUser, fromPM);
                    return true;
                }

				UncountDeflood(curUser, fromPM);
				clsBanManager::mPtr->ClearTemp();

                if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES] == true) {
                    if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES_AS_PM] == true) {
                        int imsgLen = sprintf(msg, "%s $<%s> *** %s %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                            curUser->sNick, clsLanguageManager::mPtr->sTexts[LAN_HAS_CLEARED_TEMPBANS]);
                        if(CheckSprintf(imsgLen, 1024, "clsHubCommands::DoCommand242") == true) {
							clsGlobalDataQueue::mPtr->SingleItemStore(msg, imsgLen, NULL, 0, clsGlobalDataQueue::SI_PM2OPS);
                        }
                    } else {
                        int imsgLen = sprintf(msg, "<%s> *** %s %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], curUser->sNick, clsLanguageManager::mPtr->sTexts[LAN_HAS_CLEARED_TEMPBANS]);
                        if(CheckSprintf(imsgLen, 1024, "clsHubCommands::DoCommand243") == true) {
                            clsGlobalDataQueue::mPtr->AddQueueItem(msg, imsgLen, NULL, 0, clsGlobalDataQueue::CMD_OPS);
                        }
                    }
                }

                if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES] == false || ((curUser->ui32BoolBits & User::BIT_OPERATOR) == User::BIT_OPERATOR) == false) {
                    int imsgLen = CheckFromPm(curUser, fromPM);

           	        int iret = sprintf(msg+imsgLen, "<%s> %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_TEMP_BANS_CLEARED]);
                    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand245") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
                }
                return true;
            }

            // !clrpermbans
			if(dlen == 11 && strncasecmp(sCommand+1, "lrpermbans", 10) == 0) {
                if(clsProfileManager::mPtr->IsAllowed(curUser, clsProfileManager::CLRPERMBAN) == false) {
                    SendNoPermission(curUser, fromPM);
                    return true;
                }

				UncountDeflood(curUser, fromPM);
				clsBanManager::mPtr->ClearPerm();

                if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES] == true) {
                    if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES_AS_PM] == true) {
                        int imsgLen = sprintf(msg, "%s $<%s> *** %s %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                            curUser->sNick, clsLanguageManager::mPtr->sTexts[LAN_HAS_CLEARED_PERMBANS]);
                        if(CheckSprintf(imsgLen, 1024, "clsHubCommands::DoCommand246") == true) {
							clsGlobalDataQueue::mPtr->SingleItemStore(msg, imsgLen, NULL, 0, clsGlobalDataQueue::SI_PM2OPS);
                        }
                    } else {
                        int imsgLen = sprintf(msg, "<%s> *** %s %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], curUser->sNick, clsLanguageManager::mPtr->sTexts[LAN_HAS_CLEARED_PERMBANS]);
                        if(CheckSprintf(imsgLen, 1024, "clsHubCommands::DoCommand247") == true) {
                            clsGlobalDataQueue::mPtr->AddQueueItem(msg, imsgLen, NULL, 0, clsGlobalDataQueue::CMD_OPS);
                        }
                    }
                }

                if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES] == false || ((curUser->ui32BoolBits & User::BIT_OPERATOR) == User::BIT_OPERATOR) == false) {
                    int imsgLen = CheckFromPm(curUser, fromPM);

           	        int iret = sprintf(msg+imsgLen, "<%s> %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_PERM_BANS_CLEARED]);
                    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand249") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
                }
                return true;
            }

            // !clrrangetempbans
			if(dlen == 16 && strncasecmp(sCommand+1, "lrrangetempbans", 15) == 0) {
                if(clsProfileManager::mPtr->IsAllowed(curUser, clsProfileManager::CLR_RANGE_TBANS) == false) {
                    SendNoPermission(curUser, fromPM);
                    return true;
                }

				UncountDeflood(curUser, fromPM);
				clsBanManager::mPtr->ClearTempRange();

                if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES] == true) {
                    if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES_AS_PM] == true) {
                        int imsgLen = sprintf(msg, "%s $<%s> *** %s %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                            curUser->sNick, clsLanguageManager::mPtr->sTexts[LAN_HAS_CLEARED_TEMP_RANGEBANS]);
                        if(CheckSprintf(imsgLen, 1024, "clsHubCommands::DoCommand250") == true) {
							clsGlobalDataQueue::mPtr->SingleItemStore(msg, imsgLen, NULL, 0, clsGlobalDataQueue::SI_PM2OPS);
                        }
                    } else {
                        int imsgLen = sprintf(msg, "<%s> *** %s %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], curUser->sNick, clsLanguageManager::mPtr->sTexts[LAN_HAS_CLEARED_TEMP_RANGEBANS]);
                        if(CheckSprintf(imsgLen, 1024, "clsHubCommands::DoCommand251") == true) {
                            clsGlobalDataQueue::mPtr->AddQueueItem(msg, imsgLen, NULL, 0, clsGlobalDataQueue::CMD_OPS);
                        }
                    }
                }

                if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES] == false || ((curUser->ui32BoolBits & User::BIT_OPERATOR) == User::BIT_OPERATOR) == false) {
                    int imsgLen = CheckFromPm(curUser, fromPM);

           	        int iret = sprintf(msg+imsgLen, "<%s> %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_TEMP_RANGE_BANS_CLEARED]);
                    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand253") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
                }
                return true;
            }

            // !clrrangepermbans
			if(dlen == 16 && strncasecmp(sCommand+1, "lrrangepermbans", 15) == 0) {
                if(clsProfileManager::mPtr->IsAllowed(curUser, clsProfileManager::CLR_RANGE_BANS) == false) {
                    SendNoPermission(curUser, fromPM);
                    return true;
                }

				UncountDeflood(curUser, fromPM);
				clsBanManager::mPtr->ClearPermRange();

                if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES] == true) {
                    if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES_AS_PM] == true) {
                        int imsgLen = sprintf(msg, "%s $<%s> *** %s %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                            curUser->sNick, clsLanguageManager::mPtr->sTexts[LAN_HAS_CLEARED_PERM_RANGEBANS]);
                        if(CheckSprintf(imsgLen, 1024, "clsHubCommands::DoCommand254") == true) {
							clsGlobalDataQueue::mPtr->SingleItemStore(msg, imsgLen, NULL, 0, clsGlobalDataQueue::SI_PM2OPS);
                        }
                    } else {
                        int imsgLen = sprintf(msg, "<%s> *** %s %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], curUser->sNick,
                            clsLanguageManager::mPtr->sTexts[LAN_HAS_CLEARED_PERM_RANGEBANS]);
                        if(CheckSprintf(imsgLen, 1024, "clsHubCommands::DoCommand255") == true) {
                            clsGlobalDataQueue::mPtr->AddQueueItem(msg, imsgLen, NULL, 0, clsGlobalDataQueue::CMD_OPS);
                        }
                    }
                }

                if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES] == false || ((curUser->ui32BoolBits & User::BIT_OPERATOR) == User::BIT_OPERATOR) == false) {
                    int imsgLen = CheckFromPm(curUser, fromPM);

           	        int iret = sprintf(msg+imsgLen, "<%s> %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_PERM_RANGE_BANS_CLEARED]);
           	        imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand257") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
                }
                return true;
            }

            // !checknickban nick
			if(strncasecmp(sCommand+1, "hecknickban ", 12) == 0) {
                if(clsProfileManager::mPtr->IsAllowed(curUser, clsProfileManager::GETBANLIST) == false) {
                    SendNoPermission(curUser, fromPM);
                    return true;
                }

				UncountDeflood(curUser, fromPM);

                if(dlen < 14 || sCommand[13] == '\0') {
                    int imsgLen = CheckFromPm(curUser, fromPM);

                   	int iret = sprintf(msg+imsgLen, "<%s> ***%s %cchecknickban <%s>. %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_SNTX_ERR_IN_CMD],
                        clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0], clsLanguageManager::mPtr->sTexts[LAN_NICK_LWR], clsLanguageManager::mPtr->sTexts[LAN_NO_PARAM_GIVEN]);
           	        imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand259") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
            		return true;
                }

                sCommand += 13;

                BanItem * Ban = clsBanManager::mPtr->FindNick(sCommand, dlen-13);
                if(Ban == NULL) {
                    int imsgLen = CheckFromPm(curUser, fromPM);   

                   	int iret = sprintf(msg+imsgLen, "<%s> *** %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_NO_BAN_FOUND]);
           	        imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand261") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
                    return true;
                }

                int imsgLen = CheckFromPm(curUser, fromPM);                        

                int iret = sprintf(msg+imsgLen, "<%s> %s: %s", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_BANNED_NICK], Ban->sNick);
           	    imsgLen += iret;
                if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand263") == false) {
                    return true;
                }

                if(Ban->sIp[0] != '\0') {
                    if(((Ban->ui8Bits & clsBanManager::IP) == clsBanManager::IP) == true) {
                        int iret = sprintf(msg+imsgLen, " %s", clsLanguageManager::mPtr->sTexts[LAN_BANNED]);
               	        imsgLen += iret;
                        if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand264") == false) {
                            return true;
                        }
                    }
                    int iret = sprintf(msg+imsgLen, " %s: %s", clsLanguageManager::mPtr->sTexts[LAN_IP], Ban->sIp);
           	        imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand265") == false) {
                        return true;
                    }
                    if(((Ban->ui8Bits & clsBanManager::FULL) == clsBanManager::FULL) == true) {
                        int iret = sprintf(msg+imsgLen, " (%s)", clsLanguageManager::mPtr->sTexts[LAN_FULL]);
           	            imsgLen += iret;
                        if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand266") == false) {
                            return true;
                        }
                    }
                }

                if(Ban->sReason != NULL) {
                    int iret = sprintf(msg+imsgLen, " %s: %s", clsLanguageManager::mPtr->sTexts[LAN_REASON], Ban->sReason);
           	        imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand267") == false) {
                        return true;
                    }
                }

                if(Ban->sBy != NULL) {
                    int iret = sprintf(msg+imsgLen, " %s: %s|", clsLanguageManager::mPtr->sTexts[LAN_BANNED_BY], Ban->sBy);
           	        imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand268") == false) {
                        return true;
                    }
                }

                if(((Ban->ui8Bits & clsBanManager::TEMP) == clsBanManager::TEMP) == true) {
                    char msg1[256];
                    struct tm *tm = localtime(&Ban->tempbanexpire);
                    strftime(msg1, 256, "%c", tm);
                    int iret = sprintf(msg+imsgLen, " %s: %s.|", clsLanguageManager::mPtr->sTexts[LAN_EXPIRE], msg1);
           	        imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand269") == false) {
                        return true;
                    }
                } else {
                    msg[imsgLen] = '.';
                    msg[imsgLen+1] = '|';
                    msg[imsgLen+2] = '\0';
                    imsgLen += 2;
                }
                curUser->SendCharDelayed(msg, imsgLen);
                return true;
            }

            // !checkipban ip
			if(strncasecmp(sCommand+1, "heckipban ", 10) == 0) {
                if(clsProfileManager::mPtr->IsAllowed(curUser, clsProfileManager::GETBANLIST) == false) {
                    SendNoPermission(curUser, fromPM);
                    return true;
                }

				UncountDeflood(curUser, fromPM);

                if(dlen < 18 || sCommand[11] == '\0') {
                    int imsgLen = CheckFromPm(curUser, fromPM);

                   	int iret = sprintf(msg+imsgLen, "<%s> *** %s %ccheckipban <%s>. %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_SNTX_ERR_IN_CMD],
                        clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0], clsLanguageManager::mPtr->sTexts[LAN_IP], clsLanguageManager::mPtr->sTexts[LAN_NO_PARAM_GIVEN]);
                    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand271") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
            		return true;
                }

                sCommand += 11;

				uint8_t ui128Hash[16];
				memset(ui128Hash, 0, 16);

                // check ip
                if(HashIP(sCommand, ui128Hash) == false) {
                    int imsgLen = CheckFromPm(curUser, fromPM);

               	    int iret = sprintf(msg+imsgLen, "<%s> *** %s %ccheckipban <%s>. %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_SNTX_ERR_IN_CMD],
                        clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0], clsLanguageManager::mPtr->sTexts[LAN_IP], clsLanguageManager::mPtr->sTexts[LAN_NO_VALID_IP_SPECIFIED]);
                    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand273") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
                    return true;
                }
                
                time_t acc_time;
                time(&acc_time);

                BanItem *nxtBan = clsBanManager::mPtr->FindIP(ui128Hash, acc_time);

                if(nxtBan != NULL) {
                    int imsgLen = CheckFromPm(curUser, fromPM);

                    int iret = sprintf(msg+imsgLen, "<%s> ", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC]);
                    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand275") == false) {
                        return true;
                    }

					string Bans(msg, imsgLen);
                    uint32_t iBanNum = 0;

                    while(nxtBan != NULL) {
                        BanItem *curBan = nxtBan;
                        nxtBan = curBan->hashiptablenext;

                        if(((curBan->ui8Bits & clsBanManager::TEMP) == clsBanManager::TEMP) == true) {
                            // PPK ... check if temban expired
                            if(acc_time >= curBan->tempbanexpire) {
								clsBanManager::mPtr->Rem(curBan);
                                delete curBan;

								continue;
                            }
                        }

                        iBanNum++;
                        imsgLen = sprintf(msg, "\n[%u] %s: %s", iBanNum, clsLanguageManager::mPtr->sTexts[LAN_BANNED_IP], curBan->sIp);
                        if(CheckSprintf(imsgLen, 1024, "clsHubCommands::DoCommand276") == false) {
                            return true;
                        }
                        if(((curBan->ui8Bits & clsBanManager::FULL) == clsBanManager::FULL) == true) {
                            int iret = sprintf(msg+imsgLen, " (%s)", clsLanguageManager::mPtr->sTexts[LAN_FULL]);
                            imsgLen += iret;
                            if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand277") == false) {
                                return true;
                            }
                        }

						Bans += string(msg, imsgLen);

                        if(curBan->sNick != NULL) {
                            imsgLen = 0;
                            if(((curBan->ui8Bits & clsBanManager::NICK) == clsBanManager::NICK) == true) {
                                imsgLen = sprintf(msg, " %s", clsLanguageManager::mPtr->sTexts[LAN_BANNED]);
                                if(CheckSprintf(imsgLen, 1024, "clsHubCommands::DoCommand278") == false) {
                                    return true;
                                }
                            }
                            int iret = sprintf(msg+imsgLen, " %s: %s", clsLanguageManager::mPtr->sTexts[LAN_NICK], curBan->sNick);
                            imsgLen += iret;
                            if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand279") == false) {
                                return true;
                            }
							Bans += msg;
                        }

                        if(curBan->sReason != NULL) {
                            imsgLen = sprintf(msg, " %s: %s", clsLanguageManager::mPtr->sTexts[LAN_REASON], curBan->sReason);
                            if(CheckSprintf(imsgLen, 1024, "clsHubCommands::DoCommand280") == false) {
                                return true;
                            }
							Bans += msg;
                        }

                        if(curBan->sBy != NULL) {
                            imsgLen = sprintf(msg, " %s: %s", clsLanguageManager::mPtr->sTexts[LAN_BANNED_BY], curBan->sBy);
                            if(CheckSprintf(imsgLen, 1024, "clsHubCommands::DoCommand281") == false) {
                                return true;
                            }
							Bans += msg;
                        }

                        if(((curBan->ui8Bits & clsBanManager::TEMP) == clsBanManager::TEMP) == true) {
                            struct tm *tm = localtime(&curBan->tempbanexpire);
                            strftime(msg, 256, "%c", tm);
							Bans += " " + string(clsLanguageManager::mPtr->sTexts[LAN_EXPIRE], (size_t)clsLanguageManager::mPtr->ui16TextsLens[LAN_EXPIRE]) + ": " + string(msg);
                        }
                    }

                    if(clsProfileManager::mPtr->IsAllowed(curUser, clsProfileManager::GET_RANGE_BANS) == false) {
                        Bans += "|";

						curUser->SendTextDelayed(Bans);
                        return true;
                    }

					RangeBanItem *nxtRangeBan = clsBanManager::mPtr->FindRange(ui128Hash, acc_time);

                    while(nxtRangeBan != NULL) {
                        RangeBanItem *curRangeBan = nxtRangeBan;
                        nxtRangeBan = curRangeBan->next;

						if(((curRangeBan->ui8Bits & clsBanManager::TEMP) == clsBanManager::TEMP) == true) {
							// PPK ... check if temban expired
							if(acc_time >= curRangeBan->tempbanexpire) {
								clsBanManager::mPtr->RemRange(curRangeBan);
								delete curRangeBan;

								continue;
							}
						}

                        if(memcmp(curRangeBan->ui128FromIpHash, ui128Hash, 16) <= 0 && memcmp(curRangeBan->ui128ToIpHash, ui128Hash, 16) >= 0) {
                            iBanNum++;
                            imsgLen = sprintf(msg, "\n[%u] %s: %s-%s", iBanNum, clsLanguageManager::mPtr->sTexts[LAN_RANGE], curRangeBan->sIpFrom, curRangeBan->sIpTo);
                            if(CheckSprintf(imsgLen, 1024, "clsHubCommands::DoCommand282") == false) {
                                return true;
                            }
                            if(((curRangeBan->ui8Bits & clsBanManager::FULL) == clsBanManager::FULL) == true) {
                                int iret= sprintf(msg+imsgLen, " (%s)", clsLanguageManager::mPtr->sTexts[LAN_FULL]);
                                imsgLen += iret;
                                if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand283") == false) {
                                    return true;
                                }
                            }

							Bans += string(msg, imsgLen);

                            if(curRangeBan->sReason != NULL) {
                                imsgLen = sprintf(msg, " %s: %s", clsLanguageManager::mPtr->sTexts[LAN_REASON], curRangeBan->sReason);
                                if(CheckSprintf(imsgLen, 1024, "clsHubCommands::DoCommand284") == false) {
                                    return true;
                                }
								Bans += msg;
                            }

                            if(curRangeBan->sBy != NULL) {
                                imsgLen = sprintf(msg, " %s: %s", clsLanguageManager::mPtr->sTexts[LAN_BANNED_BY], curRangeBan->sBy);
                                if(CheckSprintf(imsgLen, 1024, "clsHubCommands::DoCommand285") == false) {
                                    return true;
                                }
								Bans += msg;
                            }

                            if(((curRangeBan->ui8Bits & clsBanManager::TEMP) == clsBanManager::TEMP) == true) {
                                struct tm *tm = localtime(&curRangeBan->tempbanexpire);
                                strftime(msg, 256, "%c", tm);
								Bans += " " + string(clsLanguageManager::mPtr->sTexts[LAN_EXPIRE], (size_t)clsLanguageManager::mPtr->ui16TextsLens[LAN_EXPIRE]) + ": " + string(msg);
                            }
                        }
                    }

                    Bans += "|";

					curUser->SendTextDelayed(Bans);
                    return true;
                } else if(clsProfileManager::mPtr->IsAllowed(curUser, clsProfileManager::GET_RANGE_BANS) == true) {
					RangeBanItem *nxtBan = clsBanManager::mPtr->FindRange(ui128Hash, acc_time);

                    if(nxtBan != NULL) {
                        int imsgLen = CheckFromPm(curUser, fromPM);

                        int iret = sprintf(msg+imsgLen, "<%s> ", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC]);
                        imsgLen += iret;
                        if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand287") == false) {
                            return true;
                        }

						string Bans(msg, imsgLen);
                        uint32_t iBanNum = 0;

                        while(nxtBan != NULL) {
                            RangeBanItem *curBan = nxtBan;
                            nxtBan = curBan->next;

							if(((curBan->ui8Bits & clsBanManager::TEMP) == clsBanManager::TEMP) == true) {
								// PPK ... check if temban expired
								if(acc_time >= curBan->tempbanexpire) {
									clsBanManager::mPtr->RemRange(curBan);
									delete curBan;

									continue;
								}
							}

                            if(memcmp(curBan->ui128FromIpHash, ui128Hash, 16) <= 0 && memcmp(curBan->ui128ToIpHash, ui128Hash, 16) >= 0) {
                                iBanNum++;
                                imsgLen = sprintf(msg, "\n[%u] %s: %s-%s", iBanNum, clsLanguageManager::mPtr->sTexts[LAN_RANGE], curBan->sIpFrom, curBan->sIpTo);
                                if(CheckSprintf(imsgLen, 1024, "clsHubCommands::DoCommand288") == false) {
                                    return true;
                                }
                                if(((curBan->ui8Bits & clsBanManager::FULL) == clsBanManager::FULL) == true) {
                                    int iret = sprintf(msg+imsgLen, " (%s)", clsLanguageManager::mPtr->sTexts[LAN_FULL]);
                                    imsgLen += iret;
                                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand289") == false) {
                                        return true;
                                    }
                                }

								Bans += string(msg, imsgLen);

                                if(curBan->sReason != NULL) {
                                    imsgLen = sprintf(msg, " %s: %s", clsLanguageManager::mPtr->sTexts[LAN_REASON], curBan->sReason);
                                    if(CheckSprintf(imsgLen, 1024, "clsHubCommands::DoCommand290") == false) {
                                        return true;
                                    }
									Bans += msg;
                                }

                                if(curBan->sBy != NULL) {
                                    imsgLen = sprintf(msg, " %s: %s", clsLanguageManager::mPtr->sTexts[LAN_BANNED_BY], curBan->sBy);
                                    if(CheckSprintf(imsgLen, 1024, "clsHubCommands::DoCommand291") == false) {
                                        return true;
                                    }
									Bans += msg;
                                }

                                if(((curBan->ui8Bits & clsBanManager::TEMP) == clsBanManager::TEMP) == true) {
                                    struct tm *tm = localtime(&curBan->tempbanexpire);
                                    strftime(msg, 256, "%c", tm);
									Bans += " " + string(clsLanguageManager::mPtr->sTexts[LAN_EXPIRE], (size_t)clsLanguageManager::mPtr->ui16TextsLens[LAN_EXPIRE]) + ": " + string(msg);
                                }
                            }
                        }

                        Bans += "|";

						curUser->SendTextDelayed(Bans);
                        return true;
                    }
                }
                
                int imsgLen = CheckFromPm(curUser, fromPM);

                int iret = sprintf(msg+imsgLen, "<%s> *** %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_NO_BAN_FOUND]);
                imsgLen += iret;
                if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand293") == false) {
                    return true;
                }
                curUser->SendCharDelayed(msg, imsgLen);
                return true;
            }

            // !checkrangeban ipfrom ipto
			if(strncasecmp(sCommand+1, "heckrangeban ", 13) == 0) {
                if(clsProfileManager::mPtr->IsAllowed(curUser, clsProfileManager::GET_RANGE_BANS) == false) {
                    SendNoPermission(curUser, fromPM);
                    return true;
                }

				UncountDeflood(curUser, fromPM);

                if(dlen < 26) {
                    int imsgLen = CheckFromPm(curUser, fromPM);

                   	int iret = sprintf(msg+imsgLen, "<%s> *** %s %ccheckrangeban <%s> <%s>. %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                        clsLanguageManager::mPtr->sTexts[LAN_SNTX_ERR_IN_CMD], clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0], clsLanguageManager::mPtr->sTexts[LAN_FROMIP],
                        clsLanguageManager::mPtr->sTexts[LAN_TOIP], clsLanguageManager::mPtr->sTexts[LAN_NO_PARAM_GIVEN]);
                    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand295") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
            		return true;
                }

                sCommand += 14;
                char *ipto = strchr(sCommand, ' ');
                
                if(ipto == NULL) {
                    int imsgLen = CheckFromPm(curUser, fromPM);

                   	int iret = sprintf(msg+imsgLen, "<%s> *** %s %ccheckrangeban <%s> <%s>. %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                        clsLanguageManager::mPtr->sTexts[LAN_SNTX_ERR_IN_CMD], clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0], clsLanguageManager::mPtr->sTexts[LAN_FROMIP],
                        clsLanguageManager::mPtr->sTexts[LAN_TOIP], clsLanguageManager::mPtr->sTexts[LAN_BAD_PARAMS_GIVEN]);
                    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand297") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
            		return true;
                }

                ipto[0] = '\0';
                ipto++;

                // check ipfrom
                if(sCommand[0] == '\0' || ipto[0] == '\0') {
                    int imsgLen = CheckFromPm(curUser, fromPM);

                   	int iret = sprintf(msg+imsgLen, "<%s> *** %s %ccheckrangeban <%s> <%s>. %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                        clsLanguageManager::mPtr->sTexts[LAN_SNTX_ERR_IN_CMD], clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0], clsLanguageManager::mPtr->sTexts[LAN_FROMIP],
                        clsLanguageManager::mPtr->sTexts[LAN_TOIP], clsLanguageManager::mPtr->sTexts[LAN_BAD_PARAMS_GIVEN]);
                    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand299") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
            		return true;
                }

				uint8_t ui128FromHash[16], ui128ToHash[16];
				memset(ui128FromHash, 0, 16);
				memset(ui128ToHash, 0, 16);

                if(HashIP(sCommand, ui128FromHash) == false || HashIP(ipto, ui128ToHash) == false) {
                    int imsgLen = CheckFromPm(curUser, fromPM);

               	    int iret = sprintf(msg+imsgLen, "<%s> *** %s %ccheckrangeban <%s> <%s>. %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                        clsLanguageManager::mPtr->sTexts[LAN_SNTX_ERR_IN_CMD], clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0], clsLanguageManager::mPtr->sTexts[LAN_FROMIP],
                        clsLanguageManager::mPtr->sTexts[LAN_TOIP], clsLanguageManager::mPtr->sTexts[LAN_NO_VALID_IP_RANGE_SPECIFIED]);
                    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand301") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
                    return true;
                }

                time_t acc_time;
                time(&acc_time);

				RangeBanItem *RangeBan = clsBanManager::mPtr->FindRange(ui128FromHash, ui128ToHash, acc_time);
                if(RangeBan == NULL) {
                    int imsgLen = CheckFromPm(curUser, fromPM);

                    int iret = sprintf(msg+imsgLen, "<%s> *** %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_NO_RANGE_BAN_FOUND]);
                    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand303") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
                    return true;
                }
                
                int imsgLen = CheckFromPm(curUser, fromPM);

                int iret = sprintf(msg+imsgLen, "<%s> %s: %s-%s", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_RANGE], RangeBan->sIpFrom, RangeBan->sIpTo);
                imsgLen += iret;
                if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand305") == false) {
                    return true;
                }
                    
                if(((RangeBan->ui8Bits & clsBanManager::FULL) == clsBanManager::FULL) == true) {
                    int iret = sprintf(msg+imsgLen, " (%s)", clsLanguageManager::mPtr->sTexts[LAN_FULL]);
                    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand306") == false) {
                        return true;
                    }
                }

                if(RangeBan->sReason != NULL) {
                    int iret = sprintf(msg+imsgLen, " %s: %s", clsLanguageManager::mPtr->sTexts[LAN_REASON], RangeBan->sReason);
                    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand307") == false) {
                        return true;
                    }
                }

                if(RangeBan->sBy != NULL) {
                    int iret = sprintf(msg+imsgLen, " %s: %s", clsLanguageManager::mPtr->sTexts[LAN_BANNED_BY], RangeBan->sBy);
                    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand308") == false) {
                        return true;
                    }
                }

                if(((RangeBan->ui8Bits & clsBanManager::TEMP) == clsBanManager::TEMP) == true) {
                    char msg1[256];
                    struct tm *tm = localtime(&RangeBan->tempbanexpire);
                    strftime(msg1, 256, "%c", tm);
                    int iret = sprintf(msg+imsgLen, " %s: %s.|", clsLanguageManager::mPtr->sTexts[LAN_EXPIRE], msg1);
                    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand309") == false) {
                        return true;
                    }
                } else {
                    msg[imsgLen] = '.';
                    msg[imsgLen+1] = '|';
                    msg[imsgLen+2] = '\0';
                    imsgLen += 2;
                }

                curUser->SendCharDelayed(msg, imsgLen);
                return true;
            }

            return false;

        case 'a':
            // !addreguser <nick> <pass> <profile_idx>
			if(strncasecmp(sCommand+1, "ddreguser ", 10) == 0) {
                if(clsProfileManager::mPtr->IsAllowed(curUser, clsProfileManager::ADDREGUSER) == false) {
                    SendNoPermission(curUser, fromPM);
                    return true;
                }

                if(dlen > 255) {
                    int imsgLen = CheckFromPm(curUser, fromPM);

                    int iret = sprintf(msg+imsgLen, "<%s> *** %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_CMD_TOO_LONG]);
                    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand311") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
                    return true;
                }

                char *sCmdParts[] = { NULL, NULL, NULL };
                uint16_t iCmdPartsLen[] = { 0, 0, 0 };

                uint8_t cPart = 0;

                sCmdParts[cPart] = sCommand+11; // nick start

				for(size_t szi = 11; szi < dlen; szi++) {
					if(sCommand[szi] == ' ') {
						sCommand[szi] = '\0';
						iCmdPartsLen[cPart] = (uint16_t)((sCommand+szi)-sCmdParts[cPart]);

						// are we on last space ???
						if(cPart == 1) {
							sCmdParts[2] = sCommand+szi+1;
							iCmdPartsLen[2] = (uint16_t)(dlen-szi-1);
							for(size_t szj = dlen; szj > szi; szj--) {
								if(sCommand[szj] == ' ') {
									sCommand[szj] = '\0';

									sCmdParts[2] = sCommand+szj+1;
									iCmdPartsLen[2] = (uint16_t)(dlen-szj-1);

									sCommand[szi] = ' ';
									iCmdPartsLen[1] = (uint16_t)((sCommand+szj)-sCmdParts[1]);

									break;
								}
							}
							break;
                        }

                        cPart++;
						sCmdParts[cPart] = sCommand+szi+1;
                    }
                }

                if(iCmdPartsLen[0] == 0 || iCmdPartsLen[1] == 0 || iCmdPartsLen[2] == 0) {
                    int imsgLen = CheckFromPm(curUser, fromPM);

               	    int iret = sprintf(msg+imsgLen, "<%s> *** %s %caddreguser <%s> <%s> <%s>. %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                        clsLanguageManager::mPtr->sTexts[LAN_SNTX_ERR_IN_CMD], clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0], clsLanguageManager::mPtr->sTexts[LAN_NICK_LWR],
                        clsLanguageManager::mPtr->sTexts[LAN_PASSWORD_LWR], clsLanguageManager::mPtr->sTexts[LAN_PROFILENAME_LWR], clsLanguageManager::mPtr->sTexts[LAN_BAD_PARAMS_GIVEN]);
                    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand313") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
                    return true;
                }

                if(iCmdPartsLen[0] > 65) {
                    int imsgLen = CheckFromPm(curUser, fromPM);

                    int iret = sprintf(msg+imsgLen, "<%s> *** %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_MAX_ALWD_NICK_LEN_64_CHARS]);
                    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand315") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
                    return true;
                }

				if(strpbrk(sCmdParts[0], " $|") != NULL) {
                    int imsgLen = CheckFromPm(curUser, fromPM);

                    int iret = sprintf(msg+imsgLen, "<%s> *** %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_NO_BAD_CHARS_IN_NICK]);
                    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand416") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
                    return true;
                }

                if(iCmdPartsLen[1] > 65) {
                    int imsgLen = CheckFromPm(curUser, fromPM);

                    int iret = sprintf(msg+imsgLen, "<%s> *** %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_MAX_ALWD_PASS_LEN_64_CHARS]);
                    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand317") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
                    return true;
                }

				if(strchr(sCmdParts[1], '|') != NULL) {
                    int imsgLen = CheckFromPm(curUser, fromPM);

                    int iret = sprintf(msg+imsgLen, "<%s> *** %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_NO_PIPE_IN_PASS]);
                    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand439") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
                    return true;
                }

                int pidx = clsProfileManager::mPtr->GetProfileIndex(sCmdParts[2]);
                if(pidx == -1) {
                    int imsgLen = CheckFromPm(curUser, fromPM);

               	    int iret = sprintf(msg+imsgLen, "<%s> *** %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_ERR_NO_PROFILE_GIVEN_NAME_EXIST]);
               	    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand319") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
                    return true;
                }

                // check hierarchy
                // deny if curUser is not Master and tries add equal or higher profile
                if(curUser->iProfile > 0 && pidx <= curUser->iProfile) {
                    int imsgLen = CheckFromPm(curUser, fromPM);

               	    int iret = sprintf(msg+imsgLen, "<%s> *** %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_YOU_NOT_ALLOWED_TO_ADD_USER_THIS_PROFILE]);
               	    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand321") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
                    return true;
                }

                // try to add the user
                if(clsRegManager::mPtr->AddNew(sCmdParts[0], sCmdParts[1], (uint16_t)pidx) == false) {
                    int imsgLen = CheckFromPm(curUser, fromPM);

                    int iret = sprintf(msg+imsgLen, "<%s> *** %s %s %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_USER], sCmdParts[0],
                        clsLanguageManager::mPtr->sTexts[LAN_IS_ALREDY_REGISTERED]);
               	    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand323") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);      
                    }
                    return true;
                }

				UncountDeflood(curUser, fromPM);

                if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES] == true) {
                    if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES_AS_PM] == true) {
                        int imsgLen = sprintf(msg, "%s $<%s> *** %s %s %s %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],  clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                            curUser->sNick, clsLanguageManager::mPtr->sTexts[LAN_SUCCESSFULLY_ADDED], sCmdParts[0], clsLanguageManager::mPtr->sTexts[LAN_TO_REGISTERED_USERS]);
                        if(CheckSprintf(imsgLen, 1024, "clsHubCommands::DoCommand324") == true) {
							clsGlobalDataQueue::mPtr->SingleItemStore(msg, imsgLen, NULL, 0, clsGlobalDataQueue::SI_PM2OPS);
                        }
                    } else {
                        int imsgLen = sprintf(msg, "<%s> *** %s %s %s %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], curUser->sNick,
                            clsLanguageManager::mPtr->sTexts[LAN_SUCCESSFULLY_ADDED], sCmdParts[0], clsLanguageManager::mPtr->sTexts[LAN_TO_REGISTERED_USERS]);
                        if(CheckSprintf(imsgLen, 1024, "clsHubCommands::DoCommand325") == true) {
                            clsGlobalDataQueue::mPtr->AddQueueItem(msg, imsgLen, NULL, 0, clsGlobalDataQueue::CMD_OPS);
                        }
                    }
                }

                if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES] == false || ((curUser->ui32BoolBits & User::BIT_OPERATOR) == User::BIT_OPERATOR) == false) {
                    int imsgLen = CheckFromPm(curUser, fromPM);

                    int iret = sprintf(msg+imsgLen, "<%s> %s %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], sCmdParts[0],
                        clsLanguageManager::mPtr->sTexts[LAN_SUCCESSFULLY_ADDED_TO_REGISTERED_USERS]);
               	    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand327") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
                }
            
                User *AddedUser = clsHashManager::mPtr->FindUser(sCmdParts[0], iCmdPartsLen[0]);
                if(AddedUser != NULL) {
                    bool bAllowedOpChat = clsProfileManager::mPtr->IsAllowed(AddedUser, clsProfileManager::ALLOWEDOPCHAT);
                    AddedUser->iProfile = pidx;
                    if(((AddedUser->ui32BoolBits & User::BIT_OPERATOR) == User::BIT_OPERATOR) == false) {
                        if(clsProfileManager::mPtr->IsAllowed(AddedUser, clsProfileManager::HASKEYICON) == true) {
                            AddedUser->ui32BoolBits |= User::BIT_OPERATOR;
                        } else {
                            AddedUser->ui32BoolBits &= ~User::BIT_OPERATOR;
                        }

                        if(((AddedUser->ui32BoolBits & User::BIT_OPERATOR) == User::BIT_OPERATOR) == true) {
                            clsUsers::mPtr->Add2OpList(AddedUser);
                            clsGlobalDataQueue::mPtr->OpListStore(AddedUser->sNick);
                            if(bAllowedOpChat != clsProfileManager::mPtr->IsAllowed(AddedUser, clsProfileManager::ALLOWEDOPCHAT)) {
                                if(clsSettingManager::mPtr->bBools[SETBOOL_REG_OP_CHAT] == true &&
                                    (clsSettingManager::mPtr->bBools[SETBOOL_REG_BOT] == false || clsSettingManager::mPtr->bBotsSameNick == false)) {
                                    if(((AddedUser->ui32SupportBits & User::SUPPORTBIT_NOHELLO) == User::SUPPORTBIT_NOHELLO) == false) {
                                        AddedUser->SendCharDelayed(clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_OP_CHAT_HELLO],
                                            clsSettingManager::mPtr->ui16PreTextsLens[clsSettingManager::SETPRETXT_OP_CHAT_HELLO]);
                                    }
                                    AddedUser->SendCharDelayed(clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_OP_CHAT_MYINFO],
                                        clsSettingManager::mPtr->ui16PreTextsLens[clsSettingManager::SETPRETXT_OP_CHAT_MYINFO]);
                                    int imsgLen = sprintf(msg, "$OpList %s$$|", clsSettingManager::mPtr->sTexts[SETTXT_OP_CHAT_NICK]);
                                    if(CheckSprintf(imsgLen, 1024, "clsHubCommands::DoCommand328") == true) {
                                        AddedUser->SendCharDelayed(msg, imsgLen);
                                    }
                                }
                            }
                        }
                    } 
                }         
                return true;
            }

            return false;

        case 'h':
            // Hub commands: !help
			if(dlen == 4 && strncasecmp(sCommand+1, "elp", 3) == 0) {
                int imsglen = CheckFromPm(curUser, fromPM);

                int iret  = sprintf(msg+imsglen, "<%s> ", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC]);
               	imsglen += iret;
                if(CheckSprintf1(iret, imsglen, 1024, "clsHubCommands::DoCommand330") == false) {
                    return true;
                }

				string help(msg, imsglen);
                bool bFull = false;
                bool bTemp = false;

                imsglen = sprintf(msg, "%s:\n", clsLanguageManager::mPtr->sTexts[LAN_FOLOW_COMMANDS_AVAILABLE_TO_YOU]);
                if(CheckSprintf(imsglen, 1024, "clsHubCommands::DoCommand331") == false) {
                    return true;
                }
				help += msg;

                if(curUser->iProfile != -1) {
                    imsglen = sprintf(msg, "\n%s:\n", clsLanguageManager::mPtr->sTexts[LAN_PROFILE_SPECIFIC_CMDS]);
                    if(CheckSprintf(imsglen, 1024, "clsHubCommands::DoCommand332") == false) {
                        return true;
                    }
					help += msg;

                    imsglen = sprintf(msg, "\t%cpasswd <%s> - %s.\n", clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0],
                        clsLanguageManager::mPtr->sTexts[LAN_NEW_PASSWORD], clsLanguageManager::mPtr->sTexts[LAN_CHANGE_YOUR_PASSWORD]);
                    if(CheckSprintf(imsglen, 1024, "clsHubCommands::DoCommand333") == false) {
                        return true;
                    }
					help += msg;
                }

                if(clsProfileManager::mPtr->IsAllowed(curUser, clsProfileManager::BAN)) {
                    bFull = true;

                    imsglen = sprintf(msg, "\t%cban <%s> <%s> - %s.\n", clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0], clsLanguageManager::mPtr->sTexts[LAN_NICK_LWR],
                        clsLanguageManager::mPtr->sTexts[LAN_REASON_LWR], clsLanguageManager::mPtr->sTexts[LAN_PERM_BAN_USER_GIVEN_NICK_DISCONNECT]);
                    if(CheckSprintf(imsglen, 1024, "clsHubCommands::DoCommand334") == false) {
                        return true;
                    }
					help += msg;

                    imsglen = sprintf(msg, "\t%cbanip <%s> <%s> - %s.\n", clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0], clsLanguageManager::mPtr->sTexts[LAN_IP],
                        clsLanguageManager::mPtr->sTexts[LAN_REASON_LWR], clsLanguageManager::mPtr->sTexts[LAN_PERM_BAN_IP_ADDRESS]);
                    if(CheckSprintf(imsglen, 1024, "clsHubCommands::DoCommand335") == false) {
                        return true;
                    }
					help += msg;

                    imsglen = sprintf(msg, "\t%cfullban <%s> <%s> - %s.\n", clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0], clsLanguageManager::mPtr->sTexts[LAN_NICK_LWR],
                        clsLanguageManager::mPtr->sTexts[LAN_REASON_LWR], clsLanguageManager::mPtr->sTexts[LAN_PERM_BAN_USER_GIVEN_NICK_DISCONNECT]);
                    if(CheckSprintf(imsglen, 1024, "clsHubCommands::DoCommand336") == false) {
                        return true;
                    }
					help += msg;

                    imsglen = sprintf(msg, "\t%cfullbanip <%s> <%s> - %s.\n", clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0], clsLanguageManager::mPtr->sTexts[LAN_IP],
                        clsLanguageManager::mPtr->sTexts[LAN_REASON_LWR], clsLanguageManager::mPtr->sTexts[LAN_PERM_BAN_IP_ADDRESS]);
                    if(CheckSprintf(imsglen, 1024, "clsHubCommands::DoCommand337") == false) {
                        return true;
                    }
					help += msg;

                    imsglen = sprintf(msg, "\t%cnickban <%s> <%s> - %s.\n", clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0], clsLanguageManager::mPtr->sTexts[LAN_NICK_LWR],
                        clsLanguageManager::mPtr->sTexts[LAN_REASON_LWR], clsLanguageManager::mPtr->sTexts[LAN_BAN_USERS_NICK_IFCONN_THENDISCONN]);
                    if(CheckSprintf(imsglen, 1024, "clsHubCommands::DoCommand338") == false) {
                        return true;
                    }
                    help += msg;
                }

                if(clsProfileManager::mPtr->IsAllowed(curUser, clsProfileManager::TEMP_BAN)) {
                    bFull = true; bTemp = true;

					imsglen = sprintf(msg, "\t%ctempban <%s> <%s> <%s> - %s.\n", clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0], clsLanguageManager::mPtr->sTexts[LAN_NICK_LWR],
                        clsLanguageManager::mPtr->sTexts[LAN_TIME_LWR], clsLanguageManager::mPtr->sTexts[LAN_REASON_LWR], clsLanguageManager::mPtr->sTexts[LAN_TEMP_BAN_USER_GIVEN_NICK_DISCONNECT]);
                    if(CheckSprintf(imsglen, 1024, "clsHubCommands::DoCommand339") == false) {
                        return true;
                    }
					help += msg;

                    imsglen = sprintf(msg, "\t%ctempbanip <%s> <%s> <%s> - %s.\n", clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0], clsLanguageManager::mPtr->sTexts[LAN_IP],
                        clsLanguageManager::mPtr->sTexts[LAN_TIME_LWR], clsLanguageManager::mPtr->sTexts[LAN_REASON_LWR], clsLanguageManager::mPtr->sTexts[LAN_TEMP_BAN_IP_ADDRESS]);
                    if(CheckSprintf(imsglen, 1024, "clsHubCommands::DoCommand340") == false) {
                        return true;
                    }
					help += msg;

                    imsglen = sprintf(msg, "\t%cfulltempban <%s> <%s> <%s> - %s.\n", clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0], clsLanguageManager::mPtr->sTexts[LAN_NICK_LWR],
                        clsLanguageManager::mPtr->sTexts[LAN_TIME_LWR], clsLanguageManager::mPtr->sTexts[LAN_REASON_LWR], clsLanguageManager::mPtr->sTexts[LAN_TEMP_BAN_USER_GIVEN_NICK_DISCONNECT]);
                    if(CheckSprintf(imsglen, 1024, "clsHubCommands::DoCommand341") == false) {
                        return true;
                    }
					help += msg;

                    imsglen = sprintf(msg, "\t%cfulltempbanip <%s> <%s> <%s> - %s.\n", clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0], clsLanguageManager::mPtr->sTexts[LAN_IP],
                        clsLanguageManager::mPtr->sTexts[LAN_TIME_LWR], clsLanguageManager::mPtr->sTexts[LAN_REASON_LWR], clsLanguageManager::mPtr->sTexts[LAN_TEMP_BAN_IP_ADDRESS]);
                    if(CheckSprintf(imsglen, 1024, "clsHubCommands::DoCommand342") == false) {
                        return true;
                    }
					help += msg;

                    imsglen = sprintf(msg, "\t%cnicktempban <%s> <%s> <%s> - %s.\n", clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0], clsLanguageManager::mPtr->sTexts[LAN_NICK_LWR],
                        clsLanguageManager::mPtr->sTexts[LAN_TIME_LWR], clsLanguageManager::mPtr->sTexts[LAN_REASON_LWR], clsLanguageManager::mPtr->sTexts[LAN_TEMP_BAN_USERS_NICK_IFCONN_THENDISCONN]);
                    if(CheckSprintf(imsglen, 1024, "clsHubCommands::DoCommand343") == false) {
                        return true;
                    }
					help += msg;
                }

                if(clsProfileManager::mPtr->IsAllowed(curUser, clsProfileManager::UNBAN) || clsProfileManager::mPtr->IsAllowed(curUser, clsProfileManager::TEMP_UNBAN)) {
                    imsglen = sprintf(msg, "\t%cunban <%s> - %s.\n", clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0], clsLanguageManager::mPtr->sTexts[LAN_IP_OR_NICK],
                        clsLanguageManager::mPtr->sTexts[LAN_UNBAN_IP_OR_NICK]);
                    if(CheckSprintf(imsglen, 1024, "clsHubCommands::DoCommand344") == false) {
                        return true;
					}
					help += msg;
				}

                if(clsProfileManager::mPtr->IsAllowed(curUser, clsProfileManager::UNBAN)) {
                    imsglen = sprintf(msg, "\t%cpermunban <%s> - %s.\n", clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0], clsLanguageManager::mPtr->sTexts[LAN_IP_OR_NICK],
                        clsLanguageManager::mPtr->sTexts[LAN_UNBAN_PERM_BANNED_IP_OR_NICK]);
                    if(CheckSprintf(imsglen, 1024, "clsHubCommands::DoCommand345") == false) {
                        return true;
                    }
					help += msg;
                }

                if(clsProfileManager::mPtr->IsAllowed(curUser, clsProfileManager::TEMP_UNBAN)) {
                    imsglen = sprintf(msg, "\t%ctempunban <%s> - %s.\n", clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0], clsLanguageManager::mPtr->sTexts[LAN_IP_OR_NICK],
                        clsLanguageManager::mPtr->sTexts[LAN_UNBAN_TEMP_BANNED_IP_OR_NICK]);
                    if(CheckSprintf(imsglen, 1024, "clsHubCommands::DoCommand346") == false) {
                        return true;
                    }
					help += msg;
                }

                if(clsProfileManager::mPtr->IsAllowed(curUser, clsProfileManager::GETBANLIST)) {
                    imsglen = sprintf(msg, "\t%cgetbans - %s.\n", clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0], clsLanguageManager::mPtr->sTexts[LAN_DISPLAY_LIST_OF_BANS]);
                    if(CheckSprintf(imsglen, 1024, "clsHubCommands::DoCommand347") == false) {
                        return true;
                    }
					help += msg;

                    imsglen = sprintf(msg, "\t%cgetpermbans - %s.\n", clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0], clsLanguageManager::mPtr->sTexts[LAN_DISPLAY_LIST_OF_PERM_BANS]);
                    if(CheckSprintf(imsglen, 1024, "clsHubCommands::DoCommand348") == false) {
                        return true;
                    }
					help += msg;

                    imsglen = sprintf(msg, "\t%cgettempbans - %s.\n", clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0], clsLanguageManager::mPtr->sTexts[LAN_DISPLAY_LIST_OF_TEMP_BANS]);
                    if(CheckSprintf(imsglen, 1024, "clsHubCommands::DoCommand349") == false) {
                        return true;
                    }
					help += msg;
                }

                if(clsProfileManager::mPtr->IsAllowed(curUser, clsProfileManager::CLRPERMBAN)) {
                    imsglen = sprintf(msg, "\t%cclrpermbans - %s.\n", clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0], clsLanguageManager::mPtr->sTexts[LAN_CLEAR_PERM_BANS_LWR]);
                    if(CheckSprintf(imsglen, 1024, "clsHubCommands::DoCommand350") == false) {
                        return true;
                    }
					help += msg;
                }

                if(clsProfileManager::mPtr->IsAllowed(curUser, clsProfileManager::CLRTEMPBAN)) {
                    imsglen = sprintf(msg, "\t%cclrtempbans - %s.\n", clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0], clsLanguageManager::mPtr->sTexts[LAN_CLEAR_TEMP_BANS_LWR]);
                    if(CheckSprintf(imsglen, 1024, "clsHubCommands::DoCommand351") == false) {
                        return true;
                    }
					help += msg;
                }

                if(clsProfileManager::mPtr->IsAllowed(curUser, clsProfileManager::RANGE_BAN)) {
                    bFull = true;

                    imsglen = sprintf(msg, "\t%crangeban <%s> <%s> <%s> - %s.\n", clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0], clsLanguageManager::mPtr->sTexts[LAN_FROMIP],
                        clsLanguageManager::mPtr->sTexts[LAN_TOIP], clsLanguageManager::mPtr->sTexts[LAN_REASON_LWR], clsLanguageManager::mPtr->sTexts[LAN_PERM_BAN_GIVEN_IP_RANGE]);
                    if(CheckSprintf(imsglen, 1024, "clsHubCommands::DoCommand352") == false) {
                        return true;
                    }
					help += msg;

                    imsglen = sprintf(msg, "\t%cfullrangeban <%s> <%s> <%s> - %s.\n", clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0], clsLanguageManager::mPtr->sTexts[LAN_FROMIP],
                        clsLanguageManager::mPtr->sTexts[LAN_TOIP], clsLanguageManager::mPtr->sTexts[LAN_REASON_LWR], clsLanguageManager::mPtr->sTexts[LAN_PERM_BAN_GIVEN_IP_RANGE]);
                    if(CheckSprintf(imsglen, 1024, "clsHubCommands::DoCommand353") == false) {
                        return true;
                    }
					help += msg;
                }

                if(clsProfileManager::mPtr->IsAllowed(curUser, clsProfileManager::RANGE_TBAN)) {
                    bFull = true; bTemp = true;

                    imsglen = sprintf(msg, "\t%crangetempban <%s> <%s> <%s> <%s> - %s.\n", clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0],
                        clsLanguageManager::mPtr->sTexts[LAN_FROMIP], clsLanguageManager::mPtr->sTexts[LAN_TOIP], clsLanguageManager::mPtr->sTexts[LAN_TIME_LWR], clsLanguageManager::mPtr->sTexts[LAN_REASON_LWR],
                        clsLanguageManager::mPtr->sTexts[LAN_TEMP_BAN_GIVEN_IP_RANGE]);
                    if(CheckSprintf(imsglen, 1024, "clsHubCommands::DoCommand354") == false) {
                        return true;
                    }
					help += msg;

                    imsglen = sprintf(msg, "\t%cfullrangetempban <%s> <%s> <%s> <%s> - %s.\n", clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0],
                        clsLanguageManager::mPtr->sTexts[LAN_FROMIP], clsLanguageManager::mPtr->sTexts[LAN_TOIP], clsLanguageManager::mPtr->sTexts[LAN_TIME_LWR], clsLanguageManager::mPtr->sTexts[LAN_REASON_LWR],
                        clsLanguageManager::mPtr->sTexts[LAN_TEMP_BAN_GIVEN_IP_RANGE]);
                    if(CheckSprintf(imsglen, 1024, "clsHubCommands::DoCommand355") == false) {
                        return true;
                    }
					help += msg;
                }

                if(clsProfileManager::mPtr->IsAllowed(curUser, clsProfileManager::RANGE_UNBAN) || clsProfileManager::mPtr->IsAllowed(curUser, clsProfileManager::RANGE_TUNBAN)) {
                    imsglen = sprintf(msg, "\t%crangeunban <%s> <%s> - %s.\n", clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0], clsLanguageManager::mPtr->sTexts[LAN_FROMIP],
                        clsLanguageManager::mPtr->sTexts[LAN_TOIP], clsLanguageManager::mPtr->sTexts[LAN_UNBAN_BANNED_IP_RANGE]);
                    if(CheckSprintf(imsglen, 1024, "clsHubCommands::DoCommand356") == false) {
                        return true;
                    }
					help += msg;
                }

                if(clsProfileManager::mPtr->IsAllowed(curUser, clsProfileManager::RANGE_UNBAN)) {
                    imsglen = sprintf(msg, "\t%crangepermunban <%s> <%s> - %s.\n", clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0], clsLanguageManager::mPtr->sTexts[LAN_FROMIP],
                        clsLanguageManager::mPtr->sTexts[LAN_TOIP], clsLanguageManager::mPtr->sTexts[LAN_UNBAN_PERM_BANNED_IP_RANGE]);
                    if(CheckSprintf(imsglen, 1024, "clsHubCommands::DoCommand357") == false) {
                        return true;
                    }
					help += msg;
                }

                if(clsProfileManager::mPtr->IsAllowed(curUser, clsProfileManager::RANGE_TUNBAN)) {
                    imsglen = sprintf(msg, "\t%crangetempunban <%s> <%s> - %s.\n", clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0], clsLanguageManager::mPtr->sTexts[LAN_FROMIP],
                        clsLanguageManager::mPtr->sTexts[LAN_TOIP], clsLanguageManager::mPtr->sTexts[LAN_UNBAN_TEMP_BANNED_IP_RANGE]);
                    if(CheckSprintf(imsglen, 1024, "clsHubCommands::DoCommand358") == false) {
                        return true;
                    }
					help += msg;
                }

                if(clsProfileManager::mPtr->IsAllowed(curUser, clsProfileManager::GET_RANGE_BANS)) {
                    bFull = true;

                    imsglen = sprintf(msg, "\t%cgetrangebans - %s.\n", clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0],
                        clsLanguageManager::mPtr->sTexts[LAN_DISPLAY_LIST_OF_RANGE_BANS]);
                    if(CheckSprintf(imsglen, 1024, "clsHubCommands::DoCommand359") == false) {
                        return true;
                    }
					help += msg;

                    imsglen = sprintf(msg, "\t%cgetrangepermbans - %s.\n", clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0],
                        clsLanguageManager::mPtr->sTexts[LAN_DISPLAY_LIST_OF_RANGE_PERM_BANS]);
                    if(CheckSprintf(imsglen, 1024, "clsHubCommands::DoCommand360") == false) {
                        return true;
                    }
					help += msg;

                    imsglen = sprintf(msg, "\t%cgetrangetempbans - %s.\n", clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0],
                        clsLanguageManager::mPtr->sTexts[LAN_DISPLAY_LIST_OF_RANGE_TEMP_BANS]);
                    if(CheckSprintf(imsglen, 1024, "clsHubCommands::DoCommand361") == false) {
                        return true;
                    }
                    help += msg;
                }

                if(clsProfileManager::mPtr->IsAllowed(curUser, clsProfileManager::CLR_RANGE_BANS)) {
                    imsglen = sprintf(msg, "\t%cclrrangepermbans - %s.\n", clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0],
                        clsLanguageManager::mPtr->sTexts[LAN_CLEAR_PERM_RANGE_BANS_LWR]);
                    if(CheckSprintf(imsglen, 1024, "clsHubCommands::DoCommand362") == false) {
                        return true;
                    }
					help += msg;
                }

                if(clsProfileManager::mPtr->IsAllowed(curUser, clsProfileManager::CLR_RANGE_TBANS)) {
                    imsglen = sprintf(msg, "\t%cclrrangetempbans - %s.\n", clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0],
                        clsLanguageManager::mPtr->sTexts[LAN_CLEAR_TEMP_RANGE_BANS_LWR]);
                    if(CheckSprintf(imsglen, 1024, "clsHubCommands::DoCommand363") == false) {
                        return true;
                    }
					help += msg;
                }

                if(clsProfileManager::mPtr->IsAllowed(curUser, clsProfileManager::GETBANLIST)) {
                    imsglen = sprintf(msg, "\t%cchecknickban <%s> - %s.\n", clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0], clsLanguageManager::mPtr->sTexts[LAN_NICK_LWR],
                        clsLanguageManager::mPtr->sTexts[LAN_DISPLAY_BAN_FOUND_FOR_GIVEN_NICK]);
                    if(CheckSprintf(imsglen, 1024, "clsHubCommands::DoCommand364") == false) {
                        return true;
                    }
					help += msg;

                    imsglen = sprintf(msg, "\t%ccheckipban <%s> - %s.\n", clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0], clsLanguageManager::mPtr->sTexts[LAN_IP],
                        clsLanguageManager::mPtr->sTexts[LAN_DISPLAY_BANS_FOUND_FOR_GIVEN_IP]);
                    if(CheckSprintf(imsglen, 1024, "clsHubCommands::DoCommand365") == false) {
                        return true;
                    }
					help += msg;
                }

                if(clsProfileManager::mPtr->IsAllowed(curUser, clsProfileManager::GET_RANGE_BANS)) {
                    imsglen = sprintf(msg, "\t%ccheckrangeban <%s> <%s> - %s.\n", clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0], clsLanguageManager::mPtr->sTexts[LAN_FROMIP],
                        clsLanguageManager::mPtr->sTexts[LAN_TOIP], clsLanguageManager::mPtr->sTexts[LAN_DISPLAY_RANGE_BAN_FOR_GIVEN_RANGE]);
                    if(CheckSprintf(imsglen, 1024, "clsHubCommands::DoCommand366") == false) {
                        return true;
                    }
					help += msg;
                }

                if(clsProfileManager::mPtr->IsAllowed(curUser, clsProfileManager::DROP)) {
                    imsglen = sprintf(msg, "\t%cdrop <%s> <%s> - %s.\n", clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0], clsLanguageManager::mPtr->sTexts[LAN_NICK_LWR],
                        clsLanguageManager::mPtr->sTexts[LAN_REASON_LWR], clsLanguageManager::mPtr->sTexts[LAN_DISCONNECT_WITH_TEMPBAN]);
                    if(CheckSprintf(imsglen, 1024, "clsHubCommands::DoCommand367") == false) {
                        return true;
                    }
					help += msg;
                }

                if(clsProfileManager::mPtr->IsAllowed(curUser, clsProfileManager::GETINFO)) {
                    imsglen = sprintf(msg, "\t%cgetinfo <%s> - %s.\n", clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0], clsLanguageManager::mPtr->sTexts[LAN_NICK_LWR],
                        clsLanguageManager::mPtr->sTexts[LAN_DISPLAY_INFO_GIVEN_NICK]);
                    if(CheckSprintf(imsglen, 1024, "clsHubCommands::DoCommand368") == false) {
                        return true;
                    }
					help += msg;
                }

                if(clsProfileManager::mPtr->IsAllowed(curUser, clsProfileManager::TEMPOP)) {
                    imsglen = sprintf(msg, "\t%cop <%s> - %s.\n", clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0], clsLanguageManager::mPtr->sTexts[LAN_NICK_LWR],
                        clsLanguageManager::mPtr->sTexts[LAN_GIVE_TEMP_OP]);
                    if(CheckSprintf(imsglen, 1024, "clsHubCommands::DoCommand369") == false) {
                        return true;
                    }
					help += msg;
                }

                if(clsProfileManager::mPtr->IsAllowed(curUser, clsProfileManager::GAG)) {
                    imsglen = sprintf(msg, "\t%cgag <%s> - %s.\n", clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0], clsLanguageManager::mPtr->sTexts[LAN_NICK_LWR],
                        clsLanguageManager::mPtr->sTexts[LAN_DISALLOW_USER_TO_POST_IN_MAIN]);
                    if(CheckSprintf(imsglen, 1024, "clsHubCommands::DoCommand370") == false) {
                        return true;
                    }
					help += msg;

                    imsglen = sprintf(msg, "\t%cungag <%s> - %s.\n", clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0], clsLanguageManager::mPtr->sTexts[LAN_NICK_LWR],
                        clsLanguageManager::mPtr->sTexts[LAN_USER_CAN_POST_IN_MAIN_AGAIN]);
                    if(CheckSprintf(imsglen, 1024, "clsHubCommands::DoCommand371") == false) {
                        return true;
                    }
					help += msg;
                }

                if(clsProfileManager::mPtr->IsAllowed(curUser, clsProfileManager::RSTHUB)) {
                    imsglen = sprintf(msg, "\t%crestart - %s.\n", clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0], clsLanguageManager::mPtr->sTexts[LAN_RESTART_HUB_LWR]);
                    if(CheckSprintf(imsglen, 1024, "clsHubCommands::DoCommand372") == false) {
                        return true;
                    }
					help += msg;
                }

                if(clsProfileManager::mPtr->IsAllowed(curUser, clsProfileManager::RSTSCRIPTS)) {
                    imsglen = sprintf(msg, "\t%cstartscript <%s> - %s.\n", clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0], clsLanguageManager::mPtr->sTexts[LAN_FILENAME_LWR],
                        clsLanguageManager::mPtr->sTexts[LAN_START_SCRIPT_GIVEN_FILENAME]);
                    if(CheckSprintf(imsglen, 1024, "clsHubCommands::DoCommand373") == false) {
                        return true;
                    }
					help += msg;

                    imsglen = sprintf(msg, "\t%cstopscript <%s> - %s.\n", clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0], clsLanguageManager::mPtr->sTexts[LAN_FILENAME_LWR],
                        clsLanguageManager::mPtr->sTexts[LAN_STOP_SCRIPT_GIVEN_FILENAME]);
                    if(CheckSprintf(imsglen, 1024, "clsHubCommands::DoCommand374") == false) {
                        return true;
                    }
					help += msg;

                    imsglen = sprintf(msg, "\t%crestartscript <%s> - %s.\n", clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0], clsLanguageManager::mPtr->sTexts[LAN_FILENAME_LWR],
                        clsLanguageManager::mPtr->sTexts[LAN_RESTART_SCRIPT_GIVEN_FILENAME]);
                    if(CheckSprintf(imsglen, 1024, "clsHubCommands::DoCommand375") == false) {
                        return true;
                    }
					help += msg;

                    imsglen = sprintf(msg, "\t%crestartscripts - %s.\n", clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0],
                        clsLanguageManager::mPtr->sTexts[LAN_RESTART_SCRIPTING_PART_OF_THE_HUB]);
                    if(CheckSprintf(imsglen, 1024, "clsHubCommands::DoCommand376") == false) {
                        return true;
                    }
					help += msg;

                    imsglen = sprintf(msg, "\t%cgetscripts - %s.\n", clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0], clsLanguageManager::mPtr->sTexts[LAN_DISPLAY_LIST_OF_SCRIPTS]);
                    if(CheckSprintf(imsglen, 1024, "clsHubCommands::DoCommand377") == false) {
                        return true;
                    }
					help += msg;
                }

                if(clsProfileManager::mPtr->IsAllowed(curUser, clsProfileManager::REFRESHTXT)) {
                    imsglen = sprintf(msg, "\t%creloadtxt - %s.\n", clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0], clsLanguageManager::mPtr->sTexts[LAN_RELOAD_ALL_TEXT_FILES]);
                    if(CheckSprintf(imsglen, 1024, "clsHubCommands::DoCommand378") == false) {
                        return true;
                    }
					help += msg;
                }

                if(clsProfileManager::mPtr->IsAllowed(curUser, clsProfileManager::ADDREGUSER)) {
                    imsglen = sprintf(msg, "\t%creguser <%s> <%s> - %s.\n", clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0], clsLanguageManager::mPtr->sTexts[LAN_NICK_LWR],
                        clsLanguageManager::mPtr->sTexts[LAN_PROFILENAME_LWR], clsLanguageManager::mPtr->sTexts[LAN_REG_USER_WITH_PROFILE]);
                    if(CheckSprintf(imsglen, 1024, "clsHubCommands::DoCommand379-1") == false) {
                        return true;
                    }
					help += msg;

                    imsglen = sprintf(msg, "\t%caddreguser <%s> <%s> <%s> - %s.\n", clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0], clsLanguageManager::mPtr->sTexts[LAN_NICK_LWR],
                        clsLanguageManager::mPtr->sTexts[LAN_PASSWORD_LWR], clsLanguageManager::mPtr->sTexts[LAN_PROFILENAME_LWR], clsLanguageManager::mPtr->sTexts[LAN_ADD_REG_USER_WITH_PROFILE]);
                    if(CheckSprintf(imsglen, 1024, "clsHubCommands::DoCommand379-2") == false) {
                        return true;
                    }
					help += msg;
                }

                if(clsProfileManager::mPtr->IsAllowed(curUser, clsProfileManager::DELREGUSER)) {
                    imsglen = sprintf(msg, "\t%cdelreguser <%s> - %s.\n", clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0], clsLanguageManager::mPtr->sTexts[LAN_NICK_LWR],
                        clsLanguageManager::mPtr->sTexts[LAN_REMOVE_REG_USER]);
                    if(CheckSprintf(imsglen, 1024, "clsHubCommands::DoCommand380") == false) {
                        return true;
                    }
					help += msg;
                }

                if(clsProfileManager::mPtr->IsAllowed(curUser, clsProfileManager::TOPIC) == true) {
                    imsglen = sprintf(msg, "\t%ctopic <%s> - %s %ctopic <off> - %s.\n", clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0],
                        clsLanguageManager::mPtr->sTexts[LAN_NEW_TOPIC], clsLanguageManager::mPtr->sTexts[LAN_SET_NEW_TOPIC_OR], clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0],
                        clsLanguageManager::mPtr->sTexts[LAN_CLEAR_TOPIC]);
                    if(CheckSprintf(imsglen, 1024, "clsHubCommands::DoCommand381") == false) {
                        return true;
                    }
					help += msg;
                }

                if(clsProfileManager::mPtr->IsAllowed(curUser, clsProfileManager::MASSMSG) == true) {
                    imsglen = sprintf(msg, "\t%cmassmsg <%s> - %s.\n", clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0], clsLanguageManager::mPtr->sTexts[LAN_MESSAGE_LWR],
                        clsLanguageManager::mPtr->sTexts[LAN_SEND_MSG_TO_ALL_USERS]);
                    if(CheckSprintf(imsglen, 1024, "clsHubCommands::DoCommand382") == false) {
                        return true;
                    }
					help += msg;

                    imsglen = sprintf(msg, "\t%copmassmsg <%s> - %s.\n", clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0], clsLanguageManager::mPtr->sTexts[LAN_MESSAGE_LWR],
                        clsLanguageManager::mPtr->sTexts[LAN_SEND_MSG_TO_ALL_OPS]);
                    if(CheckSprintf(imsglen, 1024, "clsHubCommands::DoCommand383") == false) {
                        return true;
                    }
					help += msg;
                }

                if(((curUser->ui32BoolBits & User::BIT_OPERATOR) == User::BIT_OPERATOR) == true) {
//                    help += "\t"+String(clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0])+"stat - Show some hub statistics.\n\n";
//                    help += "\t"+String(clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0])+"ipinfo <IP> - Show all on/offline users with that IP\n";
//                    help += "\t"+String(clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0])+"iprangeinfo <IP> - Show all on/offline users within that IP range\n";
//                    help += "\t"+String(clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0])+"userinfo <username> - Show all visits of that user\n";
                }

                if(bFull == true) {
                    imsglen = sprintf(msg, "*** %s.\n", clsLanguageManager::mPtr->sTexts[LAN_REASON_IS_ALWAYS_OPTIONAL]);
                    if(CheckSprintf(imsglen, 1024, "clsHubCommands::DoCommand384") == false) {
                        return true;
                    }
					help += msg;

                    imsglen = sprintf(msg, "*** %s.\n", clsLanguageManager::mPtr->sTexts[LAN_FULLBAN_HELP_TXT]);
                    if(CheckSprintf(imsglen, 1024, "clsHubCommands::DoCommand385") == false) {
                        return true;
                    }
					help += msg;
                }

                if(bTemp == true) {
                    imsglen = sprintf(msg, "*** %s: m = %s, h = %s, d = %s, w = %s, M = %s, Y = %s.\n", clsLanguageManager::mPtr->sTexts[LAN_TEMPBAN_TIME_VALUES],
                        clsLanguageManager::mPtr->sTexts[LAN_MINUTES_LWR], clsLanguageManager::mPtr->sTexts[LAN_HOURS_LWR], clsLanguageManager::mPtr->sTexts[LAN_DAYS_LWR], clsLanguageManager::mPtr->sTexts[LAN_WEEKS_LWR],
                        clsLanguageManager::mPtr->sTexts[LAN_MONTHS_LWR], clsLanguageManager::mPtr->sTexts[LAN_YEARS_LWR]);
                    if(CheckSprintf(imsglen, 1024, "clsHubCommands::DoCommand386") == false) {
                        return true;
                    }
                    help += msg;
                }

                imsglen = sprintf(msg, "\n%s:\n", clsLanguageManager::mPtr->sTexts[LAN_GLOBAL_COMMANDS]);
                if(CheckSprintf(imsglen, 1024, "clsHubCommands::DoCommand387") == false) {
                    return true;
                }
				help += msg;

                imsglen = sprintf(msg, "\t%cme <%s> - %s.\n", clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0], clsLanguageManager::mPtr->sTexts[LAN_MESSAGE_LWR],
                    clsLanguageManager::mPtr->sTexts[LAN_SPEAK_IN_3RD_PERSON]);
                if(CheckSprintf(imsglen, 1024, "clsHubCommands::DoCommand388") == false) {
                    return true;
                }
				help += msg;

                imsglen = sprintf(msg, "\t%cmyip - %s.|", clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0], clsLanguageManager::mPtr->sTexts[LAN_SHOW_YOUR_IP]);
                if(CheckSprintf(imsglen, 1024, "clsHubCommands::DoCommand389") == false) {
                    return true;
                }
				help += msg;

//                help += "\t"+String(clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0])+"help - this help page.\n|";

				curUser->SendTextDelayed(help);
                return true;
            }

            return false;

		case 's':
			//Hub-Commands: !stat !stats
			if((dlen == 4 && strncasecmp(sCommand+1, "tat", 3) == 0) || (dlen == 5 && strncasecmp(sCommand+1, "tats", 4) == 0)) {
				int imsglen = CheckFromPm(curUser, fromPM);

				int iret = sprintf(msg+imsglen, "<%s>", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC]);
				imsglen += iret;
				if(CheckSprintf1(iret, imsglen, 1024, "clsHubCommands::DoCommand391") == false) {
					return true;
				}

				string Statinfo(msg, imsglen);

				Statinfo+="\n------------------------------------------------------------\n";
				Statinfo+="Current stats:\n";
				Statinfo+="------------------------------------------------------------\n";
				Statinfo+="Uptime: "+string(clsServerManager::ui64Days)+" days, "+string(clsServerManager::ui64Hours) + " hours, " + string(clsServerManager::ui64Mins) + " minutes\n";

                Statinfo+="Version: DCBEELINEKZ " PtokaXVersionString
#ifdef _WIN32
    #ifdef _M_X64
                " x64"
    #endif
#endif
#ifdef _PtokaX_TESTING_
                " [build " BUILD_NUMBER "]"
#endif
                " built on " __DATE__ " " __TIME__ "\n";
#ifdef _WIN32
				Statinfo+="OS: "+clsServerManager::sOS+"\r\n";
#else

				struct utsname osname;
				if(uname(&osname) >= 0) {
					Statinfo+="OS: "+string(osname.sysname)+" "+string(osname.release)+" ("+string(osname.machine)+")\n";
				}
#endif
				Statinfo+="Users (Max/Actual Peak (Max Peak)/Logged): "+string(clsSettingManager::mPtr->iShorts[SETSHORT_MAX_USERS])+" / "+string(clsServerManager::ui32Peak)+" ("+
					string(clsSettingManager::mPtr->iShorts[SETSHORT_MAX_USERS_PEAK])+") / "+string(clsServerManager::ui32Logged)+ "\n";
                Statinfo+="Joins / Parts: "+string(clsServerManager::ui32Joins)+" / "+string(clsServerManager::ui32Parts)+"\n";
				Statinfo+="Users shared size: "+string(clsServerManager::ui64TotalShare)+" Bytes / "+string(formatBytes(clsServerManager::ui64TotalShare))+"\n";
				Statinfo+="Chat messages: "+string(clsDcCommands::mPtr->iStatChat)+" x\n";
				Statinfo+="Unknown commands: "+string(clsDcCommands::mPtr->iStatCmdUnknown)+" x\n";
				Statinfo+="PM commands: "+string(clsDcCommands::mPtr->iStatCmdTo)+" x\n";
				Statinfo+="Key commands: "+string(clsDcCommands::mPtr->iStatCmdKey)+" x\n";
				Statinfo+="Supports commands: "+string(clsDcCommands::mPtr->iStatCmdSupports)+" x\n";
				Statinfo+="MyINFO commands: "+string(clsDcCommands::mPtr->iStatCmdMyInfo)+" x\n";
				Statinfo+="ValidateNick commands: "+string(clsDcCommands::mPtr->iStatCmdValidate)+" x\n";
				Statinfo+="GetINFO commands: "+string(clsDcCommands::mPtr->iStatCmdGetInfo)+" x\n";
				Statinfo+="Password commands: "+string(clsDcCommands::mPtr->iStatCmdMyPass)+" x\n";
				Statinfo+="Version commands: "+string(clsDcCommands::mPtr->iStatCmdVersion)+" x\n";
				Statinfo+="GetNickList commands: "+string(clsDcCommands::mPtr->iStatCmdGetNickList)+" x\n";
				Statinfo+="Search commands: "+string(clsDcCommands::mPtr->iStatCmdSearch)+" x ("+string(clsDcCommands::mPtr->iStatCmdMultiSearch)+" x)\n";
				Statinfo+="SR commands: "+string(clsDcCommands::mPtr->iStatCmdSR)+" x\n";
				Statinfo+="CTM commands: "+string(clsDcCommands::mPtr->iStatCmdConnectToMe)+" x ("+string(clsDcCommands::mPtr->iStatCmdMultiConnectToMe)+" x)\n";
				Statinfo+="RevCTM commands: "+string(clsDcCommands::mPtr->iStatCmdRevCTM)+" x\n";
				Statinfo+="BotINFO commands: "+string(clsDcCommands::mPtr->iStatBotINFO)+" x\n";
				Statinfo+="Close commands: "+string(clsDcCommands::mPtr->iStatCmdClose)+" x\n";
				//Statinfo+="------------------------------------------------------------\n";
				//Statinfo+="ClientSocket Errors: "+string(iStatUserSocketErrors)+" x\n";
				Statinfo+="------------------------------------------------------------\n";
#ifdef _WIN32
				FILETIME tmpa, tmpb, kernelTimeFT, userTimeFT;
				GetProcessTimes(GetCurrentProcess(), &tmpa, &tmpb, &kernelTimeFT, &userTimeFT);
				int64_t kernelTime = kernelTimeFT.dwLowDateTime | (((int64_t)kernelTimeFT.dwHighDateTime) << 32);
				int64_t userTime = userTimeFT.dwLowDateTime | (((int64_t)userTimeFT.dwHighDateTime) << 32);
				int64_t icpuSec = (kernelTime + userTime) / (10000000I64);

				char cpuusage[32];
				int iLen = sprintf(cpuusage, "%.2f%%\r\n", clsServerManager::dCpuUsage/0.6);
				if(CheckSprintf(iLen, 32, "clsHubCommands::DoCommand3921") == false) {
					return true;
				}
				Statinfo+="CPU usage (60 sec avg): "+string(cpuusage, iLen);

				char cputime[64];
				iLen = sprintf(cputime, "%01I64d:%02d:%02d", icpuSec / (60*60), (int32_t)((icpuSec / 60) % 60), (int32_t)(icpuSec % 60));
				if(CheckSprintf(iLen, 64, "clsHubCommands::DoCommand392") == false) {
					return true;
				}
				Statinfo+="CPU time: "+string(cputime, iLen)+"\r\n";

				PROCESS_MEMORY_COUNTERS pmc;
				memset(&pmc, 0, sizeof(PROCESS_MEMORY_COUNTERS));
				pmc.cb = sizeof(pmc);

				typedef BOOL (WINAPI *PGPMI)(HANDLE, PPROCESS_MEMORY_COUNTERS, DWORD);
				PGPMI pGPMI = (PGPMI)GetProcAddress(LoadLibrary("psapi.dll"), "GetProcessMemoryInfo");

                if(pGPMI != NULL) {
					pGPMI(GetCurrentProcess(), &pmc, sizeof(pmc));

					Statinfo+="Mem usage (Peak): "+string(formatBytes(pmc.WorkingSetSize))+ " ("+string(formatBytes(pmc.PeakWorkingSetSize))+")\r\n";
					Statinfo+="VM size (Peak): "+string(formatBytes(pmc.PagefileUsage))+ " ("+string(formatBytes(pmc.PeakPagefileUsage))+")\r\n";
                }
#else // _WIN32
				char cpuusage[32];
				int iLen = sprintf(cpuusage, "%.2f%%\n", (clsServerManager::dCpuUsage/0.6)/(double)clsServerManager::ui32CpuCount);
				if(CheckSprintf(iLen, 32, "clsHubCommands::DoCommand3921") == false) {
					return true;
				}
				Statinfo+="CPU usage (60 sec avg): "+string(cpuusage, iLen);

				struct rusage rs;

				getrusage(RUSAGE_SELF, &rs);

				uint64_t ui64cpuSec = uint64_t(rs.ru_utime.tv_sec) + (uint64_t(rs.ru_utime.tv_usec)/1000000) +
					uint64_t(rs.ru_stime.tv_sec) + (uint64_t(rs.ru_stime.tv_usec)/1000000);

				char cputime[64];
				iLen = sprintf(cputime, "%01" PRIu64 ":%02" PRIu64 ":%02" PRIu64, ui64cpuSec / (60*60),
					(ui64cpuSec / 60) % 60, ui64cpuSec % 60);
				if(CheckSprintf(iLen, 64, "clsHubCommands::DoCommand392") == false) {
					return true;
				}
				Statinfo+="CPU time: "+string(cputime, iLen)+"\n";

				FILE *fp = fopen("/proc/self/status", "r");
				if(fp != NULL) {
					string memrss, memhwm, memvms, memvmp, memstk, memlib;
					char buf[1024];
					while(fgets(buf, 1024, fp) != NULL) {
						if(strncmp(buf, "VmRSS:", 6) == 0) {
							char * tmp = buf+6;
							while(isspace(*tmp) && *tmp) {
								tmp++;
							}
							memrss = string(tmp, strlen(tmp)-1);
						} else if(strncmp(buf, "VmHWM:", 6) == 0) {
							char * tmp = buf+6;
							while(isspace(*tmp) && *tmp) {
								tmp++;
							}
							memhwm = string(tmp, strlen(tmp)-1);
						} else if(strncmp(buf, "VmSize:", 7) == 0) {
							char * tmp = buf+7;
							while(isspace(*tmp) && *tmp) {
								tmp++;
							}
							memvms = string(tmp, strlen(tmp)-1);
						} else if(strncmp(buf, "VmPeak:", 7) == 0) {
							char * tmp = buf+7;
							while(isspace(*tmp) && *tmp) {
								tmp++;
							}
							memvmp = string(tmp, strlen(tmp)-1);
						} else if(strncmp(buf, "VmStk:", 6) == 0) {
							char * tmp = buf+6;
							while(isspace(*tmp) && *tmp) {
								tmp++;
							}
							memstk = string(tmp, strlen(tmp)-1);
						} else if(strncmp(buf, "VmLib:", 6) == 0) {
							char * tmp = buf+6;
							while(isspace(*tmp) && *tmp) {
								tmp++;
							}
							memlib = string(tmp, strlen(tmp)-1);
						}
					}

					fclose(fp);

					if(memhwm.size() != 0 && memrss.size() != 0) {
						Statinfo+="Mem usage (Peak): "+memrss+ " ("+memhwm+")\n";
					} else if(memrss.size() != 0) {
						Statinfo+="Mem usage: "+memrss+"\n";
					}

					if(memvmp.size() != 0 && memvms.size() != 0) {
						Statinfo+="VM size (Peak): "+memvms+ " ("+memvmp+")\n";
					} else if(memrss.size() != 0) {
						Statinfo+="VM size: "+memvms+"\n";
					}

					if(memstk.size() != 0 && memlib.size() != 0) {
						Statinfo+="Stack size / Libs size: "+memstk+ " / "+memlib+"\n";
					}
				}
#endif
				Statinfo+="------------------------------------------------------------\n";
				Statinfo+="SendRests (Peak): "+string(clsServiceLoop::mPtr->iLastSendRest)+" ("+string(clsServiceLoop::mPtr->iSendRestsPeak)+")\n";
				Statinfo+="RecvRests (Peak): "+string(clsServiceLoop::mPtr->iLastRecvRest)+" ("+string(clsServiceLoop::mPtr->iRecvRestsPeak)+")\n";
				Statinfo+="Compression saved: "+string(formatBytes(clsServerManager::ui64BytesSentSaved))+" ("+string(clsDcCommands::mPtr->iStatZPipe)+")\n";
				Statinfo+="Data sent: "+string(formatBytes(clsServerManager::ui64BytesSent))+ "\n";
				Statinfo+="Data received: "+string(formatBytes(clsServerManager::ui64BytesRead))+ "\n";
				Statinfo+="Tx (60 sec avg): "+string(formatBytesPerSecond(clsServerManager::ui32ActualBytesSent))+" ("+string(formatBytesPerSecond(clsServerManager::ui32AverageBytesSent/60))+")\n";
				Statinfo+="Rx (60 sec avg): "+string(formatBytesPerSecond(clsServerManager::ui32ActualBytesRead))+" ("+string(formatBytesPerSecond(clsServerManager::ui32AverageBytesRead/60))+")|";
				curUser->SendTextDelayed(Statinfo);
				return true;
			}

			// Hub commands: !stopscript scriptname
			if(strncasecmp(sCommand+1, "topscript ", 10) == 0) {
				if(clsProfileManager::mPtr->IsAllowed(curUser, clsProfileManager::RSTSCRIPTS) == false) {
					SendNoPermission(curUser, fromPM);
					return true;
				}

				if(clsSettingManager::mPtr->bBools[SETBOOL_ENABLE_SCRIPTING] == false) {
					int imsgLen = CheckFromPm(curUser, fromPM);

					int iret = sprintf(msg+imsgLen, "<%s> *** %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_ERR_SCRIPTS_DISABLED]);
					imsgLen += iret;
					if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand400") == true) {
						curUser->SendCharDelayed(msg, imsgLen);
					}
					return true;
				}

				if(dlen < 12) {
					int imsgLen = CheckFromPm(curUser, fromPM);

					int iret = sprintf(msg+imsgLen, "<%s> *** %s %cstopscript <%s>. %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_SNTX_ERR_IN_CMD],
                        clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0], clsLanguageManager::mPtr->sTexts[LAN_SCRIPTNAME_LWR], clsLanguageManager::mPtr->sTexts[LAN_NO_PARAM_GIVEN]);
					imsgLen += iret;
					if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand402") == true) {
						curUser->SendCharDelayed(msg, imsgLen);
					}
					return true;
				}

				sCommand += 11;

                if(dlen-11 > 256) {
                    int imsgLen = CheckFromPm(curUser, fromPM);

                    int iret = sprintf(msg+imsgLen, "<%s> *** %s %cstopscript <%s>. %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_SNTX_ERR_IN_CMD],
                        clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0], clsLanguageManager::mPtr->sTexts[LAN_SCRIPTNAME_LWR], clsLanguageManager::mPtr->sTexts[LAN_MAX_ALWD_SCRIPT_NAME_LEN_256_CHARS]);
                    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand403") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
                    return true;
                }

				Script * curScript = clsScriptManager::mPtr->FindScript(sCommand);
				if(curScript == NULL || curScript->bEnabled == false || curScript->LUA == NULL) {
					int imsgLen = CheckFromPm(curUser, fromPM);

					int iret = sprintf(msg+imsgLen, "<%s> *** %s %s %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_ERROR_SCRIPT], sCommand,
                        clsLanguageManager::mPtr->sTexts[LAN_NOT_RUNNING]);
					imsgLen += iret;
					if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand404") == true) {
						curUser->SendCharDelayed(msg, imsgLen);
					}
					return true;
				}

				UncountDeflood(curUser, fromPM);

				clsScriptManager::mPtr->StopScript(curScript, true);

				if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES] == true) {
					if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES_AS_PM] == true) {
						int imsgLen = sprintf(msg, "%s $<%s> *** %s %s: %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                            curUser->sNick, clsLanguageManager::mPtr->sTexts[LAN_STOPPED_SCRIPT], sCommand);
						if(CheckSprintf(imsgLen, 1024, "clsHubCommands::DoCommand405") == true) {
							clsGlobalDataQueue::mPtr->SingleItemStore(msg, imsgLen, NULL, 0, clsGlobalDataQueue::SI_PM2OPS);
						}
					} else {
						int imsgLen = sprintf(msg, "<%s> *** %s %s: %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], curUser->sNick, clsLanguageManager::mPtr->sTexts[LAN_STOPPED_SCRIPT],
                            sCommand);
						if(CheckSprintf(imsgLen, 1024, "clsHubCommands::DoCommand406") == true) {
							clsGlobalDataQueue::mPtr->AddQueueItem(msg, imsgLen, NULL, 0, clsGlobalDataQueue::CMD_OPS);
						}
					}
				}

				if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES] == false ||
					((curUser->ui32BoolBits & User::BIT_OPERATOR) == User::BIT_OPERATOR) == false) {
					int imsgLen = CheckFromPm(curUser, fromPM);

					int iret = sprintf(msg+imsgLen, "<%s> %s %s %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_SCRIPT], sCommand,
                        clsLanguageManager::mPtr->sTexts[LAN_STOPPED_LWR]);
					imsgLen += iret;
					if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand408") == true) {
						curUser->SendCharDelayed(msg, imsgLen);
					}
				}

				return true;
			}

			// Hub commands: !startscript scriptname
			if(strncasecmp(sCommand+1, "tartscript ", 11) == 0) {
				if(clsProfileManager::mPtr->IsAllowed(curUser, clsProfileManager::RSTSCRIPTS) == false) {
					SendNoPermission(curUser, fromPM);
					return true;
				}

				if(clsSettingManager::mPtr->bBools[SETBOOL_ENABLE_SCRIPTING] == false) {
					int imsgLen = CheckFromPm(curUser, fromPM);

					int iret = sprintf(msg+imsgLen, "<%s> *** %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_ERR_SCRIPTS_DISABLED]);
					imsgLen += iret;
					if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand412") == true) {
						curUser->SendCharDelayed(msg, imsgLen);
					}
					return true;
				}

				if(dlen < 13) {
					int imsgLen = CheckFromPm(curUser, fromPM);

					int iret = sprintf(msg+imsgLen, "<%s> *** %s %cstartscript <%s>. %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_SNTX_ERR_IN_CMD],
                        clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0], clsLanguageManager::mPtr->sTexts[LAN_SCRIPTNAME_LWR], clsLanguageManager::mPtr->sTexts[LAN_NO_PARAM_GIVEN]);
					imsgLen += iret;
					if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand414") == true) {
						curUser->SendCharDelayed(msg, imsgLen);
					}
					return true;
				}

				sCommand += 12;

                if(dlen-12 > 256) {
                    int imsgLen = CheckFromPm(curUser, fromPM);

                    int iret = sprintf(msg+imsgLen, "<%s> *** %s %cstartscript <%s>. %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_SNTX_ERR_IN_CMD],
                        clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0], clsLanguageManager::mPtr->sTexts[LAN_SCRIPTNAME_LWR], clsLanguageManager::mPtr->sTexts[LAN_MAX_ALWD_SCRIPT_NAME_LEN_256_CHARS]);
                    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand415") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
                    return true;
                }

                char * sBadChar = strpbrk(sCommand, "/\\");
				if(sBadChar != NULL || FileExist((clsServerManager::sScriptPath+sCommand).c_str()) == false) {
					int imsgLen = CheckFromPm(curUser, fromPM);

					int iret = sprintf(msg+imsgLen, "<%s> *** %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_ERROR_SCRIPT_NOT_EXIST]);
					imsgLen += iret;
					if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand426") == true) {
						curUser->SendCharDelayed(msg, imsgLen);
					}
					return true;
				}

				Script * curScript = clsScriptManager::mPtr->FindScript(sCommand);
				if(curScript != NULL) {
					if(curScript->bEnabled == true && curScript->LUA != NULL) {
						int imsgLen = CheckFromPm(curUser, fromPM);

						int iret = sprintf(msg+imsgLen, "<%s> *** %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_ERROR_SCRIPT_ALREDY_RUNNING]);
						imsgLen += iret;
						if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand416") == true) {
							curUser->SendCharDelayed(msg, imsgLen);
						}
						return true;
					}

					UncountDeflood(curUser, fromPM);

					if(clsScriptManager::mPtr->StartScript(curScript, true) == true) {
						if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES] == true) {
							if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES_AS_PM] == true) {
								int imsgLen = sprintf(msg, "%s $<%s> *** %s %s: %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                                    curUser->sNick, clsLanguageManager::mPtr->sTexts[LAN_STARTED_SCRIPT], sCommand);
								if(CheckSprintf(imsgLen, 1024, "clsHubCommands::DoCommand419") == true) {
									clsGlobalDataQueue::mPtr->SingleItemStore(msg, imsgLen, NULL, 0, clsGlobalDataQueue::SI_PM2OPS);
								}
							} else {
								int imsgLen = sprintf(msg, "<%s> *** %s %s: %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], curUser->sNick,
                                    clsLanguageManager::mPtr->sTexts[LAN_STARTED_SCRIPT], sCommand);
								if(CheckSprintf(imsgLen, 1024, "clsHubCommands::DoCommand420") == true) {
									clsGlobalDataQueue::mPtr->AddQueueItem(msg, imsgLen, NULL, 0, clsGlobalDataQueue::CMD_OPS);
								}
							}
						}

						if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES] == false || ((curUser->ui32BoolBits & User::BIT_OPERATOR) == User::BIT_OPERATOR) == false) {
							int imsgLen = CheckFromPm(curUser, fromPM);

							int iret = sprintf(msg+imsgLen, "<%s> %s %s %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_SCRIPT], sCommand,
                                clsLanguageManager::mPtr->sTexts[LAN_STARTED_LWR]);
							imsgLen += iret;
							if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand422") == true) {
								curUser->SendCharDelayed(msg, imsgLen);
							}
						}
						return true;
					} else {
						int imsgLen = CheckFromPm(curUser, fromPM);

						int iret = sprintf(msg+imsgLen, "<%s> *** %s %s %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_ERROR_SCRIPT], sCommand,
                            clsLanguageManager::mPtr->sTexts[LAN_START_FAILED]);
						imsgLen += iret;
						if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand424") == true) {
							curUser->SendCharDelayed(msg, imsgLen);
						}
						return true;
					}
				}

				UncountDeflood(curUser, fromPM);

				if(clsScriptManager::mPtr->AddScript(sCommand, true, true) == true && clsScriptManager::mPtr->StartScript(clsScriptManager::mPtr->ScriptTable[clsScriptManager::mPtr->ui8ScriptCount-1], false) == true) {
					if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES] == true) {
						if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES_AS_PM] == true) {
							int imsgLen = sprintf(msg, "%s $<%s> *** %s %s: %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                                curUser->sNick, clsLanguageManager::mPtr->sTexts[LAN_STARTED_SCRIPT], sCommand);
							if(CheckSprintf(imsgLen, 1024, "clsHubCommands::DoCommand427") == true) {
								clsGlobalDataQueue::mPtr->SingleItemStore(msg, imsgLen, NULL, 0, clsGlobalDataQueue::SI_PM2OPS);
							}
						} else {
							int imsgLen = sprintf(msg, "<%s> *** %s %s: %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], curUser->sNick, clsLanguageManager::mPtr->sTexts[LAN_STARTED_SCRIPT],
                                sCommand);
							if(CheckSprintf(imsgLen, 1024, "clsHubCommands::DoCommand428") == true) {
								clsGlobalDataQueue::mPtr->AddQueueItem(msg, imsgLen, NULL, 0, clsGlobalDataQueue::CMD_OPS);
							}
						}
					}

					if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES] == false || ((curUser->ui32BoolBits & User::BIT_OPERATOR) == User::BIT_OPERATOR) == false) {
						int imsgLen = CheckFromPm(curUser, fromPM);

						int iret = sprintf(msg+imsgLen, "<%s> %s %s %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_SCRIPT], sCommand,
                            clsLanguageManager::mPtr->sTexts[LAN_STARTED_LWR]);
						imsgLen += iret;
						if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand430") == true) {
							curUser->SendCharDelayed(msg, imsgLen);
						}
					}
					return true;
				} else {
					int imsgLen = CheckFromPm(curUser, fromPM);

					int iret = sprintf(msg+imsgLen, "<%s> *** %s %s %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_ERROR_SCRIPT], sCommand,
                        clsLanguageManager::mPtr->sTexts[LAN_START_FAILED]);
					imsgLen += iret;
					if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand432") == true) {
						curUser->SendCharDelayed(msg, imsgLen);
					}
					return true;
				}
			}

			return false;

		case 'p':
            //Hub-Commands: !passwd
			if(strncasecmp(sCommand+1, "asswd ", 6) == 0) {
                RegUser * pReg = clsRegManager::mPtr->Find(curUser);
                if(curUser->iProfile == -1 || pReg == NULL) {
                    int imsgLen = CheckFromPm(curUser, fromPM);

                    int iret = sprintf(msg+imsgLen, "<%s> *** %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_YOU_ARE_NOT_ALLOWED_TO_CHANGE_PASS]);
                    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand434") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
                    return true;
                }

                if(dlen < 8) {
                    int imsgLen = CheckFromPm(curUser, fromPM);

                    int iret = sprintf(msg+imsgLen, "<%s> *** %s %cpasswd <%s>. %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_SNTX_ERR_IN_CMD],
                        clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0], clsLanguageManager::mPtr->sTexts[LAN_NEW_PASSWORD], clsLanguageManager::mPtr->sTexts[LAN_NO_PARAM_GIVEN]);
                    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand436") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
                    return true;
                }

                if(dlen > 71) {
                    int imsgLen = CheckFromPm(curUser, fromPM);

                    int iret = sprintf(msg+imsgLen, "<%s> *** %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_MAX_ALWD_PASS_LEN_64_CHARS]);
                    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand438") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
                    return true;
                }

            	if(strchr(sCommand+7, '|') != NULL) {
                    int imsgLen = CheckFromPm(curUser, fromPM);

                    int iret = sprintf(msg+imsgLen, "<%s> *** %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_NO_PIPE_IN_PASS]);
                    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand439") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
                    return true;
                }

                size_t szPassLen = strlen(sCommand+7);

                if(pReg->UpdatePassword(sCommand+7, szPassLen) == false) {
                    return true;
                }

                clsRegManager::mPtr->Save(true);

#ifdef _BUILD_GUI
                if(clsRegisteredUsersDialog::mPtr != NULL) {
                    clsRegisteredUsersDialog::mPtr->RemoveReg(pReg);
                    clsRegisteredUsersDialog::mPtr->AddReg(pReg);
                }
#endif

                int imsgLen = CheckFromPm(curUser, fromPM);

                int iret = sprintf(msg+imsgLen, "<%s> *** %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_YOUR_PASSWORD_UPDATE_SUCCESS]);
                imsgLen += iret;
                if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand442") == true) {
                    curUser->SendCharDelayed(msg, imsgLen);
                }
                return true;
            }

            // Hub commands: !permunban
			if(strncasecmp(sCommand+1, "ermunban ", 9) == 0) {
                if(clsProfileManager::mPtr->IsAllowed(curUser, clsProfileManager::UNBAN) == false) {
                    SendNoPermission(curUser, fromPM);
                    return true;
                }

                if(dlen < 11 || sCommand[10] == '\0') {
                    int imsgLen = CheckFromPm(curUser, fromPM);

                    int iret = sprintf(msg+imsgLen, "<%s> *** %s %cpermunban <%s>. %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_SNTX_ERR_IN_CMD],
                        clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0], clsLanguageManager::mPtr->sTexts[LAN_IP_OR_NICK], clsLanguageManager::mPtr->sTexts[LAN_NO_PARAM_GIVEN]);
                    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand444") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
                    return true;
                }

                sCommand += 10;

                if(dlen > 100) {
                    int imsgLen = CheckFromPm(curUser, fromPM);

                    int iret = sprintf(msg+imsgLen, "<%s> *** %s %cpermunban <%s>. %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_SNTX_ERR_IN_CMD],
                        clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0], clsLanguageManager::mPtr->sTexts[LAN_IP_OR_NICK], clsLanguageManager::mPtr->sTexts[LAN_MAX_ALWD_NICK_LEN_64_CHARS]);
                    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand445") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
                    return true;
                }

                if(clsBanManager::mPtr->PermUnban(sCommand) == false) {
                    int imsgLen = CheckFromPm(curUser, fromPM);

                    int iret = sprintf(msg+imsgLen, "<%s> *** %s %s %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_SORRY], sCommand,
                        clsLanguageManager::mPtr->sTexts[LAN_IS_NOT_IN_BANS]);
                    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand446") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
                    return true;
                }

				UncountDeflood(curUser, fromPM);

                if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES] == true) {
                   	if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES_AS_PM] == true) {
           	            int imsgLen = sprintf(msg, "%s $<%s> *** %s %s %s %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                            curUser->sNick, clsLanguageManager::mPtr->sTexts[LAN_REMOVED_LWR], sCommand, clsLanguageManager::mPtr->sTexts[LAN_FROM_BANS]);
                        if(CheckSprintf(imsgLen, 1024, "clsHubCommands::DoCommand447") == true) {
                            clsGlobalDataQueue::mPtr->SingleItemStore(msg, imsgLen, NULL, 0, clsGlobalDataQueue::SI_PM2OPS);
                        }
              	    } else {
                   	    int imsgLen = sprintf(msg, "<%s> *** %s %s %s %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], curUser->sNick, clsLanguageManager::mPtr->sTexts[LAN_REMOVED_LWR],
                            sCommand, clsLanguageManager::mPtr->sTexts[LAN_FROM_BANS]);
                        if(CheckSprintf(imsgLen, 1024, "clsHubCommands::DoCommand448") == true) {
          	        	    clsGlobalDataQueue::mPtr->AddQueueItem(msg, imsgLen, NULL, 0, clsGlobalDataQueue::CMD_OPS);
                        }
          	    	}
                }

                if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES] == false || ((curUser->ui32BoolBits & User::BIT_OPERATOR) == User::BIT_OPERATOR) == false) {
                    int imsgLen = CheckFromPm(curUser, fromPM);

                    int iret = sprintf(msg+imsgLen, "<%s> %s %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], sCommand, clsLanguageManager::mPtr->sTexts[LAN_REMOVED_FROM_BANS]);
                    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand450") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
                }
                return true;
            }

            return false;

        case 'f':
            // Hub commands: !fullban nick reason
			if(strncasecmp(sCommand+1, "ullban ", 7) == 0) {
                if(clsProfileManager::mPtr->IsAllowed(curUser, clsProfileManager::BAN) == false) {
                    SendNoPermission(curUser, fromPM);
                    return true;
                }

                if(dlen < 9) {
                    int imsgLen = CheckFromPm(curUser, fromPM);

               	    int iret = sprintf(msg+imsgLen, "<%s> *** %s %cfullban <%s> <%s>. %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_SNTX_ERR_IN_CMD],
                        clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0], clsLanguageManager::mPtr->sTexts[LAN_NICK_LWR], clsLanguageManager::mPtr->sTexts[LAN_REASON_LWR],
                        clsLanguageManager::mPtr->sTexts[LAN_NO_PARAM_GIVEN]);
                    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand452") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
                    return true;
                }

                return Ban(curUser, sCommand+8, fromPM, true);
            }

            // Hub commands: !fullbanip ip reason
			if(strncasecmp(sCommand+1, "ullbanip ", 9) == 0) {
                if(clsProfileManager::mPtr->IsAllowed(curUser, clsProfileManager::BAN) == false) {
                    SendNoPermission(curUser, fromPM);
                    return true;
                }

                if(dlen < 16) {
                    int imsgLen = CheckFromPm(curUser, fromPM);

               	    int iret = sprintf(msg+imsgLen, "<%s> *** %s %cfullbanip <%s> <%s>. %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_SNTX_ERR_IN_CMD],
                        clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0], clsLanguageManager::mPtr->sTexts[LAN_IP], clsLanguageManager::mPtr->sTexts[LAN_REASON_LWR],
                        clsLanguageManager::mPtr->sTexts[LAN_NO_PARAM_GIVEN]);
                    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand454") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
                    return true;
                }

                return BanIp(curUser, sCommand+10, fromPM, true);
            }

            // Hub commands: !fulltempban nick time reason ... PPK m = minutes, h = hours, d = day, w = weeks, M = months (30 day per month), Y = years (365 day per year)
			if(strncasecmp(sCommand+1, "ulltempban ", 11) == 0) {
            	if(clsProfileManager::mPtr->IsAllowed(curUser, clsProfileManager::TEMP_BAN) == false) {
                    SendNoPermission(curUser, fromPM);
                    return true;
                }

                if(dlen < 16) {
                    int imsgLen = CheckFromPm(curUser, fromPM);

               	    int iret = sprintf(msg+imsgLen, "<%s> *** %s %cfulltempban <%s> <%s> <%s>. %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                        clsLanguageManager::mPtr->sTexts[LAN_SNTX_ERR_IN_CMD], clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0], clsLanguageManager::mPtr->sTexts[LAN_NICK_LWR],
                        clsLanguageManager::mPtr->sTexts[LAN_TIME_LWR], clsLanguageManager::mPtr->sTexts[LAN_REASON_LWR], clsLanguageManager::mPtr->sTexts[LAN_NO_PARAM_GIVEN]);
                    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand456") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
                    return true;
                }

                return TempBan(curUser, sCommand+12, dlen-12, fromPM, true);
            }

            // !fulltempbanip ip time reason ... PPK m = minutes, h = hours, d = day, w = weeks, M = months (30 day per month), Y = years (365 day per year)
			if(strncasecmp(sCommand+1, "ulltempbanip ", 13) == 0) {
                if(clsProfileManager::mPtr->IsAllowed(curUser, clsProfileManager::TEMP_BAN) == false) {
                    SendNoPermission(curUser, fromPM);
                    return true;
                }

                if(dlen < 24) {
                    int imsgLen = CheckFromPm(curUser, fromPM);

               	    int iret = sprintf(msg+imsgLen, "<%s> *** %s %cfulltempbanip <%s> <%s> <%s>. %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                        clsLanguageManager::mPtr->sTexts[LAN_SNTX_ERR_IN_CMD], clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0], clsLanguageManager::mPtr->sTexts[LAN_IP],
                        clsLanguageManager::mPtr->sTexts[LAN_TIME_LWR], clsLanguageManager::mPtr->sTexts[LAN_REASON_LWR], clsLanguageManager::mPtr->sTexts[LAN_NO_PARAM_GIVEN]);
                    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand458") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
                    return true;
                }

                return TempBanIp(curUser, sCommand+14, dlen-14, fromPM, true);
            }

            // Hub commands: !fullrangeban fromip toip reason
			if(strncasecmp(sCommand+1, "ullrangeban ", 12) == 0) {
                if(clsProfileManager::mPtr->IsAllowed(curUser, clsProfileManager::RANGE_BAN) == false) {
                    SendNoPermission(curUser, fromPM);
                    return true;
                }

                if(dlen < 28) {
                    int imsgLen = CheckFromPm(curUser, fromPM);

               	    int iret = sprintf(msg+imsgLen, "<%s> *** %s %cfullrangeban <%s> <%s> <%s>. %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                        clsLanguageManager::mPtr->sTexts[LAN_SNTX_ERR_IN_CMD], clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0], clsLanguageManager::mPtr->sTexts[LAN_FROMIP],
                        clsLanguageManager::mPtr->sTexts[LAN_TOIP], clsLanguageManager::mPtr->sTexts[LAN_REASON_LWR], clsLanguageManager::mPtr->sTexts[LAN_NO_PARAM_GIVEN]);
                    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand460") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
                    return true;
                }

                return RangeBan(curUser, sCommand+13, dlen-13, fromPM, true);
            }

            // Hub commands: !fullrangetempban fromip toip time reason
			if(strncasecmp(sCommand+1, "ullrangetempban ", 16) == 0) {
                if(clsProfileManager::mPtr->IsAllowed(curUser, clsProfileManager::RANGE_TBAN) == false) {
                    SendNoPermission(curUser, fromPM);
                    return true;
                }

                if(dlen < 35) {
                    int imsgLen = CheckFromPm(curUser, fromPM);

               	    int iret = sprintf(msg+imsgLen, "<%s> *** %s %cfullrangetempban <%s> <%s> <%s> <%s>. %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                        clsLanguageManager::mPtr->sTexts[LAN_SNTX_ERR_IN_CMD], clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0], clsLanguageManager::mPtr->sTexts[LAN_FROMIP],
                        clsLanguageManager::mPtr->sTexts[LAN_TOIP], clsLanguageManager::mPtr->sTexts[LAN_TIME_LWR], clsLanguageManager::mPtr->sTexts[LAN_REASON_LWR], clsLanguageManager::mPtr->sTexts[LAN_NO_PARAM_GIVEN]);
                    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand462") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
                    return true;
                }

                return RangeTempBan(curUser, sCommand+17, dlen-17, fromPM, true);
            }
            
            return false;
    }

/*	int imsgLen = sprintf(msg, "<%s> *** Unknown command.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC]);
    curUser->SendCharDelayed(msg, imsgLen);
    
    return(sCommand[0] == clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0]); // returns TRUE for unknown command, FALSE for Not_a_command*/
	return false; // PPK ... and send to all as chat ;)
}
//---------------------------------------------------------------------------

bool clsHubCommands::Ban(User * curUser, char * sCommand, bool fromPM, bool bFull) {
    char *reason = strchr(sCommand, ' ');
    if(reason != NULL) {
        reason[0] = '\0';
        if(reason[1] == '\0') {
            reason = NULL;
        } else {
            reason++;
        }
    }

    if(sCommand[0] == '\0') {
        int imsgLen = CheckFromPm(curUser, fromPM);

        int iret = sprintf(msg+imsgLen, "<%s> *** %s %c%sban <%s> <%s>. %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_SNTX_ERR_IN_CMD],
            clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0], bFull == true ? "full" : "", clsLanguageManager::mPtr->sTexts[LAN_NICK_LWR],
            clsLanguageManager::mPtr->sTexts[LAN_REASON_LWR], clsLanguageManager::mPtr->sTexts[LAN_BAD_PARAMS_GIVEN]);
        imsgLen += iret;
        if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand464") == true) {
            curUser->SendCharDelayed(msg, imsgLen);
        }
        return true;
    }

    if(strlen(sCommand) > 100) {
        int imsgLen = CheckFromPm(curUser, fromPM);

        int iret = sprintf(msg+imsgLen, "<%s> *** %s %c%sban <%s> <%s>. %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_SNTX_ERR_IN_CMD],
            clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0], bFull == true ? "full" : "", clsLanguageManager::mPtr->sTexts[LAN_NICK_LWR],
            clsLanguageManager::mPtr->sTexts[LAN_REASON_LWR], clsLanguageManager::mPtr->sTexts[LAN_MAX_ALWD_NICK_LEN_64_CHARS]);
        imsgLen += iret;
        if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand465") == true) {
            curUser->SendCharDelayed(msg, imsgLen);
        }
        return true;
    }

    // Self-ban ?
	if(strcasecmp(sCommand, curUser->sNick) == 0) {
        int imsgLen = CheckFromPm(curUser, fromPM);

        int iret = sprintf(msg+imsgLen, "<%s> %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_YOU_CANT_BAN_YOURSELF]);
        imsgLen += iret;
        if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand466") == true) {
            curUser->SendCharDelayed(msg, imsgLen);
        }
        return true;
    }

    User *uban = clsHashManager::mPtr->FindUser(sCommand, strlen(sCommand));
    if(uban != NULL) {
        // PPK don't ban user with higher profile
        if(uban->iProfile != -1 && curUser->iProfile > uban->iProfile) {
            int imsgLen = CheckFromPm(curUser, fromPM);

            int iret = sprintf(msg+imsgLen, "<%s> %s %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_YOU_ARE_NOT_ALWD_TO_BAN], sCommand);
            imsgLen += iret;
            if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand470") == true) {
                curUser->SendCharDelayed(msg, imsgLen);
            }
            return true;
        }

        UncountDeflood(curUser, fromPM);

        // Ban user
        clsBanManager::mPtr->Ban(uban, reason, curUser->sNick, bFull);

        // Send user a message that he has been banned
        int imsgLen;
        if(reason != NULL && strlen(reason) > 512) {
            imsgLen = sprintf(msg, "<%s> %s: ", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_YOU_HAD_BEEN_BANNED_BCS]);
            if(CheckSprintf(imsgLen, 1024, "clsHubCommands::DoCommand471-1") == false) {
                return true;
            }
            memcpy(msg+imsgLen, reason, 512);
            imsgLen += (int)strlen(reason) + 2;
            msg[imsgLen-2] = '.';
            msg[imsgLen-1] = '|';
            msg[imsgLen] = '\0';
        } else {
            imsgLen = sprintf(msg, "<%s> %s: %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_YOU_HAD_BEEN_BANNED_BCS],
                reason == NULL ? clsLanguageManager::mPtr->sTexts[LAN_NO_REASON_SPECIFIED] : reason);
            if(CheckSprintf(imsgLen, 1024, "clsHubCommands::DoCommand471-2") == false) {
                return true;
            }
        }

        uban->SendChar(msg, imsgLen);

        // Send message to all OPs that the user have been banned
        if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES] == true) {
            if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES_AS_PM] == true) {
                if(reason != NULL && strlen(reason) > 512) {
                    imsgLen = sprintf(msg, "%s $<%s> *** %s %s %s %s %s%s %s %s %s: ", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                        sCommand, clsLanguageManager::mPtr->sTexts[LAN_WITH_IP], uban->sIP, clsLanguageManager::mPtr->sTexts[LAN_HAS_BEEN], bFull == true ? clsLanguageManager::mPtr->sTexts[LAN_FULL_LWR] : "",
                        clsLanguageManager::mPtr->sTexts[LAN_BANNED_LWR], clsLanguageManager::mPtr->sTexts[LAN_BY_LWR], curUser->sNick, clsLanguageManager::mPtr->sTexts[LAN_BECAUSE_LWR]);
                    if(CheckSprintf(imsgLen, 1024, "clsHubCommands::DoCommand472-1") == false) {
                        return true;
                    }
                    memcpy(msg+imsgLen, reason, 512);
                    imsgLen += (int)strlen(reason) + 2;
                    msg[imsgLen-2] = '.';
                    msg[imsgLen-1] = '|';
                    msg[imsgLen] = '\0';
                } else {
                    imsgLen = sprintf(msg, "%s $<%s> *** %s %s %s %s %s%s %s %s %s: %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                        clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], sCommand, clsLanguageManager::mPtr->sTexts[LAN_WITH_IP], uban->sIP, clsLanguageManager::mPtr->sTexts[LAN_HAS_BEEN],
                        bFull == true ? clsLanguageManager::mPtr->sTexts[LAN_FULL_LWR] : "", clsLanguageManager::mPtr->sTexts[LAN_BANNED_LWR], clsLanguageManager::mPtr->sTexts[LAN_BY_LWR], curUser->sNick,
                        clsLanguageManager::mPtr->sTexts[LAN_BECAUSE_LWR], reason == NULL ? clsLanguageManager::mPtr->sTexts[LAN_NO_REASON_SPECIFIED] : reason);
                    if(CheckSprintf(imsgLen, 1024, "clsHubCommands::DoCommand472-2") == false) {
                        return true;
                    }
                }

                clsGlobalDataQueue::mPtr->SingleItemStore(msg, imsgLen, NULL, 0, clsGlobalDataQueue::SI_PM2OPS);
            } else {
                if(reason != NULL && strlen(reason) > 512) {
                    imsgLen = sprintf(msg, "<%s> *** %s %s %s %s %s%s %s %s %s: ", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], sCommand, clsLanguageManager::mPtr->sTexts[LAN_WITH_IP], uban->sIP,
                        clsLanguageManager::mPtr->sTexts[LAN_HAS_BEEN], bFull == true ? clsLanguageManager::mPtr->sTexts[LAN_FULL_LWR] : "", clsLanguageManager::mPtr->sTexts[LAN_BANNED_LWR],
                        clsLanguageManager::mPtr->sTexts[LAN_BY_LWR], curUser->sNick, clsLanguageManager::mPtr->sTexts[LAN_BECAUSE_LWR]);
                    if(CheckSprintf(imsgLen, 1024, "clsHubCommands::DoCommand473-1") == false) {
                        return true;
                    }
                    memcpy(msg+imsgLen, reason, 512);
                    imsgLen += (int)strlen(reason) + 2;
                    msg[imsgLen-2] = '.';
                    msg[imsgLen-1] = '|';
                    msg[imsgLen] = '\0';
                } else {
                    imsgLen = sprintf(msg, "<%s> *** %s %s %s %s %s%s %s %s %s: %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], sCommand, clsLanguageManager::mPtr->sTexts[LAN_WITH_IP], uban->sIP,
                        clsLanguageManager::mPtr->sTexts[LAN_HAS_BEEN], bFull == true ? clsLanguageManager::mPtr->sTexts[LAN_FULL_LWR] : "", clsLanguageManager::mPtr->sTexts[LAN_BANNED_LWR],
                        clsLanguageManager::mPtr->sTexts[LAN_BY_LWR], curUser->sNick, clsLanguageManager::mPtr->sTexts[LAN_BECAUSE_LWR], reason == NULL ? clsLanguageManager::mPtr->sTexts[LAN_NO_REASON_SPECIFIED] : reason);
                    if(CheckSprintf(imsgLen, 1024, "clsHubCommands::DoCommand473-2") == false) {
                        return true;
                    }
                }

                clsGlobalDataQueue::mPtr->AddQueueItem(msg, imsgLen, NULL, 0, clsGlobalDataQueue::CMD_OPS);
            }
        }

        if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES] == false || ((curUser->ui32BoolBits & User::BIT_OPERATOR) == User::BIT_OPERATOR) == false) {
            imsgLen = CheckFromPm(curUser, fromPM);

            int iret = sprintf(msg+imsgLen, "<%s> *** %s %s %s %s %s%s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], sCommand, clsLanguageManager::mPtr->sTexts[LAN_WITH_IP], uban->sIP,
                clsLanguageManager::mPtr->sTexts[LAN_HAS_BEEN], bFull == true ? clsLanguageManager::mPtr->sTexts[LAN_FULL_LWR] : "", clsLanguageManager::mPtr->sTexts[LAN_BANNED_LWR]);
            imsgLen += iret;
            if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand475") == true) {
                curUser->SendCharDelayed(msg, imsgLen);
            }
        }

        // Finish him !
        imsgLen = sprintf(msg, "[SYS] User %s (%s) %sbanned by %s", uban->sNick, uban->sIP, bFull == true ? clsLanguageManager::mPtr->sTexts[LAN_FULL_LWR] : "", curUser->sNick);
        if(CheckSprintf(imsgLen, 1024, "clsHubCommands::DoCommand476") == true) {
            clsUdpDebug::mPtr->Broadcast(msg, imsgLen);
        }
        uban->Close();
        return true;
    } else if(bFull == true) {
        int imsgLen = CheckFromPm(curUser, fromPM);

        int iret = sprintf(msg+imsgLen, "<%s> *** %s %s %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_ERROR], sCommand,
            clsLanguageManager::mPtr->sTexts[LAN_IS_NOT_ONLINE]);
        imsgLen += iret;
        if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand468") == true) {
            curUser->SendCharDelayed(msg, imsgLen);
        }
        return true;
    } else {
        return NickBan(curUser, sCommand, reason, fromPM);
    }
}
//---------------------------------------------------------------------------

bool clsHubCommands::BanIp(User * curUser, char * sCommand, bool fromPM, bool bFull) {
    char *reason = strchr(sCommand, ' ');
    if(reason != NULL) {
        reason[0] = '\0';
        if(reason[1] == '\0') {
            reason = NULL;
        } else {
            reason++;
        }
    }

    if(sCommand[0] == '\0') {
        int imsgLen = CheckFromPm(curUser, fromPM);

        int iret = sprintf(msg+imsgLen, "<%s> *** %s %c%sbanip <%s> <%s>. %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_SNTX_ERR_IN_CMD],
            clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0], bFull == true ? "full" : "", clsLanguageManager::mPtr->sTexts[LAN_IP],
            clsLanguageManager::mPtr->sTexts[LAN_REASON_LWR], clsLanguageManager::mPtr->sTexts[LAN_BAD_PARAMS_GIVEN]);
        imsgLen += iret;
        if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand478") == true) {
            curUser->SendCharDelayed(msg, imsgLen);
        }
        return true;
    }

    switch(clsBanManager::mPtr->BanIp(NULL, sCommand, reason, curUser->sNick, bFull)) {
        case 0: {
			UncountDeflood(curUser, fromPM);

			uint8_t ui128Hash[16];
			memset(ui128Hash, 0, 16);

            HashIP(sCommand, ui128Hash);
          
            User *next = clsHashManager::mPtr->FindUser(ui128Hash);
            while(next != NULL) {
            	User *cur = next;
                next = cur->hashiptablenext;

                if(cur == curUser || (cur->iProfile != -1 && clsProfileManager::mPtr->IsAllowed(cur, clsProfileManager::ENTERIFIPBAN) == true)) {
                    continue;
                }

                // PPK don't nickban user with higher profile
                if(cur->iProfile != -1 && curUser->iProfile > cur->iProfile) {
                    int imsgLen = CheckFromPm(curUser, fromPM);

                    int iret = sprintf(msg+imsgLen, "<%s> %s %s %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_YOU_NOT_ALLOWED_TO],
                        clsLanguageManager::mPtr->sTexts[LAN_BAN_LWR], cur->sNick);
                    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::BanIp1") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
                    continue;
                }

                int imsgLen;
                if(reason != NULL && strlen(reason) > 512) {
                    imsgLen = sprintf(msg, "<%s> %s: ", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_YOU_HAD_BEEN_BANNED_BCS]);
                    if(CheckSprintf(imsgLen, 1024, "clsHubCommands::BanIp2-1") == false) {
                        return true;
                    }
                    memcpy(msg+imsgLen, reason, 512);
                    imsgLen += (int)strlen(reason) + 2;
                    msg[imsgLen-2] = '.';
                    msg[imsgLen-1] = '|';
                    msg[imsgLen] = '\0';
                } else {
                    imsgLen = sprintf(msg, "<%s> %s: %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_YOU_HAD_BEEN_BANNED_BCS],
                        reason == NULL ? clsLanguageManager::mPtr->sTexts[LAN_NO_REASON_SPECIFIED] : reason);
                    if(CheckSprintf(imsgLen, 1024, "clsHubCommands::BanIp2-2") == false) {
                        return true;
                    }
                }

                cur->SendChar(msg, imsgLen);

                imsgLen = sprintf(msg, "[SYS] User %s (%s) ipbanned by %s", cur->sNick, cur->sIP, curUser->sNick);
                if(CheckSprintf(imsgLen, 1024, "clsHubCommands::BanIp3") == true) {
                    clsUdpDebug::mPtr->Broadcast(msg, imsgLen);
                }
                cur->Close();
            }

            if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES] == true) {
                if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES_AS_PM] == true) {
                    int imsgLen;
                    if(reason == NULL) {
                        imsgLen = sprintf(msg, "%s $<%s> *** %s %s %s%s %s %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                            sCommand, clsLanguageManager::mPtr->sTexts[LAN_IS_LWR], bFull == true ? clsLanguageManager::mPtr->sTexts[LAN_FULL_LWR] : "", clsLanguageManager::mPtr->sTexts[LAN_BANNED_LWR],
                            clsLanguageManager::mPtr->sTexts[LAN_BY_LWR], curUser->sNick);
                        if(CheckSprintf(imsgLen, 1024, "clsHubCommands::DoCommand479") == false) {
                            return true;
                        }
                    } else {
                        if(strlen(reason) > 512) {
                            imsgLen = sprintf(msg, "%s $<%s> *** %s %s %s%s %s %s %s: ", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                                clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], sCommand, clsLanguageManager::mPtr->sTexts[LAN_IS_LWR], bFull == true ? clsLanguageManager::mPtr->sTexts[LAN_FULL_LWR] : "",
                                clsLanguageManager::mPtr->sTexts[LAN_BANNED_LWR], clsLanguageManager::mPtr->sTexts[LAN_BY_LWR], curUser->sNick, clsLanguageManager::mPtr->sTexts[LAN_BECAUSE_LWR]);
                            if(CheckSprintf(imsgLen, 1024, "clsHubCommands::DoCommand480-1") == false) {
                                return true;
                            }
                            memcpy(msg+imsgLen, reason, 512);
                            imsgLen += (int)strlen(reason) + 2;
                            msg[imsgLen-2] = '.';
                            msg[imsgLen-1] = '|';
                            msg[imsgLen] = '\0';
                        } else {
                            imsgLen = sprintf(msg, "%s $<%s> *** %s %s %s%s %s %s %s: %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                                clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], sCommand, clsLanguageManager::mPtr->sTexts[LAN_IS_LWR], bFull == true ? clsLanguageManager::mPtr->sTexts[LAN_FULL_LWR] : "",
                                clsLanguageManager::mPtr->sTexts[LAN_BANNED_LWR], clsLanguageManager::mPtr->sTexts[LAN_BY_LWR], curUser->sNick, clsLanguageManager::mPtr->sTexts[LAN_BECAUSE_LWR], reason);
                            if(CheckSprintf(imsgLen, 1024, "clsHubCommands::DoCommand480-2") == false) {
                                return true;
                           }
                        }
                    }

					clsGlobalDataQueue::mPtr->SingleItemStore(msg, imsgLen, NULL, 0, clsGlobalDataQueue::SI_PM2OPS);
                } else {
                    int imsgLen;
                    if(reason == NULL) {
                        imsgLen = sprintf(msg, "<%s> *** %s %s %s%s %s %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], sCommand, clsLanguageManager::mPtr->sTexts[LAN_IS_LWR],
                            bFull == true ? clsLanguageManager::mPtr->sTexts[LAN_FULL_LWR] : "", clsLanguageManager::mPtr->sTexts[LAN_BANNED_LWR], clsLanguageManager::mPtr->sTexts[LAN_BY_LWR], curUser->sNick);
                        if(CheckSprintf(imsgLen, 1024, "clsHubCommands::DoCommand479") == false) {
                            return true;
                        }
                    } else {
                        if(strlen(reason) > 512) {
                            imsgLen = sprintf(msg, "%s $<%s> *** %s %s %s%s %s %s %s: ", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                                clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], sCommand, clsLanguageManager::mPtr->sTexts[LAN_IS_LWR], bFull == true ? clsLanguageManager::mPtr->sTexts[LAN_FULL_LWR] : "",
                                clsLanguageManager::mPtr->sTexts[LAN_BANNED_LWR], clsLanguageManager::mPtr->sTexts[LAN_BY_LWR], curUser->sNick, clsLanguageManager::mPtr->sTexts[LAN_BECAUSE_LWR]);
                            if(CheckSprintf(imsgLen, 1024, "clsHubCommands::DoCommand480-1") == false) {
                                return true;
                            }
                            memcpy(msg+imsgLen, reason, 512);
                            imsgLen += (int)strlen(reason) + 2;
                            msg[imsgLen-2] = '.';
                            msg[imsgLen-1] = '|';
                            msg[imsgLen] = '\0';
                        } else {
                            imsgLen = sprintf(msg, "<%s> *** %s %s %s%s %s %s %s: %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], sCommand, clsLanguageManager::mPtr->sTexts[LAN_IS_LWR],
                                bFull == true ? clsLanguageManager::mPtr->sTexts[LAN_FULL_LWR] : "", clsLanguageManager::mPtr->sTexts[LAN_BANNED_LWR], clsLanguageManager::mPtr->sTexts[LAN_BY_LWR], curUser->sNick,
                                clsLanguageManager::mPtr->sTexts[LAN_BECAUSE_LWR], reason);
                            if(CheckSprintf(imsgLen, 1024, "clsHubCommands::DoCommand480-2") == false) {
                                return true;
                           }
                        }
                    }

                    clsGlobalDataQueue::mPtr->AddQueueItem(msg, imsgLen, NULL, 0, clsGlobalDataQueue::CMD_OPS);
                }
            }

            if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES] == false || ((curUser->ui32BoolBits & User::BIT_OPERATOR) == User::BIT_OPERATOR) == false) {
                int imsgLen = CheckFromPm(curUser, fromPM);

                int iret = sprintf(msg+imsgLen, "<%s> %s %s %s%s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], sCommand, clsLanguageManager::mPtr->sTexts[LAN_IS_LWR],
                    bFull == true ? clsLanguageManager::mPtr->sTexts[LAN_FULL_LWR] : "", clsLanguageManager::mPtr->sTexts[LAN_BANNED_LWR]);
                imsgLen += iret;
                if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand484") == true) {
                    curUser->SendCharDelayed(msg, imsgLen);
                }
            }
            return true;
        }
        case 1: {
            int imsgLen = CheckFromPm(curUser, fromPM);

            int iret = sprintf(msg+imsgLen, "<%s> *** %s %c%sbanip <%s> <%s>. %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_SNTX_ERR_IN_CMD],
                clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0], bFull == true ? "full" : "", clsLanguageManager::mPtr->sTexts[LAN_IP],
                clsLanguageManager::mPtr->sTexts[LAN_REASON_LWR], clsLanguageManager::mPtr->sTexts[LAN_NO_VALID_IP_SPECIFIED]);
            imsgLen += iret;
            if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand486") == true) {
                curUser->SendCharDelayed(msg, imsgLen);
            }
            return true;
        }
        case 2: {
            int imsgLen = CheckFromPm(curUser, fromPM);

            int iret = sprintf(msg+imsgLen, "<%s> *** %s %s %s %s%s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_IP], sCommand,
                clsLanguageManager::mPtr->sTexts[LAN_IS_ALREADY], bFull == true ? clsLanguageManager::mPtr->sTexts[LAN_FULL_LWR] : "", clsLanguageManager::mPtr->sTexts[LAN_BANNED_LWR]);
            imsgLen += iret;
            if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand488") == true) {
                curUser->SendCharDelayed(msg, imsgLen);
            }
            return true;
        }
        default:
            return true;
    }
}
//---------------------------------------------------------------------------

bool clsHubCommands::NickBan(User * curUser, char * sNick, char * sReason, bool bFromPM) {
    RegUser * pReg = clsRegManager::mPtr->Find(sNick, strlen(sNick));

    // don't nickban user with higher profile
    if(pReg != NULL && curUser->iProfile > pReg->ui16Profile) {
        int imsgLen = CheckFromPm(curUser, bFromPM);

        int iret = sprintf(msg+imsgLen, "<%s> %s %s %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_YOU_NOT_ALLOWED_TO],
            clsLanguageManager::mPtr->sTexts[LAN_BAN_LWR], sNick);
        imsgLen += iret;
        if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::NickBan0") == true) {
            curUser->SendCharDelayed(msg, imsgLen);
        }
        return true;
    }

    if(clsBanManager::mPtr->NickBan(NULL, sNick, sReason, curUser->sNick) == true) {
        int imsgLen = sprintf(msg, "[SYS] User %s nickbanned by %s", sNick, curUser->sNick);
        if(CheckSprintf(imsgLen, 1024, "clsHubCommands::NickBan1") == true) {
            clsUdpDebug::mPtr->Broadcast(msg, imsgLen);
        }
    } else {
        int imsgLen = CheckFromPm(curUser, bFromPM);

        int iret = sprintf(msg+imsgLen, "<%s> *** %s %s %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_NICK], sNick,
            clsLanguageManager::mPtr->sTexts[LAN_IS_ALREDY_BANNED]);
        imsgLen += iret;
        if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::NickBan2") == true) {
            curUser->SendCharDelayed(msg, imsgLen);
        }

        return true;
    }

    UncountDeflood(curUser, bFromPM);

    if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES] == true) {
        int imsgLen = 0;
        if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES_AS_PM] == true) {
            imsgLen = sprintf(msg, "%s $", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC]);
            if(CheckSprintf(imsgLen, 1024, "clsHubCommands::NickBan3") == false) {
                return true;
            }
        }

        if(sReason == NULL) {
            int iret = sprintf(msg+imsgLen, "<%s> *** %s %s %s %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], curUser->sNick, clsLanguageManager::mPtr->sTexts[LAN_ADDED_LWR], sNick,
                clsLanguageManager::mPtr->sTexts[LAN_TO_BANS]);
            imsgLen += iret;
            if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::NickBan4") == false) {
                return true;
            }
        } else {
            if(strlen(sReason) > 512) {
                int iret = sprintf(msg+imsgLen, "<%s> *** %s %s %s %s: ", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], sNick, clsLanguageManager::mPtr->sTexts[LAN_HAS_BEEN_BANNED_BY], curUser->sNick,
                    clsLanguageManager::mPtr->sTexts[LAN_BECAUSE_LWR]);
                imsgLen += iret;
                if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::NickBan5") == false) {
                    return true;
                }
                memcpy(msg+imsgLen, sReason, 512);
                imsgLen += (int)strlen(sReason) + 2;
                msg[imsgLen-2] = '.';
                msg[imsgLen-1] = '|';
                msg[imsgLen] = '\0';
            } else {
                int iret = sprintf(msg+imsgLen, "<%s> *** %s %s %s %s: %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], sNick, clsLanguageManager::mPtr->sTexts[LAN_HAS_BEEN_BANNED_BY],
                    curUser->sNick, clsLanguageManager::mPtr->sTexts[LAN_BECAUSE_LWR], sReason);
                imsgLen += iret;
                if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::NickBan6") == false) {
                    return true;
                }
            }
        }

        if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES_AS_PM] == true) {
            clsGlobalDataQueue::mPtr->SingleItemStore(msg, imsgLen, NULL, 0, clsGlobalDataQueue::SI_PM2OPS);
        } else {
            clsGlobalDataQueue::mPtr->AddQueueItem(msg, imsgLen, NULL, 0, clsGlobalDataQueue::CMD_OPS);
        }
    }

    if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES] == false || ((curUser->ui32BoolBits & User::BIT_OPERATOR) == User::BIT_OPERATOR) == false) {
        int imsgLen = CheckFromPm(curUser, bFromPM);

        int iret = sprintf(msg+imsgLen, "<%s> %s %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], sNick, clsLanguageManager::mPtr->sTexts[LAN_ADDED_TO_BANS]);
        imsgLen += iret;
        if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::NickBan7") == true) {
            curUser->SendCharDelayed(msg, imsgLen);
        }
    }

    return true;
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

bool clsHubCommands::TempBan(User * curUser, char * sCommand, const size_t &dlen, bool fromPM, bool bFull) {
    char *sCmdParts[] = { NULL, NULL, NULL };
    uint16_t iCmdPartsLen[] = { 0, 0, 0 };

    uint8_t cPart = 0;

    sCmdParts[cPart] = sCommand; // nick start

    for(size_t szi = 0; szi < dlen; szi++) {
        if(sCommand[szi] == ' ') {
            sCommand[szi] = '\0';
            iCmdPartsLen[cPart] = (uint16_t)((sCommand+szi)-sCmdParts[cPart]);

            // are we on last space ???
            if(cPart == 1) {
                sCmdParts[2] = sCommand+szi+1;
                iCmdPartsLen[2] = (uint16_t)(dlen-szi-1);
                break;
            }

            cPart++;
            sCmdParts[cPart] = sCommand+szi+1;
        }
    }

    if(sCmdParts[2] == NULL && iCmdPartsLen[1] == 0 && sCmdParts[1] != NULL) {
        iCmdPartsLen[1] = (uint16_t)(dlen-(sCmdParts[1]-sCommand));
    }

    if(sCmdParts[2] != NULL && iCmdPartsLen[2] == 0) {
        sCmdParts[2] = NULL;
    }

    if(iCmdPartsLen[0] == 0 || iCmdPartsLen[1] == 0) {
        int imsgLen = CheckFromPm(curUser, fromPM);

        int iret = sprintf(msg+imsgLen, "<%s> *** %s %c%stempban <%s> <%s> <%s>. %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_SNTX_ERR_IN_CMD],
            clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0], bFull == true ? "full" : "", clsLanguageManager::mPtr->sTexts[LAN_NICK_LWR],
            clsLanguageManager::mPtr->sTexts[LAN_TIME_LWR], clsLanguageManager::mPtr->sTexts[LAN_REASON_LWR], clsLanguageManager::mPtr->sTexts[LAN_BAD_PARAMS_GIVEN]);
        imsgLen += iret;
        if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand490") == true) {
            curUser->SendCharDelayed(msg, imsgLen);
        }
        return true;
    }

    if(iCmdPartsLen[0] > 100) {
        int imsgLen = CheckFromPm(curUser, fromPM);

        int iret = sprintf(msg+imsgLen, "<%s> *** %s %c%stempban <%s> <%s> <%s>. %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_SNTX_ERR_IN_CMD],
            clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0], bFull == true ? "full" : "", clsLanguageManager::mPtr->sTexts[LAN_NICK_LWR],
            clsLanguageManager::mPtr->sTexts[LAN_TIME_LWR], clsLanguageManager::mPtr->sTexts[LAN_REASON_LWR], clsLanguageManager::mPtr->sTexts[LAN_MAX_ALWD_NICK_LEN_64_CHARS]);
        imsgLen += iret;
        if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand491") == true) {
            curUser->SendCharDelayed(msg, imsgLen);
        }
        return true;
    }

    // Self-ban ?
	if(strcasecmp(sCmdParts[0], curUser->sNick) == 0) {
        int imsgLen = CheckFromPm(curUser, fromPM);

        int iret = sprintf(msg+imsgLen, "<%s> %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_YOU_CANT_BAN_YOURSELF]);
        imsgLen += iret;
        if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand492") == true) {
            curUser->SendCharDelayed(msg, imsgLen);
        }
        return true;
    }
           	    
    User *utempban = clsHashManager::mPtr->FindUser(sCmdParts[0], iCmdPartsLen[0]);
    if(utempban != NULL) {
        // PPK don't tempban user with higher profile
        if(utempban->iProfile != -1 && curUser->iProfile > utempban->iProfile) {
            int imsgLen = CheckFromPm(curUser, fromPM);

            int iret = sprintf(msg+imsgLen, "<%s> %s %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_YOU_ARE_NOT_ALWD_TO_TEMPBAN], sCmdParts[0]);
            imsgLen += iret;
            if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand496") == true) {
                curUser->SendCharDelayed(msg, imsgLen);
            }
            return true;
        }

        char cTime = sCmdParts[1][iCmdPartsLen[1]-1];
        sCmdParts[1][iCmdPartsLen[1]-1] = '\0';
        int iTime = atoi(sCmdParts[1]);
        time_t acc_time, ban_time;

        if(iTime <= 0 || GenerateTempBanTime(cTime, (uint32_t)iTime, acc_time, ban_time) == false) {
            int imsgLen = CheckFromPm(curUser, fromPM);

            int iret = sprintf(msg+imsgLen, "<%s> *** %s %c%stempban <%s> <%s> <%s>. %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_SNTX_ERR_IN_CMD],
                clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0], bFull == true ? "full" : "", clsLanguageManager::mPtr->sTexts[LAN_NICK_LWR],
                clsLanguageManager::mPtr->sTexts[LAN_TIME_LWR], clsLanguageManager::mPtr->sTexts[LAN_REASON_LWR], clsLanguageManager::mPtr->sTexts[LAN_BAD_TIME_SPECIFIED]);
                imsgLen += iret;
            if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand498") == true) {
                curUser->SendCharDelayed(msg, imsgLen);
            }
            return true;
        }

        clsBanManager::mPtr->TempBan(utempban, sCmdParts[2], curUser->sNick, 0, ban_time, bFull);
        UncountDeflood(curUser, fromPM);
        static char sTime[256];
        strcpy(sTime, formatTime((ban_time-acc_time)/60));

        // Send user a message that he has been tempbanned
        int imsgLen;
        if(sCmdParts[2] != NULL && iCmdPartsLen[2] > 512) {
            imsgLen = sprintf(msg, "<%s> %s: %s %s: ", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_YOU_HAD_BEEN_TEMP_BANNED_TO], sTime,
                clsLanguageManager::mPtr->sTexts[LAN_BECAUSE_LWR]);
            if(CheckSprintf(imsgLen, 1024, "clsHubCommands::DoCommand499-1") == false) {
                return true;
            }
            memcpy(msg+imsgLen, sCmdParts[2], 512);
            imsgLen += iCmdPartsLen[2] + 2;
            msg[imsgLen-2] = '.';
            msg[imsgLen-1] = '|';
            msg[imsgLen] = '\0';
        } else {
            imsgLen = sprintf(msg, "<%s> %s: %s %s: %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_YOU_HAD_BEEN_TEMP_BANNED_TO], sTime,
                clsLanguageManager::mPtr->sTexts[LAN_BECAUSE_LWR], sCmdParts[2] == NULL ? clsLanguageManager::mPtr->sTexts[LAN_NO_REASON_SPECIFIED] : sCmdParts[2]);
            if(CheckSprintf(imsgLen, 1024, "clsHubCommands::DoCommand499-2") == false) {
                return true;
            }
        }

        utempban->SendChar(msg, imsgLen);

        if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES] == true) {
            if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES_AS_PM] == true) {
                if(sCmdParts[2] != NULL && iCmdPartsLen[2] > 512) {
                    imsgLen = sprintf(msg, "%s $<%s> *** %s %s %s %s %s%s %s %s %s: %s %s: ", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                        clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], sCmdParts[0], clsLanguageManager::mPtr->sTexts[LAN_WITH_IP], utempban->sIP, clsLanguageManager::mPtr->sTexts[LAN_HAS_BEEN],
                        bFull == true ? clsLanguageManager::mPtr->sTexts[LAN_FULL_LWR] : "", clsLanguageManager::mPtr->sTexts[LAN_TEMP_BANNED], clsLanguageManager::mPtr->sTexts[LAN_BY_LWR], curUser->sNick,
                        clsLanguageManager::mPtr->sTexts[LAN_TO_LWR], sTime, clsLanguageManager::mPtr->sTexts[LAN_BECAUSE_LWR]);
                    if(CheckSprintf(imsgLen, 1024, "clsHubCommands::DoCommand500-1") == false) {
                        return true;
                    }
                    memcpy(msg+imsgLen, sCmdParts[2], 512);
                    imsgLen += iCmdPartsLen[2] + 2;
                    msg[imsgLen-2] = '.';
                    msg[imsgLen-1] = '|';
                    msg[imsgLen] = '\0';
                } else {
                    imsgLen = sprintf(msg, "%s $<%s> *** %s %s %s %s %s%s %s %s %s: %s %s: %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                        clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], sCmdParts[0], clsLanguageManager::mPtr->sTexts[LAN_WITH_IP], utempban->sIP, clsLanguageManager::mPtr->sTexts[LAN_HAS_BEEN],
                        bFull == true ? clsLanguageManager::mPtr->sTexts[LAN_FULL_LWR] : "", clsLanguageManager::mPtr->sTexts[LAN_TEMP_BANNED], clsLanguageManager::mPtr->sTexts[LAN_BY_LWR], curUser->sNick,
                        clsLanguageManager::mPtr->sTexts[LAN_TO_LWR], sTime, clsLanguageManager::mPtr->sTexts[LAN_BECAUSE_LWR], sCmdParts[2] == NULL ? clsLanguageManager::mPtr->sTexts[LAN_NO_REASON_SPECIFIED] : sCmdParts[2]);
                    if(CheckSprintf(imsgLen, 1024, "clsHubCommands::DoCommand500-2") == false) {
                        return true;
                    }
                }

                clsGlobalDataQueue::mPtr->SingleItemStore(msg, imsgLen, NULL, 0, clsGlobalDataQueue::SI_PM2OPS);
            } else {
                if(sCmdParts[2] != NULL && iCmdPartsLen[2] > 512) {
                    imsgLen = sprintf(msg, "<%s> *** %s %s %s %s %s%s %s %s %s: %s %s: ", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], sCmdParts[0], clsLanguageManager::mPtr->sTexts[LAN_WITH_IP],
                        utempban->sIP, clsLanguageManager::mPtr->sTexts[LAN_HAS_BEEN], bFull == true ? clsLanguageManager::mPtr->sTexts[LAN_FULL_LWR] : "", clsLanguageManager::mPtr->sTexts[LAN_TEMP_BANNED],
                        clsLanguageManager::mPtr->sTexts[LAN_BY_LWR], curUser->sNick, clsLanguageManager::mPtr->sTexts[LAN_TO_LWR], sTime, clsLanguageManager::mPtr->sTexts[LAN_BECAUSE_LWR]);
                    if(CheckSprintf(imsgLen, 1024, "clsHubCommands::DoCommand500-1") == false) {
                        return true;
                    }
                    memcpy(msg+imsgLen, sCmdParts[2], 512);
                    imsgLen += iCmdPartsLen[2] + 2;
                    msg[imsgLen-2] = '.';
                    msg[imsgLen-1] = '|';
                    msg[imsgLen] = '\0';
                } else {
                    imsgLen = sprintf(msg, "<%s> *** %s %s %s %s %s%s %s %s %s: %s %s: %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], sCmdParts[0],
                        clsLanguageManager::mPtr->sTexts[LAN_WITH_IP], utempban->sIP, clsLanguageManager::mPtr->sTexts[LAN_HAS_BEEN], bFull == true ? clsLanguageManager::mPtr->sTexts[LAN_FULL_LWR] : "",
                        clsLanguageManager::mPtr->sTexts[LAN_TEMP_BANNED], clsLanguageManager::mPtr->sTexts[LAN_BY_LWR], curUser->sNick, clsLanguageManager::mPtr->sTexts[LAN_TO_LWR], sTime,
                        clsLanguageManager::mPtr->sTexts[LAN_BECAUSE_LWR], sCmdParts[2] == NULL ? clsLanguageManager::mPtr->sTexts[LAN_NO_REASON_SPECIFIED] : sCmdParts[2]);
                    if(CheckSprintf(imsgLen, 1024, "clsHubCommands::DoCommand500-2") == false) {
                        return true;
                    }
                }

                clsGlobalDataQueue::mPtr->AddQueueItem(msg, imsgLen, NULL, 0, clsGlobalDataQueue::CMD_OPS);
            }
        }

        if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES] == false || ((curUser->ui32BoolBits & User::BIT_OPERATOR) == User::BIT_OPERATOR) == false) {
            imsgLen = CheckFromPm(curUser, fromPM);

            if(sCmdParts[2] != NULL && iCmdPartsLen[2] > 512) {
                int iret = sprintf(msg+imsgLen, "<%s> %s %s %s %s %s%s %s: %s %s: ", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], sCmdParts[0], clsLanguageManager::mPtr->sTexts[LAN_WITH_IP],
                    utempban->sIP, clsLanguageManager::mPtr->sTexts[LAN_HAS_BEEN], bFull == true ? clsLanguageManager::mPtr->sTexts[LAN_FULL_LWR] : "", clsLanguageManager::mPtr->sTexts[LAN_TEMP_BANNED],
                    clsLanguageManager::mPtr->sTexts[LAN_TO_LWR], sTime, clsLanguageManager::mPtr->sTexts[LAN_BECAUSE_LWR]);
                imsgLen += iret;
                if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand503-1") == false) {
                    return true;
                }

                memcpy(msg+imsgLen, sCmdParts[2], 512);
                imsgLen += iCmdPartsLen[2] + 2;
                msg[imsgLen-2] = '.';
                msg[imsgLen-1] = '|';
                msg[imsgLen] = '\0';
            } else {
                int iret = sprintf(msg+imsgLen, "<%s> %s %s %s %s %s%s %s: %s %s: %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], sCmdParts[0], clsLanguageManager::mPtr->sTexts[LAN_WITH_IP],
                    utempban->sIP, clsLanguageManager::mPtr->sTexts[LAN_HAS_BEEN], bFull == true ? clsLanguageManager::mPtr->sTexts[LAN_FULL_LWR] : "", clsLanguageManager::mPtr->sTexts[LAN_TEMP_BANNED],
                    clsLanguageManager::mPtr->sTexts[LAN_TO_LWR], sTime, clsLanguageManager::mPtr->sTexts[LAN_BECAUSE_LWR], sCmdParts[2] == NULL ? clsLanguageManager::mPtr->sTexts[LAN_NO_REASON_SPECIFIED] : sCmdParts[2]);
                imsgLen += iret;
                if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand503-2") == false) {
                    return true;
                }
            }

            curUser->SendCharDelayed(msg, imsgLen);
        }

        // Finish him !
        imsgLen = sprintf(msg, "[SYS] User %s (%s) %stemp banned by %s", utempban->sNick, utempban->sIP, bFull == true ? "full " : "", curUser->sNick);
        if(CheckSprintf(imsgLen, 1024, "clsHubCommands::DoCommand504") == true) {
            clsUdpDebug::mPtr->Broadcast(msg, imsgLen);
        }
        utempban->Close();
        return true;
    } else if(bFull == true) {
        int imsgLen = CheckFromPm(curUser, fromPM);

        int iret = sprintf(msg+imsgLen, "<%s> *** %s %s %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_ERROR], sCmdParts[0],
            clsLanguageManager::mPtr->sTexts[LAN_IS_NOT_ONLINE]);
        imsgLen += iret;
        if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand494") == true) {
            curUser->SendCharDelayed(msg, imsgLen);
        }
        return true;
    } else {
        return TempNickBan(curUser, sCmdParts[0], sCmdParts[1], iCmdPartsLen[1], sCmdParts[2], fromPM);
    }
}
//---------------------------------------------------------------------------

bool clsHubCommands::TempBanIp(User * curUser, char * sCommand, const size_t &dlen, bool fromPM, bool bFull) {
    char *sCmdParts[] = { NULL, NULL, NULL };
    uint16_t iCmdPartsLen[] = { 0, 0, 0 };

    uint8_t cPart = 0;

    sCmdParts[cPart] = sCommand; // nick start

    for(size_t szi = 0; szi < dlen; szi++) {
        if(sCommand[szi] == ' ') {
            sCommand[szi] = '\0';
            iCmdPartsLen[cPart] = (uint16_t)((sCommand+szi)-sCmdParts[cPart]);

            // are we on last space ???
            if(cPart == 1) {
                sCmdParts[2] = sCommand+szi+1;
                iCmdPartsLen[2] = (uint16_t)(dlen-szi-1);
                break;
            }

            cPart++;
            sCmdParts[cPart] = sCommand+szi+1;
        }
    }

    if(sCmdParts[2] == NULL && iCmdPartsLen[1] == 0 && sCmdParts[1] != NULL) {
        iCmdPartsLen[1] = (uint16_t)(dlen-(sCmdParts[1]-sCommand));
    }

    if(sCmdParts[2] != NULL && iCmdPartsLen[2] == 0) {
        sCmdParts[2] = NULL;
    }

    if(iCmdPartsLen[0] == 0 || iCmdPartsLen[1] == 0) {
        int imsgLen = CheckFromPm(curUser, fromPM);

        int iret = sprintf(msg+imsgLen, "<%s> *** %s %c%stempbanip <%s> <%s> <%s>. %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_SNTX_ERR_IN_CMD],
            clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0], bFull == true ? "full" : "", clsLanguageManager::mPtr->sTexts[LAN_IP],
            clsLanguageManager::mPtr->sTexts[LAN_TIME_LWR], clsLanguageManager::mPtr->sTexts[LAN_REASON_LWR], clsLanguageManager::mPtr->sTexts[LAN_BAD_PARAMS_GIVEN]);
        imsgLen += iret;
        if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand506") == true) {
            curUser->SendCharDelayed(msg, imsgLen);
        }
        return true;
    }

    char cTime = sCmdParts[1][iCmdPartsLen[1]-1];
    sCmdParts[1][iCmdPartsLen[1]-1] = '\0';
    int iTime = atoi(sCmdParts[1]);
    time_t acc_time, ban_time;

    if(iTime <= 0 || GenerateTempBanTime(cTime, (uint32_t)iTime, acc_time, ban_time) == false) {
        int imsgLen = CheckFromPm(curUser, fromPM);

        int iret = sprintf(msg+imsgLen, "<%s> *** %s %c%stempbanip <%s> <%s> <%s>. %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_SNTX_ERR_IN_CMD],
            clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0], bFull == true ? "full" : "", clsLanguageManager::mPtr->sTexts[LAN_IP],
            clsLanguageManager::mPtr->sTexts[LAN_TIME_LWR], clsLanguageManager::mPtr->sTexts[LAN_REASON_LWR], clsLanguageManager::mPtr->sTexts[LAN_BAD_TIME_SPECIFIED]);
        imsgLen += iret;
        if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand508") == true) {
            curUser->SendCharDelayed(msg, imsgLen);
        }
        return true;
    }

    switch(clsBanManager::mPtr->TempBanIp(NULL, sCmdParts[0], sCmdParts[2], curUser->sNick, 0, ban_time, bFull)) {
        case 0: {
			uint8_t ui128Hash[16];
			memset(ui128Hash, 0, 16);

            HashIP(sCommand, ui128Hash);
          
            User *next = clsHashManager::mPtr->FindUser(ui128Hash);
            while(next != NULL) {
            	User *cur = next;
                next = cur->hashiptablenext;

                if(cur == curUser || (cur->iProfile != -1 && clsProfileManager::mPtr->IsAllowed(cur, clsProfileManager::ENTERIFIPBAN) == true)) {
                    continue;
                }

                // PPK don't nickban user with higher profile
                if(cur->iProfile != -1 && curUser->iProfile > cur->iProfile) {
                    int imsgLen = CheckFromPm(curUser, fromPM);

                    int iret = sprintf(msg+imsgLen, "<%s> %s %s %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_YOU_NOT_ALLOWED_TO],
                        clsLanguageManager::mPtr->sTexts[LAN_BAN_LWR], cur->sNick);
                    imsgLen += iret;
                    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::TempBanIp1") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
                    continue;
                }

                int imsgLen;
                if(sCmdParts[2] != NULL && iCmdPartsLen[2] > 512) {
                    imsgLen = sprintf(msg, "<%s> %s: ", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_YOU_HAD_BEEN_BANNED_BCS]);
                    if(CheckSprintf(imsgLen, 1024, "clsHubCommands::TempBanIp2-1") == false) {
                        return true;
                    }
                    memcpy(msg+imsgLen, sCmdParts[2], 512);
                    imsgLen += iCmdPartsLen[2] + 2;
                    msg[imsgLen-2] = '.';
                    msg[imsgLen-1] = '|';
                    msg[imsgLen] = '\0';
                } else {
                    imsgLen = sprintf(msg, "<%s> %s: %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_YOU_HAD_BEEN_BANNED_BCS],
                        sCmdParts[2] == NULL ? clsLanguageManager::mPtr->sTexts[LAN_NO_REASON_SPECIFIED] : sCmdParts[2]);
                    if(CheckSprintf(imsgLen, 1024, "clsHubCommands::TempBanIp2-2") == false) {
                        return true;
                    }
                }

                cur->SendChar(msg, imsgLen);

                imsgLen = sprintf(msg, "[SYS] User %s (%s) tempipbanned by %s", cur->sNick, cur->sIP, curUser->sNick);
                if(CheckSprintf(imsgLen, 1024, "clsHubCommands::TempBanIp3") == true) {
                    clsUdpDebug::mPtr->Broadcast(msg, imsgLen);
                }
                cur->Close();
            }
            break;
        }
        case 1: {
            int imsgLen = CheckFromPm(curUser, fromPM);

            int iret = sprintf(msg+imsgLen, "<%s> *** %s %c%sbanip <%s> <%s>. %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_SNTX_ERR_IN_CMD],
                clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0], bFull == true ? "full" : "", clsLanguageManager::mPtr->sTexts[LAN_IP],
                clsLanguageManager::mPtr->sTexts[LAN_REASON_LWR], clsLanguageManager::mPtr->sTexts[LAN_NO_VALID_IP_SPECIFIED]);
            imsgLen += iret;
            if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand510") == true) {
                curUser->SendCharDelayed(msg, imsgLen);
            }
            return true;
        }
        case 2: {
            int imsgLen = CheckFromPm(curUser, fromPM);

            int iret = sprintf(msg+imsgLen, "<%s> *** %s %s %s %s%s %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_IP], sCmdParts[0],
                clsLanguageManager::mPtr->sTexts[LAN_IS_ALREADY], bFull == true ? clsLanguageManager::mPtr->sTexts[LAN_FULL_LWR] : "", clsLanguageManager::mPtr->sTexts[LAN_BANNED_LWR],
                clsLanguageManager::mPtr->sTexts[LAN_TO_LONGER_TIME]);
            imsgLen += iret;
            if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand512") == true) {
                curUser->SendCharDelayed(msg, imsgLen);
            }
            return true;
        }
        default:
            return true;
    }

	UncountDeflood(curUser, fromPM);
    static char sTime[256];
    strcpy(sTime, formatTime((ban_time-acc_time)/60));

    if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES] == true) {
        if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES_AS_PM] == true) {
            int imsgLen;
            if(sCmdParts[2] != NULL && iCmdPartsLen[2] > 512) {
                imsgLen = sprintf(msg, "%s $<%s> *** %s %s %s%s %s %s %s: %s %s: ", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                    sCommand, clsLanguageManager::mPtr->sTexts[LAN_HAS_BEEN], bFull == true ? clsLanguageManager::mPtr->sTexts[LAN_FULL_LWR] : "", clsLanguageManager::mPtr->sTexts[LAN_TEMP_BANNED],
                    clsLanguageManager::mPtr->sTexts[LAN_BY_LWR], curUser->sNick, clsLanguageManager::mPtr->sTexts[LAN_TO_LWR], sTime, clsLanguageManager::mPtr->sTexts[LAN_BECAUSE_LWR]);
                if(CheckSprintf(imsgLen, 1024, "clsHubCommands::DoCommand513-1") == false) {
                    return true;
                }
                memcpy(msg+imsgLen, sCmdParts[2], 512);
                imsgLen += iCmdPartsLen[2] + 2;
                msg[imsgLen-2] = '.';
                msg[imsgLen-1] = '|';
                msg[imsgLen] = '\0';
            } else {
                imsgLen = sprintf(msg, "%s $<%s> *** %s %s %s%s %s %s %s: %s %s: %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                    sCommand, clsLanguageManager::mPtr->sTexts[LAN_HAS_BEEN], bFull == true ? clsLanguageManager::mPtr->sTexts[LAN_FULL_LWR] : "", clsLanguageManager::mPtr->sTexts[LAN_TEMP_BANNED],
                    clsLanguageManager::mPtr->sTexts[LAN_BY_LWR], curUser->sNick, clsLanguageManager::mPtr->sTexts[LAN_TO_LWR], sTime, clsLanguageManager::mPtr->sTexts[LAN_BECAUSE_LWR],
                    sCmdParts[2] == NULL ? clsLanguageManager::mPtr->sTexts[LAN_NO_REASON_SPECIFIED] : sCmdParts[2]);
                if(CheckSprintf(imsgLen, 1024, "clsHubCommands::DoCommand513-2") == false) {
                    return true;
                }
            }

			clsGlobalDataQueue::mPtr->SingleItemStore(msg, imsgLen, NULL, 0, clsGlobalDataQueue::SI_PM2OPS);
        } else {
            int imsgLen;
            if(sCmdParts[2] != NULL && iCmdPartsLen[2] > 512) {
                imsgLen = sprintf(msg, "<%s> *** %s %s %s%s %s %s %s: %s %s: ", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], sCommand, clsLanguageManager::mPtr->sTexts[LAN_HAS_BEEN],
                    bFull == true ? clsLanguageManager::mPtr->sTexts[LAN_FULL_LWR] : "", clsLanguageManager::mPtr->sTexts[LAN_TEMP_BANNED], clsLanguageManager::mPtr->sTexts[LAN_BY_LWR], curUser->sNick,
                    clsLanguageManager::mPtr->sTexts[LAN_TO_LWR], sTime, clsLanguageManager::mPtr->sTexts[LAN_BECAUSE_LWR]);
                if(CheckSprintf(imsgLen, 1024, "clsHubCommands::DoCommand514-1") == false) {
                    return true;
                }
                memcpy(msg+imsgLen, sCmdParts[2], 512);
                imsgLen += iCmdPartsLen[2] + 2;
                msg[imsgLen-2] = '.';
                msg[imsgLen-1] = '|';
                msg[imsgLen] = '\0';
            } else {
                imsgLen = sprintf(msg, "<%s> *** %s %s %s%s %s %s %s: %s %s: %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], sCommand, clsLanguageManager::mPtr->sTexts[LAN_HAS_BEEN],
                    bFull == true ? clsLanguageManager::mPtr->sTexts[LAN_FULL_LWR] : "", clsLanguageManager::mPtr->sTexts[LAN_TEMP_BANNED], clsLanguageManager::mPtr->sTexts[LAN_BY_LWR], curUser->sNick,
                    clsLanguageManager::mPtr->sTexts[LAN_TO_LWR], sTime, clsLanguageManager::mPtr->sTexts[LAN_BECAUSE_LWR], sCmdParts[2] == NULL ? clsLanguageManager::mPtr->sTexts[LAN_NO_REASON_SPECIFIED] : sCmdParts[2]);
                if(CheckSprintf(imsgLen, 1024, "clsHubCommands::DoCommand514-2") == false) {
                    return true;
                }
            }

            clsGlobalDataQueue::mPtr->AddQueueItem(msg, imsgLen, NULL, 0, clsGlobalDataQueue::CMD_OPS);
        }
    }

    if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES] == false || ((curUser->ui32BoolBits & User::BIT_OPERATOR) == User::BIT_OPERATOR) == false) {
        int imsgLen = CheckFromPm(curUser, fromPM);

        if(sCmdParts[2] != NULL && iCmdPartsLen[2] > 512) {
            int iret = sprintf(msg+imsgLen, "<%s> %s %s %s%s %s: %s %s: ", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], sCmdParts[0], clsLanguageManager::mPtr->sTexts[LAN_HAS_BEEN],
                bFull == true ? clsLanguageManager::mPtr->sTexts[LAN_FULL_LWR] : "", clsLanguageManager::mPtr->sTexts[LAN_TEMP_BANNED], clsLanguageManager::mPtr->sTexts[LAN_TO_LWR], sTime,
                clsLanguageManager::mPtr->sTexts[LAN_BECAUSE_LWR]);
            imsgLen += iret;
            if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand516-1") == false) {
                return true;
            }

            memcpy(msg+imsgLen, sCmdParts[2], 512);
            imsgLen += iCmdPartsLen[2] + 2;
            msg[imsgLen-2] = '.';
            msg[imsgLen-1] = '|';
            msg[imsgLen] = '\0';
        } else {
            int iret = sprintf(msg+imsgLen, "<%s> %s %s %s%s %s: %s %s: %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], sCmdParts[0], clsLanguageManager::mPtr->sTexts[LAN_HAS_BEEN],
                bFull == true ? clsLanguageManager::mPtr->sTexts[LAN_FULL_LWR] : "", clsLanguageManager::mPtr->sTexts[LAN_TEMP_BANNED], clsLanguageManager::mPtr->sTexts[LAN_TO_LWR], sTime,
                clsLanguageManager::mPtr->sTexts[LAN_BECAUSE_LWR], sCmdParts[2] == NULL ? clsLanguageManager::mPtr->sTexts[LAN_NO_REASON_SPECIFIED] : sCmdParts[2]);
            imsgLen += iret;
            if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand516-2") == false) {
                return true;
            }
        }

        curUser->SendCharDelayed(msg, imsgLen);
    }

    int imsgLen = sprintf(msg, "[SYS] IP %s %stemp banned by %s", sCmdParts[0], bFull == true ? "full " : "", curUser->sNick);
    if(CheckSprintf(imsgLen, 1024, "clsHubCommands::DoCommand517") == true) {
        clsUdpDebug::mPtr->Broadcast(msg, imsgLen);
    }
    return true;
}
//---------------------------------------------------------------------------

bool clsHubCommands::TempNickBan(User * curUser, char * sNick, char * sTime, const size_t &szTimeLen, char * sReason, bool bFromPM) {
    RegUser * pReg = clsRegManager::mPtr->Find(sNick, strlen(sNick));

    // don't nickban user with higher profile
    if(pReg != NULL && curUser->iProfile > pReg->ui16Profile) {
        int imsgLen = CheckFromPm(curUser, bFromPM);

        int iret = sprintf(msg+imsgLen, "<%s> %s %s %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_YOU_NOT_ALLOWED_TO],
            clsLanguageManager::mPtr->sTexts[LAN_BAN_LWR], sNick);
        imsgLen += iret;
        if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::TempNickBan1") == true) {
            curUser->SendCharDelayed(msg, imsgLen);
        }
        return true;
    }

    char cTime = sTime[szTimeLen-1];
    sTime[szTimeLen-1] = '\0';
    int iTime = atoi(sTime);
    time_t acc_time, ban_time;

    if(iTime <= 0 || GenerateTempBanTime(cTime, (uint32_t)iTime, acc_time, ban_time) == false) {
        int imsgLen = CheckFromPm(curUser, bFromPM);

        int iret = sprintf(msg+imsgLen, "<%s> *** %s %cnicktempban <%s> <%s> <%s>. %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_SNTX_ERR_IN_CMD],
            clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0], clsLanguageManager::mPtr->sTexts[LAN_NICK_LWR], clsLanguageManager::mPtr->sTexts[LAN_TIME_LWR], clsLanguageManager::mPtr->sTexts[LAN_REASON_LWR],
            clsLanguageManager::mPtr->sTexts[LAN_BAD_TIME_SPECIFIED]);
        imsgLen += iret;
        if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::TempNickBan2") == true) {
            curUser->SendCharDelayed(msg, imsgLen);
        }

        return true;
    }

    if(clsBanManager::mPtr->NickTempBan(NULL, sNick, sReason, curUser->sNick, 0, ban_time) == false) {
        int imsgLen = CheckFromPm(curUser, bFromPM);

        int iret = sprintf(msg+imsgLen, "<%s> *** %s %s %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_NICK], sNick,
            clsLanguageManager::mPtr->sTexts[LAN_IS_ALRD_BANNED_LONGER_TIME]);
        imsgLen += iret;
        if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::TempNickBan3") == true) {
            curUser->SendCharDelayed(msg, imsgLen);
        }

        return true;
    }

	UncountDeflood(curUser, bFromPM);

    char sBanTime[256];
    strcpy(sBanTime, formatTime((ban_time-acc_time)/60));

    if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES] == true) {
        int imsgLen = 0;
        if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES_AS_PM] == true) {
            imsgLen = sprintf(msg, "%s $", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC]);
            if(CheckSprintf(imsgLen, 1024, "clsHubCommands::TempNickBan4") == false) {
                return true;
            }
        }

        if(sReason != NULL && strlen(sReason) > 512) {
            int iRet = sprintf(msg+imsgLen, "<%s> *** %s %s %s %s: %s %s: ", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], sNick, clsLanguageManager::mPtr->sTexts[LAN_HAS_BEEN_TMPBND_BY],
                curUser->sNick, clsLanguageManager::mPtr->sTexts[LAN_TO_LWR], sBanTime, clsLanguageManager::mPtr->sTexts[LAN_BECAUSE_LWR]);
            imsgLen += iRet;
            if(CheckSprintf1(iRet, imsgLen, 1024, "clsHubCommands::TempNickBan5") == false) {
                return true;
            }
            memcpy(msg+imsgLen, sReason, 512);
            imsgLen += (int)strlen(sReason) + 2;
            msg[imsgLen-2] = '.';
            msg[imsgLen-1] = '|';
            msg[imsgLen] = '\0';
        } else {
            int iRet = sprintf(msg+imsgLen, "<%s> *** %s %s %s %s: %s %s: %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], sNick, clsLanguageManager::mPtr->sTexts[LAN_HAS_BEEN_TMPBND_BY],
                curUser->sNick, clsLanguageManager::mPtr->sTexts[LAN_TO_LWR], sBanTime, clsLanguageManager::mPtr->sTexts[LAN_BECAUSE_LWR],
                sReason == NULL ? clsLanguageManager::mPtr->sTexts[LAN_NO_REASON_SPECIFIED] : sReason);
            imsgLen += iRet;
            if(CheckSprintf1(iRet, imsgLen, 1024, "clsHubCommands::TempNickBan6") == false) {
                return true;
            }
        }

        if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES_AS_PM] == true) {
			clsGlobalDataQueue::mPtr->SingleItemStore(msg, imsgLen, NULL, 0, clsGlobalDataQueue::SI_PM2OPS);
        } else {
            clsGlobalDataQueue::mPtr->AddQueueItem(msg, imsgLen, NULL, 0, clsGlobalDataQueue::CMD_OPS);
        }
    }

    if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES] == false || ((curUser->ui32BoolBits & User::BIT_OPERATOR) == User::BIT_OPERATOR) == false) {
        int imsgLen = CheckFromPm(curUser, bFromPM);

        if(sReason != NULL && strlen(sReason) > 512) {
            int iret = sprintf(msg+imsgLen, "<%s> %s %s: %s %s: ", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], sNick, clsLanguageManager::mPtr->sTexts[LAN_BEEN_TEMP_BANNED_TO], sBanTime,
                clsLanguageManager::mPtr->sTexts[LAN_BECAUSE_LWR]);
            imsgLen += iret;
            if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::TempNickBan7") == false) {
                return true;
            }

            memcpy(msg+imsgLen, sReason, 512);
            imsgLen += (int)strlen(sReason) + 2;
            msg[imsgLen-2] = '.';
            msg[imsgLen-1] = '|';
            msg[imsgLen] = '\0';
        } else {
            int iret = sprintf(msg+imsgLen, "<%s> %s %s: %s %s: %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], sNick, clsLanguageManager::mPtr->sTexts[LAN_BEEN_TEMP_BANNED_TO],
                sBanTime, clsLanguageManager::mPtr->sTexts[LAN_BECAUSE_LWR], sReason == NULL ? clsLanguageManager::mPtr->sTexts[LAN_NO_REASON_SPECIFIED] : sReason);
            imsgLen += iret;
            if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::TempNickBan8") == false) {
                return true;
            }
        }

        curUser->SendCharDelayed(msg, imsgLen);
    }

    int imsgLen = sprintf(msg, "[SYS] Nick %s tempbanned by %s", sNick, curUser->sNick);
    if(CheckSprintf(imsgLen, 1024, "clsHubCommands::TempNickBan9") == false) {
        return true;
    }

    clsUdpDebug::mPtr->Broadcast(msg, imsgLen);
    return true;
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

bool clsHubCommands::RangeBan(User * curUser, char * sCommand, const size_t &dlen, bool fromPM, bool bFull) {
    char *sCmdParts[] = { NULL, NULL, NULL };
    uint16_t iCmdPartsLen[] = { 0, 0, 0 };

    uint8_t cPart = 0;

    sCmdParts[cPart] = sCommand; // nick start

    for(size_t szi = 0; szi < dlen; szi++) {
        if(sCommand[szi] == ' ') {
            sCommand[szi] = '\0';
            iCmdPartsLen[cPart] = (uint16_t)((sCommand+szi)-sCmdParts[cPart]);

            // are we on last space ???
            if(cPart == 1) {
                sCmdParts[2] = sCommand+szi+1;
                iCmdPartsLen[2] = (uint16_t)(dlen-szi-1);
                break;
            }

            cPart++;
            sCmdParts[cPart] = sCommand+szi+1;
        }
    }

    if(sCmdParts[2] == NULL && iCmdPartsLen[1] == 0 && sCmdParts[1] != NULL) {
        iCmdPartsLen[1] = (uint16_t)(dlen-(sCmdParts[1]-sCommand));
    }

    if(sCmdParts[2] != NULL && iCmdPartsLen[2] == 0) {
        sCmdParts[2] = NULL;
    }

    if(iCmdPartsLen[0] > 15 || iCmdPartsLen[1] > 15) {
        int imsgLen = CheckFromPm(curUser, fromPM);

        int iret = sprintf(msg+imsgLen, "<%s> %s %c%srangeban <%s> <%s> <%s>. %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_SNTX_ERR_IN_CMD],
            clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0], bFull == true ? "full" : "", clsLanguageManager::mPtr->sTexts[LAN_FROMIP],
            clsLanguageManager::mPtr->sTexts[LAN_TOIP], clsLanguageManager::mPtr->sTexts[LAN_REASON_LWR], clsLanguageManager::mPtr->sTexts[LAN_MAX_ALWD_IP_LEN_15_CHARS]);
        imsgLen += iret;
        if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand518") == true) {
            curUser->SendCharDelayed(msg, imsgLen);
        }
        return true;
    }

	uint8_t ui128FromHash[16], ui128ToHash[16];
	memset(ui128FromHash, 0, 16);
	memset(ui128ToHash, 0, 16);

    if(iCmdPartsLen[0] == 0 || iCmdPartsLen[1] == 0 || HashIP(sCmdParts[0], ui128FromHash) == false || HashIP(sCmdParts[1], ui128ToHash) == false) {
        int imsgLen = CheckFromPm(curUser, fromPM);

        int iret = sprintf(msg+imsgLen, "<%s> %s %c%srangeban <%s> <%s> <%s>. %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_SNTX_ERR_IN_CMD],
            clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0], bFull == true ? "full" : "", clsLanguageManager::mPtr->sTexts[LAN_FROMIP],
            clsLanguageManager::mPtr->sTexts[LAN_TOIP], clsLanguageManager::mPtr->sTexts[LAN_REASON_LWR], clsLanguageManager::mPtr->sTexts[LAN_BAD_PARAMS_GIVEN]);
        imsgLen += iret;
        if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand519") == true) {
            curUser->SendCharDelayed(msg, imsgLen);
        }
        return true;
    }

    if(memcmp(ui128ToHash, ui128FromHash, 16) <= 0) {
        int imsgLen = CheckFromPm(curUser, fromPM);

        int iret = sprintf(msg+imsgLen, "<%s> *** %s %s %s %s %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_ERROR_FROM], sCmdParts[0],
            clsLanguageManager::mPtr->sTexts[LAN_TO_LWR], sCmdParts[1], clsLanguageManager::mPtr->sTexts[LAN_IS_NOT_VALID_RANGE]);
        imsgLen += iret;
        if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand521") == true) {
            curUser->SendCharDelayed(msg, imsgLen);
        }
        return true;
    }

    if(clsBanManager::mPtr->RangeBan(sCmdParts[0], ui128FromHash, sCmdParts[1], ui128ToHash, sCmdParts[2], curUser->sNick, bFull) == false) {
        int imsgLen = CheckFromPm(curUser, fromPM);

        int iret = sprintf(msg+imsgLen, "<%s> *** %s %s-%s %s %s%s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_RANGE], sCmdParts[0], sCmdParts[1],
            clsLanguageManager::mPtr->sTexts[LAN_IS_ALREADY], bFull == true ? clsLanguageManager::mPtr->sTexts[LAN_FULL_LWR] : "", clsLanguageManager::mPtr->sTexts[LAN_BANNED_LWR]);
        imsgLen += iret;
        if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand523") == true) {
            curUser->SendCharDelayed(msg, imsgLen);
        }
    }

	UncountDeflood(curUser, fromPM);

    if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES] == true) {
        if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES_AS_PM] == true) {
            int imsgLen;
            if(sCmdParts[2] == NULL) {
                imsgLen = sprintf(msg, "%s $<%s> *** %s %s-%s %s %s%s %s %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                    clsLanguageManager::mPtr->sTexts[LAN_RANGE], sCmdParts[0], sCmdParts[1], clsLanguageManager::mPtr->sTexts[LAN_IS_LWR], bFull == true ? clsLanguageManager::mPtr->sTexts[LAN_FULL_LWR] : "",
                    clsLanguageManager::mPtr->sTexts[LAN_BANNED_LWR], clsLanguageManager::mPtr->sTexts[LAN_BY_LWR], curUser->sNick);
                if(CheckSprintf(imsgLen, 1024, "clsHubCommands::DoCommand524") == false) {
                    return true;
                }
            } else {
                if(iCmdPartsLen[2] > 512) {
                    imsgLen = sprintf(msg, "%s $<%s> *** %s %s-%s %s %s%s %s %s %s: ", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                        clsLanguageManager::mPtr->sTexts[LAN_RANGE], sCmdParts[0], sCmdParts[1], clsLanguageManager::mPtr->sTexts[LAN_IS_LWR], bFull == true ? clsLanguageManager::mPtr->sTexts[LAN_FULL_LWR] : "",
                        clsLanguageManager::mPtr->sTexts[LAN_BANNED_LWR], clsLanguageManager::mPtr->sTexts[LAN_BY_LWR], curUser->sNick, clsLanguageManager::mPtr->sTexts[LAN_BECAUSE_LWR]);
                    if(CheckSprintf(imsgLen, 1024, "clsHubCommands::DoCommand525-1") == false) {
                        return true;
                    }
                    memcpy(msg+imsgLen, sCmdParts[2], 512);
                    imsgLen += iCmdPartsLen[2] + 2;
                    msg[imsgLen-2] = '.';
                    msg[imsgLen-1] = '|';
                    msg[imsgLen] = '\0';
                } else {
                    imsgLen = sprintf(msg, "%s $<%s> *** %s %s-%s %s %s%s %s %s %s: %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                        clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_RANGE], sCmdParts[0], sCmdParts[1], clsLanguageManager::mPtr->sTexts[LAN_IS_LWR],
                        bFull == true ? clsLanguageManager::mPtr->sTexts[LAN_FULL_LWR] : "", clsLanguageManager::mPtr->sTexts[LAN_BANNED_LWR], clsLanguageManager::mPtr->sTexts[LAN_BY_LWR], curUser->sNick,
                        clsLanguageManager::mPtr->sTexts[LAN_BECAUSE_LWR], sCmdParts[2]);
                    if(CheckSprintf(imsgLen, 1024, "clsHubCommands::DoCommand525-2") == false) {
                        return true;
                    }
                }
            }

			clsGlobalDataQueue::mPtr->SingleItemStore(msg, imsgLen, NULL, 0, clsGlobalDataQueue::SI_PM2OPS);
        } else {
            int imsgLen;
            if(sCmdParts[2] == NULL) {
                imsgLen = sprintf(msg, "<%s> *** %s %s-%s %s %s%s %s %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_RANGE], sCmdParts[0], sCmdParts[1],
                    clsLanguageManager::mPtr->sTexts[LAN_IS_LWR], bFull == true ? clsLanguageManager::mPtr->sTexts[LAN_FULL_LWR] : "", clsLanguageManager::mPtr->sTexts[LAN_BANNED_LWR],
                    clsLanguageManager::mPtr->sTexts[LAN_BY_LWR], curUser->sNick);
                if(CheckSprintf(imsgLen, 1024, "clsHubCommands::DoCommand526") == false) {
                    return true;
                }
            } else {
                if(iCmdPartsLen[2] > 512) {
                    imsgLen = sprintf(msg, "<%s> *** %s %s-%s %s %s%s %s %s %s: ", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_RANGE], sCmdParts[0],
                        sCmdParts[1], clsLanguageManager::mPtr->sTexts[LAN_IS_LWR], bFull == true ? clsLanguageManager::mPtr->sTexts[LAN_FULL_LWR] : "", clsLanguageManager::mPtr->sTexts[LAN_BANNED_LWR],
                        clsLanguageManager::mPtr->sTexts[LAN_BY_LWR], curUser->sNick, clsLanguageManager::mPtr->sTexts[LAN_BECAUSE_LWR]);
                    if(CheckSprintf(imsgLen, 1024, "clsHubCommands::DoCommand525-1") == false) {
                        return true;
                    }
                    memcpy(msg+imsgLen, sCmdParts[2], 512);
                    imsgLen += iCmdPartsLen[2] + 2;
                    msg[imsgLen-2] = '.';
                    msg[imsgLen-1] = '|';
                    msg[imsgLen] = '\0';
                } else {
                    imsgLen = sprintf(msg, "<%s> *** %s %s-%s %s %s%s %s %s %s: %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_RANGE], sCmdParts[0],
                        sCmdParts[1], clsLanguageManager::mPtr->sTexts[LAN_IS_LWR], bFull == true ? clsLanguageManager::mPtr->sTexts[LAN_FULL_LWR] : "", clsLanguageManager::mPtr->sTexts[LAN_BANNED_LWR],
                        clsLanguageManager::mPtr->sTexts[LAN_BY_LWR], curUser->sNick, clsLanguageManager::mPtr->sTexts[LAN_BECAUSE_LWR], sCmdParts[2]);
                    if(CheckSprintf(imsgLen, 1024, "clsHubCommands::DoCommand525-2") == false) {
                        return true;
                    }
                }
            }

            clsGlobalDataQueue::mPtr->AddQueueItem(msg, imsgLen, NULL, 0, clsGlobalDataQueue::CMD_OPS);
        }
    }

    if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES] == false || ((curUser->ui32BoolBits & User::BIT_OPERATOR) == User::BIT_OPERATOR) == false) {
        int imsgLen = CheckFromPm(curUser, fromPM);

        int iret = sprintf(msg+imsgLen, "<%s> %s %s-%s %s %s%s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_RANGE], sCmdParts[0], sCmdParts[1],
            clsLanguageManager::mPtr->sTexts[LAN_IS_LWR], bFull == true ? clsLanguageManager::mPtr->sTexts[LAN_FULL_LWR] : "", clsLanguageManager::mPtr->sTexts[LAN_BANNED_LWR]);
        imsgLen += iret;
        if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand529") == true) {
            curUser->SendCharDelayed(msg, imsgLen);
        }
    }

    return true;
}
//---------------------------------------------------------------------------

bool clsHubCommands::RangeTempBan(User * curUser, char * sCommand, const size_t &dlen, bool fromPM, bool bFull) {
    char *sCmdParts[] = { NULL, NULL, NULL, NULL };
    uint16_t iCmdPartsLen[] = { 0, 0, 0, 0 };

    uint8_t cPart = 0;

    sCmdParts[cPart] = sCommand; // nick start

    for(size_t szi = 0; szi < dlen; szi++) {
        if(sCommand[szi] == ' ') {
            sCommand[szi] = '\0';
            iCmdPartsLen[cPart] = (uint16_t)((sCommand+szi)-sCmdParts[cPart]);

            // are we on last space ???
            if(cPart == 2) {
                sCmdParts[3] = sCommand+szi+1;
                iCmdPartsLen[3] = (uint16_t)(dlen-szi-1);
                break;
            }

            cPart++;
            sCmdParts[cPart] = sCommand+szi+1;
        }
    }

    if(sCmdParts[3] == NULL && iCmdPartsLen[2] == 0 && sCmdParts[2] != NULL) {
        iCmdPartsLen[2] = (uint16_t)(dlen-(sCmdParts[2]-sCommand));
    }

    if(sCmdParts[3] != NULL && iCmdPartsLen[3] == 0) {
        sCmdParts[3] = NULL;
    }

    if(iCmdPartsLen[0] > 15 || iCmdPartsLen[1] > 15) {
        int imsgLen = CheckFromPm(curUser, fromPM);

        int iret = sprintf(msg+imsgLen, "<%s> %s %c%srangetempban <%s> <%s> <%s> <%s>. %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_SNTX_ERR_IN_CMD],
            clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0], bFull == true ? "full" : "", clsLanguageManager::mPtr->sTexts[LAN_FROMIP],
            clsLanguageManager::mPtr->sTexts[LAN_TOIP], clsLanguageManager::mPtr->sTexts[LAN_TIME_LWR], clsLanguageManager::mPtr->sTexts[LAN_REASON_LWR], clsLanguageManager::mPtr->sTexts[LAN_MAX_ALWD_IP_LEN_15_CHARS]);
        imsgLen += iret;
        if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand530") == true) {
            curUser->SendCharDelayed(msg, imsgLen);
        }
        return true;
    }

	uint8_t ui128FromHash[16], ui128ToHash[16];
	memset(ui128FromHash, 0, 16);
	memset(ui128ToHash, 0, 16);

    if(iCmdPartsLen[0] == 0 || iCmdPartsLen[1] == 0 || iCmdPartsLen[2] == 0 || HashIP(sCmdParts[0], ui128FromHash) == false || HashIP(sCmdParts[1], ui128ToHash) == false) {
        int imsgLen = CheckFromPm(curUser, fromPM);

        int iret = sprintf(msg+imsgLen, "<%s> %s %c%srangetempban <%s> <%s> <%s> <%s>. %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_SNTX_ERR_IN_CMD],
            clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0], bFull == true ? "full" : "", clsLanguageManager::mPtr->sTexts[LAN_FROMIP],
            clsLanguageManager::mPtr->sTexts[LAN_TOIP], clsLanguageManager::mPtr->sTexts[LAN_TIME_LWR], clsLanguageManager::mPtr->sTexts[LAN_REASON_LWR], clsLanguageManager::mPtr->sTexts[LAN_BAD_PARAMS_GIVEN]);
        imsgLen += iret;
        if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand531") == true) {
            curUser->SendCharDelayed(msg, imsgLen);
        }
        return true;
    }

    if(memcmp(ui128ToHash, ui128FromHash, 16) <= 0) {
        int imsgLen = CheckFromPm(curUser, fromPM);

        int iret = sprintf(msg+imsgLen, "<%s> *** %s %s %s %s %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_ERROR_FROM], sCmdParts[0],
            clsLanguageManager::mPtr->sTexts[LAN_TO_LWR], sCmdParts[1], clsLanguageManager::mPtr->sTexts[LAN_IS_NOT_VALID_RANGE]);
        imsgLen += iret;
        if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand533") == true) {
            curUser->SendCharDelayed(msg, imsgLen);
        }
        return true;
    }

    char cTime = sCmdParts[2][iCmdPartsLen[2]-1];
    sCmdParts[2][iCmdPartsLen[2]-1] = '\0';
    int iTime = atoi(sCmdParts[2]);
    time_t acc_time, ban_time;

    if(iTime <= 0 || GenerateTempBanTime(cTime, (uint32_t)iTime, acc_time, ban_time) == false) {
        int imsgLen = CheckFromPm(curUser, fromPM);

        int iret = sprintf(msg+imsgLen, "<%s> *** %s %c%srangetempban <%s> <%s> <%s> <%s>. %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
            clsLanguageManager::mPtr->sTexts[LAN_SNTX_ERR_IN_CMD], clsSettingManager::mPtr->sTexts[SETTXT_CHAT_COMMANDS_PREFIXES][0], bFull == true ? "full" : "",
            clsLanguageManager::mPtr->sTexts[LAN_FROMIP], clsLanguageManager::mPtr->sTexts[LAN_TOIP], clsLanguageManager::mPtr->sTexts[LAN_TIME_LWR], clsLanguageManager::mPtr->sTexts[LAN_REASON_LWR],
            clsLanguageManager::mPtr->sTexts[LAN_BAD_TIME_SPECIFIED]);
        imsgLen += iret;
        if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand535") == true) {
            curUser->SendCharDelayed(msg, imsgLen);
        }
        return true;
    }

    if(clsBanManager::mPtr->RangeTempBan(sCmdParts[0], ui128FromHash, sCmdParts[1], ui128ToHash, sCmdParts[3], curUser->sNick, 0, ban_time, bFull) == false) {
        int imsgLen = CheckFromPm(curUser, fromPM);

        int iret = sprintf(msg+imsgLen, "<%s> *** %s %s-%s %s %s%s %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_RANGE], sCmdParts[0], sCmdParts[1],
            clsLanguageManager::mPtr->sTexts[LAN_IS_ALREADY], bFull == true ? clsLanguageManager::mPtr->sTexts[LAN_FULL_LWR] : "", clsLanguageManager::mPtr->sTexts[LAN_BANNED_LWR],
            clsLanguageManager::mPtr->sTexts[LAN_TO_LONGER_TIME]);
        imsgLen += iret;
        if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand537") == true) {
            curUser->SendCharDelayed(msg, imsgLen);
        }
        return true;
    }

	UncountDeflood(curUser, fromPM);
    static char sTime[256];
    strcpy(sTime, formatTime((ban_time-acc_time)/60));

    if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES] == true) {
        if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES_AS_PM] == true) {
            int imsgLen;
            if(sCmdParts[3] == NULL) {
                imsgLen = sprintf(msg, "%s $<%s> *** %s %s-%s %s %s%s %s %s %s: %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                    clsLanguageManager::mPtr->sTexts[LAN_RANGE], sCmdParts[0], sCmdParts[1], clsLanguageManager::mPtr->sTexts[LAN_IS_LWR], bFull == true ? clsLanguageManager::mPtr->sTexts[LAN_FULL_LWR] : "",
                    clsLanguageManager::mPtr->sTexts[LAN_TEMP_BANNED],  clsLanguageManager::mPtr->sTexts[LAN_BY_LWR], curUser->sNick, clsLanguageManager::mPtr->sTexts[LAN_TO_LWR], sTime);
                if(CheckSprintf(imsgLen, 1024, "clsHubCommands::DoCommand538") == false) {
                    return true;
                }
            } else {
                if(iCmdPartsLen[3] > 512) {
                    imsgLen = sprintf(msg, "%s $<%s> *** %s %s-%s %s %s%s %s %s %s: %s %s: ", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                        clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_RANGE], sCmdParts[0], sCmdParts[1], clsLanguageManager::mPtr->sTexts[LAN_IS_LWR],
                        bFull == true ? clsLanguageManager::mPtr->sTexts[LAN_FULL_LWR] : "", clsLanguageManager::mPtr->sTexts[LAN_TEMP_BANNED], clsLanguageManager::mPtr->sTexts[LAN_BY_LWR], curUser->sNick,
                        clsLanguageManager::mPtr->sTexts[LAN_TO_LWR], sTime, clsLanguageManager::mPtr->sTexts[LAN_BECAUSE_LWR]);
                    if(CheckSprintf(imsgLen, 1024, "clsHubCommands::DoCommand539-1") == false) {
                        return true;
                    }
                    memcpy(msg+imsgLen, sCmdParts[3], 512);
                    imsgLen += iCmdPartsLen[3] + 2;
                    msg[imsgLen-2] = '.';
                    msg[imsgLen-1] = '|';
                    msg[imsgLen] = '\0';
                } else {
                    imsgLen = sprintf(msg, "%s $<%s> *** %s %s-%s %s %s%s %s %s %s: %s %s: %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                        clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_RANGE], sCmdParts[0], sCmdParts[1], clsLanguageManager::mPtr->sTexts[LAN_IS_LWR],
                        bFull == true ? clsLanguageManager::mPtr->sTexts[LAN_FULL_LWR] : "", clsLanguageManager::mPtr->sTexts[LAN_TEMP_BANNED], clsLanguageManager::mPtr->sTexts[LAN_BY_LWR], curUser->sNick,
                        clsLanguageManager::mPtr->sTexts[LAN_TO_LWR], sTime, clsLanguageManager::mPtr->sTexts[LAN_BECAUSE_LWR], sCmdParts[3]);
                    if(CheckSprintf(imsgLen, 1024, "clsHubCommands::DoCommand539-2") == false) {
                        return true;
                    }
                }
            }

			clsGlobalDataQueue::mPtr->SingleItemStore(msg, imsgLen, NULL, 0, clsGlobalDataQueue::SI_PM2OPS);
        } else {
            int imsgLen;
            if(sCmdParts[3] == NULL) {
                imsgLen = sprintf(msg, "<%s> *** %s %s-%s %s %s%s %s %s %s: %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_RANGE], sCmdParts[0],
                    sCmdParts[1], clsLanguageManager::mPtr->sTexts[LAN_IS_LWR], bFull == true ? clsLanguageManager::mPtr->sTexts[LAN_FULL_LWR] : "", clsLanguageManager::mPtr->sTexts[LAN_TEMP_BANNED],
                    clsLanguageManager::mPtr->sTexts[LAN_BY_LWR], curUser->sNick, clsLanguageManager::mPtr->sTexts[LAN_TO_LWR], sTime);
                if(CheckSprintf(imsgLen, 1024, "clsHubCommands::DoCommand540") == false) {
                    return true;
                }
            } else {
                if(iCmdPartsLen[3] > 512) {
                    imsgLen = sprintf(msg, "<%s> *** %s %s-%s %s %s%s %s %s %s: %s %s: ", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_RANGE], sCmdParts[0],
                        sCmdParts[1], clsLanguageManager::mPtr->sTexts[LAN_IS_LWR], bFull == true ? clsLanguageManager::mPtr->sTexts[LAN_FULL_LWR] : "", clsLanguageManager::mPtr->sTexts[LAN_TEMP_BANNED],
                        clsLanguageManager::mPtr->sTexts[LAN_BY_LWR], curUser->sNick, clsLanguageManager::mPtr->sTexts[LAN_TO_LWR], sTime, clsLanguageManager::mPtr->sTexts[LAN_BECAUSE_LWR]);
                    if(CheckSprintf(imsgLen, 1024, "clsHubCommands::DoCommand539-1") == false) {
                        return true;
                    }
                    memcpy(msg+imsgLen, sCmdParts[3], 512);
                    imsgLen += iCmdPartsLen[3] + 2;
                    msg[imsgLen-2] = '.';
                    msg[imsgLen-1] = '|';
                    msg[imsgLen] = '\0';
                } else {
                    imsgLen = sprintf(msg, "<%s> *** %s %s-%s %s %s%s %s %s %s: %s %s: %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_RANGE],
                        sCmdParts[0], sCmdParts[1], clsLanguageManager::mPtr->sTexts[LAN_IS_LWR], bFull == true ? clsLanguageManager::mPtr->sTexts[LAN_FULL_LWR] : "", clsLanguageManager::mPtr->sTexts[LAN_TEMP_BANNED],
                        clsLanguageManager::mPtr->sTexts[LAN_BY_LWR], curUser->sNick, clsLanguageManager::mPtr->sTexts[LAN_TO_LWR], sTime, clsLanguageManager::mPtr->sTexts[LAN_BECAUSE_LWR], sCmdParts[3]);
                    if(CheckSprintf(imsgLen, 1024, "clsHubCommands::DoCommand539-2") == false) {
                        return true;
                    }
                }
            }

            clsGlobalDataQueue::mPtr->AddQueueItem(msg, imsgLen, NULL, 0, clsGlobalDataQueue::CMD_OPS);
        }
    }

    if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES] == false || ((curUser->ui32BoolBits & User::BIT_OPERATOR) == User::BIT_OPERATOR) == false) {
        int imsgLen = CheckFromPm(curUser, fromPM);

        int iret = sprintf(msg+imsgLen, "<%s> %s %s-%s %s %s%s %s: %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_RANGE], sCmdParts[0], sCmdParts[1],
            clsLanguageManager::mPtr->sTexts[LAN_IS_LWR], bFull == true ? clsLanguageManager::mPtr->sTexts[LAN_FULL_LWR] : "", clsLanguageManager::mPtr->sTexts[LAN_TEMP_BANNED], clsLanguageManager::mPtr->sTexts[LAN_TO_LWR], sTime);
        imsgLen += iret;
        if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand543") == true) {
            curUser->SendCharDelayed(msg, imsgLen);
        }
    }
    return true;
}
//---------------------------------------------------------------------------

bool clsHubCommands::RangeUnban(User * curUser, char * sCommand, bool fromPM, unsigned char cType) {
	char *toip = strchr(sCommand, ' ');

	if(toip != NULL) {
        toip[0] = '\0';
        toip++;
    }

	uint8_t ui128FromHash[16], ui128ToHash[16];
	memset(ui128FromHash, 0, 16);
	memset(ui128ToHash, 0, 16);

	if(toip == NULL || sCommand[0] == '\0' || toip[0] == '\0' || HashIP(sCommand, ui128FromHash) == false || HashIP(toip, ui128ToHash) == false) {
        int imsgLen = CheckFromPm(curUser, fromPM);

        int iret= sprintf(msg+imsgLen, "<%s> *** %s. %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_SNTX_ERR_IN_CMD],
            clsLanguageManager::mPtr->sTexts[LAN_BAD_PARAMS_GIVEN]);
        imsgLen += iret;
        if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand545") == true) {
            curUser->SendCharDelayed(msg, imsgLen);
        }
        return true;
    }

    if(memcmp(ui128ToHash, ui128FromHash, 16) <= 0) {
        int imsgLen = CheckFromPm(curUser, fromPM);

        int iret = sprintf(msg+imsgLen, "<%s> *** %s %s %s %s %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_ERROR_FROM], sCommand,
            clsLanguageManager::mPtr->sTexts[LAN_TO_LWR], toip, clsLanguageManager::mPtr->sTexts[LAN_IS_NOT_VALID_RANGE]);
        imsgLen += iret;
        if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand547") == true) {
            curUser->SendCharDelayed(msg, imsgLen);
        }
        return true;
    }

    if(clsBanManager::mPtr->RangeUnban(ui128FromHash, ui128ToHash, cType) == false) {
        int imsgLen = CheckFromPm(curUser, fromPM);

        int iret = sprintf(msg+imsgLen, "<%s> %s %s-%s %s %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_RANGE], sCommand, toip,
            clsLanguageManager::mPtr->sTexts[LAN_IS_NOT_IN_MY_RANGE], cType == 1 ? clsLanguageManager::mPtr->sTexts[LAN_TEMP_BANS_LWR] : clsLanguageManager::mPtr->sTexts[LAN_PERM_BANS_LWR]);
        imsgLen += iret;
        if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand549") == true) {
            curUser->SendCharDelayed(msg, imsgLen);
        }
    }

	UncountDeflood(curUser, fromPM);

    if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES] == true) {
        if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES_AS_PM] == true) {
            int imsgLen = sprintf(msg, "%s $<%s> *** %s %s-%s %s %s %s %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                clsLanguageManager::mPtr->sTexts[LAN_RANGE], sCommand, toip, clsLanguageManager::mPtr->sTexts[LAN_IS_REMOVED_FROM_RANGE],
                cType == 1 ? clsLanguageManager::mPtr->sTexts[LAN_TEMP_BANS_LWR] : clsLanguageManager::mPtr->sTexts[LAN_PERM_BANS_LWR], clsLanguageManager::mPtr->sTexts[LAN_BY_LWR], curUser->sNick);
            if(CheckSprintf(imsgLen, 1024, "clsHubCommands::DoCommand550") == true) {
				clsGlobalDataQueue::mPtr->SingleItemStore(msg, imsgLen, NULL, 0, clsGlobalDataQueue::SI_PM2OPS);
            }
        } else {
            int imsgLen = sprintf(msg, "<%s> *** %s %s-%s %s %s by %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_RANGE], sCommand, toip,
                clsLanguageManager::mPtr->sTexts[LAN_IS_REMOVED_FROM_RANGE], cType == 1 ? clsLanguageManager::mPtr->sTexts[LAN_TEMP_BANS_LWR] : clsLanguageManager::mPtr->sTexts[LAN_PERM_BANS_LWR], curUser->sNick);
            if(CheckSprintf(imsgLen, 1024, "clsHubCommands::DoCommand551") == true) {
                clsGlobalDataQueue::mPtr->AddQueueItem(msg, imsgLen, NULL, 0, clsGlobalDataQueue::CMD_OPS);
            }
        }
    }

    if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES] == false || ((curUser->ui32BoolBits & User::BIT_OPERATOR) == User::BIT_OPERATOR) == false) {
        int imsgLen = CheckFromPm(curUser, fromPM);

        int iret = sprintf(msg+imsgLen, "<%s> %s %s-%s %s %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_RANGE], sCommand, toip,
            clsLanguageManager::mPtr->sTexts[LAN_IS_REMOVED_FROM_RANGE], cType == 1 ? clsLanguageManager::mPtr->sTexts[LAN_TEMP_BANS_LWR] : clsLanguageManager::mPtr->sTexts[LAN_PERM_BANS_LWR]);
        imsgLen += iret;
        if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand553") == true) {
            curUser->SendCharDelayed(msg, imsgLen);
        }
    }
    
    return true;
}
//---------------------------------------------------------------------------

bool clsHubCommands::RangeUnban(User * curUser, char * sCommand, bool fromPM) {
	char *toip = strchr(sCommand, ' ');

	if(toip != NULL) {
        toip[0] = '\0';
        toip++;
    }

	uint8_t ui128FromHash[16], ui128ToHash[16];
	memset(ui128FromHash, 0, 16);
	memset(ui128ToHash, 0, 16);

	if(toip == NULL || sCommand[0] == '\0' || toip[0] == '\0' || HashIP(sCommand, ui128FromHash) == false || HashIP(toip, ui128ToHash) == false) {
        int imsgLen = CheckFromPm(curUser, fromPM);

        int iret = sprintf(msg+imsgLen, "<%s> *** %s. %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_SNTX_ERR_IN_CMD],
            clsLanguageManager::mPtr->sTexts[LAN_BAD_PARAMS_GIVEN]);
        imsgLen += iret;
        if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand555") == true) {
            curUser->SendCharDelayed(msg, imsgLen);
        }
        return true;
    }

    if(memcmp(ui128ToHash, ui128FromHash, 16) <= 0) {
        int imsgLen = CheckFromPm(curUser, fromPM);

        int iret = sprintf(msg+imsgLen, "<%s> *** %s %s %s %s %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_ERROR_FROM], sCommand,
            clsLanguageManager::mPtr->sTexts[LAN_TO_LWR], toip, clsLanguageManager::mPtr->sTexts[LAN_IS_NOT_VALID_RANGE]);
        imsgLen += iret;
        if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand557") == true) {
            curUser->SendCharDelayed(msg, imsgLen);
        }
        return true;
    }

    if(clsBanManager::mPtr->RangeUnban(ui128FromHash, ui128ToHash) == false) {
        int imsgLen = CheckFromPm(curUser, fromPM);

        int iret = sprintf(msg+imsgLen, "<%s> %s %s-%s %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_RANGE], sCommand, toip,
            clsLanguageManager::mPtr->sTexts[LAN_IS_NOT_IN_MY_RANGE_BANS]);
        imsgLen += iret;
        if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand559") == true) {
            curUser->SendCharDelayed(msg, imsgLen);
        }
    }

    UncountDeflood(curUser, fromPM);

    if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES] == true) {
        if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES_AS_PM] == true) {
            int imsgLen = sprintf(msg, "%s $<%s> *** %s %s-%s %s %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                clsLanguageManager::mPtr->sTexts[LAN_RANGE], sCommand, toip, clsLanguageManager::mPtr->sTexts[LAN_IS_REMOVED_FROM_RANGE_BANS_BY], curUser->sNick);
            if(CheckSprintf(imsgLen, 1024, "clsHubCommands::DoCommand560") == true) {
                clsGlobalDataQueue::mPtr->SingleItemStore(msg, imsgLen, NULL, 0, clsGlobalDataQueue::SI_PM2OPS);
            }
        } else {
            int imsgLen = sprintf(msg, "<%s> *** %s %s-%s %s %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_RANGE], sCommand, toip,
                clsLanguageManager::mPtr->sTexts[LAN_IS_REMOVED_FROM_RANGE_BANS_BY], curUser->sNick);
            if(CheckSprintf(imsgLen, 1024, "clsHubCommands::DoCommand561") == true) {
                clsGlobalDataQueue::mPtr->AddQueueItem(msg, imsgLen, NULL, 0, clsGlobalDataQueue::CMD_OPS);
            }
        }
    }

    if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES] == false || ((curUser->ui32BoolBits & User::BIT_OPERATOR) == User::BIT_OPERATOR) == false) {
        int imsgLen = CheckFromPm(curUser, fromPM);

        int iret = sprintf(msg+imsgLen, "<%s> %s %s-%s %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_RANGE], sCommand, toip,
            clsLanguageManager::mPtr->sTexts[LAN_IS_REMOVED_FROM_RANGE_BANS]);
        imsgLen += iret;
        if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand563") == true) {
            curUser->SendCharDelayed(msg, imsgLen);
        }
    }
    
    return true;
}
//---------------------------------------------------------------------------

void clsHubCommands::SendNoPermission(User * curUser, const bool &fromPM) {
    int imsgLen = CheckFromPm(curUser, fromPM);

    int iret = sprintf(msg+imsgLen, "<%s> %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_YOU_ARE_NOT_ALWD_TO_USE_THIS_CMD]);
    imsgLen += iret;
    if(CheckSprintf1(iret, imsgLen, 1024, "clsHubCommands::DoCommand565") == true) {
        curUser->SendCharDelayed(msg, imsgLen);
    }
}
//---------------------------------------------------------------------------

int clsHubCommands::CheckFromPm(User * curUser, const bool &fromPM) {
    if(fromPM == false) {
        return 0;
    }

    int imsglen = sprintf(msg, "$To: %s From: %s $", curUser->sNick, clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC]);
    if(CheckSprintf(imsglen, 1024, "clsHubCommands::CheckFromPm") == false) {
        return 0;
    }

    return imsglen;
}
//---------------------------------------------------------------------------

void clsHubCommands::UncountDeflood(User * curUser, const bool &fromPM) {
    if(fromPM == false) {
        if(curUser->ui16ChatMsgs != 0) {
			curUser->ui16ChatMsgs--;
			curUser->ui16ChatMsgs2--;
        }
    } else {
        if(curUser->ui16PMs != 0) {
            curUser->ui16PMs--;
            curUser->ui16PMs2--;
        }
    }
}
//---------------------------------------------------------------------------
