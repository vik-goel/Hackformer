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


Editor
-------
- Adding text
- Setting the height of a laser controller
- Setting and moving around waypoints
- Displaying backgrounds at different zoom levels

*/

#define SAVE_BINARY
#include "hackformer_types.h"

#define HACKFORMER_GAME

struct GameState;
struct Input;

#include "hackformer_renderer.h"
#include "hackformer_consoleField.h"
#include "hackformer_entity.h"
#include "hackformer_save.h"

#define SHOW_COLLISION_BOUNDS 0
#define SHOW_CLICK_BOUNDS 0
#define DRAW_ENTITIES 1
#define PLAY_MUSIC 0
#define SHOW_MAIN_MENU 0
#define ENABLE_LIGHTING 1
#define DRAW_BACKGROUND 1
#define DRAW_DOCK 1

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
	Texture* defaultTex;
	Texture* hoverTex;
	Texture* clickedTex;

	R2 renderBounds;
	R2 clickBounds;
	double scale;
	bool32 selected;
	bool32 shouldScale;
};

struct PauseMenu {
	Texture* background;
	Animation backgroundAnim;
	double animCounter;

	Button quit;
	Button restart;
	Button resume;
	Button settings;
};

struct MainMenu {
	Texture* background;
	Animation backgroundAnim;
	double animCounter;

	Button quit;
	Button play;
	Button settings;
};

struct Dock {
	Texture* dockTex;
	Texture* subDockTex;
	Texture* energyBarStencil;
	Texture* barCircleTex;
	Texture* gravityTex;
	Texture* timeTex;

	Button acceptButton;
	Button cancelButton;

	double subDockYOffs;
};

enum ScreenType {
	ScreenType_mainMenu,
	ScreenType_settings,
	ScreenType_game,
	ScreenType_pause
};

struct Music {
	Mix_Music* data;
	bool32 playing;
};

struct MusicState {
	Music menuMusic;
	Music gameMusic;
};

struct LaserImages {
	Texture* baseOff;
	Texture* baseOn;
	Texture* topOff;
	Texture* topOn;
	Texture* beam;
};

struct MotherShipImages {
	Texture* emitter;
	Texture* base;
	Texture* rotators[3];
	Animation spawning;
	Animation projectileMoving;
	CharacterAnim* projectileDeath;
};

struct TrawlerImages {
	Texture* frame;
	Texture* wheel;
	Texture* body;
	Animation shoot;
	Animation bootUp;
};

struct GameState {
	s32 numEntities;
	Entity entities[1000];

	//NOTE: 0 is the null reference
	EntityReference entityRefs_[500];

	ScreenType screenType;
	
	//NOTE: These must be sequential for laser collisions to work
	s32 refCount_;

	MemoryArena permanentStorage;
	MemoryArena levelStorage;
	MemoryArena hackSaveStorage;

	RenderGroup* renderGroup;
	TTF_Font* textFont;

	Input input;
	Camera camera;

	RefNode* targetRefs;
	s32 consoleEntityRef;
	RefNode* fadingOutConsoles;
	s32 playerRef;

	bool32 loadNextLevel;
	bool32 reloadCurrentLevel;
	bool32 doingInitialSim;

	ConsoleField* consoleFreeList;
	RefNode* refNodeFreeList;
	EntityReference* entityRefFreeList;
	Hitbox* hitboxFreeList;
	Messages* messagesFreeList;
	Waypoint* waypointFreeList;

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
	V2 mapSize;
	V2 worldSize;

	double pixelsPerMeter;
	s32 windowWidth, windowHeight;
	V2 windowSize;

	V2 gravity;

	AnimNode* playerHack;
	CharacterAnim* playerAnim;
	CharacterAnim* virus1Anim;
	CharacterAnim* flyingVirusAnim;
	CharacterAnim* trojanAnim;

	BackgroundTextures backgroundTextures;

	Animation hackEnergyAnim;
	Texture* laserBolt;
	Texture* endPortal;

	LaserImages laserImages;
	MotherShipImages motherShipImages;
	TrawlerImages trawlerImages;

	Texture* heavyTileTex;
	s32 tileAtlasCount;
	Texture** tileAtlas;

	s32 texturesCount;
	Texture textures[TEXTURE_DATA_COUNT];

	s32 animNodesCount;
	AnimNode animNodes[ANIM_NODE_DATA_COUNT];

	s32 characterAnimsCount;
	CharacterAnim characterAnims[CHARACTER_ANIM_DATA_COUNT];

	PauseMenu pauseMenu;
	MainMenu mainMenu;
	Dock dock;

	MusicState musicState;

	double collisionBoundsAlpha;
};


void setCameraTarget(Camera* camera, V2 target) {
	camera->newP = target;
	camera->moveToTarget = true;
}

void initSpatialPartition(GameState* gameState);


bool inGame(GameState* gameState) {
	bool result = gameState->screenType == ScreenType_game || 
				  gameState->screenType == ScreenType_pause;
	return result;
}

void togglePause(GameState* gameState) {
	if(gameState->screenType == ScreenType_game) {
		gameState->screenType = ScreenType_pause;
	} 
	else if(gameState->screenType == ScreenType_pause) {
		gameState->screenType = ScreenType_game;
	}
	else {
		InvalidCodePath;
	}
}