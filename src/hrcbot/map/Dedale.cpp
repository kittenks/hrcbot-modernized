// Dedale.cpp - clean-room automatic map analysis and container persistence.
#include "Dedale.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "core/HrcEngine.h"
#include "core/Stream.h"
#include "core/Tools.h"

namespace hrc
{

static const unsigned char CELL_EMPTY = 0;
static const unsigned char CELL_WALKABLE = 1;
static const unsigned char CELL_VISITED = 2;

Dedale::Dedale(Network *network, NetworkRaster *raster)
	: m_network(network), m_raster(raster),
	  m_cellSize(64.0f), m_gridRadius(48),
	  m_nodeSpacing(96.0f), m_stepHeight(18.0f), m_jumpHeight(48.0f)
{
}

void Dedale::ContainerPath(char *out, size_t outLen, const char *mapName,
                           int mapCrc, bool legacy2)
{
	snprintf(out, outLen, "addons/hrcbot_server_plugin/%s-%i.%s",
	         mapName, mapCrc, legacy2 ? "hrcbot2" : "hrcbot");
}

// ---------------------------------------------------------------------------
// Rasterization: probe every cell for a standing player hull and ground.
// ---------------------------------------------------------------------------

void Dedale::Rasterize(const Vector &seed)
{
	int cells = m_gridRadius * 2;
	Vector origin(seed.x - m_gridRadius * m_cellSize,
	              seed.y - m_gridRadius * m_cellSize, seed.z);
	m_raster->Configure(origin, m_cellSize, cells, cells);

	for (int y = 0; y < cells; ++y)
	{
		for (int x = 0; x < cells; ++x)
		{
			Vector center = m_raster->CellCenter(x, y);
			float groundZ = 0.0f;
			// Probe from a height above the expected surface.
			Vector high(center.x, center.y, center.z + 256.0f);
			if (GroundHeight(high, groundZ, NULL, 384.0f))
			{
				Vector stand(center.x, center.y, groundZ + 1.0f);
				if (!IsPositionStuck(stand))
					m_raster->Cell(x, y) = CELL_WALKABLE;
			}
		}
	}
}

// ---------------------------------------------------------------------------
// Turn walkable cells into nodes and connect the reachable neighbours.
// ---------------------------------------------------------------------------

void Dedale::SpreadNetwork()
{
	int w = m_raster->CellsX();
	int h = m_raster->CellsY();

	// One node per walkable cell.  Keep a mapping cell -> node id.
	int *cellNode = new int[w * h];
	for (int i = 0; i < w * h; ++i)
		cellNode[i] = -1;

	for (int y = 0; y < h; ++y)
	{
		for (int x = 0; x < w; ++x)
		{
			if (m_raster->Cell(x, y) != CELL_WALKABLE)
				continue;
			Vector c = m_raster->CellCenter(x, y);
			float groundZ = c.z;
			GroundHeight(Vector(c.x, c.y, c.z + 64.0f), groundZ, NULL,
			             128.0f);
			Node *n = m_network->CreateNode(
				Vector(c.x, c.y, groundZ + 1.0f));
			if (n)
				cellNode[y * w + x] = n->Id();
		}
	}

	// Link 8-connected neighbours when the standing hull can move between
	// them and the height delta is climbable.
	for (int y = 0; y < h; ++y)
	{
		for (int x = 0; x < w; ++x)
		{
			int id = cellNode[y * w + x];
			if (id < 0)
				continue;
			Node *from = m_network->GetNode(id);
			for (int dy = -1; dy <= 1; ++dy)
			{
				for (int dx = -1; dx <= 1; ++dx)
				{
					if (dx == 0 && dy == 0)
						continue;
					int nx = x + dx, ny = y + dy;
					if (nx < 0 || ny < 0 || nx >= w || ny >= h)
						continue;
					int nid = cellNode[ny * w + nx];
					if (nid < 0 || nid <= id)
						continue;
					Node *to = m_network->GetNode(nid);
					float dz = fabs(to->Origin().z - from->Origin().z);
					if (dz > m_jumpHeight)
						continue;
					Vector mid((from->Origin().x + to->Origin().x) * 0.5f,
					           (from->Origin().y + to->Origin().y) * 0.5f,
					           (from->Origin().z > to->Origin().z
					                ? from->Origin().z
					                : to->Origin().z) + 2.0f);
					if (!CanStandMove(from->Origin() + Vector(0, 0, 2.0f),
					                  to->Origin() + Vector(0, 0, 2.0f)))
						continue;
					m_network->CreateArc(from, to);
				}
			}
		}
	}

	delete[] cellNode;
}

// ---------------------------------------------------------------------------
// Flood fill connected components into rooms and build gateways between
// rooms that are physically adjacent but not directly linked.
// ---------------------------------------------------------------------------

void Dedale::BuildRooms()
{
	int n = m_network->NodeCount();
	for (int i = 0; i < n; ++i)
		m_network->GetNode(i)->SetRoomId(-1);

	for (int start = 0; start < n; ++start)
	{
		Node *seedNode = m_network->GetNode(start);
		if (seedNode->RoomId() >= 0)
			continue;
		Room *room = m_network->CreateRoom();
		if (!room)
			break;
		TStack<int> stack;
		stack.Push(seedNode->Id());
		while (!stack.IsEmpty())
		{
			int cur = stack.Pop();
			Node *node = m_network->GetNode(cur);
			if (!node || node->RoomId() == room->Id())
				continue;
			node->SetRoomId(room->Id());
			room->AddNode(cur);
			room->AddBounds(node->Origin());
			for (int a = 0; a < node->ArcCount(); ++a)
			{
				int nid = node->NeighborId(a);
				Node *nb = m_network->GetNode(nid);
				if (nb && nb->RoomId() < 0)
					stack.Push(nid);
			}
		}
	}
}

void Dedale::BuildGateways()
{
	int n = m_network->NodeCount();
	for (int i = 0; i < n; ++i)
	{
		Node *a = m_network->GetNode(i);
		for (int j = i + 1; j < n; ++j)
		{
			Node *b = m_network->GetNode(j);
			if (a->RoomId() == b->RoomId() || a->RoomId() < 0 ||
			    b->RoomId() < 0)
				continue;
			if ((a->Origin() - b->Origin()).LengthSqr() >
			    m_nodeSpacing * m_nodeSpacing * 2.0f)
				continue;
			// A gateway exists where two rooms' nodes face each other closely.
			m_network->CreateGateway(a->RoomId(), b->RoomId(), a->Id(),
			                         b->Id());
		}
	}
}

void Dedale::OptimizeRooms()
{
	// The original merged degenerate rooms ("weird room, ignored").  Remove
	// rooms that contain too few nodes to be useful.
	for (int r = 0; r < m_network->RoomCount(); ++r)
	{
		Room *room = m_network->GetRoom(r);
		if (room && room->NodeCount() == 1 &&
		    room->GetBounds().Superficy() < 1.0f)
		{
			HRC_DEBUG("Ignoring degenerate room %i", r);
		}
	}
}

bool Dedale::AnalyseMap(const Vector &seed)
{
	if (!EngineReady())
	{
		HRC_WARN("Engine not ready, cannot analyse the map");
		return false;
	}
	HRC_MSG("Analysing map around %0.0f,%0.0f ...", seed.x, seed.y);
	m_network->Clear();
	Rasterize(seed);
	SpreadNetwork();
	BuildRooms();
	BuildGateways();
	OptimizeRooms();
	HRC_MSG("Map analysis: %i nodes, %i arcs, %i rooms, %i gateways",
	        m_network->NodeCount(), m_network->ArcCount(),
	        m_network->RoomCount(), m_network->GatewayCount());
	return m_network->NodeCount() > 0;
}

// ---------------------------------------------------------------------------
// Persistence
// ---------------------------------------------------------------------------

bool Dedale::Serialize(Stream &s)
{
	// Self-describing body header; Deserialize() reads these four counts
	// before the payload.  They mirror the values in the container header.
	if (!s.WriteInt(m_network->NodeCount()) ||
	    !s.WriteInt(m_network->ArcCount()) ||
	    !s.WriteInt(m_network->RoomCount()) ||
	    !s.WriteInt(m_network->GatewayCount()))
		return false;
	for (int i = 0; i < m_network->NodeCount(); ++i)
	{
		Node *n = m_network->GetNode(i);
		if (!s.WriteVector(n->Origin()))
			return false;
	}
	for (int i = 0; i < m_network->ArcCount(); ++i)
	{
		Arc *a = m_network->GetArc(i);
		if (!s.WriteInt(a->From()->Id()) || !s.WriteInt(a->To()->Id()))
			return false;
	}
	for (int i = 0; i < m_network->RoomCount(); ++i)
	{
		Room *r = m_network->GetRoom(i);
		if (!s.WriteInt(r->NodeCount()))
			return false;
		for (int j = 0; j < r->NodeCount(); ++j)
			if (!s.WriteInt(r->GetNode(j)))
				return false;
	}
	for (int i = 0; i < m_network->GatewayCount(); ++i)
	{
		Gateway *g = m_network->GetGateway(i);
		if (!s.WriteInt(g->RoomA()) || !s.WriteInt(g->RoomB()) ||
		    !s.WriteInt(g->NodeA()) || !s.WriteInt(g->NodeB()))
			return false;
	}
	return s.Good();
}

bool Dedale::Deserialize(Stream &s)
{
	m_network->Clear();

	int nodeCount = s.ReadInt();
	int arcCount = s.ReadInt();
	int roomCount = s.ReadInt();
	int gatewayCount = s.ReadInt();
	if (!s.Good() || nodeCount < 0 || nodeCount > HRC_MAX_NAV_NODES ||
	    arcCount < 0 || arcCount > HRC_MAX_NAV_ARCS ||
	    roomCount < 0 || roomCount > HRC_MAX_NAV_ROOMS ||
	    gatewayCount < 0 || gatewayCount > HRC_MAX_NAV_GATEWAYS)
		return false;

	for (int i = 0; i < nodeCount; ++i)
		m_network->CreateNode(s.ReadVector());
	for (int i = 0; i < arcCount; ++i)
	{
		int a = s.ReadInt();
		int b = s.ReadInt();
		Node *na = m_network->GetNode(a);
		Node *nb = m_network->GetNode(b);
		if (na && nb)
			m_network->CreateArc(na, nb);
	}
	for (int i = 0; i < roomCount; ++i)
	{
		Room *room = m_network->CreateRoom();
		int count = s.ReadInt();
		for (int j = 0; j < count; ++j)
		{
			int nid = s.ReadInt();
			Node *n = m_network->GetNode(nid);
			if (n)
			{
				room->AddNode(nid);
				n->SetRoomId(room->Id());
				room->AddBounds(n->Origin());
			}
		}
	}
	for (int i = 0; i < gatewayCount; ++i)
	{
		int ra = s.ReadInt(), rb = s.ReadInt();
		int na = s.ReadInt(), nb = s.ReadInt();
		m_network->CreateGateway(ra, rb, na, nb);
	}
	return s.Good();
}

static bool WriteHeader(Stream &s, const char *mapName, int crc,
                        int nodes, int arcs, int rooms, int gateways)
{
	HrcContainerHeader hdr;
	memset(&hdr, 0, sizeof(hdr));
	memcpy(hdr.magic, "HRCBOT2", 7);
	snprintf(hdr.banner, sizeof(hdr.banner),
	         "Hurricane Bot ver %s", HRCBOT_LEGACY_VERSION);
	hdr.formatVersion = HRC_STREAM_VERSION;
	hdr.mapCrc = crc;
	snprintf(hdr.mapName, sizeof(hdr.mapName), "%s", mapName);
	hdr.nodeCount = nodes;
	hdr.arcCount = arcs;
	hdr.roomCount = rooms;
	hdr.gatewayCount = gateways;
	return s.WriteRaw(&hdr, sizeof(hdr));
}

static bool ReadHeader(Stream &s, HrcContainerHeader *hdr)
{
	return s.ReadRaw(hdr, sizeof(*hdr)) &&
	       memcmp(hdr->magic, "HRCBOT2", 7) == 0;
}

bool Dedale::SaveMap(const char *mapName, int mapCrc)
{
	char path[256];
	ContainerPath(path, sizeof(path), mapName, mapCrc, true);
	Stream s;
	if (!s.OpenForWrite(path))
	{
		HRC_WARN("Could not open %s for writing", path);
		return false;
	}
	if (!WriteHeader(s, mapName, mapCrc, m_network->NodeCount(),
	                 m_network->ArcCount(), m_network->RoomCount(),
	                 m_network->GatewayCount()) || !Serialize(s))
	{
		HRC_WARN("Failed writing %s", path);
		return false;
	}
	HRC_MSG("Saved navigation container %s", path);
	return true;
}

bool Dedale::LoadMap(const char *mapName, int mapCrc)
{
	char path[256];
	ContainerPath(path, sizeof(path), mapName, mapCrc, true);
	Stream s;
	if (!s.OpenForRead(path))
	{
		// Check for a legacy 1.3.4 container: it cannot be read by the new
		// format, so signal the caller to re-analyse the map.
		ContainerPath(path, sizeof(path), mapName, mapCrc, false);
		if (Stream::IsLegacyContainer(path))
		{
			HRC_MSG("Legacy .hrcbot container found for %s; re-analysing "
			        "(old bit format is not readable by the modern build)",
			        mapName);
			return false;
		}
		return false;
	}
	HrcContainerHeader hdr;
	if (!ReadHeader(s, &hdr))
	{
		HRC_WARN("Bad container header for %s", mapName);
		return false;
	}
	if (!Deserialize(s))
	{
		HRC_WARN("Corrupt container for %s", mapName);
		return false;
	}
	HRC_MSG("Loaded navigation for %s: %i nodes, %i arcs, %i rooms",
	        mapName, m_network->NodeCount(), m_network->ArcCount(),
	        m_network->RoomCount());
	return true;
}

} // namespace hrc
