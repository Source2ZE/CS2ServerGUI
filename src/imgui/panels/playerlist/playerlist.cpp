#include "playerlist.h"
#include "imgui/main.h"
#include "imgui/panels/entitybrowser/entitybrowser.h"
#include "cs2_sdk/entity/cbaseplayercontroller.h"
#include "entity2/entitysystem.h"
#include "interfaces.h"
#include <string>
#include <imgui.h>

CGlobalVars* GetGameGlobals();

namespace GUI::PlayerList
{
std::string ConnectStateToString(PlayerConnectedState connectState)
{
	switch (connectState)
	{
	case PlayerConnectedState::PlayerNeverConnected:
		return "Never Connected";
	case PlayerConnectedState::PlayerConnected:
		return "Connected";
	case PlayerConnectedState::PlayerConnecting:
		return "Connecting";
	case PlayerConnectedState::PlayerDisconnected:
		return "Disconnected";
	case PlayerConnectedState::PlayerDisconnecting:
		return "Disconnecting";
	case PlayerConnectedState::PlayerReconnecting:
		return "Reconnecting";
	case PlayerConnectedState::PlayerReserved:
		return "Reserved";
	default:
		return "<Unknown>";
	}
}

void Draw(bool* isOpen)
{
	ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);
	ImGui::Begin("Player List", isOpen);

	if (ImGui::BeginTable("Players", 5))
	{
		ImGui::TableSetupColumn("Slot", ImGuiTableColumnFlags_WidthFixed);
		ImGui::TableSetupColumn("UserID", ImGuiTableColumnFlags_WidthFixed);
		ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed);
		ImGui::TableSetupColumn("SteamID", ImGuiTableColumnFlags_WidthFixed);
		ImGui::TableSetupColumn("State", ImGuiTableColumnFlags_WidthFixed);
		ImGui::TableHeadersRow();

		auto gpGlobals = GetGameGlobals();
		if (!gpGlobals)
			return;

		for (int i = 1; i <= gpGlobals->maxClients; i++)
		{
			auto controller = static_cast<CBasePlayerController*>(GameEntitySystem()->GetBaseEntity(CEntityIndex(i)));

			if (!controller)
				continue;

			ImGui::TableNextRow();

			ImGui::TableNextColumn();
			ImGui::Text("%i", i - 1);

			ImGui::TableNextColumn();
			ImGui::Text("%hu", Interfaces::engine->GetPlayerUserId(CPlayerSlot(i-1)).Get());

			ImGui::TableNextColumn();
			ImGui::PushID(i);
			if (ImGui::Selectable(controller->GetPlayerName(), false, ImGuiSelectableFlags_SpanAllColumns))
			{
				GUI::EntityBrowser::g_pSelectedEntity = controller->GetRefEHandle();
				GUI::g_GUICtx.m_WindowStates.m_bEntityBrowser = true;
			}
			ImGui::PopID();

			ImGui::TableNextColumn();
			ImGui::Text("%llu", controller->m_steamID());

			ImGui::TableNextColumn();
			ImGui::Text("%s", ConnectStateToString(controller->m_iConnected()).c_str());
		}

		ImGui::EndTable();
	}

	ImGui::End();
}

} // namespace GUI::PlayerList