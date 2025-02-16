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

#include "actions.h"
#include "imgui/main.h"
#include "interfaces.h"
#include <imgui.h>

namespace GUI::Actions
{

static uint64 g_iFlagsToRemove = (FCVAR_HIDDEN | FCVAR_DEVELOPMENTONLY | FCVAR_DEFENSIVE);

void Draw()
{
	if (ImGui::MenuItem("Unlock all commands"))
	{
		if (!Interfaces::icvar)
			return;

		ConCommandData* data = g_pCVar->GetConCommandData(ConCommandRef());
		for (ConCommandRef ref = ConCommandRef((uint16)0); ref.GetRawData() != data; ref = ConCommandRef(ref.GetAccessIndex() + 1))
		{
			ref.RemoveFlags(g_iFlagsToRemove);
		}
	}

	if (ImGui::MenuItem("Unlock all convars"))
	{
		if (!Interfaces::icvar)
			return;

		for (ConVarRefAbstract ref(ConVarRef((uint16)0)); ref.IsValidRef(); ref = ConVarRefAbstract(ConVarRef(ref.GetAccessIndex() + 1)))
		{
			ref.RemoveFlags(g_iFlagsToRemove);
		}
	}
}

} // namespace GUI::PlayerList