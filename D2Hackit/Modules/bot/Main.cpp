#include <fstream>
#include <vector>
#include <set>
#include <algorithm>
#include <queue>

#include "../../Includes/ClientCore.cpp"

enum States
{
	STATE_Idle,
	STATE_TeleportingToStairsRoom,
	STATE_TeleportingToStairsObject,
	STATE_UsingStairs,
	STATE_UsingWaypoint,
	STATE_BackToTown,
};

struct StairsData
{
	StairsData()
	{
		Id = 0;
		XOffset = 0;
		YOffset = 0;
	}
	StairsData(DWORD id, WORD xOffset, WORD yOffset)
	{
		Id = id;
		XOffset = xOffset;
		YOffset = yOffset;
	}

	DWORD Id;
	short XOffset;
	short YOffset;
};

struct BotTravelData
{
	void InitSteps(int numSteps)
	{
		RoomTransitions.resize(numSteps);
		NumSteps = numSteps;
		CurrentStep = -1;
	}

	void Reset()
	{
		CurrentStep = -1;
	}

	std::map<DWORD, StairsData> &Current()
	{
		return RoomTransitions[CurrentStep];
	}

	std::string Name;
	int WaypointDestination;
	std::vector<std::map<DWORD, StairsData>> RoomTransitions;
	unsigned int CurrentStep;
	unsigned int NumSteps;
};

bool CheckForOurStairs();
void NextStep();
BOOL CALLBACK enumUnitProc(LPCGAMEUNIT lpUnit, LPARAM lParam);

States currentState = STATE_Idle;

int stairsRoomNum = 0;

GAMEUNIT stairsGameUnit;
PATH path;
int pathIndex = 0;


BotTravelData toBloodRaven;
BotTravelData toBloodRavenLair;
BotTravelData toCountress;
BotTravelData toAndarial;

BotTravelData toMephisto;
BotTravelData toBaal;
BotTravelData toDeadEnd;

std::vector<BotTravelData *> allTravelData;
BotTravelData *currentTravelData = nullptr;

#include "../../Core/definitions.h"


BOOL PRIVATE Pos(char **argv, int argc)
{
	MAPPOS coords;
	RoomOther *room = (RoomOther*)server->D2GetRoomCoords(server->D2GetCurrentRoomNum(), &coords, nullptr);
	
	if(room == nullptr)
	{
		server->GameStringf("Failed to find room 1026");
		return TRUE;
	}

	if(room->pRoom == nullptr)
	{
		server->GameStringf("room->pRoom == nullptr");
		return TRUE;
	}

	MAPPOS myPos = me->GetPosition();

	
	server->GameStringf("Room %d: %d, %d", room->pPresetType2info->roomNum, room->pRoom->pColl->nPosRoomX, room->pRoom->pColl->nPosRoomY);

	//server->GameStringf("Player position %d, %d  room %d offset x: %d y: %d", (int)myPos.x, (int)myPos.y, server->D2GetCurrentRoomNum(), (int)myPos.x - (int)room->pRoom->pColl->nPosGameX, (int)myPos.y - (int)room->pRoom->pColl->nPosGameY);

	return TRUE;
}


