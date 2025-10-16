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

#include <stdio.h>
#include "extension.h"
#include <iserver.h>
#include <funchook.h>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <vector>
#include "imgui/main.h"
#include <thread>
#include <schemasystem/schemasystem.h>
#include <entity2/entitysystem.h>
#include "interfaces.h"
#include "networkstringtabledefs.h"
#include "igameeventsystem.h"
#include "imgui/panels/eventlogger/eventlogger.h"
#include "../protobufs/generated/netmessages.pb.h"
#include "../protobufs/generated/usercmd.pb.h"
#include "../protobufs/generated/cs_usercmd.pb.h"
#include "networksystem/inetworkmessages.h"
#include "cs2_sdk/serversideclient.h"
#include "utils/module.h"

#ifdef _WIN32
#define ROOTBIN "/bin/win64/"
#define GAMEBIN "/csgo/bin/win64/"
#else
#define ROOTBIN "/bin/linuxsteamrt64/"
#define GAMEBIN "/csgo/bin/linuxsteamrt64/"
#endif

CS2ServerGUI g_CS2ServerGUI;
std::thread g_thread;

typedef bool (*FilterMessage_t)(INetworkMessageProcessingPreFilterCustom* player, CNetMessage* pData, void* pNetChan);
int g_iFilterMessage;
int g_iSendNetMessage;

/*
SH_DECL_HOOK8_void(IGameEventSystem, PostEventAbstract, SH_NOATTRIB, 0, CSplitScreenSlot, bool, int, const uint64*,
	INetworkMessageInternal*, const CNetMessage*, unsigned long, NetChannelBufType_t);
*/
SH_DECL_MANUALHOOK2(FilterMessage, 0, 0, 0, bool, CNetMessage*, void*);
SH_DECL_MANUALHOOK2(SendNetMessage, 15, 0, 0, bool, CNetMessage*, NetChannelBufType_t);

CGameEntitySystem* GameEntitySystem()
{
#ifdef WIN32
	static int offset = 88;
#else
	static int offset = 80;
#endif
	return *reinterpret_cast<CGameEntitySystem**>((uintptr_t)(g_pGameResourceServiceServer)+offset);
}

// Should only be called within the active game loop (i e map should be loaded and active)
// otherwise that'll be nullptr!
CGlobalVars* GetGameGlobals()
{
	INetworkGameServer* server = g_pNetworkServerService->GetIGameServer();

	if (!server)
		return nullptr;

	return g_pNetworkServerService->GetIGameServer()->GetGlobals();
}

CON_COMMAND_F(gui, "Opens server GUI", FCVAR_LINKED_CONCOMMAND | FCVAR_SPONLY)
{
	if (!GUI::g_GUICtx.m_bIsGUIOpen)
	{
		g_thread = std::thread(GUI::InitializeGUI);
		g_thread.detach();
	}
}

int aa = 0;

template <typename T>
bool ReadPBFromBuffer(bf_read& buffer, T& pb)
{
	auto size = buffer.ReadVarInt32();

	if (size < 0 || size > 262140)
	{
		return false;
	}

	if (size > buffer.GetNumBytesLeft())
	{
		return false;
	}

	if ((buffer.GetNumBitsRead() % 8) == 0)
	{
		bool parseResult = pb.ParseFromArray(buffer.GetBasePointer() + buffer.GetNumBytesRead(), size);
		buffer.SeekRelative(size * 8);

		return true;
	}

	void* parseBuffer = stackalloc(size);
	if (!buffer.ReadBytes(parseBuffer, size))
	{
		return false;
	}

	if (!pb.ParseFromArray(parseBuffer, size))
	{
		return false;
	}

	return true;
}

