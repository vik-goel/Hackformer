#include "hackformer.h"

#include "hackformer_renderer.cpp"
#include "hackformer_consoleField.cpp"
#include "hackformer_entity.cpp"
#include "hackformer_save.cpp"

void initSpatialPartition(GameState* gameState) {
	gameState->chunksWidth = (s32)ceil(gameState->mapSize.x / gameState->chunkSize.x);
	gameState->chunksHeight = (s32)ceil(gameState->windowSize.y / gameState->chunkSize.y);

	s32 numChunks = gameState->chunksWidth * gameState->chunksHeight;
	gameState->chunks = pushArray(&gameState->levelStorage, EntityChunk, numChunks);

	memset(gameState->chunks, 0, numChunks * sizeof(EntityChunk));
}

void loadHackMap(GameState* gameState, char* fileName) {
	char filePath[2000];
	assert(strlen(fileName) < arrayCount(filePath) - 100);
	sprintf(filePath, "maps/%s", fileName);

	FILE* file = fopen(filePath, "r");
	assert(file);

	s32 mapWidthInTiles = readS32(file);
	s32 mapHeightInTiles = readS32(file);

	BackgroundType bgType = (BackgroundType)readS32(file);
	setBackgroundTexture(&gameState->backgroundTextures, bgType, gameState->renderGroup);
	addBackground(gameState);

	V2 tileSize = v2(gameState->tileSize.x, TILE_HEIGHT_WITHOUT_OVERHANG_IN_METERS);

	gameState->mapSize = hadamard(v2(mapWidthInTiles, mapHeightInTiles), tileSize);
	gameState->worldSize = maxComponents(gameState->mapSize, gameState->windowSize);

	initSpatialPartition(gameState);

	for(s32 tileY = 0; tileY < mapHeightInTiles; tileY++) {
		for(s32 tileX = 0; tileX < mapWidthInTiles; tileX++) {
			s32 tileIndex = readS32(file);

			if(tileIndex >= 0) {
				V2 tileP = hadamard(v2(tileX + 0.5, tileY + 0.5), tileSize);

				if(tileIndex == Tile_heavy) {
					addHeavyTile(gameState, tileP);
				} 
				else if(tileIndex == Tile_disappear) {
					addDisappearingTile(gameState, tileP);
				} else {
					addTile(gameState, tileP, gameState->tileAtlas[tileIndex]);
				}
			}
		}
	}

	s32 numEntities = readS32(file);

	for(s32 entityIndex = 0; entityIndex < numEntities; entityIndex++) {
		EntityType entityType = (EntityType)readS32(file);
		V2 p = readV2(file);

		switch(entityType) {
			case EntityType_player: {
				addPlayer(gameState, p);
			} break;
				 
			case EntityType_virus: {
				addVirus(gameState, p);
			} break;

			case EntityType_flyingVirus: {
				addFlyingVirus(gameState, p);
			} break;


			case EntityType_laserBase: {
				//TODO: Load the height in from the file
				double height = 4;
				addLaserController(gameState, p, height);
			} break;

			case EntityType_hackEnergy: {
				addHackEnergy(gameState, p);
			} break;

			case EntityType_endPortal: {
				addEndPortal(gameState, p);
			} break;

			case EntityType_trojan: {
				s32 numWaypoints = readS32(file);

				Entity* entity = addTrojan(gameState, p);

				if(numWaypoints > 0) {
					ConsoleField* wpField = addFollowsWaypointsField(entity, gameState);
					wpField->curWaypoint = pushArray(&gameState->levelStorage, Waypoint, numWaypoints);

					for(s32 wpIndex = 0; wpIndex < numWaypoints; wpIndex++) {
						wpField->curWaypoint[wpIndex].p = readV2(file);
						wpField->curWaypoint[wpIndex].next = wpField->curWaypoint + ((wpIndex + 1) % numWaypoints);						
					}					
				}
			} break;


			InvalidDefaultCase;
		}
	}

	fclose(file);
}

void freeLevel(GameState* gameState) {
	for (s32 entityIndex = 0; entityIndex < gameState->numEntities; entityIndex++) {
		freeEntityAtLevelEnd(gameState->entities + entityIndex, gameState);
	}

	memset(gameState->entityRefs_, 0, sizeof(gameState->entityRefs_));

	gameState->entityRefFreeList = NULL;
	gameState->consoleFreeList = NULL;
	gameState->hitboxFreeList = NULL;
	gameState->refNodeFreeList = NULL;
	gameState->messagesFreeList = NULL;
	gameState->targetRefs = NULL;
	gameState->swapField = NULL;

	gameState->consoleEntityRef = 0;
	gameState->playerRef = 0;
	gameState->numEntities = 0;
	gameState->fieldSpec.hackEnergy = 0;
	gameState->levelStorage.allocated = 0;

	gameState->reloadCurrentLevel = false;
	
	gameState->refCount_ = 1; //NOTE: This is for the null reference
}

