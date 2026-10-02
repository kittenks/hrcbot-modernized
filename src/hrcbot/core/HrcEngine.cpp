// HrcEngine.cpp - engine interface globals and trace helpers.
#include "HrcEngine.h"
#include <stdio.h>
#include <stdarg.h>
#include <bspflags.h>
#include <engine/IEngineTrace.h>
#include <eiface.h>

namespace hrc
{

IVEngineServer *g_engine = NULL;
IServerGameClients *g_gameClients = NULL;
IServerGameDLL *g_gameDll = NULL;
IServerGameEnts *g_gameEnts = NULL;
IEngineTrace *g_trace = NULL;
IPlayerInfoManager *g_playerInfoManager = NULL;
IBotManager *g_botManager = NULL;
IGameEventManager2 *g_gameEvents = NULL;
IServerPluginHelpers *g_helpers = NULL;
ICvar *g_cvar = NULL;
CGlobalVars *g_globals = NULL;
IFileSystem *g_pFileSystem = NULL;

const Vector g_playerMins(-16.0f, -16.0f, 0.0f);
const Vector g_playerMaxs(16.0f, 16.0f, 72.0f);

bool EngineReady()
{
	return g_engine && g_trace && g_playerInfoManager && g_globals;
}

// Navigation probes only collide with the world and static props; players and
// other dynamic entities are ignored (the ignore edict is kept in the public
// signature for call-site readability but is implicitly skipped).
static inline CTraceFilterWorldAndPropsOnly &WorldFilter(edict_t *ignore)
{
	static CTraceFilterWorldAndPropsOnly s_filter;
	(void)ignore;
	return s_filter;
}

void TraceHull(const Vector &start, const Vector &end, const Vector &mins,
               const Vector &maxs, unsigned int mask, edict_t *ignore,
               trace_t *result)
{
	if (!g_trace)
	{
		if (result)
			result->fraction = 1.0f;
		return;
	}
	Ray_t ray;
	ray.Init(start, end, mins, maxs);
	g_trace->TraceRay(ray, mask, &WorldFilter(ignore), result);
}

bool IsPositionStuck(const Vector &origin, edict_t *ignore)
{
	Vector start = origin;
	Vector end = origin;
	trace_t tr;
	TraceHull(start, end, g_playerMins, g_playerMaxs, MASK_PLAYERSOLID,
	          ignore, &tr);
	return tr.startsolid;
}

bool GroundHeight(const Vector &at, float &groundZ, edict_t *ignore,
                  float probeDown)
{
	Vector start(at.x, at.y, at.z);
	Vector end(at.x, at.y, at.z - probeDown);
	trace_t tr;
	TraceHull(start, end, g_playerMins, g_playerMaxs, MASK_PLAYERSOLID,
	          ignore, &tr);
	if (tr.fraction >= 1.0f && !tr.startsolid)
		return false;
	groundZ = tr.endpos.z;
	return true;
}

bool HasLineOfSight(const Vector &from, const Vector &to, edict_t *ignore)
{
	if (!g_trace)
		return true;
	Ray_t ray;
	ray.Init(from, to);
	trace_t tr;
	g_trace->TraceRay(ray, MASK_SOLID | MASK_VISIBLE, &WorldFilter(ignore),
	                  &tr);
	return tr.fraction >= 1.0f && !tr.startsolid;
}

bool CanStandMove(const Vector &a, const Vector &b, edict_t *ignore)
{
	trace_t tr;
	TraceHull(a, b, g_playerMins, g_playerMaxs, MASK_PLAYERSOLID, ignore, &tr);
	return tr.fraction >= 1.0f && !tr.startsolid;
}

void ClientCommandSafe(edict_t *who, const char *fmt, ...)
{
	if (!g_engine || !who)
		return;
	char buffer[512];
	va_list args;
	va_start(args, fmt);
	vsnprintf(buffer, sizeof(buffer), fmt, args);
	va_end(args);
	g_engine->ClientCommand(who, "%s", buffer);
}

} // namespace hrc
