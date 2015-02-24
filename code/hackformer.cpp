#include "SDL.h"
#include "SDL_image.h"
#include <stdio.h>
#include <cassert>
#include "hackformer_math.cpp"

#define uint unsigned int
#define uint16 unsigned short

#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 720

//Pre-condition: IMG_Init has already been called
SDL_Texture* loadPNGTexture(SDL_Renderer* renderer, char* fileName) {
	SDL_Surface *image = IMG_Load(fileName);
	assert(image);
	SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, image);
	assert(texture);
	SDL_FreeSurface(image);
	SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
	return texture;
}

struct Entity {
	V2 pos;
	SDL_Texture* texture;
};

int main(int argc, char *argv[]) {
	//TODO: Only initialize what is needed
	if (SDL_Init(SDL_INIT_EVERYTHING) < 0) {
		fprintf(stderr, "Failed to initialize SDL. Error: %s", SDL_GetError());
		return -1;
	}

	if (IMG_Init(IMG_INIT_PNG) != IMG_INIT_PNG) {
		fprintf(stderr, "Failed to initialize SDL Image.");
		return -1;
	}

	SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1); 
	SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);

	SDL_Window *window = SDL_CreateWindow("C++former", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_ALLOW_HIGHDPI);
	SDL_Surface *screenSurface = SDL_GetWindowSurface(window);

	if (!window) {
		fprintf(stderr, "Failed to create window. Error: %s", SDL_GetError());
		return -1;
	}

	SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

	if (!renderer) {
		fprintf(stderr, "Failed to create renderer. Error: %s", SDL_GetError());
		return -1;
	}

	bool running = true;

	double frameRate = 60.0;
	double frameTimeMS = 1000.0 / 60.0;
	uint fpsCounterTimer = SDL_GetTicks();
	uint fps = 0;

	double dt = 0.0;

	uint lastTime = SDL_GetTicks();
	uint currentTime;

	bool left = false, right = false, up = false, down = false;
	bool leftDown = false;
	V2 mouse = {};
	SDL_Rect dstRect = {};

	Entity player = {};
	player.pos = {100, 100};
	player.texture = loadPNGTexture(renderer, "res/player_stand.png");

	while (running) {
		bool leftClicked = false;

		currentTime = SDL_GetTicks();
		dt += (currentTime - lastTime) / frameTimeMS; 
		lastTime = currentTime;

		if (currentTime - fpsCounterTimer > 1000) {
			fpsCounterTimer += 1000;
			printf("Fps: %d\n", fps);
			fps = 0;
		}

		if (dt > 1) {
			fps++;
			SDL_Event event;
			while(SDL_PollEvent(&event) > 0) {
				switch(event.type) {
					case SDL_QUIT:
					running = false;
					break;
					case SDL_KEYDOWN:
					case SDL_KEYUP: {
						bool pressed = event.key.state == SDL_PRESSED;
						SDL_Keycode key = event.key.keysym.sym;

						if (key == SDLK_w || key == SDLK_UP) up = pressed;
						else if (key == SDLK_a || key == SDLK_LEFT) left = pressed;
						else if (key == SDLK_s || key == SDLK_DOWN) down = pressed;
						else if (key == SDLK_d || key == SDLK_RIGHT) right = pressed;
					} break;
					case SDL_MOUSEMOTION:
					mouse.x = event.motion.x;
					mouse.y = event.motion.y;
					break;
					case SDL_MOUSEBUTTONDOWN:
					case SDL_MOUSEBUTTONUP:
					if (event.button.button == SDL_BUTTON_LEFT) {
						if (event.button.state == SDL_PRESSED) leftDown = leftClicked = true;
						else leftDown = false;
					}
				}
			}
		}

		while (dt > 1) {
			dt--;

			SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
			SDL_RenderClear(renderer);		

			dstRect.x = player.pos.x;
			dstRect.y = player.pos.y;
			dstRect.w = 200;
			dstRect.h = 200;

			SDL_RenderCopy(renderer, player.texture, NULL, &dstRect);

			SDL_RenderPresent(renderer); //Swap the buffers
		}

		SDL_Delay(1);
	}

	return 0;
}