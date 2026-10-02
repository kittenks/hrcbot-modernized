// hrcbot_mm.cpp - Metamod:Source 1.12 plugin implementation.
#include <stdio.h>
#include <string.h>
#include "hrcbot_mm.h"

#include <ISmmPlugin.h>
#include <igameevents.h>
#include <iplayerinfo.h>
#include <eiface.h>
#include <tier1/convar.h>
#include <filesystem.h>

#include "hrcbot_version.h"
#include "core/HrcEngine.h"
#include "core/GameManager.h"
#include "core/Tools.h"
#include "hrcbot_cvars.h"

// SourceHook declarations for the game DLL / clients callbacks we consume.
SH_DECL_HOOK6(IServerGameDLL, LevelInit, SH_NOATTRIB, 0, bool, char const *,
              char const *, char const *, char const *, bool, bool);
SH_DECL_HOOK3_void(IServerGameDLL, ServerActivate, SH_NOATTRIB, 0, edict_t *,
                   int, int);
SH_DECL_HOOK1_void(IServerGameDLL, GameFrame, SH_NOATTRIB, 0, bool);
SH_DECL_HOOK0_void(IServerGameDLL, LevelShutdown, SH_NOATTRIB, 0);
SH_DECL_HOOK2_void(IServerGameClients, ClientActive, SH_NOATTRIB, 0, edict_t *,
                   bool);
SH_DECL_HOOK1_void(IServerGameClients, ClientDisconnect, SH_NOATTRIB, 0,
                   edict_t *);
SH_DECL_HOOK2_void(IServerGameClients, ClientPutInServer, SH_NOATTRIB, 0,
                   edict_t *, char const *);
SH_DECL_HOOK2(IGameEventManager2, FireEvent, SH_NOATTRIB, 0, bool,
              IGameEvent *, bool);

CHurricaneBotServerPlugin g_HurricaneBotPlugin;

// Raw MM:S / engine pointers kept at file scope; the hrc:: aliases in
// HrcEngine.cpp are assigned from these in Load().
static IServerGameDLL *s_server = NULL;
static IServerGameClients *s_gameClients = NULL;
static IServerPluginHelpers *s_helpers = NULL;
static IGameEventManager2 *s_gameEvents = NULL;
static IServerPluginCallbacks *s_vspCallbacks = NULL;

PLUGIN_EXPOSE(CHurricaneBotServerPlugin, g_HurricaneBotPlugin);

// ---------------------------------------------------------------------------
// ConVar / command registration
// ---------------------------------------------------------------------------

class CHrcConvarAccessor : public IConCommandBaseAccessor
{
public:
	virtual bool RegisterConCommandBase(ConCommandBase *pBase)
	{
		return META_REGCVAR(pBase);
	}
} s_HrcAccessor;

static void Cmd_Add(const CCommand &args)
{
	if (hrc::g_gameManager)
		hrc::g_gameManager->CmdAdd(args);
}
static void Cmd_Kick(const CCommand &args)
{
	if (hrc::g_gameManager)
		hrc::g_gameManager->CmdKick(args);
}
static void Cmd_Do(const CCommand &args)
{
	if (hrc::g_gameManager)
		hrc::g_gameManager->CmdDo(args);
}
static void Cmd_Info(const CCommand &args)
{
	if (hrc::g_gameManager)
		hrc::g_gameManager->CmdInfo(args);
}
static void Cmd_Fire(const CCommand &args)
{
	if (hrc::g_gameManager)
		hrc::g_gameManager->CmdFire(args);
}
static void Cmd_Move(const CCommand &args)
{
	if (hrc::g_gameManager)
		hrc::g_gameManager->CmdMove(args);
}
static void Cmd_AnalyseGround(const CCommand &args)
{
	if (hrc::g_gameManager)
		hrc::g_gameManager->CmdAnalyseGround(args);
}
static void Cmd_Version(const CCommand &args)
{
	(void)args;
	Msg("%s\n", HRCBOT_BANNER);
}

