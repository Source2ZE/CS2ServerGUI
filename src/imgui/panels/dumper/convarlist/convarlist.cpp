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

#define FCVAR_MISSING5	((uint64_t)1<<(uint64_t)30)
#define FCVAR_MISSING6	((uint64_t)1<<(uint64_t)31)
#define FCVAR_DEFENSIVE	((uint64_t)1<<(uint64_t)32)

using json = nlohmann::json;

namespace GUI::Dumper::ConVarList
{

std::string StringifyValue(ConVar* pCvar)
{
	auto& value = *pCvar->m_cvvDefaultValue;

	switch (pCvar->m_eVarType)
	{
	case EConVarType_Bool:
		return value.m_bValue ? "true" : "false";
	case EConVarType_Int16:
		return std::to_string(value.m_i16Value);
	case EConVarType_UInt16:
		return std::to_string(value.m_u16Value);
	case EConVarType_Int32:
		return std::to_string(value.m_i32Value);
	case EConVarType_UInt32:
		return std::to_string(value.m_u32Value);
	case EConVarType_Int64:
		return std::to_string(value.m_i64Value);
	case EConVarType_UInt64:
		return std::to_string(value.m_u64Value);
	case EConVarType_Float32:
		return std::to_string(std::round(value.m_flValue * 10000.0f) / 10000.0f);
	case EConVarType_Float64:
		return std::to_string(value.m_dbValue);
	case EConVarType_String:
		return value.m_u64Value ? value.m_szValue : "";
	case EConVarType_Color:
		return std::format("{} {} {} {}", value.m_clrValue.r(), value.m_clrValue.g(), value.m_clrValue.b(), value.m_clrValue.a());
	case EConVarType_Vector2:
		return std::format("{} {}", value.m_vec2Value.x, value.m_vec2Value.y);
	case EConVarType_Vector3:
		return std::format("{} {} {}", value.m_vec3Value.x, value.m_vec3Value.y, value.m_vec3Value.z);
	case EConVarType_Vector4:
		return std::format("{} {} {} {}", value.m_vec4Value.x, value.m_vec4Value.y, value.m_vec4Value.z, value.m_vec4Value.w);
	case EConVarType_Qangle:
		return std::format("{} {} {}", value.m_angValue.x, value.m_angValue.y, value.m_angValue.z);
	default:
		return "Unknown";
	}
}

void ValueToJson(ConVar* pCvar, json& j)
{
	auto& value = *pCvar->m_cvvDefaultValue;

	switch (pCvar->m_eVarType)
	{
	case EConVarType_Bool:
		j["default"] = value.m_bValue;
		break;
	case EConVarType_Int16:
		j["default"] = value.m_i16Value;
		if(pCvar->m_cvvMinValue)
			j["min"] = pCvar->m_cvvMinValue->m_i16Value;
		if (pCvar->m_cvvMaxValue)
			j["max"] = pCvar->m_cvvMaxValue->m_i16Value;
		break;
	case EConVarType_UInt16:
		j["default"] = value.m_u16Value;
		if (pCvar->m_cvvMinValue)
			j["min"] = pCvar->m_cvvMinValue->m_u16Value;
		if (pCvar->m_cvvMaxValue)
			j["max"] = pCvar->m_cvvMaxValue->m_u16Value;
		break;
	case EConVarType_Int32:
		j["default"] = value.m_i32Value;
		if (pCvar->m_cvvMinValue)
			j["min"] = pCvar->m_cvvMinValue->m_i32Value;
		if (pCvar->m_cvvMaxValue)
			j["max"] = pCvar->m_cvvMaxValue->m_i32Value;
		break;
	case EConVarType_UInt32:
		j["default"] = value.m_u32Value;
		if (pCvar->m_cvvMinValue)
			j["min"] = pCvar->m_cvvMinValue->m_u32Value;
		if (pCvar->m_cvvMaxValue)
			j["max"] = pCvar->m_cvvMaxValue->m_u32Value;
		break;
	case EConVarType_Int64:
		j["default"] = value.m_i64Value;
		if (pCvar->m_cvvMinValue)
			j["min"] = pCvar->m_cvvMinValue->m_i64Value;
		if (pCvar->m_cvvMaxValue)
			j["max"] = pCvar->m_cvvMaxValue->m_i64Value;
		break;
	case EConVarType_UInt64:
		j["default"] = value.m_u64Value;
		if (pCvar->m_cvvMinValue)
			j["min"] = pCvar->m_cvvMinValue->m_u64Value;
		if (pCvar->m_cvvMaxValue)
			j["max"] = pCvar->m_cvvMaxValue->m_u64Value;
		break;
	case EConVarType_Float32:
		j["default"] = (json::number_float_t)(round((int)(value.m_flValue * 10000)) / 10000);
		if (pCvar->m_cvvMinValue)
			j["min"] = (json::number_float_t)(round((int)(pCvar->m_cvvMinValue->m_flValue * 10000)) / 10000);
		if (pCvar->m_cvvMaxValue)
			j["max"] = (json::number_float_t)(round((int)(pCvar->m_cvvMaxValue->m_flValue * 10000)) / 10000);
		break;
	case EConVarType_Float64:
		j["default"] = value.m_dbValue;
		if (pCvar->m_cvvMinValue)
			j["min"] = pCvar->m_cvvMinValue->m_dbValue;
		if (pCvar->m_cvvMaxValue)
			j["max"] = pCvar->m_cvvMaxValue->m_dbValue;
		break;
	case EConVarType_String:
		j["default"] = value.m_u64Value ? value.m_szValue : "";
		break;
	case EConVarType_Color:
		j["default"] = { value.m_clrValue.r(), value.m_clrValue.g(), value.m_clrValue.b(), value.m_clrValue.a() };
		break;
	case EConVarType_Vector2:
		j["default"] = { value.m_vec2Value.x, value.m_vec2Value.y };
		break;
	case EConVarType_Vector3:
		j["default"] = { value.m_vec3Value.x, value.m_vec3Value.y, value.m_vec3Value.z };
		break;
	case EConVarType_Vector4:
		j["default"] = { value.m_vec4Value.x, value.m_vec4Value.y, value.m_vec4Value.z, value.m_vec4Value.w };
		break;
	case EConVarType_Qangle:
		j["default"] = { value.m_angValue.x, value.m_angValue.y, value.m_angValue.z };
		break;
	default:
		j["default"] = "Unknown";
		break;
	}
}

void DumpToJSON(std::string& path)
{
	json jsonArray;
	ConVar* pCvar = nullptr;
	ConVarHandle hCvarHandle;
	hCvarHandle.Set(0);

	do
	{
		pCvar = g_pCVar->GetConVar(hCvarHandle);

		hCvarHandle.Set(hCvarHandle.Get() + 1);

		if (!pCvar)
			continue;

		json cvar;
		cvar["name"] = pCvar->m_pszName;
		cvar["description"] = pCvar->m_pszHelpString;

		ValueToJson(pCvar, cvar);

		cvar["type"] = pCvar->m_eVarType;
		cvar["flags"] = CommandList::PrettifyFlags(static_cast<uint64_t>(pCvar->flags));
		cvar["flagsRaw"] = static_cast<uint64_t>(pCvar->flags);
		jsonArray.push_back(cvar);
	} while (pCvar);

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

	if (ImGui::BeginTable("Convar List", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg))
	{
		ImGui::TableSetupColumn("Name");
		ImGui::TableSetupColumn("Default Value");
		ImGui::TableSetupColumn("Flags");
		ImGui::TableSetupColumn("Description");
		ImGui::TableHeadersRow();

		ConVar* pCvar = nullptr;
		ConVarHandle hCvarHandle;
		hCvarHandle.Set(0);

		do
		{
			pCvar = g_pCVar->GetConVar(hCvarHandle);

			hCvarHandle.Set(hCvarHandle.Get() + 1);

			if (!pCvar)
				continue;

			if (!m_nameFilter.PassFilter(pCvar->m_pszName))
				continue;

			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text("%s", pCvar->m_pszName);

			ImGui::TableNextColumn();
			ImGui::Text("%s", StringifyValue(pCvar).c_str());

			ImGui::TableNextColumn();
			ImGui::Text("%s", CommandList::PrettifyFlags(static_cast<uint64_t>(pCvar->flags)).c_str());

			ImGui::TableNextColumn();
			ImGui::Text("%s", pCvar->m_pszHelpString);


		} while (pCvar);

		ImGui::EndTable();
	}	
}

} // namespace GUI::Dumper::ConVarList