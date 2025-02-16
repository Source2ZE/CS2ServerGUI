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

#include "commandlist.h"
#include "imgui/main.h"
#include "interfaces.h"
#include <string>
#include <imgui.h>
#include <vector>
#include <imgui-notify/ImGuiNotify.hpp>
#include <ImGuiFileDialog.h>
#include <nlohmann/json.hpp>

#define FCVAR_MISSING1	(1ull<<30)
#define FCVAR_MISSING2	(1ull<<31)
using json = nlohmann::json;

namespace GUI::Dumper::CommandList
{

std::vector<std::pair<uint64_t, const char*>> g_flagMap{
	{FCVAR_LINKED_CONCOMMAND, "linked_concommand"},
	{FCVAR_DEVELOPMENTONLY, "developmentonly"},
	{FCVAR_GAMEDLL, "gamedll"},
	{FCVAR_CLIENTDLL, "clientdll"},
	{FCVAR_HIDDEN, "hidden"},
	{FCVAR_PROTECTED, "protected"},
	{FCVAR_SPONLY, "sponly"},
	{FCVAR_ARCHIVE, "archive"},
	{FCVAR_NOTIFY, "notify"},
	{FCVAR_USERINFO, "userinfo"},
	{FCVAR_REFERENCE, "reference"},
	{FCVAR_UNLOGGED, "unlogged"},
	{FCVAR_INITIAL_SETVALUE, "initial_setvalue"},
	{FCVAR_REPLICATED, "replicated"},
	{FCVAR_CHEAT, "cheat"},
	{FCVAR_PER_USER, "per_user"},
	{FCVAR_DEMO, "demo"},
	{FCVAR_DONTRECORD, "dontrecord"},
	{FCVAR_PERFORMING_CALLBACKS, "performing_callbacks"},
	{FCVAR_RELEASE, "release"},
	{FCVAR_MENUBAR_ITEM, "menubar_item"},
	{FCVAR_COMMANDLINE_ENFORCED, "commandline_enforced"},
	{FCVAR_NOT_CONNECTED, "notconnected"},
	{FCVAR_VCONSOLE_FUZZY_MATCHING, "vconsole_fuzzy_matching"},
	{FCVAR_SERVER_CAN_EXECUTE, "server_can_execute"},
	{FCVAR_CLIENT_CAN_EXECUTE, "client_can_execute"},
	{FCVAR_SERVER_CANNOT_QUERY, "server_cannot_query"},
	{FCVAR_VCONSOLE_SET_FOCUS, "vconsole_set_focus"},
	{FCVAR_CLIENTCMD_CAN_EXECUTE, "clientcmd_can_execute"},
	{FCVAR_EXECUTE_PER_TICK, "execute_per_tick"},
	{FCVAR_MISSING1, "missing1"},
	{FCVAR_MISSING2, "missing2"},
	{FCVAR_DEFENSIVE, "defensive"}
};

std::string PrettifyFlags(uint64_t flags)
{
	std::string result("");
	for (const auto& [value, name] : g_flagMap)
	{
		if (flags & value)
		{
			result += std::string(" ") + name;
		}
	}
	result.erase(0, result.find_first_not_of(" \t\n\r\f\v"));
	return result;
}

void DumpToJSON(std::string& path)
{
	json jsonArray;

	ConCommandData* data = g_pCVar->GetConCommandData(ConCommandRef());
	for (ConCommandRef ref = ConCommandRef((uint16)0); ref.GetRawData() != data; ref = ConCommandRef(ref.GetAccessIndex() + 1))
	{
		json cvar;
		cvar["name"] = ref.GetName();
		cvar["description"] = ref.GetHelpText();
		cvar["flags"] = PrettifyFlags(static_cast<uint64_t>(ref.GetFlags()));
		cvar["flagsRaw"] = static_cast<uint64_t>(ref.GetFlags());
		jsonArray.push_back(cvar);
	}


	std::ofstream file(path);
	file << jsonArray.dump();
	file.close();
}

void Draw()
{
	static ImGuiTextFilter m_nameFilter;

	m_nameFilter.Draw("Search");

	if (ImGui::Button("Dump to file"))
	{
		const char* filters = "JSON File (*.json){.json}";
		IGFD::FileDialogConfig config;
		config.path = ".";
		config.fileName = "commands.json";
		config.countSelectionMax = 1;
		config.flags = ImGuiFileDialogFlags_Default;
		ImGuiFileDialog::Instance()->OpenDialog("SaveCommandsFileDlg", "Choose a File", filters, config);
	}

	if (ImGui::BeginTable("Command List", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg))
	{
		ImGui::TableSetupColumn("Name");
		ImGui::TableSetupColumn("Flags");
		ImGui::TableSetupColumn("Description");
		ImGui::TableHeadersRow();

		ConCommandData* data = g_pCVar->GetConCommandData(ConCommandRef());
		for (ConCommandRef ref = ConCommandRef((uint16)0); ref.GetRawData() != data; ref = ConCommandRef(ref.GetAccessIndex() + 1))
		{
			if (!m_nameFilter.PassFilter(ref.GetName()))
				continue;

			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text("%s", ref.GetName());

			ImGui::TableNextColumn();
			ImGui::Text("%s", PrettifyFlags(static_cast<uint64_t>(ref.GetFlags())).c_str());

			ImGui::TableNextColumn();
			ImGui::Text("%s", ref.GetHelpText());
		}

		ImGui::EndTable();
	}	
}

} // namespace GUI::Dumper::CommandList