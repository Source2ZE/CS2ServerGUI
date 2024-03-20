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