BOOL CALLBACK enumUnitProc(LPCGAMEUNIT lpUnit, LPARAM lParam)
{

	DWORD unitClassId = server->GetUnitClassID(lpUnit);
	if(unitClassId == 0)
	{
		return TRUE;
	}

	//server->GameStringf("Unit class %d", unitClassId);
	
	if(currentTravelData->Current()[stairsRoomNum].Id != unitClassId)
	{
		return TRUE;
	}

	//UnitAny *unit = (UnitAny *)server->VerifyUnit(lpUnit);
	//if(unit == nullptr)
	//{
	//	server->GameStringf("unit == nullptr");
	//	return TRUE;
	//}
	//if(unit->hPath == nullptr)
	//{
	//	server->GameStringf("unit->hPath == nullptr");
	//	return TRUE;
	//}
	//if(unit->hOPath->ptRoom == nullptr)
	//{
	//	server->GameStringf("unit->hOPath->ptRoom == nullptr");
	//	return TRUE;
	//}
	//if(unit->hOPath->ptRoom->ptRoomOther == nullptr)
	//{
	//	server->GameStringf("unit->hOPath->ptRoom->ptRoomOther == nullptr");
	//	return TRUE;
	//}
	//if(unit->hOPath->ptRoom->ptRoomOther->pPresetType2info == nullptr)
	//{
	//	server->GameStringf("unit->hOPath->ptRoom->ptRoomOther->pPresetType2info == nullptr");
	//	return TRUE;
	//}
	//
	//int roomNum = unit->hOPath->ptRoom->ptRoomOther->pPresetType2info->roomNum;

	GAMEUNIT *previouslyFoundStairs = (GAMEUNIT *)lParam;
	if(previouslyFoundStairs->dwUnitID != 0 && previouslyFoundStairs->dwUnitType != 0)
	{
		MAPPOS previousStairsPos = server->GetUnitPosition(previouslyFoundStairs);
		MAPPOS thisStairsPos = server->GetUnitPosition(lpUnit);

		double distanceToPreviousStairs = me->GetDistanceFrom(previousStairsPos.x, previousStairsPos.y);
		double distanceToThisStairs = me->GetDistanceFrom(thisStairsPos.x, thisStairsPos.y);
		
		if(distanceToThisStairs < distanceToPreviousStairs)
		{
			*((GAMEUNIT*)lParam) = *lpUnit;
		}
	}
	else
	{
		*((GAMEUNIT*)lParam) = *lpUnit;
	}

	return TRUE;
}

struct GameUnitDistance
{
	GAMEUNIT unit;
	float distance;
};


bool CheckForOurStairs()
{
	//server->GameStringf("%s", __FUNCTION__);
	// A cave/stairs/entrance has come into range, check to see if it's our staircase
		
	stairsGameUnit.dwUnitID = 0;
	stairsGameUnit.dwUnitType = 0;

	server->EnumUnits(UNIT_TYPE_ROOMTILE, enumUnitProc, (LPARAM)&stairsGameUnit);
	if(stairsGameUnit.dwUnitID == 0 && stairsGameUnit.dwUnitType == 0)
	{
		return false;
	}

	MAPPOS stairsPos = server->GetUnitPosition(&stairsGameUnit);
	//server->GameStringf("Our stairs = %d", server->GetUnitClassID(&stairsGameUnit));

	currentState = STATE_TeleportingToStairsObject;

	pathIndex = 0;
	server->CalculatePath(stairsPos.x, stairsPos.y, &path, 5);
		
	return true;
}


void NextStep()
{
	//server->GameStringf("%s", __FUNCTION__);

	if(pathIndex >= path.iNodeCount)
	{
		if(currentState == STATE_TeleportingToStairsObject)
		{
			//server->GameStringf("We have arrived at our stairs [class %X]", server->GetUnitClassID(&stairsGameUnit));

			MAPPOS stairsPosition = server->GetUnitPosition(&stairsGameUnit);
			currentState = STATE_UsingStairs;
			me->Interact(&stairsGameUnit);
			return;
		}

		server->GameStringf("We have arrived at our room, checking for stairs...");
		if(!CheckForOurStairs())
		{
			currentState = STATE_Idle;
			server->GameStringf("Failed to find stairs, aborting!");
			return;
		}

		// Fall through...
	}
		

	me->CastOnMap(D2S_TELEPORT, path.aPathNodes[pathIndex].x, path.aPathNodes[pathIndex].y, false);
	++pathIndex;
}

