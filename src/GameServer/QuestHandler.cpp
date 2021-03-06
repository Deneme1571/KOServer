#include "stdafx.h"
#include "KnightsManager.h"

void CUser::QuestDataRequest()
{
	Packet result(WIZ_QUEST, uint8(1));
	result << uint16(m_questMap.size());
	foreach (itr, m_questMap)
		result	<< itr->first << itr->second;
	Send(&result);
}

void CUser::QuestV2PacketProcess(Packet & pkt)
{
	uint8 opcode = pkt.read<uint8>();
	uint32 nQuestID = pkt.read<uint32>();

	CNpc *pNpc = g_pMain->GetNpcPtr(m_sEventNid);
	_QUEST_HELPER * pQuestHelper = g_pMain->m_QuestHelperArray.GetData(nQuestID);

	if(5 != opcode){ 
		if(pQuestHelper == nullptr
			|| pNpc == nullptr
			|| pNpc->isDead()
			|| GetZoneID() != pQuestHelper->bZone
			|| pQuestHelper->sNpcId != pNpc->GetProtoID()
			|| m_sEventNid < 0
			|| !isInRange(pNpc, MAX_NPC_RANGE))  
			return;
	}

	if (pQuestHelper == nullptr
		|| (pQuestHelper->bNation != 3 && pQuestHelper->bNation != GetNation())
		|| (pQuestHelper->bLevel > GetLevel())
		|| (pQuestHelper->bClass != 5 && !JobGroupCheck(pQuestHelper->bClass)))
			return;

	if (pQuestHelper->bLevel == GetLevel() && pQuestHelper->nExp > m_iExp)
		return;

	printf("quest opcode num:%d\n",opcode);
	switch (opcode) {
		case 3:
		case 7:
			QuestV2ExecuteHelper(pQuestHelper);
			QuestV2MonsterDataRequest();
			break;
		case 4:
			QuestV2CheckFulfill(pQuestHelper);
			break;
		case 5:
			if (!CheckExistEvent(pQuestHelper->sEventDataIndex, 2))
				SaveEvent(pQuestHelper->sEventDataIndex, 4);

			if (m_sEventDataIndex > 0 && m_sEventDataIndex == pQuestHelper->sEventDataIndex) {
				QuestV2MonsterDataDeleteAll();
				QuestV2MonsterDataRequest();
			}

			if (GetZoneID() >= 81 && GetZoneID() <= 83)
				KickOutZoneUser(true);
			break;
		case 6:
			if (!CheckExistEvent(pQuestHelper->sEventDataIndex, 2))
				SaveEvent(pQuestHelper->sEventDataIndex, 1);
			break;
		case 12:
			if (!CheckExistEvent(pQuestHelper->sEventDataIndex, 3))
				SaveEvent(pQuestHelper->sEventDataIndex, 1);
			break;
		default:
			printf("uncatched num:%d\n",opcode);
			break;
	}
}

void CUser::SaveEvent(uint16 sQuestID, uint8 bQuestState)
{
	_QUEST_MONSTER * pQuestMonster = g_pMain->m_QuestMonsterArray.GetData(sQuestID);

	if (pQuestMonster != nullptr && bQuestState == 1 && m_sEventDataIndex > 0)
		return;

	m_questMap[sQuestID] = bQuestState;

	if (sQuestID >= QUEST_KILL_GROUP1)
		return;

	Packet result(WIZ_QUEST, uint8(2));
	result << sQuestID << bQuestState;
	Send(&result);

	if (m_sEventDataIndex == sQuestID && bQuestState == 2) {
		QuestV2MonsterDataDeleteAll();
		QuestV2MonsterDataRequest();
	}

	if (bQuestState == 1 && pQuestMonster != nullptr) {
		int16 v11 = ((int16)((uint32)(6711 * sQuestID) >> 16) >> 10) - (sQuestID >> 15);
		int16 v12 = ((int16)((uint32)(5243 * (int16)(sQuestID - 10000 * v11)) >> 16) >> 3) - ((int16)(sQuestID - 10000 * v11) >> 15);

		SaveEvent(32005, (uint8)v11);
		SaveEvent(32006, (uint8)v12);
		SaveEvent(32007, sQuestID - 100 * v12);
		m_sEventDataIndex = sQuestID;
		QuestV2MonsterDataRequest();
	}
}

