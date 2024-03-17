#include <string>

namespace GUI::Dumper::CommandList
{

void Draw();
void DumpToJSON(std::string& path);
std::string PrettifyFlags(uint64_t flags);

} // namespace GUI::Dumper::CommandList