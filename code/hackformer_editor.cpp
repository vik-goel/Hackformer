#include "hackformer_editor.h"

#include "hackformer_renderer.cpp"

void moveCamera(Input* input, Camera* camera) {
	if(input->mouseScroll) {
		double minScale = 0.25;
		double maxScale = 4.0;
		double scrollPerNotch = 0.2 * camera->scale;

		double requestedScale = camera->scale + input->mouseScroll * scrollPerNotch;

		camera->scale = clamp(requestedScale, minScale, maxScale);
	}

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
	TextureData* tileAtlas = textureData + *textureDataCount;

	for(s32 tileIndex = 0; tileIndex < arrayCount(globalTileFileNames); tileIndex++) {
		char* fileName = globalTileFileNames[tileIndex];
		TextureData* data = tileAtlas + tileIndex;

		char fileNameWithPrefix[150];
		sprintf(fileNameWithPrefix, "tiles/%s", fileName);

		*data = loadPNGTexture(group, fileNameWithPrefix, false, false);
	}

	*textureDataCount += arrayCount(globalTileFileNames);

	return arrayCount(globalTileFileNames);
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

void drawScaledLine(RenderGroup* group, Color lineColor, V2 lineStart, V2 lineEnd, double lineThickness, double dashSize,
	                double spaceSize, Camera* camera, V2 scaleCenter) {

	V2 lineCenter = (lineStart + lineEnd) * 0.5;
	V2 toScaleCenter = lineCenter - scaleCenter;
	V2 newLineCenter = scaleCenter + toScaleCenter * camera->scale;

	lineStart = lineStart - lineCenter + newLineCenter; 
	lineEnd = lineEnd - lineCenter + newLineCenter; 

	pushDashedLine(group, lineColor, lineStart, lineEnd, lineThickness, dashSize, spaceSize, true);
}

void drawScaledTex(RenderGroup* group, TextureData* tex, Camera* camera, V2 center, V2 size, V2 scaleCenter) {
	center = (center - scaleCenter) * camera->scale + scaleCenter;

	R2 bounds = rectCenterDiameter(center, size);
	pushTexture(group, tex, bounds, false, DrawOrder_gui, true);
}

V2 unprojectP(Camera* camera, V2 scaleCenter, V2 p) {
	V2 result = (scaleCenter * (camera->scale - 1) + p) * (1.0 / camera->scale);
	return result;
}

void setTile(s32* tiles, s32 mapWidthInTiles, s32 mapHeightInTiles, Camera* camera, Input* input, V2 scaleCenter, V2 gridSize, s32 value) {
	V2 mousePos = unprojectP(camera, scaleCenter, input->mouseInWorld);

	s32 tileX = (s32)(mousePos.x / gridSize.x);
	s32 tileY = (s32)(mousePos.y / gridSize.y);

	if(tileX >= 0 && tileY >= 0 && tileX < mapWidthInTiles && tileY < mapHeightInTiles) {
		tiles[tileY * mapWidthInTiles + tileX] = value;
	}
}

int main(int argc, char* argv[]) {
	s32 panelWidth = 250;
	s32 windowWidth = 1280 + panelWidth, windowHeight = 720;
	SDL_Window* window = createWindow(windowWidth, windowHeight);

	Input input = {};
	initInputKeyCodes(&input);

	MemoryArena arena = createArena(1024 * 1024 * 128, true);

	Camera camera = {};
	initCamera(&camera);
	double oldScale = camera.scale;

	TextureData* textureData = pushArray(&arena, TextureData, TEXTURE_DATA_COUNT);
	s32 textureDataCount = 1;

	RenderGroup* renderGroup  = createRenderGroup(1024 * 1024, &arena, TEMP_PIXELS_PER_METER, windowWidth, windowHeight, 
													&camera, textureData, &textureDataCount);
	renderGroup->enabled = true;

	bool running = true;

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

	CursorMode cursorMode = CursorMode_stampTile;

	#define ENTITY(type, width, height, fileName) {EntityType_##type, DrawOrder_##type, v2(width, height), fileName},
	EntitySpec entitySpecs[] {
		ENTITY(player, 1.75, 1.75, "player/stand2")
		ENTITY(virus, 1.6, 1.6, "virus1/stand")
		ENTITY(flyingVirus, 0.75, 0.75, "virus2/full")
		ENTITY(laserBase, 0.9, 0.65, "virus3/base_off")
		ENTITY(blueEnergy, 0.7, 0.7, "blue_energy")
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


	char* saveFileName = "edit_save.txt";

	FILE* file = fopen(saveFileName, "r");

	s32 mapWidthInTiles = 0;
	s32 mapHeightInTiles = 0;
	s32* tiles = NULL;

	if(file) {
		mapWidthInTiles = readS32(file);
		mapHeightInTiles = readS32(file);

		tiles = pushArray(&arena, s32, mapHeightInTiles * mapWidthInTiles);

		for(s32 tileY = 0; tileY < mapHeightInTiles; tileY++) {
			for(s32 tileX = 0; tileX < mapWidthInTiles; tileX++) {
				tiles[tileY * mapWidthInTiles + tileX] = readS32(file);
			}
		}

		entityCount = readS32(file);

		for(s32 entityIndex = 0; entityIndex < entityCount; entityIndex++) {
			Entity* entity = entities + entityIndex;

			entity->type = (EntityType)readS32(file);
			bool foundSpec = false;

			for(s32 specIndex = 0; specIndex < arrayCount(entitySpecs); specIndex++) {
				EntitySpec* spec = entitySpecs + specIndex;

				if(spec->type == entity->type) {
					entity->drawOrder = spec->drawOrder;
					entity->tex = entityTextureAtlas + specIndex;
					entity->bounds = rectCenterDiameter(readV2(file), spec->size);

					foundSpec = true;
					break;
				}
			}

			assert(foundSpec);
		}

		fclose(file);
	} else {
		mapWidthInTiles = 60;
		mapHeightInTiles = 12;

		tiles = pushArray(&arena, s32, mapHeightInTiles * mapWidthInTiles);

		for(s32 tileY = 0; tileY < mapHeightInTiles; tileY++) {
			for(s32 tileX = 0; tileX < mapWidthInTiles; tileX++) {
				tiles[tileY * mapWidthInTiles + tileX] = -1;
			}
		}
	}

	while(running) {
		pollInput(&input, &running, windowHeight, TEMP_PIXELS_PER_METER, &camera);

		if(input.esc.justPressed) {
			cursorMode = CursorMode_moveEntity;
		}

		//NOTE: Draw the game

		camera.scale = 1;
		pushClipRect(renderGroup, gameBounds);
		pushFilledRect(renderGroup, renderGroup->windowBounds, createColor(150, 150, 150, 255));
		camera.scale = oldScale;

		V2 scaleCenter = gameWindowSize * 0.5;

		for(s32 rowIndex = 0; rowIndex <= mapHeightInTiles; rowIndex++) {
			V2 lineStart = v2(0, gridSize.y * rowIndex);
			V2 lineEnd = v2(mapWidthInTiles * gridSize.x, lineStart.y);

			drawScaledLine(renderGroup, lineColor, lineStart, lineEnd, lineThickness, dashSize, spaceSize, &camera, 
				scaleCenter);
		}

		for(s32 colIndex = 0; colIndex <= mapWidthInTiles; colIndex++) {
			V2 lineStart = v2(gridSize.x * colIndex, 0);
			V2 lineEnd = v2(lineStart.x, mapHeightInTiles * gridSize.y);

			drawScaledLine(renderGroup, lineColor, lineStart, lineEnd, lineThickness, dashSize, spaceSize, &camera, 
					scaleCenter);
		}

		for(s32 tileY = 0; tileY < mapHeightInTiles; tileY++) {
			for(s32 tileX = 0; tileX < mapWidthInTiles; tileX++) {
				s32 tileIndex = tiles[tileY * mapWidthInTiles + tileX];

				if(tileIndex >= 0) {
					TextureData* tex = tileAtlas + tileIndex;
					V2 tileCenter = hadamard(v2(tileX, tileY), gridSize) + tileSize * 0.5;
					drawScaledTex(renderGroup, tex, &camera, tileCenter, tileSize, scaleCenter);					
				}
			}
		}

		for(s32 entityIndex = 0; entityIndex < entityCount; entityIndex++) {
			Entity* entity = entities + entityIndex;

			V2 entityCenter = getRectCenter(entity->bounds);
			V2 unscaledSize = getRectSize(entity->bounds);

			V2 mouseP = unprojectP(&camera, scaleCenter, input.mouseInWorld);
			if(pointInsideRect(entity->bounds, mouseP)) {

			}

			drawScaledTex(renderGroup, entity->tex, &camera, entityCenter, unscaledSize, scaleCenter);	
		}

		//NOTE: Draw the panel

		camera.scale = 1;
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

		s32 numEntitiesPerRow = 2;
		for(s32 specIndexIter = 0; specIndexIter < arrayCount(entitySpecs); specIndexIter += numEntitiesPerRow) {
			double minX = panelBounds.min.x;
			double maxY = 0;

			for(s32 specOffset = 0; specOffset < numEntitiesPerRow; specOffset++) {
				s32 specIndex = specIndexIter + specOffset;

				if(specIndex > arrayCount(entitySpecs)) break;

				EntitySpec* spec = entitySpecs + specIndex;
				V2 specSpacing = v2(0.1, 0.1);

				V2 specMin = v2(minX, maxTileY);

				R2 specBounds = r2(specMin, specMin + spec->size);
				minX = specBounds.max.x;
				maxY = max(maxY, specBounds.max.y);

				if(clickedInside(&input, specBounds)) {
					selectedEntitySpecIndex = specIndex;
					cursorMode = CursorMode_stampEntity;
				}

				TextureData* tex = entityTextureAtlas + specIndex;

				pushTexture(renderGroup, tex, specBounds, false, DrawOrder_gui);
			}

			maxTileY = maxY;
		}


		//NOTE: Draw things which can cross between the game and panel
		camera.scale = oldScale;
		pushDefaultClipRect(renderGroup);

		bool mouseInGame = pointInsideRect(gameBounds, input.mouseInMeters);

		if(cursorMode == CursorMode_stampTile) {
			if(selectedTileIndex >= 0) {
				R2 tileBounds = rectCenterDiameter(input.mouseInMeters, tileSize);

				if(input.leftMouse.pressed && mouseInGame) {
					setTile((s32*)tiles, mapWidthInTiles, mapHeightInTiles, &camera, &input, scaleCenter, gridSize, selectedTileIndex);
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

					V2 center = unprojectP(&camera, scaleCenter, input.mouseInWorld);

					entity->bounds = rectCenterDiameter(center, spec->size);
					entity->tex = tex;
				}
				
				pushTexture(renderGroup, tex, entityBounds, false, DrawOrder_gui);
			}
		}

		Entity* clickedEntity = NULL;
		s32 clickedEntityIndex = -1;

		for(s32 entityIndex = 0; entityIndex < entityCount; entityIndex++) {
			Entity* entity = entities + entityIndex;

			V2 mouseP = unprojectP(&camera, scaleCenter, input.mouseInWorld);

			if(pointInsideRect(entity->bounds, mouseP)) {
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
				setTile((s32*)tiles, mapWidthInTiles, mapHeightInTiles, &camera, &input, scaleCenter, gridSize, -1);
			}
		}

		bool maintainMovingEntity = false;
		if(input.leftMouse.pressed) {
			if(cursorMode == CursorMode_moveEntity) {
				if(input.leftMouse.justPressed) {
					movingEntity = clickedEntity;
				}

				if(movingEntity) {
					V2 center = unprojectP(&camera, scaleCenter, input.mouseInWorld);
					movingEntity->bounds = reCenterRect(movingEntity->bounds, center);
					maintainMovingEntity = true;
				}
			}
		}
		if(!maintainMovingEntity) {
			movingEntity = NULL;
		}

		if(input.x.justPressed) {
			FILE* file = fopen(saveFileName, "w");
			assert(file);

			writeS32(file, mapWidthInTiles);
			writeS32(file, mapHeightInTiles);

			for(s32 tileY = 0; tileY < mapHeightInTiles; tileY++) {
				for(s32 tileX = 0; tileX < mapWidthInTiles; tileX++) {
					writeS32(file, tiles[tileY * mapWidthInTiles + tileX]);
				}
			}

			writeS32(file, entityCount);

			for(s32 entityIndex = 0; entityIndex < entityCount; entityIndex++) {
				Entity* entity = entities + entityIndex;
				writeS32(file, entity->type);
				writeV2(file, getRectCenter(entity->bounds));
			}

			fclose(file);
		}

		glClear(GL_COLOR_BUFFER_BIT);
		drawRenderGroup(renderGroup, NULL);
		
		moveCamera(&input, &camera);
		oldScale = camera.scale;

		SDL_GL_SwapWindow(window);
	}

	return 0;
}