void CUser::DeleteEvent(uint16 sQuestID)
{
	m_questMap.erase(sQuestID);
	m_quest_moster_map.erase(sQuestID);
}

bool CUser::CheckExistEvent(uint16 sQuestID, uint8 bQuestState)
{
	QuestMap::iterator itr = m_questMap.find(sQuestID);
	if (itr == m_questMap.end())
		return bQuestState == 0;

	return itr->second == bQuestState;
}

void CUser::QuestV2MonsterCountAdd(uint16 sNpcID)
{
	if (m_sEventDataIndex == 0)
		return;

	uint16 sQuestNum = m_sEventDataIndex;
	_QUEST_MONSTER *pQuestMonster = g_pMain->m_QuestMonsterArray.GetData(sQuestNum);
	if (pQuestMonster == nullptr)
		return;

	for (int group = 0; group < QUEST_MOB_GROUPS; group++) {
		for (int i = 0; i < QUEST_MOBS_PER_GROUP; i++) {
			if (pQuestMonster->sNum[group][i] != sNpcID)
				continue;		

			m_bKillCounts[group]++;
			SaveEvent(QUEST_KILL_GROUP1 + group, m_bKillCounts[group]);

			Packet result(WIZ_QUEST, uint8(9));
			result << uint8(2) << uint16(sQuestNum) << uint8(1) << uint16(m_bKillCounts[group]);
			Send(&result);

			if (m_bKillCounts[group] >=  pQuestMonster->sCount[group]) {
				uint8 bQuestState = 3;
				SaveEvent(sQuestNum, bQuestState);
				Quest_Moster_Couner_Map::iterator itr  = m_quest_moster_map.find(sQuestNum);
				if(itr == m_quest_moster_map.end())
					m_quest_moster_map.insert(std::make_pair(sQuestNum,m_bKillCounts[group]));
				else
					itr->second = m_bKillCounts[group];
			}
			return;
		}
	}
}

uint8 CUser::QuestV2CheckMonsterCount(uint16 sQuestID)
{
	if (sQuestID >= QUEST_KILL_GROUP1) {
		QuestMap::iterator itr = m_questMap.find(sQuestID);
		if (itr == m_questMap.end())
			return 0;
		
		return itr->second;	
	} else {
		Quest_Moster_Couner_Map::iterator itr  = m_quest_moster_map.find(sQuestID);
		if(itr == m_quest_moster_map.end())
			return 0;

		return itr->second;
	}
}

void CUser::QuestV2MonsterDataDeleteAll()
{
	memset(&m_bKillCounts, 0, sizeof(m_bKillCounts));
	m_quest_moster_map.erase(m_sEventDataIndex);
	m_sEventDataIndex = 0;

	for (int i = QUEST_KILL_GROUP1; i <= 32007; i++)
		DeleteEvent(i);
}

void CUser::QuestV2MonsterDataRequest()
{
	Packet result(WIZ_QUEST, uint8(9));

	m_sEventDataIndex = 
		10000	*	QuestV2CheckMonsterCount(32005) +
		100		*	QuestV2CheckMonsterCount(32006) +
		QuestV2CheckMonsterCount(32007);

	m_bKillCounts[0] = QuestV2CheckMonsterCount(QUEST_KILL_GROUP1);
	m_bKillCounts[1] = QuestV2CheckMonsterCount(QUEST_KILL_GROUP2);
	m_bKillCounts[2] = QuestV2CheckMonsterCount(QUEST_KILL_GROUP3);
	m_bKillCounts[3] = QuestV2CheckMonsterCount(QUEST_KILL_GROUP4);

	result	<< uint8(1)
			<< m_sEventDataIndex
			<< m_bKillCounts[0] << m_bKillCounts[1]
			<< m_bKillCounts[2] << m_bKillCounts[3];

	Send(&result);
}

void CUser::QuestV2ExecuteHelper(_QUEST_HELPER * pQuestHelper)
{
	if (pQuestHelper == nullptr && pQuestHelper->bQuestType != 3)
		return;

	QuestV2RunEvent(pQuestHelper, pQuestHelper->nEventTriggerIndex);
}

