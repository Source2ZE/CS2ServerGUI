/**
 * =============================================================================
 * CS2Fixes
 * Copyright (C) 2023 Source2ZE
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

#include "../schema.h"
#include "ehandle.h"

class CGameSceneNode
{
public:
	DECLARE_SCHEMA_CLASS(CGameSceneNode)

	SCHEMA_FIELD(QAngle, m_angRotation)
	SCHEMA_FIELD(float, m_flScale)
	SCHEMA_FIELD(float, m_flAbsScale)
	SCHEMA_FIELD(Vector, m_vecAbsOrigin)
	SCHEMA_FIELD(QAngle, m_angAbsRotation)
	SCHEMA_FIELD(Vector, m_vRenderOrigin)
};

class CBodyComponent
{
public:
	DECLARE_SCHEMA_CLASS(CBodyComponent)

	SCHEMA_FIELD(CGameSceneNode*, m_pSceneNode)
};

class C_CSGameRulesProxy
{
public:
	DECLARE_SCHEMA_CLASS(C_CSGameRulesProxy)

	SCHEMA_FIELD(void*, m_pGameRules)
};

class Z_CBaseEntity : public CEntityInstance
{
public:
	typedef Z_CBaseEntity ThisClass;
	static constexpr const char *ThisClassName = "C_BaseEntity";
	static constexpr bool IsStruct = false;

	SCHEMA_FIELD(CBodyComponent*, m_CBodyComponent)
	SCHEMA_FIELD(Vector, m_vecAbsVelocity)

	Vector GetAbsOrigin() { return m_CBodyComponent()->m_pSceneNode()->m_vecAbsOrigin(); }
};
