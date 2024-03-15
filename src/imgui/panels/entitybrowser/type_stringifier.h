#include <string>

class CSchemaType;

namespace GUI::EntityBrowser::TypeStringifier
{

void DumpStringValue(void* ptr, CSchemaType* pType, bool* isNested, const char* fieldName);

} // namespace GUI::EntityBrowser::TypeStringifier