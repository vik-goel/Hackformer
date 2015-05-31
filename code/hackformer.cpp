#include "hackformer.h"

#include "hackformer_renderer.cpp"
#include "hackformer_consoleField.cpp"
#include "hackformer_entity.cpp"

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


				//NOTE: This sets up the spatial partition
				{
					gameState->chunksWidth = (s32)ceil(gameState->mapSize.x / gameState->chunkSize.x);
					gameState->chunksHeight = (s32)ceil(gameState->windowSize.y / gameState->chunkSize.y);

					s32 numChunks = gameState->chunksWidth * gameState->chunksHeight;
					gameState->chunks = pushArray(&gameState->levelStorage, EntityChunk, numChunks);
					zeroSize(gameState->chunks, numChunks * sizeof(EntityChunk));
				}
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
						addTile(gameState, tileP, gameState->tileAtlas + tileIndex);
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
						gameState->bgTex = gameState->marineCityBg;
						gameState->mgTex = gameState->marineCityMg;
					}
					else if (stringsMatch(buffer, "sunset")) {
						gameState->bgTex = gameState->sunsetCityBg;
						gameState->mgTex = gameState->sunsetCityMg;
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
		Input* input = &gameState->input;

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

void loadLevel(GameState* gameState, char** maps, s32 numMaps, s32* mapFileIndex, bool initPlayerDeath) {
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

	if (gameState->loadNextLevel) {
		gameState->loadNextLevel = false;
		(*mapFileIndex)++;
		(*mapFileIndex) %= numMaps;
		initPlayerDeath = false;
	}

	{
		double gravityValues[] = {-9.8, -4.9, 0, 4.9, 9.8};
		s32 gravityFieldModifyCost = 10;
		gameState->gravityField = createPrimitiveField(double, gameState, "gravity", gravityValues, arrayCount(gravityValues), 0, gravityFieldModifyCost);
		gameState->gravityField->p = v2(2.25, gameState->windowSize.y - 0.8);
	}

	{
		double timeValues[] = {0, 0.2, 0.5, 1, 2};
		s32 timeFieldModifyCost = 10;
		gameState->timeField = createPrimitiveField(double, gameState, "time", timeValues, arrayCount(timeValues), 3, timeFieldModifyCost);
		gameState->timeField->p = v2(gameState->windowSize.x - 2.5, gameState->windowSize.y - 0.8);
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

	gameState->camera.scale = 1;

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

Button createPauseMenuButton(GameState* gameState, char* textureFilePath, V2 p, double buttonHeight) {
	Button result = {};

	result.texture = loadPNGTexture(gameState, textureFilePath, false);

	double buttonWidth = getAspectRatio(&result.texture) * buttonHeight;

	V2 size = v2(buttonWidth, buttonHeight);
	result.bounds = rectCenterDiameter(p, size);
	result.scale = 1;
	result.selected = false;

	return result;
}

bool updateAndDrawButton(Button* button, RenderGroup* group, Input* input, double dt) {
	bool clicked = false;

	if(input->leftMouse.justPressed) {
		button->selected = pointInsideRect(button->bounds, input->mouseInMeters);
	}

	double scaleSpeed = 2.25 * dt;
	double minScale = 0.825;

	if(button->selected) {
		if(input->leftMouse.pressed) {
			button->scale -= scaleSpeed;
		} else {
			button->selected = false;
			clicked = true;
		}
	} 

	if(!button->selected) {
		button->scale += scaleSpeed;
	}

	if(button->scale < minScale) button->scale = minScale;
	else if(button->scale > 1) button->scale = 1;

	R2 scaledBounds = scaleRect(button->bounds, v2(1, 1) * button->scale);

	pushTexture(group, &button->texture, scaledBounds, false, DrawOrder_gui);

	return clicked;
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

		Mix_Music* music = Mix_LoadMUS("res/hackformer theme.mp3");
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

	gameState->pixelsPerMeter = 70.0f;
	gameState->windowWidth = windowWidth;
	gameState->windowHeight = windowHeight;
	gameState->windowSize = v2((double)windowWidth, (double)windowHeight) * (1.0 / gameState->pixelsPerMeter);

	gameState->renderGroup = createRenderGroup(gameState, 32 * 1024);
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
	
	gameState->gravity = v2(0, -9.81f);
	gameState->solidGridSquareSize = 0.1;
	gameState->tileSize = 0.9f; 
	gameState->chunkSize = v2(7, 7);

	gameState->textFont = loadFont("fonts/Roboto-Regular.ttf", 64);

	Texture playerStand = loadPNGTexture(gameState, "res/player/stand2");
	Texture playerJump = loadPNGTexture(gameState, "res/player/jump3");
	Animation playerStandJumpTransition = loadAnimation(gameState, "res/player/jump_transition", 256, 256, 0.025f, false);
	Animation playerWalk = loadAnimation(gameState, "res/player/running 2", 256, 256, 0.0325f, true);
	Animation playerStandWalkTransition = loadAnimation(gameState, "res/player/stand_run_transition", 256, 256, 0.01f, false);
	Animation playerHackingAnimation = loadAnimation(gameState, "res/player/Hacking Flow Sprite", 512, 256, 0.07f, true);
	Animation playerHackingAnimationTransition = loadAnimation(gameState, "res/player/Hacking Sprite Full 2", 256, 256, 0.025f, false);

	AnimNode playerStandAnimNode = createAnimNode(&playerStand);
	AnimNode playerJumpAnimNode = {}, playerWalkAnimNode = {};

	playerJumpAnimNode.intro = playerStandJumpTransition;
	playerJumpAnimNode.main = createAnimation(&playerJump);
	playerJumpAnimNode.outro = createReversedAnimation(&playerStandJumpTransition);

	playerWalkAnimNode.main = playerWalk;
	playerWalkAnimNode.intro = playerStandWalkTransition;
	playerWalkAnimNode.finishMainBeforeOutro = true;

	gameState->playerAnim = createCharacterAnim(&playerStandAnimNode, &playerJumpAnimNode, NULL, &playerWalkAnimNode);
	gameState->playerDeathAnim = createCharacterAnim(&playerStandAnimNode, NULL, NULL, NULL);

	gameState->playerHack.main = playerHackingAnimation;
	gameState->playerHack.intro = playerHackingAnimationTransition;
	gameState->playerHack.outro = createReversedAnimation(&playerHackingAnimationTransition);

	Texture virus1Stand = loadPNGTexture(gameState, "res/virus1/stand");
	Animation virus1Shoot = loadAnimation(gameState, "res/virus1/shoot", 256, 256, 0.04f, true);
	gameState->shootDelay = getAnimationDuration(&virus1Shoot);

	AnimNode virus1StandAnimNode = createAnimNode(&virus1Stand);
	AnimNode virus1ShootAnimNode = {};
	virus1ShootAnimNode.main = virus1Shoot;

	gameState->virus1Anim = createCharacterAnim(&virus1StandAnimNode, NULL, &virus1ShootAnimNode, NULL);

	//TODO: Make shoot animation time per frame be set by the shootDelay
	Texture flyingVirusStand = loadPNGTexture(gameState, "res/virus2/full");
	Animation flyingVirusShoot = loadAnimation(gameState, "res/virus2/shoot", 256, 256, 0.04f, true);

	AnimNode flyingVirusStandAnimNode = createAnimNode(&flyingVirusStand);
	AnimNode flyingVirusShootAnimNode = {};
	flyingVirusShootAnimNode.main = flyingVirusShoot;

	gameState->flyingVirusAnim = createCharacterAnim(&flyingVirusStandAnimNode, NULL, &flyingVirusShootAnimNode, NULL);


	gameState->sunsetCityBg = loadPNGTexture(gameState, "res/backgrounds/sunset city bg", false);
	gameState->sunsetCityMg = loadPNGTexture(gameState, "res/backgrounds/sunset city mg", false);
	gameState->marineCityBg = loadPNGTexture(gameState, "res/backgrounds/marine city bg", false);
	gameState->marineCityMg = loadPNGTexture(gameState, "res/backgrounds/marine city mg", false);

	gameState->blueEnergyTex = loadPNGTexture(gameState, "res/blue energy");
	gameState->laserBolt = loadPNGTexture(gameState, "res/virus1/laser bolt", false);
	gameState->endPortal = loadPNGTexture(gameState, "res/end portal");

	gameState->laserBaseOff = loadPNGTexture(gameState, "res/virus3/base off");
	gameState->laserBaseOn = loadPNGTexture(gameState, "res/virus3/base on");
	gameState->laserTopOff = loadPNGTexture(gameState, "res/virus3/top off");
	gameState->laserTopOn = loadPNGTexture(gameState, "res/virus3/top on");
	gameState->laserBeam = loadPNGTexture(gameState, "res/virus3/laser beam");

	gameState->heavyTileTex = loadPNGTexture(gameState, "res/Heavy1");

	FieldSpec* spec = &gameState->fieldSpec;

	spec->consoleTriangle = loadPNGTexture(gameState, "res/triangle_blue");
	spec->consoleTriangleSelected = loadPNGTexture(gameState, "res/triangle_light_blue");
	spec->consoleTriangleGrey = loadPNGTexture(gameState, "res/triangle_grey");
	spec->consoleTriangleYellow = loadPNGTexture(gameState, "res/triangle_yellow");
	spec->consoleFont = loadCachedFont(gameState, "fonts/PTS55f.ttf", 16, 2);
	spec->attribute = loadPNGTexture(gameState, "res/attributes/Attribute", false);
	spec->behaviour = loadPNGTexture(gameState, "res/attributes/Behaviour", false);
	spec->valueBackground = loadPNGTexture(gameState, "res/attributes/Changer Readout", false);
	spec->leftButtonDefault = loadPNGTexture(gameState, "res/attributes/Left Button", false);
	spec->leftButtonClicked = loadPNGTexture(gameState, "res/attributes/Left Button Clicked", false);
	spec->leftButtonUnavailable = loadPNGTexture(gameState, "res/attributes/Left Button Unavailable", false);
	spec->waypoint = loadPNGTexture(gameState, "res/waypoint", false);
	spec->waypointArrow = loadPNGTexture(gameState, "res/waypoint_arrow", false);
	spec->tileHackShield = loadPNGTexture(gameState, "res/Tile Hack Shield", false);

	spec->fieldSize = getDrawSize(&spec->behaviour, 0.5);
	spec->valueSize = getDrawSize(&spec->valueBackground, 0.8);
	spec->valueBackgroundPenetration = 0.4;
	
	spec->triangleSize = getDrawSize(&spec->leftButtonDefault, spec->valueSize.y - spec->valueBackgroundPenetration + spec->fieldSize.y);
	spec->spacing = v2(0.05, 0);
	spec->childInset = spec->fieldSize.x * 0.125;

	gameState->dock = loadPNGTexture(gameState, "res/dock 2", false);
	gameState->dockBlueEnergyTile = loadPNGTexture(gameState, "res/Blue Energy Tile", false);

	PauseMenu* pauseMenu = &gameState->pauseMenu;

	pauseMenu->background = loadPNGTexture(gameState, "res/Pause Menu", false);
	pauseMenu->backgroundAnim = loadAnimation(gameState, "res/Pause Menu Sprite", 1280, 720, 1.f, true);

	pauseMenu->quit = createPauseMenuButton(gameState, "res/Quit Button", v2(15.51, 2.62), 1.5);
	pauseMenu->restart = createPauseMenuButton(gameState, "res/Restart Button", v2(6.54, 4.49), 1.1);
	pauseMenu->resume = createPauseMenuButton(gameState, "res/Resume Button", v2(3.21, 8.2), 1.18);
	pauseMenu->settings = createPauseMenuButton(gameState, "res/Settings Button", v2(12.4, 6.8), 1.08);

	u32 numTilesInAtlas;
	gameState->tileAtlas = extractTextures(gameState, "res/tiles_floored", 120, 240, 12, &numTilesInAtlas);

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

		gameState->swapFieldP = gameState->windowSize * 0.5 + v2(0.08, 4.43);

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
		pushSortEnd(gameState->renderGroup);

		{ //Draw the dock
			V2 size = gameState->windowSize.x * v2(1, gameState->dock.size.y / gameState->dock.size.x);
			V2 dockP = v2(0, gameState->windowSize.y - size.y);
			R2 bounds = r2(dockP, dockP + size);
			pushTexture(gameState->renderGroup, &gameState->dock, bounds, false, DrawOrder_gui, false);

			s32 maxBlueEnergy = 60;
			if(gameState->fieldSpec.blueEnergy > maxBlueEnergy) gameState->fieldSpec.blueEnergy = maxBlueEnergy;
			else if(gameState->fieldSpec.blueEnergy < 0) gameState->fieldSpec.blueEnergy = 0;

			int blueEnergy = gameState->fieldSpec.blueEnergy;

			if(blueEnergy > 0) {
				char blueEnergyStr[25];
				sprintf(blueEnergyStr, "%d", blueEnergy);

				CachedFont* font = &gameState->fieldSpec.consoleFont;
				double textWidth = getTextWidth(font, gameState->renderGroup, blueEnergyStr);
				R2 textPadding = getTextPadding(font, gameState->renderGroup, blueEnergyStr);

				V2 blueEnergyP = dockP + v2(1.84, 0.6);
				Color blue = createColor(66, 217, 255, 255);
				V2 blueEnergySize = v2(1, 1) * 0.49;

				R2 blueEnergyBounds = rectCenterDiameter(blueEnergyP, blueEnergySize);
				pushTexture(gameState->renderGroup, &gameState->dockBlueEnergyTile, blueEnergyBounds, false, DrawOrder_gui, false);
				
				V2 textP = blueEnergyP - blueEnergySize * 0.5 - textPadding.min;
				textP += (blueEnergySize - v2(textWidth, font->lineHeight)) * 0.5;
				pushText(gameState->renderGroup, font, blueEnergyStr, textP);

				if(blueEnergy > 1) {
					double rectHeight = 0.15;
					V2 rectOffs = v2(0.22, -0.03);
					double maxRectLength = 6.32;
					double rectLength = maxRectLength * (double)blueEnergy/(double)(maxBlueEnergy - 2);
					if(rectLength > maxRectLength) rectLength = maxRectLength;

					V2 rectStartP = blueEnergyP + rectOffs + v2(0, -rectHeight / 2);
					V2 rectEndP = rectStartP + v2(rectLength, rectHeight);

					R2 rectBounds = r2(rectStartP, rectEndP);
					pushFilledRect(gameState->renderGroup, rectBounds, blue);

					if(blueEnergy >= maxBlueEnergy) {
						blueEnergyP += v2(maxRectLength + 0.02, 0) + rectOffs;
						blueEnergyBounds = rectCenterDiameter(blueEnergyP, 0.16 * v2(1, 1));
						pushTexture(gameState->renderGroup, &gameState->dockBlueEnergyTile, blueEnergyBounds, false, DrawOrder_gui, false);
					}
				}
			}
		}

		updateConsole(gameState, dtForFrame, paused);

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

			pushTexture(gameState->renderGroup, &pauseMenu->background, windowBounds, false, DrawOrder_gui, false);
			pushTexture(gameState->renderGroup, frame1, windowBounds, false, DrawOrder_gui, false, Orientation_0, frame1Color);
			pushTexture(gameState->renderGroup, frame2, windowBounds, false, DrawOrder_gui, false, Orientation_0, frame2Color);
		
			if (updateAndDrawButton(&pauseMenu->quit, gameState->renderGroup, input, unpausedDtForFrame)) {
				running = false;
			}

			if (updateAndDrawButton(&pauseMenu->restart, gameState->renderGroup, input, unpausedDtForFrame)) {
				levelResetRequested = true;
				paused = false;
			}

			if (updateAndDrawButton(&pauseMenu->resume, gameState->renderGroup, input, unpausedDtForFrame)) {
				paused = false;
			}

			if (updateAndDrawButton(&pauseMenu->settings, gameState->renderGroup, input, unpausedDtForFrame)) {
				//TODO: Implement settings
			}
		}

		drawRenderGroup(gameState->renderGroup, &gameState->fieldSpec);
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

			gameState->renderGroup->defaultClipRect = screenSpaceClipRect;
		}

		dtForFrame = 0;

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