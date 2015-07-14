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

struct Entity {
	EntityType type;
	DrawOrder drawOrder;
	R2 bounds;
	Texture* tex;
	Waypoint* waypoints;
	bool32 flipX;
	bool32 flipY;
	Messages* messages;
};

enum CursorMode {
	CursorMode_stampTile,
	CursorMode_stampEntity,
	CursorMode_moveEntity,
	CursorMode_editWaypoints,
};

struct EditorState {
	MemoryArena arena;
	Input input;
	Camera camera;
	Assets assets;
	RenderGroup* renderGroup;
	TTF_Font* textFont;

	Entity* entities;
	s32 entityCount;

	BackgroundTextures backgroundTextures;

	Texture* entityTextureAtlas;
	Texture* tileAtlas;
	s32 tileAtlasCount;
	Texture* textures;
	s32 texturesCount;

	s32 mapWidthInTiles;
	s32 mapHeightInTiles;
	s32 initialEnergy;

	TileSpec* tiles;

	HackAbilities hackAbilities;
};