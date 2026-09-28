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

#include "entitydecoder.h"
#include <cstdarg>
#include <cstddef>
#include <cstring>
#include <mutex>
#include <unordered_map>
#include <vector>
#include "interfaces.h"
#include "utils/module.h"
#include "networksystem/iflattenedserializers.h"
#include "entity2/entitysystem.h"
#include "entity2/entityclass.h"
#include "entity2/entityidentity.h"
#include "../protobufs/generated/netmessages.pb.h"

extern CGameEntitySystem* GameEntitySystem();

namespace EntityDecoder
{

// Reverse engineered from engine2.dll / networksystem.dll

// IFlattenedSerializers ("FlattenedSerializersVersion001") vtable indices
constexpr int FS_READFIELDLIST = 4;			// bool (const FlattenedSerializerDesc_t*, bf_read*, CSerializedEntity*, int entindex, int, int)
constexpr int FS_ADDREADFIELDLISTENER = 29;
constexpr int FS_REMOVEREADFIELDLISTENER = 30;

// ISerializedEntities ("SerializedEntitiesVersion001") vtable indices
constexpr int SE_ALLOCATE = 0;				// CSerializedEntity* (const void* name[2], bool, int sizeHint)
constexpr int SE_FREE = 1;					// void (CSerializedEntity*)

constexpr int MAX_SNAPSHOT_ENTITIES = 0x4000;
constexpr int SERIAL_NUM_BITS = 17;
constexpr size_t MAX_OUTPUT_SIZE = 512 * 1024;

// public/server_class.h, which can't be included since the SDK lacks dt_send.h
struct ServerClassLayout
{
	const char* m_pDLLClassName;
	const char* m_pNetworkName;
	CEntityClass* m_pEntityClass;
	FlattenedSerializerDesc_t m_flattenedSerializer;
	int m_ClassID;
};
static_assert(offsetof(ServerClassLayout, m_flattenedSerializer) == 24);
static_assert(offsetof(ServerClassLayout, m_ClassID) == 40);

// tier1 bf_read as used by engine2 and networksystem
struct EngineBitReader
{
	const uint8_t* m_pData;
	int m_nDataBytes;
	int m_nDataBits;
	int m_iCurBit;
	int m_unk14;
	const char* m_pDebugName;
	bool m_bOverflow;
	bool m_bAssertOnOverflow;
	bool m_bDwordReads;					// data may be read as whole dwords, requires m_nDataBytes % 4 == 0
	uint8_t m_pad[29];
};
static_assert(offsetof(EngineBitReader, m_iCurBit) == 16);
static_assert(offsetof(EngineBitReader, m_bOverflow) == 32);
static_assert(offsetof(EngineBitReader, m_bDwordReads) == 34);

// Record networksystem passes to read field list listeners for every field read (see fs_spew_readfieldlist)
struct ReadFieldRecord_t
{
	int m_nEntIndex;
	uint8_t m_fieldPath[32];
	const char* m_pszFieldName;
	const char* m_pszFullName;			// dotted path including parent fields and [array indices]
	const char* m_pszEncoderInfo;
	const char* m_pszBitCount;
	const char* m_pszValue;
	const char* m_pszFieldFunc;
	const char* m_pszDecoder;
	const char* m_pszSerializerName;
	const char* m_pszVarType;
	const char* m_pszExtra;
	int m_unk120;
	int m_nEntIndex2;
	int m_unk128;
	int m_nArrayIndex;
	int m_nStartBit;
	int m_nEndBit;
};
static_assert(offsetof(ReadFieldRecord_t, m_pszFieldName) == 40);
static_assert(offsetof(ReadFieldRecord_t, m_pszValue) == 72);
static_assert(offsetof(ReadFieldRecord_t, m_pszVarType) == 104);
static_assert(offsetof(ReadFieldRecord_t, m_nStartBit) == 136);

// Matches the vtable of networksystem's CFlattenedSerializerReadFieldListListener
class IReadFieldListListener
{
public:
	virtual ~IReadFieldListListener() {}
	virtual bool WantsReads(uint32 flags) = 0;
	virtual int GetMode() = 0;
	virtual bool WantsEntity(int entindex, int unk) = 0;
	virtual void OnField(const ReadFieldRecord_t* record) = 0;
	virtual void OnFinished(int logChannel, int entindex, int bits) = 0;
};

struct DecodedField
{
	std::string name;
	std::string type;
	std::string value;
	std::string bits;
};

thread_local std::vector<DecodedField>* t_pFieldCollector = nullptr;

class CFieldCollector : public IReadFieldListListener
{
public:
	bool WantsReads(uint32 flags) override { return (flags & 1) && t_pFieldCollector; }
	int GetMode() override { return 0; }
	bool WantsEntity(int entindex, int unk) override { return t_pFieldCollector != nullptr; }
	void OnField(const ReadFieldRecord_t* record) override
	{
		if (!t_pFieldCollector)
			return;

		auto str = [](const char* psz) { return std::string(psz ? psz : ""); };
		const char* name = record->m_pszFullName && *record->m_pszFullName ? record->m_pszFullName : record->m_pszFieldName;
		t_pFieldCollector->push_back({str(name), str(record->m_pszVarType), str(record->m_pszValue), str(record->m_pszBitCount)});
	}
	void OnFinished(int logChannel, int entindex, int bits) override {}
};

CFieldCollector g_fieldCollector;

void* g_pFlattenedSerializers = nullptr;
void* g_pSerializedEntities = nullptr;
bool g_bListenerInstalled = false;

std::mutex g_decodeLock;
std::vector<ServerClassLayout*> g_classesById;
int g_nClassBits = 0;
std::unordered_map<CServerSideClient*, std::vector<ServerClassLayout*>> g_clientEntities;

template <typename R, typename... Args>
R CallVirtual(void* pThis, int index, Args... args)
{
	return (*reinterpret_cast<R(***)(void*, Args...)>(pThis))[index](pThis, args...);
}

class CBitReader
{
public:
	CBitReader(const std::string& data)
	{
		// pad to whole dwords so networksystem can use its fast dword reads
		m_storage.resize(data.size() / 4 + 2, 0);
		if (!data.empty())
			memcpy(m_storage.data(), data.data(), data.size());

		memset(&m_buf, 0, sizeof(m_buf));
		m_buf.m_pData = reinterpret_cast<const uint8_t*>(m_storage.data());
		m_buf.m_nDataBytes = (int)(m_storage.size() * 4);
		m_buf.m_nDataBits = (int)(data.size() * 8);
		m_buf.m_bDwordReads = true;
	}

