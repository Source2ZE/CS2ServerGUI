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

static std::vector<CEntityClass*> entClasses;

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
	for (CEntityClass* entClass : entClasses)
	{
		ImGui::LogText("| %s | %s |\n", entClass->m_pClassInfo->m_pszCPPClassname, entClass->m_designerName.String());
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
			entClasses.push_back(GameEntitySystem()->m_entClassesByClassname[i]);
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
				std::sort(entClasses.begin(), entClasses.end(), [sort_specs](CEntityClass* a, CEntityClass* b) -> bool
					{
						int delta = 0;
						if (sort_specs->Specs->ColumnUserID == ItemColumnID_Classname)
						{
							delta = strcmp(a->m_pClassInfo->m_pszCPPClassname, b->m_pClassInfo->m_pszCPPClassname);
						}
						else if (sort_specs->Specs->ColumnUserID == ItemColumnID_DesignerName)
						{
							delta = strcmp(a->m_designerName.String(), b->m_designerName.String());
						}

						if (sort_specs->Specs->SortDirection == ImGuiSortDirection_Ascending)
							return delta < 0;
						else
							return delta > 0;

						return false;
					});
			}
		}


		for (CEntityClass* entClass : entClasses)
		{
			if (!m_nameFilter.PassFilter(entClass->m_pClassInfo->m_pszCPPClassname) && !m_nameFilter.PassFilter(entClass->m_designerName.String()))
				continue;

			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text("%s", entClass->m_pClassInfo->m_pszCPPClassname);
			ImGui::TableNextColumn();
			ImGui::Text("%s", entClass->m_designerName.String());
		}

		ImGui::EndTable();
	}
}

} // namespace GUI::Dumper