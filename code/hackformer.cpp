#include "SDL.h"
#include "SDL_image.h"
#include <stdio.h>
#include <cassert>
#include "hackformer_math.h"
#include <cstdlib>
#include <cstring>
#include "SDL_ttf.h"

#define uint unsigned int
#define uint16 unsigned short

#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 720

#define PIXELS_PER_METER 70.0

#define ARRAY_COUNT(array) sizeof(array) / sizeof(array[0])

struct Input {
	V2 mouseInPixels;
	V2 mouseInMeters;
	V2 mouseInWorld;
	bool upPressed, leftPressed, rightPressed;
	bool upJustPressed;
	bool leftMousePressed, leftMouseJustPressed;
};

struct Texture {
	SDL_Texture* tex;
	SDL_Rect srcRect;
};

struct Animation {
	Texture* frames;
	uint numFrames;
	float secondsPerFrame;
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
	EntityFlag_onGround = 1 << 3,
	EntityFlag_consoleSelected = 1 << 4,
};

enum ConsoleFieldType {
	ConsoleField_Float,
	ConsoleField_String,
	ConsoleField_Int
};

struct ConsoleField {
	ConsoleFieldType type;
	char* name;
	void* values;
	int numValues;
	int selectedIndex;
	int initialIndex;
};

struct Entity {
	EntityType type;
	uint flags;

	V2 p;
	V2 dP;

	V2 renderSize;
	Texture* texture;
	float animTime;

	ConsoleField** fields;
	int numFields;
};

struct MemoryArena {
	char* base;
	uint allocated;
	uint size;
};

struct GameState {
	Entity entities[1000];
	int numEntities;
	MemoryArena permanentStorage;
	SDL_Renderer* renderer;
	Input input;
};

#define pushArray(arena, type, count) pushIntoArena_(arena, count * sizeof(type))
#define pushStruct(arena, type) pushIntoArena_(arena, sizeof(type))

char* pushIntoArena_(MemoryArena* arena, uint amt) {
	arena->allocated += amt;
	assert(arena->allocated < arena->size);

	char* result = arena->base + arena->allocated - amt;
	return result;
}

SDL_Texture* loadPNGTexture(SDL_Renderer* renderer, char* fileName) {
	SDL_Surface *image = IMG_Load(fileName);
	assert(image);
	SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, image);
	assert(texture);
	SDL_FreeSurface(image);
	SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);

	return texture;
}

void loadPNGTexture(Texture* texture, SDL_Renderer* renderer, char* fileName) {
	texture->tex = loadPNGTexture(renderer, fileName);
	SDL_QueryTexture(texture->tex, NULL, NULL, &texture->srcRect.w, &texture->srcRect.h);
	texture->srcRect.x = texture->srcRect.y = 0;
}

Animation loadAnimation(GameState* gameState, char* fileName, int frameWidth, int frameHeight, float secondsPerFrame) {
	Animation result = {};

	assert(secondsPerFrame > 0);
	result.secondsPerFrame = secondsPerFrame;
	SDL_Texture* tex = loadPNGTexture(gameState->renderer, fileName);

	int width, height;
	SDL_QueryTexture(tex, NULL, NULL, &width, &height);

	int numCols = width / frameWidth;
	int numRows = height / frameHeight;

	assert(frameWidth > 0);
	assert(frameHeight > 0);
	assert(width % numCols == 0);
	assert(height % numRows == 0);

	result.numFrames = numRows * numCols;
	result.frames = (Texture*)pushArray(&gameState->permanentStorage, Texture, result.numFrames);

	for (int rowIndex = 0; rowIndex < numRows; rowIndex++) {
		for (int colIndex = 0; colIndex < numCols; colIndex++) {
			int textureIndex = colIndex + rowIndex * numCols;
			Texture* frame = result.frames + textureIndex;

			frame->tex = tex;

			SDL_Rect* rect = &frame->srcRect;

			rect->x = colIndex * frameWidth;
			rect->y = rowIndex * frameHeight;
			rect->w = frameWidth;
			rect->h = frameHeight;
		}
	}

	return result;
}

