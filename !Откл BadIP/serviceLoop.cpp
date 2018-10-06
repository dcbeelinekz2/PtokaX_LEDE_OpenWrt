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
#include "serviceLoop.h"
//---------------------------------------------------------------------------
#include "colUsers.h"
#include "eventqueue.h"
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
#include "LuaScript.h"
#include "RegThread.h"
//---------------------------------------------------------------------------
#ifdef _BUILD_GUI
    #include "../gui.win/MainWindowPageUsersChat.h"
#endif
//---------------------------------------------------------------------------
#ifndef _WIN32
	#include "regtmrinc.h"
	#include "scrtmrinc.h"
#endif
//---------------------------------------------------------------------------
clsServiceLoop * clsServiceLoop::mPtr = NULL;
#ifdef _WIN32
    UINT_PTR clsServiceLoop::srvLoopTimer = 0;
#endif
//---------------------------------------------------------------------------

#ifndef _WIN32
static void RegTimerHandler() {
    if(clsSettingManager::mPtr->bBools[SETBOOL_AUTO_REG] == true && clsSettingManager::mPtr->sTexts[SETTXT_REGISTER_SERVERS] != NULL) {
		// First destroy old hublist reg thread if any
        if(clsRegisterThread::mPtr != NULL) {
            clsRegisterThread::mPtr->Close();
            clsRegisterThread::mPtr->WaitFor();
            delete clsRegisterThread::mPtr;
        }
        
        // Create hublist reg thread
        clsRegisterThread::mPtr = new clsRegisterThread();
        if(clsRegisterThread::mPtr == NULL) {
        	AppendDebugLog("%s - [MEM] Cannot allocate clsRegisterThread::mPtr in RegTimerHandler\n", 0);
        	return;
        }
        
        // Setup hublist reg thread
        clsRegisterThread::mPtr->Setup(clsSettingManager::mPtr->sTexts[SETTXT_REGISTER_SERVERS], clsSettingManager::mPtr->ui16TextsLens[SETTXT_REGISTER_SERVERS]);
        
        // Start the hublist reg thread
    	clsRegisterThread::mPtr->Resume();
    }
}
#endif
//---------------------------------------------------------------------------

void clsServiceLoop::Looper() {
#ifdef _WIN32
	KillTimer(NULL, srvLoopTimer);
#else
	// alex82 ... Переместили сюда обработку таймеров, и тем самым повысили точность срабатывания в 2 раза
    while((iSIG = sigtimedwait(&sst, &info, &zerotime)) != -1) {
        if(iSIG == SIGSCRTMR) {
            ScriptOnTimer((ScriptTimer *)info.si_value.sival_ptr);
        } else if(iSIG == SIGREGTMR) {
            RegTimerHandler();
        }
    }
#endif

	// PPK ... two loop stategy for saving badwith
	if(bRecv == true) {
		ReceiveLoop();
	} else {
		SendLoop();
		clsEventQueue::mPtr->ProcessEvents();
	}

	if(clsServerManager::bServerTerminated == false) {
		bRecv = !bRecv;

#ifdef _WIN32
        srvLoopTimer = SetTimer(NULL, 0, 100, NULL);

	    if(srvLoopTimer == 0) {
	        AppendDebugLog("%s - [ERR] Cannot start Looper in clsServiceLoop::Looper\n", 0);
	        exit(EXIT_FAILURE);
	    }
#endif
	} else {
	    // tell the scripts about the end
	    clsScriptManager::mPtr->OnExit();
	    
	    // send last possible global data
	    clsGlobalDataQueue::mPtr->SendFinalQueue();
	    
	    clsServerManager::FinalStop(true);
	}
}
//---------------------------------------------------------------------------

clsServiceLoop::clsServiceLoop() {
    msg[0] = '\0';

#ifdef _WIN32
    InitializeCriticalSection(&csAcceptQueue);
#else
	iSIG = 0;

	pthread_mutex_init(&mtxAcceptQueue, NULL);
#endif

    AcceptedSocketsS = NULL;
    AcceptedSocketsE = NULL;
    
    bRecv = true;
    
    iLstUptmTck = clsServerManager::ui64ActualTick;

	clsServerManager::bServerTerminated = false;
	iSendRestsPeak = iRecvRestsPeak = iLastSendRest = iLastRecvRest = iLoopsForLogins = 0;
	dLoggedUsers = dActualSrvLoopLogins = 0;

#ifdef _WIN32
	srvLoopTimer = SetTimer(NULL, 0, 100, NULL);

    if(srvLoopTimer == 0) {
		AppendDebugLog("%s - [ERR] Cannot start Looper in clsServiceLoop::clsServiceLoop\n", 0);
    	exit(EXIT_FAILURE);
    }
#else
    sigemptyset(&sst);
    sigaddset(&sst, SIGSCRTMR);
    sigaddset(&sst, SIGREGTMR);

    zerotime.tv_sec = 0;
    zerotime.tv_nsec = 0;
#endif
}
//---------------------------------------------------------------------------

clsServiceLoop::~clsServiceLoop() {
    AcceptedSocket *nextsck = AcceptedSocketsS;
        
    while(nextsck != NULL) {
        AcceptedSocket *cursck = nextsck;
		nextsck = cursck->next;
#ifdef _WIN32
		shutdown(cursck->s, SD_SEND);
        closesocket(cursck->s);
#else
		shutdown(cursck->s, SHUT_RDWR);
        close(cursck->s);
#endif
        delete cursck;
	}
   
    AcceptedSocketsS = NULL;
    AcceptedSocketsE = NULL;

#ifdef _WIN32
	DeleteCriticalSection(&csAcceptQueue);
#else
	pthread_mutex_destroy(&mtxAcceptQueue);
#endif

	Cout("MainLoop terminated.");
}
//---------------------------------------------------------------------------

