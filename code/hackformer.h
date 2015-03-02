#ifndef HACKFORMER_H
#define HACKFORMER_H

#define uint unsigned int
#define uint16 unsigned short

#define ARRAY_COUNT(array) sizeof(array) / sizeof(array[0])

#include "hackformer_math.h"
#include "hackformer_renderer.h"
#include "hackformer_entity.h"

#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 720

#define PIXELS_PER_METER 70.0f

#define SHOW_COLLISION_BOUNDS 0

struct Input {
	V2 mouseInPixels;
	V2 mouseInMeters;
	V2 mouseInWorld;
	bool upPressed, leftPressed, rightPressed;
	bool upJustPressed;
	bool leftMousePressed, leftMouseJustPressed;
};

struct MemoryArena {
	char* base;
	uint allocated;
	uint size;
};

struct DrawCall {
	DrawOrder drawOrder;
	Texture* texture;
	R2 bounds;
	bool flipX;

#if SHOW_COLLISION_BOUNDS
	V2** collisionPoints;
	int numCollisionPoints;
#endif
};

struct GameState {
	Entity entities[1000];
	int numEntities;

	MemoryArena permanentStorage;
	SDL_Renderer* renderer;

	Input input;
	V2 cameraP;

	Entity* consoleEntity;
	TTF_Font* font;

	Texture playerStand, playerJump;
	Animation playerWalk;
	Texture blueEnergy;
	Texture background;
	DrawCall drawCalls[5000];

	float tileSize;
	float mapWidth, mapHeight;

	V2 polygonSum[1024];
};

#define pushArray(arena, type, count) pushIntoArena_(arena, count * sizeof(type))
#define pushStruct(arena, type) pushIntoArena_(arena, sizeof(type))

char* pushIntoArena_(MemoryArena* arena, uint amt) {
	arena->allocated += amt;
	assert(arena->allocated < arena->size);

	char* result = arena->base + arena->allocated - amt;
	return result;
}

#endif