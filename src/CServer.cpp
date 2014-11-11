#include "CServer.h"

#include "Addresses.h"
#include "CPlayerData.h"
#include "CCallbackManager.h"
#include "Functions.h"
#include "RPCs.h"
#include "Utils.h"
#include "main.h"

#ifndef _WIN32
	#include <cstring>
#endif
#include <stdio.h>

CServer::CServer(eSAMPVersion version)
{
	m_fGravity = 0.008f;
	m_byteWeather = 10;
	m_iTicks = 0;

	memset(pPlayerData, NULL, MAX_PLAYERS);
	
	// Initialize addresses
	CAddress::Initialize(version);
	// Initialize SAMP Function
	CSAMPFunctions::Initialize();
}

CServer::~CServer()
{
	for(WORD i = 0; i != MAX_PLAYERS; i++)
		RemovePlayer(i);
}

bool CServer::AddPlayer(int playerid)
{
	if(!pPlayerData[playerid])
	{
		pPlayerData[playerid] = new CPlayerData(playerid);
		return 1;
	}
	return 0;
}

bool CServer::RemovePlayer(int playerid)
{
	if(pPlayerData[playerid])
	{
		delete pPlayerData[playerid];
		pPlayerData[playerid] = NULL;
		return 1;
	}
	return 0;
}

void CServer::Process()
{
	if(++m_iTicks == 5)
	{
		m_iTicks = 0;
		for(WORD playerid = 0; playerid != MAX_PLAYERS; playerid++)
		{
			if(!IsPlayerConnected(playerid)) continue;
			
			// Process player
			pPlayerData[playerid]->Process();
		}

		if(pNetGame)
		{
			pNetGame->pPickupPool->Process();
		}
	}
}

bool CServer::OnPlayerStreamIn(WORD playerid, WORD forplayerid)
{
	//logprintf("join stream zone playerid = %d, forplayerid = %d", playerid, forplayerid);
	PlayerID playerId = pRakServer->GetPlayerIDFromIndex(playerid);
	PlayerID forplayerId = pRakServer->GetPlayerIDFromIndex(forplayerid);
	
	// For security..
	if (playerId.binaryAddress == UNASSIGNED_PLAYER_ID.binaryAddress || forplayerId.binaryAddress == UNASSIGNED_PLAYER_ID.binaryAddress)
		return 0;

	if(!IsPlayerConnected(playerid) || !IsPlayerConnected(forplayerid))
		return 0;

	CObjectPool *pObjectPool = pNetGame->pObjectPool;
	for(WORD i = 0; i != MAX_OBJECTS; i++)
	{
		if(pPlayerData[forplayerid]->stObj[i].usAttachPlayerID == playerid)
		{
			// logprintf("should work");
			if(!pObjectPool->m_pPlayerObjects[forplayerid][i]) 
			{
				//logprintf("YSF ASSERTATION FAILED <OnPlayerStreamIn> - m_pPlayerObjects = 0");
				return 0;
			}

			//logprintf("attach objects i: %d, forplayerid: %d", i, forplayerid);
			// First create the object for the player. We don't remove it from the pools, so we need to send RPC for the client to create object
			RakNet::BitStream bs2;
			bs2.Write(pObjectPool->m_pPlayerObjects[forplayerid][i]->wObjectID); // m_wObjectID
			bs2.Write(pObjectPool->m_pPlayerObjects[forplayerid][i]->iModel);  // iModel

			bs2.Write(pPlayerData[forplayerid]->stObj[i].vecOffset.fX);
			bs2.Write(pPlayerData[forplayerid]->stObj[i].vecOffset.fY);
			bs2.Write(pPlayerData[forplayerid]->stObj[i].vecOffset.fZ);

			bs2.Write(pPlayerData[forplayerid]->stObj[i].vecRot.fX);
			bs2.Write(pPlayerData[forplayerid]->stObj[i].vecRot.fY);
			bs2.Write(pPlayerData[forplayerid]->stObj[i].vecRot.fZ);
			bs2.Write(300.0f); // fDrawDistance

			pRakServer->RPC(&RPC_CreateObject, &bs2, HIGH_PRIORITY, RELIABLE_ORDERED, 2, forplayerId, 0, 0);

			// Attach created object to player
			RakNet::BitStream bs;
			bs.Write(pObjectPool->m_pPlayerObjects[forplayerid][i]->wObjectID); // m_wObjectID
			bs.Write(pPlayerData[forplayerid]->stObj[i].usAttachPlayerID); // playerid

			bs.Write(pPlayerData[forplayerid]->stObj[i].vecOffset.fX);
			bs.Write(pPlayerData[forplayerid]->stObj[i].vecOffset.fY);
			bs.Write(pPlayerData[forplayerid]->stObj[i].vecOffset.fZ);

			bs.Write(pPlayerData[forplayerid]->stObj[i].vecRot.fX);
			bs.Write(pPlayerData[forplayerid]->stObj[i].vecRot.fY);
			bs.Write(pPlayerData[forplayerid]->stObj[i].vecRot.fZ);

			pRakServer->RPC(&RPC_AttachObject, &bs, HIGH_PRIORITY, RELIABLE_ORDERED, 2, forplayerId, 0, 0);
			/*
			logprintf("join, modelid: %d, %d, %f, %f, %f, %f, %f, %f",pObjectPool->m_pPlayerObjects[forplayerid][i]->m_iModel,
				gAOData[forplayerid][i].AttachPlayerID,
				gAOData[forplayerid][i].vecOffset.fX, gAOData[forplayerid][i].vecOffset.fY, gAOData[forplayerid][i].vecOffset.fZ,
				gAOData[forplayerid][i].vecRot.fX, gAOData[forplayerid][i].vecRot.fY, gAOData[forplayerid][i].vecRot.fZ);
			*/
		}
	}
	return 1;
}

