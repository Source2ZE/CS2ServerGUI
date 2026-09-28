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

#include <string>

class CServerSideClient;
class CSVCMsg_PacketEntities;

// Decodes the entity_data bitstream of svc_PacketEntities.
// Packet framing (entity headers, out-of-PVS stream, non-transmit list, explicit deletions) is parsed here,
// field paths and values of each entity are read by networksystem's own CFlattenedSerializer so they always
// match the encoders the server used.
namespace EntityDecoder
{
bool Init();
void Shutdown();

// Safe to call from snapshot worker threads
std::string Decode(CServerSideClient* client, const CSVCMsg_PacketEntities& msg);
}
