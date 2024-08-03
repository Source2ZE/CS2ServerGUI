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

#include "entitybrowser.h"
#include <imgui.h>
#include <schemasystem/schemasystem.h>
#include <entity2/entitysystem.h>
#include "cs2_sdk/entity/cbaseentity.h"
#include "type_stringifier.h"
#include <format>
#include <unordered_map>

namespace GUI::EntityBrowser
{

class MenuContext
{
public:
	ImGuiTextFilter m_entityFilter;
	ImGuiTextFilter m_propertyFilter;
};

static MenuContext g_menuContext;

CEntityHandle g_pSelectedEntity;
void DumpEntitySchema(void* pSchemaField, CSchemaClassInfo* pSchema, std::unordered_map<std::string, std::string>& overrideMap, bool root = false);
void DumpFieldValue(const char* name, void* pSchemaField, CSchemaType* pType, bool wasPtr, std::unordered_map<std::string, std::string>& overrideMap);

void Draw(bool* isOpen)
{
	ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);
	ImGui::Begin("Entity Browser", isOpen);

	int availableHeight = ImGui::GetWindowSize().y - 50;

	ImGui::BeginChild("Entity List", ImVec2(0, availableHeight / 2), true);

	g_menuContext.m_entityFilter.Draw("Search");

	if (ImGui::BeginTable("Entity Table", 2))
	{
		ImGui::TableSetupColumn("Name");
		ImGui::TableSetupColumn("Entity Index");
		ImGui::TableHeadersRow();

		EntityInstanceIter_t iter;

		while (CEntityInstance* entity = iter.Next())
		{
			if (!g_menuContext.m_entityFilter.PassFilter(entity->GetClassname()))
				continue;

			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::PushID(entity->GetEntityIndex().Get());
			if (ImGui::Selectable(entity->GetClassname(), false, ImGuiSelectableFlags_SpanAllColumns))
			{
				g_pSelectedEntity = entity->GetRefEHandle();
			}
			ImGui::PopID();
			ImGui::TableSetColumnIndex(1);
			ImGui::Text("%d", entity->GetEntityIndex());
		}

		ImGui::EndTable();
	}

	ImGui::EndChild();

	if (g_pSelectedEntity.IsValid())
	{
		ImGui::PushID(g_pSelectedEntity.GetSerialNumber());
		ImGui::BeginChild("Entity Info", ImVec2(0, availableHeight / 2), true);

		auto pEntity = g_pSelectedEntity.Get();
		if (pEntity)
		{
			ImGui::Text("Classname: %s", pEntity->GetClassname());
			ImGui::Text("Index: %d", pEntity->GetEntityIndex());
			ImGui::Text("Schema: %s", pEntity->Schema_DynamicBinding().Get()->m_pszName);
			g_menuContext.m_propertyFilter.Draw("Search");

			if (ImGui::BeginTable("Schema", 3, ImGuiTableFlags_Borders))
			{

				ImGui::TableSetupColumn("Name");
				ImGui::TableSetupColumn("Type");
				ImGui::TableSetupColumn("Value");
				ImGui::TableHeadersRow();

				auto pSchema = pEntity->Schema_DynamicBinding().Get();
				std::unordered_map<std::string, std::string> overrideMap;

				DumpEntitySchema(pEntity, pSchema, overrideMap, true);

				ImGui::EndTable();
			}

		}
		ImGui::EndChild();
		ImGui::PopID();
	}


	ImGui::End();
}

void DumpEntitySchema(void* pSchemaField, CSchemaClassInfo* pSchema, std::unordered_map<std::string, std::string>& overrideMap, bool root)
{
	if (!pSchemaField)
		return;

	ImGui::TableNextRow();
	ImGui::TableNextColumn();
	ImGui::TextColored(ImVec4(255, 255, 0, 255), "%s", pSchema->m_pszName);

	for (int i = 0; i < pSchema->m_nStaticMetadataCount; i++)
	{
		auto& metadata = pSchema->m_pStaticMetadata[i];
		if (!strcmp(metadata.m_pszName, "MNetworkVarTypeOverride"))
			overrideMap[((char**)metadata.m_pData)[0]] = ((char**)metadata.m_pData)[1]; // [0] = field name, [1] = type override
	}

	for (int i = 0; i < pSchema->m_nFieldCount; i++)
	{
		SchemaClassFieldData_t& field = pSchema->m_pFields[i];

		if(root && !g_menuContext.m_propertyFilter.PassFilter(field.m_pszName))
			continue;

		auto ptr = (void*)((uint8*)pSchemaField + field.m_nSingleInheritanceOffset);
		DumpFieldValue(field.m_pszName, ptr, field.m_pType, false, overrideMap);
	}

	for (int i = 0; i < pSchema->m_nBaseClassCount; i++)
	{
		auto& baseClass = pSchema->m_pBaseClasses[i];
		DumpEntitySchema(pSchemaField, baseClass.m_pClass, overrideMap, root);
	}
}

