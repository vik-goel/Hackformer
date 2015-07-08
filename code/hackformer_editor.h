#define SAVE_BINARY
#include "hackformer_types.h"

#include "hackformer_renderer.h"

struct FieldSpec {};

struct EntitySpec {
	EntityType type;
	DrawOrder drawOrder;
	V2 size;
	char* fileName;
	V2 panelP;
	double panelScale;
};

struct TileSpec {
	s32 index;
	bool32 flipX;
	bool32 flipY;
	bool32 tall;
};

struct Waypoint {
	V2 p;
	Waypoint* next;
};

struct Entity {
	EntityType type;
	DrawOrder drawOrder;
	R2 bounds;
	Texture* tex;
	Waypoint* waypoints;
	bool32 flipX;
	bool32 flipY;
};

enum CursorMode {
	CursorMode_stampTile,
	CursorMode_stampEntity,
	CursorMode_moveEntity,
	CursorMode_editWaypoints,
};