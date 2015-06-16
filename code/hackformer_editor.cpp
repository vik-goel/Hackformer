#include "hackformer_editor.h"

#include "hackformer_renderer.cpp"

void moveCamera(Input* input, Camera* camera) {
	V2 movement = {};

	if(input->up.pressed) movement.y++;
	if(input->down.pressed) movement.y--;
	if(input->right.pressed) movement.x++;
	if(input->left.pressed) movement.x--;
	
	if(movement.x || movement.y) {
		double movementSpeed = 0.5;
		V2 delta = normalize(movement) * movementSpeed;

		camera->p += delta; 
	}
}

s32 loadTileAtlas(RenderGroup* group, TextureData* textureData, s32* textureDataCount) {
	char* tileFileNames[] = {
		"basic_1",
		"basic_2",
		"basic_3",
		"delay",
		"disappear",
		"heavy",
	};

	TextureData* tileAtlas = textureData + *textureDataCount;

	for(s32 tileIndex = 0; tileIndex < arrayCount(tileFileNames); tileIndex++) {
		char* fileName = tileFileNames[tileIndex];
		TextureData* data = tileAtlas + tileIndex;

		char fileNameWithPrefix[150];
		sprintf(fileNameWithPrefix, "tiles/%s", fileName);

		*data = loadPNGTexture(group, fileNameWithPrefix, false, false);
	}

	*textureDataCount += arrayCount(tileFileNames);

	return arrayCount(tileFileNames);
}

bool clickedInside(Input* input, R2 rect) {
	bool result = false;

	if(input->leftMouse.justPressed) {
		if(pointInsideRect(rect, input->mouseInMeters)) {
			result = true;
		}
	}

	return result;
}

