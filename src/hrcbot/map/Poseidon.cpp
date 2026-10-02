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
	: m_network(network), m_cursor(0)
{
	m_lastGoal.Init();
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
		for (int a = 0; a < cn->ArcCount(); ++a)
		{
			int nid = cn->NeighborId(a);
			if (nid < 0 || closed[nid])
				continue;
			Node *nn = m_network->GetNode(nid);
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
	Node *sn = m_network->FindClosestNode(start, maxStartDist);
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
	HRandomStream rng((unsigned int)time(NULL) ^ 0x9e3779b9u);

	// Prefer a connected node comfortably far from the current position.
	for (int attempt = 0; attempt < 32; ++attempt)
	{
		Node *nd = m_network->GetNode(rng.RandomInt(0, n - 1));
		if (!nd || nd->ArcCount() == 0)
			continue;
		if ((nd->Origin() - near).Length2D() >= minDist)
		{
			goal = nd->Origin();
			return true;
		}
	}

	// Fallback: the connected node farthest from the current position.
	Node *best = NULL;
	float bestDist = -1.0f;
	for (int i = 0; i < n; ++i)
	{
		Node *nd = m_network->GetNode(i);
		if (!nd || nd->ArcCount() == 0)
			continue;
		float d = (nd->Origin() - near).Length2D();
		if (d > bestDist)
		{
			bestDist = d;
			best = nd;
		}
	}
	if (!best)
		return false;
	goal = best->Origin();
	return true;
}

} // namespace hrc
