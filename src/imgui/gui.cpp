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

#include <imgui.h>
#include "main.h"
#include "panels/entitybrowser/entitybrowser.h"
#include "panels/menubar/menubar.h"
#include "panels/playerlist/playerlist.h"
#include "panels/stringtables/stringtables.h"
#include "panels/dumper/dumper.h"
#include "panels/dumper/convarlist/convarlist.h"
#include "panels/dumper/commandlist/commandlist.h"
#include "panels/eventlogger/eventlogger.h"
#include <ImGuiFileDialog.h>
#include "extension.h"

namespace GUI
{

void DrawFileDialogs();
void DrawWelcomeModal();

void DrawMainWindow()
{
	if (g_GUICtx.m_WindowStates.m_bDemoWindow)
		ImGui::ShowDemoWindow(&g_GUICtx.m_WindowStates.m_bDemoWindow);

	if (g_GUICtx.m_WindowStates.m_bEntityBrowser)
		EntityBrowser::Draw(&g_GUICtx.m_WindowStates.m_bEntityBrowser);

	if (g_GUICtx.m_WindowStates.m_bPlayerList)
		PlayerList::Draw(&g_GUICtx.m_WindowStates.m_bPlayerList);

	if (g_GUICtx.m_WindowStates.m_bStringTables)
		StringTables::Draw(&g_GUICtx.m_WindowStates.m_bStringTables);

	if (g_GUICtx.m_WindowStates.m_bDumper)
		Dumper::Draw(&g_GUICtx.m_WindowStates.m_bDumper);

	if (g_GUICtx.m_WindowStates.m_bEventLogger)
		EventLogger::Draw(&g_GUICtx.m_WindowStates.m_bEventLogger);

	MenuBar::Draw();

	DrawFileDialogs();
	DrawWelcomeModal();
}

void DrawFileDialogs()
{
	ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);
	if (ImGuiFileDialog::Instance()->Display("SaveConvarFileDlg", ImGuiWindowFlags_NoCollapse, ImVec2(0, 0)))
	{
		if (ImGuiFileDialog::Instance()->IsOk())
		{
			std::string filePathName = ImGuiFileDialog::Instance()->GetFilePathName();
			GUI::Dumper::ConVarList::DumpToJSON(filePathName);
		}

		ImGuiFileDialog::Instance()->Close();
	}

	ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);
	if (ImGuiFileDialog::Instance()->Display("SaveCommandsFileDlg", ImGuiWindowFlags_NoCollapse, ImVec2(0, 0)))
	{
		if (ImGuiFileDialog::Instance()->IsOk())
		{
			std::string filePathName = ImGuiFileDialog::Instance()->GetFilePathName();
			GUI::Dumper::CommandList::DumpToJSON(filePathName);
		}

		ImGuiFileDialog::Instance()->Close();
	}
}

void DrawWelcomeModal()
{
	if (!g_CS2ServerGUI.m_config.m_bWelcomeSeen)
	{
		g_CS2ServerGUI.m_config.m_bWelcomeSeen = true;
		g_CS2ServerGUI.m_config.SaveConfig();

		ImGui::OpenPopup("Welcome");
	}

	ImVec2 center = ImGui::GetMainViewport()->GetCenter();
	ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

	if (ImGui::BeginPopupModal("Welcome", NULL, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::Text("CS2ServerGUI is a debugging tool designed for development purposes.");
		ImGui::Text("It is not intended for use in production environments.");
		ImGui::Separator();
		ImGui::Spacing();

		if (ImGui::Button("I understand")) { ImGui::CloseCurrentPopup(); }
		ImGui::EndPopup();
	}
}

} // namespace GUI