void clsServiceLoop::AcceptUser(AcceptedSocket *AccptSocket) {
    bool bIPv6 = false;

    char sIP[46];

    uint8_t ui128IpHash[16];
    memset(ui128IpHash, 0, 16);

    uint16_t ui16IpTableIdx = 0;

    if(AccptSocket->addr.ss_family == AF_INET6) {
        memcpy(ui128IpHash, &((struct sockaddr_in6 *)&AccptSocket->addr)->sin6_addr.s6_addr, 16);

        if(IN6_IS_ADDR_V4MAPPED(&((struct sockaddr_in6 *)&AccptSocket->addr)->sin6_addr)) {
			in_addr ipv4addr;
			memcpy(&ipv4addr, ((struct sockaddr_in6 *)&AccptSocket->addr)->sin6_addr.s6_addr + 12, 4);
			strcpy(sIP, inet_ntoa(ipv4addr));

            ui16IpTableIdx = ui128IpHash[14] * ui128IpHash[15];
        } else {
            bIPv6 = true;
#ifdef _WIN32
            win_inet_ntop(&((struct sockaddr_in6 *)&AccptSocket->addr)->sin6_addr, sIP, 46);
#else
            inet_ntop(AF_INET6, &((struct sockaddr_in6 *)&AccptSocket->addr)->sin6_addr, sIP, 46);
#endif
            ui16IpTableIdx = GetIpTableIdx(ui128IpHash);
        }
    } else {
        strcpy(sIP, inet_ntoa(((struct sockaddr_in *)&AccptSocket->addr)->sin_addr));

        ui128IpHash[10] = 255;
        ui128IpHash[11] = 255;
        memcpy(ui128IpHash+12, &((struct sockaddr_in *)&AccptSocket->addr)->sin_addr.s_addr, 4);

        ui16IpTableIdx = ui128IpHash[14] * ui128IpHash[15];
    }

    // set the recv buffer
#ifdef _WIN32
    int32_t bufsize = 8192;
    if(setsockopt(AccptSocket->s, SOL_SOCKET, SO_RCVBUF, (char *) &bufsize, sizeof(bufsize)) == SOCKET_ERROR) {
    	int iError = WSAGetLastError();
    	int imsgLen = sprintf(msg, "[SYS] setsockopt failed on attempt to set SO_RCVBUF. IP: %s Err: %s (%d)",  sIP, WSErrorStr(iError), iError);
#else
    int bufsize = 8192;
    if(setsockopt(AccptSocket->s, SOL_SOCKET, SO_RCVBUF, &bufsize, sizeof(bufsize)) == -1) {
    	int imsgLen = sprintf(msg, "[SYS] setsockopt failed on attempt to set SO_RCVBUF. IP: %s Err: %d", sIP, errno);
#endif
        if(CheckSprintf(imsgLen, 1024, "clsServiceLoop::AcceptUser1") == true) {
    	   clsUdpDebug::mPtr->Broadcast(msg, imsgLen);
        }
#ifdef _WIN32
    	shutdown(AccptSocket->s, SD_SEND);
        closesocket(AccptSocket->s);
#else
    	shutdown(AccptSocket->s, SHUT_RDWR);
        close(AccptSocket->s);
#endif
        return;
    }

    // set the send buffer
    bufsize = 32768;
#ifdef _WIN32
    if(setsockopt(AccptSocket->s, SOL_SOCKET, SO_SNDBUF, (char *) &bufsize, sizeof(bufsize)) == SOCKET_ERROR) {
        int iError = WSAGetLastError();
        int imsgLen = sprintf(msg, "[SYS] setsockopt failed on attempt to set SO_SNDBUF. IP: %s Err: %s (%d)", sIP, WSErrorStr(iError), iError);
#else
    if(setsockopt(AccptSocket->s, SOL_SOCKET, SO_SNDBUF, &bufsize, sizeof(bufsize)) == -1) {
        int imsgLen = sprintf(msg, "[SYS] setsockopt failed on attempt to set SO_SNDBUF. IP: %s Err: %d", sIP, errno);
#endif
        if(CheckSprintf(imsgLen, 1024, "clsServiceLoop::AcceptUser2") == true) {
	       clsUdpDebug::mPtr->Broadcast(msg, imsgLen);
        }
#ifdef _WIN32
	    shutdown(AccptSocket->s, SD_SEND);
        closesocket(AccptSocket->s);
#else
	    shutdown(AccptSocket->s, SHUT_RDWR);
        close(AccptSocket->s);
#endif
        return;
    }

    // set sending of keepalive packets
#ifdef _WIN32
    bool bKeepalive = true;
    setsockopt(AccptSocket->s, SOL_SOCKET, SO_KEEPALIVE, (char *)&bKeepalive, sizeof(bKeepalive));
#else
    int iKeepAlive = 1;
    if(setsockopt(AccptSocket->s, SOL_SOCKET, SO_KEEPALIVE, &iKeepAlive, sizeof(iKeepAlive)) == -1) {
        int imsgLen = sprintf(msg, "[SYS] setsockopt failed on attempt to set SO_KEEPALIVE. IP: %s Err: %d", sIP, errno);
        if(CheckSprintf(imsgLen, 1024, "clsServiceLoop::AcceptUser3") == true) {
	       clsUdpDebug::mPtr->Broadcast(msg, imsgLen);
        }
	    shutdown(AccptSocket->s, SHUT_RDWR);
        close(AccptSocket->s);
        return;
    }
#endif

    // set non-blocking mode
#ifdef _WIN32
    uint32_t block = 1;
	if(ioctlsocket(AccptSocket->s, FIONBIO, (unsigned long *)&block) == SOCKET_ERROR) {
    	int iError = WSAGetLastError();
    	int imsgLen = sprintf(msg, "[SYS] ioctlsocket failed on attempt to set FIONBIO. IP: %s Err: %s (%d)", sIP, WSErrorStr(iError), iError);
#else
    int oldFlag = fcntl(AccptSocket->s, F_GETFL, 0);
    if(fcntl(AccptSocket->s, F_SETFL, oldFlag | O_NONBLOCK) == -1) {
    	int imsgLen = sprintf(msg, "[SYS] fcntl failed on attempt to set O_NONBLOCK. IP: %s Err: %d", sIP, errno);
#endif
        if(CheckSprintf(imsgLen, 1024, "clsServiceLoop::AcceptUser3") == true) {
		  clsUdpDebug::mPtr->Broadcast(msg, imsgLen);
		}
#ifdef _WIN32
		shutdown(AccptSocket->s, SD_SEND);
		closesocket(AccptSocket->s);
#else
		shutdown(AccptSocket->s, SHUT_RDWR);
		close(AccptSocket->s);
#endif
		return;
    }
    
    if(clsSettingManager::mPtr->bBools[SETBOOL_REDIRECT_ALL] == true) {
        if(clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_REDIRECT_ADDRESS] != NULL) {
       	    int imsgLen = sprintf(msg, "<%s> %s %s|%s", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_YOU_REDIR_TO],
               clsSettingManager::mPtr->sTexts[SETTXT_REDIRECT_ADDRESS], clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_REDIRECT_ADDRESS]);
            if(CheckSprintf(imsgLen, 1024, "clsServiceLoop::AcceptUser4") == true) {
                send(AccptSocket->s, msg, imsgLen, 0);
                clsServerManager::ui64BytesSent += imsgLen;
            }
        }
#ifdef _WIN32
        shutdown(AccptSocket->s, SD_SEND);
        closesocket(AccptSocket->s);
#else
        shutdown(AccptSocket->s, SHUT_RDWR);
        close(AccptSocket->s);
#endif
		return;
    }

    time_t acc_time;
    time(&acc_time);

	BanItem * Ban = clsBanManager::mPtr->FindFull(ui128IpHash, acc_time);

	if(Ban != NULL) {
        if(((Ban->ui8Bits & clsBanManager::FULL) == clsBanManager::FULL) == true) {
            int imsglen;
            char *messg = GenerateBanMessage(Ban, imsglen, acc_time);
            send(AccptSocket->s, messg, imsglen, 0);
#ifdef _WIN32
            shutdown(AccptSocket->s, SD_SEND);
            closesocket(AccptSocket->s);
#else
            shutdown(AccptSocket->s, SHUT_RD);
            close(AccptSocket->s);
#endif
/*            int imsgLen = sprintf(msg, "[SYS] Banned ip %s - connection closed.", sIP);
            if(CheckSprintf(imsgLen, 1024, "clsServiceLoop::AcceptUser5") == true) {
                clsUdpDebug::mPtr->Broadcast(msg, imsgLen);
            }*/
            return;
        }
    }

	RangeBanItem * RangeBan = clsBanManager::mPtr->FindFullRange(ui128IpHash, acc_time);

	if(RangeBan != NULL) {
        if(((RangeBan->ui8Bits & clsBanManager::FULL) == clsBanManager::FULL) == true) {
            int imsglen;
            char *messg = GenerateRangeBanMessage(RangeBan, imsglen, acc_time);
            send(AccptSocket->s, messg, imsglen, 0);
#ifdef _WIN32
            shutdown(AccptSocket->s, SD_SEND);
            closesocket(AccptSocket->s);
#else
            shutdown(AccptSocket->s, SHUT_RD);
            close(AccptSocket->s);
#endif
/*            int imsgLen = sprintf(msg, "[SYS] Range Banned ip %s - connection closed.", sIP);
            if(CheckSprintf(imsgLen, 1024, "clsServiceLoop::AcceptUser6") == true) {
                clsUdpDebug::mPtr->Broadcast(msg, imsgLen);
            }*/
            return;
        }
    }

    clsServerManager::ui32Joins++;

    // set properties of the new user object
	User * pUser = new User();

	if(pUser == NULL) {
#ifdef _WIN32
		shutdown(AccptSocket->s, SD_SEND);
		closesocket(AccptSocket->s);
#else
		shutdown(AccptSocket->s, SHUT_RDWR);
		close(AccptSocket->s);
#endif
        AppendDebugLog("%s - [MEM] Cannot allocate pUser in clsServiceLoop::AcceptUser\n", 0);
		return;
	}

	pUser->uLogInOut = new LoginLogout();

    if(pUser->uLogInOut == NULL) {
#ifdef _WIN32
		shutdown(AccptSocket->s, SD_SEND);
		closesocket(AccptSocket->s);
#else
		shutdown(AccptSocket->s, SHUT_RDWR);
		close(AccptSocket->s);
#endif
        delete pUser;

        AppendDebugLog("%s - [MEM] Cannot allocate uLogInOut in clsServiceLoop::AcceptUser\n", 0);
		return;
    }

    pUser->uLogInOut->logonClk = clsServerManager::ui64ActualTick;
    pUser->Sck = AccptSocket->s;
    pUser->ui8State = User::STATE_KEY_OR_SUP;

    memcpy(pUser->ui128IpHash, ui128IpHash, 16);
    pUser->ui16IpTableIdx = ui16IpTableIdx;

    pUser->SetIP(sIP);

    if(bIPv6 == true) {
        pUser->ui32BoolBits |= User::BIT_IPV6;
    } else {
        pUser->ui32BoolBits |= User::BIT_IPV4;
    }

    if(pUser->MakeLock() == false) {
#ifdef _WIN32
		shutdown(AccptSocket->s, SD_SEND);
		closesocket(AccptSocket->s);
#else
		shutdown(AccptSocket->s, SHUT_RDWR);
		close(AccptSocket->s);
#endif
        delete pUser;
        return;
    }
    
    if(Ban != NULL) {
        uint32_t hash = 0;
        if(((Ban->ui8Bits & clsBanManager::NICK) == clsBanManager::NICK) == true) {
            hash = Ban->ui32NickHash;
        }
        int imsglen;
        char *messg = GenerateBanMessage(Ban, imsglen, acc_time);
        pUser->uLogInOut->uBan = UserBan::CreateUserBan(messg, imsglen, hash);
        if(pUser->uLogInOut->uBan == NULL) {
#ifdef _WIN32
            shutdown(AccptSocket->s, SD_SEND);
            closesocket(AccptSocket->s);
#else
            shutdown(AccptSocket->s, SHUT_RDWR);
            close(AccptSocket->s);
#endif

            AppendDebugLog("%s - [MEM] Cannot allocate new uBan in clsServiceLoop::AcceptUser\n", 0);

            delete pUser;

            return;
        }
    } else if(RangeBan != NULL) {
        int imsglen;
        char *messg = GenerateRangeBanMessage(RangeBan, imsglen, acc_time);
        pUser->uLogInOut->uBan = UserBan::CreateUserBan(messg, imsglen, 0);
        if(pUser->uLogInOut->uBan == NULL) {
#ifdef _WIN32
            shutdown(AccptSocket->s, SD_SEND);
            closesocket(AccptSocket->s);
#else
            shutdown(AccptSocket->s, SHUT_RDWR);
            close(AccptSocket->s);
#endif

        	AppendDebugLog("%s - [MEM] Cannot allocate new uBan in clsServiceLoop::AcceptUser1\n", 0);

            delete pUser;

        	return;
        }
    }
        
    // Everything is ok, now add to users...
    clsUsers::mPtr->AddUser(pUser);
}
//---------------------------------------------------------------------------

