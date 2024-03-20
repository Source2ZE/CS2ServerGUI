#include "config.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <tier0/dbg.h>

using json = nlohmann::json;

void PluginConfig::LoadConfig()
{
	std::ifstream file(m_path);

	if (!file.is_open())
		return;

	try
	{
		json j;
		j << file;
		file.close();

		m_bWelcomeSeen = j["WelcomeSeen"];
	}
	catch (const std::exception& e)
	{
		ConMsg("Exception: %s", e.what());
	}
}
void PluginConfig::SaveConfig()
{
	std::ofstream file(m_path);

	if (!file.is_open())
		return;

	json j = {
		{"WelcomeSeen", m_bWelcomeSeen}
	};

	file << j.dump(2);
	file.close();
}