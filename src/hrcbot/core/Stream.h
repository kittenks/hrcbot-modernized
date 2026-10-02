// Stream.h
// Binary serialization used for the per-map navigation containers.
//
// The original 1.3.4 release wrote "addons/hrcbot_server_plugin/<map>-<crc>
// .hrcbot" files with a custom compact bit writer (Stream.cpp).  Only the
// ASCII banner ("Hurricane Bot ver 1.1.0v (80204)") and map name survive in
// the shipped containers; the exact bit layout is not recoverable from the
// stripped binary.
//
// The modernized tree therefore writes a new, versioned, little-endian format
// with the ".hrcbot2" extension and still recognises the legacy banner so old
// containers can be detected and the map re-analysed instead of crashing.
#ifndef HRCBOT_CORE_STREAM_H_
#define HRCBOT_CORE_STREAM_H_

#include <stddef.h>
#include <stdio.h>
#include <mathlib/vector.h>

namespace hrc
{

#pragma pack(push, 1)
struct HrcContainerHeader
{
	char magic[8];       // "HRCBOT2\0"
	char banner[40];     // human readable version banner
	int  formatVersion;  // container format version
	int  mapCrc;
	char mapName[64];
	int  nodeCount;
	int  arcCount;
	int  roomCount;
	int  gatewayCount;
};
#pragma pack(pop)

static const int HRC_STREAM_VERSION = 2;

class Stream
{
public:
	Stream();
	~Stream();

	bool OpenForWrite(const char *path);
	bool OpenForRead(const char *path);
	void Close();

	bool WriteRaw(const void *data, size_t size);
	bool ReadRaw(void *data, size_t size);

	bool WriteInt(int v);
	bool WriteFloat(float v);
	bool WriteVector(const Vector &v);
	bool WriteString(const char *s);

	int ReadInt();
	float ReadFloat();
	Vector ReadVector();
	void ReadString(char *out, size_t outLen);

	bool IsReading() const { return m_mode == 1; }
	bool IsWriting() const { return m_mode == 2; }
	bool Good() const { return m_good; }

	// Returns true if the file begins with the legacy 1.3.4 ASCII banner.
	static bool IsLegacyContainer(const char *path);

private:
	FILE *m_fp;
	int m_mode; // 1 read, 2 write
	bool m_good;
};

} // namespace hrc

#endif // HRCBOT_CORE_STREAM_H_
