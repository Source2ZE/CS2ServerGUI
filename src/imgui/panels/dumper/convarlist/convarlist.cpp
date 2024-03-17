#include "convarlist.h"
#include "imgui/main.h"
#include "interfaces.h"
#include <string>
#include <imgui.h>
#include <vector>
#include <imgui-notify/ImGuiNotify.hpp>

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

void Draw()
{
	ImGui::Button("Dump to file");

	if (ImGui::BeginTable("Convar List", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg))
	{
		ImGui::TableSetupColumn("Name");
		ImGui::TableSetupColumn("Default Value");
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

			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text("%s", pCvar->m_pszName);

			ImGui::TableNextColumn();
			ImGui::Text("%s", StringifyValue(pCvar));

			ImGui::TableNextColumn();
			ImGui::Text("%s", pCvar->m_pszHelpString);


		} while (pCvar);

		ImGui::EndTable();
	}	
}

} // namespace GUI::ConVarList