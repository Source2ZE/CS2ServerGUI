#include <string>

namespace GUI::EventLogger
{

void AddEventLog(std::string&& name, std::string&& data, bool ingress, std::string&& senderName = "");
void Draw(bool* isOpen);

} // namespace GUI::EventLogger