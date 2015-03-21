/*TODO:

- Make console fields much smoother (moving around the fields, fading them in and out)
- Handle overlaps between entities when selecting which one to hack better
- Handle overlaps between entities (and their fields) with the swap field
- Make camera change much smoother

- Allow players to reset the level

- Player death animation

- Better loading in of entities from the tmx file
- Multiple strings for one text (hack)
- Multiline text

- Use 1 triangle image and rotate it in the console, 4 images are unecessary
- Show single triangle indicating direction of tile movement

- Make energy necessary for tweaking values
- Collision with left and right edges of the map

- Fix tile pushing another tile to the side bugs

- keyboard controlled flying enemies, not jumping -- flying up and down 
- allow player to click the laser beam to hack the laser controller

- more console fields
- make radius console fields do things

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
};

struct MemoryArena {
	char* base;
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

struct GameState {
	Entity entities[1000];
	int numEntities;

	//NOTE: 0 is the null reference
	EntityReference entityRefs_[300];
	EntityReference* entityRefFreeList;
	int refCount;

	MemoryArena permanentStorage;
	SDL_Renderer* renderer;
	TTF_Font* textFont;
	TTF_Font* consoleFont;

	Input input;
	V2 cameraP;

	int shootTargetRef;
	int consoleEntityRef;
	int playerRef;

	bool loadNextLevel;

	Texture playerStand, playerJump;
	Animation playerWalk;

	Texture virus1Stand;
	Animation virus1Shoot;

	Texture bgTex, mgTex;
	Texture sunsetCityBg, sunsetCityMg;
	Texture marineCityBg, marineCityMg;

	Texture blueEnergy;
	Texture laserBolt;
	Texture endPortal;

	Texture consoleTriangle, consoleTriangleSelected;
	Texture consoleTriangleDown, consoleTriangleDownSelected;
	Texture consoleTriangleUp, consoleTriangleUpSelected;

	Texture laserBaseOff, laserBaseOn;
	Texture laserTopOff, laserTopOn;
	Texture laserBeam;

	Texture flyingVirus;
	Animation flyingVirusShoot;

	ConsoleField keyboardSpeedField;
	ConsoleField keyboardJumpHeightField;
	ConsoleField keyboardControlledField;
	ConsoleField keyboardDoubleJumpField;

	ConsoleField movesBackAndForthField;
	ConsoleField patrolSpeedField;

	ConsoleField seeksTargetField;
	ConsoleField seekTargetSpeedField;
	ConsoleField seekTargetRadiusField;

	ConsoleField shootsAtTargetField;
	ConsoleField shootRadiusField;
	ConsoleField bulletSpeedField;
	ConsoleField isShootTargetField;

	ConsoleField tileXOffsetField;
	ConsoleField tileYOffsetField;

	ConsoleField* consoleFreeList;
	ConsoleField* swapField;
	V2 swapFieldP;

	RefNode* refNodeFreeList;

	PathNode* openPathNodes[10000];
	int openPathNodesCount;
	PathNode** solidGrid;
	int solidGridWidth, solidGridHeight;
	double solidGridSquareSize;

	double shootDelay;
	double tileSize;
	V2 mapSize;
	V2 worldSize;

	double pixelsPerMeter;
	int windowWidth, windowHeight;
	V2 windowSize;

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
