#define SAVE_BINARY
#include "hackformer_types.h"

#include "hackformer_renderer.h"

struct FieldSpec {};

struct EntitySpec {
	EntityType type;
	DrawOrder drawOrder;
	V2 size;
	char* fileName;
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
};

enum CursorMode {
	CursorMode_stampTile,
	CursorMode_stampEntity,
	CursorMode_moveEntity,
	CursorMode_editWaypoints,
};