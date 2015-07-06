#define _CRT_SECURE_NO_WARNINGS

#define arrayCount(array) sizeof(array) / sizeof(array[0])
#define InvalidCodePath assert(!"Invalid Code Path")
#define InvalidDefaultCase default: { assert(!"Invalid Default Case"); } break

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
#include <cmath>
#include <cstdio>
#include <cassert>

#include "glew.h"

#include "SDL.h"
#include "SDL_image.h"
#include "SDL_ttf.h"
#include "SDL_opengl.h"
#include "SDL_mixer.h"

#include "hackformer_math.h"

#define TEMP_PIXELS_PER_METER 70.0f
#define TILE_WIDTH_IN_METERS 0.9f
#define TILE_HEIGHT_IN_METERS 1.035f
#define TILE_HEIGHT_WITHOUT_OVERHANG_IN_METERS ((TILE_HEIGHT_IN_METERS) * (116.0 / 138.0))

#define TEXTURE_DATA_COUNT 500
#define ANIM_NODE_DATA_COUNT 20
#define CHARACTER_ANIM_DATA_COUNT 10

enum EntityType {
	EntityType_background,
	EntityType_player,
	EntityType_tile,
	EntityType_heavyTile,
	EntityType_death,
	EntityType_hackEnergy,
	EntityType_text,
	EntityType_virus,
	EntityType_laserBolt,
	EntityType_endPortal,
	EntityType_laserBase,
	EntityType_laserBeam,
	EntityType_flyingVirus,
	EntityType_pickupField,
	EntityType_trojan,
	EntityType_disappearingTile,
	EntityType_droppingTile,
};

enum DrawOrder {
	DrawOrder_background,
	DrawOrder_middleground,
	DrawOrder_text,
	DrawOrder_tile,
	DrawOrder_pickupField,
	DrawOrder_movingTile,
	DrawOrder_endPortal,
	DrawOrder_hackEnergy,
	DrawOrder_laserBeam,
	DrawOrder_laserBase,
	DrawOrder_virus,
	DrawOrder_flyingVirus,
	DrawOrder_trojan,
	DrawOrder_disappearingTile,
	DrawOrder_droppingTile,
	DrawOrder_heavyTile,
	DrawOrder_laserBolt,
	DrawOrder_player,
	DrawOrder_gui,
};

enum TileType {
	Tile_basic1,
	Tile_basic2,
	Tile_basic3, 
	Tile_delay,
	Tile_disappear,
	Tile_heavy,
};

static char* globalTileFileNames[] = {
	"basic_1",
	"basic_2",
	"basic_3",
	"delay",
	"disappear",
	"heavy",
};

struct MemoryArena {
	void* base;
	size_t allocated;
	size_t size;
};

MemoryArena createArena(size_t size, bool clearToZero) {
	MemoryArena result = {};
	result.size = size;

	if(clearToZero) {
		result.base = calloc(size, 1);
	} else {
		result.base = malloc(size);
	}

	assert(result.base);
	return result;
}

#define pushArray(arena, type, count) (type*)pushIntoArena_(arena, count * sizeof(type))
#define pushStruct(arena, type) (type*)pushIntoArena_(arena, sizeof(type))
#define pushSize(arena, size) pushIntoArena_(arena, size);
#define pushElem(arena, type, value) {*(type*)pushIntoArena_(arena, sizeof(type)) = value;}

void* pushIntoArena_(MemoryArena* arena, size_t amt) {
	void* result = (char*)arena->base + arena->allocated;

	arena->allocated += amt;
	assert(arena->allocated < arena->size);

	return result;
}

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
		Key keys[26];

		//TODO: The members of this struct may not be packed such that they align perfectly with the array of keys
		struct {
			Key up;
			Key down;
			Key left;
			Key right;
			Key r;
			Key x;
			Key c;
			Key n;
			Key m;
			Key z;
			Key ctrl;
			Key shift;
			Key esc;
			Key pause;
			Key leftMouse;
			Key rightMouse;
			Key num[10];
		};
	};

	s32 mouseScroll;
};

struct Camera {
	V2 p;
	V2 newP;
	bool32 moveToTarget;
	bool32 deferredMoveToTarget;
	double scale;
	V2 scaleCenter;
};

void initCamera(Camera* camera) {
	camera->scale = 1;
	camera->p = camera->newP = camera->scaleCenter =  v2(0, 0);
	camera->moveToTarget = camera->deferredMoveToTarget = false;
}

