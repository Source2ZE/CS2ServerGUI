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