void clsServiceLoop::ReceiveLoop() {
    // Receiving loop for process all incoming data and store in queues
    uint32_t iRecvRests = 0;
    
    clsServerManager::ui8SrCntr++;
    
    if(clsServerManager::ui8SrCntr >= 7 || (clsUsers::mPtr->ui16ActSearchs + clsUsers::mPtr->ui16PasSearchs) > 8 || 
        clsUsers::mPtr->ui16ActSearchs > 5) {
        clsServerManager::ui8SrCntr = 0;
    }

    if(clsServerManager::ui64ActualTick-iLstUptmTck > 60) {
        time_t acctime;
        time(&acctime);
        acctime -= clsServerManager::tStartTime;
    
        uint64_t iValue = acctime / 86400;
		acctime -= (time_t)(86400*iValue);
        clsServerManager::ui64Days = iValue;
    
        iValue = acctime / 3600;
    	acctime -= (time_t)(3600*iValue);
        clsServerManager::ui64Hours = iValue;
    
    	iValue = acctime / 60;
        clsServerManager::ui64Mins = iValue;

        if(clsServerManager::ui64Mins == 0 || clsServerManager::ui64Mins == 15 || clsServerManager::ui64Mins == 30 || clsServerManager::ui64Mins == 45) {
            clsRegManager::mPtr->Save(false, true);
#ifdef _WIN32
            if(HeapValidate(GetProcessHeap(), 0, 0) == 0) {
                AppendDebugLog("%s - [ERR] Process memory heap corrupted\n", 0);
            }
            HeapCompact(GetProcessHeap(), 0);

            if(HeapValidate(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE, 0) == 0) {
                AppendDebugLog("%s - [ERR] PtokaX memory heap corrupted\n", 0);
            }
            HeapCompact(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE);

            if(HeapValidate(clsServerManager::hRecvHeap, HEAP_NO_SERIALIZE, 0) == 0) {
                AppendDebugLog("%s - [ERR] Recv memory heap corrupted\n", 0);
            }
            HeapCompact(clsServerManager::hRecvHeap, HEAP_NO_SERIALIZE);

            if(HeapValidate(clsServerManager::hSendHeap, HEAP_NO_SERIALIZE, 0) == 0) {
                AppendDebugLog("%s - [ERR] Send memory heap corrupted\n", 0);
            }
            HeapCompact(clsServerManager::hSendHeap, HEAP_NO_SERIALIZE);

            if(HeapValidate(clsServerManager::hLuaHeap, 0, 0) == 0) {
                AppendDebugLog("%s - [ERR] Lua memory heap corrupted\n", 0);
            }
            HeapCompact(clsServerManager::hLuaHeap, 0);
#endif
        }

        iLstUptmTck = clsServerManager::ui64ActualTick;
    }

    AcceptedSocket *NextSck = NULL;

#ifdef _WIN32
    EnterCriticalSection(&csAcceptQueue);
#else
	pthread_mutex_lock(&mtxAcceptQueue);
#endif

    if(AcceptedSocketsS != NULL) {
        NextSck = AcceptedSocketsS;
        AcceptedSocketsS = NULL;
        AcceptedSocketsE = NULL;
    }

#ifdef _WIN32
    LeaveCriticalSection(&csAcceptQueue);
#else
	pthread_mutex_unlock(&mtxAcceptQueue);
#endif

    while(NextSck != NULL) {
        AcceptedSocket *CurSck = NextSck;
        NextSck = CurSck->next;
        AcceptUser(CurSck);
        delete CurSck;
    }

    User *nextUser = clsUsers::mPtr->llist;
    while(nextUser != 0 && clsServerManager::bServerTerminated == false) {
        User *curUser = nextUser;
        nextUser = curUser->next;

        // PPK ... true == we have rest ;)
        if(curUser->DoRecv() == true) {
            iRecvRests++;
            //Memo("Rest " + string(curUser->Nick, curUser->NickLen) + ": '" + string(curUser->recvbuf) + "'");
        }

        //    curUser->ProcessLines();
        //}

        switch(curUser->ui8State) {
            case User::STATE_KEY_OR_SUP:{
                // check logon timeout for iState 1
                if(clsServerManager::ui64ActualTick - curUser->uLogInOut->logonClk > 20) {
                    int imsgLen = sprintf(msg, "[SYS] Login timeout 1 for %s - user disconnected.", curUser->sIP);
                    if(CheckSprintf(imsgLen, 1024, "clsServiceLoop::ReceiveLoop3") == true) {
                        clsUdpDebug::mPtr->Broadcast(msg, imsgLen);
                    }
                    curUser->Close();
                    continue;
                }
                break;
            }
            case User::STATE_IPV4_CHECK: {
                // check IPv4Check timeout
                if((clsServerManager::ui64ActualTick - curUser->uLogInOut->ui64IPv4CheckTick) > 10) {
                    int imsgLen = sprintf(msg, "[SYS] IPv4Check timeout for %s (%s).", curUser->sNick, curUser->sIP);
                    if(CheckSprintf(imsgLen, 1024, "clsServiceLoop::ReceiveLoop31") == true) {
                        clsUdpDebug::mPtr->Broadcast(msg, imsgLen);
                    }
                    curUser->ui8State = User::STATE_ADDME;
                    continue;
                }
                break;
            }
            case User::STATE_ADDME: {
                // PPK ... Add user, but only if send $GetNickList (or have quicklist supports) <- important, used by flooders !!!
                if(((curUser->ui32BoolBits & User::BIT_GETNICKLIST) == User::BIT_GETNICKLIST) == false &&
                    ((curUser->ui32SupportBits & User::SUPPORTBIT_QUICKLIST) == User::SUPPORTBIT_QUICKLIST) == false &&
                    ((curUser->ui32BoolBits & User::BIT_PINGER) == User::BIT_PINGER) == true)
                    continue;

                int imsgLen = GetWlcmMsg(msg);
                curUser->SendCharDelayed(msg, imsgLen);
                curUser->ui8State = User::STATE_ADDME_1LOOP;
                continue;
            }
            case User::STATE_ADDME_1LOOP: {
                // PPK ... added login delay.
                if(dLoggedUsers >= dActualSrvLoopLogins && ((curUser->ui32BoolBits & User::BIT_OPERATOR) == User::BIT_OPERATOR) == false) {
                    if(clsServerManager::ui64ActualTick - curUser->uLogInOut->logonClk > 300) {
                        int imsgLen = sprintf(msg, "[SYS] Login timeout (%d) 3 for %s (%s) - user disconnected.", (int)curUser->ui8State, curUser->sNick, curUser->sIP);
                        if(CheckSprintf(imsgLen, 1024, "clsServiceLoop::ReceiveLoop4") == true) {
                            clsUdpDebug::mPtr->Broadcast(msg, imsgLen);
                        }
                        curUser->Close();
                    }
                    continue;
                }

                // PPK ... is not more needed, free mem ;)
                curUser->FreeBuffer();

                if((curUser->ui32BoolBits & User::BIT_IPV6) == User::BIT_IPV6 && ((curUser->ui32BoolBits & User::SUPPORTBIT_IPV4) == User::SUPPORTBIT_IPV4) == false) {
                    in_addr ipv4addr;
					ipv4addr.s_addr = INADDR_NONE;

                    if(curUser->ui128IpHash[0] == 32 && curUser->ui128IpHash[1] == 2) { // 6to4 tunnel
                        memcpy(&ipv4addr, curUser->ui128IpHash + 2, 4);
                    } else if(curUser->ui128IpHash[0] == 32 && curUser->ui128IpHash[1] == 1 && curUser->ui128IpHash[2] == 0 && curUser->ui128IpHash[3] == 0) { // teredo tunnel
                        uint32_t ui32Ip = 0;
                        memcpy(&ui32Ip, curUser->ui128IpHash + 12, 4);
                        ui32Ip ^= 0xffffffff;
                        memcpy(&ipv4addr, &ui32Ip, 4);
                    }

                    if(ipv4addr.s_addr != INADDR_NONE) {
                        strcpy(curUser->sIPv4, inet_ntoa(ipv4addr));
                        curUser->ui8IPv4Len = (uint8_t)strlen(curUser->sIPv4);
                        curUser->ui32BoolBits |= User::BIT_IPV4;
                    }
                }

                //New User Connected ... the user is operator ? invoke lua User/OpConnected
                uint32_t iBeforeLuaLen = curUser->sbdatalen;

				bool bRet = clsScriptManager::mPtr->UserConnected(curUser);
				if(curUser->ui8State >= User::STATE_CLOSING) {// connection closed by script?
                    if(bRet == false) { // only when all scripts process userconnected
                        clsScriptManager::mPtr->UserDisconnected(curUser);
                    }

					continue;
				}

                if(iBeforeLuaLen < curUser->sbdatalen) {
                    size_t szNeededLen = curUser->sbdatalen-iBeforeLuaLen;

					void * sOldBuf = curUser->uLogInOut->pBuffer;
#ifdef _WIN32
					if(curUser->uLogInOut->pBuffer == NULL) {
						curUser->uLogInOut->pBuffer = (char *)HeapAlloc(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE, szNeededLen+1);
					} else {
						curUser->uLogInOut->pBuffer = (char *)HeapReAlloc(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE, sOldBuf, szNeededLen+1);
					}
#else
					curUser->uLogInOut->pBuffer = (char *)realloc(sOldBuf, szNeededLen+1);
#endif
                    if(curUser->uLogInOut->pBuffer == NULL) {
                        curUser->ui32BoolBits |= User::BIT_ERROR;
                        curUser->Close();

						AppendDebugLog("%s - [MEM] Cannot allocate %" PRIu64 " bytes for sLockUsrConn in clsServiceLoop::ReceiveLoop\n", (uint64_t)(szNeededLen+1));

                		continue;
                    }
                    memcpy(curUser->uLogInOut->pBuffer, curUser->sendbuf+iBeforeLuaLen, szNeededLen);
                	curUser->uLogInOut->iUserConnectedLen = (uint32_t)szNeededLen;
                	curUser->uLogInOut->pBuffer[curUser->uLogInOut->iUserConnectedLen] = '\0';
                	curUser->sbdatalen = iBeforeLuaLen;
                	curUser->sendbuf[curUser->sbdatalen] = '\0';
                }

                // PPK ... wow user is accepted, now add it :)
                if(((curUser->ui32BoolBits & User::BIT_HAVE_BADTAG) == User::BIT_HAVE_BADTAG) == true) {
                    curUser->HasSuspiciousTag();
                }
                    
                curUser->Add2Userlist();
                
                dLoggedUsers++;
                curUser->ui8State = User::STATE_ADDME_2LOOP;
                clsServerManager::ui64TotalShare += curUser->ui64SharedSize;
                curUser->ui32BoolBits |= User::BIT_HAVE_SHARECOUNTED;
                
#ifdef _BUILD_GUI
                if(::SendMessage(clsMainWindowPageUsersChat::mPtr->hWndPageItems[clsMainWindowPageUsersChat::BTN_AUTO_UPDATE_USERLIST], BM_GETCHECK, 0, 0) == BST_CHECKED) {
                    clsMainWindowPageUsersChat::mPtr->AddUser(curUser);
                }
#endif
//                if(sqldb) sqldb->AddVisit(curUser);

                // PPK ... change to NoHello supports
            	int imsgLen = sprintf(msg, "$Hello %s|", curUser->sNick);
            	if(CheckSprintf(imsgLen, 1024, "clsServiceLoop::ReceiveLoop6") == true) {
                    clsGlobalDataQueue::mPtr->AddQueueItem(msg, imsgLen, NULL, 0, clsGlobalDataQueue::CMD_HELLO);
                }

                clsGlobalDataQueue::mPtr->UserIPStore(curUser);

                switch(clsSettingManager::mPtr->ui8FullMyINFOOption) {
                    case 0:
                        clsGlobalDataQueue::mPtr->AddQueueItem(curUser->sMyInfoLong, curUser->ui16MyInfoLongLen, NULL, 0, clsGlobalDataQueue::CMD_MYINFO);
                        break;
                    case 1:
                        clsGlobalDataQueue::mPtr->AddQueueItem(curUser->sMyInfoShort, curUser->ui16MyInfoShortLen, curUser->sMyInfoLong, curUser->ui16MyInfoLongLen, clsGlobalDataQueue::CMD_MYINFO);
                        break;
                    case 2:
                        clsGlobalDataQueue::mPtr->AddQueueItem(curUser->sMyInfoShort, curUser->ui16MyInfoShortLen, NULL, 0, clsGlobalDataQueue::CMD_MYINFO);
                        break;
                    default:
                        break;
                }
                
                if(((curUser->ui32BoolBits & User::BIT_OPERATOR) == User::BIT_OPERATOR) == true) {
                    clsGlobalDataQueue::mPtr->OpListStore(curUser->sNick);
                }
                
                curUser->iLastMyINFOSendTick = clsServerManager::ui64ActualTick;
                break;
            }
            case User::STATE_ADDED:
                if(curUser->cmdToUserStrt != NULL) {
                    PrcsdToUsrCmd *next = curUser->cmdToUserStrt;
                        
                    curUser->cmdToUserStrt = NULL;
                    curUser->cmdToUserEnd = NULL;
            
                    while(next != NULL) {
                        PrcsdToUsrCmd *cur = next;
                        next = cur->next;
                                               
                        if(cur->iLoops >= 2) {
                            User * ToUser = clsHashManager::mPtr->FindUser(cur->ToNick, cur->iToNickLen);
                            if(ToUser == cur->To) {
                                if(clsSettingManager::mPtr->iShorts[SETSHORT_MAX_PM_COUNT_TO_USER] == 0 || cur->iPmCount == 0) {
                                    cur->To->SendCharDelayed(cur->sCommand, cur->iLen);
                                } else {
                                    if(cur->To->iReceivedPmCount == 0) {
                                        cur->To->iReceivedPmTick = clsServerManager::ui64ActualTick;
                                    } else if(cur->To->iReceivedPmCount >= (uint32_t)clsSettingManager::mPtr->iShorts[SETSHORT_MAX_PM_COUNT_TO_USER]) {
                                        if(cur->To->iReceivedPmTick+60 < clsServerManager::ui64ActualTick) {
                                            cur->To->iReceivedPmTick = clsServerManager::ui64ActualTick;
                                            cur->To->iReceivedPmCount = 0;
                                        } else {
                                            bool bSprintfCheck;
                                            int imsgLen;
                                            if(cur->iPmCount == 1) {
                                                imsgLen = sprintf(msg, "$To: %s From: %s $<%s> %s %s %s!|", curUser->sNick, cur->ToNick, clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                                                    clsLanguageManager::mPtr->sTexts[LAN_SRY_LST_MSG_BCS], cur->ToNick, clsLanguageManager::mPtr->sTexts[LAN_EXC_MSG_LIMIT]);
                                                bSprintfCheck = CheckSprintf(imsgLen, 1024, "clsServiceLoop::ReceiveLoop1");
                                            } else {
                                                imsgLen = sprintf(msg, "$To: %s From: %s $<%s> %s %d %s %s %s!|", curUser->sNick, cur->ToNick,
                                                    clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_SORRY_LAST], cur->iPmCount,
                                                    clsLanguageManager::mPtr->sTexts[LAN_MSGS_NOT_SENT], cur->ToNick, clsLanguageManager::mPtr->sTexts[LAN_EXC_MSG_LIMIT]);
                                                bSprintfCheck = CheckSprintf(imsgLen, 1024, "clsServiceLoop::ReceiveLoop2");
                                            }
                                            if(bSprintfCheck == true) {
                                                curUser->SendCharDelayed(msg, imsgLen);
                                            }

#ifdef _WIN32
                                            if(HeapFree(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE, (void *)cur->sCommand) == 0) {
												AppendDebugLog("%s - [MEM] Cannot deallocate cur->sCommand in clsServiceLoop::ReceiveLoop\n", 0);
                                            }
#else
											free(cur->sCommand);
#endif
                                            cur->sCommand = NULL;

#ifdef _WIN32
                                            if(HeapFree(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE, (void *)cur->ToNick) == 0) {
												AppendDebugLog("%s - [MEM] Cannot deallocate cur->ToNick in clsServiceLoop::ReceiveLoop\n", 0);
                                            }
#else
											free(cur->ToNick);
#endif
                                            cur->ToNick = NULL;
        
                                            delete cur;

                                            continue;
                                        }
                                    }  
                                    cur->To->SendCharDelayed(cur->sCommand, cur->iLen);
                                    cur->To->iReceivedPmCount += cur->iPmCount;
                                }
                            }
                        } else {
                            cur->iLoops++;
                            if(curUser->cmdToUserStrt == NULL) {
                                cur->next = NULL;
                                curUser->cmdToUserStrt = cur;
                                curUser->cmdToUserEnd = cur;
                            } else {
                                curUser->cmdToUserEnd->next = cur;
                                curUser->cmdToUserEnd = cur;
                            }
                            continue;
                        }

#ifdef _WIN32
                        if(HeapFree(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE, (void *)cur->sCommand) == 0) {
							AppendDebugLog("%s - [MEM] Cannot deallocate cur->sCommand1 in clsServiceLoop::ReceiveLoop\n", 0);
                        }
#else
						free(cur->sCommand);
#endif
                        cur->sCommand = NULL;

#ifdef _WIN32
                        if(HeapFree(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE, (void *)cur->ToNick) == 0) {
							AppendDebugLog("%s - [MEM] Cannot deallocate cur->ToNick1 in clsServiceLoop::ReceiveLoop\n", 0);
                        }
#else
						free(cur->ToNick);
#endif
                        cur->ToNick = NULL;
        
                        delete cur;
					}
                }
        
                if(clsServerManager::ui8SrCntr == 0) {
                    if(curUser->cmdActive6Search != NULL) {
						if(curUser->cmdActive4Search != NULL) {
							clsGlobalDataQueue::mPtr->AddQueueItem(curUser->cmdActive6Search->sCommand, curUser->cmdActive6Search->iLen, 
								curUser->cmdActive4Search->sCommand, curUser->cmdActive4Search->iLen, clsGlobalDataQueue::CMD_ACTIVE_SEARCH_V64);
						} else {
							clsGlobalDataQueue::mPtr->AddQueueItem(curUser->cmdActive6Search->sCommand, curUser->cmdActive6Search->iLen, NULL, 0, clsGlobalDataQueue::CMD_ACTIVE_SEARCH_V6);
						}
                    } else if(curUser->cmdActive4Search != NULL) {
						clsGlobalDataQueue::mPtr->AddQueueItem(curUser->cmdActive4Search->sCommand, curUser->cmdActive4Search->iLen, NULL, 0, clsGlobalDataQueue::CMD_ACTIVE_SEARCH_V4);
					}

                    if(curUser->cmdPassiveSearch != NULL) {
						uint8_t ui8CmdType = clsGlobalDataQueue::CMD_PASSIVE_SEARCH_V4;
						if((curUser->ui32BoolBits & User::BIT_IPV6) == User::BIT_IPV6) {
							if((curUser->ui32BoolBits & User::BIT_IPV4) == User::BIT_IPV4) {
                                if((curUser->ui32BoolBits & User::BIT_IPV6_ACTIVE) == User::BIT_IPV6_ACTIVE) {
                                    ui8CmdType = clsGlobalDataQueue::CMD_PASSIVE_SEARCH_V4_ONLY;
                                } else if((curUser->ui32BoolBits & User::BIT_IPV4_ACTIVE) == User::BIT_IPV4_ACTIVE) {
                                    ui8CmdType = clsGlobalDataQueue::CMD_PASSIVE_SEARCH_V6_ONLY;
                                } else {
                                    ui8CmdType = clsGlobalDataQueue::CMD_PASSIVE_SEARCH_V64;
                                }
                            } else {
								ui8CmdType = clsGlobalDataQueue::CMD_PASSIVE_SEARCH_V6;
                            }
						}

						clsGlobalDataQueue::mPtr->AddQueueItem(curUser->cmdPassiveSearch->sCommand, curUser->cmdPassiveSearch->iLen, NULL, 0, ui8CmdType);

                        User::DeletePrcsdUsrCmd(curUser->cmdPassiveSearch);
                        curUser->cmdPassiveSearch = NULL;
                    }
                }

                // PPK ... deflood memory cleanup, if is not needed anymore
				if(clsServerManager::ui8SrCntr == 0) {
					if(curUser->sLastChat != NULL && curUser->ui16LastChatLines < 2 && 
                        (curUser->ui64SameChatsTick+clsSettingManager::mPtr->iShorts[SETSHORT_SAME_MAIN_CHAT_TIME]) < clsServerManager::ui64ActualTick) {
#ifdef _WIN32
                        if(HeapFree(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE, (void *)curUser->sLastChat) == 0) {
                            AppendDebugLog("%s - [MEM] Cannot deallocate curUser->sLastChat in clsServiceLoop::ReceiveLoop\n", 0);
                        }
#else
						free(curUser->sLastChat);
#endif
                        curUser->sLastChat = NULL;
						curUser->ui16LastChatLen = 0;
						curUser->ui16SameMultiChats = 0;
						curUser->ui16LastChatLines = 0;
                    }

					if(curUser->sLastPM != NULL && curUser->ui16LastPmLines < 2 && 
                        (curUser->ui64SamePMsTick+clsSettingManager::mPtr->iShorts[SETSHORT_SAME_PM_TIME]) < clsServerManager::ui64ActualTick) {
#ifdef _WIN32
						if(HeapFree(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE, (void *)curUser->sLastPM) == 0) {
							AppendDebugLog("%s - [MEM] Cannot deallocate curUser->sLastPM in clsServiceLoop::ReceiveLoop\n", 0);
                        }
#else
						free(curUser->sLastPM);
#endif
                        curUser->sLastPM = NULL;
                        curUser->ui16LastPMLen = 0;
						curUser->ui16SameMultiPms = 0;
                        curUser->ui16LastPmLines = 0;
                    }
                    
					if(curUser->sLastSearch != NULL && (curUser->ui64SameSearchsTick+clsSettingManager::mPtr->iShorts[SETSHORT_SAME_SEARCH_TIME]) < clsServerManager::ui64ActualTick) {
#ifdef _WIN32
                        if(HeapFree(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE, (void *)curUser->sLastSearch) == 0) {
							AppendDebugLog("%s - [MEM] Cannot deallocate curUser->sLastSearch in clsServiceLoop::ReceiveLoop\n", 0);
                        }
#else
						free(curUser->sLastSearch);
#endif
                        curUser->sLastSearch = NULL;
                        curUser->ui16LastSearchLen = 0;
                    }
                }
                continue;
            case User::STATE_CLOSING: {
                if(((curUser->ui32BoolBits & User::BIT_ERROR) == User::BIT_ERROR) == false && curUser->sbdatalen != 0) {
                  	if(curUser->uLogInOut->iToCloseLoops != 0 || ((curUser->ui32BoolBits & User::BIT_PINGER) == User::BIT_PINGER) == true) {
                		curUser->Try2Send();
                		curUser->uLogInOut->iToCloseLoops--;
                		continue;
                	}
                }
            	curUser->ui8State = User::STATE_REMME;
            	continue;
            }
            // if user is marked as dead, remove him
            case User::STATE_REMME: {
#ifdef _WIN32
                shutdown(curUser->Sck, SD_SEND);
                closesocket(curUser->Sck);
#else
                shutdown(curUser->Sck, SHUT_RD);
                close(curUser->Sck);
#endif

                // linked list
                clsUsers::mPtr->RemUser(curUser);

                delete curUser;
                continue;
            }
            default: {
                // check logon timeout
                if(clsServerManager::ui64ActualTick - curUser->uLogInOut->logonClk > 60) {
                    int imsgLen = sprintf(msg, "[SYS] Login timeout (%d) 2 for %s (%s) - user disconnected.", (int)curUser->ui8State, curUser->sNick, curUser->sIP);
                    if(CheckSprintf(imsgLen, 1024, "clsServiceLoop::ReceiveLoop7") == true) {
                        clsUdpDebug::mPtr->Broadcast(msg, imsgLen);
                    }
                    curUser->Close();
                    continue;
                }
                break;
            }
        }
    }
    
    if(clsServerManager::ui8SrCntr == 0) {
        clsUsers::mPtr->ui16ActSearchs = 0;
        clsUsers::mPtr->ui16PasSearchs = 0;
    }

    iLastRecvRest = iRecvRests;
    iRecvRestsPeak = iRecvRests > iRecvRestsPeak ? iRecvRests : iRecvRestsPeak;
}
//---------------------------------------------------------------------------