void loadLevel(GameState* gameState, char** maps, s32 numMaps, s32* mapFileIndex, bool firstLevelLoad) {
	freeLevel(gameState);

	if (gameState->loadNextLevel) {
		firstLevelLoad = true;
		gameState->loadNextLevel = false;
		(*mapFileIndex)++;
		(*mapFileIndex) %= numMaps;
	}

	double topFieldYOffset = 1.05;
	{
		double gravityValues[] = {-9.8, -4.9, 0, 4.9, 9.8};
		s32 gravityFieldModifyCost = 10;
		gameState->gravityField = createPrimitiveField(double, gameState, "gravity", gravityValues, arrayCount(gravityValues), 0, gravityFieldModifyCost);
		gameState->gravityField->p = v2(2.25, gameState->windowSize.y - topFieldYOffset);
	}

	{
		double timeValues[] = {0, 0.2, 0.5, 1, 2};
		s32 timeFieldModifyCost = 10;
		gameState->timeField = createPrimitiveField(double, gameState, "time", timeValues, arrayCount(timeValues), 3, timeFieldModifyCost);
		gameState->timeField->p = v2(gameState->windowSize.x - 2.4, gameState->windowSize.y - topFieldYOffset);
	}

	loadHackMap(gameState, maps[*mapFileIndex]);

	char textValues[10][100];
	sprintf(textValues[0], "Batman!");
	sprintf(textValues[1], "NANANANANA");
	sprintf(textValues[2], "batman");
	addText(gameState, v2(4, 5), textValues, 3, 1);

	gameState->doingInitialSim = true;
	gameState->renderGroup->enabled = false;

	double timeStep = 1.0 / 60.0;

	for (s32 frame = 0; frame < 30; frame++) {
		updateAndRenderEntities(gameState, timeStep);
	}

	gameState->renderGroup->enabled = true;
	gameState->doingInitialSim = false;

	Entity* player = getEntityByRef(gameState, gameState->playerRef);
	assert(player);

	player->currentAnim = player->characterAnim->stand;
	clearFlags(player, EntityFlag_animIntro|EntityFlag_animOutro);

	centerCameraAround(player, gameState);

	if(firstLevelLoad) {
		gameState->camera.p = gameState->camera.newP;
		gameState->camera.moveToTarget = false;
	} else {
		gameState->camera.moveToTarget = true;
	}
}

Button createButton(GameState* gameState, char* defaultTextureFilePath, V2 p, double buttonHeight) {
	Button result = {};

	result.defaultTex = loadPNGTexture(gameState->renderGroup, defaultTextureFilePath, false);

	double buttonWidth = getAspectRatio(result.defaultTex) * buttonHeight;

	V2 size = v2(buttonWidth, buttonHeight);
	result.renderBounds = rectCenterDiameter(p, size);
	result.clickBounds = rectCenterDiameter(p, size);
	
	result.scale = 1;

	return result;
}

Button createDockButton(GameState* gameState, char* textureFileName, V2 p, double buttonHeight) {
	char pathBuffer[100];
	assert(strlen(textureFileName) - 10 < arrayCount(pathBuffer));

	sprintf(pathBuffer, "%s_default", textureFileName);
	Button result = createButton(gameState, pathBuffer, p, buttonHeight);

	sprintf(pathBuffer, "%s_clicked", textureFileName);
	result.clickedTex = loadPNGTexture(gameState->renderGroup, pathBuffer, false);

	sprintf(pathBuffer, "%s_hover", textureFileName);
	result.hoverTex = loadPNGTexture(gameState->renderGroup, pathBuffer, false);

	return result;
}

Button createPauseMenuButton(GameState* gameState, char* textureFilePath, V2 p, double buttonHeight) {
	Button result = createButton(gameState, textureFilePath, p, buttonHeight);
	result.shouldScale = true;

	return result;
}

bool updateAndDrawButton(Button* button, RenderGroup* group, Input* input, double dt) {
	bool clicked = false;

	Texture* tex = NULL;

	bool mouseInsideButton = pointInsideRect(button->clickBounds, input->mouseInMeters);

	if(input->leftMouse.justPressed) {
		button->selected = mouseInsideButton;
	}

	double scaleSpeed = 2.25 * dt;
	double minScale = 0.825;

	if(button->selected) {
		if(input->leftMouse.pressed) {
			tex = button->clickedTex;
			button->scale -= scaleSpeed;
		} else {
			button->selected = false;
			clicked = mouseInsideButton;
		}
	} 

	if(!button->selected) {
		button->scale += scaleSpeed;

		if(mouseInsideButton) {
			tex = button->hoverTex;
		} else {
			tex = button->defaultTex;
		}
	}

	button->scale = clamp(button->scale, minScale, 1);

	double scale = button->scale;
	if(!button->shouldScale) scale = 1;

	R2 scaledRenderBounds = scaleRect(button->renderBounds, v2(1, 1) * scale);

	if(!validTexture(tex)) tex = button->defaultTex;

	assert(validTexture(tex));
	pushTexture(group, tex, scaledRenderBounds, false, DrawOrder_gui);

	return clicked;
}

void translateButton(Button* button, V2 translation) {
	button->clickBounds = translateRect(button->clickBounds, translation);
	button->renderBounds = translateRect(button->renderBounds, translation);
}

