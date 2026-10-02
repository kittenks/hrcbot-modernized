// hrcbot_mm.h
// Metamod:Source 1.12 plugin entry for the modernized Hurricane Bot.
#ifndef HRCBOT_PLUGIN_HRCBOT_MM_H_
#define HRCBOT_PLUGIN_HRCBOT_MM_H_

#include <ISmmPlugin.h>
#include <igameevents.h>
#include <iplayerinfo.h>
#include <eiface.h>

namespace hrc
{
class GameManager;
}

class CHurricaneBotServerPlugin : public ISmmPlugin,
                                  public IMetamodListener
{
public:
	bool Load(PluginId id, ISmmAPI *ismm, char *error, size_t maxlen,
	          bool late);
	bool Unload(char *error, size_t maxlen);
	bool Pause(char *error, size_t maxlen);
	bool Unpause(char *error, size_t maxlen);
	void AllPluginsLoaded();

public: // IMetamodListener
	void OnVSPListening(IServerPluginCallbacks *iface);

public: // SourceHook callbacks
	bool Hook_LevelInit(const char *pMapName, char const *pMapEntities,
	                   char const *pOldLevel, char const *pLandmarkName,
	                   bool loadGame, bool background);
	void Hook_ServerActivate(edict_t *pEdictList, int edictCount,
	                         int clientMax);
	void Hook_GameFrame(bool simulating);
	void Hook_LevelShutdown(void);
	void Hook_ClientActive(edict_t *pEntity, bool bLoadGame);
	void Hook_ClientDisconnect(edict_t *pEntity);
	void Hook_ClientPutInServer(edict_t *pEntity, char const *playername);
	bool Hook_FireEvent(IGameEvent *pEvent, bool bDontBroadcast);

public: // metadata
	const char *GetAuthor();
	const char *GetName();
	const char *GetDescription();
	const char *GetURL();
	const char *GetLicense();
	const char *GetVersion();
	const char *GetDate();
	const char *GetLogTag();

	// SourceHook callbacks are wired in the .cpp.
};

extern CHurricaneBotServerPlugin g_HurricaneBotPlugin;

PLUGIN_GLOBALVARS();

#endif // HRCBOT_PLUGIN_HRCBOT_MM_H_