struct CommandSpec
{
	const char *name;
	FnCommandCallback_t callback;
	const char *help;
};
static const CommandSpec s_commands[] =
{
	{ "hrcbot_add", Cmd_Add,
	  "Add a bot. You can use an optional [team] parameter. It should be 2 for "
	  "combine or 3 for rebels." },
	{ "hrcbot_kick", Cmd_Kick,
	  "Removes a bot. You can use an optional [team] parameter. It should be "
	  "2 for combine or 3 for rebels." },
	{ "hrcbot_do", Cmd_Do,
	  "hrcbot_do \"bot name\" command : the bot will execute the command as "
	  "himself. Quotes are mandatory for composed names." },
	{ "hrcbot_info", Cmd_Info,
	  "Print internal navigation and bot state for debugging." },
	{ "hrcbot_fire", Cmd_Fire, "Debug: make the bots fire." },
	{ "hrcbot_move", Cmd_Move, "Debug: bot movement helper." },
	{ "hrcbot_analyseground", Cmd_AnalyseGround,
	  "Debug: force a ground/navigation re-analysis of the current map." },
	{ "hrcbot_version", Cmd_Version, "Print the Hurricane Bot version." },
};

static ConCommand *s_registeredCommands[
	sizeof(s_commands) / sizeof(s_commands[0])];

// ---------------------------------------------------------------------------
// Load / Unload
// ---------------------------------------------------------------------------