void drawAnimatedBackground(GameState* gameState, Texture* background, Animation* backgroundAnim, double animTime) {
	R2 windowBounds = r2(v2(0, 0), gameState->windowSize);

	Texture* frame1 = getAnimationFrame(backgroundAnim, animTime);
	Texture* frame2 = getAnimationFrame(backgroundAnim, animTime + backgroundAnim->secondsPerFrame);

	double frameIndex = animTime / backgroundAnim->secondsPerFrame;
	double frameIndexPercent = frameIndex - (int)frameIndex;

	assert(frameIndexPercent >= 0 && frameIndexPercent <= 1);

	int frame1Alpha = (int)(255 * (1 - frameIndexPercent) + 0.5);
	int frame2Alpha = 255 - frame1Alpha;

	Color frame1Color = createColor(255, 255, 255, frame1Alpha);
	Color frame2Color = createColor(255, 255, 255, frame2Alpha);

	pushTexture(gameState->renderGroup, background, windowBounds, false, DrawOrder_gui, false);
	pushTexture(gameState->renderGroup, frame1, windowBounds, false, DrawOrder_gui, false, Orientation_0, frame1Color);
	pushTexture(gameState->renderGroup, frame2, windowBounds, false, DrawOrder_gui, false, Orientation_0, frame2Color);
}

void clearInput(Input* input) {
	input->mouseInPixels = {};
	input->mouseInMeters = {};
	input->mouseInWorld = {};
	input->dMouseMeters = {};

	for(s32 keyIndex = 0; keyIndex < arrayCount(input->keys); keyIndex++) {
		Key* key = input->keys + keyIndex;
		key->pressed = false;
		key->justPressed = false;
	}
}

Music loadMusic(char* fileName) {
	char filePath[1000];
	sprintf(filePath, "res/music/%s.mp3", fileName);

	Music result = {};
	result.data = Mix_LoadMUS(filePath);

	if(!result.data) {
		fprintf(stderr, "Error loading music: %s\n", Mix_GetError());
		InvalidCodePath;
	}

	return result;
}

void playMusic(Music* music, MusicState* musicState) {
	musicState->menuMusic.playing = false;
	musicState->gameMusic.playing = false;

	Mix_HaltMusic();
	if (music->data) Mix_PlayMusic(music->data, -1); //loop forever
	music->playing = true;
}

void initMusic(MusicState* musicState) {
#if PLAY_MUSIC
	s32 mixerFlags = MIX_INIT_MP3;
	s32 mixerInitStatus = Mix_Init(mixerFlags);

	if(mixerFlags != mixerInitStatus) {
		fprintf(stderr, "Error initializing SDL_mixer: %s\n", Mix_GetError());
		InvalidCodePath;
	}

	if (Mix_OpenAudio(48000, AUDIO_S16SYS, 2, 1024) < 0) {
		fprintf(stderr, "Error initializing SDL_mixer: %s\n", Mix_GetError());
		InvalidCodePath;
	}

	musicState->menuMusic = loadMusic("hackformer_theme");
	musicState->gameMusic = loadMusic("hackformerlevel0");
#endif
}

GameState* createGameState(s32 windowWidth, s32 windowHeight) {
	MemoryArena arena_ = createArena(2 * 1024 * 1024, true);

	GameState* gameState = pushStruct(&arena_, GameState);
	gameState->permanentStorage = arena_;

	gameState->levelStorage = createArena(16 * 1024 * 1024, false);
	gameState->hackSaveStorage = createArena(12 * 1024 * 1024, false);

	gameState->pixelsPerMeter = TEMP_PIXELS_PER_METER;
	gameState->windowWidth = windowWidth;
	gameState->windowHeight = windowHeight;
	gameState->windowSize = v2((double)windowWidth, (double)windowHeight) * (1.0 / gameState->pixelsPerMeter);

	gameState->gravity = v2(0, -9.81f);
	gameState->solidGridSquareSize = 0.1;
	gameState->tileSize = v2(TILE_WIDTH_IN_METERS, TILE_HEIGHT_IN_METERS); 
	gameState->chunkSize = v2(6, 6);

	gameState->texturesCount = 1; //NOTE: 0 is a null texture data
	gameState->animNodesCount = 1; //NOTE: 0 is a null anim data
	gameState->characterAnimsCount = 1; //NOTE: 0 is a null character data

	gameState->renderGroup = createRenderGroup(256 * 1024, &gameState->permanentStorage, gameState->pixelsPerMeter, 
		gameState->windowWidth, gameState->windowHeight, &gameState->camera,
		gameState->textures, &gameState->texturesCount);

	initInputKeyCodes(&gameState->input);

	return gameState;
}

