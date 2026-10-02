// hrcbot_cvars.cpp - convar registration, values recovered from 1.3.4.
#include "hrcbot_cvars.h"

namespace hrc
{

ConVar *g_cvEnabled = NULL;
ConVar *g_cvMinPlayers = NULL;
ConVar *g_cvMaxPlayers = NULL;
ConVar *g_cvPreferredCount = NULL;
ConVar *g_cvForceTeam = NULL;
ConVar *g_cvAutoBalance = NULL;
ConVar *g_cvWaitForPlayers = NULL;
ConVar *g_cvFreezeIfNoPlayers = NULL;
ConVar *g_cvSpawnWeapon = NULL;
ConVar *g_cvHandicap = NULL;
ConVar *g_cvMute = NULL;
ConVar *g_cvSpawnProtectionTime = NULL;
ConVar *g_cvSpawnProtectionSeconds = NULL;
ConVar *g_cvSpawnProtectedHealth = NULL;
ConVar *g_cvCrowbarManiacs = NULL;
ConVar *g_cvMotd = NULL;
ConVar *g_cvAutoWeaponSwitch = NULL;
ConVar *g_cvPlayerModel = NULL;
ConVar *g_cvDialogMsg = NULL;
ConVar *g_cvKickCommand = NULL;
ConVar *g_cvNotifyCriticals = NULL;
ConVar *g_cvKnownWeapons = NULL;
ConVar *g_cvNamesFile = NULL;
ConVar *g_cvClan = NULL;
ConVar *g_cvLog = NULL;
ConVar *g_cvStatusMsgs = NULL;

struct CvarSpec
{
	const char *name;
	const char *def;
	int flags;
	const char *help;
	ConVar **slot;
};

#define A 0 // no special flags for most cvars

static const CvarSpec s_specs[] =
{
	{ "hrcbot_enabled", "1", FCVAR_NOTIFY,
	  "If set to 0, the bots will cease all activity and the map will not be "
	  "analysed at load time. If you re-enable the bot in a level with an "
	  "unanalysed map, you will have to change the level again to get it "
	  "analyzed.", &g_cvEnabled },
	{ "hrcbot_minplayers", "0", A,
	  "Sets the minimum number of players", &g_cvMinPlayers },
	{ "hrcbot_maxplayers", "5", A,
	  "Bots will balance down to this number of players", &g_cvMaxPlayers },
	{ "hrcbot_preferredcount", "0", A,
	  "As long as the minplayers and maxplayers conditions are respected, the "
	  "plugin will try to get at least this number of bots in game.",
	  &g_cvPreferredCount },
	{ "hrcbot_forceteam", "-1", A,
	  "Force the bot into a preset team. -1=balanced teams; 0=deathmatch*; "
	  "1=spectator*; 2=combine; 3=rebels. (*does not make sense in HL2DM)",
	  &g_cvForceTeam },
	{ "hrcbot_autobalancebots", "1", A,
	  "Set this to 0 to disable min,max,preferred and forceteam features. "
	  "hrcbot_add can only work in this mode.", &g_cvAutoBalance },
	{ "hrcbot_waitforplayers", "0", A,
	  "Start filling the game with bots only if there are already players in "
	  "it", &g_cvWaitForPlayers },
	{ "hrcbot_freezeifnoplayers", "0", A,
	  "When the last human player leaves the game, the bots are disabled.",
	  &g_cvFreezeIfNoPlayers },
	{ "hrcbot_player_spawnweapon", "smg1", A,
	  "ie: smg1 pistol 357 crossbow shotgun ar2 frag rpg", &g_cvSpawnWeapon },
	{ "hrcbot_handicap", "65", A,
	  "Set this to 0 for a very strong bot. Set a higher integer for your "
	  "little sister (100).", &g_cvHandicap },
	{ "hrcbot_mute", "0", A,
	  "Set this to anything else than 0 to disable the bot chatter (default "
	  "is 0).", &g_cvMute },
	{ "hrcbot_spawnprotectiontime", "1800", A,
	  "When a player spawns, the bot will leave him alone until this time has "
	  "elapsed (1/60 seconds units).", &g_cvSpawnProtectionTime },
	{ "hrcbot_spawnprotectionseconds", "2", A,
	  "When a player spawns, the bot will leave him alone until this time has "
	  "elapsed (seconds units). Modifies hrcbot_spawnprotectiontime.",
	  &g_cvSpawnProtectionSeconds },
	{ "hrcbot_spawnprotectedhealth", "125", A,
	  "The bot will assume that a player with at least this amount of health "
	  "is spawn protected.", &g_cvSpawnProtectedHealth },
	{ "hrcbot_crowbarmaniacs", "0", A,
	  "The bot will only use the crowbar", &g_cvCrowbarManiacs },
	{ "hrcbot_motd", "0", A,
	  "Set this to 0 to disable the plugin notification", &g_cvMotd },
	{ "hrcbot_autoweaponswitch", "1", A,
	  "Set this to 0 to prevent the bot from changing weapons",
	  &g_cvAutoWeaponSwitch },
	{ "hrcbot_playermodel", "*", A,
	  "All the bots will use the specified model. Use '*' for the default "
	  "random choice.", &g_cvPlayerModel },
	{ "hrcbot_dialogmsg", "1", A,
	  "If enabled, the bot version will be displayed on each user's screen as "
	  "they connect", &g_cvDialogMsg },
	{ "hrcbot_kickcommand", "kickid", A,
	  "On some modded servers, the kick command does not work. You can define "
	  "what command will kick a player with this cvar.", &g_cvKickCommand },
	{ "hrcbot_notifycriticals", "0", A,
	  "If set to 1, the plugin will log critical sections of the status of "
	  "the bots", &g_cvNotifyCriticals },
	{ "hrcbot_knownweapons", HRC_KNOWN_WEAPONS_DEFAULT, A,
	  "List of the weapons known to the bots", &g_cvKnownWeapons },
	{ "hrcbot_namesfile", HRC_DEFAULT_NAMES_FILE, A,
	  "Full path to the file containing the bot names.", &g_cvNamesFile },
	{ "hrcbot_clan", "", A,
	  "Optional clan tag applied to bot names.", &g_cvClan },
	{ "hrcbot_log", "0", FCVAR_DEVELOPMENTONLY,
	  "Logs information about the internals of the bot on the console",
	  &g_cvLog },
	{ "hrcbot_statusmsgs", "1", A,
	  "Print bot join, kick, kill and death status lines to the server "
	  "console (1=on, 0=off)", &g_cvStatusMsgs },
};

void RegisterConvars(IConCommandBaseAccessor *accessor)
{
	for (size_t i = 0; i < sizeof(s_specs) / sizeof(s_specs[0]); ++i)
	{
		const CvarSpec *s = &s_specs[i];
		ConVar *cv = new ConVar(s->name, s->def, s->flags, s->help);
		accessor->RegisterConCommandBase(cv);
		*(s->slot) = cv;
	}
}

void UnregisterConvars()
{
	CvarSpec *first = NULL;
	(void)first;
	// ConVar instances are engine-managed once registered; on unload we only
	// drop our cached pointers.  The engine removes them with the plugin.
	for (size_t i = 0; i < sizeof(s_specs) / sizeof(s_specs[0]); ++i)
	{
		if (s_specs[i].slot)
			*(s_specs[i].slot) = NULL;
	}
}

} // namespace hrc
