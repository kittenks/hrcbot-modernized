// GameManager.cpp - bot population, lifecycle and console commands.
#include "GameManager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <eiface.h>
#include <tier1/convar.h>

#include "HrcEngine.h"
#include "Hibernate.h"
#include "Tools.h"
#include "bot/Bot.h"
#include "plugin/hrcbot_cvars.h"

namespace hrc
{

GameManager *g_gameManager = NULL;

// Player model tables recovered from the hl2mp_models_* data symbols.
static const char *kMaleModels[] =
{
	"models/Humans/Group03/male_01.mdl",
	"models/Humans/Group03/male_02.mdl",
	"models/Humans/Group03/male_03.mdl",
	"models/Humans/Group03/Male_04.mdl",
	"models/Humans/Group03/Male_05.mdl",
	"models/Humans/Group03/male_06.mdl",
	"models/Humans/Group03/male_07.mdl",
	"models/Humans/Group03/male_08.mdl",
	"models/Humans/Group03/male_09.mdl",
};
static const char *kFemaleModels[] =
{
	"models/Humans/Group03/Female_01.mdl",
	"models/Humans/Group03/Female_02.mdl",
	"models/Humans/Group03/Female_03.mdl",
	"models/Humans/Group03/Female_04.mdl",
	"models/Humans/Group03/Female_05.mdl",
	"models/Humans/Group03/Female_06.mdl",
	"models/Humans/Group03/Female_07.mdl",
};
static const char *kCombineModels[] =
{
	"models/Combine_Soldier.mdl",
	"models/Combine_Super_Soldier.mdl",
	"models/Police.mdl",
};

static unsigned int StableHash(const char *s)
{
	unsigned int h = 2166136261u;
	while (*s)
	{
		h ^= (unsigned char)(*s++);
		h *= 16777619u;
	}
	return h % 100000;
}

GameManager::GameManager()
	: m_dedale(NULL), m_poseidon(NULL), m_mapCrc(0), m_maxClients(0),
	  m_teamPlay(false), m_levelLoaded(false), m_analysed(false),
	  m_nextMaintenance(0.0f), m_hibernateTicks(0)
{
	m_mapName[0] = '\0';
	m_dedale = new Dedale(&m_network, &m_raster);
	m_poseidon = new Poseidon(&m_network);
	ReloadNames();
}

GameManager::~GameManager()
{
	KickAllBots();
	delete m_poseidon;
	delete m_dedale;
	for (int i = 0; i < m_names.Count(); ++i)
		delete[] m_names[i];
}

// ---------------------------------------------------------------------------
// Names
// ---------------------------------------------------------------------------

void GameManager::ReloadNames()
{
	for (int i = 0; i < m_names.Count(); ++i)
		delete[] m_names[i];
	m_names.Clear();

	const char *path = g_cvNamesFile ? g_cvNamesFile->GetString()
	                                 : HRC_DEFAULT_NAMES_FILE;
	char full[512];
	if (!ResolveGamePath(path, full, sizeof(full)))
	{
		HRC_WARN("Could not resolve names file %s", path);
		return;
	}
	FILE *fp = fopen(full, "r");
	if (!fp)
	{
		HRC_WARN("Could not open names file %s (resolved: %s)", path, full);
		return;
	}
	char line[128];
	while (fgets(line, sizeof(line), fp))
	{
		// Trim leading/trailing whitespace (the shipped file uses leading tabs).
		char *p = line;
		while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
			++p;
		size_t len = strlen(p);
		while (len && (p[len - 1] == ' ' || p[len - 1] == '\t' ||
		               p[len - 1] == '\r' || p[len - 1] == '\n'))
			p[--len] = '\0';
		// The names list ends at the first blank line; everything after it
		// is bilingual documentation and must be ignored.
		if (!*p)
			break;
		char *copy = new char[len + 1];
		strcpy(copy, p);
		m_names.Add(copy);
	}
	fclose(fp);
	HRC_MSG("Loaded %i bot names", m_names.Count());
}

const char *GameManager::PickName()
{
	if (m_names.IsEmpty())
		return "Bot";
	static unsigned int seed = 0;
	if (!seed)
		seed = (unsigned int)time(NULL);

	// Draw a name that no live bot is using, so the engine does not rename a
	// duplicate fake client to "(1)Name".  Fall back to the raw draw once the
	// pool is exhausted.
	int total = m_names.Count();
	for (int attempt = 0; attempt < total; ++attempt)
	{
		seed = seed * 1103515245u + 12345u;
		int idx = (seed / 65536) % total;
		const char *candidate = m_names[idx];
		bool inUse = false;
		for (int i = 0; i < m_bots.Count(); ++i)
		{
			Bot *b = m_bots.Get(i);
			if (b && strcmp(b->GetName(), candidate) == 0)
			{
				inUse = true;
				break;
			}
		}
		if (!inUse)
			return candidate;
	}
	seed = seed * 1103515245u + 12345u;
	return m_names[(seed / 65536) % total];
}

int GameManager::PickTeam(int requested) const
{
	if (requested == 2 || requested == 3)
		return requested;
	if (requested == 1)
		return 1; // spectator slot as requested
	// hl2dm spawns players on combine (2) or rebels (3) even in free-for-all
	// deathmatch; team 0 is unassigned and never receives a player spawn, so
	// balance the bots across the two real teams in every game mode.
	return (BotCount() % 2 == 0) ? 3 : 2;
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void GameManager::OnServerActivate(edict_t *edictList, int edictCount,
                                   int maxClients)
{
	m_maxClients = maxClients;
	HRC_MSG("ServerActivate(edictCount=%i, max_clients=%i)", edictCount,
	        maxClients);
}

void GameManager::OnLevelInit(const char *mapName)
{
	KickAllBots();
	m_network.Clear();
	if (m_poseidon)
		m_poseidon->Invalidate();
	snprintf(m_mapName, sizeof(m_mapName), "%s", mapName ? mapName : "");
	m_mapCrc = (int)StableHash(m_mapName);
	m_levelLoaded = true;
	m_analysed = false;
	m_hibernateTicks = 0;

	// Detect team play mode through the game's own convar.
	m_teamPlay = false;
	if (g_cvar)
	{
		ConVar *mp = g_cvar->FindVar("mp_teamplay");
		if (mp)
			m_teamPlay = mp->GetBool();
	}
	g_teamPlay = m_teamPlay;
	HRC_MSG("game::teamplay: %i", m_teamPlay ? 1 : 0);

	AnalyseOrLoad();
}

void GameManager::AnalyseOrLoad()
{
	if (g_cvEnabled && !g_cvEnabled->GetBool())
	{
		HRC_MSG("The bots were disabled at map load time.");
		return;
	}
	if (m_dedale->LoadMap(m_mapName, m_mapCrc))
	{
		m_analysed = true;
		return;
	}
	Vector seed(0, 0, 0);
	if (m_dedale->AnalyseMap(seed))
	{
		m_dedale->SaveMap(m_mapName, m_mapCrc);
		m_analysed = true;
	}
}

void GameManager::OnLevelShutdown()
{
	KickAllBots();
	m_network.Clear();
	if (m_poseidon)
		m_poseidon->Invalidate();
	m_levelLoaded = false;
	m_analysed = false;
}

void GameManager::OnClientActive(edict_t *who)
{
	if (!who)
		return;
	for (int i = 0; i < m_bots.Count(); ++i)
	{
		Bot *b = m_bots.Get(i);
		if (b && b->GetEdict() == who)
		{
			// Fully connected and active: this is the authoritative "in the
			// game" signal (also confirms the hibernation workaround worked).
			if (g_cvStatusMsgs && g_cvStatusMsgs->GetBool())
			{
				HRC_MSG("bot '%s' joined the game (team %i)",
				        b->GetName(), b->GetTeam());
			}
			return;
		}
	}
}

void GameManager::OnClientDisconnect(edict_t *who)
{
	if (!who)
		return;
	for (int i = 0; i < m_bots.Count(); ++i)
	{
		Bot *b = m_bots.Get(i);
		if (b && b->GetEdict() == who)
		{
			if (g_cvStatusMsgs && g_cvStatusMsgs->GetBool())
				HRC_MSG("bot '%s' left the game", b->GetName());
			m_bots.Remove(b, true);
			return;
		}
	}
}

// ---------------------------------------------------------------------------
// Player / bot counting
// ---------------------------------------------------------------------------

int GameManager::HumanPlayerCount() const
{
	int humans = 0;
	int maxClients = g_globals ? g_globals->maxClients : m_maxClients;
	for (int i = 1; i <= maxClients; ++i)
	{
		edict_t *ent = g_engine->PEntityOfEntIndex(i);
		if (!ent || ent->IsFree())
			continue;
		// Never count our own bots as humans.  The IsFakeClient() flag can
		// lag behind CreateFakeClient during the connection handshake, which
		// otherwise makes population logic oscillate (add then kick).
		bool isOurs = false;
		for (int j = 0; j < m_bots.Count(); ++j)
		{
			Bot *b = m_bots.Get(j);
			if (b && b->GetEdict() == ent)
			{
				isOurs = true;
				break;
			}
		}
		if (isOurs)
			continue;
		IPlayerInfo *pi = g_playerInfoManager->GetPlayerInfo(ent);
		if (pi && pi->IsConnected() && !pi->IsFakeClient())
			++humans;
	}
	return humans;
}

// ---------------------------------------------------------------------------
// Bot creation / removal
// ---------------------------------------------------------------------------

Bot *GameManager::AddBot(int team, const char *forcedName)
{
	if (g_cvEnabled && !g_cvEnabled->GetBool())
	{
		HRC_MSG("The bots were disabled at map load time. Unable to add a bot.");
		return NULL;
	}
	const char *picked = forcedName ? forcedName : PickName();
	char netname[64];
	snprintf(netname, sizeof(netname), "%s", picked);

	edict_t *edict = g_engine->CreateFakeClient(netname);
	if (!edict)
	{
		HRC_WARN("CreateFakeClient failed (server full?)");
		return NULL;
	}
	int chosen = PickTeam(team >= 0 ? team
	                          : (g_cvForceTeam ? g_cvForceTeam->GetInt() : -1));
	Bot *bot = new Bot(edict, netname, chosen, m_poseidon);
	m_bots.Add(bot);
	if (g_cvStatusMsgs && g_cvStatusMsgs->GetBool())
	{
		HRC_MSG("bot '%s' added to team %i (%i bots)", netname,
		        chosen, BotCount());
	}
	return bot;
}

bool GameManager::KickBot(int team)
{
	for (int i = m_bots.Count() - 1; i >= 0; --i)
	{
		Bot *b = m_bots.Get(i);
		if (!b)
			continue;
		if (team >= 0 && b->GetTeam() != team)
			continue;
		edict_t *e = b->GetEdict();
		if (e && g_engine)
		{
			// Prefer the configurable kick command against the user id so the
			// removal also works on heavily modded servers.
			int userId = -1;
			IPlayerInfo *pi = g_playerInfoManager
			                     ? g_playerInfoManager->GetPlayerInfo(e)
			                     : NULL;
			if (pi)
				userId = pi->GetUserID();
			if (userId >= 0)
			{
				char cmd[128];
				const char *kickCmd = g_cvKickCommand
				                          ? g_cvKickCommand->GetString()
				                          : "kickid";
				snprintf(cmd, sizeof(cmd), "%s %d\n", kickCmd, userId);
				g_engine->ServerCommand(cmd);
			}
			if (g_cvStatusMsgs && g_cvStatusMsgs->GetBool())
				HRC_MSG("kicking bot '%s'", b->GetName());
		}
		m_bots.Remove(b, true);
		return true;
	}
	return false;
}

void GameManager::KickAllBots()
{
	for (int i = m_bots.Count() - 1; i >= 0; --i)
	{
		Bot *b = m_bots.Get(i);
		if (b && b->GetEdict() && g_engine)
			g_engine->ClientCommand(b->GetEdict(), "kill");
		m_bots.Remove(b, true);
	}
}

// ---------------------------------------------------------------------------
// Per-frame population maintenance
// ---------------------------------------------------------------------------

void GameManager::MaintainPopulation()
{
	if (!m_levelLoaded || !g_cvEnabled || !g_cvEnabled->GetBool())
		return;

	int humans = HumanPlayerCount();

	if (g_cvWaitForPlayers && g_cvWaitForPlayers->GetBool() && humans == 0)
		return;

	if (g_cvFreezeIfNoPlayers && g_cvFreezeIfNoPlayers->GetBool() &&
	    humans == 0)
	{
		if (BotCount() > 0)
			KickAllBots();
		return;
	}

	// Manual mode: hrcbot_add / hrcbot_kick only.
	if (g_cvAutoBalance && !g_cvAutoBalance->GetBool())
		return;

	int minPlayers = g_cvMinPlayers ? g_cvMinPlayers->GetInt() : 0;
	int maxPlayers = g_cvMaxPlayers ? g_cvMaxPlayers->GetInt() : 5;
	int preferred = g_cvPreferredCount ? g_cvPreferredCount->GetInt() : 0;

	int desiredTotal;
	if (preferred > 0)
		desiredTotal = preferred;
	else
		desiredTotal = maxPlayers;
	if (desiredTotal < minPlayers)
		desiredTotal = minPlayers;
	if (desiredTotal > maxPlayers)
		desiredTotal = maxPlayers;

	int desiredBots = desiredTotal - humans;
	// Never fill the last slot so a human can always connect.
	int capacity = m_maxClients ? m_maxClients - 1 : desiredBots;
	if (desiredBots > capacity - humans)
		desiredBots = capacity - humans;
	if (desiredBots < 0)
		desiredBots = 0;

	while (BotCount() < desiredBots)
	{
		if (!AddBot())
			break;
	}
	while (BotCount() > desiredBots)
	{
		if (!KickBot())
			break;
	}
}

void GameManager::UpdateHibernateControl()
{
	// Install the engine wake detour lazily on the first frame (the engine is
	// awake and running on its own thread here, well before the empty-server
	// hibernation delay elapses).
	if (!HibernateReady())
		return;

	bool want = false;
	if (m_levelLoaded && m_analysed && g_cvEnabled && g_cvEnabled->GetBool())
		want = (BotCount() > 0 || BotsWanted());
	HibernateSetKeepAwake(want);
}

void GameManager::OnGameFrame()
{
	UpdateHibernateControl();
	float now = g_globals ? g_globals->curtime : 0.0f;
	for (int i = 0; i < m_bots.Count(); ++i)
	{
		Bot *b = m_bots.Get(i);
		if (b)
			b->Think();
	}
	if (now >= m_nextMaintenance)
	{
		MaintainPopulation();
		m_nextMaintenance = now + 1.0f;
	}
}

void GameManager::OnHibernatingFrame()
{
	// Safety net for the case where the detour was not installed (unsupported
	// build): if the engine ever reports a hibernation frame while bots are
	// expected, keep the control flag current and still try to maintain the
	// population.  With the detour active this path is normally never reached
	// because the server is prevented from hibernating at all.
	UpdateHibernateControl();
	if (!m_levelLoaded || !m_analysed)
		return;

	// Throttle the actual population work with a frame counter because the
	// game clock does not advance while hibernating.  Checking every 20
	// hibernation ticks is roughly once per second.
	if (++m_hibernateTicks < 20)
		return;
	m_hibernateTicks = 0;
	MaintainPopulation();
}

bool GameManager::BotsWanted() const
{
	if (!g_cvEnabled || !g_cvEnabled->GetBool())
		return false;
	// Manual mode only adds bots through hrcbot_add; do not wake for that.
	if (g_cvAutoBalance && !g_cvAutoBalance->GetBool())
		return false;
	// Modes that deliberately keep an empty server frozen/waiting.
	if (g_cvWaitForPlayers && g_cvWaitForPlayers->GetBool() &&
	    HumanPlayerCount() == 0)
		return false;
	if (g_cvFreezeIfNoPlayers && g_cvFreezeIfNoPlayers->GetBool() &&
	    HumanPlayerCount() == 0)
		return false;
	int minPlayers = g_cvMinPlayers ? g_cvMinPlayers->GetInt() : 0;
	int maxPlayers = g_cvMaxPlayers ? g_cvMaxPlayers->GetInt() : 5;
	int preferred = g_cvPreferredCount ? g_cvPreferredCount->GetInt() : 0;
	int desiredTotal = preferred > 0 ? preferred : maxPlayers;
	if (desiredTotal < minPlayers)
		desiredTotal = minPlayers;
	return desiredTotal > HumanPlayerCount();
}

// ---------------------------------------------------------------------------
// Console commands
// ---------------------------------------------------------------------------

void GameManager::CmdAdd(const CCommand &args)
{
	if (g_cvAutoBalance && g_cvAutoBalance->GetBool())
	{
		HRC_MSG("hrcbot_add can only work with 'hrcbot_autobalancebots 0'");
		return;
	}
	int team = -1;
	if (args.ArgC() >= 2)
		team = atoi(args.Arg(1));
	AddBot(team);
}

void GameManager::CmdKick(const CCommand &args)
{
	if (g_cvAutoBalance && g_cvAutoBalance->GetBool())
	{
		HRC_MSG("hrcbot_kick can only work with 'hrcbot_autobalancebots 0'");
		return;
	}
	int team = -1;
	if (args.ArgC() >= 2)
		team = atoi(args.Arg(1));
	if (!KickBot(team))
		HRC_MSG("No matching bot found");
}

void GameManager::CmdDo(const CCommand &args)
{
	if (args.ArgC() < 3)
	{
		HRC_MSG("hrcbot_do \"bot name\" command : the bot will execute the "
		        "command as himself. Quotes are mandatory for composed names.");
		return;
	}
	const char *wanted = args.Arg(1);
	// Join everything after the name so commands with arguments ("jointeam 2")
	// survive as a single bot command line.
	char command[256];
	command[0] = '\0';
	for (int a = 2; a < args.ArgC(); ++a)
	{
		if (a > 2)
			snprintf(command + strlen(command),
			         sizeof(command) - strlen(command), " ");
		snprintf(command + strlen(command),
		         sizeof(command) - strlen(command), "%s", args.Arg(a));
	}
	if (command[0] == '\0')
	{
		HRC_MSG("Missing bot command.");
		return;
	}
	for (int i = 0; i < m_bots.Count(); ++i)
	{
		Bot *b = m_bots.Get(i);
		if (b && strcmp(b->GetName(), wanted) == 0)
		{
			b->RunCommand(command);
			HRC_MSG("'%s' will do '%s'", wanted, command);
			return;
		}
	}
	HRC_MSG("Unknown bot");
}

void GameManager::CmdInfo(const CCommand &args)
{
	(void)args;
	HRC_MSG("Hurricane Bot ver %s ; map=%s crc=%i ; bots=%i humans=%i",
	        HRCBOT_LEGACY_VERSION, m_mapName, m_mapCrc, BotCount(),
	        HumanPlayerCount());
	HRC_MSG("Nodes=%i arcs=%i rooms=%i gateways=%i", m_network.NodeCount(),
	        m_network.ArcCount(), m_network.RoomCount(),
	        m_network.GatewayCount());
	static const char *stateNames[] = {
		"connecting", "spawning", "idle", "preparing", "goto",
		"hunting", "attacking", "dead"};
	for (int i = 0; i < m_bots.Count(); ++i)
	{
		Bot *b = m_bots.Get(i);
		if (!b)
			continue;
		IPlayerInfo *pi = b->PlayerInfo();
		int st = (int)b->State();
		const char *stName = (st >= 0 && st < 8) ? stateNames[st] : "?";
		if (pi && pi->IsConnected())
		{
			Vector o = pi->GetAbsOrigin();
			QAngle a = pi->GetAbsAngles();
			HRC_MSG("  %-12s team=%i hp=%-3i state=%-9s pos=(%.0f,%.0f,%.0f) "
			        "ang=(%.0f,%.0f)",
			        b->GetName(), pi->GetTeamIndex(), pi->GetHealth(),
			        stName, o.x, o.y, o.z, a.x, a.y);
		}
		else
		{
			HRC_MSG("  %-12s team=%i state=%-9s (not connected)",
			        b->GetName(), b->GetTeam(), stName);
		}
	}
	HRC_MSG("These information are meant for debugging and are subject to "
	        "change.");
}

void GameManager::CmdFire(const CCommand &args)
{
	(void)args;
	// Debug helper: force every bot to fire once.
	for (int i = 0; i < m_bots.Count(); ++i)
	{
		Bot *b = m_bots.Get(i);
		if (b)
			b->RunCommand("+attack");
	}
}

void GameManager::CmdMove(const CCommand &args)
{
	(void)args;
	HRC_MSG("hrcbot_move is a debug command and is handled per-frame in the "
	        "modern build.");
}

void GameManager::CmdAnalyseGround(const CCommand &args)
{
	(void)args;
	if (m_levelLoaded)
	{
		Vector seed(0, 0, 0);
		if (m_dedale->AnalyseMap(seed))
			m_dedale->SaveMap(m_mapName, m_mapCrc);
	}
}

} // namespace hrc
