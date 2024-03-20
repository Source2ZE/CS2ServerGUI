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

#include "stringtables.h"
#include "imgui/main.h"
#include "imgui/panels/entitybrowser/entitybrowser.h"
#include "cs2_sdk/entity/cbaseplayercontroller.h"
#include "entity2/entitysystem.h"
#include "interfaces.h"
#include <string>
#include "networkstringtabledefs.h"
#include <imgui.h>

namespace GUI::StringTables
{

void Draw(bool* isOpen)
{
	ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);
	ImGui::Begin("String Tables", isOpen);

	int num = Interfaces::networkStringTableContainerServer->GetNumTables();

	if (ImGui::BeginTable("Players", 3))
	{
		ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthFixed);
		ImGui::TableSetupColumn("Table", ImGuiTableColumnFlags_WidthFixed);
		ImGui::TableSetupColumn("Ptr", ImGuiTableColumnFlags_WidthFixed);
		ImGui::TableHeadersRow();

		for (int i = 0; i < num; i++)
		{
			ImGui::PushID(i);
			auto table = Interfaces::networkStringTableContainerServer->GetTable(i);

			if (!table)
				continue;

			ImGui::TableNextRow();

			ImGui::TableNextColumn();
			bool open = ImGui::TreeNodeEx("StringTable", ImGuiTreeNodeFlags_SpanAllColumns, "%i", table->GetTableId());

			ImGui::TableNextColumn();
			ImGui::Text("%s", table->GetTableName());

			ImGui::TableNextColumn();
			ImGui::Text("%p", table);

			if (open)
			{
				for (int j = 0; j < table->GetNumStrings(); j++)
				{
					ImGui::PushID(j);
					ImGui::TableNextRow();

					ImGui::TableNextColumn();
					ImGui::Text("[%i]", j);
		
					ImGui::TableNextColumn();
					ImGui::Text("%s", table->GetString(j));

					ImGui::PopID();
				}

				ImGui::TreePop();
			}

			ImGui::PopID();
		}

		ImGui::EndTable();
	}

	ImGui::End();
}

} // namespace GUI::StringTables