void NextRoomTransition()
{
	//server->GameStringf("%s", __FUNCTION__);

	pathIndex = 0;
	memset(&path, 0, sizeof(PATH));
	memset(&stairsGameUnit, 0, sizeof(GAMEUNIT));
	currentState = STATE_Idle;

	if(currentTravelData->CurrentStep + 1 < currentTravelData->NumSteps)
	{
		currentTravelData->CurrentStep++;
	}

	ROOMPOS allRoomCoords[256];
	DWORD numRooms = server->D2GetAllRoomCoords(allRoomCoords, ARRAYSIZE(allRoomCoords));
	if(numRooms == 0)
	{
		server->GameStringf("Failed to find all rooms");
		return;
	}


	MAPPOS stairRoomCoord;
	std::map<DWORD, StairsData>::iterator foundStairs;
	std::map<DWORD, StairsData> &currentStairsData = currentTravelData->Current();

	for(unsigned int i = 0; i < numRooms; ++i)
	{
		foundStairs = currentStairsData.find(allRoomCoords[i].roomnum);
		if(foundStairs != currentStairsData.end())
		{
			stairsRoomNum = allRoomCoords[i].roomnum;
			stairRoomCoord = allRoomCoords[i].pos;
			break;
		}
	}

	if(foundStairs == currentStairsData.end())
	{
		server->GameStringf("No stairs found");
		return;
	}

	//server->GameStringf("stairRoomCoord %d, %d",stairRoomCoord.x, stairRoomCoord.y);

	stairRoomCoord.x = stairRoomCoord.x*5 + foundStairs->second.XOffset;
	stairRoomCoord.y = stairRoomCoord.y*5 + foundStairs->second.YOffset;
	
	//server->GameStringf("stairRoomCoord MOD %d, %d",stairRoomCoord.x, stairRoomCoord.y);


	if(server->CalculatePath(stairRoomCoord.x, stairRoomCoord.y, &path, 5) == 0)
	{
		server->GameStringf("Failed to find spot :(");
		return;
	}

	//for(int i =0 ;i <= path.iNodeCount; ++i)
	//{
	//	server->GameStringf("%d, %d", path.aPathNodes[i].x, path.aPathNodes[i].y);
	//}
	
	//server->GameStringf("Teleporing to room %d", stairsRoomNum);
	currentState = STATE_TeleportingToStairsRoom;
	NextStep();
}

void ShowUsage()
{
	server->GameStringf("Usage: .bot start #");
	for(unsigned int i = 0; i < allTravelData.size(); ++i)
	{
		server->GameStringf("  %d: %s", i+1, allTravelData[i]->Name.c_str());
	}
}

BOOL PRIVATE Start(char** argv, int argc)
{
	server->GameStringf("%s", __FUNCTION__);

	if(argc != 3)
	{
		ShowUsage();
		return TRUE;
	}

	unsigned int travelIndex = (unsigned int)atoi(argv[2]);
	if(travelIndex == 0 || travelIndex > allTravelData.size())
	{
		ShowUsage();
		return TRUE;
	}

	currentTravelData = allTravelData[travelIndex - 1];
	currentTravelData->Reset();

	if(!me->IsInTown())
	{
		currentState = STATE_BackToTown;
		server->GameCommandf("load flee");
		server->GameCommandf("flee tp");
		return TRUE;
	}
	
	currentState = STATE_UsingWaypoint;
	server->GameCommandf("load wp");
	server->GameCommandf("wp start %d", currentTravelData->WaypointDestination);

	return TRUE;
}


////////////////////////////////////////////
//
//               EXPORTS
//
/////////////////////////////////////////////

VOID EXPORT OnGameJoin(THISGAMESTRUCT* thisgame)
{
	server->GameStringf("%s", __FUNCTION__);
	currentState = STATE_Idle;
}

VOID EXPORT OnGameLeave(THISGAMESTRUCT* thisgame)
{
	currentState = STATE_Idle;
}

