#include "SDL.h"
#include "SDL_image.h"
#include <stdio.h>
#include <cassert>
#include "hackformer_math.cpp"
#include <cstdlib>
#include <cstring>
#include "SDL_ttf.h"

#define uint unsigned int
#define uint16 unsigned short

#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 720

#define PIXELS_PER_METER 70.0f

#define ARRAY_COUNT(array) sizeof(array) / sizeof(array[0])

#define SHOW_COLLISION_BOUNDS 1

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
	EntityType_Null,
	EntityType_Player,
	EntityType_Tile,
	EntityType_Background,
	EntityType_BlueEnergy,
};

enum DrawOrder {
	DrawOrder_Player = 10000,
	DrawOrder_BlueEnergy = 10,
	DrawOrder_Tile = 0,
	DrawOrder_Background = -1000,
};

enum EntityFlag {
	EntityFlag_moveable = 1 << 0,
	EntityFlag_collidable = 1 << 1,
	EntityFlag_facesLeft = 1 << 2,
	EntityFlag_consoleSelected = 1 << 3,
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
	DrawOrder drawOrder;

	V2* collisionPoints;
	int numCollisionPoints;

	ConsoleField* fields;
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
	V2 oldCameraP = {};
	V2 newCameraP = {};

	Entity* consoleEntity;

	Texture playerStand, playerJump;
	Animation playerWalk;

	Texture blueEnergy;
	Texture background;