	bool ReadBit()
	{
		if (m_buf.m_iCurBit >= m_buf.m_nDataBits)
		{
			m_buf.m_bOverflow = true;
			return false;
		}

		bool bit = (m_buf.m_pData[m_buf.m_iCurBit >> 3] >> (m_buf.m_iCurBit & 7)) & 1;
		m_buf.m_iCurBit++;
		return bit;
	}

	uint32 ReadUBitLong(int numBits)
	{
		uint32 ret = 0;
		for (int i = 0; i < numBits; i++)
			ret |= (uint32)ReadBit() << i;
		return ret;
	}

	uint32 ReadUBitVar()
	{
		uint32 ret = ReadUBitLong(6);
		switch (ret & (16 | 32))
		{
		case 16:
			ret = (ret & 15) | (ReadUBitLong(4) << 4);
			break;
		case 32:
			ret = (ret & 15) | (ReadUBitLong(8) << 4);
			break;
		case 48:
			ret = (ret & 15) | (ReadUBitLong(32 - 4) << 4);
			break;
		}
		return ret;
	}

	uint32 ReadVarInt32()
	{
		uint32 ret = 0;
		for (int shift = 0; shift < 35; shift += 7)
		{
			uint32 b = ReadUBitLong(8);
			ret |= (b & 0x7F) << shift;
			if (!(b & 0x80) || IsOverflowed())
				break;
		}
		return ret;
	}