BOOL EXPORT OnClientStart()
{
	currentTravelData = &toCountress;

	toDeadEnd.Name = "Dead End";
	toDeadEnd.InitSteps(6);
	toDeadEnd.WaypointDestination = WAYPOINTDEST_TheAncientsWay;
	toDeadEnd.RoomTransitions[0][1026] = StairsData(75, 13, 29); //	Act 5 - Ice Down W
	toDeadEnd.RoomTransitions[0][1027] = StairsData(75, 21, 24); //	Act 5 - Ice Down E
	toDeadEnd.RoomTransitions[0][1028] = StairsData(75, 36, 24); //	Act 5 - Ice Down S
	toDeadEnd.RoomTransitions[0][1029] = StairsData(75, 10, 39); //	Act 5 - Ice Down N
	for(int i = 1; i <= 3; ++i)
	{
		toDeadEnd.RoomTransitions[i][1018] = StairsData(73, 14, 7); //	Act 5 - Ice Prev W
		toDeadEnd.RoomTransitions[i][1019] = StairsData(73, 12, 19); //	Act 5 - Ice Prev E
		toDeadEnd.RoomTransitions[i][1020] = StairsData(73, 0, 0); //	Act 5 - Ice Prev S
		toDeadEnd.RoomTransitions[i][1021] = StairsData(73, 12, 25); //	Act 5 - Ice Prev N
	}

	allTravelData.push_back(&toDeadEnd);

	// BlackMarsh -> Countress
	toCountress.Name = "Countress";
	toCountress.InitSteps(3);
	toCountress.WaypointDestination = WAYPOINTDEST_BlackMarsh;
	toCountress.RoomTransitions[0][163] = StairsData(10, 14, 14); //	Act 1 Wilderness to Tower
	toCountress.RoomTransitions[1][164] = StairsData(12, 2, 12); //	Act 1 Tower to Crypt
	toCountress.RoomTransitions[2][143] = StairsData(9, 22, 14); //	Crypt Next W DOWN
	toCountress.RoomTransitions[2][144] = StairsData(9, 2, 21); //	Crypt Next E DOWN
	toCountress.RoomTransitions[2][145] = StairsData(9, 7, 19); //	Crypt Next S DOWN
	toCountress.RoomTransitions[2][146] = StairsData(9, 7, 20); //	Crypt Next N DOWN
	allTravelData.push_back(&toCountress);

	toAndarial.Name = "Andarial";
	toAndarial.InitSteps(1);
	toAndarial.WaypointDestination = WAYPOINTDEST_CataCombsLevel2;
	toAndarial.RoomTransitions[0][291] = StairsData(18, 0, 0); // Act 1 - Catacombs Next W	
	toAndarial.RoomTransitions[0][292] = StairsData(18, 24, 29); // Act 1 - Catacombs Next E	
	toAndarial.RoomTransitions[0][293] = StairsData(18, 29, 19); // Act 1 - Catacombs Next S	
	toAndarial.RoomTransitions[0][294] = StairsData(18, 29, 38); // Act 1 - Catacombs Next N	
	allTravelData.push_back(&toAndarial);

	toBloodRaven.Name = "Blood raven 1";
	toBloodRaven.InitSteps(1);
	toBloodRaven.WaypointDestination = WAYPOINTDEST_OuterCloister;
	toBloodRaven.RoomTransitions[0][51] = StairsData(3, 15, 20); // Act 1 - Cave Entrance
	allTravelData.push_back(&toBloodRaven);

	toBloodRavenLair.Name = "Blood raven 2";
	toBloodRavenLair.InitSteps(1);
	toBloodRavenLair.WaypointDestination = WAYPOINTDEST_OuterCloister;
	toBloodRavenLair.RoomTransitions[0][51] = StairsData(3, 15, 20); // Act 1 - Cave Entrance
	toBloodRavenLair.RoomTransitions[0][91] = StairsData(5, 0, 0); // Act 1 - Cave Down W	
	toBloodRavenLair.RoomTransitions[0][92] = StairsData(5, 0, 0); // Act 1 - Cave Down E	
	toBloodRavenLair.RoomTransitions[0][93] = StairsData(5, 5, 34); // Act 1 - Cave Down S	
	toBloodRavenLair.RoomTransitions[0][94] = StairsData(5, 16, 14); // Act 1 - Cave Down N	
	allTravelData.push_back(&toBloodRavenLair);

	toMephisto.Name = "Mephisto";
	toMephisto.InitSteps(1);
	toMephisto.WaypointDestination = WAYPOINTDEST_DuranceOfHateLevel2;
	toMephisto.RoomTransitions[0][788] = StairsData(67, 17, 12); // Act 3 - Mephisto Next W
	toMephisto.RoomTransitions[0][789] = StairsData(67, 12, 29); // Act 3 - Mephisto Next E
	toMephisto.RoomTransitions[0][790] = StairsData(67, 29, 12); // Act 3 - Mephisto Next S
	toMephisto.RoomTransitions[0][791] = StairsData(67, 7, 28); // Act 3 - Mephisto Next N
	allTravelData.push_back(&toMephisto);

	toBaal.Name = "Throne of destruction";
	toBaal.InitSteps(1);
	toBaal.WaypointDestination = WAYPOINTDEST_WorldstoneKeepLevel2;
	toBaal.RoomTransitions[0][1078] = StairsData(82, 17, 18); // Act 3 - Mephisto Next W
	toBaal.RoomTransitions[0][1079] = StairsData(82, 17, 19); // Act 3 - Mephisto Next E
	toBaal.RoomTransitions[0][1080] = StairsData(82, 18, 17); // Act 3 - Mephisto Next S
	toBaal.RoomTransitions[0][1081] = StairsData(82, 19, 17); // Act 3 - Mephisto Next N
	allTravelData.push_back(&toBaal);

	return TRUE;
}