void DumpFieldValue(const char* name, void* pSchemaField, CSchemaType* pType, bool wasPtr, std::unordered_map<std::string, std::string>& overrideMap)
{
	if (pType->m_eTypeCategory == SCHEMA_TYPE_POINTER)
	{
		auto ptrType = static_cast<CSchemaType_Ptr*>(pType);
		DumpFieldValue(name, *static_cast<void**>(pSchemaField), ptrType->m_pObjectType, true, overrideMap);
		return;
	}

	ImGui::TableNextRow();
	ImGui::TableSetColumnIndex(2);
	bool isRenderNested = false;
	TypeStringifier::DumpStringValue(pSchemaField, pType, &isRenderNested, name);

	if (ImGui::BeginItemTooltip())
	{
		ImGui::Text("%p", pSchemaField);
		ImGui::EndTooltip();
	}
		
	ImGui::TableSetColumnIndex(0);
	bool hasNested = isRenderNested || pType->m_eTypeCategory == SCHEMA_TYPE_DECLARED_CLASS || (pType->m_eTypeCategory == SCHEMA_TYPE_ATOMIC && pType->m_eAtomicCategory == SCHEMA_ATOMIC_COLLECTION_OF_T);
	bool open = ImGui::TreeNodeEx(name, pSchemaField && hasNested ? ImGuiTreeNodeFlags_SpanAllColumns : (ImGuiTreeNodeFlags_SpanAllColumns | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_Leaf));

	ImGui::TableNextColumn();
	CBufferStringGrowable<256> buffer;
	pType->ToString(buffer, false);
	std::string typeName(buffer.Get());

	if (overrideMap.contains(name))
		typeName = overrideMap[name];

	ImGui::Text("%s", (typeName + (wasPtr ? " *" : "")).c_str());

	ImGui::TableNextColumn();
	/*
	if (!strcmp(name, "m_vRagdollDamageForce"))
	{
		DebugBreak();
	}*/
	
	if (hasNested && open && pSchemaField)
	{
		if (pType->m_eTypeCategory == SCHEMA_TYPE_DECLARED_CLASS)
		{
			auto classType = static_cast<CSchemaType_DeclaredClass*>(pType);

			if(classType->m_pTypeScope && overrideMap.contains(name))
				classType = classType->m_pTypeScope->FindType_DeclaredClass(overrideMap[name].c_str());

			DumpEntitySchema(pSchemaField, classType->m_pClassInfo, overrideMap);
			ImGui::TreePop();
		}
		else if (pType->m_eTypeCategory == SCHEMA_TYPE_ATOMIC && pType->m_eAtomicCategory == SCHEMA_ATOMIC_COLLECTION_OF_T)
		{
			auto atomicType = static_cast<CSchemaType_Atomic_CollectionOfT*>(pType);
			size_t count = (int)atomicType->m_pfnManipulator(SCHEMA_COLLECTION_MANIPULATOR_ACTION_GET_COUNT, pSchemaField, 0, 0);

			for (int i = 0; i < count; i++)
			{
				auto fieldName = std::format("[{}]", i);
				DumpFieldValue(fieldName.c_str(), (void*)atomicType->m_pfnManipulator(SCHEMA_COLLECTION_MANIPULATOR_ACTION_GET_ELEMENT_CONST, pSchemaField, (void*)i, 0), atomicType->m_pTemplateType, false, overrideMap);
			}
			ImGui::TreePop();
		}
		else if (pType->m_eTypeCategory == SCHEMA_TYPE_FIXED_ARRAY)
		{
			auto fixedArrayType = static_cast<CSchemaType_FixedArray*>(pType);
			for (int i = 0; i < fixedArrayType->m_nElementCount; i++)
			{
				// TODO ALIGNMENT
				auto arrayField = (void*)((uint8*)pSchemaField + fixedArrayType->m_nElementSize * i);
				auto fieldName = std::format("[{}]", i);
				DumpFieldValue(fieldName.c_str(), arrayField, fixedArrayType->m_pElementType, false, overrideMap);
			}
			ImGui::TreePop();
		}
	}
}


} // namespace GUI::EntityBrowser