bool CServer::OnPlayerStreamOut(WORD playerid, WORD forplayerid)
{
	//logprintf("leave stream zone playerid = %d, forplayerid = %d", playerid, forplayerid);
	PlayerID playerId = pRakServer->GetPlayerIDFromIndex(playerid);
	PlayerID forplayerId = pRakServer->GetPlayerIDFromIndex(forplayerid);
	
	if (playerId.binaryAddress == UNASSIGNED_PLAYER_ID.binaryAddress || forplayerId.binaryAddress == UNASSIGNED_PLAYER_ID.binaryAddress)
		return 0;

	if(!IsPlayerConnected(playerid) || !IsPlayerConnected(forplayerid))
		return 0;

	CObjectPool *pObjectPool = pNetGame->pObjectPool;
	for(WORD i = 0; i != MAX_OBJECTS; i++)
	{
		if(pPlayerData[forplayerid]->stObj[i].usAttachPlayerID == playerid)
		{
			if(!pObjectPool->m_pPlayerObjects[forplayerid][i]) 
			{
				//logprintf("YSF ASSERTATION FAILED <OnPlayerStreamOut> - m_pPlayerObjects = 0");
				return 0;
			}

			//logprintf("remove objects i: %d, forplayerid: %d", i, forplayerid);
			pPlayerData[playerid]->DestroyObject(i);

			/*
			logprintf("leave %d, %f, %f, %f, %f, %f, %f", gAOData[forplayerid][i].AttachPlayerID,
				gAOData[forplayerid][i].vecOffset.fX, gAOData[forplayerid][i].vecOffset.fY, gAOData[forplayerid][i].vecOffset.fZ,
				gAOData[forplayerid][i].vecRot.fX, gAOData[forplayerid][i].vecRot.fY, gAOData[forplayerid][i].vecRot.fZ);
			*/
		}
	}
	return 1;
}

void CServer::SetGravity_(float fGravity)
{
	// Update console
	char szGravity[16];
	sprintf(szGravity, "%f", fGravity);
	CSAMPFunctions::SetStringVariable("gravity", szGravity);

	// Minden j�t�kos gravit�ci�ja �t�ll�t�sa arra, amire a szerver gravit�ci�j�t be�ll�tottuk
	for(WORD i = 0; i != MAX_PLAYERS; i++)
	{
		if(IsPlayerConnected(i))
			pPlayerData[i]->fGravity = fGravity; 
	}
	
	RakNet::BitStream bs;
	bs.Write(fGravity);
	pRakServer->RPC(&RPC_Gravity, &bs, HIGH_PRIORITY, RELIABLE_ORDERED, 2, UNASSIGNED_PLAYER_ID, true, 0);
}

float CServer::GetGravity_(void)
{
	return m_fGravity;
}

