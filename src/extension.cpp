/**
 * =============================================================================
 * CleanerCS2
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
#include "utils/module.h"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <vector>
#include "imgui/gui.h"
#include <thread>
#include <schemasystem/schemasystem.h>
#include <entity2/entitysystem.h>

#ifdef _WIN32
#define ROOTBIN "/bin/win64/"
#define GAMEBIN "/csgo/bin/win64/"
#else
#define ROOTBIN "/bin/linuxsteamrt64/"
#define GAMEBIN "/csgo/bin/linuxsteamrt64/"
#endif

CS2ServerGUI g_CS2ServerGUI;
IServerGameDLL *server = NULL;
IServerGameClients *gameclients = NULL;
IVEngineServer *engine = NULL;
IGameEventManager2 *gameevents = NULL;
ICvar *icvar = NULL;
CSchemaSystem *g_pSchemaSystem2 = NULL;

std::thread g_thread;

CGameEntitySystem* GameEntitySystem()
{
#ifdef WIN32
	static int offset = 88;
#else
	static int offset = 80;
#endif
	return *reinterpret_cast<CGameEntitySystem**>((uintptr_t)(g_pGameResourceServiceServer)+offset);
}

PLUGIN_EXPOSE(CS2ServerGUI, g_CS2ServerGUI);
bool CS2ServerGUI::Load(PluginId id, ISmmAPI *ismm, char *error, size_t maxlen, bool late)
{
	PLUGIN_SAVEVARS();

	GET_V_IFACE_CURRENT(GetEngineFactory, engine, IVEngineServer, INTERFACEVERSION_VENGINESERVER);
	GET_V_IFACE_CURRENT(GetEngineFactory, icvar, ICvar, CVAR_INTERFACE_VERSION);
	GET_V_IFACE_ANY(GetServerFactory, server, IServerGameDLL, INTERFACEVERSION_SERVERGAMEDLL);
	GET_V_IFACE_ANY(GetServerFactory, gameclients, IServerGameClients, INTERFACEVERSION_SERVERGAMECLIENTS);
	GET_V_IFACE_ANY(GetEngineFactory, g_pNetworkServerService, INetworkServerService, NETWORKSERVERSERVICE_INTERFACE_VERSION);
	GET_V_IFACE_ANY(GetEngineFactory, g_pSchemaSystem2, CSchemaSystem, SCHEMASYSTEM_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetEngineFactory, g_pGameResourceServiceServer, IGameResourceServiceServer, GAMERESOURCESERVICESERVER_INTERFACE_VERSION);

	g_SMAPI->AddListener( this, this );

	g_pCVar = icvar;
	ConVar_Register( FCVAR_RELEASE | FCVAR_CLIENT_CAN_EXECUTE | FCVAR_GAMEDLL );

	// InitializeGUI on another thread
	g_thread = std::thread(InitializeGUI);
	g_thread.detach();
	return true;
}

bool CS2ServerGUI::Unload(char *error, size_t maxlen)
{
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
	return "1.0.0";
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
	return "Server GUI using Dear ImGUI";
}

const char *CS2ServerGUI::GetName()
{
	return "CS2ServerGUI";
}

const char *CS2ServerGUI::GetURL()
{
	return "https://poggu.me";
}
