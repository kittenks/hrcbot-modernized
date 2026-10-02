// Stream.cpp - versioned binary container reader/writer.
#include "Stream.h"
#include <string.h>
#include <stdlib.h>
#include "Tools.h"

namespace hrc
{

static const char kMagic[8] = { 'H', 'R', 'C', 'B', 'O', 'T', '2', '\0' };

Stream::Stream() : m_fp(NULL), m_mode(0), m_good(false) {}

Stream::~Stream()
{
	Close();
}

bool Stream::OpenForWrite(const char *path)
{
	Close();
	char full[512];
	if (!ResolveGamePath(path, full, sizeof(full)))
		return false;
	m_fp = fopen(full, "wb");
	m_mode = 2;
	m_good = m_fp != NULL;
	return m_good;
}

bool Stream::OpenForRead(const char *path)
{
	Close();
	char full[512];
	if (!ResolveGamePath(path, full, sizeof(full)))
		return false;
	m_fp = fopen(full, "rb");
	m_mode = 1;
	m_good = m_fp != NULL;
	return m_good;
}

void Stream::Close()
{
	if (m_fp)
	{
		fclose(m_fp);
		m_fp = NULL;
	}
	m_mode = 0;
	m_good = false;
}

bool Stream::WriteRaw(const void *data, size_t size)
{
	if (!m_fp || fwrite(data, 1, size, m_fp) != size)
	{
		m_good = false;
		return false;
	}
	return true;
}

bool Stream::ReadRaw(void *data, size_t size)
{
	if (!m_fp || fread(data, 1, size, m_fp) != size)
	{
		m_good = false;
		return false;
	}
	return true;
}

bool Stream::WriteInt(int v) { return WriteRaw(&v, sizeof(v)); }
bool Stream::WriteFloat(float v) { return WriteRaw(&v, sizeof(v)); }

bool Stream::WriteVector(const Vector &v)
{
	return WriteFloat(v.x) && WriteFloat(v.y) && WriteFloat(v.z);
}

bool Stream::WriteString(const char *s)
{
	int len = s ? (int)strlen(s) : 0;
	if (!WriteInt(len))
		return false;
	if (len && !WriteRaw(s, len))
		return false;
	return true;
}

int Stream::ReadInt()
{
	int v = 0;
	ReadRaw(&v, sizeof(v));
	return v;
}

float Stream::ReadFloat()
{
	float v = 0.0f;
	ReadRaw(&v, sizeof(v));
	return v;
}

Vector Stream::ReadVector()
{
	Vector v;
	v.x = ReadFloat();
	v.y = ReadFloat();
	v.z = ReadFloat();
	return v;
}

void Stream::ReadString(char *out, size_t outLen)
{
	int len = ReadInt();
	if (len < 0 || !outLen)
	{
		if (outLen)
			out[0] = '\0';
		return;
	}
	if ((size_t)len >= outLen)
		len = (int)outLen - 1;
	if (len)
		ReadRaw(out, len);
	out[len] = '\0';
}

bool Stream::IsLegacyContainer(const char *path)
{
	char full[512];
	if (!ResolveGamePath(path, full, sizeof(full)))
		return false;
	FILE *fp = fopen(full, "rb");
	if (!fp)
		return false;
	char probe[64];
	size_t n = fread(probe, 1, sizeof(probe), fp);
	fclose(fp);
	if (n < 16)
		return false;
	// Legacy containers begin with a few encoded bytes followed by the ASCII
	// banner "Hurricane Bot ver ...".
	for (size_t i = 0; i + 15 < n; ++i)
	{
		if (memcmp(probe + i, "Hurricane Bot ver", 16) == 0)
			return true;
	}
	return false;
}

} // namespace hrc