void CServer::SetWeather_(BYTE byteWeather)
{
	// Update console
	char szWeather[8];
	sprintf(szWeather, "%d", byteWeather);
	CSAMPFunctions::SetStringVariable("weather", szWeather);

	// Minden j�t�kos id�j�r�sa �t�ll�t�sa arra, amire a szerver id�j�r�st be�ll�tottuk
	for (WORD i = 0; i != MAX_PLAYERS; i++)
	{
		if (IsPlayerConnected(i))
			pPlayerData[i]->byteWeather = byteWeather;
	}

	RakNet::BitStream bs;
	bs.Write(byteWeather);
	pRakServer->RPC(&RPC_Weather, &bs, HIGH_PRIORITY, RELIABLE_ORDERED, 2, UNASSIGNED_PLAYER_ID, true, 0);
}

BYTE CServer::GetWeather_(void)
{
	return m_byteWeather;
}

void CServer::AllowNickNameCharacter(char character, bool enable)
{
	if (enable)
	{
		// If vector already doesn't contain item, then add it
		if (!Contains(m_vecValidNameCharacters, character))
		{
			m_vecValidNameCharacters.push_back(character);
		}
	}
	else
	{
		std::vector<char>::iterator it = std::find(m_vecValidNameCharacters.begin(), m_vecValidNameCharacters.end(), character);
		if (it != m_vecValidNameCharacters.end())
		{
			m_vecValidNameCharacters.erase(it);
		}
	}
}

bool CServer::IsNickNameCharacterAllowed(char character)
{
	return Contains(m_vecValidNameCharacters, character);
}

bool CServer::IsValidNick(char *szName)
{
	while (*szName)
	{
		if ((*szName >= '0' && *szName <= '9') || (*szName >= 'A' && *szName <= 'Z') || (*szName >= 'a' && *szName <= 'z') ||
			*szName == ']' || *szName == '[' || *szName == '_' || *szName == '$' || *szName == ':' || *szName == '=' ||
			*szName == '(' || *szName == ')' || *szName == '@' || *szName == '.')
		{

			szName++;
		}
		else
		{
			if (IsNickNameCharacterAllowed(*szName))
			{
				szName++;
			}
			else
			{
				return false;
			}
		}
	}
	return true;
}

//----------------------------------------------------

void CServer::Packet_StatsUpdate(Packet *p)
{
	RakNet::BitStream bsStats((unsigned char*)p->data, p->length, false);
	CPlayerPool *pPlayerPool = pNetGame->pPlayerPool;
	WORD playerid = p->playerIndex;
	int money;
	int drunklevel;
//	BYTE bytePacketID;

	bsStats.SetReadOffset(8);
	bsStats.Read(money);
	bsStats.Read(drunklevel);

	if (!IsPlayerConnected(playerid)) return;
	
	pPlayerPool->dwMoney[playerid] = money;
	pPlayerPool->dwDrunkLevel[playerid] = drunklevel;

	CCallbackManager::OnPlayerStatsAndWeaponsUpdate(playerid);
}

//----------------------------------------------------

void CServer::Packet_WeaponsUpdate(Packet *p)
{
	RakNet::BitStream bsData((unsigned char*)p->data, p->length, false);
	
	WORD playerid = p->playerIndex;
	BYTE byteLength = p->length;
	if (byteLength > 3)
		byteLength = (byteLength - 3) >> 2;

	if (!IsPlayerConnected(playerid)) return;
	printf("Original: %d New: %d\n", p->length, byteLength);

	CPlayer* pPlayer = pNetGame->pPlayerPool->pPlayer[playerid];
	
	WORD wTarget;
	BYTE byteIndex;
	BYTE byteWeapon;
	WORD wordAmmo;

	bsData.SetReadOffset(8);
	bsData.Read(wTarget);
	pPlayer->wTargetId = wTarget;
	logprintf("targetid: %d, byteLenghth: %d", byteLength);

	while (byteLength)
	{
		logprintf("byteLength: %d", byteLength);
		bsData.Read(byteIndex);
		bsData.Read(byteWeapon);
		bsData.Read(wordAmmo);
		logprintf("%u %u %i", byteIndex, byteWeapon, wordAmmo);
		if (byteIndex < 13)
		{
			logprintf("%d %d %d", byteIndex, byteWeapon, wordAmmo);
			pPlayer->wWeaponAmmo[byteIndex] = wordAmmo;
			pPlayer->byteWeaponId[byteIndex] = byteWeapon;
		}
		byteLength--;
	}

	CCallbackManager::OnPlayerStatsAndWeaponsUpdate(playerid);
}