bool CHurricaneBotServerPlugin::Load(PluginId id, ISmmAPI *ismm,
                                     char *error, size_t maxlen, bool late)
{
	PLUGIN_SAVEVARS();

	GET_V_IFACE_CURRENT(GetEngineFactory, hrc::g_engine, IVEngineServer,
	                    INTERFACEVERSION_VENGINESERVER);
	GET_V_IFACE_CURRENT(GetEngineFactory, s_gameEvents, IGameEventManager2,
	                    INTERFACEVERSION_GAMEEVENTSMANAGER2);
	hrc::g_gameEvents = s_gameEvents;
	GET_V_IFACE_CURRENT(GetEngineFactory, s_helpers, IServerPluginHelpers,
	                    INTERFACEVERSION_ISERVERPLUGINHELPERS);
	hrc::g_helpers = s_helpers;
	GET_V_IFACE_CURRENT(GetEngineFactory, hrc::g_cvar, ICvar,
	                    CVAR_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetEngineFactory, hrc::g_trace, IEngineTrace,
	                    INTERFACEVERSION_ENGINETRACE_SERVER);
	GET_V_IFACE_CURRENT(GetEngineFactory, hrc::g_pFileSystem, IFileSystem,
	                    FILESYSTEM_INTERFACE_VERSION);

	GET_V_IFACE_ANY(GetServerFactory, s_server, IServerGameDLL,
	                INTERFACEVERSION_SERVERGAMEDLL);
	hrc::g_gameDll = s_server;
	GET_V_IFACE_ANY(GetServerFactory, s_gameClients, IServerGameClients,
	                INTERFACEVERSION_SERVERGAMECLIENTS);
	hrc::g_gameClients = s_gameClients;
	GET_V_IFACE_ANY(GetServerFactory, hrc::g_playerInfoManager,
	                IPlayerInfoManager, INTERFACEVERSION_PLAYERINFOMANAGER);
	// The bot controller interface is optional: games without native bot
	// support will not expose it, so a NULL result here is non-fatal.
	hrc::g_botManager = (IBotManager *)ismm->GetServerFactory()(
	    INTERFACEVERSION_PLAYERBOTMANAGER, NULL);
	hrc::LogMsg("IBotManager (%s): %s", INTERFACEVERSION_PLAYERBOTMANAGER,
	            hrc::g_botManager ? "available" : "NOT available");
	GET_V_IFACE_ANY(GetServerFactory, hrc::g_gameEnts, IServerGameEnts,
	                INTERFACEVERSION_SERVERGAMEENTS);

	hrc::g_globals = ismm->GetCGlobals();
	g_pCVar = hrc::g_cvar;

	// Enable the VSP listener needed for IServerPluginHelpers.
	if ((s_vspCallbacks = ismm->GetVSPInfo(NULL)) == NULL)
	{
		ismm->AddListener(this, this);
		ismm->EnableVSPListener();
	}

	// Register convars and console commands.
	hrc::RegisterConvars(&s_HrcAccessor);
	for (size_t i = 0; i < sizeof(s_commands) / sizeof(s_commands[0]); ++i)
	{
		ConCommand *cmd = new ConCommand(s_commands[i].name,
		                                s_commands[i].callback,
		                                s_commands[i].help, 0);
		s_HrcAccessor.RegisterConCommandBase(cmd);
		s_registeredCommands[i] = cmd;
	}

	// SourceHook hooks.
	SH_ADD_HOOK_MEMFUNC(IServerGameDLL, LevelInit, s_server, this,
	                    &CHurricaneBotServerPlugin::Hook_LevelInit, true);
	SH_ADD_HOOK_MEMFUNC(IServerGameDLL, ServerActivate, s_server, this,
	                    &CHurricaneBotServerPlugin::Hook_ServerActivate, true);
	SH_ADD_HOOK_MEMFUNC(IServerGameDLL, GameFrame, s_server, this,
	                    &CHurricaneBotServerPlugin::Hook_GameFrame, true);
	SH_ADD_HOOK_MEMFUNC(IServerGameDLL, LevelShutdown, s_server, this,
	                    &CHurricaneBotServerPlugin::Hook_LevelShutdown, false);
	SH_ADD_HOOK_MEMFUNC(IServerGameClients, ClientActive, s_gameClients, this,
	                    &CHurricaneBotServerPlugin::Hook_ClientActive, true);
	SH_ADD_HOOK_MEMFUNC(IServerGameClients, ClientDisconnect, s_gameClients,
	                    this,
	                    &CHurricaneBotServerPlugin::Hook_ClientDisconnect, true);
	SH_ADD_HOOK_MEMFUNC(IServerGameClients, ClientPutInServer, s_gameClients,
	                    this,
	                    &CHurricaneBotServerPlugin::Hook_ClientPutInServer,
	                    true);
	SH_ADD_HOOK_MEMFUNC(IGameEventManager2, FireEvent, s_gameEvents, this,
	                    &CHurricaneBotServerPlugin::Hook_FireEvent, false);

	hrc::g_gameManager = new hrc::GameManager();

	Msg("%s\n", HRCBOT_BANNER);
	Msg("Hurricane Bot Server Plugin loaded (Metamod:Source 1.12)\n");
	(void)id;
	(void)late;
	(void)error;
	(void)maxlen;
	return true;
}

bool CHurricaneBotServerPlugin::Unload(char *error, size_t maxlen)
{
	if (hrc::g_gameManager)
	{
		delete hrc::g_gameManager;
		hrc::g_gameManager = NULL;
	}

	SH_REMOVE_HOOK_MEMFUNC(IServerGameDLL, LevelInit, s_server, this,
	                      &CHurricaneBotServerPlugin::Hook_LevelInit, true);
	SH_REMOVE_HOOK_MEMFUNC(IServerGameDLL, ServerActivate, s_server, this,
	                      &CHurricaneBotServerPlugin::Hook_ServerActivate,
	                      true);
	SH_REMOVE_HOOK_MEMFUNC(IServerGameDLL, GameFrame, s_server, this,
	                      &CHurricaneBotServerPlugin::Hook_GameFrame, true);
	SH_REMOVE_HOOK_MEMFUNC(IServerGameDLL, LevelShutdown, s_server, this,
	                      &CHurricaneBotServerPlugin::Hook_LevelShutdown,
	                      false);
	SH_REMOVE_HOOK_MEMFUNC(IServerGameClients, ClientActive, s_gameClients,
	                      this,
	                      &CHurricaneBotServerPlugin::Hook_ClientActive, true);
	SH_REMOVE_HOOK_MEMFUNC(IServerGameClients, ClientDisconnect,
	                      s_gameClients, this,
	                      &CHurricaneBotServerPlugin::Hook_ClientDisconnect,
	                      true);
	SH_REMOVE_HOOK_MEMFUNC(IServerGameClients, ClientPutInServer,
	                      s_gameClients, this,
	                      &CHurricaneBotServerPlugin::Hook_ClientPutInServer,
	                      true);
	SH_REMOVE_HOOK_MEMFUNC(IGameEventManager2, FireEvent, s_gameEvents, this,
	                      &CHurricaneBotServerPlugin::Hook_FireEvent, false);

	hrc::UnregisterConvars();
	(void)error;
	(void)maxlen;
	return true;
}

