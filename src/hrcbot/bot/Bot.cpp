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
	  m_respawnTimer(0.0f), m_jumpTimer(0.0f), m_target(NULL),
	  m_hasMoveGoal(false)
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
	if (!m_pi || !m_controller)
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
		if (!pi || pi->IsFakeClient())
			continue; // bots do not hunt other bots
		if (pi->IsDead() || !pi->IsConnected())
			continue;
		// Respect teams only on team play servers; deathmatch is free-for-all.
		if (g_teamPlay && m_team >= 2 && pi->GetTeamIndex() == m_team)
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
	// Pick a navigation goal when no enemy is known.  The path itself is
	// resolved lazily by GoTo() through Poseidon.
	if (!m_hasMoveGoal && g_globals)
	{
		// Roam toward a point a few hundred units ahead of the spawn area.
		HRandomStream rng((unsigned int)(g_globals->curtime * 100.0f) +
		                  m_commandNumber);
		Vector origin = m_pi->GetAbsOrigin();
		float ang = rng.RandomFloat(0.0f, 6.2831f);
		float rad = rng.RandomFloat(150.0f, 900.0f);
		m_moveGoal = origin + Vector(cos(ang) * rad, sin(ang) * rad, 0.0f);
		m_hasMoveGoal = true;
	}
	m_state = BOT_STATE_GOTO;
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
	float yaw = RAD2DEG(atan2(to.y, to.x));
	float yawErr = fabs(WrapAngle(yaw - cur.y));
	float handicap = g_cvHandicap ? (float)g_cvHandicap->GetInt() : 0.0f;
	float tolerance = 8.0f + handicap * 0.25f;
	if (yawErr < tolerance)
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

	m_cmd.Reset();
	float dt = g_globals ? g_globals->frametime : 0.016f;

	switch (m_state)
	{
	case BOT_STATE_CONNECTING: StateConnecting(); break;
	case BOT_STATE_SPAWNING:  StateSpawning();  break;
	case BOT_STATE_DEAD:       StateDead();      break;
	default:
		HandleEvents();
		DetectPlayers();
		switch (m_state)
		{
		case BOT_STATE_IDLE:         StateIdle();    break;
		case BOT_STATE_PREPARE_TRIP: PrepareTrip();  break;
		case BOT_STATE_GOTO:         GoTo();         break;
		case BOT_STATE_HUNT:         Hunt();         break;
		case BOT_STATE_ATTACK:       Attack();       break;
		default:                     StateIdle();    break;
		}
		break;
	}

	EmitCommand();
}

} // namespace hrc
