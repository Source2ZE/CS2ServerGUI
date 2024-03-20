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

#include "dumper.h"
#include "imgui/main.h"
#include "interfaces.h"
#include "entitylist/entitylist.h"
#include "convarlist/convarlist.h"
#include "commandlist/commandlist.h"
#include <string>
#include <imgui.h>
#include <vector>


namespace GUI::Dumper
{

void Draw(bool* isOpen)
{
	ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);
	ImGui::Begin("Dumper", isOpen);

	if (ImGui::BeginTabBar("DumperTabs"))
	{
		if (ImGui::BeginTabItem("Entity List"))
		{
			EntityList::Draw();
			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("ConVar List"))
		{
			ConVarList::Draw();
			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("Command List"))
		{
			CommandList::Draw();
			ImGui::EndTabItem();
		}

		ImGui::EndTabBar();
	}

	ImGui::End();
}

} // namespace GUI::Dumper