DWORD EXPORT OnGamePacketBeforeSent(BYTE* aPacket, DWORD aLen)
{
	//if(aPacket[0] == 0x13)
	//{
	//	DWORD unitType = *((DWORD *)&aPacket[1]);
	//	DWORD unitId = *((DWORD *)&aPacket[5]);
	//
	//	GAMEUNIT gu;
	//	gu.dwUnitID = unitId;
	//	gu.dwUnitType = unitType;
	//
	//	DWORD classId = server->GetUnitClassID(&gu);
	//	server->GameStringf("Interact with: transitions[%d] = %d;", server->D2GetCurrentRoomNum(), classId);
	//}

	if(aPacket[0] == 0x15 && aPacket[1] == 0x01)
	{
		char *chatMessage = (char *)(aPacket+3);

		if(strncmp(chatMessage, "�c5Waypoint�c0:", 15) == 0)
		{
			if(strcmp(chatMessage, "�c5Waypoint�c0: Complete") != 0)
			{
				currentState = STATE_Idle;
			}

			return 0;
		}
	}

	return aLen;
}


VOID EXPORT OnGamePacketAfterReceived(BYTE* aPacket, DWORD aLen)
{
	//if(currentState == STATE_TeleportingToStairsRoom && aPacket[0] == 0x09 && aPacket[1] == 0x05)
	//{
	//	CheckForOurStairs();
	//}

	if(aPacket[0] == 0x15 && *((DWORD *)&aPacket[2]) == me->GetID())
	{
		if(currentState != STATE_TeleportingToStairsRoom && currentState != STATE_TeleportingToStairsObject)
		{
			return;
		}

		NextStep();
	}
	return;
}

VOID EXPORT OnThisPlayerMessage(UINT nMessage, WPARAM wParam, LPARAM lParam)
{
	if(nMessage == PM_MOVECOMPLETE)
	{
		//if(currentState == STATE_TeleportingToStairsObject)
		//{
		//	currentState = STATE_UsingStairs;
		//	me->Interact(&stairsGameUnit);
		//}
	}
	else if(nMessage == PM_MAPCHANGE)
	{
		//server->GameStringf("PM_MAPCHANGE");
		if(currentState == STATE_UsingStairs || currentState == STATE_UsingWaypoint)
		{
			currentState = STATE_Idle;
			NextRoomTransition();
		}
		else if(currentState == STATE_BackToTown)
		{
			currentState = STATE_UsingWaypoint;
			server->GameCommandf("load wp");
			server->GameCommandf("wp start %d", currentTravelData->WaypointDestination);
			return;
		}
	}
}

BYTE EXPORT OnGameKeyDown(BYTE iKeyCode)
{
	if(iKeyCode == VK_SPACE)
	{
		currentState = STATE_Idle;
	}
	return iKeyCode;
}


CLIENTINFO
(
	0,1,
	"",
	"",
	"bot",
	""
)

MODULECOMMANDSTRUCT ModuleCommands[]=
{
	{
		"help",
		OnGameCommandHelp,
		"help�c0 List commands available in this module.\n"
		"<command> help�c0 Shows detailed help for <command> in this module."
	},
	{
		"Pos",
		Pos,
		"Pos Pos"
	},
		{
		"Start",
		Start,
		"Start bot"
	},
	{NULL}
};