	int GetNumBitsRead() const { return m_buf.m_iCurBit; }
	int GetNumBitsLeft() const { return m_buf.m_nDataBits - m_buf.m_iCurBit; }
	bool IsOverflowed() const { return m_buf.m_bOverflow; }
	void SeekToBit(int bit)
	{
		m_buf.m_iCurBit = bit;
		m_buf.m_bOverflow = bit < 0 || bit > m_buf.m_nDataBits;
	}
	EngineBitReader* GetEngineReader() { return &m_buf; }

private:
	std::vector<uint32> m_storage;
	EngineBitReader m_buf;
};

void* GetInterface(CModule& module, const char* name)
{
	CreateInterfaceFn fn = (CreateInterfaceFn)dlsym(module.m_hModule, "CreateInterface");
	return fn ? fn(name, nullptr) : nullptr;
}

bool Init()
{
	CModule networkSystem(ROOTBIN, "networksystem");
	g_pFlattenedSerializers = GetInterface(networkSystem, "FlattenedSerializersVersion001");
	g_pSerializedEntities = GetInterface(networkSystem, "SerializedEntitiesVersion001");

	if (!g_pFlattenedSerializers || !g_pSerializedEntities)
		return false;

	CallVirtual<void>(g_pFlattenedSerializers, FS_ADDREADFIELDLISTENER, static_cast<IReadFieldListListener*>(&g_fieldCollector));
	g_bListenerInstalled = true;
	return true;
}

void Shutdown()
{
	std::lock_guard<std::mutex> lock(g_decodeLock);

	if (g_bListenerInstalled)
		CallVirtual<void>(g_pFlattenedSerializers, FS_REMOVEREADFIELDLISTENER, static_cast<IReadFieldListListener*>(&g_fieldCollector));

	g_bListenerInstalled = false;
	g_pFlattenedSerializers = nullptr;
	g_pSerializedEntities = nullptr;
	g_clientEntities.clear();
}

void RefreshServerClasses()
{
	CUtlVector<ServerClass*>* pClasses = Interfaces::server->GetAllServerClasses();
	if (!pClasses || pClasses->Count() == (int)g_classesById.size())
		return;

	g_classesById.assign(pClasses->Count(), nullptr);
	for (int i = 0; i < pClasses->Count(); i++)
	{
		ServerClassLayout* pClass = reinterpret_cast<ServerClassLayout*>(pClasses->Element(i));
		if (pClass && pClass->m_ClassID >= 0 && pClass->m_ClassID < (int)g_classesById.size())
			g_classesById[pClass->m_ClassID] = pClass;
	}

	// client uses Q_log2(CSVCMsg_ServerInfo::max_classes) + 1
	int bits = 0;
	for (uint32 count = pClasses->Count(); count > 1; count >>= 1)
		bits++;
	g_nClassBits = bits + 1;
}

ServerClassLayout* GetServerEntityClass(int entindex)
{
	CGameEntitySystem* pEntitySystem = GameEntitySystem();
	if (!pEntitySystem)
		return nullptr;

	CEntityIdentity* pIdentity = pEntitySystem->GetEntityIdentity(CEntityIndex(entindex));
	if (!pIdentity || !pIdentity->m_pClass)
		return nullptr;

	return reinterpret_cast<ServerClassLayout*>(pIdentity->m_pClass->m_pServerClass);
}

const char* GetClassName(ServerClassLayout* pClass)
{
	return pClass && pClass->m_pNetworkName ? pClass->m_pNetworkName : "<unknown>";
}

class CPacketEntitiesDecoder
{
public:
	CPacketEntitiesDecoder(CServerSideClient* client, const CSVCMsg_PacketEntities& msg) :
		m_msg(msg), m_entities(g_clientEntities[client])
	{
		if (m_entities.size() != MAX_SNAPSHOT_ENTITIES)
			m_entities.assign(MAX_SNAPSHOT_ENTITIES, nullptr);
	}

