#include <string>

namespace GUI::EventLogger
{

void AddEventLog(std::string&& name, std::string&& data);
void Draw(bool* isOpen);

} // namespace GUI::EventLogger