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

#include "type_stringifier.h"
#include "schemasystem/schemasystem.h"
#include <entity2/entitysystem.h>
#include <format>
#include <imgui.h>

namespace GUI::EntityBrowser
{
	extern CEntityHandle g_pSelectedEntity;
}

namespace GUI::EntityBrowser::TypeStringifier
{

std::string DumpBuiltinValue(void* value, CSchemaType_Builtin* pType)
{
	switch (pType->m_eBuiltinType)
	{
	case SCHEMA_BUILTIN_TYPE_BOOL:
		return *(bool*)value ? "true" : "false";
	case SCHEMA_BUILTIN_TYPE_CHAR:
		return std::to_string(*(char*)value);
	case SCHEMA_BUILTIN_TYPE_INT8:
		return std::to_string(*(int8*)value);
	case SCHEMA_BUILTIN_TYPE_UINT8:
		return std::to_string(*(uint8*)value);
	case SCHEMA_BUILTIN_TYPE_INT16:
		return std::to_string(*(int16*)value);
	case SCHEMA_BUILTIN_TYPE_UINT16:
		return std::to_string(*(uint16*)value);
	case SCHEMA_BUILTIN_TYPE_INT32:
		return std::to_string(*(int32*)value);
	case SCHEMA_BUILTIN_TYPE_UINT32:
		return std::to_string(*(uint32*)value);
	case SCHEMA_BUILTIN_TYPE_INT64:
		return std::to_string(*(int64*)value);
	case SCHEMA_BUILTIN_TYPE_UINT64:
		return std::to_string(*(uint64*)value);
	case SCHEMA_BUILTIN_TYPE_FLOAT32:
		return std::to_string(*(float*)value);
	case SCHEMA_BUILTIN_TYPE_FLOAT64:
		return std::to_string(*(double*)value);
	default:
		return "? (Builtin)";
	}
}

void DumpAtomicBasicValue(void* value, CSchemaType_Atomic* pType, const char* fieldName)
{
	if (!pType->m_pAtomicInfo)
		return ImGui::Text("? (Atomic null info)");

	if (!strcmp(pType->m_pAtomicInfo->m_pszName, "Vector"))
	{
		auto& vector = *static_cast<Vector*>(value);
		ImGui::Text("%f %f %f", vector.x, vector.y, vector.z);
		return;
	}

	if (!strcmp(pType->m_pAtomicInfo->m_pszName, "Vector2D"))
	{
		auto& vector = *static_cast<Vector2D*>(value);
		ImGui::Text("%f %f", vector.x, vector.y);
		return;
	}

	if (!strcmp(pType->m_pAtomicInfo->m_pszName, "QAngle"))
	{
		auto& qangle = *static_cast<QAngle*>(value);
		ImGui::Text("%f %f %f", qangle.x, qangle.y, qangle.z);
		return;
	}

	if (!strcmp(pType->m_pAtomicInfo->m_pszName, "Color"))
	{
		auto& color = *static_cast<Color*>(value);
		auto imColor = ImVec4(color.r() / 255.0f, color.g() / 255.0f, color.b() / 255.0f, color.a() / 255.0f);
		ImGui::ColorEdit4("color", (float*)&imColor, ImGuiColorEditFlags_NoOptions | ImGuiColorEditFlags_InputRGB);
		return;
	}

	if (!strcmp(pType->m_pAtomicInfo->m_pszName, "CUtlSymbolLarge"))
	{
		auto& symbolLarge = *static_cast<CUtlSymbolLarge*>(value);
		ImGui::Text("\"%s\"", symbolLarge.String());

		return;
	}

	if (!strcmp(pType->m_pAtomicInfo->m_pszName, "CUtlString"))
	{
		auto& string = *static_cast<CUtlString*>(value);
		ImGui::Text("\"%s\"", string.Get());

		return;
	}

	if (!strcmp(pType->m_pAtomicInfo->m_pszName, "CEntityIndex"))
	{
		auto& index = *static_cast<CEntityIndex*>(value);
		auto entity = GameEntitySystem()->GetEntityInstance(index);
		if (entity)
		{
			if (ImGui::SmallButton(std::format("{} ({})##{}", entity->GetClassname(), index.Get(), fieldName).c_str()))
				EntityBrowser::g_pSelectedEntity = entity->GetRefEHandle();
		}
		else
			ImGui::Text("[null entity]");

		return;
	}

	if (!strcmp(pType->m_pAtomicInfo->m_pszName, "CEntityHandle"))
	{
		auto& handle = *static_cast<CEntityHandle*>(value);
		if (auto entity = handle.Get())
		{
			if (ImGui::SmallButton(std::format("{} ({})##{}", entity->GetClassname(), handle.GetEntryIndex(), fieldName).c_str()))
				EntityBrowser::g_pSelectedEntity = handle;
		}
		else
			ImGui::Text("[null entity]");

		return;
	}

	if (!strcmp(pType->m_pAtomicInfo->m_pszName, "CNetworkedQuantizedFloat"))
	{
		ImGui::Text("%f", *static_cast<float*>(value));
		return;
	}

	if (!strcmp(pType->m_pAtomicInfo->m_pszName, "CUtlStringToken"))
	{
		ImGui::Text("%u", static_cast<CUtlStringToken*>(value)->m_nHashCode);
		return;
	}

	ImGui::Text("? (Atomic)");
}

void DumpAtomicTValue(void* value, CSchemaType_Atomic_T* pType, const char* fieldName)
{
	if (!pType->m_pAtomicInfo)
		return ImGui::Text("? (AtomicT null info)");

	if (!strcmp(pType->m_pAtomicInfo->m_pszName, "CHandle"))
	{
		auto& handle = *static_cast<CEntityHandle*>(value);
		if (auto entity = handle.Get())
		{
			if (ImGui::SmallButton(std::format("{} ({})##{}", entity->GetClassname(), handle.GetEntryIndex(), fieldName).c_str()))
				EntityBrowser::g_pSelectedEntity = handle;
		}
		else
			ImGui::Text("[null entity]");

		return;
	}
}
void DumpStringValue(void* ptr, CSchemaType* pType, bool* isNested, const char* fieldName)
{
	if (!ptr)
		return ImGui::Text("[null ptr]");

	if (pType->m_eTypeCategory == SCHEMA_TYPE_BUILTIN)
		return ImGui::Text("%s", DumpBuiltinValue(ptr, static_cast<CSchemaType_Builtin*>(pType)).c_str());

	if (pType->m_eTypeCategory == SCHEMA_TYPE_FIXED_ARRAY)
	{
		auto fixedArrayType = static_cast<CSchemaType_FixedArray*>(pType);
		if (fixedArrayType->m_pElementType->m_eTypeCategory == SCHEMA_TYPE_BUILTIN)
			if (static_cast<CSchemaType_Builtin*>(fixedArrayType->m_pElementType)->m_eBuiltinType == SCHEMA_BUILTIN_TYPE_CHAR)
				return ImGui::Text("\"%s\"", (char*)ptr);

		*isNested = true;
	}
	else if (pType->m_eTypeCategory == SCHEMA_TYPE_ATOMIC)
	{
		if (pType->m_eAtomicCategory == SCHEMA_ATOMIC_PLAIN)
			return DumpAtomicBasicValue(ptr, static_cast<CSchemaType_Atomic*>(pType), fieldName);
		else if (pType->m_eAtomicCategory == SCHEMA_ATOMIC_T)
		{
			auto atomicTType = static_cast<CSchemaType_Atomic_T*>(pType);
			return DumpAtomicTValue(ptr, static_cast<CSchemaType_Atomic_T*>(pType), fieldName);
		}
	}
	else if (pType->m_eTypeCategory == SCHEMA_TYPE_DECLARED_ENUM)
	{
		auto enumType = static_cast<CSchemaType_DeclaredEnum*>(pType);
		if (enumType->m_pEnumInfo)
		{
			for (int i = 0; i < enumType->m_pEnumInfo->m_nEnumeratorCount; i++)
			{
				auto& _enum = enumType->m_pEnumInfo->m_pEnumerators[i];
				if (enumType->m_pEnumInfo->m_nSize == 1 && _enum.m_nValue == *(int8*)(ptr))
					return ImGui::Text("%s (%i)", _enum.m_pszName, *(int8*)(ptr));
				else if (enumType->m_pEnumInfo->m_nSize == 2 && _enum.m_nValue == *(int16*)(ptr))
					return ImGui::Text("%s (%i)", _enum.m_pszName, *(int16*)(ptr));
				else if (enumType->m_pEnumInfo->m_nSize == 4 && _enum.m_nValue == *(int32*)(ptr))
					return ImGui::Text("%s (%i)", _enum.m_pszName, *(int32*)(ptr));
				else if (enumType->m_pEnumInfo->m_nSize == 8 && _enum.m_nValue == *(int64*)(ptr))
					return ImGui::Text("%s (%i)", _enum.m_pszName, *(int64*)(ptr));
			}

			return ImGui::Text("? (Enum)");
		}
	}

	ImGui::Text("");
}

} // namespace GUI::EntityBrowser::TypeStringifier