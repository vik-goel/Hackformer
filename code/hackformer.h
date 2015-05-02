/*TODO:

Clean Up
---------
- Free the tile texture atlas when loading a new level
- clean up text entity, texture memory

- Better loading in of entities from the tmx file

- re-enable laser bolt player, collision


Bug Fixes
----------
- Fix flickering bug (need proper subpixel accuracy when blitting sprites)
- Fix entity position on bigger screen sizes (when loading in from .tmx)
- when patrolling change all of the hitboxes, not just the first one
- test remove when outside level to see that it works
- if the entity which you are keyboard controlling is removed, the level should restart

- If the text becomes keyboard controlled when inside an entity, ignore collisions with it until it gets outside


New Features
-------------
- locking fields so they can't be moved
- locking fields so they can't be modified

- Fade console fields in and out

- Multiline text

- use a priority queue to process path requests

- Trail effect on death

- Cloning fields
- Cloning entire entities 

- When an entity dies, make its fields collectable in the world so they don't just disappear

- Handle shadows properly

- Hacking gravity
- Hack the topology of the world
- Hack to make player reflect bullets
- Hack the mass of entities

- Make the background hackable (change from sunset to marine)
- Hack the text and type in new messages

- Make entities resizable

*/

#define arrayCount(array) sizeof(array) / sizeof(array[0])
#define InvalidCodePath assert(!"Invalid Code Path");
#define InvalidDefaultCase default: { assert(!"Invalid Default Case"); } break

#define SHOW_COLLISION_BOUNDS 0
#define SHOW_CLICK_BOUNDS 0

#define uint unsigned int

#include <cstdlib>
#include <cstring>
#include <cmath>
#include <cstdio>
#include <cassert>
#include <string>

#include "glew.h"

#include "SDL.h"
#include "SDL_image.h"
#include "SDL_ttf.h"
#include "SDL_opengl.h"

#include "hackformer_math.h"
#include "hackformer_renderer.h"
#include "hackformer_consoleField.h"
#include "hackformer_entity.h"

struct Input {
	V2 mouseInPixels;
	V2 mouseInMeters;
	V2 mouseInWorld;
	V2 dMouseMeters;
	bool upPressed, leftPressed, rightPressed;
	bool upJustPressed;
	bool leftMousePressed, leftMouseJustPressed;
	bool rPressed, rJustPressed;
	bool xPressed, xJustPressed;
};

struct MemoryArena {
	void* base;
	uint allocated;
	uint size;
};

struct PathNode {
	bool solid;
	bool open;
	bool closed;
	PathNode* parent;
	double costToHere;
	double costToGoal;
	V2 p;
	int tileX, tileY;
};

struct EntityChunk {
	int entityRefs[16];
	int numRefs;
	EntityChunk* next;
};

struct GameState {
	Entity entities[1000];
	int numEntities;

	//NOTE: 0 is the null reference
	EntityReference entityRefs_[500];
	

	//NOTE: These must be sequential for laser collisions to work
	int refCount;

	MemoryArena permanentStorage;
	MemoryArena levelStorage;

	RenderGroup* renderGroup;
	SDL_Renderer* renderer;
	TTF_Font* textFont;
	CachedFont consoleFont;

	Input input;
	V2 cameraP;
	V2 newCameraP;

	int shootTargetRef;
	int consoleEntityRef;
	int playerRef;
	int playerDeathRef;

	bool loadNextLevel;
	bool doingInitialSim;

	ConsoleField* consoleFreeList;
	RefNode* refNodeFreeList;
	EntityReference* entityRefFreeList;
	Hitbox* hitboxFreeList;

	ConsoleField* swapField;
	V2 swapFieldP;

	PathNode* openPathNodes[10000];
	int openPathNodesCount;
	PathNode* solidGrid;
	int solidGridWidth, solidGridHeight;
	double solidGridSquareSize;

	EntityChunk* chunks;
	int chunksWidth, chunksHeight;
	V2 chunkSize;

	int blueEnergy;

	double shootDelay;
	double tileSize;
	V2 mapSize;
	V2 worldSize;

	double pixelsPerMeter;
	int windowWidth, windowHeight;
	V2 windowSize;

	V2 gravity;
	V2 texel;

	V2 playerStartP;
	V2 playerDeathStartP;
	V2 playerDeathSize;

	AnimNode playerStand;
	AnimNode playerWalk;
	AnimNode playerHack;
	AnimNode playerJump;

	AnimNode virus1Stand;
	AnimNode virus1Shoot;

	AnimNode flyingVirusStand;
	AnimNode flyingVirusShoot;

	Texture bgTex, mgTex;
	Texture sunsetCityBg, sunsetCityMg;
	Texture marineCityBg, marineCityMg;

	Texture blueEnergyTex;
	Texture laserBolt;
	Texture endPortal;

	Texture consoleTriangle, consoleTriangleSelected, consoleTriangleGrey, consoleTriangleYellow;

	Texture laserBaseOff, laserBaseOn;
	Texture laserTopOff, laserTopOn;
	Texture laserBeam;

	Texture heavyTileTex;
	Texture* tileAtlas;

	Texture dock;
	Texture dockBlueEnergyTile;
	Texture attribute, behaviour;
};

#define pushArray(arena, type, count) (type*)pushIntoArena_(arena, count * sizeof(type))
#define pushStruct(arena, type) (type*)pushIntoArena_(arena, sizeof(type))
#define pushSize(arena, size) pushIntoArena_(arena, size);
void* pushIntoArena_(MemoryArena* arena, uint amt) {
	arena->allocated += amt;
	assert(arena->allocated < arena->size);

	void* result = (char*)arena->base + arena->allocated - amt;
	return result;
}

//TODO: size_t?
void zeroSize(void* base, int size) {
	for (int byteIndex = 0; byteIndex < size; byteIndex++) {
		*((char*)base + byteIndex) = 0;
	}
}
