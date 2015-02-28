#include "SDL.h"
#include "SDL_image.h"
#include <stdio.h>
#include <cassert>
#include "hackformer_math.h"
#include <cstdlib>
#include <cstring>

#define uint unsigned int
#define uint16 unsigned short

#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 720

#define PIXELS_PER_METER 60.0

#define ARRAY_COUNT(array) sizeof(array) / sizeof(array[0])

struct Texture {
	SDL_Texture* tex;
	SDL_Rect srcRect;
};

enum EntityType {
	EntityType_Player,
	EntityType_Tile,
	EntityType_Background,
};

enum EntityFlag {
	EntityFlag_moveable = 1 << 0,
	EntityFlag_collidable = 1 << 1,
	EntityFlag_facesLeft = 1 << 2,
};

struct Entity {
	EntityType type;
	uint flags;
	V2 p;
	V2 dP;
	V2 size;
	Texture* texture;
};

struct MemoryArena {
	char* base;
	uint allocated;
	uint size;
};

#define pushArray(arena, type, count) pushIntoArena_(arena, count * sizeof(type))
#define pushStruct(arena, type) pushIntoArena_(arena, sizeof(type))

char* pushIntoArena_(MemoryArena* arena, uint amt) {
	arena->allocated += amt;
	assert(arena->allocated < arena->size);

	char* result = arena->base + arena->allocated - amt;
	return result;
}

//Pre-condition: IMG_Init has already been called
SDL_Texture* loadPNGTexture(SDL_Renderer* renderer, char* fileName) {
	SDL_Surface *image = IMG_Load(fileName);
	assert(image);
	SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, image);
	assert(texture);
	SDL_FreeSurface(image);
	SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
	return texture;
}

void createPNGTexture(Texture* texture, SDL_Renderer* renderer, char* fileName) {
	texture->tex = loadPNGTexture(renderer, fileName);
	SDL_QueryTexture(texture->tex, NULL, NULL, &texture->srcRect.w, &texture->srcRect.h);
}

void addFlags(Entity* entity, uint flags) {
	entity->flags |= flags;
} 

void clearFlags(Entity* entity, uint flags) {
	entity->flags &= ~flags;
} 

bool isSet(Entity* entity, uint flags) {
	bool result = (entity->flags & flags) != 0;
	return result;
}

char* loadDelmittedStringIntoBuffer(char* src, char* dest, int* length) {
	for (char* c = src; *c != ','; c++) {
		dest[(*length)++] = c[0];
	}

	dest[*(length)++] = 0;
	return dest;
}

