// Navigation.h
// Navigation mesh data model recovered from the original class names:
//   Node, Arc, Gateway, Room, Network, NetworkRaster.
//
// The original 1.3.4 sources split these across many translation units
// (Node.cpp, Arc.cpp, Gateway.cpp, Room.cpp, Room_divideWith.cpp,
// Network.cpp, NetworkRaster.cpp).  The modernized tree keeps the class names
// and responsibilities but consolidates the boilerplate into this header.
//
// This is a clean-room reimplementation of the data structures; the original
// automatic "Dedale" analysis algorithm itself is not recoverable from a
// stripped binary and is reimplemented in Dedale.cpp/Poseidon.cpp.
#ifndef HRCBOT_MAP_NAVIGATION_H_
#define HRCBOT_MAP_NAVIGATION_H_

#include <mathlib/vector.h>
#include "containers/TList.h"
#include "containers/TCollection.h"
#include "containers/TSet.h"
#include "containers/TStack.h"

namespace hrc
{

static const int HRC_NAV_CAPACITY = 4096;
static const int HRC_MAX_LINKS_PER_NODE = 16;

class Arc;
class Room;
class Gateway;
class Network;

// A navigation node is a walkable point on the map.
class Node
{
public:
	Node();
	Node(int id, const Vector &origin);

	int Id() const { return m_id; }
	const Vector &Origin() const { return m_origin; }
	void SetOrigin(const Vector &v) { m_origin = v; }

	int RoomId() const { return m_roomId; }
	void SetRoomId(int room) { m_roomId = room; }

	int Flags() const { return m_flags; }
	void SetFlag(int bit) { m_flags |= bit; }
	void ClearFlag(int bit) { m_flags &= ~bit; }
	bool HasFlag(int bit) const { return (m_flags & bit) != 0; }

	// Arc bookkeeping.
	int ArcCount() const { return m_arcCount; }
	Arc *GetArc(int index) const;
	bool AddArc(Arc *arc);
	int NeighborId(int index) const;

	float Cost() const { return m_cost; }
	void SetCost(float c) { m_cost = c; }

private:
	int m_id;
	Vector m_origin;
	int m_roomId;
	int m_flags;
	int m_arcCount;
	Arc *m_arcs[HRC_MAX_LINKS_PER_NODE];
	float m_cost; // transient search cost
};

// A bidirectional connection between two nodes.
class Arc
{
public:
	Arc();
	Arc(int id, Node *a, Node *b);

	int Id() const { return m_id; }
	Node *From() const { return m_a; }
	Node *To() const { return m_b; }
	Node *Other(const Node *n) const
	{
		return n == m_a ? m_b : (n == m_b ? m_a : NULL);
	}
	float Length() const { return m_length; }
	void SetLength(float l) { m_length = l; }
	int Flags() const { return m_flags; }
	void SetFlags(int f) { m_flags = f; }

private:
	int m_id;
	Node *m_a;
	Node *m_b;
	float m_length;
	int m_flags;
};

// Axis aligned bounds used by rooms and the raster.
class Bounds
{
public:
	Bounds() { Clear(); }
	void Clear()
	{
		m_mins.Init(1e9f, 1e9f, 1e9f);
		m_maxs.Init(-1e9f, -1e9f, -1e9f);
	}
	void AddPoint(const Vector &v)
	{
		for (int i = 0; i < 3; ++i)
		{
			if (v[i] < m_mins[i])
				m_mins[i] = v[i];
			if (v[i] > m_maxs[i])
				m_maxs[i] = v[i];
		}
	}
	const Vector &Mins() const { return m_mins; }
	const Vector &Maxs() const { return m_maxs; }
	Vector Size() const { return m_maxs - m_mins; }
	float Superficy() const
	{
		Vector s = Size();
		return s.x * s.y;
	}

private:
	Vector m_mins;
	Vector m_maxs;
};

// A room is a connected region of nodes (the original flood-fill result).
class Room
{
public:
	Room();
	explicit Room(int id);

