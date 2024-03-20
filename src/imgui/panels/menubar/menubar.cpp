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

#include "menubar.h"
#include "imgui/main.h"
#include "imgui/panels/actions/actions.h"
#include <imgui.h>

void GUI::MenuBar::Draw()
{
	if (ImGui::BeginMainMenuBar())
	{
		if (ImGui::BeginMenu("Tools"))
		{
			ImGui::MenuItem("Entity Browser", nullptr, &g_GUICtx.m_WindowStates.m_bEntityBrowser);
			ImGui::MenuItem("Player List", nullptr, &g_GUICtx.m_WindowStates.m_bPlayerList);
			ImGui::MenuItem("String Tables", nullptr, &g_GUICtx.m_WindowStates.m_bStringTables);
			ImGui::MenuItem("Dumper", nullptr, &g_GUICtx.m_WindowStates.m_bDumper);
			ImGui::MenuItem("Event Logger", nullptr, &g_GUICtx.m_WindowStates.m_bEventLogger);
			ImGui::MenuItem("Demo Window", nullptr, &g_GUICtx.m_WindowStates.m_bDemoWindow);
			ImGui::EndMenu();
		}
		
		if (ImGui::BeginMenu("Actions"))
		{
			Actions::Draw();
			ImGui::EndMenu();
		}
		ImGui::EndMainMenuBar();
	}
}