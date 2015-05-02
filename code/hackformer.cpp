#include "hackformer.h"

#include "hackformer_renderer.cpp"
#include "hackformer_consoleField.cpp"
#include "hackformer_entity.cpp"

bool stringsMatch(char* a, char * b) {
	int len = strlen(b);
	bool result = strncmp(a, b, len) == 0;
	return result;
}

char* findFirstStringOccurrence(char* str, int strSize, char* subStr, int subStrSize) {
	int strPos = 0;
	int maxStrPos = strSize - subStrSize;

	for (; strPos < maxStrPos; strPos++) {
		if (strncmp(str + strPos, subStr, subStrSize) == 0) {
			return str + strPos;
		}
	}

	return 0;
}

bool extractIntFromLine(char* line, int lineLen, char* intName, int* result) {
	int nameLength = strlen(intName);
	char* occurrence = findFirstStringOccurrence(line, lineLen, intName, nameLength);

	if (occurrence) {
		//Need to add two to go past the ="
		occurrence += nameLength + 2;
		*result = atoi(occurrence);
		return true;
	}

	return false;
}

bool extractStringFromLine(char* line, int lineLen, char* intName, char* result, int maxResultSize) {
	int nameLength = strlen(intName);
	char* occurrence = findFirstStringOccurrence(line, lineLen, intName, nameLength);

	if (occurrence) {
		//Need to add two to go past the ="
		occurrence += nameLength + 2;
		int resultSize = 0;

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
	FILE* file;
	fopen_s(&file, fileName, "r");
	assert(file);

	int mapWidthTiles, mapHeightTiles;
	bool foundMapSizeTiles = false;

	//int tileWidth, tileHeight, tileSpacing;
	//bool foundTilesetInfo = false;
	//Texture* textures = NULL;
	//uint numTextures;

	bool loadingTileData = false;
	bool doneLoadingTiles = false;
	int tileX, tileY;

	char line[1024];
	char buffer[1024];

	while (fgets (line, sizeof(line), file)) {
		int lineLength = strlen(line);

		if (!foundMapSizeTiles) {
			if (extractIntFromLine(line, lineLength, "width", &mapWidthTiles)) {
				extractIntFromLine(line, lineLength, "height", &mapHeightTiles);
				foundMapSizeTiles = true;

				gameState->mapSize = v2((double)mapWidthTiles, (double)mapHeightTiles) * gameState->tileSize;
				gameState->worldSize = v2(max(gameState->mapSize.x, gameState->windowWidth), 
										  max(gameState->mapSize.y, gameState->windowHeight));


				//NOTE: This sets up the spatial partition
				{
					gameState->chunksWidth = (int)ceil(gameState->mapSize.x / gameState->chunkSize.x);
					gameState->chunksHeight = (int)ceil(gameState->windowSize.y / gameState->chunkSize.y);

					int numChunks = gameState->chunksWidth * gameState->chunksHeight;
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
					int tileIndex = atoi(linePtr) - 1;

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
				int entityX, entityY;
				extractIntFromLine(line, lineLength, "\" x", &entityX);
				extractIntFromLine(line, lineLength, "\" y", &entityY);

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

					int selectedIndex = -1;
					char values[10][100];
					int numValues = 0;

					while(true) {
						fgets(line, sizeof(line), file);
						if (stringsMatch(line, "   </properties>")) break;

						extractStringFromLine(line, lineLength, "name", buffer, arrayCount(buffer));

						bool selectedIndexString = false;
						int valueIndex = -1;
						
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
		gameState->solidGridWidth = (int)ceil(gameState->mapSize.x / gameState->solidGridSquareSize);
		gameState->solidGridHeight = (int)ceil(gameState->windowSize.y / gameState->solidGridSquareSize);

		int numNodes = gameState->solidGridWidth * gameState->solidGridHeight;
		gameState->solidGrid = pushArray(&gameState->levelStorage, PathNode, numNodes);
				
		for (int rowIndex = 0; rowIndex < gameState->solidGridWidth; rowIndex++) {
		 	for (int colIndex = 0; colIndex < gameState->solidGridHeight; colIndex++) {
		 		PathNode* node = gameState->solidGrid + rowIndex * gameState->solidGridHeight + colIndex;

				node->p = v2(rowIndex + 0.5, colIndex + 0.5) * gameState->solidGridSquareSize;
				node->tileX = rowIndex;
				node->tileY = colIndex;
		 	}
		 }
	}
}

void pollInput(GameState* gameState, bool* running) {
	gameState->input.dMouseMeters = v2(0, 0);
	gameState->input.rJustPressed = false;
	gameState->input.upJustPressed = false;
	gameState->input.xJustPressed = false;

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
				SDL_Keycode key = event.key.keysym.sym;

				if (key == SDLK_w || key == SDLK_UP) {
					if (pressed && !input->upPressed) input->upJustPressed = true;
					input->upPressed = pressed;
				}

				if (key == SDLK_r) {
					if (pressed && !input->rPressed) input->rJustPressed = true;
					input->rPressed = pressed;
				}

				if (key == SDLK_x) {
					if (pressed && !input->xPressed) input->xJustPressed = true;
					input->xPressed = pressed;
				}

				else if (key == SDLK_a || key == SDLK_LEFT) input->leftPressed = pressed;
				else if (key == SDLK_d || key == SDLK_RIGHT) input->rightPressed = pressed;
			} break;
			case SDL_MOUSEMOTION: {
				int mouseX = event.motion.x;
				int mouseY = event.motion.y;

				input->mouseInPixels.x = (double)mouseX;
				input->mouseInPixels.y = (double)(gameState->windowHeight - mouseY);

				V2 mouseInMeters = input->mouseInPixels * (1.0 / gameState->pixelsPerMeter);

				input->dMouseMeters = mouseInMeters - input->mouseInMeters;

				input->mouseInMeters = mouseInMeters;
				input->mouseInWorld = input->mouseInMeters + gameState->cameraP;

			} break;
			case SDL_MOUSEBUTTONDOWN:
			case SDL_MOUSEBUTTONUP:
				if (event.button.button == SDL_BUTTON_LEFT) {
					if (event.button.state == SDL_PRESSED) {
						input->leftMousePressed = input->leftMouseJustPressed = true;
					} else {
						input->leftMousePressed = input->leftMouseJustPressed = false;
					}
				}
				break;
		}
	}
}

void loadLevel(GameState* gameState, char** maps, int numMaps, int* mapFileIndex, bool initPlayerDeath) {
	gameState->shootTargetRef = 0;
	gameState->consoleEntityRef = 0;
	gameState->playerRef = 0;

	for (int entityIndex = 0; entityIndex < gameState->numEntities; entityIndex++) {
		freeEntity(gameState->entities + entityIndex, gameState);
	}

	gameState->numEntities = 0;
	gameState->blueEnergy = 0;

	for (int refIndex = 0; refIndex < arrayCount(gameState->entityRefs_); refIndex++) {
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

	gameState->levelStorage.allocated = 0;
	gameState->swapField = NULL;

	if (gameState->loadNextLevel) {
		gameState->loadNextLevel = false;
		(*mapFileIndex)++;
		(*mapFileIndex) %= numMaps;
		initPlayerDeath = false;
		gameState->cameraP = gameState->newCameraP = v2(0, 0);
	}

	loadTmxMap(gameState, maps[*mapFileIndex]);
	addFlyingVirus(gameState, v2(7, 6));
	addHeavyTile(gameState, v2(3, 7));

	V2 playerDeathStartP = gameState->playerDeathStartP;
	gameState->doingInitialSim = true;
	gameState->renderGroup->enabled = false;

	double timeStep = 1.0 / 60.0;

	for (int frame = 0; frame < 30; frame++) {
		updateAndRenderEntities(gameState, timeStep);
	}

	gameState->renderGroup->enabled = true;
	gameState->doingInitialSim = false;
	gameState->playerDeathStartP = playerDeathStartP;

	if (gameState->playerDeathSize.x && initPlayerDeath) {
		Entity* player = getEntityByRef(gameState, gameState->playerRef);
		assert(player);

		if (!epsilonEquals(player->p, gameState->playerDeathStartP, 0.1)) {
			setFlags(player, EntityFlag_remove);
			gameState->playerStartP = player->p;

			addPlayerDeath(gameState);
		}
	}
}

MemoryArena createArena(int size) {
	MemoryArena result = {};
	result.size = size;
	result.base = (char*)calloc(size, 1);
	assert(result.base);
	return result;
}

int main(int argc, char *argv[]) {
	int windowWidth = 1280, windowHeight = 720;

	//TODO: Proper error handling if any of these libraries does not load
	//TODO: Only initialize what is needed
	if (SDL_Init(SDL_INIT_EVERYTHING) < 0) {
		fprintf(stderr, "Failed to initialize SDL. Error: %s", SDL_GetError());
		assert(false);
	}

	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
	SDL_SetHint(SDL_HINT_RENDER_VSYNC, "1");

	if (IMG_Init(IMG_INIT_PNG) != IMG_INIT_PNG) {
		fprintf(stderr, "Failed to initialize SDL Image.");
		assert(false);
	}

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

	SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);

	// SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1); 
	// SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);

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
		assert(false);
	}

	if (!window) {
		fprintf(stderr, "Failed to create window. Error: %s", SDL_GetError());
		assert(false);
	}

	SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);
    if(!renderer) {
        fprintf(stderr, "Failed to create renderer. Error: %s", SDL_GetError() );
        assert(false);
    }

	if (TTF_Init()) {
		fprintf(stderr, "Failed to initialize SDL_ttf.");
		assert(false);
	}

	MemoryArena arena_ = createArena(1024 * 1024);

	GameState* gameState = pushStruct(&arena_, GameState);
	gameState->permanentStorage = arena_;
	gameState->renderer = renderer;

	gameState->levelStorage = createArena(8 * 1024 * 1024);

	gameState->pixelsPerMeter = 70.0f;
	gameState->windowWidth = windowWidth;
	gameState->windowHeight = windowHeight;
	gameState->windowSize = v2((double)windowWidth, (double)windowHeight) * (1.0 / gameState->pixelsPerMeter);

	gameState->renderGroup = createRenderGroup(gameState, 32 * 1024);
	gameState->texel = hadamard(gameState->windowSize, v2(1.0 / windowWidth, 1.0 / windowHeight));
//
//	gameState->basicShader = createShader("shaders/basic.vert", "shaders/basic.frag", gameState->windowSize);

	gameState->gravity = v2(0, -9.81f);
	gameState->solidGridSquareSize = 0.1;
	gameState->tileSize = 0.9f; 
	gameState->chunkSize = v2(7, 7);

	gameState->consoleFont = loadCachedFont(gameState, "fonts/PTS55f.ttf", 16, 2);
	gameState->textFont = loadFont("fonts/Roboto-Regular.ttf", 64);

	Texture playerStand = loadPNGTexture(gameState, "res/player/stand2");
	Texture playerJump = loadPNGTexture(gameState, "res/player/jump2");
	Animation playerWalk = loadAnimation(gameState, "res/player/running 2", 256, 256, 0.0325f, true);
	Animation playerStandWalkTransition = loadAnimation(gameState, "res/player/stand_run_transition", 256, 256, 0.01f, false);
	Animation playerHackingAnimation = loadAnimation(gameState, "res/player/Hacking Flow Sprite", 512, 256, 0.07f, false);
	Animation playerHackingAnimationTransition = loadAnimation(gameState, "res/player/Hacking Sprite Full 2", 256, 256, 0.025f, false);

	gameState->playerStand = createAnimNode(&playerStand);
	gameState->playerJump = createAnimNode(&playerJump);

	gameState->playerWalk.main = playerWalk;
	gameState->playerWalk.intro = playerStandWalkTransition;
	//gameState->playerWalk.outro = createReversedAnimation(&playerStandWalkTransition);
	//gameState->playerWalk.outro.secondsPerFrame *= 2;
	gameState->playerWalk.finishMainBeforeOutro = true;

	gameState->playerHack.main = playerHackingAnimation;
	gameState->playerHack.intro = playerHackingAnimationTransition;
	gameState->playerHack.outro = createReversedAnimation(&playerHackingAnimationTransition);

	Texture virus1Stand = loadPNGTexture(gameState, "res/virus1/stand");
	Animation virus1Shoot = loadAnimation(gameState, "res/virus1/shoot", 256, 256, 0.04f, true);
	gameState->shootDelay = getAnimationDuration(&virus1Shoot);

	gameState->virus1Stand = createAnimNode(&virus1Stand);
	gameState->virus1Shoot.main = virus1Shoot;

	//TODO: Make shoot animation time per frame be set by the shootDelay
	Texture flyingVirusStand = loadPNGTexture(gameState, "res/virus2/full");
	Animation flyingVirusShoot = loadAnimation(gameState, "res/virus2/shoot", 256, 256, 0.04f, true);

	gameState->flyingVirusStand = createAnimNode(&flyingVirusStand);
	gameState->flyingVirusShoot.main = flyingVirusShoot;


	gameState->sunsetCityBg = loadPNGTexture(gameState, "res/backgrounds/sunset city bg", false);
	gameState->sunsetCityMg = loadPNGTexture(gameState, "res/backgrounds/sunset city mg", false);
	gameState->marineCityBg = loadPNGTexture(gameState, "res/backgrounds/marine city bg", false);
	gameState->marineCityMg = loadPNGTexture(gameState, "res/backgrounds/marine city mg", false);

	gameState->blueEnergyTex = loadPNGTexture(gameState, "res/blue energy");
	gameState->laserBolt = loadPNGTexture(gameState, "res/virus1/laser bolt", false);
	gameState->endPortal = loadPNGTexture(gameState, "res/end portal");

	gameState->consoleTriangle = loadPNGTexture(gameState, "res/triangle_blue");
	gameState->consoleTriangleSelected = loadPNGTexture(gameState, "res/triangle_light_blue");
	gameState->consoleTriangleGrey = loadPNGTexture(gameState, "res/triangle_grey");
	gameState->consoleTriangleYellow = loadPNGTexture(gameState, "res/triangle_yellow");

	gameState->laserBaseOff = loadPNGTexture(gameState, "res/virus3/base off");
	gameState->laserBaseOn = loadPNGTexture(gameState, "res/virus3/base on");
	gameState->laserTopOff = loadPNGTexture(gameState, "res/virus3/top off");
	gameState->laserTopOn = loadPNGTexture(gameState, "res/virus3/top on");
	gameState->laserBeam = loadPNGTexture(gameState, "res/virus3/laser beam");

	gameState->heavyTileTex = loadPNGTexture(gameState, "res/Heavy1");

	gameState->dock = loadPNGTexture(gameState, "res/dock", false);
	gameState->dockBlueEnergyTile = loadPNGTexture(gameState, "res/Blue Energy Tile", false);
	gameState->attribute = loadPNGTexture(gameState, "res/Attribute", false);
	gameState->behaviour = loadPNGTexture(gameState, "res/Behaviour", false);

	uint numTilesInAtlas;
	gameState->tileAtlas = extractTextures(gameState, "res/tiles_floored", 120, 240, 12, &numTilesInAtlas);

	char* mapFileNames[] = {
		"map3.tmx",
		"map4.tmx",
		"map5.tmx",
	};

	int mapFileIndex = 0;
	loadLevel(gameState, mapFileNames, arrayCount(mapFileNames), &mapFileIndex, false);

	bool running = true;
	double frameTime = 1.0 / 60.0;
	uint fpsCounterTimer = SDL_GetTicks();
	uint fps = 0;
	double dtForFrame = 0;
	double maxDtForFrame = 1.0 / 6.0;
	uint lastTime = SDL_GetTicks();
	uint currentTime;

	//printf("Error: %d\n", glGetError());

	while (running) {
		gameState->input.leftMouseJustPressed = false;
		gameState->input.upJustPressed = false;

		currentTime = SDL_GetTicks();
		dtForFrame += (double)((currentTime - lastTime) / 1000.0); 
		lastTime = currentTime;

		uint frameStart = SDL_GetTicks();

		if (currentTime - fpsCounterTimer > 1000) {
			fpsCounterTimer += 1000;
			printf("Fps: %d\n", fps);
			fps = 0;
		}

		//glClearColor(0, 0, 0, 1);

		//if (dtForFrame > frameTime) {
			if (dtForFrame > maxDtForFrame) dtForFrame = maxDtForFrame;

			fps++;
			gameState->swapFieldP = gameState->windowSize * 0.5 + v2(0, 4.25);

			pollInput(gameState, &running);

			glClear(GL_COLOR_BUFFER_BIT);

			//printf("Error: %d\n", glGetError());
			updateAndRenderEntities(gameState, dtForFrame);

			pushSortEnd(gameState->renderGroup);

			{ //Draw the dock
				V2 size = gameState->windowSize.x * v2(1, gameState->dock.size.y / gameState->dock.size.x);
				V2 dockP = v2(0, gameState->windowSize.y - size.y);
				R2 bounds = r2(dockP, dockP + size);
				pushTexture(gameState->renderGroup, &gameState->dock, bounds, false, DrawOrder_gui, false);

				V2 blueEnergySize = 0.45 * v2(1, 1);
				V2 blueEnergyStartP = dockP + v2(1.6, 0.68);
				V2 barSlope = normalize(v2(1, -0.04));
				double barLength = 5.41;

				int maxEnergy = 40;
				double energySize = barLength / (double)maxEnergy;

				if(gameState->blueEnergy > maxEnergy) gameState->blueEnergy = maxEnergy;

				for(int energyIndex = 0; energyIndex < gameState->blueEnergy; energyIndex++) {
					//TODO: <= is only needed for the last energyIndex
					for(double lerpIndex = 0; lerpIndex <= 1; lerpIndex += 0.5) {
						V2 blueEnergyP = blueEnergyStartP + barSlope * ((energyIndex + lerpIndex) * energySize);
						R2 blueEnergyBounds = rectCenterDiameter(blueEnergyP, blueEnergySize);
						pushTexture(gameState->renderGroup, &gameState->dockBlueEnergyTile, blueEnergyBounds, false, DrawOrder_gui, false);
					}
				}
				
			}

			updateConsole(gameState, dtForFrame);
			drawRenderGroup(gameState->renderGroup);
			removeEntities(gameState);


			{ //NOTE: This updates the camera position
				V2 cameraPDiff = gameState->newCameraP - gameState->cameraP;
				double maxCameraMovement = 30 * dtForFrame;

				if (lengthSq(cameraPDiff) > square(maxCameraMovement)) {
					cameraPDiff = normalize(cameraPDiff) * maxCameraMovement;
				}

				gameState->cameraP += cameraPDiff;
				gameState->renderGroup->negativeCameraP = -gameState->cameraP;
			}

			dtForFrame = 0;

			if (gameState->input.xJustPressed) {
				gameState->blueEnergy += 10;
			}

			{ //NOTE: This reloads the game
				bool resetLevel = !getEntityByRef(gameState, gameState->playerDeathRef) &&
								  (!getEntityByRef(gameState, gameState->playerRef) || gameState->input.rJustPressed);

				
				if (gameState->loadNextLevel || 
					resetLevel) {
					loadLevel(gameState, mapFileNames, arrayCount(mapFileNames), &mapFileIndex, true);
				}
			}

			uint frameEnd = SDL_GetTicks();
			uint frameTime = frameEnd - frameStart;

			//printf("Frame Time: %d\n", frameTime);

			//SDL_RenderPresent(renderer); //Swap the buffers
			SDL_GL_SwapWindow(window);
		//}
	}

	return 0;
}