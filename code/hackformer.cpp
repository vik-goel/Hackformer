#include "SDL.h"
#include "SDL_image.h"
#include <stdio.h>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include "SDL_ttf.h"

#include "hackformer.h"

#include "hackformer_math.cpp"
#include "hackformer_renderer.cpp"
#include "hackformer_entity.cpp"

char* loadDelmittedStringIntoBuffer(char* src, char* dest, int* length) {
	*length = 0;

	for (char* c = src; *c != ','; c++) {
		dest[(*length)++] = c[0];
	}

	dest[*(length)++] = 0;
	return dest;
}

void loadTiledMap(GameState* gameState, char* fileName) {
	FILE* file;
	fopen_s(&file, fileName, "r");
	assert(file);

	int width = -1;
	int height = -1;
	bool encounteredTileData = false;

	Texture* textures = NULL;
	int y = 0;

	float tileSize = gameState->tileSize;

	char buffer[512];
    while (fgets (buffer, sizeof(buffer), file)) {
		if (width == -1) {
			if (strncmp(buffer, "width", 5) == 0) {
				width = atoi(buffer + 6);
			}
		}
		else if (height == -1) {
			if (strncmp(buffer, "height", 6) == 0) {
				height = atoi(buffer + 7);
				y = height;
			}
		}
		else if (textures == NULL) {
			if (strncmp(buffer, "tileset", 7) == 0) {
				char strBuf[50];
				int length = 0;
				int startPoint = 8;

				loadDelmittedStringIntoBuffer(buffer + startPoint, strBuf, &length);
				SDL_Texture* atlas = loadPNGTexture(gameState->renderer, strBuf);

				startPoint += length + 1;
				int tileWidth = atoi(loadDelmittedStringIntoBuffer(buffer + startPoint, strBuf, &length));

				startPoint += length + 1;
				int tileHeight = atoi(loadDelmittedStringIntoBuffer(buffer + startPoint, strBuf, &length));

				int atlasWidth, atlasHeight;
				SDL_QueryTexture(atlas, NULL, NULL, &atlasWidth, &atlasHeight);

				int tileCols = atlasWidth / tileWidth;
				int tileRows = atlasHeight / tileHeight;

				textures = (Texture*)pushArray(&gameState->permanentStorage, Texture, tileCols * tileRows);

				for (int rowIndex = 0; rowIndex < tileRows; rowIndex++) {
					for (int colIndex = 0; colIndex < tileCols; colIndex++) {
						Texture* tile = textures + (rowIndex * tileCols) + colIndex;
						tile->tex = atlas;
						tile->srcRect.x = colIndex * tileWidth + 3 * (colIndex + 1);
						tile->srcRect.y = rowIndex * tileHeight - 3 * (colIndex + 1);
						tile->srcRect.w = tileWidth;
						tile->srcRect.h = tileHeight;
					}
				}

			}
		}
		else if (!encounteredTileData) {
			encounteredTileData = strncmp(buffer, "data=", 5) == 0;
		} else {
			char numBuffer[5];
			int numSize = 0;
			int x = 0;

			for (char* c = buffer; *c != '\n'; c++) {
				if (*c == ',') {
					numBuffer[numSize] = 0;
					int tileIndex = atoi(numBuffer) - 1;

					if (tileIndex >= 0) {
						V2 tileP = v2((x + 0.5f) * tileSize, (y - 0.5f) * tileSize);
						Texture* tileTexture = textures + tileIndex;
						addTile(gameState, tileP, tileTexture);
					}

					x++;
					numSize = 0;
				} else {
					numBuffer[numSize++] = *c;
				}
			}

			y--;
		}
   	}

	fclose(file);

	gameState->mapWidth = width * tileSize;
	gameState->mapHeight = height * tileSize;
}

void pollInput(GameState* gameState, bool* running) {
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

				input->mouseInPixels.x = (float)mouseX;
				input->mouseInPixels.y = (float)(WINDOW_HEIGHT - mouseY);

				input->mouseInMeters = input->mouseInPixels / (float)PIXELS_PER_METER;
				input->mouseInWorld = input->mouseInMeters + gameState->cameraP;

				} break;
			case SDL_MOUSEBUTTONDOWN:
			case SDL_MOUSEBUTTONUP:
				if (event.button.button == SDL_BUTTON_LEFT) {
					if (event.button.state == SDL_PRESSED) 
						input->leftMousePressed = input->leftMouseJustPressed = true;
				}
				break;
		}
	}
}

SDL_Renderer* initGameForRendering() {
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
										  WINDOW_WIDTH, WINDOW_HEIGHT, 
										  SDL_WINDOW_ALLOW_HIGHDPI);

	if (!window) {
		fprintf(stderr, "Failed to create window. Error: %s", SDL_GetError());
		assert(false);
	}
	SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

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
	SDL_Renderer* renderer = initGameForRendering();

	int gameStateSize = sizeof(GameState); 

	MemoryArena arena_ = {};
	arena_.size = 1024 * 1024;
	arena_.base = (char*)calloc(arena_.size, 1);

	GameState* gameState = (GameState*)pushStruct(&arena_, GameState);
	gameState->numEntities = 1; //NOTE: 0 is for the null entity
	gameState->permanentStorage = arena_;
	gameState->renderer = renderer;
	gameState->entities->type = EntityType_null;

	gameState->font = loadFont("fonts/Roboto-Regular.ttf", 64);

	loadPNGTexture(&gameState->playerStand, renderer, "res/player/stand.png");
	loadPNGTexture(&gameState->playerJump, renderer, "res/player/jump.png");
	gameState->playerWalk = loadAnimation(gameState, "res/player/walk.png", 128, 128, 0.04f);
	loadPNGTexture(&gameState->background, renderer, "res/SunsetCityBackground.png");
	loadPNGTexture(&gameState->blueEnergy, renderer, "res/blue_energy.png");

	addPlayer(gameState, v2(2, 8));
	addBlueEnergy(gameState, v2(4, 6));

	gameState->tileSize = 0.8f;
	loadTiledMap(gameState, "maps/map1.txt");

	addBackground(gameState);

	bool running = true;
	double frameTime = 1.0 / 60.0;
	uint fpsCounterTimer = SDL_GetTicks();
	uint fps = 0;
	float dtForFrame = 0;
	uint lastTime = SDL_GetTicks();
	uint currentTime;
	
	while (running) {
		gameState->input.leftMouseJustPressed = false;
		gameState->input.upJustPressed = false;

		currentTime = SDL_GetTicks();
		dtForFrame += (float)((currentTime - lastTime) / 1000.0); 
		lastTime = currentTime;

		if (currentTime - fpsCounterTimer > 1000) {
			fpsCounterTimer += 1000;
			//printf("Fps: %d\n", fps);
			fps = 0;
		}

		if (dtForFrame > frameTime) {
			fps++;

			pollInput(gameState, &running);

			SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
			SDL_RenderClear(renderer);

			updateAndRenderEntities(gameState, dtForFrame);

			SDL_RenderPresent(renderer); //Swap the buffers
			dtForFrame = 0;
		}

		SDL_Delay(1);
	}

	return 0;
}