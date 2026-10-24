#include "engine2/ivengineserver2.h"
#include "engine2/isource2server.h"
#include "tier0/icvar.h"

class IGameEventManager2;
class INetworkStringTableContainer;
class IGameEventSystem;
class INetworkMessages;

namespace Interfaces
{

inline ISource2Server* server = NULL;
inline ISource2GameClients* gameclients = NULL;
inline IVEngineServer2* engine = NULL;
inline IGameEventManager2* gameevents = NULL;
inline ICvar* icvar = NULL;
inline INetworkStringTableContainer* networkStringTableContainerServer = NULL;
inline IGameEventSystem* gameEventSystem = NULL;
inline INetworkMessages* networkMessages = NULL;

} // namespace Interfaces
