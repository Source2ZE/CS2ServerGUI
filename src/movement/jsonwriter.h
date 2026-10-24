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

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace Movement
{

// Streams JSON to a file. Floats are written with 9 significant digits, which reads back to the exact same float.
class JsonWriter
{
public:
	explicit JsonWriter(FILE* file) : m_pFile(file) {}

	void BeginObject() { Value(); Put('{'); m_First.push_back(true); }
	void BeginObject(const char* name) { Key(name); BeginObject(); }
	void EndObject() { m_First.pop_back(); Put('}'); }

	void BeginArray() { Value(); Put('['); m_First.push_back(true); }
	void BeginArray(const char* name) { Key(name); BeginArray(); }
	void EndArray() { m_First.pop_back(); Put(']'); }

	void Key(const char* name)
	{
		Separator();
		String(name);
		Put(':');
		m_bAfterKey = true;
	}

	void Null() { Value(); Write("null"); }
	void Bool(bool value) { Value(); Write(value ? "true" : "false"); }
	void Int(int64_t value) { Value(); Printf("%lld", (long long)value); }
	void UInt(uint64_t value) { Value(); Printf("%llu", (unsigned long long)value); }

	// JSON has no NaN or infinity, so those are written as the strings .NET parses back
	void Float(float value)
	{
		Value();

		if (std::isnan(value))
			Write("\"NaN\"");
		else if (std::isinf(value))
			Write(value > 0 ? "\"Infinity\"" : "\"-Infinity\"");
		else
			Printf("%.9g", (double)value);
	}

	void Str(const char* value)
	{
		Value();
		String(value ? value : "");
	}

	void Vec2(const float* v)
	{
		BeginArray();
		Float(v[0]);
		Float(v[1]);
		EndArray();
	}

	void Vec3(const float* v)
	{
		BeginArray();
		Float(v[0]);
		Float(v[1]);
		Float(v[2]);
		EndArray();
	}

	void Null(const char* name) { Key(name); Null(); }
	void Bool(const char* name, bool value) { Key(name); Bool(value); }
	void Int(const char* name, int64_t value) { Key(name); Int(value); }
	void UInt(const char* name, uint64_t value) { Key(name); UInt(value); }
	void Float(const char* name, float value) { Key(name); Float(value); }
	void Str(const char* name, const char* value) { Key(name); Str(value); }
	void Vec2(const char* name, const float* v) { Key(name); Vec2(v); }
	void Vec3(const char* name, const float* v) { Key(name); Vec3(v); }

private:
	void Put(char c) { fputc(c, m_pFile); }
	void Write(const char* s) { fputs(s, m_pFile); }

	template <typename... Args>
	void Printf(const char* format, Args... args)
	{
		fprintf(m_pFile, format, args...);
	}

	void Separator()
	{
		if (m_First.empty())
			return;

		if (!m_First.back())
			Put(',');

		m_First.back() = false;
	}

	// A value directly after its key needs no separator, one in an array does
	void Value()
	{
		if (m_bAfterKey)
		{
			m_bAfterKey = false;
			return;
		}

		Separator();
	}

	void String(const char* s)
	{
		Put('"');

		for (; *s; s++)
		{
			unsigned char c = (unsigned char)*s;

			switch (c)
			{
			case '"': Write("\\\""); break;
			case '\\': Write("\\\\"); break;
			case '\n': Write("\\n"); break;
			case '\r': Write("\\r"); break;
			case '\t': Write("\\t"); break;
			default:
				if (c < 0x20)
					Printf("\\u%04x", c);
				else
					Put((char)c);
			}
		}

		Put('"');
	}

	FILE* m_pFile;
	std::vector<char> m_First; // per open object or array: no member written yet
	bool m_bAfterKey = false;
};

} // namespace Movement
