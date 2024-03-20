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

#include "entitylist.h"
#include "imgui/main.h"
#include "entity2/entitysystem.h"
#include "interfaces.h"
#include <string>
#include "entity2/entityclass.h"
#include <imgui.h>
#include <vector>
#include <imgui-notify/ImGuiNotify.hpp>


namespace GUI::Dumper::EntityList
{

static std::vector<std::pair<CEntityClass*, std::string>> entClasses;

enum ItemColumnID
{
	ItemColumnID_Classname,
	ItemColumnID_DesignerName,
};

void DumpToClipBoard()
{
	ImGui::InsertNotification({ ImGuiToastType::Info, 3000, "Entity list copied to clipboard!" });
	ImGui::LogToClipboard();
	ImGui::LogText("| Classname | Designer name |\n");
	ImGui::LogText("| --- | --- |\n");
	for (auto entClass : entClasses)
	{
		ImGui::LogText("| %s | %s |\n", entClass.first->m_pClassInfo->m_pszCPPClassname, entClass.second.c_str());
	}
	ImGui::LogFinish();
}

void Draw()
{
	static ImGuiTextFilter m_nameFilter;

	if (entClasses.empty())
	{
		FOR_EACH_MAP_FAST(GameEntitySystem()->m_entClassesByClassname, i)
		{
			entClasses.push_back({ GameEntitySystem()->m_entClassesByClassname[i], GameEntitySystem()->m_entClassesByClassname.Key(i)});
		}
	}

	m_nameFilter.Draw("Search");

	if (ImGui::Button("Dump to clipboard"))
		DumpToClipBoard();

	if (ImGui::BeginTable("Entity class list", 2, ImGuiTableFlags_Sortable))
	{
		ImGui::TableSetupColumn("Classname", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_DefaultSort, 0.0f, ItemColumnID_Classname);
		ImGui::TableSetupColumn("Designer Name", ImGuiTableColumnFlags_WidthFixed, 0.0f, ItemColumnID_DesignerName);
		ImGui::TableHeadersRow();

		if (ImGuiTableSortSpecs* sort_specs = ImGui::TableGetSortSpecs())
		{
			if (sort_specs->SpecsDirty)
			{
				std::sort(entClasses.begin(), entClasses.end(), [sort_specs](auto a, auto b) -> bool
				{
					int delta = 0;
					if (sort_specs->Specs->ColumnUserID == ItemColumnID_Classname)
					{
						delta = strcmp(a.first->m_pClassInfo->m_pszCPPClassname, b.first->m_pClassInfo->m_pszCPPClassname);
					}
					else if (sort_specs->Specs->ColumnUserID == ItemColumnID_DesignerName)
					{
						delta = strcmp(a.first->m_designerName.String(), b.first->m_designerName.String());
					}

					if (sort_specs->Specs->SortDirection == ImGuiSortDirection_Ascending)
						return delta < 0;
					else
						return delta > 0;

					return false;
				});
			}
		}


		for (auto entClass : entClasses)
		{
			if (!m_nameFilter.PassFilter(entClass.first->m_pClassInfo->m_pszCPPClassname) && !m_nameFilter.PassFilter(entClass.second.c_str()))
				continue;

			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text("%s", entClass.first->m_pClassInfo->m_pszCPPClassname);
			ImGui::TableNextColumn();
			ImGui::Text("%s", entClass.second.c_str());
		}

		ImGui::EndTable();
	}
}

} // namespace GUI::Dumper::EntityList