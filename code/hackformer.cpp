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

void loadWaypoints(IOStream* stream, Entity* entity, GameState* gameState) {
	ConsoleField* wpField = addFollowsWaypointsField(entity, gameState);
	streamWaypoints(stream, &wpField->curWaypoint, &gameState->levelStorage, true, false);
}

void loadHackMap(GameState* gameState, char* fileName) {
	char filePath[2000];
	assert(strlen(fileName) < arrayCount(filePath) - 100);
	sprintf(filePath, "maps/%s.hack", fileName);

	IOStream streamObj = createIostream(gameState, filePath, true);
	IOStream* stream = &streamObj;

	s32 mapWidthInTiles, mapHeightInTiles;
	streamElem(stream, mapWidthInTiles);
	streamElem(stream, mapHeightInTiles);

	s32 initialEnergy;
	streamElem(stream, initialEnergy);
	gameState->fieldSpec.hackEnergy = initialEnergy;

	streamHackAbilities(stream, &gameState->fieldSpec.hackAbilities);

	BackgroundType bgType;
	streamElem(stream, bgType);
	setBackgroundTexture(&gameState->backgroundTextures, bgType, gameState->renderGroup);
	addBackground(gameState);

	V2 tileSize = v2(TILE_WIDTH_IN_METERS, TILE_HEIGHT_WITHOUT_OVERHANG_IN_METERS);
	double tileEpsilon = 0.000001;
	V2 tilePlacementSize = tileSize + v2(1, 1) * tileEpsilon;

	gameState->mapSize = hadamard(v2(mapWidthInTiles, mapHeightInTiles), tileSize);
	gameState->worldSize = maxComponents(gameState->mapSize, gameState->windowSize);

	initSpatialPartition(gameState);

	Entity* testEntity = addEntity(gameState, EntityType_test, DrawOrder_test, v2(0, 0), v2(0, 0));
	setFlags(testEntity, EntityFlag_noMovementByDefault);
	gameState->testEntityRef = testEntity->ref;


	for(s32 tileY = 0; tileY < mapHeightInTiles; tileY++) {
		for(s32 tileX = 0; tileX < mapWidthInTiles; tileX++) {
			s32 tile;
			streamElem(stream, tile);

			if(tile >= 0) {
				s32 tileIndex = tile & TILE_INDEX_MASK;
				bool32 flipX = tile & TILE_FLIP_X_FLAG;
				bool32 flipY = tile & TILE_FLIP_Y_FLAG;

				V2 tileP = hadamard(v2(tileX, tileY), tilePlacementSize);

				if(globalTileData[tileIndex].tall) {
					double overhangHeight = TILE_HEIGHT_IN_METERS - TILE_HEIGHT_WITHOUT_OVERHANG_IN_METERS;
					tileP += 0.5 * (v2(TILE_WIDTH_IN_METERS, TILE_HEIGHT_IN_METERS) + v2(0, 1) * tileEpsilon);

					if(flipY) {
						tileP.y -= overhangHeight;
					}
				} else {
					tileP += 0.5 * tileSize;
				}

				if(tileIndex == Tile_heavy) {
					addHeavyTile(gameState, tileP + v2(0, 0.5), flipX, flipY);
				} 
				else if(tileIndex == Tile_disappear) {
					addDisappearingTile(gameState, tileP, flipX, flipY);
				}
				// else if(tileIndex == Tile_delay) {
				// 	addDroppingTile(gameState, tileP);
				// } 
				else {
					addTile(gameState, tileP, tileIndex, flipX, flipY);
				}
			}
		}
	}

	s32 numEntities;
	streamElem(stream, numEntities);

	for(s32 entityIndex = 0; entityIndex < numEntities; entityIndex++) {
		EntityType entityType;
		streamElem(stream, entityType);

		V2 p;
		streamV2(stream, &p);

		switch(entityType) {
			case EntityType_player: {
				addPlayer(gameState, p);
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
				bool loadSameLevel = false;

				if(strcmp(fileName, "level_2") == 0) {
					loadSameLevel = true;
				}

				addEndPortal(gameState, p, loadSameLevel);
			} break;

			case EntityType_trojan: {
				Entity* entity = addTrojan(gameState, p);
				loadWaypoints(stream, entity, gameState);
			} break;

			case EntityType_motherShip: {
				addMotherShip(gameState, p);
			} break;

			case EntityType_trawler: {
				addTrawler(gameState, p);
			} break;

			case EntityType_checkPoint: {
				addCheckPoint(gameState, p);
			} break;
		
			case EntityType_shrike: {
				addShrike(gameState, p);
			} break;

			case EntityType_lamp_0: {
				Entity* entity = addLamp(gameState, p, 0.5, gameState->lights[0], v3(1, 1, 1));

				double hitboxWidth = entity->renderSize.x;
				double hitboxHeight = entity->renderSize.y;
				double halfHitboxWidth = hitboxWidth * 0.5;
				double halfHitboxHeight = hitboxHeight * 0.5;
				Hitbox* hitbox = addHitbox(entity, gameState);
				setHitboxSize(entity, hitbox, hitboxWidth * 1, hitboxHeight * 1);
				hitbox->collisionPointsCount = 9;
				hitbox->originalCollisionPoints[0] = v2(-0.998264 * halfHitboxWidth, -0.989583 * halfHitboxHeight);
				hitbox->originalCollisionPoints[1] = v2(1 * halfHitboxWidth, -0.989583 * halfHitboxHeight);
				hitbox->originalCollisionPoints[2] = v2(0.928819 * halfHitboxWidth, -0.190972 * halfHitboxHeight);
				hitbox->originalCollisionPoints[3] = v2(0.698785 * halfHitboxWidth, 0.486111 * halfHitboxHeight);
				hitbox->originalCollisionPoints[4] = v2(0.312500 * halfHitboxWidth, 0.911458 * halfHitboxHeight);
				hitbox->originalCollisionPoints[5] = v2(-0.030382 * halfHitboxWidth, 1.006944 * halfHitboxHeight);
				hitbox->originalCollisionPoints[6] = v2(-0.438368 * halfHitboxWidth, 0.833333 * halfHitboxHeight);
				hitbox->originalCollisionPoints[7] = v2(-0.750868 * halfHitboxWidth, 0.373264 * halfHitboxHeight);
				hitbox->originalCollisionPoints[8] = v2(-0.950521 * halfHitboxWidth, -0.269097 * halfHitboxHeight);
			} break;

			case EntityType_lamp_1: {
				Entity* entity = addLamp(gameState, p, 0.4, gameState->lights[1], v3(1, 0, 0));

				double hitboxWidth = entity->renderSize.x;
				double hitboxHeight = entity->renderSize.y;
				double halfHitboxWidth = hitboxWidth * 0.5;
				double halfHitboxHeight = hitboxHeight * 0.5;
				Hitbox* hitbox = addHitbox(entity, gameState);
				setHitboxSize(entity, hitbox, hitboxWidth * 1.2, hitboxHeight * 1.2);
				hitbox->collisionPointsCount = 12;
				hitbox->originalCollisionPoints[0] = v2(-0.815972 * halfHitboxWidth, -0.985681 * halfHitboxHeight);
				hitbox->originalCollisionPoints[1] = v2(0.811632 * halfHitboxWidth, -0.999618 * halfHitboxHeight);
				hitbox->originalCollisionPoints[2] = v2(0.820312 * halfHitboxWidth, -0.693011 * halfHitboxHeight);
				hitbox->originalCollisionPoints[3] = v2(0.963542 * halfHitboxWidth, -0.330657 * halfHitboxHeight);
				hitbox->originalCollisionPoints[4] = v2(0.993924 * halfHitboxWidth, 0.157126 * halfHitboxHeight);
				hitbox->originalCollisionPoints[5] = v2(0.950521 * halfHitboxWidth, 0.617036 * halfHitboxHeight);
				hitbox->originalCollisionPoints[6] = v2(0.785590 * halfHitboxWidth, 0.979390 * halfHitboxHeight);
				hitbox->originalCollisionPoints[7] = v2(-0.772569 * halfHitboxWidth, 0.993326 * halfHitboxHeight);
				hitbox->originalCollisionPoints[8] = v2(-0.941840 * halfHitboxWidth, 0.700656 * halfHitboxHeight);
				hitbox->originalCollisionPoints[9] = v2(-1.011285 * halfHitboxWidth, 0.143189 * halfHitboxHeight);
				hitbox->originalCollisionPoints[10] = v2(-0.946181 * halfHitboxWidth, -0.400341 * halfHitboxHeight);
				hitbox->originalCollisionPoints[11] = v2(-0.820313 * halfHitboxWidth, -0.651201 * halfHitboxHeight);
			} break;

			case EntityType_text: {
				Messages* messages;
				streamMessages(stream, &messages, &gameState->levelStorage);
				assert(messages);
				addText(gameState, p, messages);
			} break;


			InvalidDefaultCase;
		}
	}

	freeIostream(stream);
}

