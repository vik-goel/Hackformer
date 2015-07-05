struct FieldSpec {};

#include "hackformer_types.h"
#include "hackformer_renderer.h"
#include "hackformer_renderer.cpp"

int main(int argc, char* argv[]) {
	char* inputFileName = "animation_data.txt";

	FILE* file = fopen(inputFileName, "r");
	assert(file);

	char animationFilePath[1000];
	fgets(animationFilePath, arrayCount(animationFilePath), file);
	animationFilePath[strlen(animationFilePath) - 1] = 0;

	s32 frameWidth = readS32(file);
	s32 frameHeight = readS32(file);
	double secondsPerFrame = readDouble(file);
	bool pingPong = readS32(file) != 0;

	fclose(file);

	s32 windowWidth = frameWidth;
	s32 windowHeight = frameHeight;

	SDL_Window* window = createWindow(windowWidth, windowHeight);

	Input input;
	initInputKeyCodes(&input);

	Camera camera;
	initCamera(&camera);

	bool running = true;

	MemoryArena arena = createArena(1024 * 1024 * 32, true);

	TextureData* textureDatas = pushArray(&arena, TextureData, 1000);
	s32 textureDatasCount = 1;

	RenderGroup* renderGroup = createRenderGroup(1024 * 1024, &arena, TEMP_PIXELS_PER_METER, windowWidth, windowHeight, 
		&camera, textureDatas, &textureDatasCount);
	renderGroup->enabled = true;

	Animation anim = loadAnimation(renderGroup, &arena, animationFilePath, frameWidth, frameHeight, secondsPerFrame, pingPong);
	V2 drawSize = getDrawSize(anim.frames, windowHeight / TEMP_PIXELS_PER_METER, renderGroup);

	double animTime = 0;

	u32 lastTime = SDL_GetTicks();
	u32 currentTime = SDL_GetTicks();

	while(running) {
		pollInput(&input, &running, windowHeight, TEMP_PIXELS_PER_METER, &camera);

		glClearColor(0.4f, 0.4f, 0.4f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);

		pushSortEnd(renderGroup);

		currentTime = SDL_GetTicks();
		animTime += (currentTime - lastTime) / 1000.0;
		lastTime = currentTime;

		Texture* tex = getAnimationFrame(&anim, animTime); 

		double bgHeight = windowHeight / TEMP_PIXELS_PER_METER;
		R2 bgBounds = r2(v2(0, 0), drawSize);

		pushTexture(renderGroup, tex, bgBounds, false, DrawOrder_gui, false, Orientation_0, WHITE, 1);

		drawRenderGroup(renderGroup, 0);
		SDL_GL_SwapWindow(window);
	}

	return 0;
}