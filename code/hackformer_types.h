#define _CRT_SECURE_NO_WARNINGS

#define arrayCount(array) sizeof(array) / sizeof(array[0])
#define InvalidCodePath assert(!"Invalid Code Path")
#define InvalidDefaultCase default: { assert(!"Invalid Default Case"); } break

#include <stdint.h>

typedef uint64_t u64;
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t u8;

typedef int64_t s64;
typedef int32_t s32;
typedef int16_t s16;
typedef int8_t s8;

typedef int32_t bool32; 
typedef int16_t bool16; 
typedef int8_t bool8; 

#ifdef HACKFORMER_MAC
    #include <OpenGL/gl3.h>
    #define STB_IMAGE_IMPLEMENTATION
    #include "stb_image.h"
#else 
    #define USE_GLEW
    #include "../build/stb_image.h"
#endif

#include <cstdlib>
#include <cmath>
#include <cstdio>
#include <cassert>

#ifdef USE_GLEW
#include "GL/glew.h"
#endif

#include "SDL2/SDL_opengl.h"
#include "SDL2/SDL.h"
#include "SDL2_ttf/SDL_ttf.h"
#include "SDL2_mixer/SDL_mixer.h"

#include "hackformer_math.h"
#include "hackformer_packBuilder.h"

#define TEMP_PIXELS_PER_METER 70.0f
#define TILE_WIDTH_IN_METERS 0.9f
#define TILE_HEIGHT_WITHOUT_OVERHANG_IN_METERS TILE_WIDTH_IN_METERS
#define TILE_HEIGHT_IN_METERS (TILE_HEIGHT_WITHOUT_OVERHANG_IN_METERS * (144.0 / 128.0))

#define TILE_FLIP_X_FLAG (1 << 28)
#define TILE_FLIP_Y_FLAG (1 << 29)
#define TILE_INDEX_MASK (0xFFFFFFFF ^ (TILE_FLIP_X_FLAG | TILE_FLIP_Y_FLAG))

#define MAX_GLOWING_TEXTURES 20
#define MAX_TEXTURES 500
#define MAX_ANIM_NODES 20
#define MAX_CHARACTER_ANIMS 10

#define KILOBYTES(num) (1024 * num)
#define MEGABYTES(num) (1024 * KILOBYTES(num))

enum EntityType {
	EntityType_background,
	EntityType_player,
	EntityType_tile,
	EntityType_heavyTile,
	EntityType_death,
	EntityType_hackEnergy,
	EntityType_text,
	EntityType_endPortal,
	EntityType_laserBase,
	EntityType_laserBeam,
	EntityType_pickupField,
	EntityType_trojan,
	EntityType_disappearingTile,
	EntityType_droppingTile,
	EntityType_motherShip,
	EntityType_motherShipProjectile,
	EntityType_motherShipProjectileDeath,
	EntityType_trawler,
	EntityType_bootUp,
	EntityType_trawlerBolt,
	EntityType_trojanBolt,
	EntityType_lamp,
	EntityType_lamp_0,
	EntityType_lamp_1,
	EntityType_shrike,
	EntityType_test,
	EntityType_checkPoint,
};

enum DrawOrder {
	DrawOrder_test,
	DrawOrder_background,
	DrawOrder_middleground,
	DrawOrder_text,
	DrawOrder_tile,
	DrawOrder_pickupField,
	DrawOrder_lamp,
	DrawOrder_lamp_0,
	DrawOrder_lamp_1,
	DrawOrder_movingTile,
	DrawOrder_checkPoint,
	DrawOrder_endPortal,
	DrawOrder_hackEnergy,
	DrawOrder_laserBeam,
	DrawOrder_laserBase,
	DrawOrder_motherShip,
	DrawOrder_motherShip_0,
	DrawOrder_motherShip_1,
	DrawOrder_motherShip_2,
	DrawOrder_motherShip_3,
	DrawOrder_motherShip_4,
	DrawOrder_motherShip_5,
	DrawOrder_shrike,
	DrawOrder_trojan,
	DrawOrder_trawler,
	DrawOrder_trawler_0,
	DrawOrder_trawler_1,
	DrawOrder_trawler_2,
	DrawOrder_trawler_3,
	DrawOrder_player,
	DrawOrder_trawlerBolt,
	DrawOrder_heavyTile,
	DrawOrder_trojanBolt,
	DrawOrder_motherShipProjectile,
	DrawOrder_light,
	DrawOrder_gui,
};

