#include "hackformer_types.h"

#include "hackformer_renderer.h"

struct FieldSpec {};

struct EntitySpec {
	EntityType type;
	DrawOrder drawOrder;
	V2 size;
	char* fileName;
};

struct Entity {
	EntityType type;
	DrawOrder drawOrder;
	R2 bounds;
	TextureData* tex;
};

enum CursorMode {
	CursorMode_stampTile,
	CursorMode_stampEntity,
	CursorMode_moveEntity,
};