bool CS2ServerGUI::Hook_FilterMessage(CNetMessage* pData, void* pNetChan)
{
	if (!GUI::g_GUICtx.m_WindowStates.m_bEventLogger)
	{
		RETURN_META_VALUE(MRES_IGNORED, true);
	}

	auto player = META_IFACEPTR(INetworkMessageProcessingPreFilterCustom);
	
	NetMessageInfo_t* info = pData->GetNetMessage()->GetNetMessageInfo();
	if (info)
	{
		if (info->m_MessageId == CLC_Messages::clc_Move)
		{
			auto msg = pData->ToPB<CCLCMsg_Move>();

			if (msg->has_data())
			{
				bf_read buffer(msg->data().data(), msg->data().size());
				CSGOUserCmdPB userCmd;
				if (ReadPBFromBuffer(buffer, userCmd))
					GUI::EventLogger::AddEventLog(std::string(info->m_pBinding->GetName()), std::string(userCmd.DebugString().c_str()), true, std::string(player->GetClientName()));
			}
		}
		else
		{
			CUtlString str;
			info->m_pBinding->ToString(pData, str);

			GUI::EventLogger::AddEventLog(std::string(info->m_pBinding->GetName()), std::string(str.String()), true, std::string(player->GetClientName()));
		}
	}

	RETURN_META_VALUE(MRES_IGNORED, true);
}

bool CS2ServerGUI::Hook_SendNetMessage(CNetMessage* pData, NetChannelBufType_t bufType)
{
	if (GUI::g_GUICtx.m_WindowStates.m_bEventLogger)
	{
		INetworkMessageProcessingPreFilterCustom* client = META_IFACEPTR(INetworkMessageProcessingPreFilterCustom);

		NetMessageInfo_t* info = pData->GetNetMessage()->GetNetMessageInfo();
		if (info)
		{
			CUtlString str;
			info->m_pBinding->ToString(pData, str);

			GUI::EventLogger::AddEventLog(std::string(info->m_pBinding->GetName()), std::string(str.String()), false);
		}
	}

	RETURN_META_VALUE(MRES_IGNORED, true);
}

// legacy hook for outgoing messages
/*
void CS2ServerGUI::Hook_PostEvent(CSplitScreenSlot nSlot, bool bLocalOnly, int nClientCount, const uint64* clients,
	INetworkMessageInternal* pEvent, const CNetMessage* pData, unsigned long nSize, NetChannelBufType_t bufType)
{
	if (!GUI::g_GUICtx.m_WindowStates.m_bEventLogger)
		return;
	
	NetMessageInfo_t* info = pEvent->GetNetMessageInfo();
	if (info)
	{
		CUtlString str;
		info->m_pBinding->ToString(pData, str);

		GUI::EventLogger::AddEventLog(std::string(info->m_pBinding->GetName()), std::string(str.String()), false);
	}
}
*/

void SetupHook()
{
	CModule engineModule(ROOTBIN, "engine2");
	
#ifdef _WIN32
	auto networkMessageProcessingPreFilterCustomVTable = engineModule.FindVirtualTable("CServerSideClient", 8);
	auto serverSideClientVTable = engineModule.FindVirtualTable("CServerSideClient", 0);
#else
	auto serverSideClientVTable = engineModule.FindVirtualTable("CServerSideClient", -64);
	auto serverSideClientVTable = nullptr;
#endif

	if (!networkMessageProcessingPreFilterCustomVTable)
	{
		META_CONPRINTF("[CS2ServerGUI] Failed to find networkMessageProcessingPreFilterCustomVTable\n");
		return;
	}
	if (!serverSideClientVTable)
	{
		META_CONPRINTF("[CS2ServerGUI] Failed to find networkMessageProcessingPreFilterCustomVTable\n");
		return;
	}

	g_iFilterMessage = SH_ADD_MANUALDVPHOOK(FilterMessage, networkMessageProcessingPreFilterCustomVTable, SH_MEMBER(&g_CS2ServerGUI, &CS2ServerGUI::Hook_FilterMessage), false);
	g_iSendNetMessage = SH_ADD_MANUALDVPHOOK(SendNetMessage, serverSideClientVTable, SH_MEMBER(&g_CS2ServerGUI, &CS2ServerGUI::Hook_SendNetMessage), false);

	return;
}

