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

#include "movementrecorder.h"
#include "movement/movementrecorder.h"
#include <imgui.h>
#include <imgui-notify/ImGuiNotify.hpp>
#include <ImGuiFileDialog.h>
#include <atomic>
#include <filesystem>
#include <mutex>
#include <thread>

namespace GUI::MovementRecorder
{

namespace Recorder = Movement::Recorder;

static std::atomic<bool> g_bSaving = false;
static std::atomic<bool> g_bSaveFinished = false;
static std::mutex g_SaveResultLock;
static std::string g_SaveResult;
static bool g_bSaveSucceeded = false;

// Writing a long recording takes a while, so it happens off the GUI thread
void SaveToFile(const std::string& path)
{
	if (g_bSaving.exchange(true))
		return;

	std::thread([path]()
	{
		std::string error;
		bool ok = Recorder::Save(path, error);

		{
			std::lock_guard<std::mutex> lock(g_SaveResultLock);
			g_bSaveSucceeded = ok;
			g_SaveResult = ok ? "Saved " + path : error;
		}

		g_bSaving = false;
		g_bSaveFinished = true;
	}).detach();
}

static void DrawFunctionStatus()
{
	if (!ImGui::CollapsingHeader("Hooks"))
		return;

	if (ImGui::BeginTable("Movement Functions", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
	{
		ImGui::TableSetupColumn("Function");
		ImGui::TableSetupColumn("RVA");
		ImGui::TableSetupColumn("Status");
		ImGui::TableHeadersRow();

		for (const auto& function : Recorder::GetFunctionStatus())
		{
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(function.name);
			ImGui::TableNextColumn();

			if (function.rva)
				ImGui::Text("0x%llX", (unsigned long long)function.rva);
			else
				ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "not found");

			ImGui::TableNextColumn();
			ImGui::TextUnformatted(function.hooked ? "hooked" : (function.rva ? "located" : "-"));
		}

		ImGui::EndTable();
	}
}

void Draw(bool* isOpen)
{
	if (g_bSaveFinished.exchange(false))
	{
		std::lock_guard<std::mutex> lock(g_SaveResultLock);
		ImGui::InsertNotification({g_bSaveSucceeded ? ImGuiToastType::Success : ImGuiToastType::Error, 5000, "%s", g_SaveResult.c_str()});
	}

	ImGui::Begin("Movement Recorder", isOpen);

	bool recording = Recorder::IsRecording();

	if (recording)
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.15f, 0.15f, 1.0f));

	if (ImGui::Button(recording ? "Stop recording" : "Record"))
		Recorder::SetRecording(!recording);

	if (recording)
		ImGui::PopStyleColor();

	ImGui::SameLine();

	if (ImGui::Button("Clear"))
		Recorder::Clear();

	ImGui::SameLine();

	bool recordTraces = Recorder::IsRecordingTraces();
	if (ImGui::Checkbox("Record traces", &recordTraces))
		Recorder::SetRecordTraces(recordTraces);

	Recorder::Stats stats = Recorder::GetStats();
	ImGui::Text("%s: %zu commands, %zu slices, %zu traces, ~%.1f MB in memory", recording ? "Recording" : "Stopped", stats.ticks, stats.slices,
		stats.traces, stats.approxBytes / (1024.0 * 1024.0));

	ImGui::Separator();

	static std::string defaultPath;
	if (defaultPath.empty() || !recording)
		defaultPath = (std::filesystem::path(Recorder::GetDefaultDirectory()) / Recorder::GetDefaultFileName()).string();

	ImGui::TextWrapped("Default path: %s", defaultPath.c_str());

	bool saving = g_bSaving;
	ImGui::BeginDisabled(saving || stats.ticks == 0);

	if (ImGui::Button("Save"))
		SaveToFile(defaultPath);

	ImGui::SameLine();

	if (ImGui::Button("Save as..."))
	{
		std::error_code ec;
		std::filesystem::create_directories(Recorder::GetDefaultDirectory(), ec);

		IGFD::FileDialogConfig config;
		config.path = Recorder::GetDefaultDirectory();
		config.fileName = Recorder::GetDefaultFileName();
		config.countSelectionMax = 1;
		config.flags = ImGuiFileDialogFlags_ConfirmOverwrite;
		ImGuiFileDialog::Instance()->OpenDialog("SaveMovementRecordingDlg", "Save movement recording", "JSON File (*.json){.json}", config);
	}

	ImGui::EndDisabled();

	if (saving)
	{
		ImGui::SameLine();
		ImGui::TextUnformatted("Saving...");
	}

	DrawFunctionStatus();

	ImGui::End();
}

} // namespace GUI::MovementRecorder