bool CHurricaneBotServerPlugin::Pause(char *error, size_t maxlen)
{
	(void)error;
	(void)maxlen;
	return true;
}

bool CHurricaneBotServerPlugin::Unpause(char *error, size_t maxlen)
{
	(void)error;
	(void)maxlen;
	return true;
}

void CHurricaneBotServerPlugin::AllPluginsLoaded()
{
}

void CHurricaneBotServerPlugin::OnVSPListening(IServerPluginCallbacks *iface)
{
	s_vspCallbacks = iface;
}

// ---------------------------------------------------------------------------
// Hook implementations
// ---------------------------------------------------------------------------

bool CHurricaneBotServerPlugin::Hook_LevelInit(const char *pMapName,
    char const *pMapEntities, char const *pOldLevel,
    char const *pLandmarkName, bool loadGame, bool background)
{
	if (hrc::g_gameManager)
		hrc::g_gameManager->OnLevelInit(pMapName);
	RETURN_META_VALUE(MRES_IGNORED, true);
}

void CHurricaneBotServerPlugin::Hook_ServerActivate(edict_t *pEdictList,
    int edictCount, int clientMax)
{
	if (hrc::g_gameManager)
		hrc::g_gameManager->OnServerActivate(pEdictList, edictCount,
		                                     clientMax);
}

void CHurricaneBotServerPlugin::Hook_GameFrame(bool simulating)
{
	if (!hrc::g_gameManager)
		return;
	if (simulating)
		hrc::g_gameManager->OnGameFrame();
	else
		hrc::g_gameManager->OnHibernatingFrame();
}

void CHurricaneBotServerPlugin::Hook_LevelShutdown(void)
{
	if (hrc::g_gameManager)
		hrc::g_gameManager->OnLevelShutdown();
}

void CHurricaneBotServerPlugin::Hook_ClientActive(edict_t *pEntity,
    bool bLoadGame)
{
	if (hrc::g_gameManager)
		hrc::g_gameManager->OnClientActive(pEntity);
}

void CHurricaneBotServerPlugin::Hook_ClientDisconnect(edict_t *pEntity)
{
	if (hrc::g_gameManager)
		hrc::g_gameManager->OnClientDisconnect(pEntity);
}

void CHurricaneBotServerPlugin::Hook_ClientPutInServer(edict_t *pEntity,
    char const *playername)
{
}

// Resolve a player/bot name (and whether it is a fake client) from a network
// user id as carried by game events.  The server engine only exposes edict ->
// user id, so scan the player slots and match IPlayerInfo::GetUserID().  The
// returned name pointer belongs to the engine and is only valid during this
// call.
static const char *PlayerLabelForUserID(int userId, bool *isFakeClient)
{
	if (isFakeClient)
		*isFakeClient = false;
	if (!hrc::g_engine || !hrc::g_playerInfoManager || userId <= 0)
		return NULL;

	int maxClients = hrc::g_globals ? hrc::g_globals->maxClients : 64;
	for (int i = 1; i <= maxClients; ++i)
	{
		edict_t *ed = hrc::g_engine->PEntityOfEntIndex(i);
		if (!ed || ed->IsFree())
			continue;
		IPlayerInfo *pi = hrc::g_playerInfoManager->GetPlayerInfo(ed);
		if (!pi || !pi->IsConnected())
			continue;
		if (pi->GetUserID() != userId)
			continue;
		if (isFakeClient)
			*isFakeClient = pi->IsFakeClient();
		return pi->GetName();
	}
	return NULL;
}

