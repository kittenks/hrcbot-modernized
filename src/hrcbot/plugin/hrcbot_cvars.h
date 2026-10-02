// hrcbot_cvars.h
// ConVar / ConCommand surface of Hurricane Bot.
//
// Every name, default value and help string below was recovered from the
// ordered read-only string tables of the original 1.3.4 Win32/Linux binaries
// (see tools/hrcbot_analyze.py and analysis/BLUEPRINT.md).
#ifndef HRCBOT_PLUGIN_CVARS_H_
#define HRCBOT_PLUGIN_CVARS_H_

#include <tier1/convar.h>

namespace hrc
{

// Configuration convars.
extern ConVar *g_cvEnabled;
extern ConVar *g_cvMinPlayers;
extern ConVar *g_cvMaxPlayers;
extern ConVar *g_cvPreferredCount;
extern ConVar *g_cvForceTeam;
extern ConVar *g_cvAutoBalance;
extern ConVar *g_cvWaitForPlayers;
extern ConVar *g_cvFreezeIfNoPlayers;
extern ConVar *g_cvSpawnWeapon;
extern ConVar *g_cvHandicap;
extern ConVar *g_cvMute;
extern ConVar *g_cvSpawnProtectionTime;
extern ConVar *g_cvSpawnProtectionSeconds;
extern ConVar *g_cvSpawnProtectedHealth;
extern ConVar *g_cvCrowbarManiacs;
extern ConVar *g_cvMotd;
extern ConVar *g_cvAutoWeaponSwitch;
extern ConVar *g_cvPlayerModel;
extern ConVar *g_cvDialogMsg;
extern ConVar *g_cvKickCommand;
extern ConVar *g_cvNotifyCriticals;
extern ConVar *g_cvKnownWeapons;
extern ConVar *g_cvNamesFile;
extern ConVar *g_cvClan;
extern ConVar *g_cvLog;
extern ConVar *g_cvStatusMsgs;

// Creates and registers every convar through Metamod (META_REGCVAR).  The
// accessor is supplied by the plugin because it owns g_PLAPI.
void RegisterConvars(IConCommandBaseAccessor *accessor);
void UnregisterConvars();

// Default weapon list, recovered verbatim from the binary.
#define HRC_KNOWN_WEAPONS_DEFAULT \
	"weapon_smg1 weapon_pistol weapon_shotgun weapon_357 weapon_crossbow " \
	"weapon_ar2 weapon_frag weapon_stunstick weapon_crowbar weapon_physcannon"

#define HRC_DEFAULT_NAMES_FILE "addons/hrcbot_server_plugin/hrcbot_names.txt"

} // namespace hrc

#endif // HRCBOT_PLUGIN_CVARS_H_
