#include "hackformer.h"

#include "hackformer_renderer.cpp"
#include "hackformer_consoleField.cpp"
#include "hackformer_entity.cpp"
#include "hackformer_save.cpp"

bool stringsMatch(char* a, char * b) {
	s32 len = strlen(b);
	bool result = strncmp(a, b, len) == 0;
	return result;
}

char* findFirstStringOccurrence(char* str, s32 strSize, char* subStr, s32 subStrSize) {
	s32 strPos = 0;
	s32 maxStrPos = strSize - subStrSize;

	for (; strPos < maxStrPos; strPos++) {
		if (strncmp(str + strPos, subStr, subStrSize) == 0) {
			return str + strPos;
		}
	}

	return 0;
}

bool extractS32FromLine(char* line, s32 lineLen, char* intName, s32* result) {
	s32 nameLength = strlen(intName);
	char* occurrence = findFirstStringOccurrence(line, lineLen, intName, nameLength);

	if (occurrence) {
		//Need to add two to go past the ="
		occurrence += nameLength + 2;
		*result = atoi(occurrence);
		return true;
	}

	return false;
}

bool extractStringFromLine(char* line, s32 lineLen, char* intName, char* result, s32 maxResultSize) {
	s32 nameLength = strlen(intName);
	char* occurrence = findFirstStringOccurrence(line, lineLen, intName, nameLength);

	if (occurrence) {
		//Need to add two to go past the ="
		occurrence += nameLength + 2;
		s32 resultSize = 0;

		while (*occurrence != '"') {
			result[resultSize] = *occurrence;
			occurrence++;
			resultSize++;
			assert(resultSize < maxResultSize - 1);
		}

		result[resultSize] = 0;
		return true;
	}

	return false;
}

void initSpatialPartition(GameState* gameState) {
	gameState->chunksWidth = (s32)ceil(gameState->mapSize.x / gameState->chunkSize.x);
	gameState->chunksHeight = (s32)ceil(gameState->windowSize.y / gameState->chunkSize.y);

	s32 numChunks = gameState->chunksWidth * gameState->chunksHeight;
	gameState->chunks = pushArray(&gameState->levelStorage, EntityChunk, numChunks);
	zeroSize(gameState->chunks, numChunks * sizeof(EntityChunk));
}

