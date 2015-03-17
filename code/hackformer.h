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
};

struct MemoryArena {
	char* base;
	uint allocated;
	uint size;
};

struct GameState {
	Entity entities[1000];
	int numEntities;

	//NOTE: 0 is the null reference
	EntityReference entityRefs_[1000];
	EntityReference* entityRefFreeList;
	int refCount;

	MemoryArena permanentStorage;
	SDL_Renderer* renderer;
	TTF_Font* textFont;
	TTF_Font* consoleFont;

	Input input;
	V2 cameraP;

	int shootTargetRef;
	int numConsoleEntities;

	Texture playerStand, playerJump;
	Animation playerWalk;

	Texture virus1Stand;
	Animation virus1Shoot;

	Texture sunsetCityBg, sunsetCityMg;
	Texture blueEnergy;
	Texture laserBolt;

	Texture consoleTriangle, consoleTriangleSelected;

	ConsoleField playerSpeedField;
	ConsoleField playerJumpHeightField;
	ConsoleField keyboardControlledField;
	ConsoleField movesBackAndForthField;
	ConsoleField shootsAtTargetField;
	ConsoleField isShootTargetField;
	ConsoleField tileMoveableField;
	
	float shootDelay;
	float tileSize;
	V2 mapSize;

	float pixelsPerMeter;
	int windowWidth, windowHeight;

	V2 gravity;
};

#define pushArray(arena, type, count) pushIntoArena_(arena, count * sizeof(type))
#define pushStruct(arena, type) pushIntoArena_(arena, sizeof(type))

char* pushIntoArena_(MemoryArena* arena, uint amt) {
	arena->allocated += amt;
	assert(arena->allocated < arena->size);

	char* result = arena->base + arena->allocated - amt;
	return result;
}