void initInputKeyCodes(Input* input) {
	input->up.keyCode1 = SDLK_w;
	input->up.keyCode2 = SDLK_UP;

	input->down.keyCode1 = SDLK_s;
	input->down.keyCode2 = SDLK_DOWN;

	input->left.keyCode1 = SDLK_a;
	input->left.keyCode2 = SDLK_LEFT;

	input->right.keyCode1 = SDLK_d;
	input->right.keyCode2 = SDLK_RIGHT;

	input->r.keyCode1 = SDLK_r;
	input->x.keyCode1 = SDLK_x;
	input->z.keyCode1 = SDLK_z;
	input->shift.keyCode1 = SDLK_RSHIFT;
	input->shift.keyCode2 = SDLK_LSHIFT;

	input->pause.keyCode1 = SDLK_p;
	input->pause.keyCode2 = SDLK_ESCAPE;

	input->n.keyCode1 = SDLK_n;
	input->m.keyCode1 = SDLK_m;
	input->c.keyCode1 = SDLK_c;

	input->esc.keyCode1 = SDLK_ESCAPE;

	input->ctrl.keyCode1 = SDLK_RCTRL;
	input->ctrl.keyCode2 = SDLK_LCTRL;

	#define INIT_NUM_KEY(n){input->num[n].keyCode1 = SDLK_##n; input->num[n].keyCode2 = SDLK_KP_##n;}

	INIT_NUM_KEY(0);
	INIT_NUM_KEY(1);
	INIT_NUM_KEY(2);
	INIT_NUM_KEY(3);
	INIT_NUM_KEY(4);
	INIT_NUM_KEY(5);
	INIT_NUM_KEY(6);
	INIT_NUM_KEY(7);
	INIT_NUM_KEY(8);
	INIT_NUM_KEY(9);

	#undef INIT_NUM_KEY
}

void pollInput(Input* input, bool* running, double windowHeight, double pixelsPerMeter, Camera* camera) {
	input->dMouseMeters = v2(0, 0);
	input->mouseScroll = 0;

	for(s32 keyIndex = 0; keyIndex < arrayCount(input->keys); keyIndex++) {
		Key* key = input->keys + keyIndex;
		key->justPressed = false;
	}

	SDL_Event event;
	while(SDL_PollEvent(&event) > 0) {
		switch(event.type) {
			case SDL_QUIT:
			*running = false;
			break;
			case SDL_KEYDOWN:
			case SDL_KEYUP: {
				bool pressed = event.key.state == SDL_PRESSED;
				SDL_Keycode keyCode = event.key.keysym.sym;

				for(s32 keyIndex = 0; keyIndex < arrayCount(input->keys); keyIndex++) {
					Key* key = input->keys + keyIndex;

					if(key->keyCode1 == keyCode || key->keyCode2 == keyCode) {
						if(pressed && !key->pressed) key->justPressed = true;
						key->pressed = pressed;
					}
				}
			} break;
			case SDL_MOUSEMOTION: {
				s32 mouseX = event.motion.x;
				s32 mouseY = event.motion.y;

				input->mouseInPixels.x = (double)mouseX;
				input->mouseInPixels.y = (double)(windowHeight - mouseY);

				V2 mouseInMeters = input->mouseInPixels * (1.0 / pixelsPerMeter);

				input->dMouseMeters = mouseInMeters - input->mouseInMeters;

				input->mouseInMeters = mouseInMeters;
				input->mouseInWorld = input->mouseInMeters + camera->p;

			} break;
			case SDL_MOUSEBUTTONDOWN:
			case SDL_MOUSEBUTTONUP: {
				if (event.button.button == SDL_BUTTON_LEFT) {
					if (event.button.state == SDL_PRESSED) {
						input->leftMouse.pressed = input->leftMouse.justPressed = true;
					} else {
						input->leftMouse.pressed = input->leftMouse.justPressed = false;
					}
				}

				if (event.button.button == SDL_BUTTON_RIGHT) {
					if (event.button.state == SDL_PRESSED) {
						input->rightMouse.pressed = input->rightMouse.justPressed = true;
					} else {
						input->rightMouse.pressed = input->rightMouse.justPressed = false;
					}
				}

				} break;

			case SDL_MOUSEWHEEL: {
				input->mouseScroll = event.wheel.y;

				//TODO: This was added in SDL 2.04
				// This is needed for cross-platform compatibility (add this in)
				#if 0
				if(event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED) input->mouseScroll *= -1;
				#endif
			} break;
		}
	}
}

bool controlZJustPressed(Input* input) {
	bool result = false;

	result |= input->ctrl.pressed && input->z.justPressed;
	result |= input->ctrl.justPressed && input->z.pressed;

	return result;
}

void writeS32(FILE* file, s32 value) {
	#ifdef SAVE_BINARY
		size_t numElementsWritten = fwrite(&value, sizeof(value), 1, file);
		assert(numElementsWritten == 1);
	#else
		fprintf(file, "%d ", value);
	#endif
}