void clsServiceLoop::SendLoop() {
    clsGlobalDataQueue::mPtr->PrepareQueueItems();

    // PPK ... send loop
    // now logging users get changed myinfo with myinfos
    // old users get it in this loop from queue -> badwith saving !!! no more twice myinfo =)
    // Sending Loop
    uint32_t iSendRests = 0;

    User *nextUser = clsUsers::mPtr->llist;
    while(nextUser != 0 && clsServerManager::bServerTerminated == false) {
        User *curUser = nextUser;
        nextUser = curUser->next;

        switch(curUser->ui8State) {
            case User::STATE_ADDME_2LOOP: {
                clsServerManager::ui32Logged++;

                if(clsServerManager::ui32Peak < clsServerManager::ui32Logged) {
                    clsServerManager::ui32Peak = clsServerManager::ui32Logged;
                    if(clsSettingManager::mPtr->iShorts[SETSHORT_MAX_USERS_PEAK] < (int16_t)clsServerManager::ui32Peak)
                        clsSettingManager::mPtr->SetShort(SETSHORT_MAX_USERS_PEAK, (int16_t)clsServerManager::ui32Peak);
                }

            	curUser->ui8State = User::STATE_ADDED;

            	// finaly send the nicklist/myinfos/oplist
                curUser->AddUserList();
                
                // PPK ... UserIP2 supports
                if(((curUser->ui32SupportBits & User::SUPPORTBIT_USERIP2) == User::SUPPORTBIT_USERIP2) == true &&
                    ((curUser->ui32BoolBits & User::BIT_QUACK_SUPPORTS) == User::BIT_QUACK_SUPPORTS) == false &&
                    clsProfileManager::mPtr->IsAllowed(curUser, clsProfileManager::SENDALLUSERIP) == false) {
            		int imsgLen = sprintf(msg, "$UserIP %s %s|", curUser->sNick, (curUser->sIPv4[0] == '\0' ? curUser->sIP : curUser->sIPv4));
            		if(CheckSprintf(imsgLen, 1024, "clsServiceLoop::SendLoop1") == true) {
                        curUser->SendCharDelayed(msg, imsgLen);
                    }
                }
                
                curUser->ui32BoolBits &= ~User::BIT_GETNICKLIST;

                // PPK ... send motd ???
                if(clsSettingManager::mPtr->ui16PreTextsLens[clsSettingManager::SETPRETXT_MOTD] != 0) {
                    if(clsSettingManager::mPtr->bBools[SETBOOL_MOTD_AS_PM] == true) {
                        int imsgLen = sprintf(clsServerManager::sGlobalBuffer, clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_MOTD], curUser->sNick);
                        if(CheckSprintf(imsgLen, clsServerManager::szGlobalBufferSize, "clsServiceLoop::SendLoop2") == true) {
                            curUser->SendCharDelayed(clsServerManager::sGlobalBuffer, imsgLen);
                        }
                    } else {
                        curUser->SendCharDelayed(clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_MOTD],
                            clsSettingManager::mPtr->ui16PreTextsLens[clsSettingManager::SETPRETXT_MOTD]);
                    }
                }

                // check for Debug subscription
                if(((curUser->ui32BoolBits & User::BIT_OPERATOR) == User::BIT_OPERATOR) == true)
                    clsUdpDebug::mPtr->CheckUdpSub(curUser, true);

                if(curUser->uLogInOut->iUserConnectedLen != 0) {
                    curUser->PutInSendBuf(curUser->uLogInOut->pBuffer, curUser->uLogInOut->iUserConnectedLen);

                    curUser->FreeBuffer();
                }

                // Login struct no more needed, free mem ! ;)
                delete curUser->uLogInOut;
                curUser->uLogInOut = NULL;

            	// PPK ... send all login data from buffer !
            	if(curUser->sbdatalen > 0) {
                    curUser->Try2Send();
                }
                    
                // apply one loop delay to avoid double sending of hello and oplist
            	continue;
            }
            case User::STATE_ADDED: {
                if(((curUser->ui32BoolBits & User::BIT_GETNICKLIST) == User::BIT_GETNICKLIST) == true) {
                    curUser->AddUserList();
                    curUser->ui32BoolBits &= ~User::BIT_GETNICKLIST;
                }

                // process global data queues
                if(clsGlobalDataQueue::mPtr->pQueueItems != NULL) {
                    clsGlobalDataQueue::mPtr->ProcessQueues(curUser);
                }
                
            	if(clsGlobalDataQueue::mPtr->pSingleItems != NULL) {
                    clsGlobalDataQueue::mPtr->ProcessSingleItems(curUser);
            	}

                // send data acumulated by queues above
                // if sending caused error, close the user
                if(curUser->sbdatalen != 0) {
                    // PPK ... true = we have rest ;)
                	if(curUser->Try2Send() == true) {
                	    iSendRests++;
                	}
                }
                break;
            }
            case User::STATE_CLOSING:
            case User::STATE_REMME:
                continue;
            default:
                if(curUser->sbdatalen > 0) {
                    curUser->Try2Send();
                }
                break;
        }
    }

    clsGlobalDataQueue::mPtr->ClearQueues();

    if(iLoopsForLogins >= 40) {
        dLoggedUsers = 0;
        iLoopsForLogins = 0;
        dActualSrvLoopLogins = 0;
    }

    dActualSrvLoopLogins += (double)clsSettingManager::mPtr->iShorts[SETSHORT_MAX_SIMULTANEOUS_LOGINS]/40;
    iLoopsForLogins++;

    iLastSendRest = iSendRests;
    iSendRestsPeak = iSendRests > iSendRestsPeak ? iSendRests : iSendRestsPeak;
}
//---------------------------------------------------------------------------

