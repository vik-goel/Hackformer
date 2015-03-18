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
				textures = extractTextures(gameState, buffer, tileWidth, tileHeight, tileSpacing, &numTextures);
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
				else if (stringsMatch(buffer, "background")) {
					addBackground(gameState);
				}
			}
		}
	}

	fclose(file);
}

void pollInput(GameState* gameState, bool* running) {
	gameState->input.dMouseMeters = v2(0, 0);

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

SDL_Renderer* initGameForRendering(int windowWidth, int windowHeight) {
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

	SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1); 
	SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);

	SDL_Window *window = SDL_CreateWindow("C++former", 
										  SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 
										  windowWidth, windowHeight, 
										  SDL_WINDOW_ALLOW_HIGHDPI);

	if (!window) {
		fprintf(stderr, "Failed to create window. Error: %s", SDL_GetError());
		assert(false);
	}
	SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);

	if (!renderer) {
		fprintf(stderr, "Failed to create renderer. Error: %s", SDL_GetError());
		assert(false);
	}

	if (TTF_Init()) {
		fprintf(stderr, "Failed to initialize SDL_ttf.");
		assert(false);
	}

	return renderer;
}

int main(int argc, char *argv[]) {
	int windowWidth = 1280, windowHeight = 720;

	SDL_Renderer* renderer = initGameForRendering(windowWidth, windowHeight);

	int gameStateSize = sizeof(GameState); 

	MemoryArena arena_ = {};
	arena_.size = 1024 * 1024;
	arena_.base = (char*)calloc(arena_.size, 1);

	GameState* gameState = (GameState*)pushStruct(&arena_, GameState);
	gameState->permanentStorage = arena_;
	gameState->renderer = renderer;

	gameState->pixelsPerMeter = 70.0f;
	gameState->windowWidth = windowWidth;
	gameState->windowHeight = windowHeight;
	gameState->windowSize = v2((double)windowWidth, (double)windowHeight) / gameState->pixelsPerMeter;
	gameState->gravity = v2(0, -9.81f);
	gameState->tileSize = 0.9f;
	gameState->refCount = 1; //NOTE: This is for the null reference

	gameState->consoleFont = loadFont("fonts/Roboto-Regular.ttf", 16);
	gameState->textFont = loadFont("fonts/Roboto-Regular.ttf", 64);

	gameState->playerStand = loadPNGTexture(renderer, "res/player/stand.png");
	gameState->playerJump = loadPNGTexture(renderer, "res/player/jump.png");
	gameState->playerWalk = loadAnimation(gameState, "res/player/walk.png", 128, 128, 0.04f);

	gameState->virus1Stand = loadPNGTexture(renderer, "res/virus1/stand.png");
	gameState->virus1Shoot = loadAnimation(gameState, "res/virus1/shoot.png", 256, 256, 0.04f);
	gameState->shootDelay = getAnimationDuration(&gameState->virus1Shoot);

	gameState->sunsetCityBg = loadPNGTexture(renderer, "res/backgrounds/sunset city bg.png");
	gameState->sunsetCityMg = loadPNGTexture(renderer, "res/backgrounds/sunset city mg.png");

	gameState->blueEnergy = loadPNGTexture(renderer, "res/blue energy.png");
	gameState->laserBolt = loadPNGTexture(renderer, "res/virus1/laser bolt.png");

	gameState->consoleTriangle = loadPNGTexture(renderer, "res/triangle.png");
	gameState->consoleTriangleSelected = loadPNGTexture(renderer, "res/grey triangle.png");
	gameState->consoleTriangleDown = loadPNGTexture(renderer, "res/triangle down.png");
	gameState->consoleTriangleDownSelected = loadPNGTexture(renderer, "res/grey triangle down.png");
	gameState->consoleTriangleUp = loadPNGTexture(renderer, "res/triangle up.png");
	gameState->consoleTriangleUpSelected = loadPNGTexture(renderer, "res/grey triangle up.png");

	double keyboardSpeedFieldValues[] = {20, 40, 60, 80, 100}; 
	gameState->keyboardSpeedField = createDoubleField(gameState, "speed", keyboardSpeedFieldValues, 
												   arrayCount(keyboardSpeedFieldValues), 2);

	double keyboardJumpHeightFieldValues[] = {1, 3, 5, 7, 9}; 
	gameState->keyboardJumpHeightField = createDoubleField(gameState, "jump_height", keyboardJumpHeightFieldValues, 
													    arrayCount(keyboardJumpHeightFieldValues), 2);

	double patrolSpeedFieldValues[] = {10, 20, 30, 40, 50}; 
	gameState->patrolSpeedField = createDoubleField(gameState, "speed", patrolSpeedFieldValues, 
												   arrayCount(patrolSpeedFieldValues), 2);

	gameState->keyboardControlledField = 
		createConsoleField(gameState, "keyboard_controlled", ConsoleField_keyboardControlled);

	gameState->movesBackAndForthField = 
		createConsoleField(gameState, "patrols", ConsoleField_movesBackAndForth);

	gameState->shootsAtTargetField = 
		createConsoleField(gameState, "shoots", ConsoleField_shootsAtTarget);

	gameState->isShootTargetField = 
		createConsoleField(gameState, "is_target", ConsoleField_isShootTarget);

	gameState->tileXOffsetField = createUnlimitedIntField(gameState, "x_offset", 0);
	gameState->tileYOffsetField = createUnlimitedIntField(gameState, "y_offset", 0);

	char* mapFileNames[] = {
		"map3.tmx",
	};

	int mapFileIndex = 0;

	addText(gameState, v2(4, 6), "Hello, World");
	loadTmxMap(gameState, mapFileNames[mapFileIndex]);

	bool running = true;
	double frameTime = 1.0 / 60.0;
	uint fpsCounterTimer = SDL_GetTicks();
	uint fps = 0;
	double dtForFrame = 0;
	uint lastTime = SDL_GetTicks();
	uint currentTime;
	
	while (running) {
		gameState->input.leftMouseJustPressed = false;
		gameState->input.upJustPressed = false;

		currentTime = SDL_GetTicks();
		dtForFrame += (double)((currentTime - lastTime) / 1000.0); 
		lastTime = currentTime;

		if (currentTime - fpsCounterTimer > 1000) {
			fpsCounterTimer += 1000;
			//printf("Fps: %d\n", fps);
			fps = 0;
		}

		if (dtForFrame > frameTime) {
			fps++;
			gameState->swapFieldP = gameState->windowSize / 2 + v2(0, 3);

			pollInput(gameState, &running);

			SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
			SDL_RenderClear(renderer);

			updateAndRenderEntities(gameState, dtForFrame);
			updateConsole(gameState);

			SDL_RenderPresent(renderer); //Swap the buffers
			dtForFrame = 0;

			//NOTE: This reloads the game
			if (!getEntityByRef(gameState, gameState->playerRef)) {
				gameState->shootTargetRef = 0;
				gameState->consoleEntityRef = 0;
				gameState->playerRef = 0;

				for (int entityIndex = 0; entityIndex < gameState->numEntities; entityIndex++) {
					freeEntity(gameState->entities + entityIndex, gameState);
				}

				gameState->numEntities = 0;

				for (int refIndex = 0; refIndex < arrayCount(gameState->entityRefs_); refIndex++) {
					freeEntityReference(gameState->entityRefs_ + refIndex, gameState);
				}

				gameState->refCount = 1; //NOTE: This is for the null reference

				if (gameState->swapField) freeConsoleField(gameState->swapField, gameState);


				loadTmxMap(gameState, mapFileNames[mapFileIndex]);
			}
		}
	}

	return 0;
}