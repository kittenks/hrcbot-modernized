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
	~Poseidon();

	// Build a path of node ids from the nearest node to start to the nearest
	// node to goal.  Returns true and fills path on success.
	bool FindPath(const Vector &start, const Vector &goal,
	              TList<int> &path, float maxStartDist = 1024.0f,
	              float maxGoalDist = 640.0f);

	// Convenience: next walkable waypoint toward a moving goal, given the
	// mover's current position and an existing path index cursor.
	bool NextWaypoint(const Vector &from, const Vector &goal,
	                  Vector &waypoint);

	// Pick a connected navigation node at least minDist (2D) away from 'near'
	// as a roam goal.  Choosing a node (rather than an arbitrary world point)
	// guarantees the goal resolves on the graph.  Returns false if the
	// network is empty.
	bool RandomGoal(const Vector &near, float minDist, Vector &goal);

	// Horizontal distance from a world position to the nearest navigation
	// node; used to tell whether a spot (e.g. a spawn point) is covered by the
	// graph so the path finder can resolve a start node.
	float NearestNodeDistance(const Vector &v) const;

	void Invalidate();

private:
	bool AStar(int startNode, int goalNode, TList<int> &path);
	// Build the unified traversal adjacency (node arcs plus cross-room
	// gateway links); cached and rebuilt only when the node count changes.
	void EnsureAdjacency();

	Network *m_network;
	TList<int> m_cachedPath;
	int m_cursor;
	Vector m_lastGoal;

	// Unified neighbour lists: m_adj[i] holds every node reachable from node i
	// through either an Arc or a cross-room Gateway.
	TList<int> *m_adj;
	int m_adjNodes;
};

} // namespace hrc

#endif // HRCBOT_MAP_POSEIDON_H_
