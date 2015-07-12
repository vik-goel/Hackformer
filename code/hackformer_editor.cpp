#include "hackformer_editor.h"

#include "hackformer_renderer.cpp"

V2 unprojectP(Camera* camera, V2 p) {
	V2 result = (camera->scaleCenter * (camera->scale - 1) + p) * (1.0 / camera->scale);
	return result;
}

void moveCamera(Input* input, Camera* camera, V2 gameWindowSize) {
	if(input->mouseScroll) {
		double minScale = 0.25;
		double maxScale = 1.5;
		double scrollPerNotch = 0.2 * camera->scale;

		double requestedScale = camera->scale + input->mouseScroll * scrollPerNotch;

		camera->scaleCenter = camera->p + gameWindowSize * 0.5;
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

s32 loadTileAtlas(RenderGroup* group, Texture* textureData, s32* textureDataCount) {
	Texture* tileAtlas = textureData + *textureDataCount;

	for(s32 tileIndex = 0; tileIndex < arrayCount(globalTileData); tileIndex++) {
		TileData* data = globalTileData + tileIndex;

		char fileNameWithPrefix[2000];
		sprintf(fileNameWithPrefix, "editor_res/tiles/%s.png", data->fileName);

		loadPNGTexture(group, fileNameWithPrefix, false);
	}


	return arrayCount(globalTileData);
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
	                double spaceSize, Camera* camera) {

	V2 lineCenter = (lineStart + lineEnd) * 0.5;
	V2 toScaleCenter = lineCenter - camera->scaleCenter;
	V2 newLineCenter = camera->scaleCenter + toScaleCenter * camera->scale;

	lineStart = lineStart - lineCenter + newLineCenter; 
	lineEnd = lineEnd - lineCenter + newLineCenter; 

	pushDashedLine(group, lineColor, lineStart, lineEnd, lineThickness, dashSize, spaceSize, true);
}

void drawScaledTex(RenderGroup* group, Texture* tex, Camera* camera, V2 center, V2 size, bool32 flipX, bool32 flipY) {
	center = (center - camera->scaleCenter) * camera->scale + camera->scaleCenter;

	R2 bounds = rectCenterDiameter(center, size);
	pushTexture(group, tex, bounds, flipX != 0, flipY != 0, DrawOrder_gui, true);
}

void setTile(TileSpec* tiles, s32 mapWidthInTiles, s32 mapHeightInTiles, Camera* camera, Input* input, V2 gridSize, TileSpec* value) {
	V2 mousePos = unprojectP(camera, input->mouseInWorld);

	s32 tileX = (s32)(mousePos.x / gridSize.x);
	s32 tileY = (s32)(mousePos.y / gridSize.y);

	if(tileX >= 0 && tileY >= 0 && tileX < mapWidthInTiles && tileY < mapHeightInTiles) {
		TileSpec* spec = tiles + (tileY * mapWidthInTiles + tileX);

		if(value) {
			*spec = *value;
		} else {
			spec->index = -1;
			spec->flipX = spec->flipY = false;
		}
	}
}

bool entityGetsWaypoints(Entity* entity) {
	bool result = entity->type == EntityType_trojan ||
				  entity->type == EntityType_shrike;
	return result;
}

Waypoint* allocateWaypoint(MemoryArena* arena, V2 p, Waypoint* next = NULL) {
	Waypoint* result = pushStruct(arena, Waypoint);
	result->p = p;
	result->next = next;
	return result;
}

int main(int argc, char* argv[]) {
	s32 panelWidth = 250;
	s32 windowWidth = 1280 + panelWidth, windowHeight = 720;
	SDL_Window* window = createWindow(windowWidth, windowHeight);

	Input input = {};
	initInputKeyCodes(&input);

	MemoryArena arena = createArena(1024 * 1024 * 150, true);

	Camera camera = {};
	initCamera(&camera);
	double oldScale = camera.scale;

	Texture* textures = pushArray(&arena, Texture, MAX_TEXTURES);
	s32 texturesCount = 1;

	Assets assets = {};
	initAssets(&assets);

	RenderGroup* renderGroup  = createRenderGroup(8 * 1024 * 1024, &arena, TEMP_PIXELS_PER_METER, windowWidth, windowHeight, 
													&camera, textures, &texturesCount, &assets);
	renderGroup->enabled = true;

	bool running = true;

	Texture* tileAtlas = textures + texturesCount;
	s32 tileAtlasCount = loadTileAtlas(renderGroup, textures, &texturesCount);

	Entity* entities = pushArray(&arena, Entity, 5000);
	s32 entityCount = 0;

	Color lineColor = createColor(100, 100, 100, 255);
	double lineThickness = 0.01;
	double dashSize = 0.1;
	double spaceSize = 0.05;

	V2 shortTileSize = v2(TILE_WIDTH_IN_METERS, TILE_HEIGHT_WITHOUT_OVERHANG_IN_METERS);
	V2 tallTileSize = v2(TILE_WIDTH_IN_METERS, TILE_HEIGHT_IN_METERS);
	V2 gridSize = v2(TILE_WIDTH_IN_METERS, TILE_HEIGHT_WITHOUT_OVERHANG_IN_METERS);

	double metersPerPixel = 1.0 / renderGroup->pixelsPerMeter;
	V2 gameWindowSize = metersPerPixel * v2(windowWidth - panelWidth, windowHeight);
	V2 panelWindowMax = metersPerPixel * v2(windowWidth, windowHeight);
	R2 gameBounds = r2(v2(0, 0), gameWindowSize);
	R2 panelBounds = r2(v2(gameBounds.max.x, 0), panelWindowMax);

	TileSpec* selectedTileSpec = NULL;

	CursorMode cursorMode = CursorMode_moveEntity;

	#define ENTITY(type, width, height, fileName, panelX, panelY, panelScale) {EntityType_##type, DrawOrder_##type, v2(width, height), fileName, v2(panelX, panelY), panelScale},
	EntitySpec entitySpecs[] {
		ENTITY(player, 1.75, 1.75, "player", 0, 6, 1)
		ENTITY(lamp_0, 1, 0.5, "light_0", 0.1, 5, 1)
		ENTITY(lamp_1, 350.0/109.0*0.4, 0.4, "light_1", 1.5, 5, 1)
		ENTITY(laserBase, 0.9, 0.65, "virus3", 1.5, 6, 1)
		ENTITY(hackEnergy, 0.7, 0.7, "energy", 0.5, 0.5, 1)
		ENTITY(endPortal, 2, 2, "end_portal", 0, 3, 1)
		ENTITY(trojan, 2, 2, "trojan", 0, 1, 1)
		ENTITY(motherShip, 6 * (264.0 / 512.0), 6 * (339.0 / 512.0), "mothership", 2, 3, 0.5)
		ENTITY(trawler, 2, 2, "trawler", 1.8, 1.2, 1)
		ENTITY(shrike, 1.5, 1.5, "shrike", 1.75, 0, 1)
	};
	#undef ENTITY

	char buffer[2000];

	Texture* entityTextureAtlas = textures + texturesCount;
	for(s32 specIndex = 0; specIndex < arrayCount(entitySpecs); specIndex++) {
		EntitySpec* spec = entitySpecs + specIndex;
		char* fileName = spec->fileName;
		sprintf(buffer, "editor_res/%s.png", fileName);
		loadPNGTexture(renderGroup, buffer, false);
	}

	s32 selectedEntitySpecIndex = 0;
	Entity* movingEntity = NULL;
	Waypoint* movingWaypoint = NULL;

	BackgroundTextures backgroundTextures;
	initBackgroundTextures(&backgroundTextures);

	char* saveFileName = "maps/edit.hack";

	FILE* file = fopen(saveFileName, "rb");

	s32 mapWidthInTiles = 0;
	s32 mapHeightInTiles = 0;
	TileSpec* tiles = NULL;

	if(file) {
		mapWidthInTiles = readS32(file);
		mapHeightInTiles = readS32(file);

		setBackgroundTexture(&backgroundTextures, (BackgroundType)readS32(file), renderGroup);

		tiles = pushArray(&arena, TileSpec, mapHeightInTiles * mapWidthInTiles);

		for(s32 tileY = 0; tileY < mapHeightInTiles; tileY++) {
			for(s32 tileX = 0; tileX < mapWidthInTiles; tileX++) {
				TileSpec* spec = tiles + (tileY * mapWidthInTiles + tileX);

				s32 tile = readS32(file);

				if(tile == -1) {
					spec->index = -1;
					spec->flipX = spec->flipY = false;
					spec->tall = false;
				} else {
					spec->index = tile & TILE_INDEX_MASK;
					spec->flipX = tile & TILE_FLIP_X_FLAG;
					spec->flipY = tile & TILE_FLIP_Y_FLAG;
					spec->tall = globalTileData[spec->index].tall;
				}
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

					if(entityGetsWaypoints(entity)) {
						s32 numWaypoints = readS32(file);

						if(numWaypoints > 0) {
							entity->waypoints = pushArray(&arena, Waypoint, numWaypoints);

							for(s32 wpIndex = 0; wpIndex < numWaypoints; wpIndex++) {
								entity->waypoints[wpIndex].p = readV2(file);

								if (wpIndex < numWaypoints - 1) {
									entity->waypoints[wpIndex].next = entity->waypoints + (wpIndex + 1);
								}
							}
						}
					}

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

		setBackgroundTexture(&backgroundTextures, Background_marine, renderGroup);

		tiles = pushArray(&arena, TileSpec, mapHeightInTiles * mapWidthInTiles);

		for(s32 tileY = 0; tileY < mapHeightInTiles; tileY++) {
			for(s32 tileX = 0; tileX < mapWidthInTiles; tileX++) {
				TileSpec* spec = tiles + (tileY * mapWidthInTiles + tileX);
				spec->index = -1;
				spec->flipX = spec->flipY = false;
				spec->tall = false;
			}
		}
	}

	bool32 gridVisible = true;

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

		if(camera.scale == 1) {
			BackgroundTexture* backgroundTexture = getBackgroundTexture(&backgroundTextures);
			drawBackgroundTexture(backgroundTexture, renderGroup, &camera, gameWindowSize, mapWidthInTiles * TILE_WIDTH_IN_METERS);
		}

		if(gridVisible) {
			for(s32 rowIndex = 0; rowIndex <= mapHeightInTiles; rowIndex++) {
				V2 lineStart = v2(0, gridSize.y * rowIndex);
				V2 lineEnd = v2(mapWidthInTiles * gridSize.x, lineStart.y);

				drawScaledLine(renderGroup, lineColor, lineStart, lineEnd, lineThickness, dashSize, spaceSize, &camera);
			}

			for(s32 colIndex = 0; colIndex <= mapWidthInTiles; colIndex++) {
				V2 lineStart = v2(gridSize.x * colIndex, 0);
				V2 lineEnd = v2(lineStart.x, mapHeightInTiles * gridSize.y);

				drawScaledLine(renderGroup, lineColor, lineStart, lineEnd, lineThickness, dashSize, spaceSize, &camera);
			}
		}

		if(input.z.justPressed) {
			gridVisible = !gridVisible;
		}

		if(input.c.justPressed) {
			camera.scale = 1;
			oldScale = 1;
		}

		if(input.num[1].justPressed) {
			setBackgroundTexture(&backgroundTextures, Background_marine, renderGroup);
		}
		if(input.num[2].justPressed) {
			setBackgroundTexture(&backgroundTextures, Background_sunset, renderGroup);
		}

		if(cursorMode == CursorMode_moveEntity && movingEntity) {
			if(movingEntity->waypoints && input.n.justPressed) {
				cursorMode = CursorMode_editWaypoints;
			}
		}

		if(selectedTileSpec) {
			if(input.g.justPressed) {
				selectedTileSpec->flipX = !selectedTileSpec->flipX;
			}

			if(input.h.justPressed) {
				selectedTileSpec->flipY = !selectedTileSpec->flipY;
			}
		}

		for(s32 tileY = 0; tileY < mapHeightInTiles; tileY++) {
			for(s32 tileX = 0; tileX < mapWidthInTiles; tileX++) {
				TileSpec* spec = tiles + (tileY * mapWidthInTiles + tileX);

				if(spec->index >= 0) {
					Texture* tex = tileAtlas + spec->index;

					V2 tileSize;

					if(spec->tall) {
						tileSize = tallTileSize;
					} else {
						tileSize = shortTileSize;
					}

					V2 tileOffset = v2(0, 0);

					if(spec->tall && spec->flipY) {
						tileOffset.y = (TILE_HEIGHT_WITHOUT_OVERHANG_IN_METERS - TILE_HEIGHT_IN_METERS);
					}

					V2 tileCenter = hadamard(v2(tileX, tileY), gridSize) + tileSize * 0.5 + tileOffset;

					drawScaledTex(renderGroup, tex, &camera, tileCenter, tileSize, spec->flipX, spec->flipY);					
				}
			}
		}

		for(s32 entityIndex = 0; entityIndex < entityCount; entityIndex++) {
			Entity* entity = entities + entityIndex;

			V2 entityCenter = getRectCenter(entity->bounds);
			V2 unscaledSize = getRectSize(entity->bounds);

			V2 mouseP = unprojectP(&camera, input.mouseInWorld);
			if(pointInsideRect(entity->bounds, mouseP)) {

			}

			drawScaledTex(renderGroup, entity->tex, &camera, entityCenter, unscaledSize, entity->flipX, entity->flipY);	
		}

		//NOTE: Draw the panel

		camera.scale = 1;
		pushClipRect(renderGroup, panelBounds);
		pushFilledRect(renderGroup, renderGroup->windowBounds, createColor(255, 255, 255, 255));

		if(clickedInside(&input, panelBounds)) {
			selectedTileSpec = NULL;
			cursorMode = CursorMode_moveEntity;
		}


		double maxTileY = 0;

		for(s32 tileIndex = 0; tileIndex < tileAtlasCount; tileIndex++) {
			V2 tileSpacing = v2(0.1, 0.1);

			TileData* data = globalTileData + tileIndex;

			V2 tileSize;

			if(data->tall) {
				tileSize = tallTileSize;
			} else {
				tileSize = shortTileSize;
			}

			V2 tileMin = tileSpacing + panelBounds.min + hadamard(tileSpacing + tileSize, v2(tileIndex % 3, tileIndex / 3));
			R2 tileBounds = r2(tileMin, tileMin + tileSize);

			if(tileBounds.max.y > maxTileY) maxTileY = tileBounds.max.y;

			if(clickedInside(&input, tileBounds)) {
				selectedTileSpec = pushStruct(&arena, TileSpec);
				selectedTileSpec->index = tileIndex;
				selectedTileSpec->flipX = false;
				selectedTileSpec->flipY = false;
				selectedTileSpec->tall = data->tall;
				cursorMode = CursorMode_stampTile;
			}

			Texture* tex = tileAtlas + tileIndex;

			pushTexture(renderGroup, tex, tileBounds, false, false, DrawOrder_gui);
		}

		for(s32 specIndex = 0; specIndex < arrayCount(entitySpecs); specIndex++) {
			EntitySpec* spec = entitySpecs + specIndex;

			V2 specMin = spec->panelP + panelBounds.min + v2(0, maxTileY);
			R2 specBounds = r2(specMin, specMin + spec->size * spec->panelScale);

			if(clickedInside(&input, specBounds)) {
				selectedEntitySpecIndex = specIndex;
				cursorMode = CursorMode_stampEntity;
			}

			Texture* tex = entityTextureAtlas + specIndex;

			pushTexture(renderGroup, tex, specBounds, false, false, DrawOrder_gui);
		}


		//NOTE: Draw things which can cross between the game and panel
		camera.scale = oldScale;
		pushDefaultClipRect(renderGroup);

		bool mouseInGame = pointInsideRect(gameBounds, input.mouseInMeters);

		if(cursorMode == CursorMode_stampTile) {
			if(selectedTileSpec) {
				V2 tileSize;

				if(selectedTileSpec->tall) {
					tileSize = tallTileSize;
				} else {
					tileSize = shortTileSize;
				}

				R2 tileBounds = rectCenterDiameter(input.mouseInMeters, tileSize);

				if(input.leftMouse.pressed && mouseInGame) {
					setTile(tiles, mapWidthInTiles, mapHeightInTiles, &camera, &input, gridSize, selectedTileSpec);
				}


				Texture* tex = tileAtlas + selectedTileSpec->index;

				pushTexture(renderGroup, tex, tileBounds, selectedTileSpec->flipX != 0, selectedTileSpec->flipY != 0, DrawOrder_gui);
			}
		}
		else if(cursorMode == CursorMode_stampEntity) {
			if(selectedEntitySpecIndex >= 0) {
				EntitySpec* spec = entitySpecs + selectedEntitySpecIndex;
				Texture* tex = entityTextureAtlas + selectedEntitySpecIndex;

				R2 entityBounds = rectCenterDiameter(input.mouseInMeters, spec->size);

				if(input.leftMouse.justPressed && mouseInGame) {
					Entity* entity = entities + entityCount++;
					entity->type = spec->type;
					entity->drawOrder = spec->drawOrder;

					V2 center = unprojectP(&camera, input.mouseInWorld);

					entity->bounds = rectCenterDiameter(center, spec->size);
					entity->tex = tex;

					if(entityGetsWaypoints(entity)) {
						entity->waypoints = allocateWaypoint(&arena, center);
					}
				}
				
				pushTexture(renderGroup, tex, entityBounds, false, false, DrawOrder_gui);
			}
		} else if(cursorMode == CursorMode_editWaypoints) {
			if(movingEntity) {
				if(input.leftMouse.justPressed) {
					Waypoint* wp = movingEntity->waypoints;

					if(wp) {
						while(wp->next) {
							wp = wp->next;
						}

						V2 center = unprojectP(&camera, input.mouseInWorld);
						wp->next = allocateWaypoint(&arena, center);
					}
				}

				if(input.m.justPressed) {
					Waypoint* wp = movingEntity->waypoints;

					if(wp->next) {
						while(wp->next) {
							Waypoint* next = wp->next;

							if(!next->next) {
								wp->next = NULL;
								break;
							}

							wp = next;
						}
					}
				}

				V2 wpSize = v2(1, 1) * 0.05;

				if(input.rightMouse.justPressed) {
					V2 mouse = unprojectP(&camera, input.mouseInWorld);

					for(Waypoint* wp = movingEntity->waypoints; wp; wp = wp->next) {
						R2 wpBounds = rectCenterRadius(wp->p, wpSize);
						
						if(pointInsideRect(wpBounds, mouse)) {
							movingWaypoint = wp;
							break;
						}
					}
				}

				if(movingWaypoint) {
					if(input.rightMouse.pressed) {
						movingWaypoint->p += input.dMouseMeters;
					} else {
						movingWaypoint = NULL;
					}
				}

				Waypoint* lastPoint = movingEntity->waypoints;
				Waypoint* firstPoint = movingEntity->waypoints;

				if(lastPoint) {
					Waypoint* wp = movingEntity->waypoints->next;

					if(wp) {
						while(true) {
							Waypoint* wp1 = lastPoint;
							Waypoint* wp2 = wp;

							if(!wp2) {
								wp2 = firstPoint;
							}

							pushDashedLine(renderGroup, RED, wp1->p, wp2->p, 0.02, 0.05, 0.05, true);

							if(!wp) {
								break;
							}

							lastPoint = wp;
							wp = wp->next;
						}
					}
				}

				for(Waypoint* wp = movingEntity->waypoints; wp; wp = wp->next) {
					R2 wpBounds = rectCenterRadius(wp->p, wpSize);
					pushFilledRect(renderGroup, wpBounds, BLUE, true);
				}
			}
		}

		Entity* clickedEntity = NULL;
		s32 clickedEntityIndex = -1;

		for(s32 entityIndex = 0; entityIndex < entityCount; entityIndex++) {
			Entity* entity = entities + entityIndex;

			V2 mouseP = unprojectP(&camera, input.mouseInWorld);

			if(pointInsideRect(entity->bounds, mouseP)) {
				clickedEntity = entity;
				clickedEntityIndex = entityIndex;
				break;
			}
		}

		if(input.rightMouse.pressed && cursorMode != CursorMode_editWaypoints) {
			if(clickedEntity) {
				if(clickedEntity == movingEntity) {
					movingEntity = NULL;
					cursorMode = CursorMode_moveEntity;
				}

				entities[clickedEntityIndex] = entities[entityCount-- - 1];
				clickedEntity = NULL;
				clickedEntityIndex = -1;
			} else {
				setTile(tiles, mapWidthInTiles, mapHeightInTiles, &camera, &input, gridSize, NULL);
			}
		}

		bool maintainMovingEntity = false;
		if(input.leftMouse.pressed) {
			if(cursorMode == CursorMode_moveEntity) {
				if(input.leftMouse.justPressed) {
					movingEntity = clickedEntity;
				}

				if(movingEntity) {
					V2 oldCenter = getRectCenter(movingEntity->bounds);

					V2 center = unprojectP(&camera, input.mouseInWorld);
					movingEntity->bounds = reCenterRect(movingEntity->bounds, center);

					V2 dCenter = center - oldCenter;

					for (Waypoint* wp = movingEntity->waypoints; wp; wp = wp->next) {
						wp->p += dCenter;
					}


					maintainMovingEntity = true;
				}
			}
		}
		if(!maintainMovingEntity && cursorMode != CursorMode_editWaypoints) {
			movingEntity = NULL;
		}

		if(input.x.justPressed) {
			s32 playerIndex = -1;

			for(s32 entityIndex = 0; entityIndex < entityCount; entityIndex++) {
				Entity* entity = entities + entityIndex;

				if(entity->type == EntityType_player) {
					playerIndex = entityIndex;
					break;
				}
			}

			if(playerIndex >= 0) {
				entities[entityCount] = entities[playerIndex];
				entities[playerIndex] = entities[entityCount - 1];
				entities[entityCount - 1] = entities[entityCount];
				entities[entityCount] = {};
			}

			FILE* file = fopen(saveFileName, "wb");
			assert(file);

			writeS32(file, mapWidthInTiles);
			writeS32(file, mapHeightInTiles);

			writeS32(file, backgroundTextures.curBackgroundType);

			for(s32 tileY = 0; tileY < mapHeightInTiles; tileY++) {
				for(s32 tileX = 0; tileX < mapWidthInTiles; tileX++) {
					TileSpec* spec = tiles + (tileY * mapWidthInTiles + tileX);
					s32 tile = spec->index;

					if(spec->flipX) {
						tile |= TILE_FLIP_X_FLAG;
					}
					if(spec->flipY) tile |= TILE_FLIP_Y_FLAG;

					writeS32(file, tile);
				}
			}

			writeS32(file, entityCount);

			for(s32 entityIndex = 0; entityIndex < entityCount; entityIndex++) {
				Entity* entity = entities + entityIndex;
				writeS32(file, entity->type);
				writeV2(file, getRectCenter(entity->bounds));

				if(entity->waypoints) {
					s32 wpCount = 0;

					for(Waypoint* wp = entity->waypoints; wp; wp = wp->next) {
						wpCount++;
					}

					writeS32(file, wpCount);

					for(Waypoint* wp = entity->waypoints; wp; wp = wp->next) {
						writeV2(file, wp->p);
					}
				}
			}

			fclose(file);
		}

		glClear(GL_COLOR_BUFFER_BIT);
		drawRenderGroup(renderGroup, NULL);
		
		moveCamera(&input, &camera, gameWindowSize);
		oldScale = camera.scale;

		SDL_GL_SwapWindow(window);
	}

	return 0;
}