void CUser::QuestV2CheckFulfill(_QUEST_HELPER * pQuestHelper)
{
	if (pQuestHelper == nullptr || !CheckExistEvent(pQuestHelper->sEventDataIndex, 1))
		return;

	QuestV2RunEvent(pQuestHelper, pQuestHelper->nEventCompleteIndex);
}

bool CUser::QuestV2RunEvent(_QUEST_HELPER * pQuestHelper, uint32 nEventID, int8 bSelectedReward /*= -1*/)
{
	if (pQuestHelper->strLuaFilename == "01_main.lua")
		m_sEventNid = 10000;

	CNpc * pNpc = g_pMain->GetNpcPtr(m_sEventNid);
	bool result = false;

	if (pNpc == nullptr || pNpc->isDead())
		return result;

	pNpc->IncRef();

	m_nQuestHelperID = pQuestHelper->nIndex;
	result = g_pMain->GetLuaEngine()->ExecuteScript(this, pNpc, nEventID, bSelectedReward, pQuestHelper->strLuaFilename.c_str());

	pNpc->DecRef();

	return result;
}

void CUser::QuestV2SaveEvent(uint16 sQuestID)
{
	_QUEST_HELPER * pQuestHelper = g_pMain->m_QuestHelperArray.GetData(sQuestID);
	if (pQuestHelper == nullptr)
		return;

	SaveEvent(pQuestHelper->sEventDataIndex, pQuestHelper->bEventStatus);
}

void CUser::QuestV2SendNpcMsg(uint32 nQuestID, uint16 sNpcID)
{
	Packet result(WIZ_QUEST, uint8(7));
	result << nQuestID << sNpcID;
	Send(&result);
}

void CUser::QuestV2ShowGiveItem(uint32 nUnk1, uint32 sUnk1, 
								uint32 nUnk2, uint32 sUnk2,
								uint32 nUnk3, uint32 sUnk3,
								uint32 nUnk4, uint32 sUnk4) {
	Packet result(WIZ_QUEST, uint8(10));
	result	<< nUnk1 << sUnk1
		<< nUnk2 << sUnk2
		<< nUnk3 << sUnk3
		<< nUnk4 << sUnk4;
	Send(&result);
}

uint16 CUser::QuestV2SearchEligibleQuest(uint16 sNpcID)
{
	Guard lock(g_pMain->m_questNpcLock);
	QuestNpcList::iterator itr = g_pMain->m_QuestNpcList.find(sNpcID);
	if (itr == g_pMain->m_QuestNpcList.end() || itr->second.empty())
		return 0;

	foreach (itr2, itr->second) {
		_QUEST_HELPER * pHelper = (*itr2);
		if (pHelper->bLevel > GetLevel()
			|| (pHelper->bLevel == GetLevel() && pHelper->nExp > m_iExp)
			|| (pHelper->bClass != 5 && !JobGroupCheck(pHelper->bClass))
			|| (pHelper->bNation != 3 && pHelper->bNation != GetNation())
			|| (pHelper->sEventDataIndex == 0)
			|| (pHelper->bEventStatus < 0 || CheckExistEvent(pHelper->sEventDataIndex, 2))
			|| !CheckExistEvent(pHelper->sEventDataIndex, pHelper->bEventStatus))
			continue;

		return 2;
	}
	return 0;
}

void CUser::QuestV2ShowMap(uint32 nQuestHelperID)
{
	Packet result(WIZ_QUEST, uint8(11));
	result << nQuestHelperID;
	Send(&result);
}

uint8 CUser::CheckMonsterCount(uint8 bGroup)
{
	_QUEST_MONSTER * pQuestMonster = g_pMain->m_QuestMonsterArray.GetData(m_sEventDataIndex);
	if (pQuestMonster == nullptr || bGroup == 0 || bGroup >= QUEST_MOB_GROUPS)
		return 0;

	return m_bKillCounts[bGroup];
}