#ifdef _WIN32
void clsServiceLoop::AcceptSocket(const SOCKET &s, const sockaddr_storage &addr) {
#else
void clsServiceLoop::AcceptSocket(const int &s, const sockaddr_storage &addr) {
#endif
    AcceptedSocket * pNewSocket = new AcceptedSocket();
    if(pNewSocket == NULL) {
#ifdef _WIN32
		shutdown(s, SD_SEND);
		closesocket(s);
#else
		shutdown(s, SHUT_RDWR);
		close(s);
#endif

		AppendDebugLog("%s - [MEM] Cannot allocate pNewSocket in clsServiceLoop::AcceptSocket\n", 0);
    	return;
    }

    pNewSocket->s = s;

    memcpy(&pNewSocket->addr, &addr, sizeof(sockaddr_storage));

    pNewSocket->next = NULL;

#ifdef _WIN32
    EnterCriticalSection(&csAcceptQueue);
#else
    pthread_mutex_lock(&mtxAcceptQueue);
#endif

    if(AcceptedSocketsS == NULL) {
        AcceptedSocketsS = pNewSocket;
        AcceptedSocketsE = pNewSocket;
    } else {
        AcceptedSocketsE->next = pNewSocket;
        AcceptedSocketsE = pNewSocket;
    }

#ifdef _WIN32
    LeaveCriticalSection(&csAcceptQueue);
#else
	pthread_mutex_unlock(&mtxAcceptQueue);
#endif
}
//---------------------------------------------------------------------------