void loadTiledMap(SDL_Renderer* renderer, MemoryArena* arena, Entity* entities, int* numEntities, char* fileName, float* mapWidth, float* mapHeight) {
	FILE* file;
	fopen_s(&file, fileName, "r");
	assert(file);

	int width = -1;
	int height = -1;
	bool encounteredTileData = false;

	Texture* textures = NULL;
	int y = 0;

	float tileSize = 1.0;

	char buffer[512];
    while (fgets (buffer, sizeof(buffer), file)) {
		if (width == -1) {
			if (strncmp(buffer, "width", 5) == 0) {
				width = atoi(buffer + 6);
			}
		}
		else if (height == -1) {
			if (strncmp(buffer, "height", 6) == 0) {
				height = atoi(buffer + 7);
				y = height;
			}
		}
		else if (textures == NULL) {
			if (strncmp(buffer, "tileset", 7) == 0) {
				char strBuf[50];
				int length = 0;
				int startPoint = 8;

				loadDelmittedStringIntoBuffer(buffer + startPoint, strBuf, &length);
				SDL_Texture* atlas = loadPNGTexture(renderer, strBuf);

				startPoint += length + 1;
				length = 0;
				int tileWidth = atoi(loadDelmittedStringIntoBuffer(buffer + startPoint, strBuf, &length));

				startPoint += length + 1;
				length = 0;
				int tileHeight = atoi(loadDelmittedStringIntoBuffer(buffer + startPoint, strBuf, &length));

				int atlasWidth, atlasHeight;
				SDL_QueryTexture(atlas, NULL, NULL, &atlasWidth, &atlasHeight);

				int tileCols = atlasWidth / tileWidth;
				int tileRows = atlasHeight / tileHeight;

				textures = (Texture*)pushArray(arena, Texture, tileCols * tileRows);

				for (int rowIndex = 0; rowIndex < tileRows; rowIndex++) {
					for (int colIndex = 0; colIndex < tileCols; colIndex++) {
						Texture* tile = textures + (rowIndex * tileCols) + colIndex;
						tile->tex = atlas;
						tile->srcRect.x = colIndex * tileWidth + 3 * (colIndex + 1);
						tile->srcRect.y = rowIndex * tileHeight - 3 * (colIndex + 1);
						tile->srcRect.w = tileWidth;
						tile->srcRect.h = tileHeight;
					}
				}

			}
		}
		else if (!encounteredTileData) {
			encounteredTileData = strncmp(buffer, "data=", 5) == 0;
		} else {
			char numBuffer[5];
			int numSize = 0;
			int x = 0;

			for (char* c = buffer; *c != '\n'; c++) {
				if (*c == ',') {
					numBuffer[numSize] = 0;
					int tileIndex = atoi(numBuffer) - 1;

					if (tileIndex >= 0) {
						Entity* tile = entities + (*numEntities)++;
						tile->p = v2((float)(x + 0.5) * tileSize, (float)(y - 0.5) * tileSize);
						tile->size = v2(tileSize, tileSize);
						tile->texture = textures + tileIndex;
						tile->type = EntityType_Tile;
						addFlags(tile, EntityFlag_collidable);
					}

					x++;
					numSize = 0;
				} else {
					numBuffer[numSize++] = *c;
				}
			}

			y--;
		}
   	}

	fclose(file);

	*mapWidth = width * tileSize;
	*mapHeight = height * tileSize;
}

SDL_Rect getPixelSpaceRect(R2 rect) {
	SDL_Rect result = {};

	float width = getRectWidth(rect);
	float height = getRectHeight(rect);

	result.w = (int)ceil(width * PIXELS_PER_METER);
	result.h = (int)ceil(height * PIXELS_PER_METER);
	result.x = (int)ceil(rect.min.x * PIXELS_PER_METER);
	result.y = (int)ceil(WINDOW_HEIGHT - rect.max.y * PIXELS_PER_METER);

	return result;
}

void drawTexture(SDL_Renderer* renderer, Texture* texture, V2 center, V2 size, bool flipX) {
	R2 rect = rectCenterDiameter(center, size);
	SDL_Rect dstRect = getPixelSpaceRect(rect);

	SDL_RendererFlip flip = flipX ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE;
	SDL_RenderCopyEx(renderer, texture->tex, &texture->srcRect, &dstRect, 0, NULL, flip);
}

void drawFilledRect(SDL_Renderer* renderer, R2 rect, V2 cameraP = v2(0, 0)) {
	R2 r = subtractFromRect(rect, cameraP);
	SDL_Rect dstRect = getPixelSpaceRect(r);
	SDL_SetRenderDrawColor(renderer, 255, 0, 255, 255);
	SDL_RenderFillRect(renderer, &dstRect);
}


bool getLineCollisionTime(float* collisionTime, float currentX, float currentY, float deltaX, float deltaY, float lineX, float lineMinY, float lineMaxY) {
	float time = 1;

	if (deltaX == 0) {
		if (currentX == lineX && currentY >= lineMinY && currentY <= lineMaxY) {
			time = 0;
		} else {
			time = -1;
		}
	} else {
		time = (lineX - currentX) / deltaX;
		float newY = currentY + time * deltaY;	

		if (newY < lineMinY || newY > lineMaxY) {
			time = -1;
		}
	}

	if (time >= 0 && *collisionTime > time) {
		*collisionTime = time;
		return true;
	} 

	return false;
}