	std::string Decode()
	{
		// full updates make the client drop every entity it had
		bool bIsDelta = m_msg.has_delta_from();
		if (!bIsDelta)
			std::fill(m_entities.begin(), m_entities.end(), nullptr);

		ReadBitCounts();

		const void* name[2] = {nullptr, nullptr};
		m_pSerializedEntity = CallVirtual<void*>(g_pSerializedEntities, SE_ALLOCATE, (const void*)name, false, 0);

		CBitReader entityData(m_msg.entity_data());
		bool bOk = ReadMainStream(entityData);

		// the client keeps reading from whichever buffer it parsed last, so explicit deletions
		// follow the out of pvs updates when those are present
		CBitReader outOfPVS(m_msg.outofpvs_entity_updates().data());
		CBitReader* pLastReader = &entityData;
		if (bOk && m_msg.has_outofpvs_entity_updates())
		{
			bOk = ReadOutOfPVSStream(outOfPVS);
			pLastReader = &outOfPVS;
		}

		if (bOk)
		{
			if (m_msg.has_non_transmitted_entities())
				ReadNonTransmitted();
			else if (bIsDelta)
				ReadExplicitDeletions(*pLastReader);
		}

		if (m_pSerializedEntity)
			CallVirtual<void>(g_pSerializedEntities, SE_FREE, m_pSerializedEntity);

		return std::move(m_out);
	}

private:
	void Print(const char* fmt, ...)
	{
		if (m_bTruncated)
			return;

		char buf[2048];
		va_list args;
		va_start(args, fmt);
		V_vsnprintf(buf, sizeof(buf), fmt, args);
		va_end(args);

		if (m_out.size() + strlen(buf) > MAX_OUTPUT_SIZE)
		{
			m_out += "... output truncated\n";
			m_bTruncated = true;
			return;
		}

		m_out += buf;
	}

	void ReadBitCounts()
	{
		// optional per entity field bit counts, the client uses them to decode entities in parallel
		if (!m_msg.has_serialized_entities())
			return;

		CBitReader reader(m_msg.serialized_entities());
		while (reader.GetNumBitsLeft() >= 8 && !reader.IsOverflowed())
			m_bitCounts.push_back((int)reader.ReadVarInt32());
	}

	// Returns false if the stream can no longer be followed
	bool ReadEntityFields(CBitReader& reader, const char* action, int entindex, ServerClassLayout* pClass, const char* extra)
	{
		int startBit = reader.GetNumBitsRead();
		int expectedBits = m_nextBitCount < m_bitCounts.size() ? m_bitCounts[m_nextBitCount++] : -1;

		std::vector<DecodedField> fields;
		bool bRead = false;
		if (pClass && m_pSerializedEntity)
		{
			FlattenedSerializerDesc_t desc = pClass->m_flattenedSerializer;
			t_pFieldCollector = &fields;
			bRead = CallVirtual<bool>(g_pFlattenedSerializers, FS_READFIELDLIST, &desc, reader.GetEngineReader(), m_pSerializedEntity, entindex, 1, 1);
			t_pFieldCollector = nullptr;
		}

		int readBits = reader.GetNumBitsRead() - startBit;
		bool bOk = bRead && !reader.IsOverflowed();

		Print("[%s] #%d %s%s - %d fields, %d bits\n", action, entindex, GetClassName(pClass), extra, (int)fields.size(), bOk ? readBits : expectedBits);
		for (const DecodedField& field : fields)
			Print("    %s = %s  (%s, %s bits)\n", field.name.c_str(), field.value.c_str(), field.type.c_str(), field.bits.c_str());

		if (expectedBits >= 0 && (!bOk || readBits != expectedBits))
		{
			if (bOk)
				Print("    ! read %d bits but server reported %d, resyncing\n", readBits, expectedBits);
			else
				Print("    ! failed to read fields, skipping %d bits\n", expectedBits);

			reader.SeekToBit(startBit + expectedBits);
			return !reader.IsOverflowed();
		}

		if (!bOk)
		{
			Print("    ! failed to read fields of %s, stopping\n", GetClassName(pClass));
			return false;
		}

		return true;
	}

	bool ReadNewEntity(CBitReader& reader, const char* action, int entindex)
	{
		uint32 classId = reader.ReadUBitLong(g_nClassBits);
		uint32 serial = reader.ReadUBitLong(SERIAL_NUM_BITS);
		uint32 unk = reader.ReadVarInt32();

		ServerClassLayout* pClass = classId < g_classesById.size() ? g_classesById[classId] : nullptr;
		if (!pClass || reader.IsOverflowed())
		{
			Print("[%s] #%d invalid class id %u, stopping\n", action, entindex, classId);
			return false;
		}

		m_entities[entindex] = pClass;

		char extra[64];
		V_snprintf(extra, sizeof(extra), " (class %u, serial %u, unk %u)", classId, serial, unk);
		return ReadEntityFields(reader, action, entindex, pClass, extra);
	}

