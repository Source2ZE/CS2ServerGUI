#include <imgui.h>
#include "main.h"
#include "panels/entitybrowser/entitybrowser.h"
#include "panels/menubar/menubar.h"
#include "panels/playerlist/playerlist.h"
#include "panels/stringtables/stringtables.h"
#include "panels/dumper/dumper.h"
#include "panels/dumper/convarlist/convarlist.h"
#include <ImGuiFileDialog.h>

namespace GUI
{

void DrawFileDialogs();

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

	MenuBar::Draw();

	DrawFileDialogs();
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
}

} // namespace GUI