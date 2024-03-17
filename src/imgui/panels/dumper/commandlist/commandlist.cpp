#include "commandlist.h"
#include "imgui/main.h"
#include "interfaces.h"
#include <string>
#include <imgui.h>
#include <vector>
#include <imgui-notify/ImGuiNotify.hpp>
#include <ImGuiFileDialog.h>
#include <nlohmann/json.hpp>

#define FCVAR_MISSING5	((uint64_t)1<<(uint64_t)30)
#define FCVAR_MISSING6	((uint64_t)1<<(uint64_t)31)
#define FCVAR_DEFENSIVE	((uint64_t)1<<(uint64_t)32)

using json = nlohmann::json;

namespace GUI::Dumper::CommandList
{

std::string PrettifyFlags(uint64_t flags)
{
	std::string result("");
	if (flags & FCVAR_NOT_CONNECTED)
		result += " notconnected";
	if (flags & FCVAR_DEVELOPMENTONLY)
		result += " developmentonly";
	if (flags & FCVAR_GAMEDLL)
		result += " gamedll";
	if (flags & FCVAR_HIDDEN)
		result += " hidden";
	if (flags & FCVAR_CLIENTDLL)
		result += " clientdll";
	if (flags & FCVAR_PROTECTED)
		result += " protected";
	if (flags & FCVAR_SPONLY)
		result += " sponly";
	if (flags & FCVAR_ARCHIVE)
		result += " archive";
	if (flags & FCVAR_NOTIFY)
		result += " notify";
	if (flags & FCVAR_MISSING0)
		result += " hide";
	if (flags & FCVAR_USERINFO)
		result += " userinfo";
	if (flags & FCVAR_MISSING1)
		result += " missing1";
	if (flags & FCVAR_UNLOGGED)
		result += " unlogged";
	if (flags & FCVAR_REPLICATED)
		result += " replicated";
	if (flags & FCVAR_CHEAT)
		result += " cheat";
	if (flags & FCVAR_PER_USER)
		result += " per_user";
	if (flags & FCVAR_DEMO)
		result += " demo";
	if (flags & FCVAR_MISSING2)
		result += " missing2";
	if (flags & FCVAR_DONTRECORD)
		result += " dontrecord";
	if (flags & FCVAR_RELEASE)
		result += " release";
	if (flags & FCVAR_MENUBAR_ITEM)
		result += " menubar_item";
	if (flags & FCVAR_LINKED_CONCOMMAND)
		result += " linked_concommand";
	if (flags & FCVAR_VCONSOLE_FUZZY_MATCHING)
		result += " vconsole_fuzzy_matching";
	if (flags & FCVAR_MISSING3)
		result += " missing3";
	if (flags & FCVAR_NOT_CONNECTED)
		result += " missing4";
	if (flags & FCVAR_SERVER_CAN_EXECUTE)
		result += " server_can_execute";
	if (flags & FCVAR_SERVER_CANNOT_QUERY)
		result += " server_cannot_query";
	if (flags & FCVAR_VCONSOLE_SET_FOCUS)
		result += " vconsole_set_focus";
	if (flags & FCVAR_CLIENTCMD_CAN_EXECUTE)
		result += " clientcmd_can_execute";
	if (flags & FCVAR_EXECUTE_PER_TICK)
		result += " execute_per_tick";
	if (flags & FCVAR_MISSING5)
		result += " missing5";
	if (flags & FCVAR_MISSING6)
		result += " missing6";
	if (flags & ((uint64_t)1 << (uint64_t)32))
		result += " defensive";
	result.erase(0, result.find_first_not_of(" \t\n\r\f\v"));
	return result;
}

void DumpToJSON(std::string& path)
{
	json jsonArray;
	ConCommand* pConCommand = nullptr;
	ConCommand* pInvalidCommand = g_pCVar->GetCommand(ConCommandHandle());
	ConCommandHandle hConCommandHandle;
	hConCommandHandle.Set(0);

	do
	{
		pConCommand = g_pCVar->GetCommand(hConCommandHandle);

		hConCommandHandle.Set(hConCommandHandle.Get() + 1);


		json cvar;
		cvar["name"] = pConCommand->GetName();
		cvar["description"] = pConCommand->GetHelpText();
		cvar["flags"] = PrettifyFlags(static_cast<uint64_t>(pConCommand->GetFlags()));
		cvar["flagsRaw"] = static_cast<uint64_t>(pConCommand->GetFlags());
		jsonArray.push_back(cvar);

	} while (pConCommand && pConCommand != pInvalidCommand);


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

		ConCommand* pConCommand = nullptr;
		ConCommand* pInvalidCommand = g_pCVar->GetCommand(ConCommandHandle());
		ConCommandHandle hConCommandHandle;
		hConCommandHandle.Set(0);

		do
		{
			pConCommand = g_pCVar->GetCommand(hConCommandHandle);

			hConCommandHandle.Set(hConCommandHandle.Get() + 1);

			if (!m_nameFilter.PassFilter(pConCommand->GetName()))
				continue;

			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text("%s", pConCommand->GetName());

			ImGui::TableNextColumn();
			ImGui::Text("%s", PrettifyFlags(static_cast<uint64_t>(pConCommand->GetFlags())).c_str());

			ImGui::TableNextColumn();
			ImGui::Text("%s", pConCommand->GetHelpText());

		} while (pConCommand && pConCommand != pInvalidCommand);

		ImGui::EndTable();
	}	
}

} // namespace GUI::Dumper::CommandList