	float tileSize;
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

Entity* addEntity(GameState* gameState, EntityType type, DrawOrder drawOrder, V2 p, V2 renderSize) {
	assert(gameState->numEntities < ARRAY_COUNT(gameState->entities));

	Entity* result = gameState->entities + gameState->numEntities;
	gameState->numEntities++;

	*result = {};
	result->type = type;
	result->drawOrder = drawOrder;
	result->p = p;
	result->renderSize = renderSize;

	result->numCollisionPoints = 4;
	result->collisionPoints = (V2*)pushArray(&gameState->permanentStorage, V2, result->numCollisionPoints);

	result->collisionPoints[0].x = -renderSize.x / 2;
	result->collisionPoints[0].y = -renderSize.y / 2;

	result->collisionPoints[1].x = -renderSize.x / 2;
	result->collisionPoints[1].y = renderSize.y / 2;

	result->collisionPoints[2].x = renderSize.x / 2;
	result->collisionPoints[2].y = renderSize.y / 2;

	result->collisionPoints[3].x = renderSize.x / 2;
	result->collisionPoints[3].y = -renderSize.y / 2;

	return result;
}

Entity* addPlayer(GameState* gameState, V2 p) {
	Entity* player = addEntity(gameState, EntityType_Player, DrawOrder_Player, p, v2(1.25f, 1.25f));
	player->texture = &gameState->playerStand;
	addFlags(player, EntityFlag_moveable|EntityFlag_collidable|EntityFlag_facesLeft);

	player->fields = (ConsoleField*)pushArray(&gameState->permanentStorage, ConsoleField, 1);
	player->numFields = 1;
	float playerSpeedValues[] = {3, 8, 13, 18, 23};
	player->fields[0] = createFloatField("speed", playerSpeedValues, ARRAY_COUNT(playerSpeedValues), 2);

	return player;
}

Entity* addBlueEnergy(GameState* gameState, V2 p) {
	Entity* blueEnergy = addEntity(gameState, EntityType_BlueEnergy, DrawOrder_BlueEnergy, p, v2(0.8f, 0.8f));
	blueEnergy->texture = &gameState->blueEnergy;
	addFlags(blueEnergy, EntityFlag_moveable|EntityFlag_collidable);
	return blueEnergy;
}

Entity* addBackground(GameState* gameState, float mapWidth, float mapHeight) {
	Entity* background = addEntity(gameState, EntityType_Background, DrawOrder_Background, v2(0, 0), v2(mapWidth, mapHeight));
	background->texture = &gameState->background;
	background->p = background->renderSize / 2;
	return background;
}

Entity* addTile(GameState* gameState, V2 p, Texture* texture) {
	Entity* tile = addEntity(gameState, EntityType_Tile, DrawOrder_Tile, p, v2(gameState->tileSize, gameState->tileSize));
	tile->texture = texture;
	addFlags(tile, EntityFlag_collidable);
	return tile;
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

	float tileSize = gameState->tileSize;

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
						V2 tileP = v2((x + 0.5f) * tileSize, (y - 0.5f) * tileSize);
						Texture* tileTexture = textures + tileIndex;
						addTile(gameState, tileP, tileTexture);
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

void drawTexture(SDL_Renderer* renderer, Texture* texture, R2 bounds, bool flipX) {
	SDL_Rect dstRect = getPixelSpaceRect(bounds);

	SDL_RendererFlip flip = flipX ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE;
	SDL_RenderCopyEx(renderer, texture->tex, &texture->srcRect, &dstRect, 0, NULL, flip);
}

void drawFilledRect(SDL_Renderer* renderer, R2 rect, V2 cameraP = v2(0, 0)) {
	R2 r = subtractFromRect(rect, cameraP);
	SDL_Rect dstRect = getPixelSpaceRect(r);
	SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
	SDL_RenderFillRect(renderer, &dstRect);
}

void drawLine(SDL_Renderer* renderer, V2 p1, V2 p2) {
	int x1 = (int)(p1.x * PIXELS_PER_METER);
	int y1 = (int)(WINDOW_HEIGHT - p1.y * PIXELS_PER_METER);
	int x2 = (int)(p2.x * PIXELS_PER_METER);
	int y2 = (int)(WINDOW_HEIGHT - p2.y * PIXELS_PER_METER);

	SDL_SetRenderDrawColor(renderer, 255, 0, 255, 255);
	SDL_RenderDrawLine(renderer, x1, y1, x2, y2);
}

void drawPolygon(SDL_Renderer* renderer, V2* vertices, int numVertices, V2 center) {
	for (int vertexIndex = 0; vertexIndex < numVertices; vertexIndex++) {
		V2* p1 = vertices + vertexIndex;
		V2* p2 = vertices + (vertexIndex + 1) % numVertices;
		drawLine(renderer, *p1 + center, *p2 + center);
	}
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

void drawText(SDL_Renderer* renderer, TTF_Font* font, char* msg, float x, float y, V2 camera = v2(0, 0)) {
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

	R2 fontBounds = rectCenterDiameter(v2(x + widthInMeters / 2, y + heightInMeters / 2) - camera, v2(widthInMeters, heightInMeters));
	SDL_Rect dstRect = getPixelSpaceRect(fontBounds);

	SDL_RenderCopy(renderer, fontTexture, NULL, &dstRect);
}

int renderOrderCompare(const void* elem1, const void* elem2) {
	DrawCall* a = (DrawCall*)elem1;
	DrawCall* b = (DrawCall*)elem2;

	if (a->drawOrder < b->drawOrder) return -1;
	if (a->drawOrder > b->drawOrder) return 1;
	return 0;
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
	gameState->entities->type = EntityType_Null;

	loadPNGTexture(&gameState->playerStand, renderer, "res/player/stand.png");
	loadPNGTexture(&gameState->playerJump, renderer, "res/player/jump.png");
	gameState->playerWalk = loadAnimation(gameState, "res/player/walk.png", 128, 128, 0.04f);
	loadPNGTexture(&gameState->background, renderer, "res/SunsetCityBackground.png");
	loadPNGTexture(&gameState->blueEnergy, renderer, "res/blue_energy.png");

	addPlayer(gameState, v2(2, 8));
	addBlueEnergy(gameState, v2(4, 6));

	float mapWidth, mapHeight;
	gameState->tileSize = 1.0f;
	loadTiledMap(gameState, "maps/map1.txt", &mapWidth, &mapHeight);

	addBackground(gameState, mapWidth, mapHeight);

	int maxDrawCalls = 5000;
	int numDrawCalls = 0;
	DrawCall* drawCalls = (DrawCall*)pushArray(&gameState->permanentStorage, DrawCall, maxDrawCalls);

	bool running = true;
	double frameTime = 1.0 / 60.0;
	uint fpsCounterTimer = SDL_GetTicks();
	uint fps = 0;
	float dtForFrame = 0;
	uint lastTime = SDL_GetTicks();
	uint currentTime;

	V2 polygonSum[1024];
	int polygonSumMaxCount = ARRAY_COUNT(polygonSum); 

	while (running) {
		gameState->input.leftMouseJustPressed = false;
		gameState->input.upJustPressed = false;

		currentTime = SDL_GetTicks();
		dtForFrame += (float)((currentTime - lastTime) / 1000.0); 
		lastTime = currentTime;

		if (currentTime - fpsCounterTimer > 1000) {
			fpsCounterTimer += 1000;
			//printf("Fps: %d\n", fps);
			fps = 0;
		}

		if (dtForFrame > frameTime) {
			fps++;
			numDrawCalls = 0;

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
						input->mouseInWorld = input->mouseInMeters + gameState->oldCameraP;

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

			float dt = dtForFrame;
			if (gameState->consoleEntity) {
				dt /= 5.0f;
			}

			SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
			SDL_RenderClear(renderer);

			for (int entityIndex = 1; entityIndex < gameState->numEntities; entityIndex++) {
				Entity* entity = gameState->entities + entityIndex;
				entity->animTime += dt;

				V2 ddP = {0, -9.8f};

				if(entity->numFields) {
					if (gameState->input.leftMouseJustPressed) {
						R2 clickBox = rectCenterDiameter(entity->p, entity->renderSize);
						bool mouseInside = isPointInsideRect(clickBox, gameState->input.mouseInWorld);

						if (mouseInside) {
							addFlags(entity, EntityFlag_consoleSelected);
							gameState->consoleEntity = entity;
						} else {
							if (isSet(entity, EntityFlag_consoleSelected)) {
								gameState->consoleEntity = NULL;
								clearFlags(entity, EntityFlag_consoleSelected);
							}
						}
					}
				}

				switch(entity->type) {
					case EntityType_Player: {
						float xMove = 0;
						float xMoveAcceleration = 17.0f;

						if (gameState->input.rightPressed) xMove += xMoveAcceleration;
						if (gameState->input.leftPressed) xMove -= xMoveAcceleration;

						ddP.x = xMove;

						if (xMove < 0) clearFlags(entity, EntityFlag_facesLeft);
						else if (xMove > 0) addFlags(entity, EntityFlag_facesLeft);

						entity->dP.x *= (float)pow(E, -9.0 * dt);

						float windowWidth = (float)(WINDOW_WIDTH / PIXELS_PER_METER);
						float maxCameraX = mapWidth - windowWidth;
						gameState->newCameraP.x = clamp((float)(entity->p.x - windowWidth / 2.0), 0, maxCameraX);

						if (entity->dP.y == 0) {
							if (gameState->input.upPressed) {
								entity->dP.y = 5.0f;
								entity->texture = &gameState->playerJump;
							} else {
								if (abs(entity->dP.x) < 0.5f) {
									entity->texture = &gameState->playerStand;
									entity->animTime = 0;
								} else {
									getAnimationFrame(&gameState->playerWalk, entity->animTime, &entity->texture);
								}
							}
						} else {
							entity->texture = &gameState->playerJump;
							entity->animTime = 0;
						}
					} break;
					case EntityType_BlueEnergy:
					case EntityType_Tile:
					case EntityType_Null:
					case EntityType_Background: {

					} break;
				}

				if (isSet(entity, EntityFlag_moveable)) {
					V2 delta = entity->dP * dt + (float)0.5 * ddP * dt * dt;
					
					float maxCollisionTime = 1;

					for (int moveIteration = 0; moveIteration < 4 && maxCollisionTime > 0; moveIteration++) {
						float collisionTime = maxCollisionTime;

						bool horizontalCollision = true; //TODO: Use wall normal instead
						V2 lineCollider;
						bool collision = false;

						for (int colliderIndex = 1; colliderIndex < gameState->numEntities; colliderIndex++) {
							if (colliderIndex != entityIndex) {
								Entity* collider = gameState->entities + colliderIndex;

								if (isSet(collider, EntityFlag_collidable)) {
									int polygonSumCount = 0;

									addPolygons(collider->p, entity->collisionPoints, entity->numCollisionPoints,
												collider->collisionPoints, collider->numCollisionPoints,
												polygonSum, polygonSumMaxCount, &polygonSumCount);

									for (int vertexIndex = 0; vertexIndex < polygonSumCount; vertexIndex++) {
										V2* lp1 = polygonSum + vertexIndex;
										V2* lp2 = polygonSum + (vertexIndex + 1) % polygonSumCount;

										bool hitLine = raycastLine(entity->p, delta, *lp1, *lp2, &collisionTime);

										if (hitLine) {
											collision = true;
											lineCollider = *lp1 - *lp2;
										}
									}

									/*if(entity->type == EntityType_Player) {
										drawPolygon(renderer, polygonSum, polygonSumCount, v2(0, 0));
									}*/
								}
							}
						}

						maxCollisionTime -= collisionTime;
						float collisionTimeEpsilon = 0.01f;
						float moveTime = max(0, collisionTime - collisionTimeEpsilon);

						V2 movement = delta * moveTime;
						entity->p += movement;

						if (collision) {
							delta -= movement;
							V2 lineNormal = normalize(rotate90(lineCollider));

							delta -= innerProduct(delta, lineNormal) * lineNormal;
                    		entity->dP -= innerProduct(entity->dP, lineNormal) * lineNormal;
                    		ddP -= innerProduct(ddP, lineNormal) * lineNormal;
						}
					}
				}

				entity->dP += ddP * dt;

				/*if (entity->type == EntityType_Player) {
					R2 rect = rectCenterRadius(entity->p, v2(0.05f, 0.05f));
					drawFilledRect(renderer, rect, gameState->oldCameraP);
				}*/

				assert(numDrawCalls + 1 < maxDrawCalls);
				DrawCall* draw = drawCalls + numDrawCalls++;

				draw->drawOrder = entity->drawOrder;
				draw->flipX = isSet(entity, EntityFlag_facesLeft);
				draw->texture = entity->texture;
				draw->bounds = rectCenterDiameter(entity->p - gameState->oldCameraP, entity->renderSize);

			#if SHOW_COLLISION_BOUNDS
				draw->collisionPoints = &entity->collisionPoints;
				draw->numCollisionPoints = entity->numCollisionPoints;
			#endif


				//drawPolygon(renderer, entity->collisionPoints, entity->numCollisionPoints, entity->p);
			}

			qsort(drawCalls, numDrawCalls, sizeof(DrawCall), renderOrderCompare);

			for (int drawIndex = 0; drawIndex < numDrawCalls; drawIndex++) {
				DrawCall* draw = drawCalls + drawIndex;
				drawTexture(renderer, draw->texture, draw->bounds, draw->flipX);

			#if SHOW_COLLISION_BOUNDS
				if (draw->collisionPoints) {
					drawPolygon(renderer, *draw->collisionPoints, draw->numCollisionPoints, getRectCenter(draw->bounds));
				}
			#endif
			}

			if (gameState->consoleEntity) {
				for (int fieldIndex = 0; fieldIndex < gameState->consoleEntity->numFields; fieldIndex++) {
					ConsoleField* field = gameState->consoleEntity->fields + fieldIndex;

					char strBuffer[100];
					_itoa_s((int)(gameState->consoleEntity->dP.y * 1000000), strBuffer, 10);
					drawText(renderer, font, strBuffer, 3, 3, gameState->oldCameraP);
				}
			}

			SDL_RenderPresent(renderer); //Swap the buffers
			dtForFrame = 0;
			gameState->oldCameraP = gameState->newCameraP; //TODO: This is no longer necessary since all drawing is done later
		}

		SDL_Delay(1);
	}

	return 0;
}