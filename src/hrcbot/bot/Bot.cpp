// Bot.cpp - fake client control and the behaviour state machine.
#include "Bot.h"
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <eiface.h>
#include <game/server/iplayerinfo.h>

#include "core/Button.h"
#include "core/HrcEngine.h"
#include "core/Tools.h"
#include "map/Poseidon.h"
#include "map/Navigation.h"
#include "plugin/hrcbot_cvars.h"

namespace hrc
{

static const float BOT_MAX_SPEED = 450.0f;
static const float BOT_EYE_HEIGHT = 64.0f;
static const float BOT_ATTACK_RANGE = 1800.0f;

// ---------------------------------------------------------------------------
// Visibility
// ---------------------------------------------------------------------------

bool BotCanSee::CanSeePlayer(edict_t *self, const Vector &eyePos,
                             edict_t *target, float maxRange)
{
	if (!target || target->IsFree())
		return false;
	IPlayerInfo *pi = g_playerInfoManager->GetPlayerInfo(target);
	if (!pi || pi->IsDead() || !pi->IsConnected())
		return false;

	Vector targetEye = pi->GetAbsOrigin();
	targetEye.z += BOT_EYE_HEIGHT * 0.85f;
	Vector delta = targetEye - eyePos;
	if (delta.Length() > maxRange)
		return false;
	return HasLineOfSight(eyePos, targetEye, self);
}

// ---------------------------------------------------------------------------
// Bot
// ---------------------------------------------------------------------------

Bot::Bot(edict_t *edict, const char *name, int team, Poseidon *nav)
	: m_edict(edict), m_pi(NULL), m_controller(NULL), m_nav(nav),
	  m_team(team), m_state(BOT_STATE_CONNECTING), m_commandNumber(0),
	  m_nextThink(0.0f), m_stateTimer(0.0f), m_retargetTimer(0.0f),
	  m_respawnTimer(0.0f), m_jumpTimer(0.0f), m_wanderTimer(0.0f),
	  m_wanderYaw(0.0f), m_target(NULL),
	  m_hasMoveGoal(false), m_warnedNoDrive(false), m_wasAlive(false),
	  m_driveWait(0.0f)
{
	snprintf(m_name, sizeof(m_name), "%s", name ? name : "Bot");
	m_aimPoint.Init();
	m_moveGoal.Init();
}

Bot::~Bot() {}

// Re-acquire the per-frame interfaces.  Both are owned by the engine and must
// not be cached across frames (the player entity is recreated on respawn).
bool Bot::RefreshInfo()
{
	if (!m_edict || m_edict->IsFree())
	{
		m_pi = NULL;
		m_controller = NULL;
		return false;
	}
	m_pi = g_playerInfoManager ? g_playerInfoManager->GetPlayerInfo(m_edict)
	                           : NULL;
	m_controller = g_botManager ? g_botManager->GetBotController(m_edict)
	                            : NULL;
	return m_pi != NULL;
}

bool Bot::IsAlive() const
{
	if (!m_pi)
		return false;
	return m_pi->IsConnected() && !m_pi->IsDead() &&
	       m_pi->GetHealth() > 0;
}

void Bot::RunCommand(const char *command)
{
	if (m_edict && command && *command)
		ClientCommandSafe(m_edict, "%s", command);
}

void Bot::RequestRespawn()
{
	ClientCommandSafe(m_edict, "jointeam %i", m_team);
	ClientCommandSafe(m_edict, "joingame");
}

Vector Bot::EyePosition() const
{
	if (!m_pi)
		return Vector(0, 0, 0);
	Vector v = m_pi->GetAbsOrigin();
	v.z += BOT_EYE_HEIGHT;
	return v;
}

void Bot::EmitCommand()
{
	// The user command pump must run every frame from the moment the fake
	// client exists, even before it has a player info / pawn.  The engine
	// processes queued console commands (such as "jointeam") and drives the
	// connection/spawn handshake from RunPlayerMove; without it a bot sits
	// forever at the origin with no team and no pawn.
	if (!m_controller)
		return;
	++m_commandNumber;
	m_cmd.command_number = m_commandNumber;
	m_cmd.tick_count = g_globals ? g_globals->tickcount : m_commandNumber;
	m_cmd.random_seed = m_commandNumber;
	m_controller->RunPlayerMove(&m_cmd);
}

void Bot::AimAt(const Vector &point, float dt)
{
	if (!m_pi)
		return;
	Vector eye = EyePosition();
	Vector d = point - eye;
	float dist = d.Length();
	if (dist < 1.0f)
		return;

	QAngle target;
	target.y = RAD2DEG(atan2(d.y, d.x));
	target.x = -RAD2DEG(asin(d.z / dist));
	target.z = 0.0f;

	QAngle cur = m_pi->GetAbsAngles();
	// Handicap widens the reaction cone and lowers turn speed.
	float handicap = g_cvHandicap ? (float)g_cvHandicap->GetInt() : 0.0f;
	float turnRate = 30.0f - handicap * 0.15f;
	if (turnRate < 6.0f)
		turnRate = 6.0f;
	float step = turnRate * (dt > 0.0f ? dt : 0.016f) * 60.0f * 0.15f;

	cur.y = ApproachAngle(target.y, cur.y, step);
	cur.x = ApproachAngle(target.x, cur.x, step);
	cur.z = 0.0f;
	m_cmd.viewangles = cur;
}

void Bot::MoveToward(const Vector &point, float dt)
{
	if (!m_pi)
		return;
	Vector origin = m_pi->GetAbsOrigin();
	QAngle ang = m_cmd.viewangles;
	Vector forward;
	AngleVectorsHRC(ang, &forward);

	Vector to(point.x - origin.x, point.y - origin.y, 0.0f);
	float dist = to.Length2D();
	if (dist < 16.0f)
	{
		m_cmd.forwardmove = 0.0f;
		m_cmd.sidemove = 0.0f;
		return;
	}
	to /= dist;
	float dotF = forward.x * to.x + forward.y * to.y;
	float dotR = forward.y * to.x - forward.x * to.y;

	m_cmd.forwardmove = BOT_MAX_SPEED * (dotF > 0.0f ? dotF : 0.0f);
	m_cmd.sidemove = BOT_MAX_SPEED * dotR;

	// Hop over small obstacles periodically.
	m_jumpTimer -= dt;
	if (m_jumpTimer <= 0.0f)
	{
		m_cmd.buttons |= IN_JUMP;
		m_jumpTimer = 0.6f;
	}
}

void Bot::ChooseTarget()
{
	m_target = NULL;
	if (!m_pi)
		return;
	float bestDist = BOT_ATTACK_RANGE;
	Vector eye = EyePosition();
	int maxClients = g_globals ? g_globals->maxClients : 0;
	for (int i = 1; i <= maxClients; ++i)
	{
		edict_t *ent = g_engine->PEntityOfEntIndex(i);
		if (!ent || ent == m_edict || ent->IsFree())
			continue;
		IPlayerInfo *pi = g_playerInfoManager->GetPlayerInfo(ent);
		if (!pi)
			continue;
		// Deathmatch is a free-for-all: every other connected player is a
		// legal target, other bots included (so an all-bot server still
		// exercises combat and respawns).  On team-play servers skip
		// team-mates, whether human or bot.
		if (g_teamPlay && m_team >= 2 && pi->GetTeamIndex() == m_team)
			continue;
		if (pi->IsDead() || !pi->IsConnected())
			continue;
		Vector pe = pi->GetAbsOrigin();
		pe.z += BOT_EYE_HEIGHT * 0.85f;
		float d = (pe - eye).Length();
		if (d < bestDist && HasLineOfSight(eye, pe, m_edict))
		{
			bestDist = d;
			m_target = ent;
			m_aimPoint = pe;
		}
	}
}

void Bot::DetectPlayers()
{
	m_retargetTimer -= g_globals ? g_globals->frametime : 0.016f;
	if (m_target)
	{
		IPlayerInfo *pi = g_playerInfoManager->GetPlayerInfo(m_target);
		Vector eye = EyePosition();
		if (!pi || pi->IsDead() ||
		    !BotCanSee::CanSeePlayer(m_edict, eye, m_target,
		                            BOT_ATTACK_RANGE))
		{
			m_target = NULL;
		}
		else
		{
			m_aimPoint = pi->GetAbsOrigin();
			m_aimPoint.z += BOT_EYE_HEIGHT * 0.85f;
		}
	}
	if (!m_target && m_retargetTimer <= 0.0f)
	{
		ChooseTarget();
		m_retargetTimer = 0.25f;
	}
}

void Bot::StateConnecting()
{
	if (!m_pi)
		RefreshInfo();
	if (m_pi && m_pi->IsConnected())
	{
		m_state = BOT_STATE_SPAWNING;
		m_stateTimer = 0.0f;
		ClientCommandSafe(m_edict, "jointeam %i", m_team);
	}
}

void Bot::StateSpawning()
{
	m_stateTimer += g_globals ? g_globals->frametime : 0.0f;
	if (m_stateTimer > 0.5f)
		ClientCommandSafe(m_edict, "joingame");
	if (IsAlive())
	{
		// Pick the configured loadout / starting weapon.
		const char *w = g_cvSpawnWeapon ? g_cvSpawnWeapon->GetString()
		                               : "smg1";
		ClientCommandSafe(m_edict, "use weapon_%s", w);
		m_state = BOT_STATE_IDLE;
		m_stateTimer = 0.0f;
	}
	else if (m_stateTimer > 8.0f)
	{
		// Give up waiting and retry.
		m_stateTimer = 0.0f;
		ClientCommandSafe(m_edict, "jointeam %i", m_team);
	}
}

void Bot::StateDead()
{
	m_respawnTimer -= g_globals ? g_globals->frametime : 0.0f;
	if (IsAlive())
	{
		m_state = BOT_STATE_IDLE;
	}
	else if (m_respawnTimer <= 0.0f)
	{
		RequestRespawn();
		m_respawnTimer = 3.0f;
	}
}

void Bot::PrepareTrip()
{
	// Pick a navigation goal when no enemy is known.  The destination must be
	// a node reachable from the bot's start node on the navigation graph.  If
	// the spawn area is not covered by the graph (no start node within range),
	// fall back to an obstacle-sliding random wander so the bot still moves and
	// can stumble into contact.
	if (!m_pi)
	{
		m_state = BOT_STATE_IDLE;
		return;
	}
	Vector origin = m_pi->GetAbsOrigin();
	Vector goal;
	if (m_nav && m_nav->RandomGoal(origin, 400.0f, goal))
	{
		m_moveGoal = goal;
		m_hasMoveGoal = true;
		m_state = BOT_STATE_GOTO;
	}
	else
	{
		HRandomStream rng((unsigned int)(g_globals->curtime * 100.0f) +
		                  m_commandNumber);
		m_wanderYaw = rng.RandomFloat(-180.0f, 180.0f);
		m_wanderTimer = rng.RandomFloat(3.0f, 6.0f);
		m_hasMoveGoal = false;
		m_state = BOT_STATE_WANDER;
	}
}

void Bot::StateWander()
{
	float dt = g_globals ? g_globals->frametime : 0.016f;
	if (!m_pi)
	{
		m_state = BOT_STATE_IDLE;
		return;
	}
	m_wanderTimer -= dt;
	if (m_wanderTimer <= 0.0f)
	{
		m_cmd.forwardmove = 0.0f;
		m_cmd.sidemove = 0.0f;
		m_state = BOT_STATE_IDLE;
		return;
	}
	Vector origin = m_pi->GetAbsOrigin();
	float rad = m_wanderYaw * 3.14159265f / 180.0f;
	Vector ahead(origin.x + cosf(rad) * 120.0f,
	             origin.y + sinf(rad) * 120.0f, origin.z + 48.0f);
	AimAt(ahead, dt);
	MoveToward(ahead, dt);
}

void Bot::GoTo()
{
	float dt = g_globals ? g_globals->frametime : 0.016f;
	Vector origin = m_pi->GetAbsOrigin();
	Vector wp;
	if (m_nav && m_nav->NextWaypoint(origin, m_moveGoal, wp))
	{
		AimAt(Vector(wp.x, wp.y, origin.z + 48.0f), dt);
		MoveToward(wp, dt);
	}
	else
	{
		m_cmd.forwardmove = 0.0f;
		m_cmd.sidemove = 0.0f;
		m_hasMoveGoal = false;
		m_state = BOT_STATE_IDLE;
	}
}

void Bot::Hunt()
{
	float dt = g_globals ? g_globals->frametime : 0.016f;
	if (m_target)
	{
		IPlayerInfo *pi = g_playerInfoManager->GetPlayerInfo(m_target);
		if (pi)
		{
			AimAt(m_aimPoint, dt);
			// Close the distance but keep some room.
			Vector origin = m_pi->GetAbsOrigin();
			float d = (m_aimPoint - origin).Length();
			if (d > 350.0f)
				MoveToward(m_aimPoint, dt);
			else
			{
				m_cmd.forwardmove = 0.0f;
				m_cmd.sidemove = 0.0f;
			}
			m_state = BOT_STATE_ATTACK;
			return;
		}
	}
	m_state = BOT_STATE_GOTO;
}

void Bot::Attack()
{
	float dt = g_globals ? g_globals->frametime : 0.016f;
	if (!m_target)
	{
		m_cmd.buttons &= ~IN_ATTACK;
		m_state = BOT_STATE_HUNT;
		return;
	}
	IPlayerInfo *pi = g_playerInfoManager->GetPlayerInfo(m_target);
	if (!pi || pi->IsDead())
	{
		m_cmd.buttons &= ~IN_ATTACK;
		m_target = NULL;
		m_state = BOT_STATE_HUNT;
		return;
	}
	AimAt(m_aimPoint, dt);

	// Fire only when roughly facing the target; handicap lowers accuracy.
	QAngle cur = m_cmd.viewangles;
	Vector to = m_aimPoint - EyePosition();
	float distTo = to.Length();
	float yaw = RAD2DEG(atan2(to.y, to.x));
	float pitch = -RAD2DEG(asin(distTo > 1.0f ? to.z / distTo : 0.0f));
	float yawErr = fabs(WrapAngle(yaw - cur.y));
	float pitchErr = fabs(WrapAngle(pitch - cur.x));
	float handicap = g_cvHandicap ? (float)g_cvHandicap->GetInt() : 0.0f;
	float tolerance = 8.0f + handicap * 0.25f;
	if (yawErr < tolerance && pitchErr < tolerance + 12.0f)
		m_cmd.buttons |= IN_ATTACK;
	else
		m_cmd.buttons &= ~IN_ATTACK;

	// Weapon selection.
	if (g_cvAutoWeaponSwitch && g_cvAutoWeaponSwitch->GetBool() &&
	    g_cvCrowbarManiacs && g_cvCrowbarManiacs->GetBool())
	{
		ClientCommandSafe(m_edict, "use weapon_crowbar");
	}
}

void Bot::StateIdle()
{
	m_stateTimer += g_globals ? g_globals->frametime : 0.0f;
	m_cmd.forwardmove = 0.0f;
	m_cmd.sidemove = 0.0f;
	if (m_target)
		m_state = BOT_STATE_HUNT;
	else if (m_stateTimer > 1.5f)
	{
		m_stateTimer = 0.0f;
		m_state = BOT_STATE_PREPARE_TRIP;
	}
}

void Bot::HandleEvents()
{
	if (!IsAlive())
	{
		if (m_state != BOT_STATE_DEAD)
		{
			m_state = BOT_STATE_DEAD;
			m_respawnTimer = 1.5f;
			m_target = NULL;
			m_hasMoveGoal = false;
			if (m_nav)
				m_nav->Invalidate();
		}
	}
}

void Bot::Think()
{
	if (!m_edict || !EngineReady())
		return;
	RefreshInfo();
	float dt = g_globals ? g_globals->frametime : 0.016f;

	// Diagnostics: a connected fake client without a bot controller can never
	// be driven and will never spawn.  The controller is normally created a
	// frame or two after the player info appears, so only warn once it has been
	// missing for a couple of seconds (avoids a harmless startup race warning).
	if (m_pi && m_pi->IsConnected() && !m_controller)
	{
		m_driveWait += dt;
		if (m_driveWait > 2.5f && !m_warnedNoDrive)
		{
			m_warnedNoDrive = true;
			HRC_WARN("bot '%s' has no IBotController; cannot send user commands "
			         "(IBotManager interface missing?)", m_name);
		}
	}
	else
	{
		m_driveWait = 0.0f;
	}

	bool aliveNow = IsAlive();
	if (aliveNow && !m_wasAlive && g_cvStatusMsgs &&
	    g_cvStatusMsgs->GetBool())
	{
		HRC_MSG("bot '%s' is now in the game (pawn ready)", m_name);
	}
	m_wasAlive = aliveNow;

	m_cmd.Reset();

	switch (m_state)
	{
	case BOT_STATE_CONNECTING: StateConnecting(); break;
	case BOT_STATE_SPAWNING:  StateSpawning();  break;
	case BOT_STATE_DEAD:       StateDead();      break;
	default:
		HandleEvents();
		DetectPlayers();
		// A known enemy overrides any navigation or wandering.
		if (m_target && (m_state == BOT_STATE_IDLE ||
		                 m_state == BOT_STATE_PREPARE_TRIP ||
		                 m_state == BOT_STATE_GOTO ||
		                 m_state == BOT_STATE_WANDER))
		{
			m_state = BOT_STATE_HUNT;
		}
		switch (m_state)
		{
		case BOT_STATE_IDLE:         StateIdle();    break;
		case BOT_STATE_PREPARE_TRIP: PrepareTrip();  break;
		case BOT_STATE_GOTO:         GoTo();         break;
		case BOT_STATE_WANDER:       StateWander();  break;
		case BOT_STATE_HUNT:         Hunt();         break;
		case BOT_STATE_ATTACK:       Attack();       break;
		default:                     StateIdle();    break;
		}
		break;
	}

	EmitCommand();
}

} // namespace hrc