int main(int argc, char *argv[]) {
	//TODO: Only initialize what is needed
	if (SDL_Init(SDL_INIT_EVERYTHING) < 0) {
		fprintf(stderr, "Failed to initialize SDL. Error: %s", SDL_GetError());
		return -1;
	}

	if (IMG_Init(IMG_INIT_PNG) != IMG_INIT_PNG) {
		fprintf(stderr, "Failed to initialize SDL Image.");
		return -1;
	}

	SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1); 
	SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);

	SDL_Window *window = SDL_CreateWindow("C++former", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_ALLOW_HIGHDPI);

	if (!window) {
		fprintf(stderr, "Failed to create window. Error: %s", SDL_GetError());
		return -1;
	}
	SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

	if (!renderer) {
		fprintf(stderr, "Failed to creat erenderer. Error: %s", SDL_GetError());
		return -1;
	}

	MemoryArena arena = {};
	arena.size = 1024 * 1024 * 1024;
	arena.base = (char*)calloc(arena.size, 1);

	Entity *entities = (Entity*)pushArray(&arena, Entity, 1000);
	int numEntities = 1; //NOTE: 0 is for the null entity

	bool running = true;

	double frameTime = 1.0 / 60.0;
	uint fpsCounterTimer = SDL_GetTicks();
	uint fps = 0;

	float dt = 0;

	uint lastTime = SDL_GetTicks();
	uint currentTime;

	bool leftPressed = false, rightPressed = false, upPressed = false;
	bool leftMouseButtonPressed = false;
	V2 mouse = {};

	Entity* background = entities + numEntities++;
	background->texture = (Texture*)pushStruct(&arena, Texture);
	createPNGTexture(background->texture, renderer, "res/SunsetCityBackground.png");
	background->type = EntityType_Background;

	Entity* player = entities + numEntities++;
	player->p = {2, 8};
	player->size = {2, 2}; 
	player->texture = (Texture*)pushStruct(&arena, Texture);
	createPNGTexture(player->texture, renderer, "res/player/stand.png");
	player->type = EntityType_Player;
	addFlags(player, EntityFlag_moveable|EntityFlag_collidable|EntityFlag_facesLeft);

	float mapWidth, mapHeight;
	loadTiledMap(renderer, &arena, entities, &numEntities, "maps/map1.txt", &mapWidth, &mapHeight);

	background->size = {mapWidth, mapHeight + 1};
	background->p = background->size / 2;

	V2 oldCameraP = {};
	V2 newCameraP = {};

	while (running) {
		bool leftMouseButtonJustPressed = false;
		bool upJustPressed = false;

		currentTime = SDL_GetTicks();
		dt += (float)((currentTime - lastTime) / 1000.0); 
		lastTime = currentTime;

		if (currentTime - fpsCounterTimer > 1000) {
			fpsCounterTimer += 1000;
			printf("Fps: %d\n", fps);
			fps = 0;
		}

		if (dt > frameTime) {
			fps++;

			SDL_Event event;
			while(SDL_PollEvent(&event) > 0) {
				switch(event.type) {
					case SDL_QUIT:
					running = false;
					break;
					case SDL_KEYDOWN:
					case SDL_KEYUP: {
						bool pressed = event.key.state == SDL_PRESSED;
						SDL_Keycode key = event.key.keysym.sym;

						if (key == SDLK_w || key == SDLK_UP) {
							if (pressed && !upPressed) upJustPressed = true;
							upPressed = pressed;
						}
						else if (key == SDLK_a || key == SDLK_LEFT) leftPressed = pressed;
						else if (key == SDLK_d || key == SDLK_RIGHT) rightPressed = pressed;
					} break;
					case SDL_MOUSEMOTION:
					mouse.x = (float)event.motion.x; //TODO: Convert this into world space
					mouse.y = (float)event.motion.y;
					break;
					case SDL_MOUSEBUTTONDOWN:
					case SDL_MOUSEBUTTONUP:
					if (event.button.button == SDL_BUTTON_LEFT) {
						if (event.button.state == SDL_PRESSED) leftMouseButtonPressed = leftMouseButtonJustPressed = true;
					}
				}
			}

			SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
			SDL_RenderClear(renderer);

			for (int entityIndex = 1; entityIndex < numEntities; entityIndex++) {
				Entity* entity = entities + entityIndex;

				V2 ddP = {0, (float)-9.8};

				switch(entity->type) {
					case EntityType_Player: {
						float xMove = 0;
						float xMoveAcceleration = 12;

						if (rightPressed) xMove += xMoveAcceleration;
						if (leftPressed) xMove -= xMoveAcceleration;
						if (upJustPressed) entity->dP.y = 6.5;

						ddP.x = xMove;

						if (xMove < 0) clearFlags(entity, EntityFlag_facesLeft);
						else if (xMove > 0) addFlags(entity, EntityFlag_facesLeft);

						entity->dP.x *= (float)pow(E, -3.0 * dt);

						float windowWidth = (float)(WINDOW_WIDTH / PIXELS_PER_METER);
						float maxCameraX = mapWidth - windowWidth;
						newCameraP.x = clamp((float)(entity->p.x - windowWidth / 2.0), 0, maxCameraX);
					} break;

					case EntityType_Tile: {

					} break;
					case EntityType_Background: {

					} break;
				}

				if (isSet(entity, EntityFlag_moveable)) {
					V2 delta = entity->dP * dt + (float)0.5 * ddP * dt * dt;
					
					float maxCollisionTime = 1;

					for (int moveIteration = 0; moveIteration < 4 && maxCollisionTime > 0; moveIteration++) {
						float collisionTime = maxCollisionTime;

						bool horizontalCollision = false;

						for (int colliderIndex = 1; colliderIndex < numEntities; colliderIndex++) {
							if (colliderIndex != entityIndex) {
								Entity* collider = entities + colliderIndex;

								if (isSet(collider, EntityFlag_collidable)) {
									R2 colliderHitbox = rectCenterDiameter(collider->p, collider->size);	 
									R2 minkowskiSum = addDiameterTo(colliderHitbox, entity->size);

									horizontalCollision |= !getLineCollisionTime(&collisionTime, entity->p.x, entity->p.y, delta.x, delta.y, minkowskiSum.min.x, minkowskiSum.min.y, minkowskiSum.max.y); //Left vertical Line
									horizontalCollision |= !getLineCollisionTime(&collisionTime, entity->p.x, entity->p.y, delta.x, delta.y, minkowskiSum.max.x, minkowskiSum.min.y, minkowskiSum.max.y); //Right vertical Line
									horizontalCollision |= getLineCollisionTime(&collisionTime, entity->p.y, entity->p.x, delta.y, delta.x, minkowskiSum.min.y, minkowskiSum.min.x, minkowskiSum.max.x); //Bottom horizontal Line
									horizontalCollision |= getLineCollisionTime(&collisionTime, entity->p.y, entity->p.x, delta.y, delta.x, minkowskiSum.max.y, minkowskiSum.min.x, minkowskiSum.max.x); //Top horizontal Line
								}
							}
						}

						maxCollisionTime -= collisionTime;
						float collisionTimeEpsilon = (float)0.005;
						float moveTime = max(0, collisionTime - collisionTimeEpsilon);

						entity->p += delta * moveTime;
						entity->dP += ddP * dt;

						if (maxCollisionTime > 0) {
							if (horizontalCollision) {
								entity->dP.y = 0;
								delta.y = 0;
							} else {
								entity->dP.x = 0;
								delta.x = 0;
							}
						}
					}
				}

				drawTexture(renderer, entity->texture, entity->p - oldCameraP, entity->size, isSet(entity, EntityFlag_facesLeft));
			}

			SDL_RenderPresent(renderer); //Swap the buffers
			dt = 0;
			oldCameraP = newCameraP;
		}

		SDL_Delay(1);
	}

	return 0;
}