bool CHurricaneBotServerPlugin::Hook_FireEvent(IGameEvent *pEvent,
    bool bDontBroadcast)
{
	// Report bot kills/deaths to the dedicated SERVER CONSOLE (not chat).
	// Only events that involve at least one fake client are logged, so human
	// versus human action stays quiet.  Gated by hrcbot_statusmsgs.
	if (pEvent && hrc::g_cvStatusMsgs && hrc::g_cvStatusMsgs->GetBool())
	{
		const char *eventName = pEvent->GetName();
		if (eventName && strcmp(eventName, "player_death") == 0)
		{
			int victimUid = pEvent->GetInt("userid");
			int attackerUid = pEvent->GetInt("attacker");
			const char *weapon = pEvent->GetString("weapon");
			if (!weapon || !*weapon)
				weapon = "world";

			bool victimBot = false;
			bool attackerBot = false;
			const char *victimName = PlayerLabelForUserID(victimUid,
			                                             &victimBot);
			const char *attackerName = NULL;
			if (attackerUid > 0 && attackerUid != victimUid)
				attackerName = PlayerLabelForUserID(attackerUid,
				                                    &attackerBot);

			if (victimBot || attackerBot)
			{
				if (attackerName)
				{
					hrc::LogMsg("%s%s killed %s%s with %s",
					            attackerName, attackerBot ? " [BOT]" : "",
					            victimName ? victimName : "?",
					            victimBot ? " [BOT]" : "", weapon);
				}
				else
				{
					hrc::LogMsg("%s%s died (%s)",
					            victimName ? victimName : "?",
					            victimBot ? " [BOT]" : "", weapon);
				}
			}
		}
		else if (eventName && strcmp(eventName, "player_spawn") == 0)
		{
			// A fake client gained a pawn and entered the world: this is the
			// authoritative "the bot is actually in the game" signal.
			int uid = pEvent->GetInt("userid");
			bool isBot = false;
			const char *name = PlayerLabelForUserID(uid, &isBot);
			if (isBot)
				hrc::LogMsg("%s [BOT] spawned", name ? name : "?");
		}
		else if (eventName && strcmp(eventName, "player_connect") == 0)
		{
			int uid = pEvent->GetInt("userid");
			bool isBot = false;
			const char *name = PlayerLabelForUserID(uid, &isBot);
			if (isBot)
				hrc::LogMsg("%s [BOT] connected", name ? name : "?");
		}
	}

	(void)bDontBroadcast;
	RETURN_META_VALUE(MRES_IGNORED, true);
}

// ---------------------------------------------------------------------------
// Metadata
// ---------------------------------------------------------------------------

const char *CHurricaneBotServerPlugin::GetAuthor() { return HRCBOT_AUTHOR; }
const char *CHurricaneBotServerPlugin::GetName() { return HRCBOT_NAME; }
const char *CHurricaneBotServerPlugin::GetDescription()
{
	return HRCBOT_DESCRIPTION;
}
const char *CHurricaneBotServerPlugin::GetURL() { return HRCBOT_URL; }
const char *CHurricaneBotServerPlugin::GetLicense()
{
	return HRCBOT_LICENSE;
}
const char *CHurricaneBotServerPlugin::GetVersion()
{
	return HRCBOT_VERSION_STRING;
}
const char *CHurricaneBotServerPlugin::GetDate() { return __DATE__; }
const char *CHurricaneBotServerPlugin::GetLogTag() { return HRCBOT_LOGTAG; }