	bool ReadExistingEntity(CBitReader& reader, const char* action, int entindex)
	{
		ServerClassLayout* pClass = m_entities[entindex];
		if (!pClass)
		{
			// we missed its create (plugin loaded late), use the server side entity
			pClass = GetServerEntityClass(entindex);
			m_entities[entindex] = pClass;
		}

		return ReadEntityFields(reader, action, entindex, pClass, "");
	}

	bool ReadEntityIndex(CBitReader& reader, int& entindex)
	{
		int next = entindex + 1 + (int)reader.ReadUBitVar();
		if (reader.IsOverflowed() || next >= MAX_SNAPSHOT_ENTITIES || next <= entindex)
		{
			Print("! bad entity header after #%d\n", entindex);
			return false;
		}

		entindex = next;
		return true;
	}

	bool ReadMainStream(CBitReader& reader)
	{
		int entindex = -1;
		for (int i = 0; i < m_msg.updated_entries(); i++)
		{
			if (!ReadEntityIndex(reader, entindex))
				return false;

			bool bLeavePVS = reader.ReadBit();
			bool bSecond = reader.ReadBit();

			if (!bLeavePVS)
			{
				if (!(bSecond ? ReadNewEntity(reader, "create", entindex) : ReadExistingEntity(reader, "update", entindex)))
					return false;
			}
			else
			{
				ServerClassLayout* pClass = m_entities[entindex] ? m_entities[entindex] : GetServerEntityClass(entindex);
				Print("[%s] #%d %s\n", bSecond ? "delete" : "leave pvs", entindex, GetClassName(pClass));
				m_entities[entindex] = nullptr;
			}
		}

		return true;
	}

	bool ReadOutOfPVSStream(CBitReader& reader)
	{
		int entindex = -1;
		for (int i = 0; i < m_msg.outofpvs_entity_updates().count(); i++)
		{
			if (!ReadEntityIndex(reader, entindex))
				return false;

			bool bCreate = reader.ReadBit();
			if (!(bCreate ? ReadNewEntity(reader, "create out of pvs", entindex) : ReadExistingEntity(reader, "update out of pvs", entindex)))
				return false;
		}

		return true;
	}

	void ReadNonTransmitted()
	{
		const auto& nonTransmitted = m_msg.non_transmitted_entities();
		CBitReader reader(nonTransmitted.data());

		std::string list;
		int entindex = -1;
		for (int i = 0; i < nonTransmitted.header_count(); i++)
		{
			if (!ReadEntityIndex(reader, entindex))
				break;

			if (reader.ReadBit())
				list += " #" + std::to_string(entindex);
		}

		Print("[not transmitted]%s\n", list.empty() ? " none" : list.c_str());
	}

	void ReadExplicitDeletions(CBitReader& reader)
	{
		uint32 count = reader.ReadUBitVar();
		int entindex = -1;
		for (uint32 i = 0; i < count && !reader.IsOverflowed(); i++)
		{
			entindex += (int)reader.ReadUBitVar();
			if (entindex < 0 || entindex >= MAX_SNAPSHOT_ENTITIES)
				break;

			Print("[delete] #%d %s\n", entindex, GetClassName(m_entities[entindex]));
			m_entities[entindex] = nullptr;
		}
	}

	const CSVCMsg_PacketEntities& m_msg;
	std::vector<ServerClassLayout*>& m_entities;
	std::vector<int> m_bitCounts;
	size_t m_nextBitCount = 0;
	void* m_pSerializedEntity = nullptr;
	std::string m_out;
	bool m_bTruncated = false;
};

std::string Decode(CServerSideClient* client, const CSVCMsg_PacketEntities& msg)
{
	std::lock_guard<std::mutex> lock(g_decodeLock);

	if (!g_bListenerInstalled)
		return "entity decoder unavailable\n";

	RefreshServerClasses();
	if (g_classesById.empty())
		return "no server classes\n";

	return CPacketEntitiesDecoder(client, msg).Decode();
}

} // namespace EntityDecoder
