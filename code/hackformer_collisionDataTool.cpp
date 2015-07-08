struct FieldSpec {};

#include "hackformer_types.h"
#include "hackformer_renderer.h"
#include "hackformer_renderer.cpp"

void writeStr(FILE* file, char* str) {
	fwrite(str, strlen(str), 1, file);
}

int main(int argc, char* argv[]) {
	char* outputFileName = "collision_code.txt";

	FILE* file = fopen(outputFileName, "r");
	assert(file);

	char bgImage[1000];
	fgets(bgImage, arrayCount(bgImage), file);
	bgImage[strlen(bgImage) - 1] = 0;

	fclose(file);

	s32 windowWidth = 512;
	s32 windowHeight = 512;

	SDL_Window* window = createWindow(windowWidth, windowHeight);

	Input input;
	initInputKeyCodes(&input);

	Camera camera;
	initCamera(&camera);

	bool running = true;

	MemoryArena arena = createArena(1024 * 1024 * 16, true);

	Texture* textures = pushArray(&arena, Texture, 10);
	s32 texturesCount = 1;

	RenderGroup* renderGroup = createRenderGroup(1024 * 1024, &arena, TEMP_PIXELS_PER_METER, windowWidth, windowHeight, 
		&camera, textures, &texturesCount);
	renderGroup->enabled = true;


	Texture* tex = loadPNGTexture(renderGroup, bgImage, false);

	bool flipX = false;
	bool drawBothFlips = false;

	s32 pointsCount = 0;
	V2 points[1000];

	s32 selectedPointIndex = -1;

	double pointThickness = 0.05;

	while(running) {
		pollInput(&input, &running, windowHeight, TEMP_PIXELS_PER_METER, &camera);

		glClearColor(0.4f, 0.4f, 0.4f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);

		pushSortEnd(renderGroup);

		if(input.x.justPressed) flipX = !flipX;
		if(input.c.justPressed) drawBothFlips = !drawBothFlips;

		double bgHeight = windowHeight / TEMP_PIXELS_PER_METER;
		V2 bgSize = getDrawSize(tex, bgHeight);

		if(bgSize.x > bgHeight) {
			bgSize.y = (bgHeight / bgSize.x) * bgHeight;
			bgSize.x = bgHeight;
		}

		R2 bgBounds = r2(v2(0, 0), bgSize);
		bgBounds = scaleRect(bgBounds, v2(1, 1) * 0.9);

		pushFilledRect(renderGroup, bgBounds, createColor(255, 255, 255, 50));
		pushTexture(renderGroup, tex, bgBounds, flipX, false, DrawOrder_gui, false, Orientation_0, WHITE, 1);
		if (drawBothFlips) pushTexture(renderGroup, tex, bgBounds, !flipX, false, DrawOrder_gui, false, Orientation_0, WHITE, 1);

		for(s32 pIndex = 0; pIndex < pointsCount; pIndex++) {
			R2 rect = rectCenterRadius(points[pIndex], v2(1, 1) * pointThickness);
			pushFilledRect(renderGroup, rect, BLUE, false);
		}

		if(pointsCount > 1) {
			for(s32 pIndex = 0; pIndex < pointsCount; pIndex++) {
				V2 p1 = points[pIndex];
				V2 p2 = points[(pIndex + 1) % pointsCount];

				pushDashedLine(renderGroup, RED, p1, p2, 0.02, 0.05, 0.05, false);
			}
		}

		if(controlZJustPressed(&input)) {
			pointsCount = max(0, pointsCount - 1);
			selectedPointIndex = -1;
		}

		if(input.leftMouse.justPressed) {
			assert(pointsCount < arrayCount(points));
			points[pointsCount++] = input.mouseInMeters;
			selectedPointIndex = -1;
		}

		if(input.rightMouse.justPressed) {
			for(s32 pIndex = 0; pIndex < pointsCount; pIndex++) {
				R2 rect = rectCenterRadius(points[pIndex], v2(1, 1) * pointThickness);

				if(pointInsideRect(rect, input.mouseInMeters)) {
					selectedPointIndex = pIndex;
					break;
				}
			}
		}

		if(selectedPointIndex >= 0) {
			if(input.rightMouse.pressed) {
				points[selectedPointIndex] += input.dMouseMeters;
			} else {
				selectedPointIndex = -1;
			}
		}

		if(input.z.justPressed) {
			FILE* file = fopen(outputFileName, "w");
			assert(file);

			writeStr(file, bgImage);
			writeStr(file, "\n\n");

			char* instructions = "Left mouse to place points\nRight mouse to move points\nCtrl+Z to remove the last point\nZ to save\nX to flip the background image on X\nC to show both the flipped and unflipped images\n\n\n";
			writeStr(file, instructions);

			writeStr(file, "double hitboxWidth = result->renderSize.x;\n");
			writeStr(file, "double hitboxHeight = result->renderSize.y;\n");
			writeStr(file, "double halfHitboxWidth = hitboxWidth * 0.5;\n");
			writeStr(file, "double halfHitboxHeight = hitboxHeight * 0.5;\n");
			writeStr(file, "Hitbox* hitbox = addHitbox(result, gameState);\n");
			writeStr(file, "setHitboxSize(hitbox, hitboxWidth * 1.2, hitboxHeight * 1.2);\n");

			char buffer[1000];

			sprintf(buffer, "hitbox->collisionPointsCount = %d;\n", pointsCount);
			writeStr(file, buffer);

			V2 bgSize = getRectSize(bgBounds);
			V2 bgCenter = getRectCenter(bgBounds);

			V2 halfBgSize = bgSize * 0.5;

			for(s32 pIndex = 0; pIndex < pointsCount; pIndex++) {
				V2 p = points[pIndex] - bgCenter;

				double xPercentage = p.x / halfBgSize.x;
				double yPercentage = p.y / halfBgSize.y;

				sprintf(buffer, "hitbox->originalCollisionPoints[%d] = v2(%f * halfHitboxWidth, %f * halfHitboxHeight);\n", 
								 pIndex, xPercentage, yPercentage);
				writeStr(file, buffer);
			}

			fclose(file);
		}

		drawRenderGroup(renderGroup, 0);
		SDL_GL_SwapWindow(window);
	}

	return 0;
}