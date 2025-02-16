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

#include "convarlist.h"
#include "imgui/main.h"
#include "interfaces.h"
#include "imgui/panels/dumper/commandlist/commandlist.h"
#include <string>
#include <imgui.h>
#include <vector>
#include <imgui-notify/ImGuiNotify.hpp>
#include <ImGuiFileDialog.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace GUI::Dumper::ConVarList
{

std::string StringifyValue(ConVarRefAbstract cvar)
{
	auto conVarData = cvar.GetConVarData();
	auto value = cvar.GetConVarData()->DefaultValue();

	CBufferStringN<512> buf;
	cvar.GetDefaultAsString(buf);
	return std::string(buf.Get(), buf.Length());
}

void ValueToJson(ConVarRefAbstract cvar, json& j)
{
	CBufferStringN<512> buf;
	cvar.GetDefaultAsString(buf);
	j["default"] = std::string(buf.Get(), buf.Length());
	if (cvar.HasMin())
	{
		cvar.GetMinAsString(buf);
		j["min"] = std::string(buf.Get(), buf.Length());
	}
	if (cvar.HasMax())
	{
		cvar.GetMaxAsString(buf);
		j["max"] = std::string(buf.Get(), buf.Length());
	}
}

void DumpToJSON(std::string& path)
{
	json jsonArray;

	for (ConVarRefAbstract ref(ConVarRef((uint16)0)); ref.IsValidRef(); ref = ConVarRefAbstract(ConVarRef(ref.GetAccessIndex() + 1)))
	{
		json cvar;
		cvar["name"] = ref.GetName();
		cvar["description"] = ref.GetHelpText();

		ValueToJson(ref, cvar);

		cvar["type"] = ref.TypeTraits()->m_TypeName;
		cvar["typeRaw"] = ref.GetType();
		cvar["flags"] = CommandList::PrettifyFlags(static_cast<uint64_t>(ref.GetFlags()));
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
		config.fileName = "convars.json";
		config.countSelectionMax = 1;
		config.flags = ImGuiFileDialogFlags_Default;
		ImGuiFileDialog::Instance()->OpenDialog("SaveConvarFileDlg", "Choose a File", filters, config);
	}

	if (ImGui::BeginTable("Convar List", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg))
	{
		ImGui::TableSetupColumn("Name");
		ImGui::TableSetupColumn("Type", 0, 20);
		ImGui::TableSetupColumn("Default Value");
		ImGui::TableSetupColumn("Flags");
		ImGui::TableSetupColumn("Description");
		ImGui::TableHeadersRow();

		for (ConVarRefAbstract ref(ConVarRef((uint16)0)); ref.IsValidRef(); ref = ConVarRefAbstract(ConVarRef(ref.GetAccessIndex() + 1)))
		{
			if (!m_nameFilter.PassFilter(ref.GetName()))
				continue;

			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text("%s", ref.GetName());

			ImGui::TableNextColumn();
			ImGui::Text("%s", ref.TypeTraits()->m_TypeName);

			ImGui::TableNextColumn();
			ImGui::Text("%s", StringifyValue(ref).c_str());

			ImGui::TableNextColumn();
			ImGui::Text("%s", CommandList::PrettifyFlags(static_cast<uint64_t>(ref.GetFlags())).c_str());

			ImGui::TableNextColumn();
			ImGui::Text("%s", ref.GetHelpText());
		}

		ImGui::EndTable();
	}	
}

} // namespace GUI::Dumper::ConVarList