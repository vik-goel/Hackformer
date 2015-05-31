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

- Heavy tiles should be able to smash entities into the ceiling

New Features
-------------
- locking fields so they can't be moved
- locking fields so they can't be modified

- Fade console fields in and out
- Multiline text
- use a priority queue to process path requests
- Trail effect on death
- Handle shadows properly

- make laser beams use the killsOnHit field

OTHER HACKS
- Cloning entire entities 
- Hack the topology of the world
- Hack to make player reflect bullets
- Hack the mass of entities
- Make the background hackable (change from sunset to marine)
- Hack the text and type in new messages
- Make entities resizable

STEALTH
- Line of sight for enemies
- Distracting enemies
- Patrols and seeks_target fields have alert and not alert states
- Enemies react to noises that they hear
- Enemies can alert other enemies

Waypoint following
	-Add waypoints
	-Delete waypoints
	-Move around waypoints
	-Clean up moving to first waypoint if not currently on the path

*/

#define _CRT_SECURE_NO_WARNINGS

#define arrayCount(array) sizeof(array) / sizeof(array[0])
#define InvalidCodePath assert(!"Invalid Code Path")
#define InvalidDefaultCase default: { assert(!"Invalid Default Case"); } break

#define SHOW_COLLISION_BOUNDS 0
#define SHOW_CLICK_BOUNDS 0
#define PRINT_FPS 0

#include <stdint.h>

typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t u8;

typedef int32_t s32;
typedef int16_t s16;
typedef int8_t s8;

typedef int32_t bool32; 
typedef int16_t bool16; 
typedef int8_t bool8; 

#include <cstdlib>
#include <cstring>
#include <cmath>
#include <cstdio>
#include <cassert>

#include "glew.h"

#include "SDL.h"
#include "SDL_image.h"
#include "SDL_ttf.h"
#include "SDL_opengl.h"
#include "SDL_mixer.h"

struct GameState;
struct Input;
struct Camera;

#include "hackformer_math.h"
#include "hackformer_renderer.h"
#include "hackformer_consoleField.h"
#include "hackformer_entity.h"

struct Key {
	bool32 pressed;
	bool32 justPressed;
	s32 keyCode1, keyCode2;
};

struct Input {
	V2 mouseInPixels;
	V2 mouseInMeters;
	V2 mouseInWorld;

	//TODO: There might be a bug when this, it was noticeable when shift was held down 
	//	 	while dragging something with the mouse
	V2 dMouseMeters;

	union {
		//NOTE: The number of keys in the array must always be equal to the number of keys in the struct below
		Key keys[9];

		//TODO: The members of this struct may not be packed such that they align perfectly with the array of keys
		struct {
			Key up;
			Key down;
			Key left;
			Key right;
			Key r;
			Key x;
			Key shift;
			Key pause;
			Key leftMouse;
		};
	};
};

struct MemoryArena {
	void* base;
	size_t allocated;
	size_t size;
};

struct Camera {
	V2 p;
	V2 newP;
	bool32 moveToTarget;
	bool32 deferredMoveToTarget;
	double scale;
};

struct PathNode {
	bool32 solid;
	bool32 open;
	bool32 closed;
	PathNode* parent;
	double costToHere;
	double costToGoal;
	V2 p;
	s32 tileX, tileY;
};

struct EntityChunk {
	s32 entityRefs[16];
	s32 numRefs;
	EntityChunk* next;
};

struct Button {
	TextureData texture;
	R2 bounds;
	bool32 selected;
	double scale;
};

struct PauseMenu {
	TextureData background;
	Animation backgroundAnim;
	double animCounter;

	Button quit;
	Button restart;
	Button resume;
	Button settings;
};

struct GameState {
	s32 numEntities;
	Entity entities[1000];

	//NOTE: 0 is the null reference
	EntityReference entityRefs_[500];
	
	//NOTE: These must be sequential for laser collisions to work
	s32 refCount;

	MemoryArena permanentStorage;
	MemoryArena levelStorage;

	RenderGroup* renderGroup;
	TTF_Font* textFont;

	Input input;
	Camera camera;

	RefNode* targetRefs;
	s32 consoleEntityRef;
	s32 playerRef;
	s32 playerDeathRef;

	bool32 loadNextLevel;
	bool32 doingInitialSim;

	ConsoleField* consoleFreeList;
	RefNode* refNodeFreeList;
	EntityReference* entityRefFreeList;
	Hitbox* hitboxFreeList;
	Messages* messagesFreeList;

	ConsoleField* timeField;
	ConsoleField* gravityField;
	ConsoleField* swapField;
	V2 swapFieldP;
	FieldSpec fieldSpec;

	PathNode* openPathNodes[10000];
	s32 openPathNodesCount;
	PathNode* solidGrid;
	s32 solidGridWidth, solidGridHeight;
	double solidGridSquareSize;

	EntityChunk* chunks;
	s32 chunksWidth, chunksHeight;
	V2 chunkSize;

	double shootDelay;
	double tileSize;
	V2 mapSize;
	V2 worldSize;

	double pixelsPerMeter;
	s32 windowWidth, windowHeight;
	V2 windowSize;

	V2 gravity;
	V2 texel;

	V2 playerStartP;
	V2 playerDeathStartP;
	V2 playerDeathSize;

	AnimNode playerHack;
	CharacterAnim playerAnim;
	CharacterAnim playerDeathAnim;
	CharacterAnim virus1Anim;
	CharacterAnim flyingVirusAnim;

	TextureData bgTex, mgTex;
	TextureData sunsetCityBg, sunsetCityMg;
	TextureData marineCityBg, marineCityMg;

	Texture blueEnergyTex;
	Texture laserBolt;
	Texture endPortal;

	Texture laserBaseOff, laserBaseOn;
	Texture laserTopOff, laserTopOn;
	Texture laserBeam;

	Texture heavyTileTex;
	Texture* tileAtlas;

	TextureData dock;
	TextureData dockBlueEnergyTile;

	s32 textureDataCount;
	TextureData textureData[200];

	s32 animDataCount;
	AnimNodeData animData[20];

	s32 characterAnimDataCount;
	CharacterAnimData characterAnimData[10];

	PauseMenu pauseMenu;
};

#define pushArray(arena, type, count) (type*)pushIntoArena_(arena, count * sizeof(type))
#define pushStruct(arena, type) (type*)pushIntoArena_(arena, sizeof(type))
#define pushSize(arena, size) pushIntoArena_(arena, size);
void* pushIntoArena_(MemoryArena* arena, size_t amt) {
	arena->allocated += amt;
	assert(arena->allocated < arena->size);

	void* result = (char*)arena->base + arena->allocated - amt;
	return result;
}

void zeroSize(void* base, size_t size) {
	for (size_t byteIndex = 0; byteIndex < size; byteIndex++) {
		*((char*)base + byteIndex) = 0;
	}
}

void setCameraTarget(Camera* camera, V2 target) {
	camera->newP = target;
	camera->moveToTarget = true;
}

void saveGame(GameState* gameState, char* fileName);
void loadGame(GameState* gameState, char* fileName);