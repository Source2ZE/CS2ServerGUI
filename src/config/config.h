#include <filesystem>

class PluginConfig
{
public:
	void LoadConfig();
	void SaveConfig();
	void SetPath(std::filesystem::path&& path) { m_path = path; }
public:
	bool m_bWelcomeSeen = false;
private:
	std::filesystem::path m_path;
};