void loadTmxMap(GameState* gameState, char* fileName) {
	FILE* file = fopen(fileName, "r");
	assert(file);

	s32 mapWidthTiles, mapHeightTiles;
	bool foundMapSizeTiles = false;

	//int tileWidth, tileHeight, tileSpacing;
	//bool foundTilesetInfo = false;
	//Texture* textures = NULL;
	//uint numTextures;

	bool loadingTileData = false;
	bool doneLoadingTiles = false;
	s32 tileX, tileY;

	char line[1024];
	char buffer[1024];

	while (fgets (line, sizeof(line), file)) {
		s32 lineLength = strlen(line);

		if (!foundMapSizeTiles) {
			if (extractS32FromLine(line, lineLength, "width", &mapWidthTiles)) {
				extractS32FromLine(line, lineLength, "height", &mapHeightTiles);
				foundMapSizeTiles = true;

				gameState->mapSize = v2((double)mapWidthTiles, (double)mapHeightTiles) * gameState->tileSize;
				gameState->worldSize = v2(max(gameState->mapSize.x, gameState->windowWidth), 
										  max(gameState->mapSize.y, gameState->windowHeight));


				initSpatialPartition(gameState);
			}
		}
		// else if(!foundTilesetInfo) {
		// 	if (extractIntFromLine(line, lineLength, "tilewidth", &tileWidth)) {
		// 		extractIntFromLine(line, lineLength, "tileheight", &tileHeight);
		// 		extractIntFromLine(line, lineLength, "spacing", &tileSpacing);
		// 		foundTilesetInfo = true;
		// 	}
		// }
		// else if(textures == NULL) {
		// 	if (extractStringFromLine(line, lineLength, "image source", buffer, arrayCount(buffer))) {
		// 		int texWidth = 0, texHeight = 0;
		// 		textures = extractTextures(gameState, buffer, tileWidth, tileHeight, tileSpacing, &numTextures );
		// 	}
		// }
		else if(!loadingTileData && !doneLoadingTiles) {
			char* beginLoadingStr = "<data encoding=\"csv\">";
			if (findFirstStringOccurrence(line, lineLength, beginLoadingStr, strlen(beginLoadingStr))) {
				loadingTileData = true;
				tileX = 0;
				tileY = mapHeightTiles - 1;
			}
		}
		else if(loadingTileData) {
			char* endLoadingStr = "</data>";
			if (findFirstStringOccurrence(line, lineLength, endLoadingStr, strlen(endLoadingStr))) {
				loadingTileData = false;
				doneLoadingTiles = true;
			} else {
				char* linePtr = line;
				tileX = 0;

				while(*linePtr) {
					s32 tileIndex = atoi(linePtr) - 1;

					if (tileIndex >= 0) {
						V2 tileP = v2(tileX + 0.5, tileY + 0.5) * gameState->tileSize;
						addTile(gameState, tileP, gameState->tileAtlas[tileIndex]);
					}

					tileX++;

					while(*linePtr && *linePtr != ',') linePtr++;
					linePtr++;
				}

				tileY--;
			}
		}
		else if (doneLoadingTiles) {
			if (extractStringFromLine(line, lineLength, "name", buffer, arrayCount(buffer))) {
				s32 entityX, entityY;
				extractS32FromLine(line, lineLength, "\" x", &entityX);
				extractS32FromLine(line, lineLength, "\" y", &entityY);

				V2 p = v2((double)entityX, (double)entityY) * (gameState->tileSize / 120.0);
				p.y = gameState->windowHeight / gameState->pixelsPerMeter - p.y;

				if (stringsMatch(buffer, "player")) {
					addPlayer(gameState, p);
				}
				else if (stringsMatch(buffer, "blue energy")) {
					addBlueEnergy(gameState, p);
				}
				else if (stringsMatch(buffer, "virus")) {
					addVirus(gameState, p);
				}
				else if (stringsMatch(buffer, "end portal")) {
					addEndPortal(gameState, p);
				}
				else if (stringsMatch(buffer, "flyer")) {
					addFlyingVirus(gameState, p);
				}
				else if (stringsMatch(buffer, "text")) {
					/*

					  <object name="text" x="622" y="658" width="398" height="76">
					   <properties>
					    <property name="0" value="Good Luck!"/>
					    <property name="1" value="Try not to die"/>
					    <property name="2" value="Batman"/>
					    <property name="selected index" value="0"/>
					   </properties>

					*/

					fgets(line, sizeof(line), file);

					s32 selectedIndex = -1;
					char values[10][100];
					s32 numValues = 0;

					while(true) {
						fgets(line, sizeof(line), file);
						if (stringsMatch(line, "   </properties>")) break;

						extractStringFromLine(line, lineLength, "name", buffer, arrayCount(buffer));

						bool selectedIndexString = false;
						s32 valueIndex = -1;
						
						if (stringsMatch(buffer, "selected index")) selectedIndexString = true;
						else valueIndex = atoi(buffer);

						extractStringFromLine(line, lineLength, "value", buffer, arrayCount(buffer));

						if (selectedIndexString) {
							selectedIndex = atoi(buffer);
						} else {
							assert(valueIndex >= 0);

							if (valueIndex > numValues) numValues = valueIndex;

							char* valuePtr = (char*)values[valueIndex];
							char* bufferPtr = (char*)buffer;
							while(*bufferPtr) {
								*valuePtr++ = *bufferPtr++;
							}
							*valuePtr = 0;
						}
					}

					assert(selectedIndex >= 0);
					addText(gameState, p, values, numValues, selectedIndex);
				}
				else if (stringsMatch(buffer, "background")) {
					fgets(line, sizeof(line), file);
					fgets(line, sizeof(line), file);
					extractStringFromLine(line, lineLength, "value", buffer, arrayCount(buffer));

					if (stringsMatch(buffer, "marine")) {
						gameState->bgTex = createTextureFromData(&gameState->marineCityBg, gameState);
						gameState->mgTex = createTextureFromData(&gameState->marineCityMg, gameState);
					}
					else if (stringsMatch(buffer, "sunset")) {
						gameState->bgTex = createTextureFromData(&gameState->sunsetCityBg, gameState);
						gameState->mgTex = createTextureFromData(&gameState->sunsetCityMg, gameState);
					}
					else {
						//NOTE: This means that there was an invalid background type
						assert(false);
					}

					addBackground(gameState);
				}
				else if (stringsMatch(buffer, "laser base")) {
					fgets(line, sizeof(line), file);
					fgets(line, sizeof(line), file);
					extractStringFromLine(line, lineLength, "value", buffer, arrayCount(buffer));
					double height = atof(buffer);

					height = 5;

					addLaserController(gameState, p, height);
				}
			}
		}
	}

	fclose(file);

	// if (gameState->solidGrid) {
	// 	for (int rowIndex = 0; rowIndex < gameState->solidGridWidth; rowIndex++) {
	// 		free(gameState->solidGrid[rowIndex]);
	// 	}

	// 	free(gameState->solidGrid);
	// 	gameState->solidGrid = NULL;
	// }

	//NOTE: This sets up the pathfinding array
	{
		//TODO: This can probably just persist for one frame, not the entire level
		gameState->solidGridWidth = (s32)ceil(gameState->mapSize.x / gameState->solidGridSquareSize);
		gameState->solidGridHeight = (s32)ceil(gameState->windowSize.y / gameState->solidGridSquareSize);

		s32 numNodes = gameState->solidGridWidth * gameState->solidGridHeight;
		gameState->solidGrid = pushArray(&gameState->levelStorage, PathNode, numNodes);
				
		for (s32 rowIndex = 0; rowIndex < gameState->solidGridWidth; rowIndex++) {
		 	for (s32 colIndex = 0; colIndex < gameState->solidGridHeight; colIndex++) {
		 		PathNode* node = gameState->solidGrid + rowIndex * gameState->solidGridHeight + colIndex;

				node->p = v2(rowIndex + 0.5, colIndex + 0.5) * gameState->solidGridSquareSize;
				node->tileX = rowIndex;
				node->tileY = colIndex;
		 	}
		 }
	}
}

