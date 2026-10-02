// Tools.cpp
#include "Tools.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <tier0/platform.h>
#include <mathlib/mathlib.h>
#include <filesystem.h>
#include "HrcEngine.h"
#include "plugin/hrcbot_cvars.h"

namespace hrc
{

static void SpewLine(const char *prefix, const char *fmt, va_list args)
{
	char buffer[1024];
	vsnprintf(buffer, sizeof(buffer), fmt, args);
	Msg("[%s] %s%s\n", HRCBOT_LOGTAG, prefix, buffer);
}

void LogMsg(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	SpewLine("", fmt, args);
	va_end(args);
}

void LogWarning(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	SpewLine("WARNING: ", fmt, args);
	va_end(args);
}

void LogCritical(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	SpewLine("CRITICAL: ", fmt, args);
	va_end(args);
}

void LogDebug(const char *fmt, ...)
{
	if (!g_cvLog || !g_cvLog->GetBool())
		return;
	va_list args;
	va_start(args, fmt);
	SpewLine("debug: ", fmt, args);
	va_end(args);
}

// ---------------------------------------------------------------------------
// Deterministic uniform random stream (xorshift32 based, seedable).
// ---------------------------------------------------------------------------

HRandomStream::HRandomStream(unsigned int seed) : m_seed(seed ? seed : 0x12345678u)
{
}

void HRandomStream::SetSeed(unsigned int seed)
{
	m_seed = seed ? seed : 0x12345678u;
}

static inline unsigned int XorShift32(unsigned int &state)
{
	unsigned int x = state;
	x ^= x << 13;
	x ^= x >> 17;
	x ^= x << 5;
	state = x;
	return x;
}

float HRandomStream::RandomFloat(float lo, float hi)
{
	if (hi <= lo)
		return lo;
	unsigned int r = XorShift32(m_seed);
	float unit = (r >> 8) * (1.0f / 16777216.0f); // 24 mantissa bits
	return lo + unit * (hi - lo);
}

int HRandomStream::RandomInt(int lo, int hi)
{
	if (hi <= lo)
		return lo;
	unsigned int range = (unsigned int)(hi - lo + 1);
	return lo + (int)(XorShift32(m_seed) % range);
}

int HRandomStream::RandomCharacter(int lo, int hi)
{
	return RandomInt(lo, hi);
}

bool HRandomStream::RandomChance(float chance)
{
	if (chance <= 0.0f)
		return false;
	if (chance >= 1.0f)
		return true;
	return RandomFloat(0.0f, 1.0f) < chance;
}

// ---------------------------------------------------------------------------
// Angle / vector helpers
// ---------------------------------------------------------------------------

float WrapAngle(float angle)
{
	while (angle > 180.0f)
		angle -= 360.0f;
	while (angle < -180.0f)
		angle += 360.0f;
	return angle;
}

float NormalizeAngle(float angle)
{
	return WrapAngle(angle);
}

void AngleVectorsHRC(const QAngle &angles, Vector *forward, Vector *right,
                    Vector *up)
{
	float sr, sp, sy, cr, cp, cy;
	SinCos(DEG2RAD(angles.y), &sy, &cy);
	SinCos(DEG2RAD(angles.x), &sp, &cp);
	SinCos(DEG2RAD(angles.z), &sr, &cr);

	if (forward)
	{
		forward->x = cp * cy;
		forward->y = cp * sy;
		forward->z = -sp;
	}
	if (right)
	{
		right->x = (-1.0f * sr * sp * cy + -1.0f * cr * -sy);
		right->y = (-1.0f * sr * sp * sy + -1.0f * cr * cy);
		right->z = -1.0f * sr * cp;
	}
	if (up)
	{
		up->x = (cr * sp * cy + -sr * -sy);
		up->y = (cr * sp * sy + -sr * cy);
		up->z = cr * cp;
	}
}

float ApproachAngle(float target, float current, float step)
{
	float delta = WrapAngle(target - current);
	if (delta > step)
		delta = step;
	else if (delta < -step)
		delta = -step;
	return current + delta;
}

bool VectorsAlmostEqual(const Vector &a, const Vector &b, float eps)
{
	return fabs(a.x - b.x) < eps && fabs(a.y - b.y) < eps &&
	       fabs(a.z - b.z) < eps;
}

// ---------------------------------------------------------------------------
// String helpers
// ---------------------------------------------------------------------------

int SplitString(const char *in, const char *separator, char *out[], int maxOut,
                int maxToken)
{
	int count = 0;
	const char *p = in;
	while (*p && count < maxOut)
	{
		while (*p && strchr(separator, *p))
			++p;
		if (!*p)
			break;
		int n = 0;
		out[count] = new char[maxToken];
		char *dst = out[count];
		while (*p && !strchr(separator, *p) && n < maxToken - 1)
			dst[n++] = *p++;
		dst[n] = '\0';
		++count;
	}
	return count;
}

const char *SkipPath(const char *path)
{
	const char *slash = strrchr(path, '/');
	const char *bslash = strrchr(path, '\\');
	const char *last = slash > bslash ? slash : bslash;
	return last ? last + 1 : path;
}

const char *ResolveGamePath(const char *relative, char *buf, size_t bufLen)
{
	if (!relative || !buf || bufLen == 0)
		return NULL;
	// Already absolute (POSIX root, Windows drive, or UNC).
	if (relative[0] == '/' || relative[0] == '\\' ||
	    (relative[0] && relative[1] == ':'))
	{
		strncpy(buf, relative, bufLen - 1);
		buf[bufLen - 1] = '\0';
		return buf;
	}
	// Ask the engine filesystem to resolve against the GAME search path.
	if (g_pFileSystem)
	{
		g_pFileSystem->GetLocalPath(relative, buf, (int)bufLen, "GAME");
		if (buf[0])
			return buf;
	}
	// Fallback: the engine CWD is the engine root and the game directory is
	// a subdirectory.  hl2dm uses "hl2mp".
	snprintf(buf, bufLen, "hl2mp/%s", relative);
	return buf;
}

} // namespace hrc
