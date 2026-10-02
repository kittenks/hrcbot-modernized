// Poseidon.h
// Path finder over the Network produced by Dedale.  The original 1.3.4 binary
// contained a Poseidon class; the modern implementation uses a classic A*
// search across nodes (room gateways are already encoded as node proximity).
#ifndef HRCBOT_MAP_POSEIDON_H_
#define HRCBOT_MAP_POSEIDON_H_

#include <mathlib/vector.h>
#include "containers/TList.h"

namespace hrc
{

class Network;
class Node;

class Poseidon
{
public:
	explicit Poseidon(Network *network);

	// Build a path of node ids from the nearest node to start to the nearest
	// node to goal.  Returns true and fills path on success.
	bool FindPath(const Vector &start, const Vector &goal,
	              TList<int> &path, float maxStartDist = 512.0f,
	              float maxGoalDist = 512.0f);

	// Convenience: next walkable waypoint toward a moving goal, given the
	// mover's current position and an existing path index cursor.
	bool NextWaypoint(const Vector &from, const Vector &goal,
	                  Vector &waypoint);

	void Invalidate();

private:
	bool AStar(int startNode, int goalNode, TList<int> &path);

	Network *m_network;
	TList<int> m_cachedPath;
	int m_cursor;
	Vector m_lastGoal;
};

} // namespace hrc

#endif // HRCBOT_MAP_POSEIDON_H_
