// Navigation.cpp - data model implementation (Node/Arc/Room/Gateway/Network).
#include "Navigation.h"
#include <math.h>
#include <string.h>

namespace hrc
{

// ---------------------------------------------------------------------------
// Node
// ---------------------------------------------------------------------------

Node::Node()
	: m_id(-1), m_roomId(-1), m_flags(0), m_arcCount(0), m_cost(0.0f)
{
	m_origin.Init();
	for (int i = 0; i < HRC_MAX_LINKS_PER_NODE; ++i)
		m_arcs[i] = NULL;
}

Node::Node(int id, const Vector &origin)
	: m_id(id), m_origin(origin), m_roomId(-1), m_flags(0), m_arcCount(0),
	  m_cost(0.0f)
{
	for (int i = 0; i < HRC_MAX_LINKS_PER_NODE; ++i)
		m_arcs[i] = NULL;
}

bool Node::AddArc(Arc *arc)
{
	if (!arc || m_arcCount >= HRC_MAX_LINKS_PER_NODE)
		return false;
	// Avoid duplicate links to the same neighbour.
	for (int i = 0; i < m_arcCount; ++i)
	{
		if (m_arcs[i]->Other(this) == arc->Other(this))
			return false;
	}
	m_arcs[m_arcCount++] = arc;
	return true;
}

Arc *Node::GetArc(int index) const
{
	if (index < 0 || index >= m_arcCount)
		return NULL;
	return m_arcs[index];
}

int Node::NeighborId(int index) const
{
	Arc *arc = GetArc(index);
	if (!arc)
		return -1;
	Node *other = arc->Other(this);
	return other ? other->Id() : -1;
}

// ---------------------------------------------------------------------------
// Arc
// ---------------------------------------------------------------------------

Arc::Arc() : m_id(-1), m_a(NULL), m_b(NULL), m_length(0.0f), m_flags(0) {}

Arc::Arc(int id, Node *a, Node *b)
	: m_id(id), m_a(a), m_b(b), m_flags(0)
{
	m_length = (a->Origin() - b->Origin()).Length();
}

// ---------------------------------------------------------------------------
// Room
// ---------------------------------------------------------------------------

Room::Room() : m_id(-1), m_gatewayCount(0)
{
	for (int i = 0; i < HRC_MAX_LINKS_PER_NODE; ++i)
		m_gateways[i] = -1;
}

Room::Room(int id) : m_id(id), m_gatewayCount(0)
{
	for (int i = 0; i < HRC_MAX_LINKS_PER_NODE; ++i)
		m_gateways[i] = -1;
}

bool Room::AddNode(int nodeId)
{
	if (m_nodes.Contains(nodeId))
		return false;
	return m_nodes.Add(nodeId);
}

bool Room::AddGateway(int gatewayId)
{
	if (m_gatewayCount >= HRC_MAX_LINKS_PER_NODE)
		return false;
	for (int i = 0; i < m_gatewayCount; ++i)
	{
		if (m_gateways[i] == gatewayId)
			return false;
	}
	m_gateways[m_gatewayCount++] = gatewayId;
	return true;
}

// ---------------------------------------------------------------------------
// Gateway
// ---------------------------------------------------------------------------

Gateway::Gateway()
	: m_id(-1), m_roomA(-1), m_roomB(-1), m_nodeA(-1), m_nodeB(-1)
{
}

Gateway::Gateway(int id, int roomA, int roomB, int nodeA, int nodeB)
	: m_id(id), m_roomA(roomA), m_roomB(roomB), m_nodeA(nodeA), m_nodeB(nodeB)
{
}

// ---------------------------------------------------------------------------
// Network
// ---------------------------------------------------------------------------

Network::Network()
	: m_nodeCount(0), m_arcCount(0), m_roomCount(0), m_gatewayCount(0)
{
	memset(m_nodes, 0, sizeof(m_nodes));
	memset(m_arcs, 0, sizeof(m_arcs));
	memset(m_rooms, 0, sizeof(m_rooms));
	memset(m_gateways, 0, sizeof(m_gateways));
}

Network::~Network()
{
	Clear();
}

void Network::Clear()
{
	for (int i = 0; i < m_nodeCount; ++i)
		delete m_nodes[i];
	for (int i = 0; i < m_arcCount; ++i)
		delete m_arcs[i];
	for (int i = 0; i < m_roomCount; ++i)
		delete m_rooms[i];
	for (int i = 0; i < m_gatewayCount; ++i)
		delete m_gateways[i];
	m_nodeCount = m_arcCount = m_roomCount = m_gatewayCount = 0;
}

Node *Network::CreateNode(const Vector &origin)
{
	if (m_nodeCount >= HRC_MAX_NAV_NODES)
		return NULL;
	Node *n = new Node(m_nodeCount, origin);
	m_nodes[m_nodeCount++] = n;
	return n;
}

Arc *Network::CreateArc(Node *a, Node *b)
{
	if (!a || !b || m_arcCount >= HRC_MAX_NAV_ARCS)
		return NULL;
	Arc *arc = new Arc(m_arcCount, a, b);
	a->AddArc(arc);
	b->AddArc(arc);
	m_arcs[m_arcCount++] = arc;
	return arc;
}

Room *Network::CreateRoom()
{
	if (m_roomCount >= HRC_MAX_NAV_ROOMS)
		return NULL;
	Room *r = new Room(m_roomCount);
	m_rooms[m_roomCount++] = r;
	return r;
}

Gateway *Network::CreateGateway(int roomA, int roomB, int nodeA, int nodeB)
{
	if (m_gatewayCount >= HRC_MAX_NAV_GATEWAYS)
		return NULL;
	Gateway *g = new Gateway(m_gatewayCount, roomA, roomB, nodeA, nodeB);
	if (roomA >= 0 && m_rooms[roomA])
		m_rooms[roomA]->AddGateway(m_gatewayCount);
	if (roomB >= 0 && m_rooms[roomB])
		m_rooms[roomB]->AddGateway(m_gatewayCount);
	m_gateways[m_gatewayCount++] = g;
	return g;
}

Node *Network::GetNode(int id) const
{
	if (id < 0 || id >= m_nodeCount)
		return NULL;
	return m_nodes[id];
}

Arc *Network::GetArc(int id) const
{
	if (id < 0 || id >= m_arcCount)
		return NULL;
	return m_arcs[id];
}

Room *Network::GetRoom(int id) const
{
	if (id < 0 || id >= m_roomCount)
		return NULL;
	return m_rooms[id];
}

Gateway *Network::GetGateway(int id) const
{
	if (id < 0 || id >= m_gatewayCount)
		return NULL;
	return m_gateways[id];
}

Node *Network::FindClosestNode(const Vector &v, float maxDist) const
{
	Node *best = NULL;
	float bestDist = maxDist * maxDist;
	for (int i = 0; i < m_nodeCount; ++i)
	{
		float d = (m_nodes[i]->Origin() - v).LengthSqr();
		if (d < bestDist)
		{
			bestDist = d;
			best = m_nodes[i];
		}
	}
	return best;
}

Node *Network::FindClosestNode2D(const Vector &v, float maxDist) const
{
	Node *best = NULL;
	float bestDist = maxDist * maxDist;
	for (int i = 0; i < m_nodeCount; ++i)
	{
		float dx = m_nodes[i]->Origin().x - v.x;
		float dy = m_nodes[i]->Origin().y - v.y;
		float d = dx * dx + dy * dy;
		if (d < bestDist)
		{
			bestDist = d;
			best = m_nodes[i];
		}
	}
	return best;
}

// ---------------------------------------------------------------------------
// NetworkRaster
// ---------------------------------------------------------------------------

NetworkRaster::NetworkRaster()
	: m_cellSize(32.0f), m_cellsX(0), m_cellsY(0), m_cells(NULL)
{
	m_origin.Init();
}

NetworkRaster::~NetworkRaster()
{
	delete[] m_cells;
}

void NetworkRaster::Configure(const Vector &origin, float cellSize, int cellsX,
                              int cellsY)
{
	delete[] m_cells;
	m_origin = origin;
	m_cellSize = cellSize > 1.0f ? cellSize : 32.0f;
	m_cellsX = cellsX;
	m_cellsY = cellsY;
	m_cells = new unsigned char[cellsX * cellsY];
	Clear();
}

void NetworkRaster::Clear()
{
	if (m_cells)
		memset(m_cells, 0, sizeof(unsigned char) * m_cellsX * m_cellsY);
}

bool NetworkRaster::InBounds(int x, int y) const
{
	return x >= 0 && y >= 0 && x < m_cellsX && y < m_cellsY;
}

unsigned char &NetworkRaster::Cell(int x, int y)
{
	return m_cells[y * m_cellsX + x];
}

unsigned char NetworkRaster::Cell(int x, int y) const
{
	return m_cells[y * m_cellsX + x];
}

Vector NetworkRaster::CellCenter(int x, int y) const
{
	return Vector(m_origin.x + (x + 0.5f) * m_cellSize,
	              m_origin.y + (y + 0.5f) * m_cellSize, m_origin.z);
}

bool NetworkRaster::WorldToCell(const Vector &v, int &x, int &y) const
{
	x = (int)((v.x - m_origin.x) / m_cellSize);
	y = (int)((v.y - m_origin.y) / m_cellSize);
	return InBounds(x, y);
}

} // namespace hrc
