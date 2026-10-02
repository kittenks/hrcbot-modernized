// Tools.h
// Small shared helpers: logging, random numbers, math and string utilities.
// The original binary exported a HurricaneUniformRandomStream class and a
// Tools.cpp translation unit; both are represented here.
#ifndef HRCBOT_CORE_TOOLS_H_
#define HRCBOT_CORE_TOOLS_H_

#include <math.h>
#include <stddef.h>
#include <stdarg.h>
#include <tier0/dbg.h>
#include <mathlib/vector.h>

#include "hrcbot_version.h"

namespace hrc
{

// Logging helpers.  These mirror the "Hurricane Bot ..." console lines used by
// the original plugin.
void LogMsg(const char *fmt, ...);
void LogWarning(const char *fmt, ...);
void LogDebug(const char *fmt, ...);
void LogCritical(const char *fmt, ...);

// Uniform random stream (replaces HurricaneUniformRandomStream).
class HRandomStream
{
public:
	HRandomStream(unsigned int seed = 0);
	void SetSeed(unsigned int seed);
	float RandomFloat(float lo, float hi);
	int RandomInt(int lo, int hi);       // inclusive range
	int RandomCharacter(int lo, int hi); // inclusive, no caching
	bool RandomChance(float chance);     // 0..1

private:
	unsigned int m_seed;
};

// Math helpers.
float WrapAngle(float angle);
float NormalizeAngle(float angle);
void AngleVectorsHRC(const QAngle &angles, Vector *forward,
                     Vector *right = NULL, Vector *up = NULL);
float ApproachAngle(float target, float current, float step);
bool VectorsAlmostEqual(const Vector &a, const Vector &b, float eps = 1.0f);

// String helpers.
int SplitString(const char *in, const char *separator, char *out[], int maxOut,
                int maxToken);
const char *SkipPath(const char *path);

// Resolve a game-relative path (e.g. "addons/...") to a local filesystem path
// suitable for fopen.  The engine CWD is the engine root; game files live
// under the game directory.  Returns buf, or NULL on invalid input.
const char *ResolveGamePath(const char *relative, char *buf, size_t bufLen);

} // namespace hrc

// Convenience macros used across the source tree.
#define HRC_MSG(...)   ::hrc::LogMsg(__VA_ARGS__)
#define HRC_WARN(...)  ::hrc::LogWarning(__VA_ARGS__)
#define HRC_DEBUG(...) ::hrc::LogDebug(__VA_ARGS__)
#define HRC_CRIT(...)  ::hrc::LogCritical(__VA_ARGS__)

#endif // HRCBOT_CORE_TOOLS_H_