void initFieldSpec(GameState* gameState) {
	FieldSpec* spec = &gameState->fieldSpec;

	spec->consoleFont = loadCachedFont(gameState->renderGroup, "fonts/PTS55f.ttf", 16, 2);
	spec->attribute = loadPNGTexture(gameState->renderGroup, "attributes/Attribute", false);
	spec->behaviour = loadPNGTexture(gameState->renderGroup, "attributes/Behaviour", false);
	spec->valueBackground = loadPNGTexture(gameState->renderGroup, "attributes/changer_readout", false);
	spec->leftButtonDefault = loadPNGTexture(gameState->renderGroup, "attributes/left_button", false);
	spec->leftButtonClicked = loadPNGTexture(gameState->renderGroup, "attributes/left_button_clicked", false);
	spec->leftButtonUnavailable = loadPNGTexture(gameState->renderGroup, "attributes/left_button_unavailable", false);
	spec->waypoint = loadPNGTexture(gameState->renderGroup, "waypoint", false);
	spec->waypointArrow = loadPNGTexture(gameState->renderGroup, "waypoint_arrow", false);
	spec->tileHackShield = loadPNGTexture(gameState->renderGroup, "tile_hacking/tile_hack_shield", false);
	spec->tileHackArrow = loadPNGTexture(gameState->renderGroup, "tile_hacking/right_button", false);
	spec->tileArrowSize = getDrawSize(spec->tileHackArrow, 0.5);

	spec->fieldSize = getDrawSize(spec->behaviour, 0.5);
	spec->valueSize = getDrawSize(spec->valueBackground, 0.8);
	spec->valueBackgroundPenetration = 0.4;
	
	spec->triangleSize = getDrawSize(spec->leftButtonDefault, spec->valueSize.y - spec->valueBackgroundPenetration + spec->fieldSize.y);
	spec->spacing = v2(0.05, 0);
	spec->childInset = spec->fieldSize.x * 0.125;
}

void initDock(GameState* gameState) {
	Dock* dock = &gameState->dock;

	dock->dockTex = loadPNGTexture(gameState->renderGroup, "dock/dock", false);
	dock->subDockTex = loadPNGTexture(gameState->renderGroup, "dock/sub_dock", false);
	dock->energyBarStencil = loadPNGTexture(gameState->renderGroup, "dock/energy_bar_stencil", true);
	dock->barCircleTex = loadPNGTexture(gameState->renderGroup, "dock/bar_energy", false);
	dock->gravityTex = loadPNGTexture(gameState->renderGroup, "dock/gravity_field", false);
	dock->timeTex = loadPNGTexture(gameState->renderGroup, "dock/time_field", false);
	dock->acceptButton = createDockButton(gameState, "dock/accept_button", v2(gameState->windowSize.x * 0.5 + 0.45, gameState->windowSize.y - 1.6), 0.51);
	dock->cancelButton = createDockButton(gameState, "dock/cancel_button", v2(gameState->windowSize.x * 0.5 - 2.55, gameState->windowSize.y - 1.6), 0.5);
	dock->cancelButton.clickBounds.max.x -= 0.2;
}

void initPauseMenu(GameState* gameState) {
	PauseMenu* pauseMenu = &gameState->pauseMenu;

	RenderGroup* group = gameState->renderGroup;

	pauseMenu->background = loadPNGTexture(group, "pause_menu/pause_menu", false);
	pauseMenu->backgroundAnim = loadAnimation(group, "pause_menu/pause_menu_sprite", 1280, 720, 1.f, true);

	pauseMenu->quit = createPauseMenuButton(gameState, "pause_menu/quit_button", v2(15.51, 2.62), 1.5);
	pauseMenu->restart = createPauseMenuButton(gameState, "pause_menu/restart_button", v2(6.54, 4.49), 1.1);
	pauseMenu->resume = createPauseMenuButton(gameState, "pause_menu/resume_button", v2(3.21, 8.2), 1.18);
	pauseMenu->settings = createPauseMenuButton(gameState, "pause_menu/settings_button", v2(12.4, 6.8), 1.08);
}

void initMainMenu(GameState* gameState) {
	MainMenu* mainMenu = &gameState->mainMenu;

	RenderGroup* group = gameState->renderGroup;

	mainMenu->background = loadPNGTexture(group, "main_menu/background", false);
	mainMenu->backgroundAnim = loadAnimation(group, "main_menu/background_animation", 1280, 720, 1.f, true);

	double mainMenuButtonHeight = 0.6;
	mainMenu->play = createPauseMenuButton(gameState, "main_menu/play_button", v2(10, 6), mainMenuButtonHeight);
	mainMenu->settings = createPauseMenuButton(gameState, "main_menu/options_button", v2(10.6, 5.2), mainMenuButtonHeight);
	mainMenu->quit = createPauseMenuButton(gameState, "main_menu/quit_button", v2(12, 3.2), mainMenuButtonHeight);
}

char* getSaveFilePath(char* saveFileName, MemoryArena* arena) {
	char* fileWritePath = SDL_GetPrefPath("DVJ Games", "Hackformer");
	assert(fileWritePath);
	char* saveFilePath = pushArray(arena, char, strlen(fileWritePath) + strlen(saveFileName) + 1);
	assert(saveFilePath);
	sprintf(saveFilePath, "%s%s", fileWritePath, saveFileName);
	SDL_free(fileWritePath);
	return saveFilePath;
}

