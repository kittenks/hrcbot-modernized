// HrcEngine.h
// Central access point to the Source engine interfaces acquired by the
// Metamod plugin at load time.  Navigation and bot code use these helpers
// instead of touching the plugin directly.
#ifndef HRCBOT_CORE_HRCENGINE_H_
#define HRCBOT_CORE_HRCENGINE_H_

#include <eiface.h>
#include <cmodel.h>
#include <engine/IEngineTrace.h>
#include <game/server/iplayerinfo.h>
#include <mathlib/vector.h>

class IServerGameEnts;
class IGameEventManager2;
class IServerPluginHelpers;
class ICvar;
class CGlobalVars;

namespace hrc
{

// Acquired in CHurricaneBotServerPlugin::Load().
extern IVEngineServer *g_engine;
extern IServerGameClients *g_gameClients;
extern IServerGameDLL *g_gameDll;
extern IServerGameEnts *g_gameEnts;
extern IEngineTrace *g_trace;
extern IPlayerInfoManager *g_playerInfoManager;
extern IBotManager *g_botManager;
extern IGameEventManager2 *g_gameEvents;
extern IServerPluginHelpers *g_helpers;
extern ICvar *g_cvar;
extern CGlobalVars *g_globals;

// Player standing hull used for walkability probes.
extern const Vector g_playerMins;
extern const Vector g_playerMaxs;

// True once all interfaces are available.
bool EngineReady();

// Trace a player-sized hull from start to end against world + props.
void TraceHull(const Vector &start, const Vector &end, const Vector &mins,
               const Vector &maxs, unsigned int mask, edict_t *ignore,
               trace_t *result);

// Convenience: is there solid geometry filling the standing hull at origin?
bool IsPositionStuck(const Vector &origin, edict_t *ignore = NULL);

// Ground height under a point (traces down), returns false if no ground.
bool GroundHeight(const Vector &at, float &groundZ,
                  edict_t *ignore = NULL, float probeDown = 64.0f);

// Line of sight between two points (no world/prop geometry in between).
bool HasLineOfSight(const Vector &from, const Vector &to,
                    edict_t *ignore = NULL);

// Returns true when the standing hull can move from a to b directly.
bool CanStandMove(const Vector &a, const Vector &b, edict_t *ignore = NULL);

// Wrapper around engine->ClientCommand that is safe from plugin context.
void ClientCommandSafe(edict_t *who, const char *fmt, ...);

} // namespace hrc

#endif // HRCBOT_CORE_HRCENGINE_H_
