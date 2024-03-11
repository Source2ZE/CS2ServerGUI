#include "menu.h"
#include <imgui.h>
#include <schemasystem/schemasystem.h>
#include <entity2/entitysystem.h>
#include "cs2_sdk/entity/cbaseentity.h"

extern CSchemaSystem* g_pSchemaSystem2;

/*
enum SchemaBuiltinType_t
{
	SCHEMA_BUILTIN_INVALID = 0,
	SCHEMA_BUILTIN_VOID,
	SCHEMA_BUILTIN_CHAR,
	SCHEMA_BUILTIN_INT8,
	SCHEMA_BUILTIN_UINT8,
	SCHEMA_BUILTIN_INT16,
	SCHEMA_BUILTIN_UINT16,
	SCHEMA_BUILTIN_INT32,
	SCHEMA_BUILTIN_UINT32,
	SCHEMA_BUILTIN_INT64,
	SCHEMA_BUILTIN_UINT64,
	SCHEMA_BUILTIN_FLOAT32,
	SCHEMA_BUILTIN_FLOAT64,
	SCHEMA_BUILTIN_BOOL,
	SCHEMA_BUILTIN_COUNT,
};
*/

std::string CSchemaTypeValueToString(CSchemaType* pType, void* value)
{
	if (pType->m_eTypeCategory == SCHEMA_TYPE_BUILTIN)
	{
		auto pBuiltin = static_cast<CSchemaType_Builtin*>(pType);

		switch (pBuiltin->m_eBuiltinType)
		{
		case SCHEMA_BUILTIN_BOOL:
			return *(bool*)value ? "true" : "false";
		case SCHEMA_BUILTIN_CHAR:
			return std::to_string(*(char*)value);
		case SCHEMA_BUILTIN_INT8:
			return std::to_string(*(int8*)value);
		case SCHEMA_BUILTIN_UINT8:
			return std::to_string(*(uint8*)value);
		case SCHEMA_BUILTIN_INT16:
			return std::to_string(*(int16*)value);
		case SCHEMA_BUILTIN_UINT16:
			return std::to_string(*(uint16*)value);
		case SCHEMA_BUILTIN_INT32:
			return std::to_string(*(int32*)value);
		case SCHEMA_BUILTIN_UINT32:
			return std::to_string(*(uint32*)value);
		case SCHEMA_BUILTIN_INT64:
			return std::to_string(*(int64*)value);
		case SCHEMA_BUILTIN_UINT64:
			return std::to_string(*(uint64*)value);
		case SCHEMA_BUILTIN_FLOAT32:
			return std::to_string(*(float*)value);
		case SCHEMA_BUILTIN_FLOAT64:
			return std::to_string(*(double*)value);
		default:
			return "???";
		}
	}

	return "???";
}

namespace Menu
{

CEntityHandle g_pSelectedEntity;

void Draw()
{
	ImGui::Begin("Entity Browser");

	int availableHeight = ImGui::GetWindowSize().y - 50;

	ImGui::BeginChild("Entity List", ImVec2(0, availableHeight / 2), true);

	if (ImGui::BeginTable("Entity Table", 2))
	{
		ImGui::TableSetupColumn("Name");
		ImGui::TableSetupColumn("Entity Index");
		ImGui::TableHeadersRow();


		EntityInstanceIter_t iter;

		while (CEntityInstance* entity = iter.Next())
		{
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::PushID(entity->GetEntityIndex().Get());
			if (ImGui::Selectable(entity->GetClassname(), false, ImGuiSelectableFlags_SpanAllColumns))
			{
				ConMsg("CLICKED\n");
				g_pSelectedEntity = entity->GetRefEHandle();
			}
			ImGui::PopID();
			ImGui::TableSetColumnIndex(1);
			ImGui::Text("%d", entity->GetEntityIndex());
		}

		ImGui::EndTable();

	}

	ImGui::EndChild();

	ImGui::BeginChild("Entity Info", ImVec2(0, availableHeight / 2), true);

	if (g_pSelectedEntity.IsValid())
	{
		auto pEntity = g_pSelectedEntity.Get();
		if (pEntity)
		{
			ImGui::Text("Classname: %s", pEntity->GetClassname());
			ImGui::Text("Index: %d", pEntity->GetEntityIndex());
			ImGui::Text("Schema: %s", pEntity->Schema_DynamicBinding().Get()->m_pszName);

			// Table of all schema properties

			ImGui::BeginTable("Schema", 3);

			ImGui::TableSetupColumn("Name");
			ImGui::TableSetupColumn("Type");
			ImGui::TableSetupColumn("Value");
			ImGui::TableHeadersRow();

			auto pSchema = pEntity->Schema_DynamicBinding().Get();

			for (int i = 0; i < pSchema->m_nFieldCount; i++)
			{
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				SchemaClassFieldData_t& field = pSchema->m_pFields[i];
				ImGui::Text("%s", field.m_pszName);

				CBufferStringGrowable<256> buffer;
				field.m_pType->ToString(buffer, false);

				ImGui::TableNextColumn();
				ImGui::Text("%s", buffer.Get());
				ImGui::TableNextColumn();

				auto ptr = (void*)((uint8*)pEntity + field.m_nSingleInheritanceOffset);
				ImGui::Text("%s", CSchemaTypeValueToString(field.m_pType, ptr).c_str());

			}

			// ff


			ImGui::EndTable();

		}
	}

	ImGui::EndChild();

	ImGui::End();
}

} // namespace Menu