void writeSize_t(FILE* file, size_t value) {
	#ifdef SAVE_BINARY
		size_t numElementsWritten = fwrite(&value, sizeof(value), 1, file);
		assert(numElementsWritten == 1);
	#else
		fprintf(file, "%u ", value);
	#endif
}


void writeU32(FILE* file, u32 value) {
	#ifdef SAVE_BINARY
		size_t numElementsWritten = fwrite(&value, sizeof(value), 1, file);
		assert(numElementsWritten == 1);
	#else
		fprintf(file, "%u ", value);
	#endif
}

void writeDouble(FILE* file, double value) {
	#ifdef SAVE_BINARY
		size_t numElementsWritten = fwrite(&value, sizeof(value), 1, file);
		assert(numElementsWritten == 1);
	#else
		fprintf(file, "%f ", value);
	#endif
}

void writeString(FILE* file, char* value) {
	#ifdef SAVE_BINARY
		s32 len = strlen(value);
		writeS32(file, len);
		size_t numElementsWritten = fwrite(value, sizeof(char), len, file);
		assert(numElementsWritten == len);
	#else
		fprintf(file, " \"");
		fprintf(file, value);
		fprintf(file, "\" ");
	#endif
}

void writeV2(FILE* file, V2 value) {
	writeDouble(file, value.x);
	writeDouble(file, value.y);
}

void writeR2(FILE* file, R2 value) {
	writeV2(file, value.min);
	writeV2(file, value.max);
}

s32 readS32(FILE* file) {
	s32 result = 0;

	#ifdef SAVE_BINARY
		size_t numElementsRead = fread(&result, sizeof(result), 1, file);
		assert(numElementsRead == 1);
	#else
		fscanf (file, "%d", &result);
	#endif

	return result;  
}

size_t readSize_t(FILE* file) {
	size_t result = 0;

	#ifdef SAVE_BINARY
		size_t numElementsRead = fread(&result, sizeof(result), 1, file);
		assert(numElementsRead == 1);
	#else
		fscanf (file, "%u", &result);
	#endif

	return result;  
}

u32 readU32(FILE* file) {
	u32 result = 0;

	#ifdef SAVE_BINARY
		size_t numElementsRead = fread(&result, sizeof(result), 1, file);
		assert(numElementsRead == 1);
	#else
		fscanf (file, "%u", &result);
	#endif

	return result;  
}

double readDouble(FILE* file) {
	double result = 0;

	#ifdef SAVE_BINARY
		size_t numElementsRead = fread(&result, sizeof(result), 1, file);
		assert(numElementsRead == 1);
	#else
		fscanf (file, "%lf", &result);
	#endif

	return result;  
}

void readString(FILE* file, char* buffer) {
	#ifdef SAVE_BINARY
		s32 len = readS32(file);
		size_t numElementsRead = fread(buffer, sizeof(char), len, file);
		assert(numElementsRead == len);
		buffer[len] = 0;
	#else
		char tempBuffer[1024];
		s32 offset = 0;
		bool32 firstWord = true;

		while(true) {
			fscanf (file, "%s", tempBuffer);

			s32 strSize = strlen(tempBuffer) - 2; //-2 for the ""
			memcpy(buffer + offset, tempBuffer + firstWord, strSize);

			offset += strSize;
			firstWord = false;

			if(tempBuffer[strSize + 1] == '\"') {
				break;
			}
			else {
				buffer[offset++] = tempBuffer[strSize + 1];
				buffer[offset++] = ' ';
			}
		}

		buffer[offset] = 0;
	#endif
}

V2 readV2(FILE* file) {
	V2 result = {};
	result.x = readDouble(file);
	result.y = readDouble(file);
	return result;
}

R2 readR2(FILE* file) {
	R2 result = {};
	result.min = readV2(file);
	result.max = readV2(file);
	return result;
}

SDL_Window* createWindow(s32 windowWidth, s32 windowHeight) {
	//TODO: Proper error handling if any of these libraries does not load
	//TODO: Only initialize what is needed
	if (SDL_Init(SDL_INIT_EVERYTHING) < 0) {
		fprintf(stderr, "Failed to initialize SDL. Error: %s", SDL_GetError());
		InvalidCodePath;
	}

	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
	SDL_SetHint(SDL_HINT_RENDER_VSYNC, "1");

	if (IMG_Init(IMG_INIT_PNG) != IMG_INIT_PNG) {
		fprintf(stderr, "Failed to initialize SDL_image.");
		InvalidCodePath;
	}

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

	SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);

	#if 1
	SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1); 
	SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);
	#endif

	SDL_GL_SetSwapInterval(1); //Enables vysnc

	SDL_Window* window = SDL_CreateWindow("Hackformer", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 
										  windowWidth, windowHeight, 
										  SDL_WINDOW_ALLOW_HIGHDPI|SDL_WINDOW_OPENGL);

	SDL_GLContext glContext = SDL_GL_CreateContext(window);
	assert(glContext);

	SDL_GL_SetSwapInterval(1); //Enables vysnc

	glDisable(GL_DEPTH_TEST);

	GLenum glewStatus = glewInit();

	if (glewStatus != GLEW_OK) {
		fprintf(stderr, "Failed to initialize glew. Error: %s\n", glewGetErrorString(glewStatus));
		InvalidCodePath;
	}

	if (!window) {
		fprintf(stderr, "Failed to create window. Error: %s", SDL_GetError());
		InvalidCodePath;
	}

	if (TTF_Init()) {
		fprintf(stderr, "Failed to initialize SDL_ttf.");
		InvalidCodePath;
	}

	return window;
}