void pollInput(GameState* gameState, bool* running) {
	Input* input = &gameState->input;

	input->dMouseMeters = v2(0, 0);

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
				input->mouseInPixels.y = (double)(gameState->windowHeight - mouseY);

				V2 mouseInMeters = input->mouseInPixels * (1.0 / gameState->pixelsPerMeter);

				input->dMouseMeters = mouseInMeters - input->mouseInMeters;

				input->mouseInMeters = mouseInMeters;
				input->mouseInWorld = input->mouseInMeters + gameState->camera.p;

			} break;
			case SDL_MOUSEBUTTONDOWN:
			case SDL_MOUSEBUTTONUP:
				if (event.button.button == SDL_BUTTON_LEFT) {
					if (event.button.state == SDL_PRESSED) {
						input->leftMouse.pressed = input->leftMouse.justPressed = true;
					} else {
						input->leftMouse.pressed = input->leftMouse.justPressed = false;
					}
				}
				break;
		}
	}
}

void freeLevel(GameState* gameState) {
	gameState->targetRefs = NULL;
	gameState->consoleEntityRef = 0;
	gameState->playerRef = 0;

	for (s32 entityIndex = 0; entityIndex < gameState->numEntities; entityIndex++) {
		freeEntity(gameState->entities + entityIndex, gameState, true);
	}

	gameState->numEntities = 0;
	gameState->fieldSpec.blueEnergy = 0;

	for (s32 refIndex = 0; refIndex < arrayCount(gameState->entityRefs_); refIndex++) {
		//freeEntityReference(gameState->entityRefs_ + refIndex, gameState);
		EntityReference* reference = gameState->entityRefs_ + refIndex;
		*reference = {};
	}

	if (gameState->swapField) freeConsoleField(gameState->swapField, gameState);

	gameState->entityRefFreeList = NULL;
	gameState->refCount = 1; //NOTE: This is for the null reference

	gameState->consoleFreeList = NULL;
	gameState->hitboxFreeList = NULL;
	gameState->refNodeFreeList = NULL;
	gameState->messagesFreeList = NULL;

	gameState->levelStorage.allocated = 0;
	gameState->swapField = NULL;

	gameState->camera.scale = 1;
}