void freeLevel(GameState* gameState, bool loadingFromCheckpoint) {
	for (s32 entityIndex = 0; entityIndex < gameState->numEntities; entityIndex++) {
		freeEntityAtLevelEnd(gameState->entities + entityIndex, gameState, loadingFromCheckpoint);
	}

	memset(gameState->entityRefs_, 0, sizeof(gameState->entityRefs_));

	gameState->entityRefFreeList = NULL;
	gameState->consoleFreeList = NULL;
	gameState->hitboxFreeList = NULL;
	gameState->refNodeFreeList = NULL;
	gameState->messagesFreeList = NULL;
	gameState->waypointFreeList = NULL;
	gameState->fadingOutConsoles = NULL;
	gameState->targetRefs = NULL;
	gameState->guardTargetRefs = NULL;
	gameState->timeField = NULL;
	gameState->gravityField = NULL;
	gameState->swapField = NULL;

	if(!loadingFromCheckpoint) {
		gameState->checkPointSaveList = NULL;
		gameState->checkPointStorage.allocated = 0;
	}

	gameState->consoleEntityRef = 0;
	gameState->playerRef = 0;
	gameState->numEntities = 0;
	gameState->fieldSpec.hackEnergy = 0;
	gameState->levelStorage.allocated = 0;

	gameState->reloadCurrentLevel = false;
	
	gameState->refCount_ = 1; //NOTE: This is for the null reference
}