void loadImages(GameState* gameState) {
	RenderGroup* renderGroup = gameState->renderGroup;

	{
		CharacterAnim* character = createCharacterAnim(gameState, &gameState->playerAnim);

		AnimNode* walkNode = createAnimNode(gameState, &character->walk);
		walkNode->intro = loadAnimation(renderGroup, "player/running_transition", 256, 256, 0.05f, true);
		walkNode->main = loadAnimation(renderGroup, "player/running", 256, 256, 0.0425f, true);
		walkNode->outro = createReversedAnimation(&walkNode->intro);

		AnimNode* standNode = createAnimNode(gameState, &character->stand);
		standNode->main = loadAnimation(renderGroup, "player/stand", 256, 256, 0.09f, true);

		AnimNode* deathNode = createAnimNode(gameState, &character->death);
		deathNode->main = loadAnimation(renderGroup, "player/death", 256, 256, 0.06f, true);

		AnimNode* jumpNode = createAnimNode(gameState, &character->jump);

		// jumpNode->intro = loadAnimation(renderGroup, "player/jumping_intro", 256, 256, 0.04f, false);
		// jumpNode->main = createAnimation(loadPNGTexture(renderGroup, "player/jump"));
		// jumpNode->outro = loadAnimation(renderGroup, "player/jumping_outro", 256, 256, 0.04f, false);

		jumpNode->intro = loadAnimation(renderGroup, "player/jumping_intro_2", 256, 256, 0.07f, false);
		jumpNode->main = loadAnimation(renderGroup, "player/jumping_2", 256, 256, 0.12f, true);
		jumpNode->outro = loadAnimation(renderGroup, "player/jumping_outro_2", 256, 256, 0.03f, false);

		AnimNode* hackNode = createAnimNode(gameState, &gameState->playerHack);
		hackNode->intro = loadAnimation(renderGroup, "player/hacking", 256, 256, 0.07f, false);
		hackNode->main = createAnimation(hackNode->intro.frames + (hackNode->intro.numFrames - 1));
		hackNode->outro = createReversedAnimation(&hackNode->intro);
	}

	{
		CharacterAnim* character = createCharacterAnim(gameState, &gameState->virus1Anim);

		AnimNode* shoot = createAnimNode(gameState, &character->shoot);
		shoot->main = loadAnimation(renderGroup, "virus1/shoot", 145, 170, 0.04f, true);
		gameState->shootDelay = getAnimationDuration(&shoot->main);
		//TODO: Make shoot animation time per frame be set by the shootDelay

		AnimNode* stand = createAnimNode(gameState, &character->stand);
		stand->main = createAnimation(loadPNGTexture(renderGroup, "virus1/stand"));
	}

	{
		CharacterAnim* character = createCharacterAnim(gameState, &gameState->flyingVirusAnim);

		AnimNode* shoot = createAnimNode(gameState, &character->shoot);
		shoot->main = loadAnimation(renderGroup, "virus2/shoot", 133, 127, 0.04f, true);

		AnimNode* stand = createAnimNode(gameState, &character->stand);
		stand->main = createAnimation(loadPNGTexture(renderGroup, "virus2/full"));
	}

	{
		CharacterAnim* character = createCharacterAnim(gameState, &gameState->trojanAnim);

		AnimNode* shoot = createAnimNode(gameState, &character->shoot);
		shoot->main = loadAnimation(renderGroup, "trojan/shoot", 256, 256, 0.04f, true);

		AnimNode* disappear = createAnimNode(gameState, &character->disappear);
		disappear->main = loadAnimation(renderGroup, "trojan/disappear", 256, 256, 0.04f, false);

		AnimNode* stand = createAnimNode(gameState, &character->stand);
		stand->main = createAnimation(loadPNGTexture(renderGroup, "trojan/full"));
	}
	
	gameState->hackEnergyAnim = loadAnimation(renderGroup, "energy_animation", 173, 172, 0.08f, true);
	gameState->laserBolt = loadPNGTexture(renderGroup, "virus1/laser_bolt");
	gameState->endPortal = loadPNGTexture(renderGroup, "end_portal");

	gameState->laserBaseOff = loadPNGTexture(renderGroup, "virus3/base_off");
	gameState->laserBaseOn = loadPNGTexture(renderGroup, "virus3/base_on");
	gameState->laserTopOff = loadPNGTexture(renderGroup, "virus3/top_off");
	gameState->laserTopOn = loadPNGTexture(renderGroup, "virus3/top_on");
	gameState->laserBeam = loadPNGTexture(renderGroup, "virus3/laser_beam");

	gameState->tileAtlasCount = arrayCount(globalTileFileNames);
	gameState->tileAtlas = pushArray(&gameState->permanentStorage, Texture*, gameState->tileAtlasCount);

	for(s32 tileIndex = 0; tileIndex < gameState->tileAtlasCount; tileIndex++) {
		char fileName[2000];
		sprintf(fileName, "tiles/%s", globalTileFileNames[tileIndex]);

		//TODO: No need to put these textures in the textures array
		gameState->tileAtlas[tileIndex] = loadPNGTexture(gameState->renderGroup, fileName);
	}
}


