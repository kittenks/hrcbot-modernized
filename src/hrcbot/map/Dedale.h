// Dedale.h
// Automatic map analysis.  The original plugin built its navigation data with
// a family of translation units:
//   Dedale.cpp, Dedale_analyseMap.cpp, Dedale_raster.cpp,
//   Dedale_spreadNetwork.cpp, Dedale_buildPaths.cpp, Dedale_buildRoom.cpp,
//   Dedale_buildRooms.cpp, Dedale_buildGateways.cpp,
//   Dedale_optimizeRooms.cpp, Dedale_loadMap.cpp, Dedale_saveMap.cpp.
//
// The exact geometry algorithm is not present in a stripped binary, so this is
// a documented clean-room reimplementation that produces the same Network data
// model using engine hull traces.  Legacy ".hrcbot" containers are handled by
// Stream.cpp; when no container exists the analyzer builds and saves one.
#ifndef HRCBOT_MAP_DEDALE_H_
#define HRCBOT_MAP_DEDALE_H_

#include "Navigation.h"

namespace hrc
{

class Stream;

class Dedale
{
public:
	Dedale(Network *network, NetworkRaster *raster);

	// Build a navigation network around the given seed origin (typically world
	// zero or the first spawn point).  Returns true on success.
	bool AnalyseMap(const Vector &seed);

	// Persist / restore the analysed network.
	bool SaveMap(const char *mapName, int mapCrc);
	bool LoadMap(const char *mapName, int mapCrc);

	// Absolute path that would be used for a given map.
	static void ContainerPath(char *out, size_t outLen, const char *mapName,
	                          int mapCrc, bool legacy2 = false);

private:
	void Rasterize(const Vector &seed);
	void SpreadNetwork();
	void BuildRooms();
	void BuildGateways();
	void OptimizeRooms();

	bool Serialize(Stream &stream);
	bool Deserialize(Stream &stream);

	Network *m_network;
	NetworkRaster *m_raster;

	// Analysis tuning.
	float m_cellSize;
	int m_gridRadius;     // cells in each direction from the seed
	float m_nodeSpacing;  // distance between spawned nodes
	float m_stepHeight;
	float m_jumpHeight;
};

} // namespace hrc

#endif // HRCBOT_MAP_DEDALE_H_
