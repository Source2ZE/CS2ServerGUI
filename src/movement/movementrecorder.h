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

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// Records every player movement command the server runs, slice by slice, in the
// "s2v-movement-recording" JSON format, so it can be compared with a reimplementation.
namespace Movement::Recorder
{

struct FunctionStatus
{
	const char* name;
	uintptr_t rva; // 0 when the signature was not found
	bool hooked;
};

struct Stats
{
	size_t ticks;
	size_t slices;
	size_t traces;
	size_t approxBytes;
};

// Finds the movement functions in server.dll and hooks them. Call on the main thread.
void Init();
void Shutdown();

void SetRecording(bool recording);
bool IsRecording();

void SetRecordTraces(bool record);
bool IsRecordingTraces();

void Clear();
Stats GetStats();
std::vector<FunctionStatus> GetFunctionStatus();

void SetMapName(const char* mapName);

std::string GetDefaultDirectory();
std::string GetDefaultFileName();

// Thread safe, may run while recording continues
bool Save(const std::string& path, std::string& error);

} // namespace Movement::Recorder