// First job change; you're a [novice], Harry!
bool CUser::PromoteUserNovice()
{
	uint8 bNewClasses[] = { ClassWarriorNovice, ClassRogueNovice, ClassMageNovice, ClassPriestNovice };
	uint8 bOldClass = GetClassType() - 1; // convert base class 1,2,3,4 to 0,1,2,3 to align with bNewClasses

	// Make sure it's a beginner class.
	if (!isBeginner())
		return false;

	Packet result(WIZ_CLASS_CHANGE, uint8(6));

	// Build the new class.
	uint16 sNewClass = (GetNation() * 100) + bNewClasses[bOldClass];
	result << sNewClass << GetID();
	SendToRegion(&result);

	// Change the class & update party.
	result.clear();
	result << uint8(2) << sNewClass;
	ClassChange(result, false); // TODO: Clean this up. Shouldn't need to build a packet for this.

	// Update the clan.
	result.clear();
	result << uint16(0);
	CKnightsManager::CurrentKnightsMember(this, result); // TODO: Clean this up too.
	return true;
}

// From novice to master.
bool CUser::PromoteUser()
{
	/* unlike the official, the checks & item removal should be handled in the script, not here */
	uint8 bOldClass = GetClassType();

	// We must be a novice before we can be promoted to master.
	if (!isNovice()) 
		return false;

	Packet result(WIZ_CLASS_CHANGE, uint8(6));

	// Build the new class.
	uint16 sNewClass = (GetNation() * 100) + bOldClass + 1;
	result << sNewClass << GetID();
	SendToRegion(&result);

	// Change the class & update party.
	result.clear();
	result << uint8(2) << sNewClass;
	ClassChange(result, false); // TODO: Clean this up. Shouldn't need to build a packet for this.

	// use integer division to get from 5/7/9/11 (novice classes) to 1/2/3/4 (base classes)
	uint8 bBaseClass = (bOldClass / 2) - 1; 

	// this should probably be moved to the script
	SaveEvent(bBaseClass, 2); 

	// Update the clan.
	result.clear();
	result << uint16(0);
	CKnightsManager::CurrentKnightsMember(this, result); // TODO: Clean this up too.
	return true;
}

void CUser::PromoteClan(ClanTypeFlag byFlag)
{
	if (!isInClan())
		return;

	CKnightsManager::UpdateKnightsGrade(GetClanID(), byFlag);
}

void CUser::SendClanPointChange(int32 nChangeAmount)
{
	if (!isInClan())
		return;

	CKnightsManager::UpdateClanPoint(GetClanID(), nChangeAmount);
}

uint8 CUser::GetClanGrade()
{
	if (!isInClan())
		return 0;

	CKnights * pClan = g_pMain->GetClanPtr(GetClanID());
	if (pClan == nullptr)
		return 0;

	return pClan->m_byGrade;
}

uint32 CUser::GetClanPoint()
{
	if (!isInClan())
		return 0;

	CKnights * pClan = g_pMain->GetClanPtr(GetClanID());
	if (pClan == nullptr)
		return 0;

	return pClan->m_nClanPointFund;
}

uint8 CUser::GetClanRank()
{
	if (!isInClan())
		return ClanTypeNone;

	CKnights * pClan = g_pMain->GetClanPtr(GetClanID());
	if (pClan == nullptr)
		return ClanTypeNone;

	return pClan->m_byFlag;
}

uint8 CUser::GetBeefRoastVictory() {
	if( g_pMain->m_sBifrostTime <= 90 * MINUTE && g_pMain->m_BifrostVictory != ALL )
		return g_pMain->m_sBifrostVictoryAll; 
	else
		return g_pMain->m_BifrostVictory; 
}

uint8 CUser::GetWarVictory() { return g_pMain->m_bVictory; }

uint8 CUser::CheckMiddleStatueCapture() { return g_pMain->m_bMiddleStatueNation == GetNation() ? 1 : 0; }

void CUser::MoveMiddleStatue() { Warp((GetNation() == KARUS ? DODO_CAMP_WARP_X : LAON_CAMP_WARP_X) + myrand(0, DODO_LAON_WARP_RADIUS),(GetNation() == KARUS ? DODO_CAMP_WARP_Z : LAON_CAMP_WARP_Z) + myrand(0, DODO_LAON_WARP_RADIUS)); }

uint8 CUser::GetPVPMonumentNation() { return g_pMain->m_nPVPMonumentNation[GetZoneID()]; }
uint8 CUser::GetEventMonumentNation() { return g_pMain->m_nEventMonumentNation[GetZoneID()]; }
