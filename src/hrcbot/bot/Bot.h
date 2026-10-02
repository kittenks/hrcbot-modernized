// Bot.h
// Server side fake-client bot.  The original tree split behaviour across
// Bot.cpp, Bot_think.cpp, Bot_attack.cpp, Bot_hunt.cpp, Bot_idle.cpp,
// Bot_goTo.cpp, Bot_doTrip.cpp, Bot_prepareTrip.cpp, Bot_dead.cpp,
// Bot_detectPlayers.cpp and Bot_events.cpp.  The modern tree keeps the state
// machine and method names but consolidates them here.
#ifndef HRCBOT_BOT_BOT_H_
#define HRCBOT_BOT_BOT_H_

#include <eiface.h>
#include <mathlib/vector.h>
#include <game/server/iplayerinfo.h>

namespace hrc
{

class Poseidon;

enum BotState
{
	BOT_STATE_CONNECTING = 0,
	BOT_STATE_SPAWNING,
	BOT_STATE_IDLE,
	BOT_STATE_PREPARE_TRIP,
	BOT_STATE_GOTO,
	BOT_STATE_HUNT,
	BOT_STATE_ATTACK,
	BOT_STATE_DEAD,
};

// Recovered interface names: IBot / IBotBaseInterface.
class IBot
{
public:
	virtual ~IBot() {}
	virtual void Think() = 0;
	virtual edict_t *GetEdict() const = 0;
	virtual bool IsAlive() const = 0;
	virtual const char *GetName() const = 0;
	virtual int GetTeam() const = 0;
};

// Small visibility helper recovered from the BotCanSee class.
class BotCanSee
{
public:
	static bool CanSeePlayer(edict_t *self, const Vector &eyePos,
	                         edict_t *target, float maxRange);
};

class Bot : public IBot
{
public:
	Bot(edict_t *edict, const char *name, int team, Poseidon *nav);
	virtual ~Bot();

	virtual void Think();
	virtual edict_t *GetEdict() const { return m_edict; }
	virtual bool IsAlive() const;
	virtual const char *GetName() const { return m_name; }
	virtual int GetTeam() const { return m_team; }

	IPlayerInfo *PlayerInfo() const { return m_pi; }
	BotState State() const { return m_state; }

	// Run a console command as the bot (hrcbot_do).
	void RunCommand(const char *command);

	// Force the bot to respawn / change team.
	void RequestRespawn();

private:
	// Behaviour steps (named after the original translation units).
	void StateConnecting();
	void StateSpawning();
	void StateDead();
	void StateIdle();
	void PrepareTrip();
	void GoTo();
	void Hunt();
	void Attack();
	void DetectPlayers();
	void HandleEvents();

	void ChooseTarget();
	void AimAt(const Vector &point, float dt);
	void MoveToward(const Vector &point, float dt);
	void EmitCommand();
	Vector EyePosition() const;
	bool RefreshInfo();

	edict_t *m_edict;
	IPlayerInfo *m_pi;
	class IBotController *m_controller;
	Poseidon *m_nav;
	char m_name[64];
	int m_team;
	BotState m_state;

	CBotCmd m_cmd;
	int m_commandNumber;
	float m_nextThink;
	float m_stateTimer;
	float m_retargetTimer;
	float m_respawnTimer;
	float m_jumpTimer;
	edict_t *m_target;
	Vector m_moveGoal;
	Vector m_aimPoint;
	bool m_hasMoveGoal;
};

} // namespace hrc

#endif // HRCBOT_BOT_BOT_H_