bool loadCheckPoint(GameState* gameState) {
	CheckPointSave* checkPoint = NULL;

	for(CheckPointSave* save = gameState->checkPointSaveList; save; save = save->next) {
		if(getEntityByRef(gameState, save->ref)) {
			checkPoint = save;
			break;
		}
	}

	if(!checkPoint) return false;

	assert(checkPoint->arena);
	loadCompleteGame(gameState, checkPoint->arena);

	return true;
}

void loadLevel(GameState* gameState, s32* mapFileIndex, bool firstLevelLoad, bool loadNextLevel) {
	//TODO: no need to print the name of every map just to load one map
	char maps[20][100];

	for(s32 i = 0; i < arrayCount(maps); i++) {
		sprintf(maps[i], "level_%d", i + 1);
	}

	if (loadNextLevel) {
		firstLevelLoad = true;
		gameState->loadNextLevel = false;
		(*mapFileIndex)++;
		(*mapFileIndex) %= arrayCount(maps);
	}
	else if(loadCheckPoint(gameState)) {
		return;
	}

	freeLevel(gameState);

	gameState->random = createRandom(RANDOM_MAX / 2);

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

Button createButton(GameState* gameState, AssetId defaultId, V2 p, double buttonHeight) {
	Button result = {};

	result.defaultTex = loadPNGTexture(gameState->renderGroup, defaultId, false);

	double buttonWidth = getAspectRatio(result.defaultTex) * buttonHeight;

	V2 size = v2(buttonWidth, buttonHeight);
	result.renderBounds = rectCenterDiameter(p, size);
	result.clickBounds = rectCenterDiameter(p, size);
	
	result.scale = 1;

	return result;
}

Button createDockButton(GameState* gameState, AssetId defaultId, AssetId clickedId, AssetId hoverId, V2 p, double buttonHeight) {
	Button result = createButton(gameState, defaultId, p, buttonHeight);

	result.clickedTex = loadPNGTexture(gameState->renderGroup, clickedId, false);
	result.hoverTex = loadPNGTexture(gameState->renderGroup, hoverId, false);

	return result;
}

Button createPauseMenuButton(GameState* gameState, AssetId defaultId, V2 p, double buttonHeight) {
	Button result = createButton(gameState, defaultId, p, buttonHeight);
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
	pushTexture(group, tex, scaledRenderBounds, false, false, DrawOrder_gui);

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

	pushTexture(gameState->renderGroup, background, windowBounds, false, false, DrawOrder_gui, false);
	pushTexture(gameState->renderGroup, frame1, windowBounds, false, false, DrawOrder_gui, false, Orientation_0, frame1Color);
	pushTexture(gameState->renderGroup, frame2, windowBounds, false, false, DrawOrder_gui, false, Orientation_0, frame2Color);
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

Music loadMusic(MusicState* state, AssetId id) {
	Music result = {};

	SDL_RWops* readStream = getFilePtrFromMem(state->assets, id, state->arena);
	result.data = Mix_LoadMUS_RW(readStream, SDL_FALSE);

	if(!result.data) {
		fprintf(stderr, "Error loading music: %s\n", Mix_GetError());
		InvalidCodePath;
	}

	const char* s = Mix_GetError();
	return result;
}

void playMusic(Music* music, MusicState* musicState) {
	musicState->menuMusic.playing = false;
	musicState->gameMusic.playing = false;

	Mix_HaltMusic();
	if (music->data) Mix_PlayMusic(music->data, -1); //loop forever
	music->playing = true;
}

void initMusic(MusicState* musicState, GameState* gameState) {
#if PLAY_MUSIC
	musicState->assets = &gameState->assets;
	musicState->arena = &gameState->permanentStorage;

	s32 mixerFlags = MIX_INIT_OGG;
	s32 mixerInitStatus = Mix_Init(mixerFlags);

	if(mixerFlags != mixerInitStatus) {
		fprintf(stderr, "Error initializing SDL_mixer: %s\n", Mix_GetError());
		InvalidCodePath;
	}

	if (Mix_OpenAudio(48000, AUDIO_U16LSB, 2, 1024) < 0) {
		fprintf(stderr, "Error initializing SDL_mixer: %s\n", Mix_GetError());
		InvalidCodePath;
	}

	musicState->menuMusic = loadMusic(musicState, Asset_menuMusic);
	musicState->gameMusic = loadMusic(musicState, Asset_levelMusic_1);
#endif
}

GameState* createGameState(s32 windowWidth, s32 windowHeight) {
	MemoryArena arena_;
	initArena(&arena_, MEGABYTES(8), true);

	GameState* gameState = pushStruct(&arena_, GameState);
	gameState->permanentStorage = arena_;

	initArena(&gameState->levelStorage, MEGABYTES(24), false);
	initArena(&gameState->hackSaveStorage, MEGABYTES(24), false);
	initArena(&gameState->checkPointStorage, MEGABYTES(24), false);

	gameState->pixelsPerMeter = TEMP_PIXELS_PER_METER;
	gameState->windowWidth = windowWidth;
	gameState->windowHeight = windowHeight;
	gameState->windowSize = v2((double)windowWidth, (double)windowHeight) * (1.0 / gameState->pixelsPerMeter);

	gameState->gravity = v2(0, -9.81f);
	gameState->solidGridSquareSize = 0.1;
	gameState->chunkSize = v2(6, 6);

	gameState->texturesCount = 1; //NOTE: 0 is a null texture data
	gameState->animNodesCount = 1; //NOTE: 0 is a null anim data
	gameState->characterAnimsCount = 1; //NOTE: 0 is a null character data
	gameState->glowingTexturesCount = 1; //NOTE: 0 is a null glowing texture

	Assets* assets = &gameState->assets;
	initAssets(assets);

	gameState->renderGroup = createRenderGroup(256 * 1024, &gameState->permanentStorage, gameState->pixelsPerMeter, 
		gameState->windowWidth, gameState->windowHeight, &gameState->camera,
		gameState->textures, &gameState->texturesCount, assets);

	initInputKeyCodes(&gameState->input);

	return gameState;
}

void initFieldSpec(GameState* gameState) {
	FieldSpec* spec = &gameState->fieldSpec;

	spec->consoleFont = loadCachedFont(gameState->renderGroup, Asset_consoleFont, 16, 2, &gameState->permanentStorage);
	spec->attribute = loadPNGTexture(gameState->renderGroup, Asset_attribute, false);
	spec->behaviour = loadPNGTexture(gameState->renderGroup, Asset_behaviour, false);
	spec->valueBackground = loadPNGTexture(gameState->renderGroup, Asset_changerReadout, false);
	spec->leftButtonDefault = loadPNGTexture(gameState->renderGroup, Asset_consoleButtonDefault, false);
	spec->leftButtonClicked = loadPNGTexture(gameState->renderGroup, Asset_consoleButtonClicked, false);
	spec->leftButtonUnavailable = loadPNGTexture(gameState->renderGroup, Asset_consoleButtonUnavailable, false);
	spec->defaultWaypoint = loadPNGTexture(gameState->renderGroup, Asset_defaultWaypoint, false);
	spec->selectedWaypoint = loadPNGTexture(gameState->renderGroup, Asset_selectedWaypoint, false);
	spec->currentWaypoint = loadPNGTexture(gameState->renderGroup, Asset_currentWaypoint, false);
	spec->movedWaypoint = loadPNGTexture(gameState->renderGroup, Asset_movedWaypoint, false);
	spec->defaultWaypointLine = loadPNGTexture(gameState->renderGroup, Asset_defaultWaypointLine, false);
	spec->currentWaypointLine = loadPNGTexture(gameState->renderGroup, Asset_currentWaypointLine, false);
	spec->movedWaypointLine = loadPNGTexture(gameState->renderGroup, Asset_movedWaypointLine, false);
	spec->waypointArrow = loadPNGTexture(gameState->renderGroup, Asset_waypointArrow, false);
	spec->tileHackShield = loadPNGTexture(gameState->renderGroup, Asset_tileHackShield, false);
	spec->cornerTileHackShield = loadPNGTexture(gameState->renderGroup, Asset_cornerTileHackShield, false);
	spec->tileHackArrow = loadPNGTexture(gameState->renderGroup, Asset_tileHackButton, false);
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

	dock->dockTex = loadPNGTexture(gameState->renderGroup, Asset_dock, false);
	dock->subDockTex = loadPNGTexture(gameState->renderGroup, Asset_subDock, false);
	dock->energyBarStencil = loadPNGTexture(gameState->renderGroup, Asset_energyBarStencil, true);
	dock->barCircleTex = loadPNGTexture(gameState->renderGroup, Asset_barEnergy, false);
	dock->gravityTex = loadPNGTexture(gameState->renderGroup, Asset_gravityField, false);
	dock->timeTex = loadPNGTexture(gameState->renderGroup, Asset_timeField, false);
	dock->acceptButton = createDockButton(gameState, Asset_dockAcceptButtonDefault, Asset_dockAcceptButtonClicked, 
				Asset_dockAcceptButtonHover, v2(gameState->windowSize.x * 0.5 + 0.45, gameState->windowSize.y - 1.6), 0.51);
	dock->cancelButton = createDockButton(gameState, Asset_dockCancelButtonDefault, Asset_dockCancelButtonClicked, 
				Asset_dockCancelButtonHover, v2(gameState->windowSize.x * 0.5 - 2.55, gameState->windowSize.y - 1.6), 0.5);
	dock->cancelButton.clickBounds.max.x -= 0.2;
}

void initPauseMenu(GameState* gameState) {
	PauseMenu* pauseMenu = &gameState->pauseMenu;

	RenderGroup* group = gameState->renderGroup;

	pauseMenu->background = loadPNGTexture(group, Asset_pauseMenuBg, false);
	pauseMenu->backgroundAnim = loadAnimation(group, Asset_pauseMenuAnim, 1280, 720, 1.f, true);

	pauseMenu->quit = createPauseMenuButton(gameState, Asset_pauseMenuQuitButton, v2(15.51, 2.62), 1.5);
	pauseMenu->restart = createPauseMenuButton(gameState, Asset_pauseMenuRestartButton, v2(6.54, 4.49), 1.1);
	pauseMenu->resume = createPauseMenuButton(gameState, Asset_pauseMenuResumeButton, v2(3.21, 8.2), 1.18);
	pauseMenu->settings = createPauseMenuButton(gameState, Asset_pauseMenuSettingsButton, v2(12.4, 6.8), 1.08);
}

void initMainMenu(GameState* gameState) {
	MainMenu* mainMenu = &gameState->mainMenu;

	RenderGroup* group = gameState->renderGroup;

	mainMenu->background = loadPNGTexture(group, Asset_mainMenuBg, false);
	mainMenu->backgroundAnim = loadAnimation(group, Asset_mainMenuAnim, 1280, 720, 1.f, true);

	double mainMenuButtonHeight = 0.6;
	mainMenu->play = createPauseMenuButton(gameState, Asset_mainMenuPlayButton, v2(10, 6), mainMenuButtonHeight);
	mainMenu->settings = createPauseMenuButton(gameState, Asset_mainMenuOptionsButton, v2(10.6, 5.2), mainMenuButtonHeight);
	mainMenu->quit = createPauseMenuButton(gameState, Asset_mainMenuQuitButton, v2(12, 3.2), mainMenuButtonHeight);
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
		walkNode->main = loadAnimation(renderGroup, Asset_playerRunning, 256, 256, 0.07f, true);

		AnimNode* standNode = createAnimNode(gameState, &character->stand);
		standNode->main = loadAnimation(renderGroup, Asset_playerStand, 256, 256, 0.09f, true);

		AnimNode* deathNode = createAnimNode(gameState, &character->death);
		deathNode->main = loadAnimation(renderGroup, Asset_playerDeath, 256, 256, 0.06f, true);

		AnimNode* jumpNode = createAnimNode(gameState, &character->jump);

		jumpNode->intro = loadAnimation(renderGroup, Asset_playerJumpingIntro, 256, 256, 0.07f, false);
		jumpNode->main = loadAnimation(renderGroup, Asset_playerJumping, 256, 256, 0.12f, true);
		jumpNode->outro = loadAnimation(renderGroup, Asset_playerJumpingOutro, 256, 256, 0.03f, false);

		AnimNode* hackNode = createAnimNode(gameState, &gameState->playerHack);
		hackNode->intro = loadAnimation(renderGroup, Asset_playerHacking, 256, 256, 0.07f, false);
		hackNode->main = createAnimation(hackNode->intro.frames + (hackNode->intro.numFrames - 1));
		//hackNode->outro = createReversedAnimation(&hackNode->intro);
	}

	{
		CharacterAnim* character = createCharacterAnim(gameState, &gameState->shrikeAnim);

		AnimNode* shoot = createAnimNode(gameState, &character->shoot);
		shoot->main = loadAnimation(renderGroup, Asset_shrikeShoot, 256, 256, 0.04f, true);
		gameState->shootDelay = getAnimationDuration(&shoot->main);
		//TODO: Make shoot animation time per frame be set by the shootDelay

		//AnimNode* stand = createAnimNode(gameState, &character->stand);
		gameState->shrikeStand = loadAnimation(renderGroup, Asset_shrikeStand, 256, 256, 0.07f, true);

		gameState->shrikeBootUp = loadAnimation(renderGroup, Asset_shrikeBootUp, 256, 256, 0.04f, false);
	}

	{
		TrojanImages* trojan = &gameState->trojanImages;

		CharacterAnim* character = createCharacterAnim(gameState, &trojan->trojanAnim);

		AnimNode* shoot = createAnimNode(gameState, &character->shoot);
		shoot->main = loadAnimation(renderGroup, Asset_trojanShoot, 256, 256, 0.04f, true);

		AnimNode* disappear = createAnimNode(gameState, &character->disappear);
		disappear->main = loadAnimation(renderGroup, Asset_trojanDisappear, 256, 256, 0.04f, false);

		AnimNode* stand = createAnimNode(gameState, &character->stand);
		stand->main = createAnimation(loadPNGTexture(renderGroup, Asset_trojanFull));

		trojan->projectile = loadPNGTexture(renderGroup, Asset_trojanBolt);
		createCharacterAnim(gameState, &trojan->projectileDeath);
		AnimNode* projectileDeath = createAnimNode(gameState, &trojan->projectileDeath->death);
		projectileDeath->main = loadAnimation(renderGroup, Asset_trojanBoltDeath, 128, 128, 0.05f, false);
	}
	
	gameState->hackEnergyAnim = loadAnimation(renderGroup, Asset_energy, 173, 172, 0.08f, true);
	gameState->endPortal = loadPNGTexture(renderGroup, Asset_endPortal);

	{
		LaserImages* laser = &gameState->laserImages;

		laser->baseOff = loadPNGTexture(renderGroup, Asset_virus3BaseOff);
		laser->baseOn = loadPNGTexture(renderGroup, Asset_virus3BaseOn);
		laser->topOff = loadPNGTexture(renderGroup, Asset_virus3TopOff);
		laser->topOn = loadPNGTexture(renderGroup, Asset_virus3TopOn);
		laser->beam = loadPNGTexture(renderGroup, Asset_virus3LaserBeam);
	}

	{
		MotherShipImages* motherShip = &gameState->motherShipImages;

		motherShip->emitter = loadPNGTexture(renderGroup, Asset_motherShipEmitter);
		motherShip->base = loadPNGTexture(renderGroup, Asset_motherShipBase);
		motherShip->rotators[0] = loadPNGTexture(renderGroup, Asset_motherShipRotator3);
		motherShip->rotators[1] = loadPNGTexture(renderGroup, Asset_motherShipRotator1);
		motherShip->rotators[2] = loadPNGTexture(renderGroup, Asset_motherShipRotator2);
		motherShip->projectileMoving = loadAnimation(renderGroup, Asset_motherShipProjectileSmoking, 120, 120, 0.04f, true);
		motherShip->spawning = loadAnimation(renderGroup, Asset_motherShipShoot, 512, 512, 0.04f, true);

		createCharacterAnim(gameState, &motherShip->projectileDeath);
		AnimNode* projectileDeath = createAnimNode(gameState, &motherShip->projectileDeath->death);
		projectileDeath->main = loadAnimation(renderGroup, Asset_motherShipProjectileDeath, 120, 120, 0.04f, false);
	}

	{
		TrawlerImages* trawler = &gameState->trawlerImages;

		trawler->frame = loadPNGTexture(renderGroup, Asset_trawlerRightFrame);
		trawler->body = loadPNGTexture(renderGroup, Asset_trawlerBody);
		trawler->wheel = loadPNGTexture(renderGroup, Asset_trawlerWheel);
		trawler->shoot = loadAnimation(renderGroup, Asset_trawlerShoot, 256, 256, 0.04f, true);
		trawler->bootUp = loadAnimation(renderGroup, Asset_trawlerBootUp, 256, 256, 0.1f, false);

		trawler->projectile = loadPNGTexture(renderGroup, Asset_trawlerBolt);
		createCharacterAnim(gameState, &trawler->projectileDeath);
		AnimNode* projectileDeath = createAnimNode(gameState, &trawler->projectileDeath->death);
		projectileDeath->main = loadAnimation(renderGroup, Asset_trawlerBoltDeath, 128, 128, 0.02f, false);
	}

	{
		CursorImages* cursor = &gameState->cursorImages;

		cursor->regular = loadPNGTexture(renderGroup, Asset_cursorDefault);
		cursor->hacking = loadAnimation(renderGroup, Asset_cursorHacking, 64, 64, 0.04f, true);
	}

	gameState->tileAtlasCount = arrayCount(globalTileData);
	gameState->tileAtlas = gameState->glowingTextures + gameState->glowingTexturesCount;

	for(s32 tileIndex = 0; tileIndex < gameState->tileAtlasCount; tileIndex++) {
		GlowingTexture* tex = createGlowingTexture(gameState);

		tex->regular = loadPNGTexture(renderGroup, globalTileData[tileIndex].regularId);
		tex->glowing = loadPNGTexture(renderGroup, globalTileData[tileIndex].glowingId);
	}

	gameState->lights[0] = loadPNGTexture(renderGroup, Asset_light0);
	gameState->lights[1] = loadPNGTexture(renderGroup, Asset_light1);
	gameState->lightCircle = loadPNGTexture(renderGroup, Asset_circle);
	gameState->lightTriangle = loadPNGTexture(renderGroup, Asset_triangle);

	gameState->checkPointReached = loadPNGTexture(renderGroup, Asset_checkPointReached);
	gameState->checkPointUnreached = loadPNGTexture(renderGroup, Asset_checkPointUnreached);
}

int main(int argc, char* argv[]) {
	s32 windowWidth = 1280, windowHeight = 720;
	SDL_Window* window = createWindow(windowWidth, windowHeight);
	SDL_ShowCursor(0);
	GameState* gameState = createGameState(windowWidth, windowHeight);

	RenderGroup* renderGroup = gameState->renderGroup;
	Input* input = &gameState->input;
	FieldSpec* spec = &gameState->fieldSpec;
	Dock* dock = &gameState->dock;
	PauseMenu* pauseMenu = &gameState->pauseMenu;
	MainMenu* mainMenu = &gameState->mainMenu;
	Camera* camera = &gameState->camera;
	initCamera(camera);

	gameState->textFont = loadTextFont(renderGroup, &gameState->permanentStorage);

	initBackgroundTextures(&gameState->backgroundTextures);

	loadImages(gameState);

	initFieldSpec(gameState);
	initDock(gameState);
	initPauseMenu(gameState);
	initMainMenu(gameState);

	MusicState* musicState = &gameState->musicState;
	initMusic(musicState, gameState);

	s32 mapFileIndex = 0;
	loadLevel(gameState, &mapFileIndex, true, false);

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
			fpsTimer = SDL_GetTicks();
			gameState->fieldSpec.hackEnergy = (frameTime / fps);
			fps = 0;
			frameTime = 0;
		}

		fps++;
		#endif

		glClearColor(0, 0, 0, 1);
		glClear(GL_COLOR_BUFFER_BIT);

		dtForFrame = 1.0 / 60.0;

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
		bool clickHandled = false;

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

				pushTexture(renderGroup, dock->subDockTex, subDockBounds, false, false, DrawOrder_gui, false);

				if (updateAndDrawButton(&dock->acceptButton, renderGroup, input, unpausedDtForFrame)) {
					acceptButtonClicked = true;
				}

				translateButton(&dock->cancelButton, -buttonTranslation);
				translateButton(&dock->acceptButton, -buttonTranslation);

				double energyLoss = getEnergyLoss(gameState);
				char energyLossStr[25];
				sprintf(energyLossStr, "%.2f", energyLoss);
				V2 energyLossTextP = dockCenter + v2(4.19, (minSubDockYOffs - dock->subDockYOffs)-0.11);
				pushText(renderGroup, font, energyLossStr, energyLossTextP, WHITE, TextAlignment_center);

				pushDefaultClipRect(renderGroup);
			}

			pushTexture(renderGroup, dock->dockTex, dockBounds, false, false, DrawOrder_gui, false);

			s32 maxEnergy = 100;
			if(gameState->fieldSpec.hackEnergy > maxEnergy) gameState->fieldSpec.hackEnergy = maxEnergy;
			else if(gameState->fieldSpec.hackEnergy < 0) gameState->fieldSpec.hackEnergy = 0;

			double energy = gameState->fieldSpec.hackEnergy;

			char energyStr[25];
			sprintf(energyStr, "%.2f", energy);

			V2 textP = dockCenter + v2(4.19, -0.74);
			pushText(renderGroup, font, energyStr, textP, WHITE, TextAlignment_center);

			Color energyColor = createColor(66, 217, 255, 50);

			V2 energyBarP = dockCenter + v2(0, -0.13);
			V2 energyBarSize = v2(10.1, 0.5);
			R2 energyBarBounds = rectCenterDiameter(energyBarP, energyBarSize);

			double energyBarPercentage = (double)energy / (double)maxEnergy;
			pushFilledStencil(renderGroup, dock->energyBarStencil, energyBarBounds, energyBarPercentage, energyColor);

			clickHandled = updateConsole(gameState, dtForFrame);

			if(!clickHandled) {
				if(cancelButtonClicked || acceptButtonClicked) {
					gameState->camera.deferredMoveToTarget = true;
					gameState->camera.moveToTarget = true;

					Entity* consoleEntity = getEntityByRef(gameState, gameState->consoleEntityRef);
					fadeOutConsoles(consoleEntity, gameState);

					gameState->consoleEntityRef = 0;

					if(cancelButtonClicked) {
						undoAllHackMoves(gameState);
					}
				}
			}

			if(controlZJustPressed(input) && getEntityByRef(gameState, gameState->consoleEntityRef)) {
				undoLastSaveGame(gameState);
			}

		//NOTE: Draw the dock after the console
			if(gameState->fieldSpec.hackAbilities.globalHacks) {
				V2 topFieldSize = getDrawSize(dock->gravityTex, 0.7);

				V2 gravityP = gameState->gravityField->p + v2(0, 0.48);
				R2 gravityBounds = rectCenterDiameter(gravityP, topFieldSize);

				V2 timeP = gameState->timeField->p + v2(0.3, 0.36);
				R2 timeBounds = rectCenterDiameter(timeP, topFieldSize);

				pushTexture(renderGroup, dock->gravityTex, gravityBounds, false, false, DrawOrder_gui, false);
				pushTexture(renderGroup, dock->timeTex, timeBounds, false, false, DrawOrder_gui, false);

				Color barColor = createColor(191, 16, 16, 255);

				double maxGravityBarLength = 2.2;
				double gravityBarLength = maxGravityBarLength * ((double)gameState->gravityField->selectedIndex / (double)(gameState->gravityField->numValues - 1));
				V2 gravityBarSize = v2(gravityBarLength, 0.07);
				V2 circleSize = v2(1, 1) * 0.085;

				if(gravityBarLength) {
					V2 gravityBarP = gravityP + v2(-0.9, -0.3);
					V2 gravityLeftCircleP = gravityBarP + v2(-0.02, 0.035);

					pushTexture(renderGroup, dock->barCircleTex, rectCenterDiameter(gravityLeftCircleP, circleSize), 
								false, false, DrawOrder_gui, false);
					
					if(gravityBarLength == maxGravityBarLength) {
						V2 gravityRightCircleP = gravityLeftCircleP + v2(gravityBarLength + 0.03, 0);
						pushTexture(renderGroup, dock->barCircleTex, rectCenterDiameter(gravityRightCircleP, circleSize), 
									false, false, DrawOrder_gui, false);
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
					pushTexture(renderGroup, dock->barCircleTex, rectCenterDiameter(timeCircleP, circleSize), 
								false, false, DrawOrder_gui, false);
				}
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

		{
			V2 mouseP = input->mouseInMeters;
			if(gameState->screenType == ScreenType_pause) mouseP = oldInput.mouseInMeters; 
			R2 cursorBounds = rectCenterDiameter(mouseP, v2(1, 1) * 0.5);

			CursorImages* cursorImages = &gameState->cursorImages;
			Texture* cursorImage = NULL;
		
			if(clickHandled) {
				cursorImages->playAnim = true;
				cursorImages->animTime = 0;
			}

			if(cursorImages->playAnim) {
				double duration = getAnimationDuration(&cursorImages->hacking);
				duration += (duration - cursorImages->hacking.secondsPerFrame);

				cursorImages->animTime += dtForFrame;

				if(cursorImages->animTime > duration) {
					cursorImages->animTime = duration - cursorImages->hacking.secondsPerFrame * 0.5;
					cursorImages->playAnim = false;
				}

				cursorImage = getAnimationFrame(&cursorImages->hacking, cursorImages->animTime);
			} else {
				cursorImage = cursorImages->regular;
			}

			pushTexture(renderGroup, cursorImage, cursorBounds, false, false, DrawOrder_gui);
		}
		
		drawRenderGroup(renderGroup, &gameState->fieldSpec);
		removeEntities(gameState);

		{ //NOTE: This updates the camera position
			if(getEntityByRef(gameState, gameState->consoleEntityRef)) {
				V2 ddP = {};

				if(input->up.pressed) ddP.y++;
				if(input->down.pressed) ddP.y--;
				if(input->right.pressed) ddP.x++;
				if(input->left.pressed) ddP.x--;

				camera->dP *= pow(E, -18 * dtForFrame);
				
				if(ddP.x || ddP.y) {
					double acceleration = 250;
					ddP = normalize(ddP) * acceleration;
				}

				V2 velocity = getVelocity(dtForFrame, camera->dP, ddP);
				camera->dP += ddP * dtForFrame;
				camera->newP = camera->p + velocity;

				V2 minCameraP = -gameState->windowSize;
				V2 maxCameraP = maxComponents(gameState->mapSize, gameState->windowSize);

				if(camera->newP.x < minCameraP.x) {
					camera->newP.x = minCameraP.x;
					camera->dP.x = 0;
				}
				else if(camera->newP.x > maxCameraP.x) {
					camera->newP.x = maxCameraP.x;					
					camera->dP.x = 0;
				}

				if(camera->p.y < minCameraP.y) {
					camera->newP.y = minCameraP.y;
					camera->dP.y = 0;
				}
				else if(camera->p.y > maxCameraP.y) {
					camera->newP.y = maxCameraP.y;					
					camera->dP.y = 0;
				}

				camera->moveToTarget = false;
				
			} else {
				camera->dP = v2(0, 0);
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

			double clipRectEpsilon = 0.005;
			screenSpaceClipRect = scaleRect(screenSpaceClipRect, v2(1, 1) * (1 + clipRectEpsilon));

			renderGroup->defaultClipRect = screenSpaceClipRect;
		}

		dtForFrame = 0;

		if(gameState->screenType == ScreenType_game) {
			if (input->n.justPressed) {
				if(!saveFilePath) saveFilePath = getSaveFilePath(saveFileName, &gameState->permanentStorage);
				saveCompleteGame(gameState, saveFilePath);
			}
			if (input->m.justPressed) {
				freeLevel(gameState);
				if(!saveFilePath) saveFilePath = getSaveFilePath(saveFileName, &gameState->permanentStorage);
				loadCompleteGame(gameState, saveFilePath);
			}

			if (input->x.justPressed && false) {
				gameState->fieldSpec.hackEnergy += 10;
			}
		}

		{ //NOTE: This reloads the game
			bool resetLevel = gameState->reloadCurrentLevel || input->r.justPressed || levelResetRequested;
			bool loadNextLevel = gameState->loadNextLevel != 0;

			if(input->c.justPressed) loadNextLevel = true;

			if(resetLevel || loadNextLevel) {
				loadLevel(gameState, &mapFileIndex, false, loadNextLevel);
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