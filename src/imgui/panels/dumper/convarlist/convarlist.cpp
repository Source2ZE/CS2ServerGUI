#include "convarlist.h"
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

namespace GUI::Dumper::ConVarList
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
		cvar["flags"] = PrettifyFlags(static_cast<uint64_t>(pCvar->flags));
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
			ImGui::Text("%s", PrettifyFlags(static_cast<uint64_t>(pCvar->flags)).c_str());

			ImGui::TableNextColumn();
			ImGui::Text("%s", pCvar->m_pszHelpString);


		} while (pCvar);

		ImGui::EndTable();
	}	
}

} // namespace GUI::ConVarList