void getAnimationFrame(Animation* animation, float animTime, Texture** texture) {
	int frameIndex = (int)(animTime / animation->secondsPerFrame) % animation->numFrames;
	*texture = &animation->frames[frameIndex];
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

Entity* addEntity(GameState* gameState) {
	assert(gameState->numEntities < ARRAY_COUNT(gameState->entities));

	Entity* result = gameState->entities + gameState->numEntities;
	gameState->numEntities++;

	*result = {};
	return result;
}

char* loadDelmittedStringIntoBuffer(char* src, char* dest, int* length) {
	for (char* c = src; *c != ','; c++) {
		dest[(*length)++] = c[0];
	}

	dest[*(length)++] = 0;
	return dest;
}

void loadTiledMap(GameState* gameState, char* fileName, float* mapWidth, float* mapHeight) {
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
				SDL_Texture* atlas = loadPNGTexture(gameState->renderer, strBuf);

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

				textures = (Texture*)pushArray(&gameState->permanentStorage, Texture, tileCols * tileRows);

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
						Entity* tile = addEntity(gameState);
						tile->p = v2((float)(x + 0.5) * tileSize, (float)(y - 0.5) * tileSize);
						tile->renderSize  = v2(tileSize, tileSize);
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

void drawTexture(SDL_Renderer* renderer, Texture* texture, V2 center, V2 renderSize, bool flipX) {
	R2 rect = rectCenterDiameter(center, renderSize
);
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

TTF_Font* loadFont(char* fileName, int fontSize) {
	TTF_Font *font = TTF_OpenFont(fileName, fontSize);

	if (!font) {
		fprintf(stderr, "Failed to load font: ");
		fprintf(stderr, fileName);
		assert(false);
	}

	return font;
}

void drawText(SDL_Renderer* renderer, TTF_Font* font, char* msg, float x, float y) {
	SDL_Color fontColor = {};
	SDL_Surface *fontSurface = TTF_RenderText_Blended(font, msg, fontColor);
	assert(fontSurface);
	
	SDL_Texture *fontTexture = SDL_CreateTextureFromSurface(renderer, fontSurface);
	assert(fontTexture);

	SDL_FreeSurface(fontSurface);

	int width, height;
	SDL_QueryTexture(fontTexture, NULL, NULL, &width, &height);

	float widthInMeters = (float)width / (float)PIXELS_PER_METER;
	float heightInMeters = (float)height / (float)PIXELS_PER_METER;

	R2 fontBounds = rectCenterDiameter(v2(x + widthInMeters / 2, y + heightInMeters / 2), v2(widthInMeters, heightInMeters));
	SDL_Rect dstRect = getPixelSpaceRect(fontBounds);

	SDL_RenderCopy(renderer, fontTexture, NULL, &dstRect);
}

bool getLineCollisionTime(float* collisionTime, float currentX, float currentY, float deltaX, 
						  float deltaY, float lineX, float lineMinY, float lineMaxY) {
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

ConsoleField createFloatField(char* name, float* values, int numValues, int initialIndex) {
	assert(numValues > 0);
	assert(initialIndex >= 0 && initialIndex < numValues);

	ConsoleField result = {};

	result.type = ConsoleField_Float;
	result.name = name;
	result.values = values;
	result.selectedIndex = result.initialIndex = initialIndex;
	result.numValues = numValues;

	return result;
}

int main(int argc, char *argv[]) {
	//TODO: Only initialize what is needed
	if (SDL_Init(SDL_INIT_EVERYTHING) < 0) {
		fprintf(stderr, "Failed to initialize SDL. Error: %s", SDL_GetError());
		assert(false);
		return -1;
	}

	if (IMG_Init(IMG_INIT_PNG) != IMG_INIT_PNG) {
		fprintf(stderr, "Failed to initialize SDL Image.");
		assert(false);
		return -1;
	}

	SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1); 
	SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);

	SDL_Window *window = SDL_CreateWindow("C++former", 
										  SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 
										  WINDOW_WIDTH, WINDOW_HEIGHT, 
										  SDL_WINDOW_ALLOW_HIGHDPI);

	if (!window) {
		fprintf(stderr, "Failed to create window. Error: %s", SDL_GetError());
		assert(false);
		return -1;
	}
	SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

	if (!renderer) {
		fprintf(stderr, "Failed to create renderer. Error: %s", SDL_GetError());
		assert(false);
		return -1;
	}

	if (TTF_Init()) {
		fprintf(stderr, "Failed to initialize SDL_ttf.");
		assert(false);
		return -1;
	}

	TTF_Font* font = loadFont("fonts/Roboto-Regular.ttf", 64);

	int gameStateSize = sizeof(GameState); 

	MemoryArena arena_ = {};
	arena_.size = 1024 * 1024 * 512;
	arena_.base = (char*)calloc(arena_.size, 1);

	GameState* gameState = (GameState*)pushStruct(&arena_, GameState);
	gameState->numEntities = 1; //NOTE: 0 is for the null entity
	gameState->permanentStorage = arena_;
	gameState->renderer = renderer;

	Texture playerStand, playerJump;
	loadPNGTexture(&playerStand, renderer, "res/player/stand.png");
	loadPNGTexture(&playerJump, renderer, "res/player/jump.png");
	Animation playerWalk = loadAnimation(gameState, "res/player/walk.png", 128, 128, 0.05f);

	Entity* background = addEntity(gameState);
	background->texture = (Texture*)pushStruct(&gameState->permanentStorage, Texture);
	loadPNGTexture(background->texture, renderer, "res/SunsetCityBackground.png");
	background->type = EntityType_Background;

	Entity* player = addEntity(gameState);
	player->p = {2, 8};
	player->renderSize = {1.25, 1.25};
	player->texture = &playerStand;
	player->type = EntityType_Player;
	addFlags(player, EntityFlag_moveable|EntityFlag_collidable|EntityFlag_facesLeft);

	ConsoleField playerFields[1];
	float playerSpeedValues[] = {3, 8, 13, 18, 23};
	playerFields[0] = createFloatField("speed", playerSpeedValues, ARRAY_COUNT(playerSpeedValues), 2);
	player->fields = (ConsoleField**)&playerFields;
	player->numFields = 1;

	float mapWidth, mapHeight;
	loadTiledMap(gameState, "maps/map1.txt", &mapWidth, &mapHeight);

	background->renderSize = {mapWidth, mapHeight + 1};
	background->p = background->renderSize / 2;

	V2 oldCameraP = {};
	V2 newCameraP = {};

	bool running = true;
	double frameTime = 1.0 / 60.0;
	uint fpsCounterTimer = SDL_GetTicks();
	uint fps = 0;
	float dt = 0;
	uint lastTime = SDL_GetTicks();
	uint currentTime;

	while (running) {
		gameState->input.leftMouseJustPressed = false;
		gameState->input.upJustPressed = false;

		currentTime = SDL_GetTicks();
		dt += (float)((currentTime - lastTime) / 1000.0); 
		lastTime = currentTime;

		if (currentTime - fpsCounterTimer > 1000) {
			fpsCounterTimer += 1000;
			//printf("Fps: %d\n", fps);
			fps = 0;
		}

		if (dt > frameTime) {
			fps++;

			SDL_Event event;
			while(SDL_PollEvent(&event) > 0) {
				Input* input = &gameState->input;

				switch(event.type) {
					case SDL_QUIT:
					running = false;
					break;
					case SDL_KEYDOWN:
					case SDL_KEYUP: {
						bool pressed = event.key.state == SDL_PRESSED;
						SDL_Keycode key = event.key.keysym.sym;

						if (key == SDLK_w || key == SDLK_UP) {
							if (pressed && !input->upPressed) input->upJustPressed = true;
							input->upPressed = pressed;
						}
						else if (key == SDLK_a || key == SDLK_LEFT) input->leftPressed = pressed;
						else if (key == SDLK_d || key == SDLK_RIGHT) input->rightPressed = pressed;
					} break;
					case SDL_MOUSEMOTION: {
						int mouseX = event.motion.x;
						int mouseY = event.motion.y;

						input->mouseInPixels.x = (float)mouseX;
						input->mouseInPixels.y = (float)(WINDOW_HEIGHT - mouseY);

						input->mouseInMeters = input->mouseInPixels / (float)PIXELS_PER_METER;
						input->mouseInWorld = input->mouseInMeters + oldCameraP;

						} break;
					case SDL_MOUSEBUTTONDOWN:
					case SDL_MOUSEBUTTONUP:
						if (event.button.button == SDL_BUTTON_LEFT) {
							if (event.button.state == SDL_PRESSED) 
								input->leftMousePressed = input->leftMouseJustPressed = true;
						}
						break;
				}
			}

			SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
			SDL_RenderClear(renderer);

			for (int entityIndex = 1; entityIndex < gameState->numEntities; entityIndex++) {
				Entity* entity = gameState->entities + entityIndex;
				entity->animTime += dt;

				V2 ddP = {0, -9.8f};

				if(entity->fields && entity->numFields) {
						if (gameState->input.leftMouseJustPressed) {
						R2 clickBox = rectCenterDiameter(entity->p, entity->renderSize);
						bool mouseInside = isPointInsideRect(clickBox, gameState->input.mouseInWorld);

						if (mouseInside) {
							addFlags(entity, EntityFlag_consoleSelected);
						} else {
							clearFlags(entity, EntityFlag_consoleSelected);
						}
					}
				}

				switch(entity->type) {
					case EntityType_Player: {
						float xMove = 0;
						float xMoveAcceleration = 13.0f;

						if (gameState->input.rightPressed) xMove += xMoveAcceleration;
						if (gameState->input.leftPressed) xMove -= xMoveAcceleration;

						ddP.x = xMove;

						if (xMove < 0) clearFlags(entity, EntityFlag_facesLeft);
						else if (xMove > 0) addFlags(entity, EntityFlag_facesLeft);

						entity->dP.x *= (float)pow(E, -9.0 * dt);

						float windowWidth = (float)(WINDOW_WIDTH / PIXELS_PER_METER);
						float maxCameraX = mapWidth - windowWidth;
						newCameraP.x = clamp((float)(entity->p.x - windowWidth / 2.0), 0, maxCameraX);

						if (isSet(entity, EntityFlag_onGround)) {
							if (gameState->input.upJustPressed) {
								entity->dP.y = 5.0f;
								entity->texture = &playerJump;
							} else {
								if (abs(entity->dP.x) < 0.5f) {
									entity->texture = &playerStand;
									entity->animTime = 0;
								} else {
									getAnimationFrame(&playerWalk, entity->animTime, &entity->texture);
								}
							}
						} else {
							entity->texture = &playerJump;
							entity->animTime = 0;
						}
					} break;

					case EntityType_Tile: {

					} break;
					case EntityType_Background: {

					} break;
				}

				if (isSet(entity, EntityFlag_moveable)) {
					clearFlags(entity, EntityFlag_onGround); //TODO: Think of a better place to do this
					V2 delta = entity->dP * dt + (float)0.5 * ddP * dt * dt;
					
					float maxCollisionTime = 1;

					for (int moveIteration = 0; moveIteration < 4 && maxCollisionTime > 0; moveIteration++) {
						float collisionTime = maxCollisionTime;

						bool horizontalCollision = false;

						for (int colliderIndex = 1; colliderIndex < gameState->numEntities; colliderIndex++) {
							if (colliderIndex != entityIndex) {
								Entity* collider = gameState->entities + colliderIndex;

								if (isSet(collider, EntityFlag_collidable)) {
									R2 colliderHitbox = rectCenterDiameter(collider->p, collider->renderSize
								);	 
									R2 minkowskiSum = addDiameterTo(colliderHitbox, entity->renderSize
								);

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
								addFlags(entity, EntityFlag_onGround); //TODO: Only add this flag when we collide with the top line of an entity
							} else {
								entity->dP.x = 0;
								delta.x = 0;
							}
						}
					}
				}

				bool flipX = isSet(entity, EntityFlag_facesLeft);
				drawTexture(renderer, entity->texture, entity->p - oldCameraP, entity->renderSize, flipX);

				if (isSet(entity, EntityFlag_consoleSelected)) {

				}
			}

			drawText(renderer, font, "Hello, World", 3, 3);

			SDL_RenderPresent(renderer); //Swap the buffers
			dt = 0;
			oldCameraP = newCameraP;
		}

		SDL_Delay(1);
	}

	return 0;
}