	int Id() const { return m_id; }
	int NodeCount() const { return m_nodes.Count(); }
	int GetNode(int i) const { return m_nodes[i]; }
	bool AddNode(int nodeId);
	const Bounds &GetBounds() const { return m_bounds; }
	void AddBounds(const Vector &v) { m_bounds.AddPoint(v); }
	int GatewayCount() const { return m_gatewayCount; }
	int GetGateway(int i) const { return m_gateways[i]; }
	bool AddGateway(int gatewayId);

private:
	int m_id;
	TList<int> m_nodes;
	Bounds m_bounds;
	int m_gatewayCount;
	int m_gateways[HRC_MAX_LINKS_PER_NODE];
};

// A gateway connects two rooms through a pair of facing nodes.
class Gateway
{
public:
	Gateway();
	Gateway(int id, int roomA, int roomB, int nodeA, int nodeB);

	int Id() const { return m_id; }
	int RoomA() const { return m_roomA; }
	int RoomB() const { return m_roomB; }
	int NodeA() const { return m_nodeA; }
	int NodeB() const { return m_nodeB; }
	int OtherRoom(int room) const
	{
		return room == m_roomA ? m_roomB : (room == m_roomB ? m_roomA : -1);
	}

private:
	int m_id;
	int m_roomA;
	int m_roomB;
	int m_nodeA;
	int m_nodeB;
};

// The navigation graph for one map.
class Network
{
public:
	Network();
	~Network();

	void Clear();

	Node *CreateNode(const Vector &origin);
	Arc *CreateArc(Node *a, Node *b);
	Room *CreateRoom();
	Gateway *CreateGateway(int roomA, int roomB, int nodeA, int nodeB);

	Node *GetNode(int id) const;
	Arc *GetArc(int id) const;
	Room *GetRoom(int id) const;
	Gateway *GetGateway(int id) const;

	int NodeCount() const { return m_nodeCount; }
	int ArcCount() const { return m_arcCount; }
	int RoomCount() const { return m_roomCount; }
	int GatewayCount() const { return m_gatewayCount; }

	Node *FindClosestNode(const Vector &v, float maxDist = 1e9f) const;
	// Same as FindClosestNode but uses horizontal (X/Y) distance only, which
	// keeps a spawn point on a different vertical level tied to its floor's
	// node instead of failing the three-dimensional radius test.
	Node *FindClosestNode2D(const Vector &v, float maxDist = 1e9f) const;

private:
	Node *m_nodes[HRC_NAV_CAPACITY];
	Arc *m_arcs[HRC_NAV_CAPACITY];
	Room *m_rooms[HRC_NAV_CAPACITY / 8];
	Gateway *m_gateways[HRC_NAV_CAPACITY / 8];
	int m_nodeCount;
	int m_arcCount;
	int m_roomCount;
	int m_gatewayCount;
};

// 2D top-down occupancy raster used while analysing a map.  Each cell records
// whether the volume above it is walkable for a standing player hull.
class NetworkRaster
{
public:
	NetworkRaster();
	~NetworkRaster();

	void Configure(const Vector &origin, float cellSize, int cellsX,
	               int cellsY);
	void Clear();
	bool InBounds(int x, int y) const;
	unsigned char &Cell(int x, int y);
	unsigned char Cell(int x, int y) const;
	Vector CellCenter(int x, int y) const;
	bool WorldToCell(const Vector &v, int &x, int &y) const;

	int CellsX() const { return m_cellsX; }
	int CellsY() const { return m_cellsY; }
	float CellSize() const { return m_cellSize; }
	const Vector &Origin() const { return m_origin; }

private:
	Vector m_origin;
	float m_cellSize;
	int m_cellsX;
	int m_cellsY;
	unsigned char *m_cells;
};

} // namespace hrc

#endif // HRCBOT_MAP_NAVIGATION_H_
