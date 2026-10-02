// Hibernate.h
// Keep a dedicated server simulating while bots are expected.
//
// An empty Source dedicated server hibernates: the engine stops advancing the
// game, so a fake client created during hibernation never finishes its
// connection handshake and is eventually kicked ("Punting bot, server is
// hibernating").  This HL2DM build exposes no hibernation ConVar and its
// IVEngineServer does not implement SetServerHibernation.
//
// Crucially, once a server is fully hibernating the engine no longer calls the
// game DLL's GameFrame, so waking it from a GameFrame hook is impossible.
// Instead we install an inline detour on the engine's internal
// CGameServer::SetHibernating(bool), which the engine keeps calling from its
// own frame loop while it decides whether to hibernate.  While HRCBot wants
// bots on the server the detour forces the argument to false, preventing the
// server from ever entering hibernation; when no bots are wanted the engine
// hibernates normally.
#ifndef HRCBOT_CORE_HIBERNATE_H_
#define HRCBOT_CORE_HIBERNATE_H_

namespace hrc
{

// Resolve the engine routine and install the detour once.  Safe to call every
// frame; returns true when the detour is active.
bool HibernateReady();

// Tell the detour whether it should block hibernation this frame.
void HibernateSetKeepAwake(bool keep);

// Whether the wake detour was installed for this build.
bool HibernateAvailable();

} // namespace hrc

#endif // HRCBOT_CORE_HIBERNATE_H_
