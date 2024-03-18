#include "eiface.h"

class CSchemaSystem;
class IGameEventManager2;
class INetworkStringTableContainer;
class IGameEventSystem;
class INetworkMessages;

namespace Interfaces
{

inline IServerGameDLL* server = NULL;
inline IServerGameClients* gameclients = NULL;
inline IVEngineServer* engine = NULL;
inline IGameEventManager2* gameevents = NULL;
inline ICvar* icvar = NULL;
inline CSchemaSystem* g_pSchemaSystem2 = NULL;
inline INetworkStringTableContainer* networkStringTableContainerServer = NULL;
inline IGameEventSystem* gameEventSystem = NULL;
inline INetworkMessages* networkMessages = NULL;

} // namespace Interfaces