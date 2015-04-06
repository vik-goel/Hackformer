/*TODO:

Clean Up
---------
- Free the tile texture atlas when loading a new level
- clean up text entity, texture memory
- clean up move tiles memory

- Better loading in of entities from the tmx file

- re-enable laser bolt player, collision


Bug Fixes
----------
- Fix flickering bug (need proper subpixel accuracy when blitting sprites)
- Fix entity position on bigger screen sizes (when loading in from .tmx)
- when patrolling change all of the hitboxes, not just the first one
- test remove when outside level to see that it works
- when keyboard controlling a laser entity, the base will just fall off without the beam or top
- bullets shot from a laser base should not collide with its corresponding beam and top
- tile render order (higher y coordinate tiles must be renderered later)



New Features
-------------
- locking fields so they can't be moved
- locking fields so they can't be modified
- more console fields

- Fade console fields in and out
- Handle overlaps between entities (and their fields) with the swap field

- Multiline text

- use a priority queue to process path requests

- Trail effect on death

*/

#define arrayCount(array) sizeof(array) / sizeof(array[0])
#define SHOW_COLLISION_BOUNDS 0

#define uint unsigned int

#include <cstdlib>
#include <cstring>
#include <cmath>
#include <cstdio>
#include <cassert>

#include "SDL.h"
#include "SDL_image.h"
#include "SDL_ttf.h"

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

	V2 playerStartP;
	V2 playerDeathStartP;
	V2 playerDeathSize;

	Texture playerStand, playerJump;
	Animation playerWalk, playerStandWalkTransition;
	Animation playerHackingAnimation;

	Texture virus1Stand;
	Animation virus1Shoot;

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

	Texture flyingVirus;
	Animation flyingVirusShoot;

	Texture heavyTileTex;
	Texture* tileAtlas;
};

#define pushArray(arena, type, count) (type*)pushIntoArena_(arena, count * sizeof(type))
#define pushStruct(arena, type) (type*)pushIntoArena_(arena, sizeof(type))

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
