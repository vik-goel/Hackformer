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

void setTile(EditorState* state, V2 gridSize, TileSpec* value) {
	V2 mousePos = unprojectP(&state->camera, state->input.mouseInWorld);

	s32 tileX = (s32)(mousePos.x / gridSize.x);
	s32 tileY = (s32)(mousePos.y / gridSize.y);

	if(tileX >= 0 && tileY >= 0 && tileX < state->mapWidthInTiles && tileY < state->mapHeightInTiles) {
		TileSpec* spec = state->tiles + (tileY * state->mapWidthInTiles + tileX);

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

char* textEditFileName = "maps/editor_text.txt";

void initText(EditorState* state, Entity* entity, V2 p) {
	assert(entity->messages);
	assert(entity->type == EntityType_text);

	for(int i = 0; i < entity->messages->count; i++) {
		entity->messages->textures[i] = createText(state->renderGroup, state->textFont, entity->messages->text[i]);
	}

	entity->drawOrder = DrawOrder_text;
	entity->bounds = rectCenterDiameter(p, entity->messages->textures[entity->messages->selectedIndex].size);
}

Entity* addEditorEntity(EditorState* state, EntityType type, DrawOrder drawOrder, V2 size = v2(0, 0), Texture* tex = NULL) {
	Messages* messages = NULL;

	if(type == EntityType_text) {
		messages = pushStruct(&state->arena, Messages);

		FILE* file = fopen(textEditFileName, "r");

		if(file) {
			messages->count = readS32(file);

			if(messages->count <= 0 || messages->count > 10) return NULL;

			messages->selectedIndex = readS32(file) - 1;
			if(messages->selectedIndex < 0 || messages->selectedIndex >= messages->count) return NULL;

			for(s32 i = 0; i < messages->count; i++) {
				readString(file, messages->text[i]);
				messages->textures[i] = createText(state->renderGroup, state->textFont, messages->text[i]);
			}

			size = messages->textures[messages->selectedIndex].size;

			fclose(file);
		} else {
			return NULL;
		}
	}

	Entity* entity = state->entities + state->entityCount++;

	entity->type = type;
	entity->drawOrder = drawOrder;

	V2 center = unprojectP(&state->camera, state->input.mouseInWorld);

	entity->bounds = rectCenterDiameter(center, size);
	entity->tex = tex;

	if(entityGetsWaypoints(entity)) {
		entity->waypoints = allocateWaypoint(&state->arena, center);
	}

	entity->messages = messages;

	return entity;
}

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
	ENTITY(checkPoint, 0.59, 1.25, "checkpoint", 2.5, 6.5, 1)
};
#undef ENTITY

void streamHackFile(IOStream* stream, EditorState* state) {
	streamElem(stream, state->mapWidthInTiles);
	streamElem(stream, state->mapHeightInTiles);
	streamElem(stream, state->initialEnergy);

	streamHackAbilities(stream, &state->hackAbilities);

	if(stream->reading) {
		BackgroundType type;
		streamElem(stream, type);
		setBackgroundTexture(&state->backgroundTextures, type, state->renderGroup);
	}
	else {
		streamElem(stream, state->backgroundTextures.curBackgroundType);
	}
	

	if(stream->reading) {
		state->tiles = pushArray(&state->arena, TileSpec, state->mapHeightInTiles * state->mapWidthInTiles);
	}

	for(s32 tileY = 0; tileY < state->mapHeightInTiles; tileY++) {
		for(s32 tileX = 0; tileX < state->mapWidthInTiles; tileX++) {
			TileSpec* spec = state->tiles + tileY * state->mapWidthInTiles + tileX;

			if(stream->reading) {
				s32 val;
				streamElem(stream, val);

				if(val == -1) {
					spec->index = -1;
				}
				else {
					spec->index = val & TILE_INDEX_MASK;
					spec->flipX = val & TILE_FLIP_X_FLAG;
					spec->flipY = val & TILE_FLIP_Y_FLAG;
					spec->tall = globalTileData[spec->index].tall;
				}
			}
			else {
				if(spec->index >= 0) {
					s32 val = spec->index & TILE_INDEX_MASK;
					if(spec->flipX) val |= TILE_FLIP_X_FLAG;
					if(spec->flipY) val |= TILE_FLIP_Y_FLAG;

					streamElem(stream, val);
				}
				else {
					streamValue(stream, s32, -1);
				}
			}
		}
	}

	streamElem(stream, state->entityCount);

	for(s32 i = 0; i < state->entityCount; i++) {
		Entity* entity = state->entities + i;
		streamElem(stream, entity->type);

		V2 p = getRectCenter(entity->bounds);
		streamV2(stream, &p);

		if(entity->type == EntityType_text) {
			streamMessages(stream, &entity->messages, &state->arena);
		}

		if(stream->reading) {
			if(entity->type == EntityType_text) {
				initText(state, entity, p);
			} else {
				bool foundSpec = false;

				for(s32 specIndex = 0; specIndex < arrayCount(entitySpecs); specIndex++) {
					EntitySpec* spec = entitySpecs + specIndex;

					if(spec->type == entity->type) {
						entity->drawOrder = spec->drawOrder;
						entity->tex = state->entityTextureAtlas + specIndex;
						entity->bounds = rectCenterDiameter(p, spec->size);

						foundSpec = true;
						break;
					}
				}

				assert(foundSpec);
			}
		}

		if(entityGetsWaypoints(entity)) {
			streamWaypoints(stream, &entity->waypoints, &state->arena, true);
		}
	}	

	freeIostream(stream);
}

void saveHackFile(EditorState* state, char* fileName) {
	IOStream stream = createIostream(NULL, fileName, false);
	streamHackFile(&stream, state);
}

bool loadHackFile(EditorState* state, char* fileName) {
	IOStream stream = createIostream(NULL, fileName, true, false);

	if(stream.file) {
		streamHackFile(&stream, state);
		return true;
	}

	return false;
}

s32 show5ButtonDialog(SDL_Window* window, char* message) {
	s32 result = 0;

	SDL_MessageBoxButtonData buttons[5] = {};

	for(s32 i = 0; i < arrayCount(buttons); i++) {
		buttons[i].buttonid = i;
	}

	buttons[0].text = "cancel";
	buttons[1].text = "increase by 1";
	buttons[2].text = "increase by 5";
	buttons[3].text = "decrease by 1";
	buttons[4].text = "decrease by 5";

	SDL_MessageBoxData messageBoxData = {};
	messageBoxData.flags = SDL_MESSAGEBOX_INFORMATION;
	messageBoxData.window = window;
	messageBoxData.title = "editor";
	messageBoxData.message = message;
	messageBoxData.numbuttons = arrayCount(buttons);
	messageBoxData.buttons = buttons;
	messageBoxData.colorScheme = NULL;

	s32 buttonId = 0;
	s32 showResult = SDL_ShowMessageBox(&messageBoxData, &buttonId);

	if(showResult < 0) {
		const char* error = SDL_GetError();
		s32 breakHere = 4;
	}
	else if(buttonId > 0) {
		switch(buttonId) {
			case 1:
				result = 1;
				break;
			case 2:
				result = 5;
				break;
			case 3:
				result = -1;
				break;
			case 4:
				result = -5;
				break;
			InvalidDefaultCase;
		}
	}

	return result;
}

void showAbilitiesDialog(SDL_Window* window, HackAbilities* abilities) {
	char message[1000];
	sprintf(message, "Edit Fields: %d, Move Tiles: %d, Move Fields %d, Clone Fields %d, Global Hacks %d", 
					  abilities->editFields, abilities->moveTiles, abilities->moveFields, abilities->cloneFields, 
					  abilities->globalHacks);

	SDL_MessageBoxButtonData buttons[6] = {};

	for(s32 i = 0; i < arrayCount(buttons); i++) {
		buttons[i].buttonid = i;
	}

	buttons[5].text = "edit fields";
	buttons[4].text = "move tiles";
	buttons[3].text = "move fields";
	buttons[2].text = "clone fields";
	buttons[1].text = "global hacks";
	buttons[0].text = "cancel";

	SDL_MessageBoxData messageBoxData = {};
	messageBoxData.flags = SDL_MESSAGEBOX_INFORMATION;
	messageBoxData.window = window;
	messageBoxData.title = "toggle_abilities";
	messageBoxData.message = message;
	messageBoxData.numbuttons = arrayCount(buttons);
	messageBoxData.buttons = buttons;
	messageBoxData.colorScheme = NULL;

	s32 buttonId = 0;
	s32 showResult = SDL_ShowMessageBox(&messageBoxData, &buttonId);

	if(showResult < 0) {
		const char* error = SDL_GetError();
		s32 breakHere = 4;
	}
	else {
		switch(buttonId) {
			case 5:
				abilities->editFields = !abilities->editFields;
				break;
			case 4:
				abilities->moveTiles = !abilities->moveTiles;
				break;
			case 3:
				abilities->moveFields = !abilities->moveFields;
				break;
			case 2:
				abilities->cloneFields = !abilities->cloneFields;
				break;
			case 1:
				abilities->globalHacks = !abilities->globalHacks;
				break;
			case 0:
				break;

			InvalidDefaultCase;
		}
	}
}

int main(int argc, char* argv[]) {
	s32 panelWidth = 250;
	s32 windowWidth = 1280 + panelWidth, windowHeight = 720;
	SDL_Window* window = createWindow(windowWidth, windowHeight);

	EditorState state = {};
	initInputKeyCodes(&state.input);

	initArena(&state.arena, MEGABYTES(200), true);

	initCamera(&state.camera);
	double oldScale = state.camera.scale;

	state.textures = pushArray(&state.arena, Texture, MAX_TEXTURES);
	state.texturesCount = 1;

	initAssets(&state.assets);

	state.renderGroup  = createRenderGroup(8 * 1024 * 1024, &state.arena, TEMP_PIXELS_PER_METER, windowWidth, windowHeight, 
													&state.camera, state.textures, &state.texturesCount, &state.assets);
	

	RenderGroup* renderGroup = state.renderGroup;
	renderGroup->enabled = true;

	state.textFont = loadTextFont(renderGroup, &state.arena);

	bool running = true;

	state.tileAtlas = state.textures + state.texturesCount;
	state.tileAtlasCount = loadTileAtlas(renderGroup, state.textures, &state.texturesCount);

	state.entities = pushArray(&state.arena, Entity, 5000);
	state.entityCount = 0;

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

	char buffer[2000];

	state.entityTextureAtlas = state.textures + state.texturesCount;
	for(s32 specIndex = 0; specIndex < arrayCount(entitySpecs); specIndex++) {
		EntitySpec* spec = entitySpecs + specIndex;
		char* fileName = spec->fileName;
		sprintf(buffer, "editor_res/%s.png", fileName);
		loadPNGTexture(renderGroup, buffer, false);
	}

	s32 selectedEntitySpecIndex = 0;
	Entity* editText = NULL;
	Entity* movingEntity = NULL;
	Waypoint* movingWaypoint = NULL;

	initBackgroundTextures(&state.backgroundTextures);

	char* saveFileName = "maps/edit.hack";

	if(loadHackFile(&state, saveFileName)) {

	} else {
		state.mapWidthInTiles = 60;
		state.mapHeightInTiles = 12;

		state.hackAbilities.editFields = true;
		state.hackAbilities.moveTiles = true;
		state.hackAbilities.moveFields = true;
		state.hackAbilities.cloneFields = true;
		state.hackAbilities.globalHacks = true;

		setBackgroundTexture(&state.backgroundTextures, Background_marine, renderGroup);

		state.tiles = pushArray(&state.arena, TileSpec, state.mapHeightInTiles * state.mapWidthInTiles);

		for(s32 tileY = 0; tileY < state.mapHeightInTiles; tileY++) {
			for(s32 tileX = 0; tileX < state.mapWidthInTiles; tileX++) {
				TileSpec* spec = state.tiles + (tileY * state.mapWidthInTiles + tileX);
				spec->index = -1;
				spec->flipX = spec->flipY = false;
				spec->tall = false;
			}
		}
	}

	bool32 gridVisible = true;

	Camera* camera = &state.camera;
	Input* input = &state.input;

	while(running) {
		pollInput(&state.input, &running, windowHeight, TEMP_PIXELS_PER_METER, &state.camera);

		if(input->esc.justPressed) {
			cursorMode = CursorMode_moveEntity;
		}
		
		if(input->t.justPressed) {
			if(movingEntity && movingEntity->type == EntityType_text) {
				Messages* messages = movingEntity->messages;
				assert(messages);

				FILE* file = fopen(textEditFileName, "w");
				assert(file);

				writeS32(file, messages->count);
				fprintf(file, "\n");
				writeS32(file, messages->selectedIndex + 1);
				fprintf(file, "\n");

				for(s32 i = 0; i < messages->count; i++) {
					writeString(file, messages->text[i]);
					fprintf(file, "\n");
				}

				fclose(file);
			} else {
				addEditorEntity(&state, EntityType_text, DrawOrder_text);
			}
		}

		if(input->k.justPressed) {
			char message[1000];
			sprintf(message, "map width in tiles: %d", state.mapWidthInTiles);

			s32 oldWidth = state.mapWidthInTiles;
			s32 newWidth = oldWidth + show5ButtonDialog(window, message);			

			if(newWidth < 1) newWidth = 1;

			if(newWidth != oldWidth) {
				TileSpec* newTiles = pushArray(&state.arena, TileSpec, state.mapHeightInTiles * newWidth);

				for(s32 tileY = 0; tileY < state.mapHeightInTiles; tileY++) {
					for (s32 tileX = 0; tileX < newWidth; tileX++) {
						TileSpec* newTile = newTiles + (tileY * newWidth + tileX);

						TileSpec oldTile = {};
						oldTile.index = -1;

						if(tileX < oldWidth) {
							oldTile = state.tiles[tileY * oldWidth + tileX];
						}

						*newTile = oldTile;
					}
				}

				state.tiles = newTiles;
				state.mapWidthInTiles = newWidth;
			}
		}

		if(input->j.justPressed) {
			char message[1000];
			sprintf(message, "initial energy: %d", state.initialEnergy);
			state.initialEnergy += show5ButtonDialog(window, message);	
			if(state.initialEnergy < 0) state.initialEnergy = 0;		
		}

		if(input->l.justPressed) {
			showAbilitiesDialog(window, &state.hackAbilities);
		}

		//NOTE: Draw the game

		camera->scale = 1;
		pushClipRect(renderGroup, gameBounds);
		pushFilledRect(renderGroup, renderGroup->windowBounds, createColor(150, 150, 150, 255));
		camera->scale = oldScale;

		if(camera->scale == 1) {
			BackgroundTexture* backgroundTexture = getBackgroundTexture(&state.backgroundTextures);
			drawBackgroundTexture(backgroundTexture, renderGroup, camera, gameWindowSize, state.mapWidthInTiles * TILE_WIDTH_IN_METERS);
		}

		if(gridVisible) {
			for(s32 rowIndex = 0; rowIndex <= state.mapHeightInTiles; rowIndex++) {
				V2 lineStart = v2(0, gridSize.y * rowIndex);
				V2 lineEnd = v2(state.mapWidthInTiles * gridSize.x, lineStart.y);

				drawScaledLine(renderGroup, lineColor, lineStart, lineEnd, lineThickness, dashSize, spaceSize, camera);
			}

			for(s32 colIndex = 0; colIndex <= state.mapWidthInTiles; colIndex++) {
				V2 lineStart = v2(gridSize.x * colIndex, 0);
				V2 lineEnd = v2(lineStart.x, state.mapHeightInTiles * gridSize.y);

				drawScaledLine(renderGroup, lineColor, lineStart, lineEnd, lineThickness, dashSize, spaceSize, camera);
			}
		}

		if(input->z.justPressed) {
			gridVisible = !gridVisible;
		}

		if(input->c.justPressed) {
			camera->scale = 1;
			oldScale = 1;
		}

		if(input->num[1].justPressed) {
			setBackgroundTexture(&state.backgroundTextures, Background_marine, renderGroup);
		}
		if(input->num[2].justPressed) {
			setBackgroundTexture(&state.backgroundTextures, Background_sunset, renderGroup);
		}

		if(cursorMode == CursorMode_moveEntity && movingEntity) {
			if(movingEntity->waypoints && input->n.justPressed) {
				cursorMode = CursorMode_editWaypoints;
			}
		}

		if(selectedTileSpec) {
			if(input->g.justPressed) {
				selectedTileSpec->flipX = !selectedTileSpec->flipX;
			}

			if(input->h.justPressed) {
				selectedTileSpec->flipY = !selectedTileSpec->flipY;
			}
		}

		for(s32 tileY = 0; tileY < state.mapHeightInTiles; tileY++) {
			for(s32 tileX = 0; tileX < state.mapWidthInTiles; tileX++) {
				TileSpec* spec = state.tiles + (tileY * state.mapWidthInTiles + tileX);

				if(spec->index >= 0) {
					Texture* tex = state.tileAtlas + spec->index;

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

					drawScaledTex(renderGroup, tex, camera, tileCenter, tileSize, spec->flipX, spec->flipY);					
				}
			}
		}

		for(s32 entityIndex = 0; entityIndex < state.entityCount; entityIndex++) {
			Entity* entity = state.entities + entityIndex;

			Texture* tex = entity->tex;
			if(!tex) {
				tex = entity->messages->textures + entity->messages->selectedIndex;
			}

			V2 entityCenter = getRectCenter(entity->bounds);
			V2 unscaledSize = getRectSize(entity->bounds);

			drawScaledTex(renderGroup, tex, camera, entityCenter, unscaledSize, entity->flipX, entity->flipY);	
		}

		//NOTE: Draw the panel

		camera->scale = 1;
		pushClipRect(renderGroup, panelBounds);
		pushFilledRect(renderGroup, renderGroup->windowBounds, createColor(255, 255, 255, 255));

		if(clickedInside(input, panelBounds)) {
			selectedTileSpec = NULL;
			cursorMode = CursorMode_moveEntity;
		}


		double maxTileY = 0;

		for(s32 tileIndex = 0; tileIndex < state.tileAtlasCount; tileIndex++) {
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

			if(clickedInside(input, tileBounds)) {
				selectedTileSpec = pushStruct(&state.arena, TileSpec);
				selectedTileSpec->index = tileIndex;
				selectedTileSpec->flipX = false;
				selectedTileSpec->flipY = false;
				selectedTileSpec->tall = data->tall;
				cursorMode = CursorMode_stampTile;
			}

			Texture* tex = state.tileAtlas + tileIndex;

			pushTexture(renderGroup, tex, tileBounds, false, false, DrawOrder_gui);
		}

		for(s32 specIndex = 0; specIndex < arrayCount(entitySpecs); specIndex++) {
			EntitySpec* spec = entitySpecs + specIndex;

			V2 specMin = spec->panelP + panelBounds.min + v2(0, maxTileY);
			R2 specBounds = r2(specMin, specMin + spec->size * spec->panelScale);

			if(clickedInside(&state.input, specBounds)) {
				selectedEntitySpecIndex = specIndex;
				cursorMode = CursorMode_stampEntity;
			}

			Texture* tex = state.entityTextureAtlas + specIndex;

			pushTexture(renderGroup, tex, specBounds, false, false, DrawOrder_gui);
		}


		//NOTE: Draw things which can cross between the game and panel
		camera->scale = oldScale;
		pushDefaultClipRect(renderGroup);

		bool mouseInGame = pointInsideRect(gameBounds, input->mouseInMeters);

		if(cursorMode == CursorMode_stampTile) {
			if(selectedTileSpec) {
				V2 tileSize;

				if(selectedTileSpec->tall) {
					tileSize = tallTileSize;
				} else {
					tileSize = shortTileSize;
				}

				R2 tileBounds = rectCenterDiameter(input->mouseInMeters, tileSize);

				if(input->leftMouse.pressed && mouseInGame) {
					setTile(&state, gridSize, selectedTileSpec);
				}


				Texture* tex = state.tileAtlas + selectedTileSpec->index;

				pushTexture(renderGroup, tex, tileBounds, selectedTileSpec->flipX != 0, selectedTileSpec->flipY != 0, DrawOrder_gui);
			}
		}
		else if(cursorMode == CursorMode_stampEntity) {
			if(selectedEntitySpecIndex >= 0) {
				EntitySpec* spec = entitySpecs + selectedEntitySpecIndex;
				Texture* tex = state.entityTextureAtlas + selectedEntitySpecIndex;

				R2 entityBounds = rectCenterDiameter(input->mouseInMeters, spec->size);

				if(input->leftMouse.justPressed && mouseInGame) {
					addEditorEntity(&state, spec->type, spec->drawOrder, spec->size, tex);
				}
				
				pushTexture(renderGroup, tex, entityBounds, false, false, DrawOrder_gui);
			}
		} else if(cursorMode == CursorMode_editWaypoints) {
			if(movingEntity) {
				if(input->leftMouse.justPressed) {
					Waypoint* wp = movingEntity->waypoints;

					if(wp) {
						while(wp->next) {
							wp = wp->next;
						}

						V2 center = unprojectP(camera, input->mouseInWorld);
						wp->next = allocateWaypoint(&state.arena, center);
					}
				}

				if(input->m.justPressed) {
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

				if(input->rightMouse.justPressed) {
					V2 mouse = unprojectP(camera, input->mouseInWorld);

					for(Waypoint* wp = movingEntity->waypoints; wp; wp = wp->next) {
						R2 wpBounds = rectCenterRadius(wp->p, wpSize);
						
						if(pointInsideRect(wpBounds, mouse)) {
							movingWaypoint = wp;
							break;
						}
					}
				}

				if(movingWaypoint) {
					if(input->rightMouse.pressed) {
						movingWaypoint->p += input->dMouseMeters;
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

		for(s32 entityIndex = 0; entityIndex < state.entityCount; entityIndex++) {
			Entity* entity = state.entities + entityIndex;

			V2 mouseP = unprojectP(camera, input->mouseInWorld);

			if(pointInsideRect(entity->bounds, mouseP)) {
				clickedEntity = entity;
				clickedEntityIndex = entityIndex;
				break;
			}
		}

		if(input->rightMouse.pressed && cursorMode != CursorMode_editWaypoints) {
			if(clickedEntity) {
				if(clickedEntity == movingEntity) {
					movingEntity = NULL;
					cursorMode = CursorMode_moveEntity;
				}

				state.entities[clickedEntityIndex] = state.entities[state.entityCount-- - 1];
				clickedEntity = NULL;
				clickedEntityIndex = -1;
			} else {
				setTile(&state, gridSize, NULL);
			}
		}

		bool maintainMovingEntity = false;
		if(input->leftMouse.pressed) {
			if(cursorMode == CursorMode_moveEntity) {
				if(input->leftMouse.justPressed) {
					movingEntity = clickedEntity;
				}

				if(movingEntity) {
					V2 oldCenter = getRectCenter(movingEntity->bounds);

					V2 center = unprojectP(camera, input->mouseInWorld);
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

		if(input->x.justPressed) {
			s32 playerIndex = -1;

			for(s32 entityIndex = 0; entityIndex < state.entityCount; entityIndex++) {
				Entity* entity = state.entities + entityIndex;

				if(entity->type == EntityType_player) {
					playerIndex = entityIndex;
					break;
				}
			}

			if(playerIndex >= 0) {
				state.entities[state.entityCount] = state.entities[playerIndex];
				state.entities[playerIndex] = state.entities[state.entityCount - 1];
				state.entities[state.entityCount - 1] = state.entities[state.entityCount];
				state.entities[state.entityCount] = {};
			}

			saveHackFile(&state, saveFileName);
		}

		glClear(GL_COLOR_BUFFER_BIT);
		drawRenderGroup(renderGroup, NULL);
		
		moveCamera(input, camera, gameWindowSize);
		oldScale = camera->scale;

		SDL_GL_SwapWindow(window);
	}

	return 0;
}