PLUGIN_EXPOSE(CS2ServerGUI, g_CS2ServerGUI);
bool CS2ServerGUI::Load(PluginId id, ISmmAPI *ismm, char *error, size_t maxlen, bool late)
{
	PLUGIN_SAVEVARS();
	m_config.SetPath((std::filesystem::path(Plat_GetGameDirectory()) / "csgo/addons/CS2ServerGUI/config.json"));
	m_config.LoadConfig();

	GET_V_IFACE_CURRENT(GetEngineFactory, Interfaces::engine, IVEngineServer, INTERFACEVERSION_VENGINESERVER);
	GET_V_IFACE_CURRENT(GetEngineFactory, Interfaces::icvar, ICvar, CVAR_INTERFACE_VERSION);
	GET_V_IFACE_ANY(GetServerFactory, Interfaces::server, IServerGameDLL, INTERFACEVERSION_SERVERGAMEDLL);
	GET_V_IFACE_ANY(GetServerFactory, Interfaces::gameclients, IServerGameClients, INTERFACEVERSION_SERVERGAMECLIENTS);
	GET_V_IFACE_ANY(GetEngineFactory, g_pNetworkServerService, INetworkServerService, NETWORKSERVERSERVICE_INTERFACE_VERSION);
	GET_V_IFACE_ANY(GetEngineFactory, g_pSchemaSystem, CSchemaSystem, SCHEMASYSTEM_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetEngineFactory, g_pGameResourceServiceServer, IGameResourceService, GAMERESOURCESERVICESERVER_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetEngineFactory, Interfaces::networkStringTableContainerServer, INetworkStringTableContainer, SOURCE2ENGINETOSERVERSTRINGTABLE_INTERFACE_VERSION);
	GET_V_IFACE_ANY(GetEngineFactory, Interfaces::gameEventSystem, IGameEventSystem, GAMEEVENTSYSTEM_INTERFACE_VERSION);
	GET_V_IFACE_ANY(GetEngineFactory, Interfaces::networkMessages, INetworkMessages, NETWORKMESSAGES_INTERFACE_VERSION);
	g_SMAPI->AddListener( this, this );

	//SH_ADD_HOOK_MEMFUNC(IGameEventSystem, PostEventAbstract, Interfaces::gameEventSystem, this, &CS2ServerGUI::Hook_PostEvent, false);

	SetupHook();

	g_pCVar = Interfaces::icvar;
	META_CONVAR_REGISTER(FCVAR_RELEASE | FCVAR_CLIENT_CAN_EXECUTE | FCVAR_GAMEDLL);

	// InitializeGUI on another thread
	g_thread = std::thread(GUI::InitializeGUI);
	g_thread.detach();
	return true;
}

bool CS2ServerGUI::Unload(char *error, size_t maxlen)
{
	//SH_REMOVE_HOOK_MEMFUNC(IGameEventSystem, PostEventAbstract, Interfaces::gameEventSystem, this, &CS2ServerGUI::Hook_PostEvent, false);
	SH_REMOVE_HOOK_ID(g_iFilterMessage);
	SH_REMOVE_HOOK_ID(g_iSendNetMessage);
	return true;
}

void CS2ServerGUI::AllPluginsLoaded()
{
}

void CS2ServerGUI::OnLevelInit( char const *pMapName,
									 char const *pMapEntities,
									 char const *pOldLevel,
									 char const *pLandmarkName,
									 bool loadGame,
									 bool background )
{
	META_CONPRINTF("OnLevelInit(%s)\n", pMapName);
}

void CS2ServerGUI::OnLevelShutdown()
{
	META_CONPRINTF("OnLevelShutdown()\n");
}

bool CS2ServerGUI::Pause(char *error, size_t maxlen)
{
	return true;
}

bool CS2ServerGUI::Unpause(char *error, size_t maxlen)
{
	return true;
}

const char *CS2ServerGUI::GetLicense()
{
	return "GPLv3";
}

const char *CS2ServerGUI::GetVersion()
{
	return "1.0.7";
}

const char *CS2ServerGUI::GetDate()
{
	return __DATE__;
}

const char *CS2ServerGUI::GetLogTag()
{
	return "SERVERGUI";
}

const char *CS2ServerGUI::GetAuthor()
{
	return "Poggu";
}

const char *CS2ServerGUI::GetDescription()
{
	return "Server GUI using Dear ImGui";
}

const char *CS2ServerGUI::GetName()
{
	return "CS2ServerGUI";
}

const char *CS2ServerGUI::GetURL()
{
	return "https://poggu.me";
}