int main(int argc, char* argv[]) {
	s32 windowWidth = 1280, windowHeight = 720;
	SDL_Window* window = createWindow(windowWidth, windowHeight);
	GameState* gameState = createGameState(windowWidth, windowHeight);
	MusicState* musicState = &gameState->musicState;

	initMusic(musicState);

	RenderGroup* renderGroup = gameState->renderGroup;
	Input* input = &gameState->input;
	FieldSpec* spec = &gameState->fieldSpec;
	Dock* dock = &gameState->dock;
	PauseMenu* pauseMenu = &gameState->pauseMenu;
	MainMenu* mainMenu = &gameState->mainMenu;
	Camera* camera = &gameState->camera;
	initCamera(camera);

	gameState->textFont = loadFont("fonts/Roboto-Regular.ttf", 64);

	initBackgroundTextures(&gameState->backgroundTextures);

	loadImages(gameState);

	initFieldSpec(gameState);
	initDock(gameState);
	initPauseMenu(gameState);
	initMainMenu(gameState);

	char* mapFileNames[] = {
		"edit.hack",
	};

	s32 mapFileIndex = 0;
	loadLevel(gameState, mapFileNames, arrayCount(mapFileNames), &mapFileIndex, true);

	#if SHOW_MAIN_MENU
	gameState->screenType = ScreenType_mainMenu;
	playMusic(&musicState->menuMusic, musicState);
	#else
	gameState->screenType = ScreenType_game;
	playMusic(&musicState->gameMusic, musicState);
	#endif

	bool running = true;
	double dtForFrame = 0;
	double maxDtForFrame = 1.0 / 6.0;
	u32 lastTime = SDL_GetTicks();
	u32 currentTime;

	char* saveFilePath = NULL;
	char* saveFileName = "test_save.txt";

	s32 fps = 0;
	u32 frameTime = 0;
	u32 fpsTimer = SDL_GetTicks();

	while(running) {
		currentTime = SDL_GetTicks();
		dtForFrame += (double)((currentTime - lastTime) / 1000.0); 
		lastTime = currentTime;

		u32 frameStartTime = SDL_GetTicks();

		#if 0
		if(SDL_GetTicks() - fpsTimer >= 1000) {
			fpsTimer += 1000;
			gameState->fieldSpec.hackEnergy = (frameTime / fps);
			fps = 0;
			frameTime = 0;
		}

		fps++;
		#endif

		glClearColor(0, 0, 0, 1);
		glClear(GL_COLOR_BUFFER_BIT);

		dtForFrame *= gameState->timeField->doubleValues[gameState->timeField->selectedIndex];
		if (dtForFrame > maxDtForFrame) dtForFrame = maxDtForFrame;
		double unpausedDtForFrame = dtForFrame;

		gameState->swapFieldP = gameState->windowSize * 0.5 + v2(0.06, 4.43);

		pollInput(input, &running, gameState->windowHeight, gameState->pixelsPerMeter, &gameState->camera);

		if(input->pause.justPressed && inGame(gameState)) {
			togglePause(gameState);
			gameState->pauseMenu.animCounter = 0;
			pauseMenu->quit.scale = 1;
			pauseMenu->restart.scale = 1;
			pauseMenu->settings.scale = 1;
			pauseMenu->resume.scale = 1;
		}

		Input oldInput;

		if(gameState->screenType == ScreenType_pause) {
			gameState->pauseMenu.animCounter += unpausedDtForFrame;
			dtForFrame = 0;
			oldInput = gameState->input;
			clearInput(input);
		}

		bool levelResetRequested = false;

		if(inGame(gameState)) { 
			updateAndRenderEntities(gameState, dtForFrame);
			pushSortEnd(renderGroup);

			//NOTE: Draw the dock before the console

		#if DRAW_DOCK
			bool cancelButtonClicked = false;
			bool acceptButtonClicked = false;

			CachedFont* font = &gameState->fieldSpec.consoleFont;

			V2 dockSize = gameState->windowSize.x * v2(1, dock->dockTex->size.y / dock->dockTex->size.x);
			V2 dockP = v2(0, gameState->windowSize.y - dockSize.y);
			R2 dockBounds = r2(dockP, dockP + dockSize);
			V2 dockCenter = getRectCenter(dockBounds);

			double minSubDockYOffs = 0.76;
			double maxSubDockYOffs = 1.76;
			double subDockMoveSpeed = dtForFrame * 3;

			if(getEntityByRef(gameState, gameState->consoleEntityRef)) {
				dock->subDockYOffs += subDockMoveSpeed;
			} else {
				dock->subDockYOffs -= subDockMoveSpeed;
			}

			dock->subDockYOffs = clamp(dock->subDockYOffs, minSubDockYOffs, maxSubDockYOffs);

			if(dock->subDockYOffs > minSubDockYOffs) {
				V2 subDockSize = (gameState->windowSize.x * 0.58) * v2(1, dock->subDockTex->size.y / dock->subDockTex->size.x);
				V2 subDockP = v2(gameState->windowSize.x * 0.5 + 0.42, gameState->windowSize.y - maxSubDockYOffs);
				R2 subDockBounds = rectCenterDiameter(subDockP, subDockSize);

				pushClipRect(renderGroup, subDockBounds);

				double subDockTranslation = maxSubDockYOffs - dock->subDockYOffs;
				subDockBounds = translateRect(subDockBounds, v2(0, subDockTranslation));

				V2 buttonTranslation = v2(0, subDockTranslation);
				translateButton(&dock->cancelButton, buttonTranslation);
				translateButton(&dock->acceptButton, buttonTranslation);

				if (updateAndDrawButton(&dock->cancelButton, renderGroup, input, unpausedDtForFrame)) {
					cancelButtonClicked = true;
				}

				pushTexture(renderGroup, dock->subDockTex, subDockBounds, false, DrawOrder_gui, false);

				if (updateAndDrawButton(&dock->acceptButton, renderGroup, input, unpausedDtForFrame)) {
					acceptButtonClicked = true;
				}

				translateButton(&dock->cancelButton, -buttonTranslation);
				translateButton(&dock->acceptButton, -buttonTranslation);

				s32 energyLoss = getEnergyLoss(gameState);
				char energyLossStr[25];
				sprintf(energyLossStr, "%d", energyLoss);
				V2 energyLossTextP = dockCenter + v2(4.19, (minSubDockYOffs - dock->subDockYOffs)-0.11);
				pushText(renderGroup, font, energyLossStr, energyLossTextP, WHITE, TextAlignment_center);

				pushDefaultClipRect(renderGroup);
			}

			pushTexture(renderGroup, dock->dockTex, dockBounds, false, DrawOrder_gui, false);

			s32 maxEnergy = 100;
			if(gameState->fieldSpec.hackEnergy > maxEnergy) gameState->fieldSpec.hackEnergy = maxEnergy;
			else if(gameState->fieldSpec.hackEnergy < 0) gameState->fieldSpec.hackEnergy = 0;

			int energy = gameState->fieldSpec.hackEnergy;

			char energyStr[25];
			sprintf(energyStr, "%d", energy);

			V2 textP = dockCenter + v2(4.19, -0.74);
			pushText(renderGroup, font, energyStr, textP, WHITE, TextAlignment_center);

			Color energyColor = createColor(66, 217, 255, 50);

			V2 energyBarP = dockCenter + v2(0, -0.13);
			V2 energyBarSize = v2(10.1, 0.5);
			R2 energyBarBounds = rectCenterDiameter(energyBarP, energyBarSize);

			double energyBarPercentage = (double)energy / (double)maxEnergy;
			pushFilledStencil(renderGroup, dock->energyBarStencil, energyBarBounds, energyBarPercentage, energyColor);

			bool clickHandled = updateConsole(gameState, dtForFrame);

			if(!clickHandled) {
				if(cancelButtonClicked || acceptButtonClicked) {
					gameState->camera.deferredMoveToTarget = true;
					gameState->camera.moveToTarget = true;
					gameState->consoleEntityRef = 0;

					if(cancelButtonClicked) {
						loadGameFromArena(gameState);
					}
				}
			}

			if(controlZJustPressed(input) && getEntityByRef(gameState, gameState->consoleEntityRef)) {
				undoLastSaveGameFromArena(gameState);
			}

		//NOTE: Draw the dock after the console
			V2 topFieldSize = getDrawSize(dock->gravityTex, 0.7);

			V2 gravityP = gameState->gravityField->p + v2(0, 0.48);
			R2 gravityBounds = rectCenterDiameter(gravityP, topFieldSize);

			V2 timeP = gameState->timeField->p + v2(0.3, 0.36);
			R2 timeBounds = rectCenterDiameter(timeP, topFieldSize);

			pushTexture(renderGroup, dock->gravityTex, gravityBounds, false, DrawOrder_gui, false);
			pushTexture(renderGroup, dock->timeTex, timeBounds, false, DrawOrder_gui, false);

			Color barColor = createColor(191, 16, 16, 255);

			double maxGravityBarLength = 2.2;
			double gravityBarLength = maxGravityBarLength * ((double)gameState->gravityField->selectedIndex / (double)(gameState->gravityField->numValues - 1));
			V2 gravityBarSize = v2(gravityBarLength, 0.07);
			V2 circleSize = v2(1, 1) * 0.085;

			if(gravityBarLength) {
				V2 gravityBarP = gravityP + v2(-0.9, -0.3);
				V2 gravityLeftCircleP = gravityBarP + v2(-0.02, 0.035);

				pushTexture(renderGroup, dock->barCircleTex, rectCenterDiameter(gravityLeftCircleP, circleSize), false, DrawOrder_gui, false);
				
				if(gravityBarLength == maxGravityBarLength) {
					V2 gravityRightCircleP = gravityLeftCircleP + v2(gravityBarLength + 0.03, 0);
					pushTexture(renderGroup, dock->barCircleTex, rectCenterDiameter(gravityRightCircleP, circleSize), false, DrawOrder_gui, false);
				}

				pushFilledRect(renderGroup, r2(gravityBarP, gravityBarP + gravityBarSize), barColor);
			}

			double maxTimeBarLength = 2.25;
			double timeBarLength = maxTimeBarLength * ((double)gameState->timeField->selectedIndex / (double)(gameState->timeField->numValues - 1));

			if(timeBarLength) {
				V2 timeBarP = timeP + v2(-1.32, 0.23);
				V2 timeBarSize = v2(timeBarLength, gravityBarSize.y);

				pushFilledRect(renderGroup, r2(timeBarP, timeBarP + timeBarSize), barColor);

				V2 timeCircleP = timeBarP - v2(0.015, -0.035);
				circleSize *= 0.9;
				pushTexture(renderGroup, dock->barCircleTex, rectCenterDiameter(timeCircleP, circleSize), false, DrawOrder_gui, false);
			}
		#endif

			if(gameState->screenType == ScreenType_pause) {
				drawAnimatedBackground(gameState, pauseMenu->background, &pauseMenu->backgroundAnim, pauseMenu->animCounter);				

				if (updateAndDrawButton(&pauseMenu->quit, renderGroup, &oldInput, unpausedDtForFrame)) {
					running = false;
				}

				if (updateAndDrawButton(&pauseMenu->restart, renderGroup, &oldInput, unpausedDtForFrame)) {
					levelResetRequested = true;
					gameState->screenType = ScreenType_game;
				}

				if (updateAndDrawButton(&pauseMenu->resume, renderGroup, &oldInput, unpausedDtForFrame)) {
					gameState->screenType = ScreenType_game;
				}

				if (updateAndDrawButton(&pauseMenu->settings, renderGroup, &oldInput, unpausedDtForFrame)) {
					//TODO: Implement settings
				}
			}
		}

		if(gameState->screenType == ScreenType_mainMenu) {
			pushSortEnd(renderGroup);

			mainMenu->animCounter += dtForFrame;
			drawAnimatedBackground(gameState, mainMenu->background, &mainMenu->backgroundAnim, mainMenu->animCounter);

			if (updateAndDrawButton(&mainMenu->quit, renderGroup, input, dtForFrame)) {
				running = false;
			}

			if (updateAndDrawButton(&mainMenu->play, renderGroup, input, dtForFrame)) {
				gameState->screenType = ScreenType_game;
				playMusic(&musicState->gameMusic, musicState);
			}

			if (updateAndDrawButton(&mainMenu->settings, renderGroup, input, dtForFrame)) {
				//TODO: Implement settings
			}
		}
		
		drawRenderGroup(renderGroup, &gameState->fieldSpec);
		removeEntities(gameState);

		{ //NOTE: This updates the camera position
			if(getEntityByRef(gameState, gameState->consoleEntityRef)) {
				V2 movement = {};

				if(input->up.pressed) movement.y++;
				if(input->down.pressed) movement.y--;
				if(input->right.pressed) movement.x++;
				if(input->left.pressed) movement.x--;
				
				if(movement.x || movement.y) {
					double movementSpeed = 25 * dtForFrame;

					V2 minCameraP = -gameState->windowSize;
					V2 maxCameraP = maxComponents(gameState->mapSize, gameState->windowSize);

					V2 delta = normalize(movement) * movementSpeed;
					camera->newP = clampToRect(camera->p + delta, r2(minCameraP, maxCameraP));
					camera->moveToTarget = false;
				}
			}

			if(camera->moveToTarget || camera->deferredMoveToTarget) {
				V2 cameraPDiff = gameState->camera.newP - gameState->camera.p;
				double lenCameraPDiff = length(cameraPDiff);

				double maxCameraMovement = (10 + 12 * sqrt(lenCameraPDiff)) * dtForFrame;

				if (lenCameraPDiff < maxCameraMovement) {
					gameState->camera.p = gameState->camera.newP;
					camera->moveToTarget = false;
				} else {
					gameState->camera.p += cameraPDiff * (maxCameraMovement / lenCameraPDiff);
				}
			} else {
				gameState->camera.p = gameState->camera.newP;
			}

			if(camera->deferredMoveToTarget) {
				camera->deferredMoveToTarget = false;
				camera->moveToTarget = true;
			}
			
			R2 windowBounds = r2(v2(0, 0), maxComponents(gameState->mapSize, gameState->windowSize));
			R2 screenBounds = r2(camera->p, camera->p + gameState->windowSize);
			R2 clipRect = intersect(windowBounds, screenBounds);
			R2 screenSpaceClipRect = translateRect(clipRect, -camera->p);

			renderGroup->defaultClipRect = screenSpaceClipRect;
		}

		dtForFrame = 0;

		if(gameState->screenType == ScreenType_game) {
			if (input->n.justPressed) {
				if(!saveFilePath) saveFilePath = getSaveFilePath(saveFileName, &gameState->permanentStorage);
				saveGame(gameState, saveFilePath);
			}
			if (input->m.justPressed) {
				freeLevel(gameState);
				if(!saveFilePath) saveFilePath = getSaveFilePath(saveFileName, &gameState->permanentStorage);
				loadGame(gameState, saveFilePath);
			}

			if (input->x.justPressed) {
				gameState->fieldSpec.hackEnergy += 10;
			}
		}

		{ //NOTE: This reloads the game
			bool resetLevel = gameState->reloadCurrentLevel || input->r.justPressed || levelResetRequested;

			if(resetLevel || gameState->loadNextLevel) {
				loadLevel(gameState, mapFileNames, arrayCount(mapFileNames), &mapFileIndex, false);
			}
		}
	

		if(gameState->screenType == ScreenType_pause) {
			gameState->input = oldInput;
		}

		u32 frameEndTime = SDL_GetTicks();
		frameTime += frameEndTime - frameStartTime;

		SDL_GL_SwapWindow(window);
	}

	return 0;
}