#include "hackformer.h"

#include "hackformer_renderer.cpp"
#include "hackformer_entity.cpp"
#include "hackformer_consoleField.cpp"

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

	int tileWidth, tileHeight, tileSpacing;
	bool foundTilesetInfo = false;
	Texture* textures = NULL;
	uint numTextures;

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
			}
		}
		else if(!foundTilesetInfo) {
			if (extractIntFromLine(line, lineLength, "tilewidth", &tileWidth)) {
				extractIntFromLine(line, lineLength, "tileheight", &tileHeight);
				extractIntFromLine(line, lineLength, "spacing", &tileSpacing);
				foundTilesetInfo = true;
			}
		}
		else if(textures == NULL) {
			if (extractStringFromLine(line, lineLength, "image source", buffer, arrayCount(buffer))) {
				int texWidth = 0, texHeight = 0;
				extractIntFromLine(line, lineLength, "width", &texWidth);
				extractIntFromLine(line, lineLength, "height", &texHeight);
				textures = extractTextures(gameState, buffer, tileWidth, tileHeight, tileSpacing, &numTextures,
										   texWidth, texHeight);
			}
		}
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
						V2 tileP = v2(tileX + 0.5f, tileY + 0.5f) * gameState->tileSize;
						addTile(gameState, tileP, textures + tileIndex);
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

				V2 p = v2((double)entityX, (double)entityY) / 120.0f * gameState->tileSize;
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
					fgets(line, sizeof(line), file);
					fgets(line, sizeof(line), file);
					extractStringFromLine(line, lineLength, "value", buffer, arrayCount(buffer));
					addText(gameState, p, buffer);
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

	if (gameState->solidGrid) {
		for (int rowIndex = 0; rowIndex < gameState->solidGridWidth; rowIndex++) {
			free(gameState->solidGrid[rowIndex]);
		}

		free(gameState->solidGrid);
		gameState->solidGrid = NULL;
	}

	gameState->solidGridWidth = (int)ceil(gameState->mapSize.x / gameState->solidGridSquareSize);
	gameState->solidGridHeight = (int)ceil(gameState->windowSize.y / gameState->solidGridSquareSize);

	gameState->solidGrid = (PathNode**)calloc(1, gameState->solidGridWidth * sizeof(PathNode*));

	for (int rowIndex = 0; rowIndex < gameState->solidGridWidth; rowIndex++) {
		gameState->solidGrid[rowIndex] = (PathNode*)calloc(1, gameState->solidGridHeight * sizeof(PathNode));

		for (int colIndex = 0; colIndex < gameState->solidGridHeight; colIndex++) {
			PathNode* node = gameState->solidGrid[rowIndex] + colIndex;

			node->p = v2(rowIndex + 0.5, colIndex + 0.5) * gameState->solidGridSquareSize;
			node->tileX = rowIndex;
			node->tileY = colIndex;
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

				V2 mouseInMeters = input->mouseInPixels / gameState->pixelsPerMeter;

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

int main(int argc, char *argv[]) {
	int windowWidth = 1280, windowHeight = 720;

	//TODO: Proper error handling if any of these libraries does not load
//TODO: Only initialize what is needed
	if (SDL_Init(SDL_INIT_EVERYTHING) < 0) {
		fprintf(stderr, "Failed to initialize SDL. Error: %s", SDL_GetError());
		assert(false);
	}

	if (IMG_Init(IMG_INIT_PNG) != IMG_INIT_PNG) {
		fprintf(stderr, "Failed to initialize SDL Image.");
		assert(false);
	}

	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

	SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);

	SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1); 
	SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);

	SDL_Window* window = SDL_CreateWindow("C++former", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 
										  windowWidth, windowHeight, 
										  SDL_WINDOW_ALLOW_HIGHDPI|SDL_WINDOW_OPENGL);
	
	SDL_GLContext glContext = SDL_GL_CreateContext(window);

	if (!window) {
		fprintf(stderr, "Failed to create window. Error: %s", SDL_GetError());
		assert(false);
	}

	if (TTF_Init()) {
		fprintf(stderr, "Failed to initialize SDL_ttf.");
		assert(false);
	}

	int gameStateSize = sizeof(GameState); 

	MemoryArena arena_ = {};
	arena_.size = 1024 * 1024;
	arena_.base = (char*)calloc(arena_.size, 1);

	GameState* gameState = (GameState*)pushStruct(&arena_, GameState);
	gameState->permanentStorage = arena_;

	gameState->pixelsPerMeter = 70.0f;
	gameState->windowWidth = windowWidth;
	gameState->windowHeight = windowHeight;
	gameState->windowSize = v2((double)windowWidth, (double)windowHeight) / gameState->pixelsPerMeter;

	glOrtho(0.0, gameState->windowSize.x, 0.0, gameState->windowSize.y, -1.0, 1.0);

	gameState->gravity = v2(0, -9.81f);
	gameState->solidGridSquareSize = 0.1;
	gameState->tileSize = 0.9f;
	gameState->refCount = 1; //NOTE: This is for the null reference

	gameState->consoleFont = loadCachedFont(gameState, "fonts/PTS55f.ttf", 16, 2);
	gameState->textFont = loadFont("fonts/Roboto-Regular.ttf", 64);

	gameState->playerStand = loadPNGTexture(gameState, "res/player/stand.png");
	gameState->playerJump = loadPNGTexture(gameState, "res/player/jump.png");
	gameState->playerWalk = loadAnimation(gameState, "res/player/walk.png", 128, 128, 0.04f, 768, 384);

	gameState->virus1Stand = loadPNGTexture(gameState, "res/virus1/stand.png");
	gameState->virus1Shoot = loadAnimation(gameState, "res/virus1/shoot.png", 256, 256, 0.04f, 2560, 256);
	gameState->shootDelay = getAnimationDuration(&gameState->virus1Shoot);

	//TODO: Make shoot animation time per frame be set by the shootDelay
	gameState->flyingVirus = loadPNGTexture(gameState, "res/virus2/full.png");
	gameState->flyingVirusShoot = loadAnimation(gameState, "res/virus2/shoot.png", 256, 256, 0.04f, 2560, 256);

	gameState->sunsetCityBg = loadPNGTexture(gameState, "res/backgrounds/sunset city bg.png");
	gameState->sunsetCityMg = loadPNGTexture(gameState, "res/backgrounds/sunset city mg.png");
	gameState->marineCityBg = loadPNGTexture(gameState, "res/backgrounds/marine city bg.png");
	gameState->marineCityMg = loadPNGTexture(gameState, "res/backgrounds/marine city mg.png");

	gameState->sunsetCityBg.size = v2(2560, 512) / gameState->pixelsPerMeter;
	gameState->marineCityBg.size = v2(2560, 512) / gameState->pixelsPerMeter;
	gameState->sunsetCityMg.size = v2(3060, 512) / gameState->pixelsPerMeter;
	gameState->marineCityMg.size = v2(3060, 512) / gameState->pixelsPerMeter;

	gameState->blueEnergyTex = loadPNGTexture(gameState, "res/blue energy.png");
	gameState->laserBolt = loadPNGTexture(gameState, "res/virus1/laser bolt.png");
	gameState->endPortal = loadPNGTexture(gameState, "res/end portal.png");

	gameState->consoleTriangle = loadPNGTexture(gameState, "res/blue triangle.png");
	gameState->consoleTriangleSelected = loadPNGTexture(gameState, "res/light blue triangle.png");
	gameState->consoleTriangleGrey = loadPNGTexture(gameState, "res/grey triangle.png");
	gameState->consoleTriangleYellow = loadPNGTexture(gameState, "res/yellow triangle.png");

	gameState->laserBaseOff = loadPNGTexture(gameState, "res/virus3/base off.png");
	gameState->laserBaseOn = loadPNGTexture(gameState, "res/virus3/base on.png");
	gameState->laserTopOff = loadPNGTexture(gameState, "res/virus3/top off.png");
	gameState->laserTopOn = loadPNGTexture(gameState, "res/virus3/top on.png");
	gameState->laserBeam = loadPNGTexture(gameState, "res/virus3/laser beam.png");

	double keyboardSpeedFieldValues[] = {20, 40, 60, 80, 100}; 
	gameState->keyboardSpeedField = createDoubleField(gameState, "speed", keyboardSpeedFieldValues, 
												   arrayCount(keyboardSpeedFieldValues), 2, 1);

	double keyboardJumpHeightFieldValues[] = {1, 3, 5, 7, 9}; 
	gameState->keyboardJumpHeightField = createDoubleField(gameState, "jump_height", keyboardJumpHeightFieldValues, 
													    arrayCount(keyboardJumpHeightFieldValues), 2, 1);

	gameState->keyboardDoubleJumpField = createBoolField(gameState, "double_jump", false, 3);

	double patrolSpeedFieldValues[] = {10, 20, 30, 40, 50}; 
	gameState->patrolSpeedField = createDoubleField(gameState, "speed", patrolSpeedFieldValues, 
												   arrayCount(patrolSpeedFieldValues), 2, 1);

	gameState->keyboardControlledField = 
		createConsoleField(gameState, "keyboard_controlled", ConsoleField_keyboardControlled);

	gameState->movesBackAndForthField = 
		createConsoleField(gameState, "patrols", ConsoleField_movesBackAndForth);

	gameState->shootsAtTargetField = 
		createConsoleField(gameState, "shoots", ConsoleField_shootsAtTarget);

	double shootRadiusFieldValues[] = {1, 3, 5, 7, 9}; 
	gameState->shootRadiusField = createDoubleField(gameState, "detect_radius", shootRadiusFieldValues, 
													    arrayCount(shootRadiusFieldValues), 2, 2);

	double bulletSpeedFieldValues[] = {1, 2, 3, 4, 5}; 
	gameState->bulletSpeedField = createDoubleField(gameState, "bullet_speed", bulletSpeedFieldValues, 
													    arrayCount(bulletSpeedFieldValues), 2, 1);

	gameState->isShootTargetField = 
		createConsoleField(gameState, "is_target", ConsoleField_isShootTarget);

	gameState->seeksTargetField = createConsoleField(gameState, "seeks_target", ConsoleField_seeksTarget);

	double seekTargetSpeedFieldValues[] = {5, 10, 15, 20, 25}; 
	gameState->seekTargetSpeedField = createDoubleField(gameState, "speed", seekTargetSpeedFieldValues, 
													    arrayCount(seekTargetSpeedFieldValues), 2, 1);

	double seekTargetRadiusFieldValues[] = {4, 8, 100, 16, 20}; 
	gameState->seekTargetRadiusField = createDoubleField(gameState, "sight_radius", seekTargetRadiusFieldValues, 
													    arrayCount(seekTargetRadiusFieldValues), 2, 2);

	gameState->tileXOffsetField = createUnlimitedIntField(gameState, "x_offset", 0, 1);
	gameState->tileYOffsetField = createUnlimitedIntField(gameState, "y_offset", 0, 1);

	gameState->hurtsEntitiesField = createBoolField(gameState, "hurts_entities", true, 2);

	char* mapFileNames[] = {
		"map3.tmx",
		"map4.tmx",
		"map5.tmx",
	};

	int mapFileIndex = 0;
	loadTmxMap(gameState, mapFileNames[mapFileIndex]);
	addFlyingVirus(gameState, v2(8, 7));

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

		if (currentTime - fpsCounterTimer > 1000) {
			fpsCounterTimer += 1000;
			printf("Fps: %d\n", fps);
			fps = 0;
		}

		glClearColor(0, 0, 0, 1);

		if (dtForFrame > frameTime) {
			if (dtForFrame > maxDtForFrame) dtForFrame = maxDtForFrame;

			fps++;
			gameState->swapFieldP = gameState->windowSize / 2 + v2(0, 3);

			pollInput(gameState, &running);


			glClear(GL_COLOR_BUFFER_BIT);

			//printf("Error: %d\n", glGetError());
			updateAndRenderEntities(gameState, dtForFrame);
			updateConsole(gameState);

			SDL_GL_SwapWindow(window);

			dtForFrame = 0;
			gameState->cameraP = gameState->newCameraP;

			if (gameState->input.xJustPressed) {
				gameState->blueEnergy += 10;
			}

			//NOTE: This reloads the game
			if (gameState->loadNextLevel || 
				!getEntityByRef(gameState, gameState->playerRef) || 
				gameState->input.rJustPressed) {

				gameState->shootTargetRef = 0;
				gameState->consoleEntityRef = 0;
				gameState->playerRef = 0;

				for (int entityIndex = 0; entityIndex < gameState->numEntities; entityIndex++) {
					freeEntity(gameState->entities + entityIndex, gameState);
				}

				gameState->numEntities = 0;
				gameState->blueEnergy = 0;

				for (int refIndex = 0; refIndex < arrayCount(gameState->entityRefs_); refIndex++) {
					freeEntityReference(gameState->entityRefs_ + refIndex, gameState);
				}

				gameState->refCount = 1; //NOTE: This is for the null reference

				if (gameState->swapField) freeConsoleField(gameState->swapField, gameState);

				if (gameState->loadNextLevel) {
					gameState->loadNextLevel = false;
					mapFileIndex++;
					mapFileIndex %= arrayCount(mapFileNames);
				}

				loadTmxMap(gameState, mapFileNames[mapFileIndex]);
			}
		}
	}

	return 0;
}