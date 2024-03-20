#include "eventlogger.h"
#include "imgui/main.h"
#include "entity2/entitysystem.h"
#include "interfaces.h"
#include <string>
#include <imgui.h>
#include <vector>

namespace GUI::EventLogger
{

struct EventLog
{
	std::string name;
	std::string data;
	bool ingress;
};

static std::map<size_t, EventLog> vecEventLogs;
static std::mutex eventLogLock;
static size_t globalId = 0;
static size_t selectedId = -1;
static bool paused = false;

void AddEventLog(std::string&& name, std::string&& data, bool ingress)
{
	if(paused)
		return;

	std::lock_guard<std::mutex> lock(eventLogLock);
	vecEventLogs[globalId++] = { std::move(name), std::move(data), ingress };

	if(vecEventLogs.size() > 500)
		vecEventLogs.erase(vecEventLogs.begin());
}

void ClearEvents()
{
	std::lock_guard<std::mutex> lock(eventLogLock);
	vecEventLogs.clear();
}

void DrawTable(bool ingress)
{
	static ImGuiTextFilter logFilter;

	logFilter.Draw("Search");

	if (ImGui::BeginTable("Event Table", 1))
	{
		ImGui::TableSetupColumn("Name");
		ImGui::TableHeadersRow();

		size_t i = vecEventLogs.size() - 1;
		{
			std::lock_guard<std::mutex> lock(eventLogLock);

			std::map<size_t, EventLog>::reverse_iterator rit;
			for (rit = vecEventLogs.rbegin(); rit != vecEventLogs.rend(); rit++)
			{
				const auto& event = rit->second;
				if (event.ingress != ingress)
					continue;

				if (!logFilter.PassFilter(event.name.c_str()) && !logFilter.PassFilter(event.data.c_str()))
					continue;

				ImGui::TableNextRow();
				ImGui::TableNextColumn();

				ImGui::PushID(rit->first);
				if (ImGui::Selectable(event.name.c_str(), rit->first == selectedId, ImGuiSelectableFlags_SpanAllColumns))
				{
					selectedId = rit->first;
				}
				ImGui::PopID();
			}
		}

		ImGui::EndTable();
	}
}

void Draw(bool* isOpen)
{
	ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);
	ImGui::Begin("Event Logger", isOpen);

	int availableHeight = ImGui::GetWindowSize().y - 50;

	ImGui::BeginChild("Event List", ImVec2(0, availableHeight / 2), true);

	if (ImGui::Button("Clear"))
		ClearEvents();

	ImGui::SameLine();
	ImGui::Checkbox("Pause", &paused);

	if (ImGui::BeginTabBar("Event Logger Tab"))
	{
		if (ImGui::BeginTabItem("Ingress"))
		{
			DrawTable(true);
			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("Egress"))
		{
			DrawTable(false);
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}


	ImGui::EndChild();

	if (selectedId != -1)
	{
		ImGui::PushID(selectedId);
		ImGui::BeginChild("Entity Info", ImVec2(0, availableHeight / 2), true);

		if (vecEventLogs.find(selectedId) != vecEventLogs.end())
		{
			const auto& event = vecEventLogs[selectedId];
			ImGui::Text("%s", event.data.c_str());
		}

		ImGui::EndChild();
		ImGui::PopID();
	}


	ImGui::End();
}

} // namespace GUI::EventLogger