struct Texture {
	GLuint texId;
	R2 uv;
	V2 size;
};

enum BackgroundType {
	Background_marine,
	Background_sunset,

	Background_count
};

struct BackgroundTexture {
	char* bgTexPath;
	char* mgTexPath;

	Texture* bg;
	Texture* mg;
};

struct BackgroundTextures {
	BackgroundType curBackgroundType;
	BackgroundTexture textures[Background_count];
};

void initBackgroundTexture(BackgroundTexture* texture, char* bgTexPath, char* mgTexPath) {
	texture->bgTexPath = bgTexPath;
	texture->mgTexPath = mgTexPath;

	texture->bg = NULL;
	texture->mg = NULL;
}

void initBackgroundTextures(BackgroundTextures* bgTextures) {
	BackgroundTexture* bt = bgTextures->textures;

	initBackgroundTexture(bt + Background_marine, "backgrounds/marine_city_bg", "backgrounds/marine_city_mg");
	initBackgroundTexture(bt + Background_sunset, "backgrounds/sunset_city_bg", "backgrounds/sunset_city_mg");
}

struct RenderGroup;
Texture* loadPNGTexture(RenderGroup*, char*, bool = false);

void setBackgroundTexture(BackgroundTextures* bgTextures, BackgroundType type, RenderGroup* group) {
	BackgroundTexture* bt = bgTextures->textures + type;

	if(!bt->bg) {
		bt->bg = loadPNGTexture(group, bt->bgTexPath);
	}

	if(!bt->mg) {
		bt->mg = loadPNGTexture(group, bt->mgTexPath);
	}

	bgTextures->curBackgroundType = type;
}

BackgroundTexture* getBackgroundTexture(BackgroundTextures* bt) {
	BackgroundTexture* result = bt->textures + bt->curBackgroundType;
	return result;
}

enum Orientation {
	Orientation_0,
	Orientation_90,
	Orientation_180,
	Orientation_270,

	Orientation_count
};

struct Color {
	u8 r, g, b, a;
};

#define WHITE (createColor(255, 255, 255, 255))
#define RED (createColor(255, 0, 0, 255))
#define GREEN (createColor(0, 255, 0, 255))
#define BLUE (createColor(0, 0, 255, 255))
#define YELLOW (createColor(255, 255, 0, 255))
#define MAGENTA (createColor(255, 0, 255, 255))
#define BLACK (createColor(0, 0, 0, 255))

Color createColor(u8 r, u8 g, u8 b, u8 a) {
	Color result = {};

	result.r = r;
	result.g = g;
	result.b = b;
	result.a = a;

	return result;
}

void pushTexture(RenderGroup* group, Texture* texture, R2 bounds, bool flipX, DrawOrder drawOrder, bool moveIntoCameraSpace = false,
	 		Orientation orientation = Orientation_0, Color color = WHITE, float emissivity = 0);

void drawBackgroundTexture(BackgroundTexture* backgroundTexture, RenderGroup* group, Camera* camera, V2 windowSize, double mapWidth) {
	Texture* bg = backgroundTexture->bg;
	Texture* mg = backgroundTexture->mg;

	double bgTexWidth = (double)bg->size.x;
	double mgTexWidth = (double)mg->size.x;
	double mgTexHeight = (double)mg->size.y;

	double bgScrollRate = bgTexWidth / mgTexWidth;

	double bgHeight = windowSize.y;
	double mgWidth = max(mapWidth, mgTexWidth);
					
	double bgWidth = mgWidth / bgScrollRate - 1;

	double bgX = max(0, camera->p.x) * (1 -  bgScrollRate);

	R2 bgBounds = r2(v2(bgX, 0), v2(bgWidth, bgHeight));
	R2 mgBounds = r2(v2(0, 0), v2(mgWidth, bgHeight));

	pushTexture(group, bg, translateRect(bgBounds, -camera->p), false, DrawOrder_background);
	pushTexture(group, mg, translateRect(mgBounds, -camera->p), false, DrawOrder_middleground);
}