void loadLevel(GameState* gameState, char** maps, s32 numMaps, s32* mapFileIndex, bool initPlayerDeath) {
	freeLevel(gameState);

	if (gameState->loadNextLevel) {
		gameState->loadNextLevel = false;
		(*mapFileIndex)++;
		(*mapFileIndex) %= numMaps;
		initPlayerDeath = false;
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

	loadTmxMap(gameState, maps[*mapFileIndex]);
	addFlyingVirus(gameState, v2(7, 6));
	addHeavyTile(gameState, v2(3, 7));

	V2 playerDeathStartP = gameState->playerDeathStartP;
	gameState->doingInitialSim = true;
	gameState->renderGroup->enabled = false;

	double timeStep = 1.0 / 60.0;

	for (s32 frame = 0; frame < 30; frame++) {
		updateAndRenderEntities(gameState, timeStep, false);
	}

	gameState->renderGroup->enabled = true;
	gameState->doingInitialSim = false;
	gameState->playerDeathStartP = playerDeathStartP;

	Entity* player = getEntityByRef(gameState, gameState->playerRef);
	assert(player);

	if(!initPlayerDeath) {
		centerCameraAround(player, gameState);
		gameState->camera.p = gameState->camera.newP;
		gameState->camera.moveToTarget = false;
	} else {
		if (!epsilonEquals(player->p, gameState->playerDeathStartP, 0.1)) {
			setFlags(player, EntityFlag_remove);
			gameState->playerStartP = player->p;

			addPlayerDeath(gameState);
		}
	}
}

MemoryArena createArena(size_t size) {
	MemoryArena result = {};
	result.size = size;
	result.base = (char*)calloc(size, 1);
	assert(result.base);
	return result;
}

Button createButton(GameState* gameState, char* defaultTextureFilePath, V2 p, double buttonHeight) {
	Button result = {};

	result.defaultTex = loadPNGTexture(gameState, defaultTextureFilePath, false);

	double buttonWidth = getAspectRatio(&result.defaultTex) * buttonHeight;

	V2 size = v2(buttonWidth, buttonHeight);
	result.renderBounds = rectCenterDiameter(p, size);
	result.clickBounds = rectCenterDiameter(p, size);
	
	result.scale = 1;

	return result;
}

Button createDockButton(GameState* gameState, char* textureFileName, V2 p, double buttonHeight) {
	char pathBuffer[1024];

	sprintf(pathBuffer, "%s_default", textureFileName);
	Button result = createButton(gameState, pathBuffer, p, buttonHeight);

	sprintf(pathBuffer, "%s_clicked", textureFileName);
	result.clickedTex = loadPNGTexture(gameState, pathBuffer, false);

	sprintf(pathBuffer, "%s_hover", textureFileName);
	result.hoverTex = loadPNGTexture(gameState, pathBuffer, false);

	return result;
}

Button createPauseMenuButton(GameState* gameState, char* textureFilePath, V2 p, double buttonHeight) {
	Button result = createButton(gameState, textureFilePath, p, buttonHeight);
	result.shouldScale = true;

	return result;
}

bool updateAndDrawButton(Button* button, RenderGroup* group, Input* input, double dt) {
	bool clicked = false;

	TextureData* tex = NULL;

	bool mouseInsideButton = pointInsideRect(button->clickBounds, input->mouseInMeters);

	if(input->leftMouse.justPressed) {
		button->selected = mouseInsideButton;
	}

	double scaleSpeed = 2.25 * dt;
	double minScale = 0.825;

	if(button->selected) {
		if(input->leftMouse.pressed) {
			tex = &button->clickedTex;
			button->scale -= scaleSpeed;
		} else {
			button->selected = false;
			clicked = true;
		}
	} 

	if(!button->selected) {
		button->scale += scaleSpeed;

		if(mouseInsideButton) {
			tex = &button->hoverTex;
		} else {
			tex = &button->defaultTex;
		}
	}

	if(button->scale < minScale) button->scale = minScale;
	else if(button->scale > 1) button->scale = 1;

	double scale = button->scale;
	if(!button->shouldScale) scale = 1;

	R2 scaledRenderBounds = scaleRect(button->renderBounds, v2(1, 1) * scale);

	if(!validTexture(tex)) tex = &button->defaultTex;

	assert(validTexture(tex));
	pushTexture(group, tex, scaledRenderBounds, false, DrawOrder_gui);

	return clicked;
}

void translateButton(Button* button, V2 translation) {
	button->clickBounds = translateRect(button->clickBounds, translation);
	button->renderBounds = translateRect(button->renderBounds, translation);
}

int main(int argc, char* argv[]) {
	s32 windowWidth = 1280, windowHeight = 720;

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

#if 1
	{ //Load and play the music
		s32 mixerFlags = MIX_INIT_MP3;
		s32 mixerInitStatus = Mix_Init(mixerFlags);

		if(mixerFlags != mixerInitStatus) {
			fprintf(stderr, "Error initializing SDL_mixer: %s\n", Mix_GetError());
			InvalidCodePath;
		}

		if (Mix_OpenAudio(44100, AUDIO_S16SYS, 2, 1024) < 0) {
		    fprintf(stderr, "Error initializing SDL_mixer: %s\n", Mix_GetError());
		  	InvalidCodePath;
		}

		Mix_Music* music = Mix_LoadMUS("res/hackformer_theme.mp3");
		if(!music) {
		    fprintf(stderr, "Error loading music: %s\n", Mix_GetError());
		  	InvalidCodePath;
		}

		Mix_PlayMusic(music, -1); //loop forever
	}
#endif

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

	MemoryArena arena_ = createArena(1024 * 1024);

	GameState* gameState = pushStruct(&arena_, GameState);
	gameState->permanentStorage = arena_;

	gameState->levelStorage = createArena(8 * 1024 * 1024);
	gameState->hackSaveStorage = createArena(256 * 1024);

	gameState->pixelsPerMeter = 70.0f;
	gameState->windowWidth = windowWidth;
	gameState->windowHeight = windowHeight;
	gameState->windowSize = v2((double)windowWidth, (double)windowHeight) * (1.0 / gameState->pixelsPerMeter);

	gameState->textureDataCount = 1; //NOTE: 0 is a null texture data
	gameState->animDataCount = 1; //NOTE: 0 is a null anim data
	gameState->characterAnimDataCount = 1; //NOTE: 0 is a null character data

	gameState->renderGroup = createRenderGroup(gameState, 32 * 1024);
	RenderGroup* renderGroup = gameState->renderGroup;

	gameState->texel = hadamard(gameState->windowSize, v2(1.0 / windowWidth, 1.0 / windowHeight));

	Input* input = &gameState->input;

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
	input->shift.keyCode1 = SDLK_RSHIFT;
	input->shift.keyCode2 = SDLK_LSHIFT;

	input->pause.keyCode1 = SDLK_p;
	input->pause.keyCode2 = SDLK_ESCAPE;

	input->n.keyCode1 = SDLK_n;
	input->m.keyCode1 = SDLK_m;
	
	gameState->gravity = v2(0, -9.81f);
	gameState->solidGridSquareSize = 0.1;
	gameState->tileSize = 0.9f; 
	gameState->chunkSize = v2(7, 7);

	gameState->textFont = loadFont("fonts/Roboto-Regular.ttf", 64);

	TextureData playerStand = loadPNGTexture(gameState, "player/stand2", false);
	TextureData playerJump = loadPNGTexture(gameState, "player/jump3", false);
	Animation playerStandJumpTransition = loadAnimation(gameState, "player/jump_transition", 256, 256, 0.025f, false);
	Animation playerWalk = loadAnimation(gameState, "player/running_2", 256, 256, 0.0325f, true);
	Animation playerStandWalkTransition = loadAnimation(gameState, "player/stand_run_transition", 256, 256, 0.01f, false);
	Animation playerHackingAnimation = loadAnimation(gameState, "player/hacking_flow_sprite", 512, 256, 0.07f, true);
	Animation playerHackingAnimationTransition = loadAnimation(gameState, "player/hacking_sprite_full_2", 256, 256, 0.025f, false);

	AnimNodeData playerWalkAnimNodeData = {};
	AnimNodeData playerJumpAnimNodeData = {};

	playerJumpAnimNodeData.intro = playerStandJumpTransition;
	playerJumpAnimNodeData.main = createAnimation(&playerJump, gameState);
	playerJumpAnimNodeData.outro = createReversedAnimation(&playerStandJumpTransition);

	playerWalkAnimNodeData.main = playerWalk;
	playerWalkAnimNodeData.intro = playerStandWalkTransition;
	playerWalkAnimNodeData.finishMainBeforeOutro = true;

	AnimNode playerStandAnimNode = createAnimNode(&playerStand, gameState);
	AnimNode playerJumpAnimNode = createAnimNodeFromData(&playerJumpAnimNodeData, gameState);
	AnimNode playerWalkAnimNode = createAnimNodeFromData(&playerWalkAnimNodeData, gameState);

	gameState->playerAnim = createCharacterAnim(gameState, playerStandAnimNode, playerJumpAnimNode, {}, playerWalkAnimNode);
	gameState->playerDeathAnim = createCharacterAnim(gameState, playerStandAnimNode, {}, {}, {});

	AnimNodeData playerHackData = {};
	playerHackData.main = playerHackingAnimation;
	playerHackData.intro = playerHackingAnimationTransition;
	playerHackData.outro = createReversedAnimation(&playerHackingAnimationTransition);

	gameState->playerHack = createAnimNodeFromData(&playerHackData, gameState);

	TextureData virus1Stand = loadPNGTexture(gameState, "virus1/stand");
	Animation virus1Shoot = loadAnimation(gameState, "virus1/shoot", 256, 256, 0.04f, true);
	gameState->shootDelay = getAnimationDuration(&virus1Shoot);

	AnimNode virus1StandAnimNode = createAnimNode(&virus1Stand, gameState);
	AnimNodeData virus1ShootAnimNodeData = {};
	virus1ShootAnimNodeData.main = virus1Shoot;

	AnimNode virus1ShootAnimNode = createAnimNodeFromData(&virus1ShootAnimNodeData, gameState);

	gameState->virus1Anim = createCharacterAnim(gameState, virus1StandAnimNode, {}, virus1ShootAnimNode, {});

	//TODO: Make shoot animation time per frame be set by the shootDelay
	TextureData flyingVirusStand = loadPNGTexture(gameState, "virus2/full");
	Animation flyingVirusShoot = loadAnimation(gameState, "virus2/shoot", 256, 256, 0.04f, true);

	AnimNode flyingVirusStandAnimNode = createAnimNode(&flyingVirusStand, gameState);
	AnimNodeData flyingVirusShootAnimNodeData = {};
	flyingVirusShootAnimNodeData.main = flyingVirusShoot;

	AnimNode flyingVirusShootAnimNode = createAnimNodeFromData(&flyingVirusShootAnimNodeData, gameState);

	gameState->flyingVirusAnim = createCharacterAnim(gameState, flyingVirusStandAnimNode, {}, flyingVirusShootAnimNode, {});


	gameState->sunsetCityBg = loadPNGTexture(gameState, "backgrounds/sunset_city_bg", false);
	gameState->sunsetCityMg = loadPNGTexture(gameState, "backgrounds/sunset_city_mg", false);
	gameState->marineCityBg = loadPNGTexture(gameState, "backgrounds/marine_city_bg", false);
	gameState->marineCityMg = loadPNGTexture(gameState, "backgrounds/marine_city_mg", false);

	gameState->blueEnergyTex = loadTexture(gameState, "blue_energy");
	gameState->laserBolt = loadTexture(gameState, "virus1/laser_bolt", false);
	gameState->endPortal = loadTexture(gameState, "end_portal");

	gameState->laserBaseOff = loadTexture(gameState, "virus3/base_off", false);
	gameState->laserBaseOn = loadTexture(gameState, "virus3/base_on", false);
	gameState->laserTopOff = loadTexture(gameState, "virus3/top_off", false);
	gameState->laserTopOn = loadTexture(gameState, "virus3/top_on", false);
	gameState->laserBeam = loadTexture(gameState, "virus3/laser_beam", false);

	gameState->heavyTileTex = loadTexture(gameState, "Heavy1");

	FieldSpec* spec = &gameState->fieldSpec;

	spec->consoleTriangle = loadPNGTexture(gameState, "triangle_blue", false);
	spec->consoleTriangleSelected = loadPNGTexture(gameState, "triangle_light_blue", false);
	spec->consoleTriangleGrey = loadPNGTexture(gameState, "triangle_grey", false);
	spec->consoleTriangleYellow = loadPNGTexture(gameState, "triangle_yellow", false);
	spec->consoleFont = loadCachedFont(gameState, "fonts/PTS55f.ttf", 16, 2);
	spec->attribute = loadPNGTexture(gameState, "attributes/Attribute", false);
	spec->behaviour = loadPNGTexture(gameState, "attributes/Behaviour", false);
	spec->valueBackground = loadPNGTexture(gameState, "attributes/changer_readout", false);
	spec->leftButtonDefault = loadPNGTexture(gameState, "attributes/left_button", false);
	spec->leftButtonClicked = loadPNGTexture(gameState, "attributes/left_button_clicked", false);
	spec->leftButtonUnavailable = loadPNGTexture(gameState, "attributes/left_button_unavailable", false);
	spec->waypoint = loadPNGTexture(gameState, "waypoint", false);
	spec->waypointArrow = loadPNGTexture(gameState, "waypoint_arrow", false);
	spec->tileHackShield = loadPNGTexture(gameState, "tile_hack_shield", false);

	spec->fieldSize = getDrawSize(&spec->behaviour, 0.5);
	spec->valueSize = getDrawSize(&spec->valueBackground, 0.8);
	spec->valueBackgroundPenetration = 0.4;
	
	spec->triangleSize = getDrawSize(&spec->leftButtonDefault, spec->valueSize.y - spec->valueBackgroundPenetration + spec->fieldSize.y);
	spec->spacing = v2(0.05, 0);
	spec->childInset = spec->fieldSize.x * 0.125;

	Dock* dock = &gameState->dock;
	dock->dockTex = loadPNGTexture(gameState, "dock/dock", false);
	dock->subDockTex = loadPNGTexture(gameState, "dock/sub_dock", false);
	dock->energyBarStencil = loadPNGTexture(gameState, "dock/energy_bar_stencil", false);
	dock->barCircleTex = loadPNGTexture(gameState, "dock/bar_energy", false);
	dock->gravityTex = loadPNGTexture(gameState, "dock/gravity_field", false);
	dock->timeTex = loadPNGTexture(gameState, "dock/time_field", false);
	dock->acceptButton = createDockButton(gameState, "dock/accept_button", v2(gameState->windowSize.x * 0.5 + 0.45, gameState->windowSize.y - 1.6), 0.51);
	dock->cancelButton = createDockButton(gameState, "dock/cancel_button", v2(gameState->windowSize.x * 0.5 - 2.55, gameState->windowSize.y - 1.6), 0.5);
	dock->cancelButton.clickBounds.max.x -= 0.2;

	PauseMenu* pauseMenu = &gameState->pauseMenu;

	pauseMenu->background = loadPNGTexture(gameState, "pause_menu", false);
	pauseMenu->backgroundAnim = loadAnimation(gameState, "pause_menu_sprite", 1280, 720, 1.f, true);

	pauseMenu->quit = createPauseMenuButton(gameState, "quit_button", v2(15.51, 2.62), 1.5);
	pauseMenu->restart = createPauseMenuButton(gameState, "restart_button", v2(6.54, 4.49), 1.1);
	pauseMenu->resume = createPauseMenuButton(gameState, "resume_button", v2(3.21, 8.2), 1.18);
	pauseMenu->settings = createPauseMenuButton(gameState, "settings_button", v2(12.4, 6.8), 1.08);

	gameState->tileAtlas = extractTextures(gameState, "tiles_floored", 120, 240, 12, &gameState->tileAtlasCount, true);

	char* mapFileNames[] = {
		"map3.tmx",
		"map4.tmx",
		"map5.tmx",
	};

	s32 mapFileIndex = 0;
	loadLevel(gameState, mapFileNames, arrayCount(mapFileNames), &mapFileIndex, false);

#ifdef PRINT_FPS
	u32 fpsCounterTimer = SDL_GetTicks();
	u32 fps = 0;
#endif

	bool running = true;
	double dtForFrame = 0;
	double maxDtForFrame = 1.0 / 6.0;
	u32 lastTime = SDL_GetTicks();
	u32 currentTime;

	bool paused = false;

	char* saveFileName = "test_save.txt";

	while (running) {
		currentTime = SDL_GetTicks();
		dtForFrame += (double)((currentTime - lastTime) / 1000.0); 
		lastTime = currentTime;

	#if PRINT_FPS
		u32 frameStart = SDL_GetTicks();

		if (currentTime - fpsCounterTimer > 1000) {
			fpsCounterTimer += 1000;
			printf("Fps: %d\n", fps);
			fps = 0;
		}

		fps++;
	#endif

		glClearColor(0, 0, 0, 1);
		glClear(GL_COLOR_BUFFER_BIT);

		dtForFrame *= gameState->timeField->doubleValues[gameState->timeField->selectedIndex];
		if (dtForFrame > maxDtForFrame) dtForFrame = maxDtForFrame;
		double unpausedDtForFrame = dtForFrame;

		gameState->swapFieldP = gameState->windowSize * 0.5 + v2(0.06, 4.43);

		pollInput(gameState, &running);

		if(input->pause.justPressed) {
			paused = !paused;
			gameState->pauseMenu.animCounter = 0;
			pauseMenu->quit.scale = 1;
			pauseMenu->restart.scale = 1;
			pauseMenu->settings.scale = 1;
			pauseMenu->resume.scale = 1;
		}

		Input oldInput;

		if(paused) {
			gameState->pauseMenu.animCounter += dtForFrame;
			dtForFrame = 0;
			oldInput = gameState->input;
		}

		updateAndRenderEntities(gameState, dtForFrame, paused);
		pushSortEnd(renderGroup);

		bool clickHandled = false;

		{ //Draw the dock before the console
			CachedFont* font = &gameState->fieldSpec.consoleFont;

			V2 dockSize = gameState->windowSize.x * v2(1, dock->dockTex.size.y / dock->dockTex.size.x);
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
				V2 subDockSize = (gameState->windowSize.x * 0.58) * v2(1, dock->subDockTex.size.y / dock->subDockTex.size.x);
				V2 subDockP = v2(gameState->windowSize.x * 0.5 + 0.42, gameState->windowSize.y - maxSubDockYOffs);
				R2 subDockBounds = rectCenterDiameter(subDockP, subDockSize);

				pushClipRect(renderGroup, subDockBounds);

				double subDockTranslation = maxSubDockYOffs - dock->subDockYOffs;
				subDockBounds = translateRect(subDockBounds, v2(0, subDockTranslation));

				V2 buttonTranslation = v2(0, subDockTranslation);
				translateButton(&dock->cancelButton, buttonTranslation);
				translateButton(&dock->acceptButton, buttonTranslation);

				if (updateAndDrawButton(&dock->cancelButton, renderGroup, input, unpausedDtForFrame)) {
					clickHandled = true;
					gameState->consoleEntityRef = 0;
					loadGameFromArena(gameState);
				}

				pushTexture(renderGroup, &dock->subDockTex, subDockBounds, false, DrawOrder_gui, false);

				if (updateAndDrawButton(&dock->acceptButton, renderGroup, input, unpausedDtForFrame)) {
					clickHandled = true;
					gameState->consoleEntityRef = 0;
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

			pushTexture(renderGroup, &dock->dockTex, dockBounds, false, DrawOrder_gui, false);

			s32 maxEnergy = 100;
			if(gameState->fieldSpec.blueEnergy > maxEnergy) gameState->fieldSpec.blueEnergy = maxEnergy;
			else if(gameState->fieldSpec.blueEnergy < 0) gameState->fieldSpec.blueEnergy = 0;

			int energy = gameState->fieldSpec.blueEnergy;

			char energyStr[25];
			sprintf(energyStr, "%d", energy);

			V2 textP = dockCenter + v2(4.19, -0.74);
			pushText(renderGroup, font, energyStr, textP, WHITE, TextAlignment_center);

			Color energyColor = createColor(66, 217, 255, 50);

			V2 energyBarP = dockCenter + v2(0, -0.13);
			V2 energyBarSize = v2(10.1, 0.5);
			R2 energyBarBounds = rectCenterDiameter(energyBarP, energyBarSize);

			double energyBarPercentage = (double)energy / (double)maxEnergy;
			pushFilledStencil(renderGroup, &dock->energyBarStencil, energyBarBounds, energyBarPercentage, energyColor);
		}

		updateConsole(gameState, dtForFrame, paused, clickHandled);

		{ //Draw the dock after the console
			V2 topFieldSize = getDrawSize(&dock->gravityTex, 0.7);

			V2 gravityP = gameState->gravityField->p + v2(0, 0.48);
			R2 gravityBounds = rectCenterDiameter(gravityP, topFieldSize);

			V2 timeP = gameState->timeField->p + v2(0.3, 0.36);
			R2 timeBounds = rectCenterDiameter(timeP, topFieldSize);

			pushTexture(renderGroup, &dock->gravityTex, gravityBounds, false, DrawOrder_gui, false);
			pushTexture(renderGroup, &dock->timeTex, timeBounds, false, DrawOrder_gui, false);

			Color barColor = createColor(191, 16, 16, 255);

			double maxGravityBarLength = 2.2;
			double gravityBarLength = maxGravityBarLength * ((double)gameState->gravityField->selectedIndex / (double)(gameState->gravityField->numValues - 1));
			V2 gravityBarSize = v2(gravityBarLength, 0.07);
			V2 circleSize = v2(1, 1) * 0.085;

			if(gravityBarLength) {
				V2 gravityBarP = gravityP + v2(-0.9, -0.3);
				V2 gravityLeftCircleP = gravityBarP + v2(-0.02, 0.035);

				pushTexture(renderGroup, &dock->barCircleTex, rectCenterDiameter(gravityLeftCircleP, circleSize), false, DrawOrder_gui, false);
				
				if(gravityBarLength == maxGravityBarLength) {
					V2 gravityRightCircleP = gravityLeftCircleP + v2(gravityBarLength + 0.03, 0);
					pushTexture(renderGroup, &dock->barCircleTex, rectCenterDiameter(gravityRightCircleP, circleSize), false, DrawOrder_gui, false);
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
				pushTexture(renderGroup, &dock->barCircleTex, rectCenterDiameter(timeCircleP, circleSize), false, DrawOrder_gui, false);
			}
		}

		bool levelResetRequested = false;

		if(paused) {
			R2 windowBounds = r2(v2(0, 0), gameState->windowSize);

			Texture* frame1 = getAnimationFrame(&pauseMenu->backgroundAnim, pauseMenu->animCounter);
			Texture* frame2 = getAnimationFrame(&pauseMenu->backgroundAnim, pauseMenu->animCounter + pauseMenu->backgroundAnim.secondsPerFrame);

			double frameIndex = pauseMenu->animCounter / pauseMenu->backgroundAnim.secondsPerFrame;
			double frameIndexPercent = frameIndex - (int)frameIndex;

			assert(frameIndexPercent >= 0 && frameIndexPercent <= 1);

			int frame1Alpha = (int)(255 * (1 - frameIndexPercent) + 0.5);
			int frame2Alpha = 255 - frame1Alpha;

			Color frame1Color = createColor(255, 255, 255, frame1Alpha);
			Color frame2Color = createColor(255, 255, 255, frame2Alpha);

			pushTexture(renderGroup, &pauseMenu->background, windowBounds, false, DrawOrder_gui, false);
			pushTexture(renderGroup, frame1, windowBounds, false, DrawOrder_gui, false, Orientation_0, frame1Color);
			pushTexture(renderGroup, frame2, windowBounds, false, DrawOrder_gui, false, Orientation_0, frame2Color);
		
			if (updateAndDrawButton(&pauseMenu->quit, renderGroup, input, unpausedDtForFrame)) {
				running = false;
			}

			if (updateAndDrawButton(&pauseMenu->restart, renderGroup, input, unpausedDtForFrame)) {
				levelResetRequested = true;
				paused = false;
			}

			if (updateAndDrawButton(&pauseMenu->resume, renderGroup, input, unpausedDtForFrame)) {
				paused = false;
			}

			if (updateAndDrawButton(&pauseMenu->settings, renderGroup, input, unpausedDtForFrame)) {
				//TODO: Implement settings
			}
		}

		drawRenderGroup(renderGroup, &gameState->fieldSpec);
		removeEntities(gameState);

		{ //NOTE: This updates the camera position
			Camera* camera = &gameState->camera;

			if(!paused && gameState->consoleEntityRef) {
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

			if(camera->moveToTarget) {
				V2 cameraPDiff = gameState->camera.newP - gameState->camera.p;
				double lenCameraPDiff = length(cameraPDiff);

				double maxCameraMovement = (7.5 + 1.5 * lenCameraPDiff) * dtForFrame;

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

		if (input->n.justPressed) {
			saveGame(gameState, saveFileName);
		}
		if (input->m.justPressed) {
			freeLevel(gameState);
			loadGame(gameState, saveFileName);
		}

		if (input->x.justPressed) {
			gameState->fieldSpec.blueEnergy += 10;
		}

		{ //NOTE: This reloads the game
			bool resetLevel = !getEntityByRef(gameState, gameState->playerDeathRef) &&
							  (!getEntityByRef(gameState, gameState->playerRef) || input->r.justPressed || levelResetRequested);

			
			if (gameState->loadNextLevel || 
				resetLevel) {
				loadLevel(gameState, mapFileNames, arrayCount(mapFileNames), &mapFileIndex, true);
			}
		}

		if(paused) {
			gameState->input = oldInput;
		}

		SDL_GL_SwapWindow(window);
	}

	return 0;
}