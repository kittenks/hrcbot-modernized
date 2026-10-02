// GameManager.h
// Owns the bots, the navigation data and the per-level lifecycle.  The
// original tree split this into GameManager_Bots.cpp and
// GameManager_Players.cpp; the modern tree keeps both responsibilities in one
// class and exposes the same hrcbot_* console commands.
#ifndef HRCBOT_CORE_GAMEMANAGER_H_
#define HRCBOT_CORE_GAMEMANAGER_H_

#include <mathlib/vector.h>
#include "containers/TCollection.h"
#include "containers/TList.h"
#include "map/Navigation.h"
#include "map/Dedale.h"
#include "map/Poseidon.h"

class edict_t;
class CCommand;

namespace hrc
{

class Bot;

class GameManager
{
public:
	GameManager();
	~GameManager();

	// Plugin lifecycle.
	void OnServerActivate(edict_t *edictList, int edictCount, int maxClients);
	void OnLevelInit(const char *mapName);
	void OnLevelShutdown();
	void OnGameFrame();
	void OnClientActive(edict_t *who);
	void OnClientDisconnect(edict_t *who);

	// Bot management.
	Bot *AddBot(int team = -1, const char *forcedName = NULL);
	bool KickBot(int team = -1);
	void KickAllBots();
	int BotCount() const { return m_bots.Count(); }
	int HumanPlayerCount() const;

	// Console commands.
	void CmdAdd(const CCommand &args);
	void CmdKick(const CCommand &args);
	void CmdDo(const CCommand &args);
	void CmdInfo(const CCommand &args);
	void CmdFire(const CCommand &args);
	void CmdMove(const CCommand &args);
	void CmdAnalyseGround(const CCommand &args);

	void ReloadNames();
	const char *PickName();
	int PickTeam(int requested) const;

	bool IsTeamPlay() const { return m_teamPlay; }
	int MapCrc() const { return m_mapCrc; }

private:
	void MaintainPopulation();
	void AnalyseOrLoad();

	TCollection<Bot> m_bots;
	TList<char *> m_names;

	Network m_network;
	NetworkRaster m_raster;
	Dedale *m_dedale;
	Poseidon *m_poseidon;

	char m_mapName[64];
	int m_mapCrc;
	int m_maxClients;
	bool m_teamPlay;
	bool m_levelLoaded;
	bool m_analysed;
	float m_nextMaintenance;
};

// Single global instance used by the plugin.
extern GameManager *g_gameManager;

} // namespace hrc

#endif // HRCBOT_CORE_GAMEMANAGER_H_
