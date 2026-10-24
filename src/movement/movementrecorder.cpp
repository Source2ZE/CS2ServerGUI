/**
 * =============================================================================
 * CS2ServerGUI
 * Copyright (C) 2024 Poggu
 * =============================================================================
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License, version 3.0, as published by the
 * Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "movementrecorder.h"
#include "jsonwriter.h"
#include "extension.h"
#include "interfaces.h"
#include "utils/module.h"
#include <schemasystem/schemasystem.h>
#include <entity2/entitysystem.h>
#include "gameentitysystem.h"
#include "engine2/gameshareddefs.h"
#include "cs_usercmd.pb.h"

#include <atomic>
#include <bitset>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <memory>
#include <mutex>
#include <unordered_map>

#include <Windows.h>
#include <intrin.h>

namespace Movement::Recorder
{

// ---------------------------------------------------------------------------
// Functions in server.dll, found by signature
// ---------------------------------------------------------------------------

enum FunctionId
{
	Fn_PlayerRunCommand,	// CPlayer_MovementServices::PlayerRunCommand(this, CUserCmd*)
	Fn_DoMovementSubticks,	// CPlayer_MovementServices::DoMovementSubticks(this, CUserCmd*, CMoveData*)
	Fn_StartSubtickMove,	// CCSPlayer_MovementServices::StartSubtickMove(this, CMoveData*)
	Fn_ProcessMovement,		// CCSPlayer_MovementServices::ProcessMovement(this, CMoveData*)
	Fn_FinishSubtickMove,	// CCSPlayer_MovementServices::FinishSubtickMove(this, CMoveData*)
	Fn_TracePlayerBBox,		// CPlayerTraceCache::TracePlayerBBox(this, trace_t*, start, end, bounds, filter)

	// Only located, to name the callers of TracePlayerBBox
	Fn_SetupMove,
	Fn_FinishMove,
	Fn_CategorizePosition,
	Fn_CheckWaterJump,
	Fn_LadderMove,
	Fn_PushEntity,
	Fn_StayOnGround,
	Fn_StepMove,
	Fn_TestPlayerPosition,
	Fn_TraceGroundNormalBelow,
	Fn_TryTouchGroundInQuadrants,
	Fn_TryPlayerMove,
	Fn_WalkMove,
	Fn_WaterMove,

	Fn_Count
};

struct FunctionSignature
{
	const char* name;
	const char* signature;
};

// Taken from the 2026-10-23 server.dll; each matched exactly one function, with the same code as the reversed build
static const FunctionSignature g_Signatures[Fn_Count] = {
	{"PlayerRunCommand", "40 55 56 57 48 83 EC 70 0F 29 74 24 50 48 8B EA 48 8B F1 E8 ? ? ? ? 4C 8B 06 48 8B D5 48 8B CE 48 8B F8 41 FF 90"},
	{"DoMovementSubticks", "48 8B C4 48 89 48 08 53 57 48 81 EC 58 01 00 00 0F 29 78 A8 49 8B F8 4C 89 60 E0 4C 8B E1"},
	{"StartSubtickMove", "48 89 5C 24 08 57 48 83 EC 50 48 8B DA 48 8B F9 E8 ? ? ? ? 33 C9 48 89 8B 00 01 00 00"},
	{"ProcessMovement", "40 57 41 57 48 81 EC B8 00 00 00 48 83 79 38 00 4C 8B FA 48 8B F9 75 ? E8"},
	{"FinishSubtickMove", "48 89 6C 24 20 56 57 41 56 48 83 EC 30 48 83 79 38 00 48 8B F2 48 8B F9"},
	{"TracePlayerBBox", "48 89 74 24 18 55 57 41 54 41 55 41 56 48 8D AC 24 60 FF FF FF 48 81 EC"},
	{"SetupMove", "40 53 56 48 83 EC 68 48 89 6C 24 60 49 8B F0 48 89 7C 24 58 48 8B F9 4C"},
	{"FinishMove", "48 89 5C 24 08 48 89 74 24 10 57 48 83 EC 30 0F 29 74 24 20 49 8B F0 48"},
	{"CategorizePosition", "40 55 56 57 41 54 41 55 48 8D AC 24 F0 FD FF FF 48 81 EC 10 03 00 00 48"},
	{"CheckWaterJump", "48 89 5C 24 10 48 89 74 24 18 4C 89 74 24 20 55 48 8D AC 24 E0 FE FF FF"},
	{"LadderMove", "40 55 53 56 57 41 57 48 8D AC 24 B0 FC FF FF 48 81 EC 50 04 00 00 48 8B"},
	{"PushEntity", "48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 57 41 56 41 57 48 81 EC B0"},
	{"StayOnGround", "48 89 5C 24 08 48 89 74 24 10 48 89 7C 24 18 4C 89 64 24 20 55 41 56 41 57 48 8D 6C 24 80"},
	{"StepMove", "48 8B C4 4C 89 40 18 55 53 56 41 54 41 55 48 8D A8 78 FC FF FF 48 81 EC"},
	{"TestPlayerPosition", "48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 48 89 7C 24 20 41 54 41 56 41 57 48 81 EC A0"},
	{"TraceGroundNormalBelow", "48 8B C4 48 89 58 08 48 89 70 10 48 89 78 18 4C 89 70 20 55 48 8D A8 68 FF FF"},
	{"TryTouchGroundInQuadrants", "48 8B C4 48 89 58 20 4C 89 40 18 48 89 50 10 55 56 57 41 54 41 55 41 56 41 57 48 8D 68 C1"},
	{"TryPlayerMove", "48 8B C4 4C 89 48 20 4C 89 40 18 48 89 50 10 48 89 48 08 55 53 56 57 41 54 41 55 41 56 41 57 48 8D A8 A8 FD"},
	{"WalkMove", "48 8B C4 48 89 70 18 48 89 78 20 55 41 54 41 55 41 56 41 57 48 8D A8 C8"},
	{"WaterMove", "48 8B C4 48 89 58 08 48 89 70 10 48 89 78 18 4C 89 60 20 55 41 56 41 57 48 8D A8 68"},
};

static uint8_t* g_pServerBase = nullptr;
static size_t g_nServerSize = 0;
static uint32_t g_nServerTimeDateStamp = 0;
static uint8_t* g_pFunctions[Fn_Count] = {};
static bool g_bHooked[Fn_Count] = {};
static void* g_pCSMovementServicesVTable = nullptr;
static CGlobalVars** g_ppGlobals = nullptr;

// ---------------------------------------------------------------------------
// Layouts that are not in the schema. Each was checked against the code of the
// hooked functions in the 2026-10-23 server.dll.
// ---------------------------------------------------------------------------

// CPlayerPawnComponent::m_pPawn
static constexpr int kServices_Pawn = 0x38;
// CPlayer_MovementServices::m_nForceReleasedButtons / m_nPendingForceReleasedButtons
static constexpr int kServices_ForceReleasedButtons = 0x198;
static constexpr int kServices_PendingForceReleasedButtons = 0x238;

// CUserCmd
static constexpr int kCmd_CommandNumber = 0x8;
static constexpr int kCmd_Message = 0x10; // CSGOUserCmdPB
static constexpr int kCmd_LateCommand = 0x90; // non-null for a queued command run with zero frametime

// CMoveData
namespace MoveDataOffsets
{
static constexpr int Flags = 0x0;
static constexpr int PlayerHandle = 0x4;
static constexpr int AbsViewAngles = 0x8;
static constexpr int ViewAngles = 0x14;
static constexpr int LastMovementImpulses = 0x20;
static constexpr int ForwardMove = 0x2C;
static constexpr int SideMove = 0x30;
static constexpr int UpMove = 0x34;
static constexpr int Velocity = 0x38;
static constexpr int Angles = 0x44;
static constexpr int SubtickMoves = 0x60; // CUtlVector<SubtickMove>: int count, then the element pointer at +8
static constexpr int AttackSubtickMoves = 0x78;
static constexpr int AttackSubtickEndFraction = 0x90;
static constexpr int TouchList = 0x98;
static constexpr int CollisionNormal = 0xB0;
static constexpr int GroundNormal = 0xBC;
static constexpr int AbsOrigin = 0xC8;
static constexpr int TickCount = 0xD4;
static constexpr int TargetTick = 0xD8;
static constexpr int SubtickStartFraction = 0xDC;
static constexpr int SubtickEndFraction = 0xE0;
static constexpr int OutWishVel = 0xE8;
static constexpr int OldAngles = 0xF4;
static constexpr int WalkWishVel = 0x100;
static constexpr int ContinuousAcceleration = 0x108;
static constexpr int FrameVelocityDelta = 0x114;
static constexpr int MaxSpeed = 0x120;
static constexpr int ClientMaxSpeed = 0x124;
static constexpr int FrictionDecel = 0x128;
static constexpr int PreAirMovePosZ = 0x12C;
static constexpr int PreAirMoveVelZ = 0x130;
static constexpr int PreAirMoveAccelZ = 0x134;
static constexpr int InAir = 0x138;
static constexpr int GameCodeMovedPlayer = 0x139;
static constexpr int UseFrictionStashedSpeed = 0x13A;
} // namespace MoveDataOffsets

// SubtickMove, 0x20 bytes
static constexpr int kSubtickMove_Size = 0x20;
static constexpr int kSubtickMove_When = 0x0;
static constexpr int kSubtickMove_Button = 0x8;
static constexpr int kSubtickMove_Data = 0x10; // bool pressed, or float analog forward + float analog left
static constexpr int kSubtickMove_Pitch = 0x18;
static constexpr int kSubtickMove_Yaw = 0x1C;

// trace_t
namespace TraceOffsets
{
static constexpr int SurfaceProperties = 0x0;
static constexpr int Entity = 0x8;
static constexpr int Contents = 0x28;
static constexpr int StartPos = 0x78;
static constexpr int EndPos = 0x84;
static constexpr int HitNormal = 0x90;
static constexpr int HitPoint = 0x9C;
static constexpr int HitOffset = 0xA8;
static constexpr int Fraction = 0xAC;
static constexpr int Triangle = 0xB4;
static constexpr int RayType = 0xBA;
static constexpr int StartInSolid = 0xBB;
} // namespace TraceOffsets

// CPhysSurfaceProperties: CUtlString m_name, uint32 m_nameHash
static constexpr int kSurface_Name = 0x0;
static constexpr int kSurface_NameHash = 0x8;

template <typename T>
static T Read(const void* base, int offset)
{
	T value;
	memcpy(&value, (const uint8_t*)base + offset, sizeof(T));
	return value;
}

static void ReadVec(const void* base, int offset, float* out, int count = 3)
{
	memcpy(out, (const uint8_t*)base + offset, sizeof(float) * count);
}

// ---------------------------------------------------------------------------
// Schema lookups, walking base classes
// ---------------------------------------------------------------------------

static int FindFieldOffset(SchemaClassInfoData_t* pClass, const char* fieldName, int depth = 0)
{
	if (!pClass || depth > 32)
		return -1;

	for (int i = 0; i < pClass->m_nNumFields; i++)
	{
		if (!strcmp(pClass->m_pFields[i].m_pName, fieldName))
			return pClass->m_pFields[i].m_nSingleInheritanceOffset;
	}

	for (int i = 0; i < pClass->m_nNumBaseClasses; i++)
	{
		int offset = FindFieldOffset(pClass->m_pBaseClasses[i].m_pClass, fieldName, depth + 1);

		if (offset >= 0)
			return pClass->m_pBaseClasses[i].m_nOffset + offset;
	}

	return -1;
}

static int SchemaOffset(const char* className, const char* fieldName)
{
	CSchemaSystemTypeScope* pScope = g_pSchemaSystem->FindTypeScopeForModule(MODULE_PREFIX "server" MODULE_EXT);

	if (!pScope)
		return -1;

	return FindFieldOffset(pScope->FindDeclaredClass(className).Get(), fieldName);
}

// ---------------------------------------------------------------------------
// Player state, read field by field through the schema
// ---------------------------------------------------------------------------

enum class FieldType : uint8_t
{
	F32,
	I32,
	U32,
	U64,
	U16,
	U8,
	Bool,
	Vec2,
	Vec3,
};

static int FieldSize(FieldType type)
{
	switch (type)
	{
	case FieldType::U64: return 8;
	case FieldType::U16: return 2;
	case FieldType::U8:
	case FieldType::Bool: return 1;
	case FieldType::Vec2: return 8;
	case FieldType::Vec3: return 12;
	default: return 4;
	}
}

enum class Owner : uint8_t
{
	Services,
	Pawn,
	SceneNode,
	WaterServices,
	CameraServices,
	Controller,
	ActiveWeapon,
	Count
};

// Fields also captured around every slice; the rest only at the start and end of a command
static constexpr uint8_t kSlice = 1;

struct FieldSpec
{
	const char* json; // C# state name where the reimplementation has one, else the schema name
	Owner owner;
	const char* className; // null: extra is an absolute offset
	const char* fieldName;
	FieldType type;
	int extra;
	const char* subClassName; // a member of an embedded class
	const char* subFieldName;
	int subFallback;
	uint8_t flags;
};

#define PAWN(json, field, type, flags) {json, Owner::Pawn, "CCSPlayerPawn", field, FieldType::type, 0, nullptr, nullptr, 0, flags}
#define NODE(json, field, type, flags) {json, Owner::SceneNode, "CGameSceneNode", field, FieldType::type, 0, nullptr, nullptr, 0, flags}
#define SERV(json, field, type, flags) {json, Owner::Services, "CCSPlayer_MovementServices", field, FieldType::type, 0, nullptr, nullptr, 0, flags}
#define SERV_X(json, field, type, extra, flags) {json, Owner::Services, "CCSPlayer_MovementServices", field, FieldType::type, extra, nullptr, nullptr, 0, flags}
#define SERV_SUB(json, field, subcls, subfield, fallback, type) {json, Owner::Services, "CCSPlayer_MovementServices", field, FieldType::type, 0, subcls, subfield, fallback, 0}

static const FieldSpec g_Fields[] = {
	NODE("AbsOrigin", "m_vecAbsOrigin", Vec3, kSlice),
	PAWN("AbsVelocity", "m_vecAbsVelocity", Vec3, kSlice),
	PAWN("BaseVelocity", "m_vecBaseVelocity", Vec3, kSlice),
	PAWN("Flags", "m_fFlags", U32, kSlice),
	PAWN("MoveType", "m_MoveType", U8, kSlice),
	PAWN("ActualMoveType", "m_nActualMoveType", U8, kSlice),
	PAWN("GroundEntityHandle", "m_hGroundEntity", U32, kSlice),

	// The networked origin: per axis a cell and an offset inside it, rebuilt into m_vecValue
	{"NetworkOrigin", Owner::SceneNode, "CGameSceneNode", "m_vecOrigin", FieldType::Vec3, 0, nullptr, nullptr, 0, kSlice},
	{"NetworkOriginCellX", Owner::SceneNode, "CGameSceneNode", "m_vecOrigin", FieldType::U16, 0, "CNetworkOriginCellCoordQuantizedVector", "m_cellX", 0x10, kSlice},
	{"NetworkOriginCellY", Owner::SceneNode, "CGameSceneNode", "m_vecOrigin", FieldType::U16, 0, "CNetworkOriginCellCoordQuantizedVector", "m_cellY", 0x12, kSlice},
	{"NetworkOriginCellZ", Owner::SceneNode, "CGameSceneNode", "m_vecOrigin", FieldType::U16, 0, "CNetworkOriginCellCoordQuantizedVector", "m_cellZ", 0x14, kSlice},
	{"NetworkOriginOutsideWorld", Owner::SceneNode, "CGameSceneNode", "m_vecOrigin", FieldType::U16, 0, "CNetworkOriginCellCoordQuantizedVector", "m_nOutsideWorld", 0x16, kSlice},
	{"NetworkOriginOffsetX", Owner::SceneNode, "CGameSceneNode", "m_vecOrigin", FieldType::F32, 0, "CNetworkOriginCellCoordQuantizedVector", "m_vecX", 0x18, kSlice},
	{"NetworkOriginOffsetY", Owner::SceneNode, "CGameSceneNode", "m_vecOrigin", FieldType::F32, 0, "CNetworkOriginCellCoordQuantizedVector", "m_vecY", 0x20, kSlice},
	{"NetworkOriginOffsetZ", Owner::SceneNode, "CGameSceneNode", "m_vecOrigin", FieldType::F32, 0, "CNetworkOriginCellCoordQuantizedVector", "m_vecZ", 0x28, kSlice},

	// The networked, quantized velocity
	{"NetworkVelocityX", Owner::Pawn, "CCSPlayerPawn", "m_vecVelocity", FieldType::F32, 0, "CNetworkVelocityVector", "m_vecX", 0x10, kSlice},
	{"NetworkVelocityY", Owner::Pawn, "CCSPlayerPawn", "m_vecVelocity", FieldType::F32, 0, "CNetworkVelocityVector", "m_vecY", 0x18, kSlice},
	{"NetworkVelocityZ", Owner::Pawn, "CCSPlayerPawn", "m_vecVelocity", FieldType::F32, 0, "CNetworkVelocityVector", "m_vecZ", 0x20, kSlice},

	PAWN("ViewAngles", "v_angle", Vec3, 0),
	PAWN("WaterLevelFraction", "m_flWaterLevel", F32, kSlice),
	PAWN("VelocityModifier", "m_flVelocityModifier", F32, 0),
	PAWN("IsWalking", "m_bIsWalking", Bool, 0),
	PAWN("IgnoreLadderJumpTime", "m_ignoreLadderJumpTime", F32, 0),
	PAWN("GravityScale", "m_flActualGravityScale", F32, 0),
	PAWN("m_flGravityScale", "m_flGravityScale", F32, 0),
	PAWN("EntityFriction", "m_flFriction", F32, 0),

	{"WaterJumpTime", Owner::WaterServices, "CCSPlayer_WaterServices", "m_flWaterJumpTime", FieldType::F32, 0, nullptr, nullptr, 0, 0},
	{"WaterJumpVelocity", Owner::WaterServices, "CCSPlayer_WaterServices", "m_vecWaterJumpVel", FieldType::Vec3, 0, nullptr, nullptr, 0, 0},
	{"SwimSoundTime", Owner::WaterServices, "CCSPlayer_WaterServices", "m_flSwimSoundTime", FieldType::F32, 0, nullptr, nullptr, 0, 0},

	{"ViewPunchAngle", Owner::CameraServices, "CPlayer_CameraServices", "m_vecCsViewPunchAngle", FieldType::Vec3, 0, nullptr, nullptr, 0, 0},
	{"m_nCsViewPunchAngleTick", Owner::CameraServices, "CPlayer_CameraServices", "m_nCsViewPunchAngleTick", FieldType::I32, 0, nullptr, nullptr, 0, 0},
	{"m_flCsViewPunchAngleTickRatio", Owner::CameraServices, "CPlayer_CameraServices", "m_flCsViewPunchAngleTickRatio", FieldType::F32, 0, nullptr, nullptr, 0, 0},

	// The held weapon sets how fast the player accelerates; its max speed comes from the weapon's vdata by subclass and mode
	{"WeaponSubclass", Owner::ActiveWeapon, "CBaseEntity", "m_nSubclassID", FieldType::U32, 0, nullptr, nullptr, 0, 0},
	{"WeaponMode", Owner::ActiveWeapon, "CCSWeaponBase", "m_weaponMode", FieldType::I32, 0, nullptr, nullptr, 0, 0},

	{"TickBase", Owner::Controller, "CBasePlayerController", "m_nTickBase", FieldType::U32, 0, nullptr, nullptr, 0, 0},

	// CInButtonState: vtable, then the three button masks
	{"ButtonsHeld", Owner::Services, "CCSPlayer_MovementServices", "m_nButtons", FieldType::U64, 0, "CInButtonState", "m_pButtonStates", 0x8, kSlice},
	{"ButtonsChanged", Owner::Services, "CCSPlayer_MovementServices", "m_nButtons", FieldType::U64, 8, "CInButtonState", "m_pButtonStates", 0x8, 0},
	{"ButtonsChangedTwice", Owner::Services, "CCSPlayer_MovementServices", "m_nButtons", FieldType::U64, 16, "CInButtonState", "m_pButtonStates", 0x8, 0},
	SERV("QueuedButtonDownMask", "m_nQueuedButtonDownMask", U64, 0),
	SERV("QueuedButtonChangeMask", "m_nQueuedButtonChangeMask", U64, 0),
	SERV("m_nButtonDoublePressed", "m_nButtonDoublePressed", U64, 0),
	SERV("m_nToggleButtonDownMask", "m_nToggleButtonDownMask", U64, 0),
	SERV("LastCommandNumberProcessed", "m_nLastCommandNumberProcessed", U32, 0),
	SERV("m_nImpulse", "m_nImpulse", I32, 0),
	SERV("CmdForwardMove", "m_flCmdForwardMove", F32, 0),
	SERV("CmdLeftMove", "m_flCmdLeftMove", F32, 0),
	SERV("CmdUpMove", "m_flCmdUpMove", F32, 0),
	SERV("MaxSpeed", "m_flMaxspeed", F32, 0),
	SERV_X("ForceSubtickMoveWhen0", "m_arrForceSubtickMoveWhen", F32, 0, 0),
	SERV_X("ForceSubtickMoveWhen1", "m_arrForceSubtickMoveWhen", F32, 4, 0),
	SERV_X("ForceSubtickMoveWhen2", "m_arrForceSubtickMoveWhen", F32, 8, 0),
	SERV_X("ForceSubtickMoveWhen3", "m_arrForceSubtickMoveWhen", F32, 12, 0),
	SERV("ForwardMove", "m_flForwardMove", F32, 0),
	SERV("LeftMove", "m_flLeftMove", F32, 0),
	SERV("UpMove", "m_flUpMove", F32, 0),
	SERV("LastMovementImpulses", "m_vecLastMovementImpulses", Vec3, 0),
	{"ForceReleasedButtons", Owner::Services, nullptr, nullptr, FieldType::U64, kServices_ForceReleasedButtons, nullptr, nullptr, 0, 0},
	{"PendingForceReleasedButtons", Owner::Services, nullptr, nullptr, FieldType::U64, kServices_PendingForceReleasedButtons, nullptr, nullptr, 0, 0},
	SERV("m_vecOldViewAngles", "m_vecOldViewAngles", Vec3, 0),
	SERV("StepSoundTime", "m_flStepSoundTime", F32, 0),
	SERV("FallVelocity", "m_flFallVelocity", F32, 0),
	SERV("GroundNormal", "m_groundNormal", Vec3, 0),
	SERV("SurfaceFriction", "m_flSurfaceFriction", F32, kSlice),
	SERV("SurfacePropertyHash", "m_surfaceProps", U32, 0),
	SERV("m_nStepside", "m_nStepside", I32, 0),
	SERV("LadderNormal", "m_vecLadderNormal", Vec3, 0),
	SERV("m_nLadderSurfacePropIndex", "m_nLadderSurfacePropIndex", I32, 0),
	SERV("Ducked", "m_bDucked", Bool, kSlice),
	SERV("DuckAmount", "m_flDuckAmount", F32, kSlice),
	SERV("DuckSpeed", "m_flDuckSpeed", F32, 0),
	SERV("m_bDuckOverride", "m_bDuckOverride", Bool, 0),
	SERV("DesiresDuck", "m_bDesiresDuck", Bool, 0),
	SERV("Ducking", "m_bDucking", Bool, kSlice),
	SERV("DuckRootOffset", "m_flDuckRootOffset", F32, 0),
	SERV("DuckViewOffset", "m_flDuckViewOffset", F32, 0),
	SERV("LastDuckTime", "m_flLastDuckTime", F32, 0),
	SERV("m_flBombPlantViewOffset", "m_flBombPlantViewOffset", F32, 0),
	SERV("LastPositionAtFullCrouchSpeed", "m_vecLastPositionAtFullCrouchSpeed", Vec2, 0),
	SERV("DuckUntilOnGround", "m_duckUntilOnGround", Bool, 0),
	SERV("HasWalkMovedSinceLastJump", "m_bHasWalkMovedSinceLastJump", Bool, 0),
	SERV("m_bInStuckTest", "m_bInStuckTest", Bool, 0),
	SERV("m_nTraceCount", "m_nTraceCount", I32, 0),
	SERV("StuckLast", "m_StuckLast", I32, 0),
	SERV("SpeedCropped", "m_bSpeedCropped", Bool, 0),
	SERV("OldWaterLevel", "m_nOldWaterLevel", I32, 0),
	SERV("m_flWaterEntryTime", "m_flWaterEntryTime", F32, 0),
	SERV("Forward", "m_vecForward", Vec3, 0),
	SERV("Left", "m_vecLeft", Vec3, 0),
	SERV("Up", "m_vecUp", Vec3, 0),
	SERV("m_nGameCodeHasMovedPlayerAfterCommand", "m_nGameCodeHasMovedPlayerAfterCommand", I32, 0),
	SERV("m_bMadeFootstepNoise", "m_bMadeFootstepNoise", Bool, 0),
	SERV("m_iFootsteps", "m_iFootsteps", I32, 0),
	SERV("StashGrenadeParameterWhen", "m_fStashGrenadeParameterWhen", F32, 0),
	SERV("UseFrictionStashedSpeed", "m_bUseFrictionStashedSpeed", Bool, 0),
	SERV("UseFrictionStashedSpeedUntilFrac", "m_flUseFrictionStashedSpeedUntilFrac", F32, 0),
	SERV("FrictionStashedSpeed", "m_flFrictionStashedSpeed", F32, 0),
	SERV("Stamina", "m_flStamina", F32, 0),
	SERV("m_flHeightAtJumpStart", "m_flHeightAtJumpStart", F32, 0),
	SERV("m_flMaxJumpHeightThisJump", "m_flMaxJumpHeightThisJump", F32, 0),
	SERV("m_flMaxJumpHeightLastJump", "m_flMaxJumpHeightLastJump", F32, 0),
	SERV("m_flStaminaAtJumpStart", "m_flStaminaAtJumpStart", F32, 0),
	SERV("m_flVelMulAtJumpStart", "m_flVelMulAtJumpStart", F32, 0),
	SERV("AccumulatedJumpError", "m_flAccumulatedJumpError", F32, 0),
	SERV_SUB("OldJumpPressed", "m_LegacyJump", "CCSPlayerLegacyJump", "m_bOldJumpPressed", 0x10, Bool),
	SERV_SUB("JumpPressedTime", "m_LegacyJump", "CCSPlayerLegacyJump", "m_flJumpPressedTime", 0x14, F32),
	SERV_SUB("LastActualJumpPressTick", "m_ModernJump", "CCSPlayerModernJump", "m_nLastActualJumpPressTick", 0x10, I32),
	SERV_SUB("LastActualJumpPressFrac", "m_ModernJump", "CCSPlayerModernJump", "m_flLastActualJumpPressFrac", 0x14, F32),
	SERV_SUB("LastUsableJumpPressTick", "m_ModernJump", "CCSPlayerModernJump", "m_nLastUsableJumpPressTick", 0x18, I32),
	SERV_SUB("LastUsableJumpPressFrac", "m_ModernJump", "CCSPlayerModernJump", "m_flLastUsableJumpPressFrac", 0x1C, F32),
	SERV_SUB("LastLandedTick", "m_ModernJump", "CCSPlayerModernJump", "m_nLastLandedTick", 0x20, I32),
	SERV_SUB("LastLandedFrac", "m_ModernJump", "CCSPlayerModernJump", "m_flLastLandedFrac", 0x24, F32),
	SERV_SUB("LastLandedVelocityX", "m_ModernJump", "CCSPlayerModernJump", "m_flLastLandedVelocityX", 0x28, F32),
	SERV_SUB("LastLandedVelocityY", "m_ModernJump", "CCSPlayerModernJump", "m_flLastLandedVelocityY", 0x2C, F32),
	SERV_SUB("LastLandedVelocityZ", "m_ModernJump", "CCSPlayerModernJump", "m_flLastLandedVelocityZ", 0x30, F32),
	SERV("LastJumpTick", "m_nLastJumpTick", I32, 0),
	SERV("LastJumpFrac", "m_flLastJumpFrac", F32, 0),
	SERV("LastJumpVelocityZ", "m_flLastJumpVelocityZ", F32, 0),
	SERV("JumpApexPending", "m_bJumpApexPending", Bool, 0),
	SERV("m_flTicksSinceLastSurfingDetected", "m_flTicksSinceLastSurfingDetected", F32, 0),
	SERV("WalkWishVel", "m_vecWalkWishVel", Vec2, 0),
	SERV("m_bHasEverProcessedCommand", "m_bHasEverProcessedCommand", Bool, 0),
};

#undef PAWN
#undef NODE
#undef SERV
#undef SERV_X
#undef SERV_SUB

static constexpr int kFieldCount = sizeof(g_Fields) / sizeof(g_Fields[0]);

enum Layout
{
	Layout_Full,
	Layout_Slice,
	Layout_Count
};

static int g_FieldOffsets[kFieldCount];
static int g_LayoutOffsets[Layout_Count][kFieldCount];
static int g_LayoutSizes[Layout_Count];

// Pointers between the objects that own the fields
static int g_nOffBodyComponent = -1;
static int g_nOffSceneNode = -1;
static int g_nOffWaterServices = -1;
static int g_nOffCameraServices = -1;
static int g_nOffController = -1;
static int g_nOffWeaponServices = -1;
static int g_nOffActiveWeapon = -1;

static int FieldIndex(const char* json)
{
	for (int i = 0; i < kFieldCount; i++)
	{
		if (!strcmp(g_Fields[i].json, json))
			return i;
	}

	return -1;
}

static int g_iAbsOrigin, g_iAbsVelocity, g_iBaseVelocity, g_iFlags, g_iMoveType, g_iActualMoveType, g_iGroundEntityHandle;
static int g_iNetworkOrigin, g_iCellX, g_iOffsetX, g_iNetVelocityX, g_iWaterLevel, g_iDucked, g_iDuckAmount, g_iSurfaceFriction;
static int g_iButtonsHeld;

static void ResolveFields()
{
	for (int i = 0; i < kFieldCount; i++)
	{
		const FieldSpec& spec = g_Fields[i];
		int offset = spec.extra;

		if (spec.className)
		{
			int base = SchemaOffset(spec.className, spec.fieldName);

			if (base < 0)
			{
				Warning("[MovementRecorder] Schema field %s::%s not found, '%s' will not be recorded\n", spec.className, spec.fieldName, spec.json);
				g_FieldOffsets[i] = -1;
				continue;
			}

			offset += base;
		}

		if (spec.subClassName)
		{
			int sub = SchemaOffset(spec.subClassName, spec.subFieldName);
			offset += sub >= 0 ? sub : spec.subFallback;
		}

		g_FieldOffsets[i] = offset;
	}

	for (int layout = 0; layout < Layout_Count; layout++)
	{
		int size = 0;

		for (int i = 0; i < kFieldCount; i++)
		{
			bool included = layout == Layout_Full || (g_Fields[i].flags & kSlice);
			g_LayoutOffsets[layout][i] = included ? size : -1;

			if (included)
				size += FieldSize(g_Fields[i].type);
		}

		g_LayoutSizes[layout] = size;
	}

	g_nOffBodyComponent = SchemaOffset("CBaseEntity", "m_CBodyComponent");
	g_nOffSceneNode = SchemaOffset("CBodyComponent", "m_pSceneNode");
	g_nOffWaterServices = SchemaOffset("CBasePlayerPawn", "m_pWaterServices");
	g_nOffCameraServices = SchemaOffset("CBasePlayerPawn", "m_pCameraServices");
	g_nOffController = SchemaOffset("CBasePlayerPawn", "m_hController");
	g_nOffWeaponServices = SchemaOffset("CBasePlayerPawn", "m_pWeaponServices");
	g_nOffActiveWeapon = SchemaOffset("CPlayer_WeaponServices", "m_hActiveWeapon");

	g_iAbsOrigin = FieldIndex("AbsOrigin");
	g_iAbsVelocity = FieldIndex("AbsVelocity");
	g_iBaseVelocity = FieldIndex("BaseVelocity");
	g_iFlags = FieldIndex("Flags");
	g_iMoveType = FieldIndex("MoveType");
	g_iActualMoveType = FieldIndex("ActualMoveType");
	g_iGroundEntityHandle = FieldIndex("GroundEntityHandle");
	g_iNetworkOrigin = FieldIndex("NetworkOrigin");
	g_iCellX = FieldIndex("NetworkOriginCellX");
	g_iOffsetX = FieldIndex("NetworkOriginOffsetX");
	g_iNetVelocityX = FieldIndex("NetworkVelocityX");
	g_iWaterLevel = FieldIndex("WaterLevelFraction");
	g_iDucked = FieldIndex("Ducked");
	g_iDuckAmount = FieldIndex("DuckAmount");
	g_iSurfaceFriction = FieldIndex("SurfaceFriction");
	g_iButtonsHeld = FieldIndex("ButtonsHeld");
}

static CEntityInstance* ResolveHandle(uint32_t handle)
{
	if (handle == 0xFFFFFFFF || !GameEntitySystem())
		return nullptr;

	return GameEntitySystem()->GetEntityInstance(CEntityHandle(handle));
}

struct Owners
{
	uint8_t* ptr[(int)Owner::Count] = {};
};

static Owners GetOwners(void* services)
{
	Owners owners;
	uint8_t* pServices = (uint8_t*)services;
	uint8_t* pPawn = Read<uint8_t*>(pServices, kServices_Pawn);

	owners.ptr[(int)Owner::Services] = pServices;
	owners.ptr[(int)Owner::Pawn] = pPawn;

	if (!pPawn)
		return owners;

	if (g_nOffBodyComponent >= 0 && g_nOffSceneNode >= 0)
	{
		uint8_t* pBody = Read<uint8_t*>(pPawn, g_nOffBodyComponent);

		if (pBody)
			owners.ptr[(int)Owner::SceneNode] = Read<uint8_t*>(pBody, g_nOffSceneNode);
	}

	if (g_nOffWaterServices >= 0)
		owners.ptr[(int)Owner::WaterServices] = Read<uint8_t*>(pPawn, g_nOffWaterServices);

	if (g_nOffCameraServices >= 0)
		owners.ptr[(int)Owner::CameraServices] = Read<uint8_t*>(pPawn, g_nOffCameraServices);

	if (g_nOffController >= 0)
		owners.ptr[(int)Owner::Controller] = (uint8_t*)ResolveHandle(Read<uint32_t>(pPawn, g_nOffController));

	if (g_nOffWeaponServices >= 0 && g_nOffActiveWeapon >= 0)
	{
		uint8_t* pWeaponServices = Read<uint8_t*>(pPawn, g_nOffWeaponServices);

		if (pWeaponServices)
			owners.ptr[(int)Owner::ActiveWeapon] = (uint8_t*)ResolveHandle(Read<uint32_t>(pWeaponServices, g_nOffActiveWeapon));
	}

	return owners;
}

struct StateSnap
{
	Layout layout = Layout_Full;
	std::vector<uint8_t> data;
	std::bitset<kFieldCount> present;
	char groundEntity[64] = {};
};

static void CaptureState(void* services, Layout layout, StateSnap& snap)
{
	Owners owners = GetOwners(services);

	snap.layout = layout;
	snap.data.assign(g_LayoutSizes[layout], 0);
	snap.present.reset();

	for (int i = 0; i < kFieldCount; i++)
	{
		int dest = g_LayoutOffsets[layout][i];
		uint8_t* owner = owners.ptr[(int)g_Fields[i].owner];

		if (dest < 0 || g_FieldOffsets[i] < 0 || !owner)
			continue;

		memcpy(snap.data.data() + dest, owner + g_FieldOffsets[i], FieldSize(g_Fields[i].type));
		snap.present.set(i);
	}

	snap.groundEntity[0] = '\0';

	if (snap.present.test(g_iGroundEntityHandle))
	{
		uint32_t handle;
		memcpy(&handle, snap.data.data() + g_LayoutOffsets[layout][g_iGroundEntityHandle], sizeof(handle));

		if (CEntityInstance* pGround = ResolveHandle(handle))
			V_strncpy(snap.groundEntity, pGround->GetClassname(), sizeof(snap.groundEntity));
	}
}

template <typename T>
static T SnapGet(const StateSnap& snap, int field, T fallback = T())
{
	if (field < 0 || !snap.present.test(field))
		return fallback;

	T value;
	memcpy(&value, snap.data.data() + g_LayoutOffsets[snap.layout][field], sizeof(T));
	return value;
}

static bool SnapGetVec(const StateSnap& snap, int field, float* out)
{
	if (field < 0 || !snap.present.test(field))
	{
		out[0] = out[1] = out[2] = 0.0f;
		return false;
	}

	memcpy(out, snap.data.data() + g_LayoutOffsets[snap.layout][field], sizeof(float) * 3);
	return true;
}

// ---------------------------------------------------------------------------
// Recorded data
// ---------------------------------------------------------------------------

struct GlobalsSnap
{
	bool valid = false;
	float curtime = 0;
	float frametime = 0;
	float subtickFraction = 0;
	int tickcount = 0;
};

static GlobalsSnap CaptureGlobals()
{
	GlobalsSnap snap;
	CGlobalVars* globals = g_ppGlobals ? *g_ppGlobals : nullptr;

	if (!globals)
		return snap;

	snap.valid = true;
	snap.curtime = globals->curtime;
	snap.frametime = globals->frametime;
	snap.subtickFraction = globals->m_flSubtickFraction;
	snap.tickcount = globals->tickcount;
	return snap;
}

struct MoveDataSnap
{
	uint8_t flags;
	uint32_t playerHandle;
	float absViewAngles[3], viewAngles[3], lastMovementImpulses[3];
	float forwardMove, sideMove, upMove;
	float velocity[3], angles[3];
	int subtickMoveCount, attackSubtickMoveCount, touchCount;
	float attackSubtickEndFraction;
	float collisionNormal[3], groundNormal[3], absOrigin[3];
	int tickCount, targetTick;
	float subtickStartFraction, subtickEndFraction;
	float outWishVel[3], oldAngles[3], walkWishVel[2];
	float continuousAcceleration[3], frameVelocityDelta[3];
	float maxSpeed, clientMaxSpeed, frictionDecel;
	float preAirMovePosZ, preAirMoveVelZ, preAirMoveAccelZ;
	bool inAir, gameCodeMovedPlayer, useFrictionStashedSpeed;
};

static void CaptureMoveData(const void* mv, MoveDataSnap& s)
{
	using namespace MoveDataOffsets;

	s.flags = Read<uint8_t>(mv, Flags);
	s.playerHandle = Read<uint32_t>(mv, PlayerHandle);
	ReadVec(mv, AbsViewAngles, s.absViewAngles);
	ReadVec(mv, ViewAngles, s.viewAngles);
	ReadVec(mv, LastMovementImpulses, s.lastMovementImpulses);
	s.forwardMove = Read<float>(mv, ForwardMove);
	s.sideMove = Read<float>(mv, SideMove);
	s.upMove = Read<float>(mv, UpMove);
	ReadVec(mv, Velocity, s.velocity);
	ReadVec(mv, Angles, s.angles);
	s.subtickMoveCount = Read<int>(mv, SubtickMoves);
	s.attackSubtickMoveCount = Read<int>(mv, AttackSubtickMoves);
	s.attackSubtickEndFraction = Read<float>(mv, AttackSubtickEndFraction);
	s.touchCount = Read<int>(mv, TouchList);
	ReadVec(mv, CollisionNormal, s.collisionNormal);
	ReadVec(mv, GroundNormal, s.groundNormal);
	ReadVec(mv, AbsOrigin, s.absOrigin);
	s.tickCount = Read<int>(mv, TickCount);
	s.targetTick = Read<int>(mv, TargetTick);
	s.subtickStartFraction = Read<float>(mv, SubtickStartFraction);
	s.subtickEndFraction = Read<float>(mv, SubtickEndFraction);
	ReadVec(mv, OutWishVel, s.outWishVel);
	ReadVec(mv, OldAngles, s.oldAngles);
	ReadVec(mv, WalkWishVel, s.walkWishVel, 2);
	ReadVec(mv, ContinuousAcceleration, s.continuousAcceleration);
	ReadVec(mv, FrameVelocityDelta, s.frameVelocityDelta);
	s.maxSpeed = Read<float>(mv, MaxSpeed);
	s.clientMaxSpeed = Read<float>(mv, ClientMaxSpeed);
	s.frictionDecel = Read<float>(mv, FrictionDecel);
	s.preAirMovePosZ = Read<float>(mv, PreAirMovePosZ);
	s.preAirMoveVelZ = Read<float>(mv, PreAirMoveVelZ);
	s.preAirMoveAccelZ = Read<float>(mv, PreAirMoveAccelZ);
	s.inAir = Read<bool>(mv, InAir);
	s.gameCodeMovedPlayer = Read<bool>(mv, GameCodeMovedPlayer);
	s.useFrictionStashedSpeed = Read<bool>(mv, UseFrictionStashedSpeed);
}

struct SubtickMoveSnap
{
	float when;
	uint64_t button;
	uint8_t data[8];
	float pitch;
	float yaw;
};

struct SliceRec
{
	GlobalsSnap globals;
	MoveDataSnap mvStart;
	bool hasProcessStart = false;
	MoveDataSnap mvProcessStart;
	bool hasProcessEnd = false;
	MoveDataSnap mvProcessEnd;
	StateSnap state;
};

struct SliceEndRec
{
	MoveDataSnap mv;
	StateSnap state;
};

struct TraceRec
{
	uintptr_t returnAddress;
	float start[3], end[3], mins[3], maxs[3];
	float fraction;
	bool startSolid;
	float endPos[3], normal[3], hitPoint[3];
	float hitOffset;
	int triangle;
	uint64_t contents;
	uint8_t rayType;
	uint32_t surfaceHash;
	char surfaceName[48];
	char entity[64];
};

enum class EventKind : uint8_t
{
	Slice,
	SliceEnd,
	Trace,
};

struct TickRecord
{
	void* services = nullptr;
	int playerIndex = -1;
	int commandNumber = 0;
	bool late = false;

	std::string commandBytes;
	bool commandSerialized = false;

	GlobalsSnap globalsBefore;
	StateSnap before;
	StateSnap after;

	bool hasSetup = false;
	GlobalsSnap setupGlobals;
	MoveDataSnap setupMv;
	std::vector<SubtickMoveSnap> setupMoves;

	bool hasFinal = false;
	MoveDataSnap finalMv;

	std::vector<std::pair<EventKind, uint32_t>> events;
	std::vector<SliceRec> slices;
	std::vector<SliceEndRec> sliceEnds;
	std::vector<TraceRec> traces;
	std::vector<std::string> markers;

	size_t ApproxSize() const
	{
		size_t size = sizeof(*this) + commandBytes.size() + before.data.size() + after.data.size();
		size += setupMoves.size() * sizeof(SubtickMoveSnap) + events.size() * 8 + traces.size() * sizeof(TraceRec);
		size += slices.size() * (sizeof(SliceRec) + g_LayoutSizes[Layout_Slice]);
		size += sliceEnds.size() * (sizeof(SliceEndRec) + g_LayoutSizes[Layout_Slice]);
		return size;
	}
};

struct VarValue
{
	const char* name;
	bool isBool;
	bool boolValue;
	float floatValue;
};

// The convars the reimplementation reads back, by their console names
static const char* g_FloatVars[] = {
	"sv_accelerate", "sv_air_max_wishspeed", "sv_airaccelerate", "sv_bhop_time_window", "sv_bounce", "sv_friction",
	"sv_gravity", "sv_jump_impulse", "sv_jump_spam_penalty_time", "sv_ladder_angle", "sv_ladder_dampen",
	"sv_ladder_scale_speed", "sv_maxspeed", "sv_maxvelocity", "sv_min_jump_landing_sound", "sv_noclipaccelerate",
	"sv_noclipspeed", "sv_rollangle", "sv_rollspeed", "sv_staminajumpcost", "sv_staminalandcost", "sv_staminamax",
	"sv_staminarecoveryrate", "sv_standable_normal", "sv_step_move_vel_min", "sv_stepsize", "sv_stopspeed",
	"sv_timebetweenducks", "sv_walkable_normal", "sv_water_slow_amount", "sv_falldamage_scale", "view_punch_decay",
};

static const char* g_BoolVars[] = {
	"sv_accelerate_use_weapon_speed", "sv_autobunnyhopping", "sv_enablebunnyhopping", "sv_jump_precision_enable",
	"sv_legacy_jump", "sv_optimizedmovement", "sv_quantize_movement_input", "sv_subtick_movement_view_angles",
	"sv_use_playercache",
};

struct LastState
{
	float origin[3];
	float velocity[3];
};

struct Recording
{
	std::mutex mutex;
	std::vector<std::shared_ptr<const TickRecord>> ticks;
	std::vector<VarValue> vars;
	std::string map;
	std::unordered_map<int, LastState> lastStates;
	size_t slices = 0;
	size_t traces = 0;
	size_t bytes = 0;
};

static Recording g_Recording;
static std::string g_MapName;
static std::mutex g_MapNameMutex;
static std::atomic<bool> g_bRecording = false;
static std::atomic<bool> g_bRecordTraces = true;

static thread_local TickRecord* t_pTick = nullptr;

// ---------------------------------------------------------------------------
// Capture
// ---------------------------------------------------------------------------

static bool IsCSMovementServices(void* services)
{
	if (g_pCSMovementServicesVTable)
		return *(void**)services == g_pCSMovementServicesVTable;

	uint8_t* pPawn = Read<uint8_t*>(services, kServices_Pawn);
	return pPawn && !strcmp(((CEntityInstance*)pPawn)->GetClassname(), "player");
}

static std::vector<VarValue> ReadVars()
{
	std::vector<VarValue> vars;

	auto add = [&vars](const char* name, bool isBool)
	{
		ConVarRefAbstract ref(name);

		if (!ref.IsValidRef() || !ref.IsConVarDataValid())
			return;

		VarValue value = {name, isBool, false, 0.0f};

		if (isBool)
			value.boolValue = ref.GetBool();
		else
			value.floatValue = ref.GetFloat();

		vars.push_back(value);
	};

	for (const char* name : g_FloatVars)
		add(name, false);

	for (const char* name : g_BoolVars)
		add(name, true);

	return vars;
}

// Serializes the game's own message through its vtable, so the layout of the game's protobuf classes does not matter
static void SerializeMessage(const google::protobuf::MessageLite* message, std::string* out)
{
	size_t size = message->ByteSizeLong();
	out->resize(size);

	if (size && !message->SerializePartialToArray(out->data(), (int)size))
		out->clear();
}

static bool SerializeMessageGuarded(const google::protobuf::MessageLite* message, std::string* out)
{
	__try
	{
		SerializeMessage(message, out);
		return true;
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		return false;
	}
}

static KHook::Return<void> Pre_PlayerRunCommand(void* services, void* cmd)
{
	if (!g_bRecording.load(std::memory_order_relaxed) || t_pTick || !services || !cmd || !IsCSMovementServices(services))
		return {KHook::Action::Ignore};

	TickRecord* tick = new TickRecord();
	tick->services = services;
	tick->commandNumber = Read<int>(cmd, kCmd_CommandNumber);
	tick->late = Read<void*>(cmd, kCmd_LateCommand) != nullptr;
	tick->commandSerialized = SerializeMessageGuarded((const google::protobuf::MessageLite*)((uint8_t*)cmd + kCmd_Message), &tick->commandBytes);
	tick->globalsBefore = CaptureGlobals();

	if (uint8_t* pPawn = Read<uint8_t*>(services, kServices_Pawn))
		tick->playerIndex = ((CEntityInstance*)pPawn)->GetEntityIndex().Get();

	CaptureState(services, Layout_Full, tick->before);

	t_pTick = tick;
	return {KHook::Action::Ignore};
}

static void Commit(TickRecord* tick)
{
	auto record = std::shared_ptr<const TickRecord>(tick);
	std::vector<VarValue> vars;
	bool needVars;

	{
		std::lock_guard<std::mutex> lock(g_Recording.mutex);
		needVars = g_Recording.vars.empty();
	}

	// The first tick of a recording notes the settings it ran with, on the game thread
	if (needVars)
		vars = ReadVars();

	std::string map;
	{
		std::lock_guard<std::mutex> lock(g_MapNameMutex);
		map = g_MapName;
	}

	if (map.empty() && g_ppGlobals && *g_ppGlobals)
		map = STRING((*g_ppGlobals)->mapname);

	std::lock_guard<std::mutex> lock(g_Recording.mutex);

	if (needVars && g_Recording.vars.empty())
	{
		g_Recording.vars = std::move(vars);
		g_Recording.map = map;
	}

	// Anything that moved the player between commands, such as a teleport or respawn, has to be restored on replay
	LastState now = {};
	SnapGetVec(tick->before, g_iAbsOrigin, now.origin);
	SnapGetVec(tick->before, g_iAbsVelocity, now.velocity);

	auto it = g_Recording.lastStates.find(tick->playerIndex);

	if (it != g_Recording.lastStates.end() && memcmp(&it->second, &now, sizeof(now)))
		tick->markers.push_back("changed outside movement");

	if (tick->late)
		tick->markers.push_back("late command");

	LastState after = {};
	SnapGetVec(tick->after, g_iAbsOrigin, after.origin);
	SnapGetVec(tick->after, g_iAbsVelocity, after.velocity);
	g_Recording.lastStates[tick->playerIndex] = after;

	g_Recording.slices += tick->slices.size();
	g_Recording.traces += tick->traces.size();
	g_Recording.bytes += tick->ApproxSize();
	g_Recording.ticks.push_back(std::move(record));
}

static KHook::Return<void> Post_PlayerRunCommand(void* services, void* cmd)
{
	TickRecord* tick = t_pTick;

	if (!tick || tick->services != services)
		return {KHook::Action::Ignore};

	t_pTick = nullptr;

	// A paused game returns before any movement runs; there is nothing to compare
	if (!tick->hasSetup || !g_bRecording.load(std::memory_order_relaxed))
	{
		delete tick;
		return {KHook::Action::Ignore};
	}

	CaptureState(services, Layout_Full, tick->after);
	Commit(tick);
	return {KHook::Action::Ignore};
}

static TickRecord* CurrentTick(void* services)
{
	TickRecord* tick = t_pTick;
	return tick && tick->services == services ? tick : nullptr;
}

// Runs right after SetupMove, so the move data holds what the command starts from
static KHook::Return<void> Pre_DoMovementSubticks(void* services, void* cmd, void* mv)
{
	TickRecord* tick = CurrentTick(services);

	if (!tick || !mv)
		return {KHook::Action::Ignore};

	tick->hasSetup = true;
	tick->setupGlobals = CaptureGlobals();
	CaptureMoveData(mv, tick->setupMv);

	int count = Read<int>(mv, MoveDataOffsets::SubtickMoves);
	uint8_t* moves = Read<uint8_t*>(mv, MoveDataOffsets::SubtickMoves + 8);

	if (moves && count > 0 && count < 1024)
	{
		tick->setupMoves = std::vector<SubtickMoveSnap>(count);

		for (int i = 0; i < count; i++)
		{
			const uint8_t* move = moves + i * kSubtickMove_Size;
			SubtickMoveSnap& snap = tick->setupMoves[i];
			snap.when = Read<float>(move, kSubtickMove_When);
			snap.button = Read<uint64_t>(move, kSubtickMove_Button);
			memcpy(snap.data, move + kSubtickMove_Data, sizeof(snap.data));
			snap.pitch = Read<float>(move, kSubtickMove_Pitch);
			snap.yaw = Read<float>(move, kSubtickMove_Yaw);
		}
	}

	return {KHook::Action::Ignore};
}

static KHook::Return<void> Post_DoMovementSubticks(void* services, void* cmd, void* mv)
{
	TickRecord* tick = CurrentTick(services);

	if (!tick || !mv)
		return {KHook::Action::Ignore};

	tick->hasFinal = true;
	CaptureMoveData(mv, tick->finalMv);
	return {KHook::Action::Ignore};
}

// The slice's time globals are already set when StartSubtickMove runs
static KHook::Return<void> Pre_StartSubtickMove(void* services, void* mv)
{
	TickRecord* tick = CurrentTick(services);

	if (!tick || !mv)
		return {KHook::Action::Ignore};

	tick->events.emplace_back(EventKind::Slice, (uint32_t)tick->slices.size());
	SliceRec& slice = tick->slices.emplace_back();
	slice.globals = CaptureGlobals();
	CaptureMoveData(mv, slice.mvStart);
	CaptureState(services, Layout_Slice, slice.state);
	return {KHook::Action::Ignore};
}

static KHook::Return<void> Pre_ProcessMovement(void* services, void* mv)
{
	TickRecord* tick = CurrentTick(services);

	if (!tick || !mv || tick->slices.empty())
		return {KHook::Action::Ignore};

	SliceRec& slice = tick->slices.back();
	slice.hasProcessStart = true;
	CaptureMoveData(mv, slice.mvProcessStart);
	return {KHook::Action::Ignore};
}

static KHook::Return<void> Post_ProcessMovement(void* services, void* mv)
{
	TickRecord* tick = CurrentTick(services);

	if (!tick || !mv || tick->slices.empty())
		return {KHook::Action::Ignore};

	SliceRec& slice = tick->slices.back();
	slice.hasProcessEnd = true;
	CaptureMoveData(mv, slice.mvProcessEnd);
	return {KHook::Action::Ignore};
}

static KHook::Return<void> Post_FinishSubtickMove(void* services, void* mv)
{
	TickRecord* tick = CurrentTick(services);

	if (!tick || !mv)
		return {KHook::Action::Ignore};

	tick->events.emplace_back(EventKind::SliceEnd, (uint32_t)tick->sliceEnds.size());
	SliceEndRec& end = tick->sliceEnds.emplace_back();
	CaptureMoveData(mv, end.mv);
	CaptureState(services, Layout_Slice, end.state);
	return {KHook::Action::Ignore};
}

// ---------------------------------------------------------------------------
// TracePlayerBBox. Hooked inline rather than through KHook, so the hook sees the
// real return address and can tell which movement function traced.
// ---------------------------------------------------------------------------

using TracePlayerBBox_t = void (*)(void* cache, void* pm, const float* start, const float* end, const float* bounds, void* filter);

// The first instructions, all position independent, moved to the trampoline
static constexpr uint8_t kTracePrologue[] = {
	0x48, 0x89, 0x74, 0x24, 0x18,			  // mov [rsp+18h], rsi
	0x55,									  // push rbp
	0x57,									  // push rdi
	0x41, 0x54,								  // push r12
	0x41, 0x55,								  // push r13
	0x41, 0x56,								  // push r14
	0x48, 0x8D, 0xAC, 0x24, 0x60, 0xFF, 0xFF, 0xFF, // lea rbp, [rsp-0A0h]
};

static constexpr size_t kTracePrologueSize = sizeof(kTracePrologue);
static uint8_t* g_pTraceTrampoline = nullptr;
static TracePlayerBBox_t g_pfnTraceOriginal = nullptr;

static void Hook_TracePlayerBBox(void* cache, void* pm, const float* start, const float* end, const float* bounds, void* filter)
{
	TickRecord* tick = t_pTick;

	if (!tick || !g_bRecordTraces.load(std::memory_order_relaxed) || !pm || !start || !end || !bounds)
	{
		g_pfnTraceOriginal(cache, pm, start, end, bounds, filter);
		return;
	}

	TraceRec rec = {};
	rec.returnAddress = (uintptr_t)_ReturnAddress();
	memcpy(rec.start, start, sizeof(rec.start));
	memcpy(rec.end, end, sizeof(rec.end));
	memcpy(rec.mins, bounds, sizeof(rec.mins));
	memcpy(rec.maxs, bounds + 3, sizeof(rec.maxs));

	g_pfnTraceOriginal(cache, pm, start, end, bounds, filter);

	rec.fraction = Read<float>(pm, TraceOffsets::Fraction);
	rec.startSolid = Read<bool>(pm, TraceOffsets::StartInSolid);
	ReadVec(pm, TraceOffsets::EndPos, rec.endPos);
	ReadVec(pm, TraceOffsets::HitNormal, rec.normal);
	ReadVec(pm, TraceOffsets::HitPoint, rec.hitPoint);
	rec.hitOffset = Read<float>(pm, TraceOffsets::HitOffset);
	rec.triangle = Read<int>(pm, TraceOffsets::Triangle);
	rec.contents = Read<uint64_t>(pm, TraceOffsets::Contents);
	rec.rayType = Read<uint8_t>(pm, TraceOffsets::RayType);

	if (const uint8_t* surface = Read<const uint8_t*>(pm, TraceOffsets::SurfaceProperties))
	{
		rec.surfaceHash = Read<uint32_t>(surface, kSurface_NameHash);

		if (const char* name = Read<const char*>(surface, kSurface_Name))
			V_strncpy(rec.surfaceName, name, sizeof(rec.surfaceName));
	}

	if (CEntityInstance* entity = Read<CEntityInstance*>(pm, TraceOffsets::Entity))
	{
		const char* classname = entity->GetClassname();
		const char* name = entity->m_pEntity ? entity->m_pEntity->m_name.String() : nullptr;

		if (name && *name)
			V_snprintf(rec.entity, sizeof(rec.entity), "%s %s", classname, name);
		else
			V_strncpy(rec.entity, classname, sizeof(rec.entity));
	}

	tick->events.emplace_back(EventKind::Trace, (uint32_t)tick->traces.size());
	tick->traces.push_back(rec);
}

static bool InstallTraceHook(uint8_t* target)
{
	if (memcmp(target, kTracePrologue, kTracePrologueSize))
	{
		Warning("[MovementRecorder] TracePlayerBBox starts with unexpected bytes (hooked by something else?), traces will not be recorded\n");
		return false;
	}

	g_pTraceTrampoline = (uint8_t*)VirtualAlloc(nullptr, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);

	if (!g_pTraceTrampoline)
		return false;

	// Trampoline: the moved prologue, then jmp qword ptr [rip+0] back to the rest of the function
	memcpy(g_pTraceTrampoline, target, kTracePrologueSize);
	uint8_t* jump = g_pTraceTrampoline + kTracePrologueSize;
	jump[0] = 0xFF;
	jump[1] = 0x25;
	memset(jump + 2, 0, 4);
	uintptr_t resume = (uintptr_t)(target + kTracePrologueSize);
	memcpy(jump + 6, &resume, sizeof(resume));
	FlushInstructionCache(GetCurrentProcess(), g_pTraceTrampoline, 64);

	g_pfnTraceOriginal = (TracePlayerBBox_t)g_pTraceTrampoline;

	uint8_t patch[kTracePrologueSize];
	memset(patch, 0xCC, sizeof(patch));
	patch[0] = 0xFF;
	patch[1] = 0x25;
	memset(patch + 2, 0, 4);
	uintptr_t hook = (uintptr_t)&Hook_TracePlayerBBox;
	memcpy(patch + 6, &hook, sizeof(hook));

	DWORD oldProtect;
	if (!VirtualProtect(target, kTracePrologueSize, PAGE_EXECUTE_READWRITE, &oldProtect))
	{
		VirtualFree(g_pTraceTrampoline, 0, MEM_RELEASE);
		g_pTraceTrampoline = nullptr;
		return false;
	}

	memcpy(target, patch, sizeof(patch));
	VirtualProtect(target, kTracePrologueSize, oldProtect, &oldProtect);
	FlushInstructionCache(GetCurrentProcess(), target, kTracePrologueSize);
	return true;
}

static void RemoveTraceHook(uint8_t* target)
{
	if (!g_pTraceTrampoline)
		return;

	DWORD oldProtect;
	if (VirtualProtect(target, kTracePrologueSize, PAGE_EXECUTE_READWRITE, &oldProtect))
	{
		memcpy(target, kTracePrologue, kTracePrologueSize);
		VirtualProtect(target, kTracePrologueSize, oldProtect, &oldProtect);
		FlushInstructionCache(GetCurrentProcess(), target, kTracePrologueSize);
	}

	// The trampoline is left allocated: a trace already inside it would otherwise return into freed memory
	g_pTraceTrampoline = nullptr;
}

// ---------------------------------------------------------------------------
// Finding and hooking
// ---------------------------------------------------------------------------

static bool ParseSignature(const char* text, std::vector<int>& bytes)
{
	bytes.clear();

	for (const char* p = text; *p;)
	{
		if (*p == ' ')
		{
			p++;
			continue;
		}

		if (*p == '?')
		{
			bytes.push_back(-1);
			while (*p == '?')
				p++;
			continue;
		}

		char* next;
		long value = strtol(p, &next, 16);

		if (next == p)
			return false;

		bytes.push_back((int)value);
		p = next;
	}

	return !bytes.empty();
}

// Only a signature that matches exactly once is trusted
static uint8_t* FindUnique(uint8_t* start, size_t size, const char* signature)
{
	std::vector<int> bytes;

	if (!ParseSignature(signature, bytes) || size < bytes.size())
		return nullptr;

	uint8_t* found = nullptr;
	size_t last = size - bytes.size();

	for (size_t i = 0; i <= last; i++)
	{
		size_t j = 0;

		while (j < bytes.size() && (bytes[j] < 0 || start[i + j] == bytes[j]))
			j++;

		if (j != bytes.size())
			continue;

		if (found)
			return nullptr;

		found = start + i;
	}

	return found;
}

static KHook::Function<void, void*, void*>* g_pPlayerRunCommandHook = nullptr;
static KHook::Function<void, void*, void*, void*>* g_pDoMovementSubticksHook = nullptr;
static KHook::Function<void, void*, void*>* g_pStartSubtickMoveHook = nullptr;
static KHook::Function<void, void*, void*>* g_pProcessMovementHook = nullptr;
static KHook::Function<void, void*, void*>* g_pFinishSubtickMoveHook = nullptr;

static void ResolveGlobals()
{
	// PlayerRunCommand reads gpGlobals->frametime early on: mov rcx, cs:gpGlobals; movss xmm0, [rcx+34h]
	uint8_t* function = g_pFunctions[Fn_PlayerRunCommand];

	if (!function)
		return;

	uint8_t* load = FindUnique(function, 0x80, "48 8B 0D ? ? ? ? F3 0F 10 41 34");

	if (load)
		g_ppGlobals = (CGlobalVars**)(load + 7 + Read<int32_t>(load, 3));
}

void Init()
{
	CModule server(GAMEBIN, "server");
	g_pServerBase = (uint8_t*)server.m_base;
	g_nServerSize = server.m_size;

	auto dosHeader = (IMAGE_DOS_HEADER*)g_pServerBase;
	auto ntHeaders = (IMAGE_NT_HEADERS64*)(g_pServerBase + dosHeader->e_lfanew);
	g_nServerTimeDateStamp = ntHeaders->FileHeader.TimeDateStamp;

	Section* text = server.GetSection(".text");
	uint8_t* scanStart = text ? (uint8_t*)text->m_pBase : g_pServerBase;
	size_t scanSize = text ? text->m_iSize : g_nServerSize;

	for (int i = 0; i < Fn_Count; i++)
	{
		g_pFunctions[i] = FindUnique(scanStart, scanSize, g_Signatures[i].signature);

		if (!g_pFunctions[i])
			Warning("[MovementRecorder] Could not find %s\n", g_Signatures[i].name);
	}

	g_pCSMovementServicesVTable = server.FindVirtualTable("CCSPlayer_MovementServices");
	ResolveGlobals();
	ResolveFields();

	if (!g_pFunctions[Fn_PlayerRunCommand] || !g_pFunctions[Fn_DoMovementSubticks])
	{
		Warning("[MovementRecorder] The command functions were not found, movement can not be recorded\n");
		return;
	}

	g_pPlayerRunCommandHook = new KHook::Function<void, void*, void*>(&Pre_PlayerRunCommand, &Post_PlayerRunCommand);
	g_pPlayerRunCommandHook->Configure((void*)g_pFunctions[Fn_PlayerRunCommand]);
	g_bHooked[Fn_PlayerRunCommand] = true;

	g_pDoMovementSubticksHook = new KHook::Function<void, void*, void*, void*>(&Pre_DoMovementSubticks, &Post_DoMovementSubticks);
	g_pDoMovementSubticksHook->Configure((void*)g_pFunctions[Fn_DoMovementSubticks]);
	g_bHooked[Fn_DoMovementSubticks] = true;

	if (g_pFunctions[Fn_StartSubtickMove])
	{
		g_pStartSubtickMoveHook = new KHook::Function<void, void*, void*>(&Pre_StartSubtickMove, nullptr);
		g_pStartSubtickMoveHook->Configure((void*)g_pFunctions[Fn_StartSubtickMove]);
		g_bHooked[Fn_StartSubtickMove] = true;
	}

	if (g_pFunctions[Fn_ProcessMovement])
	{
		g_pProcessMovementHook = new KHook::Function<void, void*, void*>(&Pre_ProcessMovement, &Post_ProcessMovement);
		g_pProcessMovementHook->Configure((void*)g_pFunctions[Fn_ProcessMovement]);
		g_bHooked[Fn_ProcessMovement] = true;
	}

	if (g_pFunctions[Fn_FinishSubtickMove])
	{
		g_pFinishSubtickMoveHook = new KHook::Function<void, void*, void*>(nullptr, &Post_FinishSubtickMove);
		g_pFinishSubtickMoveHook->Configure((void*)g_pFunctions[Fn_FinishSubtickMove]);
		g_bHooked[Fn_FinishSubtickMove] = true;
	}

	if (g_pFunctions[Fn_TracePlayerBBox])
		g_bHooked[Fn_TracePlayerBBox] = InstallTraceHook(g_pFunctions[Fn_TracePlayerBBox]);

	META_CONPRINTF("[MovementRecorder] Ready, globals %s, CCSPlayer_MovementServices vtable %s\n", g_ppGlobals ? "found" : "NOT found",
		g_pCSMovementServicesVTable ? "found" : "NOT found");
}

void Shutdown()
{
	g_bRecording = false;

	delete g_pPlayerRunCommandHook;
	delete g_pDoMovementSubticksHook;
	delete g_pStartSubtickMoveHook;
	delete g_pProcessMovementHook;
	delete g_pFinishSubtickMoveHook;
	g_pPlayerRunCommandHook = nullptr;
	g_pDoMovementSubticksHook = nullptr;
	g_pStartSubtickMoveHook = nullptr;
	g_pProcessMovementHook = nullptr;
	g_pFinishSubtickMoveHook = nullptr;

	if (g_bHooked[Fn_TracePlayerBBox])
		RemoveTraceHook(g_pFunctions[Fn_TracePlayerBBox]);

	memset(g_bHooked, 0, sizeof(g_bHooked));
}

// ---------------------------------------------------------------------------
// Control
// ---------------------------------------------------------------------------

void SetRecording(bool recording)
{
	g_bRecording = recording;
}

bool IsRecording()
{
	return g_bRecording;
}

void SetRecordTraces(bool record)
{
	g_bRecordTraces = record;
}

bool IsRecordingTraces()
{
	return g_bRecordTraces;
}

void Clear()
{
	std::lock_guard<std::mutex> lock(g_Recording.mutex);
	g_Recording.ticks.clear();
	g_Recording.vars.clear();
	g_Recording.map.clear();
	g_Recording.lastStates.clear();
	g_Recording.slices = 0;
	g_Recording.traces = 0;
	g_Recording.bytes = 0;
}

Stats GetStats()
{
	std::lock_guard<std::mutex> lock(g_Recording.mutex);
	return {g_Recording.ticks.size(), g_Recording.slices, g_Recording.traces, g_Recording.bytes};
}

std::vector<FunctionStatus> GetFunctionStatus()
{
	std::vector<FunctionStatus> status;

	for (int i = 0; i < Fn_Count; i++)
		status.push_back({g_Signatures[i].name, g_pFunctions[i] ? (uintptr_t)(g_pFunctions[i] - g_pServerBase) : 0, g_bHooked[i]});

	return status;
}

void SetMapName(const char* mapName)
{
	std::lock_guard<std::mutex> lock(g_MapNameMutex);
	g_MapName = mapName ? mapName : "";
}

std::string GetDefaultDirectory()
{
	return (std::filesystem::path(Plat_GetGameDirectory()) / "csgo/addons/CS2ServerGUI/recordings").string();
}

std::string GetDefaultFileName()
{
	std::string map;
	{
		std::lock_guard<std::mutex> lock(g_MapNameMutex);
		map = g_MapName;
	}

	for (char& c : map)
	{
		if (c == '/' || c == '\\' || c == ':')
			c = '_';
	}

	time_t now = time(nullptr);
	tm local;
	localtime_s(&local, &now);

	char stamp[32];
	strftime(stamp, sizeof(stamp), "%Y%m%d_%H%M%S", &local);

	return "movement_" + (map.empty() ? std::string("unknown") : map) + "_" + stamp + ".json";
}

// ---------------------------------------------------------------------------
// Export
// ---------------------------------------------------------------------------

static const char* ButtonName(uint64_t bit)
{
	switch (bit)
	{
	case 0: return "None";
	case 1ull << 0: return "Attack";
	case 1ull << 1: return "Jump";
	case 1ull << 2: return "Duck";
	case 1ull << 3: return "Forward";
	case 1ull << 4: return "Back";
	case 1ull << 5: return "Use";
	case 1ull << 7: return "TurnLeft";
	case 1ull << 8: return "TurnRight";
	case 1ull << 9: return "MoveLeft";
	case 1ull << 10: return "MoveRight";
	case 1ull << 11: return "Attack2";
	case 1ull << 13: return "Reload";
	case 1ull << 16: return "Speed";
	default: return nullptr;
	}
}

// The names the reimplementation's InputButtons enum prints and parses; unnamed bits as their decimal value
static std::string ButtonNames(uint64_t buttons)
{
	if (!buttons)
		return "None";

	std::string names;
	uint64_t unnamed = 0;

	for (int bit = 0; bit < 64; bit++)
	{
		uint64_t mask = 1ull << bit;

		if (!(buttons & mask))
			continue;

		if (const char* name = ButtonName(mask))
		{
			if (!names.empty())
				names += ", ";
			names += name;
		}
		else
		{
			unnamed |= mask;
		}
	}

	if (unnamed)
	{
		if (!names.empty())
			names += ", ";
		names += std::to_string(unnamed);
	}

	return names;
}

static const char* MoveTypeName(uint8_t moveType)
{
	static const char* names[] = {"None", "Obsolete", "Walk", "Fly", "FlyGravity", "VPhysics", "Push", "NoClip", "Observer", "Ladder", "Custom"};
	return moveType < sizeof(names) / sizeof(names[0]) ? names[moveType] : "Unknown";
}

// The C# hull, the recording format expects it
static void WriteHull(JsonWriter& w)
{
	static const float mins[3] = {-16.0f, -16.0f, 0.0f};
	static const float maxs[3] = {16.0f, 16.0f, 72.0f};
	static const float duckMaxs[3] = {16.0f, 16.0f, 54.0f};

	w.BeginObject("hull");
	w.Vec3("mins", mins);
	w.Vec3("maxs", maxs);
	w.Vec3("duckMins", mins);
	w.Vec3("duckMaxs", duckMaxs);
	w.EndObject();
}

static void WriteMoveData(JsonWriter& w, const char* name, const MoveDataSnap& mv)
{
	w.BeginObject(name);
	w.UInt("flags", mv.flags);
	w.UInt("playerHandle", mv.playerHandle);
	w.Vec3("absViewAngles", mv.absViewAngles);
	w.Vec3("viewAngles", mv.viewAngles);
	w.Vec3("angles", mv.angles);
	w.Vec3("oldAngles", mv.oldAngles);
	w.Vec3("lastMovementImpulses", mv.lastMovementImpulses);
	w.Float("forwardMove", mv.forwardMove);
	w.Float("sideMove", mv.sideMove);
	w.Float("upMove", mv.upMove);
	w.Vec3("absOrigin", mv.absOrigin);
	w.Vec3("velocity", mv.velocity);
	w.Vec3("continuousAcceleration", mv.continuousAcceleration);
	w.Vec3("frameVelocityDelta", mv.frameVelocityDelta);
	w.Vec3("outWishVel", mv.outWishVel);
	w.Vec2("walkWishVel", mv.walkWishVel);
	w.Float("maxSpeed", mv.maxSpeed);
	w.Float("clientMaxSpeed", mv.clientMaxSpeed);
	w.Float("frictionDecel", mv.frictionDecel);
	w.Float("preAirMovePosZ", mv.preAirMovePosZ);
	w.Float("preAirMoveVelZ", mv.preAirMoveVelZ);
	w.Float("preAirMoveAccelZ", mv.preAirMoveAccelZ);
	w.Bool("inAir", mv.inAir);
	w.Bool("gameCodeMovedPlayer", mv.gameCodeMovedPlayer);
	w.Bool("useFrictionStashedSpeed", mv.useFrictionStashedSpeed);
	w.Vec3("collisionNormal", mv.collisionNormal);
	w.Vec3("groundNormal", mv.groundNormal);
	w.Int("tickCount", mv.tickCount);
	w.Int("targetTick", mv.targetTick);
	w.Float("subtickStartFraction", mv.subtickStartFraction);
	w.Float("subtickEndFraction", mv.subtickEndFraction);
	w.Int("subtickMoveCount", mv.subtickMoveCount);
	w.Int("attackSubtickMoveCount", mv.attackSubtickMoveCount);
	w.Float("attackSubtickEndFraction", mv.attackSubtickEndFraction);
	w.Int("touchCount", mv.touchCount);
	w.EndObject();
}

static void WriteGlobals(JsonWriter& w, const GlobalsSnap& globals)
{
	if (!globals.valid)
		return;

	w.Float("curTime", globals.curtime);
	w.Float("frameTime", globals.frametime);
	w.Int("tickCount", globals.tickcount);
	w.Float("subtickFraction", globals.subtickFraction);
}

// The C# MovementState, plus the server-side origin and velocity representations. A slice's state takes its
// origin and velocity from the move data, as the reimplementation's does.
static void WriteMovementState(JsonWriter& w, const char* name, const StateSnap& snap, const MoveDataSnap* mv = nullptr)
{
	float absOrigin[3], absVelocity[3], baseVelocity[3];
	SnapGetVec(snap, g_iAbsOrigin, absOrigin);
	SnapGetVec(snap, g_iAbsVelocity, absVelocity);
	SnapGetVec(snap, g_iBaseVelocity, baseVelocity);

	uint32_t flags = SnapGet<uint32_t>(snap, g_iFlags);
	float waterLevelFraction = SnapGet<float>(snap, g_iWaterLevel);

	w.BeginObject(name);
	w.Vec3("origin", mv ? mv->absOrigin : absOrigin);
	w.Vec3("velocity", mv ? mv->velocity : absVelocity);
	w.Vec3("baseVelocity", baseVelocity);
	w.Bool("onGround", (flags & 1) != 0);
	w.UInt("flags", flags);
	w.Str("moveType", MoveTypeName(SnapGet<uint8_t>(snap, g_iMoveType)));
	w.Str("actualMoveType", MoveTypeName(SnapGet<uint8_t>(snap, g_iActualMoveType)));

	if (snap.groundEntity[0])
		w.Str("groundEntity", snap.groundEntity);

	w.Bool("ducked", SnapGet<bool>(snap, g_iDucked));
	w.Float("duckAmount", SnapGet<float>(snap, g_iDuckAmount));
	w.Float("surfaceFriction", SnapGet<float>(snap, g_iSurfaceFriction));
	w.Int("waterLevel", waterLevelFraction != 0.0f ? (int)(waterLevelFraction * 4.0f + 1.0f) : 0);

	if (mv)
	{
		w.Vec3("pawnOrigin", absOrigin);
		w.Vec3("pawnVelocity", absVelocity);
	}

	float networkOrigin[3];
	if (SnapGetVec(snap, g_iNetworkOrigin, networkOrigin))
		w.Vec3("networkOrigin", networkOrigin);

	if (g_iCellX >= 0 && snap.present.test(g_iCellX))
	{
		w.BeginArray("networkOriginCells");
		for (int axis = 0; axis < 3; axis++)
			w.UInt(SnapGet<uint16_t>(snap, g_iCellX + axis));
		w.EndArray();

		w.BeginArray("networkOriginOffsets");
		for (int axis = 0; axis < 3; axis++)
			w.Float(SnapGet<float>(snap, g_iOffsetX + axis));
		w.EndArray();
	}

	if (g_iNetVelocityX >= 0 && snap.present.test(g_iNetVelocityX))
	{
		w.BeginArray("networkVelocity");
		for (int axis = 0; axis < 3; axis++)
			w.Float(SnapGet<float>(snap, g_iNetVelocityX + axis));
		w.EndArray();
	}

	w.EndObject();
}

// Every captured field by name; the names the reimplementation's VisitState uses read straight back into it
static void WriteFullState(JsonWriter& w, const char* name, const StateSnap& snap, const GlobalsSnap& globals)
{
	w.BeginObject(name);

	for (int i = 0; i < kFieldCount; i++)
	{
		if (g_LayoutOffsets[snap.layout][i] < 0 || !snap.present.test(i))
			continue;

		const uint8_t* value = snap.data.data() + g_LayoutOffsets[snap.layout][i];
		w.Key(g_Fields[i].json);

		switch (g_Fields[i].type)
		{
		case FieldType::F32: w.Float(Read<float>(value, 0)); break;
		case FieldType::I32: w.Int(Read<int32_t>(value, 0)); break;
		case FieldType::U32: w.UInt(Read<uint32_t>(value, 0)); break;
		case FieldType::U64: w.UInt(Read<uint64_t>(value, 0)); break;
		case FieldType::U16: w.UInt(Read<uint16_t>(value, 0)); break;
		case FieldType::U8: w.UInt(Read<uint8_t>(value, 0)); break;
		case FieldType::Bool: w.Bool(Read<bool>(value, 0)); break;
		case FieldType::Vec2: w.Vec2((const float*)value); break;
		case FieldType::Vec3: w.Vec3((const float*)value); break;
		}
	}

	w.Bool("NoClip", SnapGet<uint8_t>(snap, g_iActualMoveType) == 7);

	if (snap.groundEntity[0])
		w.Str("GroundEntity", snap.groundEntity);

	if (globals.valid)
	{
		w.Float("CurTime", globals.curtime);
		w.Float("FrameTime", globals.frametime);
		w.Int("TickCount", globals.tickcount);
	}

	w.EndObject();
}

static const char* FunctionNameAt(uintptr_t address, uintptr_t& functionRva)
{
	functionRva = 0;

	if (address < (uintptr_t)g_pServerBase || address >= (uintptr_t)g_pServerBase + g_nServerSize)
		return nullptr;

	DWORD64 imageBase = 0;
	PRUNTIME_FUNCTION entry = RtlLookupFunctionEntry(address, &imageBase, nullptr);

	// Cold parts of a function chain back to the entry of the function they belong to
	for (int depth = 0; entry && depth < 8; depth++)
	{
		const uint8_t* unwind = (const uint8_t*)(imageBase + entry->UnwindData);

		if (!((unwind[0] >> 3) & UNW_FLAG_CHAININFO))
			break;

		int codes = (unwind[2] + 1) & ~1;
		entry = (PRUNTIME_FUNCTION)(unwind + 4 + codes * 2);
	}

	if (!entry)
		return nullptr;

	uint8_t* start = (uint8_t*)(imageBase + entry->BeginAddress);
	functionRva = start - g_pServerBase;

	for (int i = 0; i < Fn_Count; i++)
	{
		if (g_pFunctions[i] == start)
			return g_Signatures[i].name;
	}

	return nullptr;
}

static void WriteTrace(JsonWriter& w, const TraceRec& trace)
{
	uintptr_t functionRva;
	const char* caller = FunctionNameAt(trace.returnAddress, functionRva);
	bool hit = trace.fraction < 1.0f || trace.startSolid;

	char unknown[32];
	if (!caller)
	{
		V_snprintf(unknown, sizeof(unknown), "server+0x%llX", (unsigned long long)functionRva);
		caller = unknown;
	}

	w.Str("type", "trace");
	w.Str("caller", caller);
	w.UInt("callerRva", functionRva);
	w.UInt("returnRva", trace.returnAddress - (uintptr_t)g_pServerBase);
	w.Vec3("start", trace.start);
	w.Vec3("end", trace.end);
	w.Vec3("mins", trace.mins);
	w.Vec3("maxs", trace.maxs);
	w.Float("fraction", trace.fraction);
	w.Bool("startSolid", trace.startSolid);
	w.Bool("hit", hit);
	w.Vec3("endPos", trace.endPos);

	if (!hit)
		return;

	w.Vec3("normal", trace.normal);
	w.Vec3("hitPoint", trace.hitPoint);
	w.Float("hitOffset", trace.hitOffset);
	w.Int("triangle", trace.triangle);
	w.UInt("contents", trace.contents);
	w.UInt("rayType", trace.rayType);
	w.UInt("surface", trace.surfaceHash);

	if (trace.surfaceName[0])
		w.Str("surfaceName", trace.surfaceName);

	if (trace.entity[0])
		w.Str("entity", trace.entity);
}

static void WriteQAngle(JsonWriter& w, const char* name, const CMsgQAngle& angle)
{
	float v[3] = {angle.x(), angle.y(), angle.z()};
	w.Vec3(name, v);
}

static void WriteMsgVector(JsonWriter& w, const char* name, const CMsgVector& vector)
{
	float v[3] = {vector.x(), vector.y(), vector.z()};
	w.Vec3(name, v);
}

static void WriteInterpolation(JsonWriter& w, const char* name, const CSGOInterpolationInfoPB& info)
{
	w.BeginObject(name);
	w.Int("srcTick", info.src_tick());
	w.Int("dstTick", info.dst_tick());
	w.Float("frac", info.frac());
	w.EndObject();
}

static void WriteCommand(JsonWriter& w, const TickRecord& tick)
{
	CSGOUserCmdPB cmd;
	bool parsed = tick.commandSerialized && cmd.ParseFromString(tick.commandBytes);
	const CBaseUserCmdPB& base = cmd.base();

	w.BeginObject("command");
	w.Int("number", tick.commandNumber);
	WriteQAngle(w, "viewAngles", base.viewangles());
	w.Float("forwardMove", base.forwardmove());
	w.Float("leftMove", base.leftmove());
	w.Float("upMove", base.upmove());

	uint64_t held = base.buttons_pb().buttonstate1();
	w.Str("heldNames", ButtonNames(held).c_str());
	w.UInt("held", held);
	w.UInt("changed", base.buttons_pb().buttonstate2());
	w.UInt("changedTwice", base.buttons_pb().buttonstate3());

	w.BeginArray("subtickMoves");
	for (const CSubtickMoveStep& move : base.subtick_moves())
	{
		w.BeginObject();
		w.Float("when", move.when());

		const char* name = ButtonName(move.button());
		w.Str("button", name ? name : std::to_string(move.button()).c_str());
		w.Bool("pressed", move.pressed());
		w.Float("analogForwardDelta", move.analog_forward_delta());
		w.Float("analogLeftDelta", move.analog_left_delta());
		w.Float("pitchDelta", move.pitch_delta());
		w.Float("yawDelta", move.yaw_delta());
		w.EndObject();
	}
	w.EndArray();

	w.Bool("late", tick.late);

	if (!parsed)
	{
		w.Str("protobufError", tick.commandSerialized ? "parse failed" : "serialize failed");
		w.EndObject();
		return;
	}

	w.Int("legacyCommandNumber", base.legacy_command_number());
	w.Int("clientTick", base.client_tick());
	w.UInt("predictionOffsetTicksX256", base.prediction_offset_ticks_x256());
	w.Int("impulse", base.impulse());
	w.Int("weaponSelect", base.weaponselect());
	w.Int("randomSeed", base.random_seed());
	w.Int("mouseDx", base.mousedx());
	w.Int("mouseDy", base.mousedy());
	w.UInt("pawnEntityHandle", base.pawn_entity_handle());
	w.UInt("consumedServerAngleChanges", base.consumed_server_angle_changes());
	w.Int("cmdFlags", base.cmd_flags());
	w.Int("attack1StartHistoryIndex", cmd.attack1_start_history_index());
	w.Int("attack2StartHistoryIndex", cmd.attack2_start_history_index());
	w.Bool("leftHandDesired", cmd.left_hand_desired());

	w.BeginArray("inputHistory");
	for (const CSGOInputHistoryEntryPB& entry : cmd.input_history())
	{
		w.BeginObject();
		WriteQAngle(w, "viewAngles", entry.view_angles());
		w.Int("renderTickCount", entry.render_tick_count());
		w.Float("renderTickFraction", entry.render_tick_fraction());
		w.Int("playerTickCount", entry.player_tick_count());
		w.Float("playerTickFraction", entry.player_tick_fraction());
		w.Int("frameNumber", entry.frame_number());
		w.Int("targetEntIndex", entry.target_ent_index());

		if (entry.has_cl_interp())
			w.Float("clInterpFrac", entry.cl_interp().frac());
		if (entry.has_sv_interp0())
			WriteInterpolation(w, "svInterp0", entry.sv_interp0());
		if (entry.has_sv_interp1())
			WriteInterpolation(w, "svInterp1", entry.sv_interp1());
		if (entry.has_player_interp())
			WriteInterpolation(w, "playerInterp", entry.player_interp());
		if (entry.has_shoot_position())
			WriteMsgVector(w, "shootPosition", entry.shoot_position());

		w.EndObject();
	}
	w.EndArray();

	w.Str("protobuf", cmd.ShortDebugString().c_str());
	w.EndObject();
}

static int TickNumber(const TickRecord& tick)
{
	if (tick.setupGlobals.valid)
		return tick.setupGlobals.tickcount;

	return (int)SnapGet<uint32_t>(tick.after, FieldIndex("TickBase"));
}

static void WriteTick(JsonWriter& w, const TickRecord& tick)
{
	w.BeginObject();
	w.Int("tick", TickNumber(tick));
	w.Int("player", tick.playerIndex);

	if (!tick.markers.empty())
	{
		w.BeginArray("markers");
		for (const std::string& marker : tick.markers)
			w.Str(marker.c_str());
		w.EndArray();
	}

	WriteCommand(w, tick);
	WriteMovementState(w, "before", tick.before);
	WriteMovementState(w, "after", tick.after);
	WriteFullState(w, "state", tick.before, tick.globalsBefore);
	WriteFullState(w, "stateAfter", tick.after, GlobalsSnap());

	if (tick.hasSetup)
	{
		w.BeginObject("setup");
		WriteGlobals(w, tick.setupGlobals);
		WriteMoveData(w, "mv", tick.setupMv);

		w.BeginArray("subtickMoves");
		for (const SubtickMoveSnap& move : tick.setupMoves)
		{
			w.BeginObject();
			w.Float("when", move.when);

			const char* name = ButtonName(move.button);
			w.Str("button", name ? name : std::to_string(move.button).c_str());

			if (move.button)
			{
				w.Bool("pressed", move.data[0] != 0);
			}
			else
			{
				w.Float("analogForwardDelta", Read<float>(move.data, 0));
				w.Float("analogLeftDelta", Read<float>(move.data, 4));
			}

			w.Float("pitch", move.pitch);
			w.Float("yaw", move.yaw);
			w.EndObject();
		}
		w.EndArray();
		w.EndObject();
	}

	w.BeginArray("events");
	for (const auto& [kind, index] : tick.events)
	{
		w.BeginObject();

		switch (kind)
		{
		case EventKind::Slice:
		{
			const SliceRec& slice = tick.slices[index];
			w.Str("type", "slice");
			w.Float("start", slice.mvStart.subtickStartFraction);
			w.Float("end", slice.mvStart.subtickEndFraction);
			WriteGlobals(w, slice.globals);
			w.Float("forwardMove", slice.mvStart.forwardMove);
			w.Float("sideMove", slice.mvStart.sideMove);
			w.Vec3("viewAngles", slice.mvStart.viewAngles);
			w.Str("buttons", ButtonNames(SnapGet<uint64_t>(slice.state, g_iButtonsHeld)).c_str());
			WriteMovementState(w, "state", slice.state, &slice.mvStart);
			WriteMoveData(w, "mv", slice.mvStart);

			if (slice.hasProcessStart)
				WriteMoveData(w, "mvProcessStart", slice.mvProcessStart);
			if (slice.hasProcessEnd)
				WriteMoveData(w, "mvProcessEnd", slice.mvProcessEnd);
			break;
		}
		case EventKind::SliceEnd:
		{
			const SliceEndRec& end = tick.sliceEnds[index];
			w.Str("type", "sliceEnd");
			WriteMovementState(w, "state", end.state, &end.mv);
			WriteMoveData(w, "mv", end.mv);
			break;
		}
		case EventKind::Trace:
			WriteTrace(w, tick.traces[index]);
			break;
		}

		w.EndObject();
	}
	w.EndArray();

	if (tick.hasFinal)
		WriteMoveData(w, "final", tick.finalMv);

	w.EndObject();
}

static float Length(const float* v)
{
	return sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}

bool Save(const std::string& path, std::string& error)
{
	std::vector<std::shared_ptr<const TickRecord>> ticks;
	std::vector<VarValue> vars;
	std::string map;

	{
		std::lock_guard<std::mutex> lock(g_Recording.mutex);
		ticks = g_Recording.ticks;
		vars = g_Recording.vars;
		map = g_Recording.map;
	}

	if (ticks.empty())
	{
		error = "Nothing recorded";
		return false;
	}

	std::error_code ec;
	std::filesystem::path filePath(path);

	if (filePath.has_parent_path())
		std::filesystem::create_directories(filePath.parent_path(), ec);

	FILE* file = nullptr;
	if (fopen_s(&file, path.c_str(), "wb") || !file)
	{
		error = "Could not open " + path;
		return false;
	}

	std::vector<char> buffer(1 << 20);
	setvbuf(file, buffer.data(), _IOFBF, buffer.size());

	JsonWriter w(file);
	w.BeginObject();
	w.Str("format", "s2v-movement-recording");
	w.Int("version", 1);
	w.Str("source", "cs2-server");
	w.Str("map", map.c_str());
	w.Float("tickInterval", 1.0f / 64.0f);

	w.BeginObject("vars");
	for (const VarValue& var : vars)
	{
		if (var.isBool)
			w.Bool(var.name, var.boolValue);
		else
			w.Float(var.name, var.floatValue);
	}
	w.EndObject();

	WriteHull(w);

	w.BeginObject("server");
	w.Str("module", "server.dll");
	w.UInt("timeDateStamp", g_nServerTimeDateStamp);
	w.UInt("imageSize", g_nServerSize);
	w.BeginObject("functions");
	for (int i = 0; i < Fn_Count; i++)
	{
		if (g_pFunctions[i])
			w.UInt(g_Signatures[i].name, g_pFunctions[i] - g_pServerBase);
		else
			w.Null(g_Signatures[i].name);
	}
	w.EndObject();
	w.EndObject();

	// Ticks that lost a lot of speed, where to start looking
	w.BeginArray("speedLosses");
	for (const auto& tick : ticks)
	{
		float before[3], after[3];
		SnapGetVec(tick->before, g_iAbsVelocity, before);
		SnapGetVec(tick->after, g_iAbsVelocity, after);

		float speedBefore = Length(before);
		float speedAfter = Length(after);

		if (speedBefore - speedAfter < 100.0f || speedAfter > speedBefore * 0.75f)
			continue;

		w.BeginObject();
		w.Int("tick", TickNumber(*tick));
		w.Float("speedBefore", speedBefore);
		w.Float("speedAfter", speedAfter);
		w.Bool("onGroundAfter", (SnapGet<uint32_t>(tick->after, g_iFlags) & 1) != 0);
		w.EndObject();
	}
	w.EndArray();

	w.BeginArray("ticks");
	for (const auto& tick : ticks)
		WriteTick(w, *tick);
	w.EndArray();

	w.EndObject();

	bool failed = ferror(file) != 0;
	fclose(file);

	if (failed)
	{
		error = "Write failed for " + path;
		return false;
	}

	return true;
}

} // namespace Movement::Recorder
