// Hibernate.h
// Force a hibernating dedicated server to keep simulating.
//
// An empty Source dedicated server hibernates: the engine stops advancing the
// game, so a fake client created during hibernation never runs its connection
// handshake and is eventually kicked ("Punting bot, server is hibernating").
// This build of HL2DM exposes no hibernation ConVar and IVEngineServer does not
// implement SetServerHibernation, so we resolve the engine's internal
// CGameServer::SetHibernating(bool) at runtime and call it with false while
// bots are expected.  The engine object is the same singleton exposed as
// IVEngineServer, so g_engine is the method's `this` pointer.
#ifndef HRCBOT_CORE_HIBERNATE_H_
#define HRCBOT_CORE_HIBERNATE_H_

namespace hrc
{

// Resolve the engine hibernation setter once.  Returns true on success.
bool HibernateResolve();

// If resolution succeeded, tell the engine the server must not hibernate.
// Safe to call every frame; it is a no-op when the setter is unavailable.
void HibernateWake();

// Whether the hibernation setter was located for this build.
bool HibernateAvailable();

} // namespace hrc

#endif // HRCBOT_CORE_HIBERNATE_H_
