// Poseidon.cpp - A* path finder.
#include "Poseidon.h"
#include <math.h>
#include <time.h>
#include <limits.h>
#include "Navigation.h"
#include "core/Tools.h"

namespace hrc
{

static const float kInfinity = 1e18f;

Poseidon::Poseidon(Network *network)
	: m_network(network), m_cursor(0), m_adj(NULL), m_adjNodes(0)
{
	m_lastGoal.Init();
}

Poseidon::~Poseidon()
{
	delete[] m_adj;
}

void Poseidon::EnsureAdjacency()
{
	int n = m_network ? m_network->NodeCount() : 0;
	if (m_adj && m_adjNodes == n)
		return;
	delete[] m_adj;
	m_adj = NULL;
	m_adjNodes = 0;
	if (n <= 0)
		return;

	m_adj = new TList<int>[n];
	m_adjNodes = n;

	// Regular node arcs (already bidirectional, but add both ways defensively).
	for (int i = 0; i < m_network->ArcCount(); ++i)
	{
		Arc *a = m_network->GetArc(i);
		if (!a || !a->From() || !a->To())
			continue;
		int u = a->From()->Id();
		int v = a->To()->Id();
		if (u >= 0 && u < n && v >= 0 && v < n)
		{
			if (!m_adj[u].Contains(v))
				m_adj[u].Add(v);
			if (!m_adj[v].Contains(u))
				m_adj[v].Add(u);
		}
	}

	// Cross-room gateways are the missing edges that join each room's local
	// arc graph into a single traversable map; without them A* cannot leave a
	// room and bots near isolated nodes never find a destination.
	for (int i = 0; i < m_network->GatewayCount(); ++i)
	{
		Gateway *g = m_network->GetGateway(i);
		if (!g)
			continue;
		int u = g->NodeA();
		int v = g->NodeB();
		if (u >= 0 && u < n && v >= 0 && v < n)
		{
			if (!m_adj[u].Contains(v))
				m_adj[u].Add(v);
			if (!m_adj[v].Contains(u))
				m_adj[v].Add(u);
		}
	}
}

void Poseidon::Invalidate()
{
	m_cachedPath.Clear();
	m_cursor = 0;
	m_lastGoal.Init();
}

bool Poseidon::AStar(int startNode, int goalNode, TList<int> &path)
{
	int n = m_network->NodeCount();
	if (startNode < 0 || goalNode < 0 || startNode >= n || goalNode >= n)
		return false;
	EnsureAdjacency();

	float *gScore = new float[n];
	float *fScore = new float[n];
	int *parent = new int[n];
	bool *closed = new bool[n];
	bool *open = new bool[n];
	for (int i = 0; i < n; ++i)
	{
		gScore[i] = kInfinity;
		fScore[i] = kInfinity;
		parent[i] = -1;
		closed[i] = false;
		open[i] = false;
	}

	Node *goal = m_network->GetNode(goalNode);
	open[startNode] = true;
	gScore[startNode] = 0.0f;
	fScore[startNode] =
		(m_network->GetNode(startNode)->Origin() - goal->Origin()).Length();

	bool found = false;
	while (true)
	{
		int current = -1;
		float best = kInfinity;
		for (int i = 0; i < n; ++i)
		{
			if (open[i] && fScore[i] < best)
			{
				best = fScore[i];
				current = i;
			}
		}
		if (current < 0)
			break;
		if (current == goalNode)
		{
			found = true;
			break;
		}
		open[current] = false;
		closed[current] = true;

		Node *cn = m_network->GetNode(current);
		for (int a = 0; a < m_adj[current].Count(); ++a)
		{
			int nid = m_adj[current][a];
			if (nid < 0 || closed[nid])
				continue;
			Node *nn = m_network->GetNode(nid);
			if (!nn || !cn)
				continue;
			float tentative = gScore[current] +
			                  (nn->Origin() - cn->Origin()).Length();
			if (!open[nid])
				open[nid] = true;
			else if (tentative >= gScore[nid])
				continue;
			parent[nid] = current;
			gScore[nid] = tentative;
			fScore[nid] = tentative +
			             (nn->Origin() - goal->Origin()).Length();
		}
	}

	if (found)
	{
		TStack<int> reverse;
		int c = goalNode;
		while (c != -1)
		{
			reverse.Push(c);
			c = parent[c];
		}
		while (!reverse.IsEmpty())
			path.Add(reverse.Pop());
	}

	delete[] gScore;
	delete[] fScore;
	delete[] parent;
	delete[] closed;
	delete[] open;
	return found;
}

bool Poseidon::FindPath(const Vector &start, const Vector &goal,
                        TList<int> &path, float maxStartDist,
                        float maxGoalDist)
{
	if (!m_network || m_network->NodeCount() == 0)
		return false;
	// Resolve the start on horizontal distance so a vertical offset between a
	// spawn point and its floor node does not abort the path.  Roam goals are
	// nodes themselves, but keep the 3D goal test for caller-supplied points.
	Node *sn = m_network->FindClosestNode2D(start, maxStartDist);
	if (!sn)
		sn = m_network->FindClosestNode(start, maxStartDist);
	Node *gn = m_network->FindClosestNode(goal, maxGoalDist);
	if (!sn || !gn)
		return false;
	path.Clear();
	return AStar(sn->Id(), gn->Id(), path);
}

bool Poseidon::NextWaypoint(const Vector &from, const Vector &goal,
                            Vector &waypoint)
{
	// Recompute if the goal moved significantly or there is no path.
	if (m_cachedPath.IsEmpty() ||
	    (m_lastGoal - goal).LengthSqr() > 128.0f * 128.0f)
	{
		m_lastGoal = goal;
		m_cachedPath.Clear();
		if (!FindPath(from, goal, m_cachedPath))
			return false;
		m_cursor = 0;
	}

	// Advance the cursor when close to the current waypoint.
	while (m_cursor < m_cachedPath.Count())
	{
		Node *n = m_network->GetNode(m_cachedPath[m_cursor]);
		if (!n)
		{
			++m_cursor;
			continue;
		}
		Vector horiz(n->Origin().x, n->Origin().y, from.z);
		if ((horiz - from).Length2D() < 28.0f)
		{
			++m_cursor;
			continue;
		}
		waypoint = n->Origin();
		return true;
	}
	return false;
}

bool Poseidon::RandomGoal(const Vector &near, float minDist, Vector &goal)
{
	if (!m_network || m_network->NodeCount() == 0)
		return false;

	int n = m_network->NodeCount();
	EnsureAdjacency();

	// Resolve the start node the same way FindPath does (horizontal first),
	// then collect every node reachable from it along the unified adjacency so
	// the goal is guaranteed to be in the same connected component and A* can
	// route to it.
	Node *start = m_network->FindClosestNode2D(near, 1024.0f);
	if (!start)
		start = m_network->FindClosestNode(near, 1024.0f);
	if (!start)
		return false;

	bool *seen = new bool[n];
	int *stack = new int[n];
	for (int i = 0; i < n; ++i)
		seen[i] = false;
	int top = 0;
	stack[top++] = start->Id();
	seen[start->Id()] = true;
	while (top > 0)
	{
		int cur = stack[--top];
		for (int k = 0; k < m_adj[cur].Count(); ++k)
		{
			int nid = m_adj[cur][k];
			if (nid >= 0 && nid < n && !seen[nid])
			{
				seen[nid] = true;
				stack[top++] = nid;
			}
		}
	}

	HRandomStream rng((unsigned int)time(NULL) ^ 0x9e3779b9u);

	// Prefer a random reachable node comfortably far from the start.
	for (int attempt = 0; attempt < 32; ++attempt)
	{
		int id = rng.RandomInt(0, n - 1);
		if (!seen[id])
			continue;
		Node *nd = m_network->GetNode(id);
		if (nd && (nd->Origin() - near).Length2D() >= minDist)
		{
			goal = nd->Origin();
			delete[] seen;
			delete[] stack;
			return true;
		}
	}

	// Fallback: the reachable node farthest from the start (may be the start
	// node itself if it is isolated).
	Node *best = NULL;
	float bestDist = -1.0f;
	for (int i = 0; i < n; ++i)
	{
		if (!seen[i])
			continue;
		Node *nd = m_network->GetNode(i);
		if (nd)
		{
			float d = (nd->Origin() - near).Length2D();
			if (d > bestDist)
			{
				bestDist = d;
				best = nd;
			}
		}
	}
	delete[] seen;
	delete[] stack;
	if (!best)
		return false;
	goal = best->Origin();
	return true;
}

float Poseidon::NearestNodeDistance(const Vector &v) const
{
	if (!m_network || m_network->NodeCount() == 0)
		return -1.0f;
	float best = kInfinity;
	for (int i = 0; i < m_network->NodeCount(); ++i)
	{
		Node *nd = m_network->GetNode(i);
		if (nd)
		{
			float d = (nd->Origin() - v).Length2D();
			if (d < best)
				best = d;
		}
	}
	return best;
}

} // namespace hrc