struct HackAbilities {
	bool32 editFields;
	bool32 moveTiles;
	bool32 moveFields;
	bool32 cloneFields;
	bool32 globalHacks;
};

enum TileType {
	Tile_disappear,
	Tile_heavy,
	Tile_corner,
	Tile_middle,
	Tile_top,
};

struct TileData {
	TileType type;
	AssetId regularId;
	AssetId glowingId;
	bool32 tall;
	const char* fileName;
};


#define TILE(type, assetName, tall, fileName) {Tile_##type, Asset_##assetName##Regular, Asset_##assetName##Glowing, tall, fileName},

static TileData globalTileData[] {
	TILE(disappear, disappear, false, "disappear")
	TILE(heavy, heavy, false, "heavy")
	TILE(corner, corner1, true, "corner_1")
	TILE(middle, middle1, false, "middle_1")
	TILE(top, top1, true, "top_1")
	TILE(top, top2, true, "top_2")
};

#undef TILE

struct MemoryArena {
	void* base;
	size_t allocated;
	size_t size;
};

void initArena(MemoryArena* arena, size_t size, bool clearToZero) {
	arena->allocated = 0;
	arena->size = size;

	if(clearToZero) {
		arena->base = calloc(1, size);
	} else {
		arena->base = malloc(size);
	}

	assert(arena->base);
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

MemoryArena* subArena(MemoryArena* arena, size_t size) {
	MemoryArena* result = pushStruct(arena, MemoryArena);

	result->size = size;
	result->allocated = 0;
	result->base = pushSize(arena, size);

	return result;
}

struct Assets {
	FILE* fileHandle;
	SDL_RWops* assetFileHandle;
	s32 assetFileOffsets[Asset_count];
};

void initAssets(Assets* assets) {
	assets->fileHandle = fopen("assets.bin", "rb");
	assert(assets->fileHandle);

	assets->assetFileHandle = SDL_RWFromFile("assets.bin", "rb");
	assert(assets->assetFileHandle);

	for(s32 assetIndex = 1; assetIndex < Asset_count; assetIndex++) {
		SDL_RWread(assets->assetFileHandle, assets->assetFileOffsets + assetIndex, sizeof(s32), 1);
	}
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
		Key keys[33];

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
			Key f;
			Key g;
			Key h;
			Key j;
			Key k;
			Key t;
			Key l;
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
	V2 dP;
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
	input->f.keyCode1 = SDLK_f;
	input->g.keyCode1 = SDLK_g;
	input->h.keyCode1 = SDLK_h;
	input->j.keyCode1 = SDLK_j;
	input->k.keyCode1 = SDLK_k;
	input->t.keyCode1 = SDLK_t;
	input->l.keyCode1 = SDLK_l;

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

SDL_Window* createWindow(s32 windowWidth, s32 windowHeight) {
	//TODO: Proper error handling if any of these libraries does not load
	//TODO: Only initialize what is needed
	if (SDL_Init(SDL_INIT_EVERYTHING) < 0) {
		fprintf(stderr, "Failed to initialize SDL. Error: %s", SDL_GetError());
		InvalidCodePath;
	}

	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
	SDL_SetHint(SDL_HINT_RENDER_VSYNC, "1");

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

#ifdef USE_GLEW
	GLenum glewStatus = glewInit();

	if (glewStatus != GLEW_OK) {
		fprintf(stderr, "Failed to initialize glew. Error: %s\n", glewGetErrorString(glewStatus));
		InvalidCodePath;
	}
#endif

    glViewport(0, 0, windowWidth*2, windowHeight*2);
    
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
	AssetId bgId;
	AssetId mgId;

	Texture* bg;
	Texture* mg;
};

struct BackgroundTextures {
	BackgroundType curBackgroundType;
	BackgroundTexture textures[Background_count];
};

struct Messages {
	s32 count;
	s32 selectedIndex;
	char text[10][100];
	Texture textures[10];

	//NOTE: This is currently used for the free list
	//TODO: This could be used to support having more than 10 messages
	Messages* next;
};

TTF_Font* loadFont(struct RenderGroup* group, AssetId id, s32 fontSize, MemoryArena* arena);
TTF_Font* loadTextFont(struct RenderGroup* group, MemoryArena* arena) {
	TTF_Font* result = loadFont(group, Asset_entityFont, 64, arena);
	assert(result);
	return result;
}

void initBackgroundTexture(BackgroundTexture* texture, AssetId bgId, AssetId mgId) {
	texture->bgId = bgId;
	texture->mgId = mgId;

	texture->bg = NULL;
	texture->mg = NULL;
}

void initBackgroundTextures(BackgroundTextures* bgTextures) {
	BackgroundTexture* bt = bgTextures->textures;

	initBackgroundTexture(bt + Background_marine, Asset_marineCityBg, Asset_marineCityMg);
	initBackgroundTexture(bt + Background_sunset, Asset_sunsetCityBg, Asset_sunsetCityMg);
}

struct RenderGroup;
Texture* loadPNGTexture(RenderGroup*, AssetId, bool = false);

void setBackgroundTexture(BackgroundTextures* bgTextures, BackgroundType type, RenderGroup* group) {
	BackgroundTexture* bt = bgTextures->textures + type;

	if(!bt->bg) {
		bt->bg = loadPNGTexture(group, bt->bgId);
	}

	if(!bt->mg) {
		bt->mg = loadPNGTexture(group, bt->mgId);
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

double getAspectRatio(Texture* texture) {
	double result = texture->size.x / texture->size.y;
	return result;
}

V2 getDrawSize(Texture* texture, double height) {
	V2 result = {};

	double aspectRatio = getAspectRatio(texture);
	result.x = height * aspectRatio;
	result.y = height;

	return result;
}

void pushTexture(RenderGroup* group, Texture* texture, R2 bounds, bool flipX, bool flipY, DrawOrder drawOrder, bool moveIntoCameraSpace = false,
	 		Orientation orientation = Orientation_0, Color color = WHITE, float emissivity = 0);

void drawBackgroundTexture(BackgroundTexture* backgroundTexture, RenderGroup* group, Camera* camera, V2 windowSize, double mapWidth) {
	Texture* bg = backgroundTexture->bg;
	Texture* mg = backgroundTexture->mg;

	double bgTexWidth = (double)bg->size.x;
	double mgTexWidth = (double)mg->size.x;
	//double mgTexHeight = (double)mg->size.y;

	double bgScrollRate = bgTexWidth / mgTexWidth;

	double bgHeight = windowSize.y;
	double bgWidth = getDrawSize(bg, bgHeight).x;
	double mgWidth = getDrawSize(mg, bgHeight).x;

	//double mgWidth = max(mapWidth, mgTexWidth);
	//double bgWidth = mgWidth / bgScrollRate - 1;

	double bgX = max(0, camera->p.x) * (1 -  bgScrollRate);

	R2 bgBounds = r2(v2(bgX, 0), v2(bgWidth, bgHeight));
	R2 mgBounds = r2(v2(0, 0), v2(mgWidth, bgHeight));

	pushTexture(group, bg, translateRect(bgBounds, -camera->p), false, false, DrawOrder_background);
	pushTexture(group, mg, translateRect(mgBounds, -camera->p), false, false, DrawOrder_middleground);
}

struct IOStream {
	FILE* file;
	MemoryArena* arena;
	void* readPtr;
	bool32 reading;
	struct GameState* gameState;
};

IOStream createIostream(struct GameState* gameState, MemoryArena* arena, void* readPtr = NULL) {
	IOStream result = {};

	result.gameState = gameState;

	result.arena = arena;
	result.readPtr = readPtr;
	if(readPtr) result.reading = true;

	return result;
}

IOStream createIostream(struct GameState* gameState, char* fileName, bool32 reading, bool32 requiresFile = false) {
	IOStream result = {};

	result.gameState = gameState;

	if(reading) {
		result.file = fopen(fileName, "rb");
	} else {
		result.file = fopen(fileName, "wb");
	}

	if(requiresFile) {
		assert(result.file);
	}

	result.reading = reading;

	return result;
}

void freeIostream(IOStream* iostream) {
	if(iostream->file) {
		fclose(iostream->file);
	}
}

#define writeElem(iostream, value) writeElem_((iostream), &(value), sizeof((value)))
void writeElem_(IOStream* iostream, void* value, size_t valueSize) {
	if(iostream->file) {
		size_t numElementsWritten = fwrite(value, valueSize, 1, iostream->file);
		assert(numElementsWritten == 1);
	}
	else if(iostream->arena) {
		void* mem = pushSize(iostream->arena, valueSize);
		memcpy(mem, value, valueSize);
	}
	else {
		InvalidCodePath;
	}
}

#define readElem(iostream, value) readElem_((iostream), &(value), sizeof((value)))
void readElem_(IOStream* iostream, void* value, size_t valueSize) {
	if(iostream->file) {
		size_t numElementsRead = fread(value, valueSize, 1, iostream->file);
		assert(numElementsRead == 1);
	}
	else if(iostream->arena && iostream->readPtr) {
		void* mem = iostream->readPtr;
		iostream->readPtr = (char*)iostream->readPtr + valueSize;
		assert((size_t)iostream->readPtr <= (size_t)iostream->arena->base + (size_t)iostream->arena->allocated);
		memcpy(value, mem, valueSize);
	}
	else {
		InvalidCodePath;
	}
}

#define streamValue(iostream, type, value) {type val = value; streamElem((iostream), (val));}
#define streamElem(iostream, value) streamElem_((iostream), &(value), sizeof((value)))
void streamElem_(IOStream* stream, void* value, size_t valueSize) {
	if(stream->reading) {
		readElem_(stream, value, valueSize);
	}
	else {
		writeElem_(stream, value, valueSize);
	}
}

void streamStr(IOStream* stream, char* str) {
	if(stream->reading) {
		s32 len;
		readElem_(stream, &len, sizeof(len));
		readElem_(stream, str, len);
		str[len] = 0;
	}
	else {
		s32 len = (s32)strlen(str);
		writeElem_(stream, &len, sizeof(len));
		writeElem_(stream, str, len);
	}
}

void streamV2(IOStream* stream, V2* vec) {
	streamElem(stream, vec->x);
	streamElem(stream, vec->y);
}

void streamV3(IOStream* stream, V3* vec) {
	streamElem(stream, vec->x);
	streamElem(stream, vec->y);
	streamElem(stream, vec->z);
}

void streamR2(IOStream* stream, R2* rect) {
	streamV2(stream, &rect->min);
	streamV2(stream, &rect->max);
}

void writeS32(FILE* file, s32 value) {
	fprintf(file, "%d ", value);
}

void writeString(FILE* file, char* value, bool otherMode = false) {
	fprintf(file, " \"");
	fprintf(file, "%s", value);
	fprintf(file, "\" ");
}

void writeU32(FILE* file, u32 value) {
	fprintf(file, "%u ", value);
}

void writeDouble(FILE* file, double value) {
	fprintf(file, "%f ", value);
}

void writeV2(FILE* file, V2 value) {
	writeDouble(file, value.x);
	writeDouble(file, value.y);
}

void writeV3(FILE* file, V3 value) {
	writeDouble(file, value.x);
	writeDouble(file, value.y);
	writeDouble(file, value.z);
}

void writeV2(FILE* file, R2 value) {
	writeV2(file, value.min);
	writeV2(file, value.max);
}

size_t readSize_t(FILE* file) {
	size_t result = 0;
	fscanf (file, "%zu", &result);

	return result;  
}

u32 readU32(FILE* file) {
	u32 result = 0;
	fscanf (file, "%u", &result);

	return result;  
}

double readDouble(FILE* file) {
	double result = 0;
	fscanf (file, "%lf", &result);

	return result;  
}

V2 readV2(FILE* file) {
	V2 result = {};
	result.x = readDouble(file);
	result.y = readDouble(file);
	return result;
}

V3 readV3(FILE* file) {
	V3 result = {};
	result.x = readDouble(file);
	result.y = readDouble(file);
	result.z = readDouble(file);
	return result;
}

R2 readR2(FILE* file) {
	R2 result = {};
	result.min = readV2(file);
	result.max = readV2(file);
	return result;
}

s32 readS32(FILE* file) {
	s32 result = 0;
	fscanf (file, "%d", &result);
	return result;  
}

void readString(FILE* file, char* buffer) {
	char tempBuffer[1024];
	s32 offset = 0;

	while(true) {
		fscanf (file, "%s", tempBuffer);

		s32 strSize = (s32)strlen(tempBuffer);
		memcpy(buffer + offset, tempBuffer, strSize);

		offset += strSize;

		if(buffer[offset - 1] == '\"') {
			break;
		}
		else {
			buffer[offset++] = ' ';
		}
	}

	buffer[offset - 1] = 0;
	memcpy(buffer, buffer + 1, offset - 1);
}

void streamMessages(IOStream* stream, Messages** messagesPtr, MemoryArena* arena) {
	Messages* messages = *messagesPtr;

	if(stream->reading) {
		s32 count;
		streamElem(stream, count);

		if(count < 0) {
			*messagesPtr = NULL;
			return;
		} else {
			messages = pushStruct(arena, Messages);
			messages->count = count;

			for(s32 i = 0; i < count; i++) {
				messages->textures[i].texId = 0;
			}
		}
	}
	else {
		if(messages) {
			streamElem(stream, messages->count);
		} else {
			streamValue(stream, s32, -1);
			return;
		}
	}

	streamElem(stream, messages->selectedIndex);

	for(s32 i = 0; i < messages->count; i++) {
		streamStr(stream, messages->text[i]);
	}

	*messagesPtr = messages;
}

struct Waypoint {
	V2 p;
	bool32 moved;
	bool32 selected;
	Waypoint* next;
};

void streamWaypoints(IOStream* stream, Waypoint** waypointPtr, MemoryArena* arena, bool fromHackFile, bool inEditor) {
	s32 count;

	if(stream->reading) {
		readElem(stream, count);

		Waypoint* points = pushArray(arena, Waypoint, count);

		for(s32 i = 0; i < count; i++) {
			points[i].next = points + ((i + 1) % count);

			if(inEditor && i == count - 1) {
				points[i].next = NULL;
			}

			streamV2(stream, &points[i].p);

			if(!fromHackFile) {
				streamElem(stream, points[i].moved);
				streamElem(stream, points[i].selected);
			}
		}

		assert(*waypointPtr == NULL);
		*waypointPtr = points;
	} else {
		count = 0;

		for(Waypoint* w = *waypointPtr; w; w = w->next) {
			if(count != 0 && w == *waypointPtr) break;
			count++;
		}

		writeElem(stream, count);

		Waypoint* w = *waypointPtr;

		for(s32 i = 0; i < count; i++) {
			streamV2(stream, &w->p);

			if(!fromHackFile) {
				streamElem(stream, w->moved);
				streamElem(stream, w->selected);
			}

			w = w->next;
		}
	}
}

void streamHackAbilities(IOStream* stream, HackAbilities* abilities) {
	streamElem(stream, abilities->editFields);
	streamElem(stream, abilities->moveTiles);
	streamElem(stream, abilities->moveFields);
	streamElem(stream, abilities->cloneFields);
	streamElem(stream, abilities->globalHacks);
}