int main(int argc, char* argv[]) {
	s32 panelWidth = 250;
	s32 windowWidth = 1280 + panelWidth, windowHeight = 720;
	SDL_Window* window = createWindow(windowWidth, windowHeight);

	Input input = {};
	initInputKeyCodes(&input);

	MemoryArena arena = createArena(1024 * 1024 * 128, true);

	Camera camera = {};

	TextureData* textureData = pushArray(&arena, TextureData, TEXTURE_DATA_COUNT);
	s32 textureDataCount = 1;

	RenderGroup* renderGroup  = createRenderGroup(32 * 1024, &arena, TEMP_PIXELS_PER_METER, windowWidth, windowHeight, 
													&camera, textureData, &textureDataCount);
	renderGroup->enabled = true;

	bool running = true;

	const s32 mapWidthInTiles = 60;
	const s32 mapHeightInTiles = 12;

	TextureData* tileAtlas = textureData + textureDataCount;
	s32 tileAtlasCount = loadTileAtlas(renderGroup, textureData, &textureDataCount);

	Entity* entities = pushArray(&arena, Entity, 5000);
	s32 entityCount = 0;

	Color lineColor = createColor(100, 100, 100, 255);
	double lineThickness = 0.01;
	double dashSize = 0.1;
	double spaceSize = 0.05;

	V2 tileSize = v2(1, 1.1) * TILE_SIZE_IN_METERS;
	V2 gridSize = v2(1, 1) * TILE_SIZE_IN_METERS;

	double metersPerPixel = 1.0 / renderGroup->pixelsPerMeter;
	V2 gameWindowSize = metersPerPixel * v2(windowWidth - panelWidth, windowHeight);
	V2 panelWindowMax = metersPerPixel * v2(windowWidth, windowHeight);
	R2 gameBounds = r2(v2(0, 0), gameWindowSize);
	R2 panelBounds = r2(v2(gameBounds.max.x, 0), panelWindowMax);

	s32 selectedTileIndex = -1;

	s32 tiles[mapHeightInTiles][mapWidthInTiles];

	for(s32 tileY = 0; tileY < mapHeightInTiles; tileY++) {
		for(s32 tileX = 0; tileX < mapWidthInTiles; tileX++) {
			tiles[tileY][tileX] = -1;
		}
	}

	CursorMode cursorMode = CursorMode_stampTile;

	#define ENTITY(type, width, height, fileName) {EntityType_##type, DrawOrder_##type, v2(width, height), fileName},
	EntitySpec entitySpecs[] {
		ENTITY(player, 1.75, 1.75, "player/stand2")
		ENTITY(virus, 2.5, 2.5, "virus1/stand")
		ENTITY(flyingVirus, 1.5, 1.5, "virus2/full")
		ENTITY(laserBase, 0.9, 0.65, "virus3/base_off")
		ENTITY(blueEnergy, 0.8, 0.8, "blue_energy")
		ENTITY(endPortal, 1, 2, "end_portal")
	};
	#undef ENTITY

	TextureData* entityTextureAtlas = textureData + textureDataCount;
	for(s32 specIndex = 0; specIndex < arrayCount(entitySpecs); specIndex++) {
		EntitySpec* spec = entitySpecs + specIndex;
		char* fileName = spec->fileName;
		textureData[textureDataCount++] = loadPNGTexture(renderGroup, fileName, false, false);
	}

	s32 selectedEntitySpecIndex = 0;
	Entity* movingEntity = NULL;


	while(running) {
		pollInput(&input, &running, windowHeight, TEMP_PIXELS_PER_METER, camera.p);

		if(input.esc.justPressed) {
			cursorMode = CursorMode_moveEntity;
		}

		//NOTE: Draw the game
		pushClipRect(renderGroup, gameBounds);
		pushFilledRect(renderGroup, renderGroup->windowBounds, createColor(150, 150, 150, 255));

		for(s32 rowIndex = 0; rowIndex <= mapHeightInTiles; rowIndex++) {
			V2 lineStart = v2(0, gridSize.y * rowIndex);
			V2 lineEnd = v2(mapWidthInTiles * gridSize.x, lineStart.y);
			pushDashedLine(renderGroup, lineColor, lineStart, lineEnd, lineThickness, dashSize, spaceSize, true);
		}

		for(s32 colIndex = 0; colIndex <= mapWidthInTiles; colIndex++) {
			V2 lineStart = v2(gridSize.x * colIndex, 0);
			V2 lineEnd = v2(lineStart.x, mapHeightInTiles * gridSize.y);
			pushDashedLine(renderGroup, lineColor, lineStart, lineEnd, lineThickness, dashSize, spaceSize, true);
		}

		for(s32 tileY = 0; tileY < mapHeightInTiles; tileY++) {
			for(s32 tileX = 0; tileX < mapWidthInTiles; tileX++) {
				s32 tileIndex = tiles[tileY][tileX];

				if(tileIndex >= 0) {
					TextureData* tex = tileAtlas + tileIndex;
					V2 tileMin = hadamard(v2(tileX, tileY), gridSize);
					R2 tileBounds = r2(tileMin, tileMin + tileSize);
					pushTexture(renderGroup, tex, tileBounds, false, DrawOrder_gui, true);
				}
			}
		}

		for(s32 entityIndex = 0; entityIndex < entityCount; entityIndex++) {
			Entity* entity = entities + entityIndex;

			if(pointInsideRect(entity->bounds, input.mouseInMeters)) {

			}

			pushTexture(renderGroup, entity->tex, entity->bounds, false, entity->drawOrder, true);
		}

		//NOTE: Draw the panel
		pushClipRect(renderGroup, panelBounds);
		pushFilledRect(renderGroup, renderGroup->windowBounds, createColor(255, 255, 255, 255));

		if(clickedInside(&input, panelBounds)) {
			selectedTileIndex = -1;
			cursorMode = CursorMode_moveEntity;
		}

		double maxTileY = 0;

		for(s32 tileIndex = 0; tileIndex < tileAtlasCount; tileIndex++) {
			V2 tileSpacing = v2(0.1, 0.1);

			V2 tileMin = tileSpacing + panelBounds.min + hadamard(tileSpacing + tileSize, v2(tileIndex % 3, tileIndex / 3));
			R2 tileBounds = r2(tileMin, tileMin + tileSize);
			maxTileY = tileBounds.max.y;

			if(clickedInside(&input, tileBounds)) {
				selectedTileIndex = tileIndex;
				cursorMode = CursorMode_stampTile;
			}

			TextureData* tex = tileAtlas + tileIndex;

			pushTexture(renderGroup, tex, tileBounds, false, DrawOrder_gui);
		}

		for(s32 specIndex = 0; specIndex < arrayCount(entitySpecs); specIndex++) {
			EntitySpec* spec = entitySpecs + specIndex;

			V2 specSpacing = v2(0.1, 0.1);

			V2 specMin = v2(0, maxTileY) + specSpacing + panelBounds.min + hadamard(specSpacing + spec->size, v2(specIndex % 2, specIndex / 2));
			R2 specBounds = r2(specMin, specMin + spec->size);

			if(clickedInside(&input, specBounds)) {
				selectedEntitySpecIndex = specIndex;
				cursorMode = CursorMode_stampEntity;
			}

			TextureData* tex = entityTextureAtlas + specIndex;

			pushTexture(renderGroup, tex, specBounds, false, DrawOrder_gui);
		}


		//NOTE: Draw things which can cross between the game and panel
		pushDefaultClipRect(renderGroup);

		bool mouseInGame = pointInsideRect(gameBounds, input.mouseInMeters);

		if(cursorMode == CursorMode_stampTile) {
			if(selectedTileIndex >= 0) {
				R2 tileBounds = rectCenterDiameter(input.mouseInMeters, tileSize);

				if(input.leftMouse.pressed && mouseInGame) {
					s32 tileX = (s32)(input.mouseInWorld.x / gridSize.x);
					s32 tileY = (s32)(input.mouseInWorld.y / gridSize.y);

					if(tileX >= 0 && tileY >= 0 && tileX < mapWidthInTiles && tileY < mapHeightInTiles) {
						tiles[tileY][tileX] = selectedTileIndex;
					}
				}


				TextureData* tex = tileAtlas + selectedTileIndex;
				pushTexture(renderGroup, tex, tileBounds, false, DrawOrder_gui);
			}
		}
		else if(cursorMode == CursorMode_stampEntity) {
			if(selectedEntitySpecIndex >= 0) {
				EntitySpec* spec = entitySpecs + selectedEntitySpecIndex;
				TextureData* tex = entityTextureAtlas + selectedEntitySpecIndex;

				R2 entityBounds = rectCenterDiameter(input.mouseInMeters, spec->size);

				if(input.leftMouse.justPressed && mouseInGame) {
					Entity* entity = entities + entityCount++;
					entity->type = spec->type;
					entity->drawOrder = spec->drawOrder;
					entity->bounds = rectCenterDiameter(input.mouseInWorld, spec->size);;
					entity->tex = tex;
				}
				
				pushTexture(renderGroup, tex, entityBounds, false, DrawOrder_gui);
			}
		}

		Entity* clickedEntity = NULL;
		s32 clickedEntityIndex = -1;

		for(s32 entityIndex = 0; entityIndex < entityCount; entityIndex++) {
			Entity* entity = entities + entityIndex;

			if(pointInsideRect(entity->bounds, input.mouseInWorld)) {
				clickedEntity = entity;
				clickedEntityIndex = entityIndex;
				break;
			}
		}

		if(input.rightMouse.pressed) {
			if(clickedEntity) {
				entities[clickedEntityIndex] = entities[entityCount-- - 1];
				clickedEntity = NULL;
				clickedEntityIndex = -1;
			} else {
				s32 tileX = (s32)(input.mouseInMeters.x / gridSize.x);
				s32 tileY = (s32)(input.mouseInMeters.y / gridSize.y);

				if(tileX >= 0 && tileY >= 0 && tileX < mapWidthInTiles && tileY < mapHeightInTiles) {
					tiles[tileY][tileX] = -1;
				}
			}
		}

		bool maintainMovingEntity = false;
		if(input.leftMouse.pressed) {
			if(cursorMode == CursorMode_moveEntity) {
				if(input.leftMouse.justPressed) {
					movingEntity = clickedEntity;
				}

				if(movingEntity) {
					movingEntity->bounds = reCenterRect(movingEntity->bounds, input.mouseInWorld);
					maintainMovingEntity = true;
				}
			}
		}
		if(!maintainMovingEntity) {
			movingEntity = NULL;
		}

		glClear(GL_COLOR_BUFFER_BIT);
		drawRenderGroup(renderGroup, NULL);
		
		moveCamera(&input, &camera);

		SDL_GL_SwapWindow(window);
	}

	return 0;
}