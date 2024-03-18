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