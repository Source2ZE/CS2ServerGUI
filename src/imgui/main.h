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

#include <atomic>

namespace GUI
{

struct WindowStates
{
	bool m_bEntityBrowser = true;
	bool m_bPlayerList = false;
	bool m_bStringTables = false;
	bool m_bDumper = false;
	bool m_bDemoWindow = false;
	bool m_bEventLogger = false;
};

class GUICtx
{
public:
	std::atomic<bool> m_bIsGUIOpen;
	WindowStates m_WindowStates;
};

inline GUICtx g_GUICtx;

void InitializeGUI();

} // namespace GUI