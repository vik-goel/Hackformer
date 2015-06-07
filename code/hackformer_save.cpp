void writeS32(FILE* file, s32 value) {
	fprintf(file, "%d ", value);
	// size_t numElementsWritten = fwrite(&value, sizeof(value), 1, file);
	// assert(numElementsWritten == 1);
}

void writeU32(FILE* file, u32 value) {
	fprintf(file, "%u ", value);
	// size_t numElementsWritten = fwrite(&value, sizeof(value), 1, file);
	// assert(numElementsWritten == 1);
}

void writeDouble(FILE* file, double value) {
	fprintf(file, "%f ", value);
	// size_t numElementsWritten = fwrite(&value, sizeof(value), 1, file);
	// assert(numElementsWritten == 1);
}

void writeString(FILE* file, char* value) {
	fprintf(file, " \"");
	fprintf(file, value);
	fprintf(file, "\" ");

	// s32 len = strlen(value);
	// writeS32(file, len);
	// size_t numElementsWritten = fwrite(value, sizeof(char), len, file);
	// assert(numElementsWritten == len);
}

void writeV2(FILE* file, V2 value) {
	writeDouble(file, value.x);
	writeDouble(file, value.y);
}

void writeR2(FILE* file, R2 value) {
	writeV2(file, value.min);
	writeV2(file, value.max);
}

#define writeLinkedListCount(file, list) {s32 listCount = 0; auto listNodePtr = (list); while(listNodePtr) { listCount++; listNodePtr = listNodePtr->next;} writeS32((file), listCount); }

void writeHitboxes(FILE* file, Hitbox* hitboxes) {
	writeLinkedListCount(file, hitboxes);

	while(hitboxes) {
		writeV2(file, hitboxes->collisionSize);
		writeV2(file, hitboxes->collisionOffset);

		hitboxes = hitboxes->next;
	}
}

void writeRefNode(FILE* file, RefNode* refNode) {
	writeLinkedListCount(file, refNode);

	while(refNode) {
		writeS32(file, refNode->ref);
		refNode = refNode->next;
	}
}

void writeConsoleField(FILE* file, ConsoleField* field) {
	if(field) {
		writeS32(file, field->type);
		writeU32(file, field->flags);
		writeString(file, field->name);

		writeS32(file, field->numValues);

		if(field->type == ConsoleField_double) {
			for(s32 i = 0; i < field->numValues; i++) {
				writeDouble(file, field->doubleValues[i]);
			}
		}
		else if(field->type == ConsoleField_s32) {
			for(s32 i = 0; i < field->numValues; i++) {
				writeS32(file, field->s32Values[i]);
			}
		}
		else if(field->type == ConsoleField_followsWaypoints) {
			Waypoint* wp = field->curWaypoint;
			
			s32 count = (wp != NULL);
			wp = wp->next;

			while(wp && wp != field->curWaypoint) {
				count++;
				wp = wp->next;
			}

			writeS32(file, count);

			wp = field->curWaypoint;

			while(wp && count > 0) {
				writeV2(file, wp->p);
				wp = wp->next;
				count--;
			}

			writeDouble(file, field->waypointDelay);
		}

		writeS32(file, field->selectedIndex);
		writeS32(file, field->initialIndex);
		writeS32(file, field->tweakCost);
		writeV2(file, field->p);
		writeV2(file, field->offs);
		writeS32(file, field->numChildren);

		for(s32 childIndex = 0; childIndex < field->numChildren; childIndex++) {
			writeConsoleField(file, field->children[childIndex]);
		}

		writeDouble(file, field->childYOffs); 
	} else {
		writeS32(file, -1);
	}
}

void writeMessages(FILE* file, Messages* messages) {
	if(messages) {
		writeS32(file, messages->count);
		writeS32(file, messages->selectedIndex);

		for(s32 textIndex = 0; textIndex <= messages->count; textIndex++) {
			writeString(file, messages->text[textIndex]);
		}
	} else {
		writeS32(file, -1);
	}
}

void writeTexture(FILE* file, Texture texture) {
	writeS32(file, texture.dataIndex);
}

void writeAnimNode(FILE* file, AnimNode anim) {
	writeS32(file, anim.dataIndex);
}

void writeCharacterAnim(FILE* file, CharacterAnim anim) {
	writeS32(file, anim.dataIndex);
}

void writeEntity(FILE* file, Entity* entity) {
	writeS32(file, entity->ref);
	writeS32(file, entity->type);
	writeU32(file, entity->flags);
	writeV2(file, entity->p);
	writeV2(file, entity->dP);
	writeV2(file, entity->renderSize);
	writeS32(file, entity->drawOrder);
	writeHitboxes(file, entity->hitboxes);
	writeR2(file, entity->clickBox);
	writeS32(file, entity->numFields);

	for(s32 fieldIndex = 0; fieldIndex < entity->numFields; fieldIndex++) {
		writeConsoleField(file, entity->fields[fieldIndex]);
	}

	writeRefNode(file, entity->groundReferenceList);
	writeRefNode(file, entity->ignorePenetrationList);

	writeDouble(file, entity->emissivity);
	writeDouble(file, entity->spotLightAngle);
	writeDouble(file, entity->shootTimer);
	writeS32(file, entity->targetRef);
	writeS32(file, entity->shooterRef);
	writeV2(file, entity->startPos);
	writeS32(file, entity->tileXOffset);
	writeS32(file, entity->tileYOffset);
	writeS32(file, entity->jumpCount);
	writeDouble(file, entity->timeSinceLastOnGround);

	writeMessages(file, entity->messages);

	writeDouble(file, entity->animTime);
	writeAnimNode(file, entity->currentAnim);
	writeAnimNode(file, entity->nextAnim);
	writeTexture(file, entity->defaultTex);
	writeCharacterAnim(file, entity->characterAnim);
}

void writeFieldSpec(FILE* file, FieldSpec* spec) {
	writeV2(file, spec->mouseOffset);
	writeS32(file, spec->blueEnergy);
}

void writeAnimation(FILE* file, Animation* anim) {
	writeS32(file, anim->numFrames);

	if(anim->numFrames) {
		for(s32 frameIndex = 0; frameIndex < anim->numFrames; frameIndex++) {
			writeTexture(file, anim->frames[frameIndex]);
		}

		writeDouble(file, anim->secondsPerFrame);
		writeS32(file, anim->pingPong);
		writeS32(file, anim->reverse);
	}
}

void writeCamera(FILE* file, Camera* camera) {
	writeV2(file, camera->p);
	writeV2(file, camera->newP);
	writeS32(file, camera->moveToTarget);
	writeS32(file, camera->deferredMoveToTarget);
	writeDouble(file, camera->scale);
}

void writeButton(FILE* file, Button* button) {
	writeS32(file, button->selected);
	writeDouble(file, button->scale);
}

void writePauseMenu(FILE* file, PauseMenu* menu) {
	writeDouble(file, menu->animCounter);
	writeButton(file, &menu->quit);
	writeButton(file, &menu->restart);
	writeButton(file, &menu->resume);
	writeButton(file, &menu->settings);
}

void writeDock(FILE* file, Dock* dock) {
	writeDouble(file, dock->subDockYOffs);
}

void writeMemoryArena(FILE* file, MemoryArena* arena) {
	writeS32(file, (s32)arena->allocated);

	size_t numElementsWritten = fwrite(arena->base, sizeof(char), arena->allocated, file);
	assert(numElementsWritten == arena->allocated);
}

void saveGame(GameState* gameState, char* fileName) {
	FILE* file = fopen(fileName, "w");
	assert(file);

	writeS32(file, gameState->screenType);
	writeS32(file, gameState->refCount);
	writeCamera(file, &gameState->camera);
	writeRefNode(file, gameState->targetRefs);
	writeS32(file, gameState->consoleEntityRef);
	writeS32(file, gameState->playerRef);
	writeS32(file, gameState->playerDeathRef);
	writeS32(file, gameState->loadNextLevel);
	writeS32(file, gameState->doingInitialSim);

	writeConsoleField(file, gameState->timeField);
	writeConsoleField(file, gameState->gravityField);
	writeConsoleField(file, gameState->swapField);
	writeV2(file, gameState->swapFieldP);
	writeFieldSpec(file, &gameState->fieldSpec);

	writeV2(file, gameState->mapSize);
	writeV2(file, gameState->worldSize);
	writeV2(file, gameState->gravity);
	writeV2(file, gameState->playerStartP);
	writeV2(file, gameState->playerDeathStartP);
	writeV2(file, gameState->playerDeathSize);

	writeAnimNode(file, gameState->playerHack);
	writeCharacterAnim(file, gameState->playerAnim);
	writeCharacterAnim(file, gameState->playerDeathAnim);
	writeCharacterAnim(file, gameState->virus1Anim);
	writeCharacterAnim(file, gameState->flyingVirusAnim);

	writeTexture(file, gameState->bgTex);
	writeTexture(file, gameState->mgTex);
	writeTexture(file, gameState->blueEnergyTex);
	writeTexture(file, gameState->laserBolt);
	writeTexture(file, gameState->endPortal);
	writeTexture(file, gameState->laserBaseOff);
	writeTexture(file, gameState->laserBaseOn);
	writeTexture(file, gameState->laserTopOff);
	writeTexture(file, gameState->laserTopOn);
	writeTexture(file, gameState->laserBeam);
	writeTexture(file, gameState->heavyTileTex);

	writeS32(file, gameState->tileAtlasCount);

	for(s32 texIndex = 0; texIndex < gameState->tileAtlasCount; texIndex++) {
		writeTexture(file, gameState->tileAtlas[texIndex]);
	}

	writeS32(file, gameState->textureDataCount);

	for(s32 texIndex = 1; texIndex < gameState->textureDataCount; texIndex++) {
		TextureData* data = gameState->textureData + texIndex;
		assert(data->hasFileName);

		writeString(file, data->fileName);
		writeS32(file, data->normalId > 0);
		writeR2(file, data->uv);
	}

	writeS32(file, gameState->animDataCount);

	for(s32 nodeIndex = 1; nodeIndex < gameState->animDataCount; nodeIndex++) {
		AnimNodeData* data = gameState->animData + nodeIndex;

		writeAnimation(file, &data->intro);
		writeAnimation(file, &data->main);
		writeAnimation(file, &data->outro);
		writeS32(file, data->finishMainBeforeOutro);
	}

	writeS32(file, gameState->characterAnimDataCount);

	for(s32 characterIndex = 1; characterIndex < gameState->characterAnimDataCount; characterIndex++) {
		CharacterAnimData* data = gameState->characterAnimData + characterIndex;

		writeAnimNode(file, data->standAnim);
		writeAnimNode(file, data->jumpAnim);
		writeAnimNode(file, data->shootAnim);
		writeAnimNode(file, data->walkAnim);
	}

	writePauseMenu(file, &gameState->pauseMenu);
	writeDock(file, &gameState->dock);

	writeS32(file, gameState->numEntities);

	for(s32 entityIndex = 0; entityIndex < gameState->numEntities; entityIndex++) {
		Entity* entity = gameState->entities + entityIndex;
		writeEntity(file, entity);
	}

	if(getEntityByRef(gameState, gameState->consoleEntityRef)) {
		writeMemoryArena(file, &gameState->hackSaveStorage);
	}

	fclose(file);
}

s32 readS32(FILE* file) {
	s32 result = 0;
	fscanf (file, "%d", &result);

	// size_t numElementsRead = fread(&result, sizeof(result), 1, file);
	// assert(numElementsRead == 1);

	return result;  
}

u32 readU32(FILE* file) {
	u32 result = 0;
	fscanf (file, "%u", &result);

	// size_t numElementsRead = fread(&result, sizeof(result), 1, file);
	// assert(numElementsRead == 1);

	return result;  
}

double readDouble(FILE* file) {
	double result = 0;

	fscanf(file, "%lf", &result);

	// size_t numElementsRead = fread(&result, sizeof(result), 1, file);

	// if(numElementsRead != 1) {

	// int r = feof(file);
	// int q = ferror(file);
	// } 

	// assert(numElementsRead == 1);

	return result;  
}

void readString(FILE* file, char* buffer) {
	char tempBuffer[1024];
	s32 offset = 0;
	bool32 firstWord = true;

	while(true) {
		fscanf (file, "%s", tempBuffer);

		s32 strSize = strlen(tempBuffer) - 2; //-2 for the ""
		memcpy(buffer + offset, tempBuffer + firstWord, strSize);

		offset += strSize;
		firstWord = false;

		if(tempBuffer[strSize + 1] == '\"') {
			break;
		}
		else {
			buffer[offset++] = tempBuffer[strSize + 1];
			buffer[offset++] = ' ';
		}
	}

	buffer[offset] = 0;

	// s32 len = readS32(file);
	// size_t numElementsRead = fread(buffer, sizeof(char), len, file);
	// assert(numElementsRead == len);
}

V2 readV2(FILE* file) {
	V2 result = {};
	result.x = readDouble(file);
	result.y = readDouble(file);
	return result;
}

R2 readR2(FILE* file) {
	R2 result = {};
	result.min = readV2(file);
	result.max = readV2(file);
	return result;
}

ConsoleField* readConsoleField(FILE* file, GameState* gameState) {
	ConsoleField* field = NULL;
	s32 type = readS32(file);

	if(type >= 0) {
		field = createConsoleField_(gameState);
		*field = {};

		field->type = (ConsoleFieldType)type;
		field->flags = readU32(file);
		readString(file, field->name);

		field->numValues = readS32(file);

		if(field->type == ConsoleField_double) {
			for(s32 i = 0; i < field->numValues; i++) {
				field->doubleValues[i] = readDouble(file);
			}
		}
		else if(field->type == ConsoleField_s32) {
			for(s32 i = 0; i < field->numValues; i++) {
				field->s32Values[i] = readS32(file);
			}
		}
		else if(field->type == ConsoleField_followsWaypoints) {
			s32 numWaypoints = readS32(file);
			Waypoint* prevPoint = NULL;

			for(s32 waypointIndex = 0; waypointIndex < numWaypoints; waypointIndex++) {
				V2 p = readV2(file);
				Waypoint* wp = createWaypoint(gameState, p);

				if(prevPoint) prevPoint->next = wp;
				else field->curWaypoint = wp;

				prevPoint = wp;
				if(waypointIndex == numWaypoints - 1) wp->next = field->curWaypoint;
			}

			field->waypointDelay = readDouble(file);
		}

		field->selectedIndex = readS32(file);
		field->initialIndex = readS32(file);
		field->tweakCost = readS32(file);

		field->p = readV2(file);
		field->offs = readV2(file);

		field->next = NULL;

		field->numChildren = readS32(file);

		for(s32 fieldIndex = 0; fieldIndex < field->numChildren; fieldIndex++) {
			field->children[fieldIndex] = readConsoleField(file, gameState);
		}

		field->childYOffs = readDouble(file);
	}

	return field;
}

RefNode* readRefNode(FILE* file, GameState* gameState) {
	RefNode* result = NULL;

	s32 numNodes = readS32(file);

	for(s32 nodeIndex = 0; nodeIndex < numNodes; nodeIndex++) {
		s32 ref = readS32(file);
		RefNode* node = refNode(gameState, ref);

		node->next = result;
		result = node;
	}

	return result;
}

Messages* readMessages(FILE* file, GameState* gameState) {
	Messages* result = NULL;

	s32 count = readS32(file);

	if(count >= 0) {
		s32 selectedIndex = readS32(file);
		char text[10][100];

		for(s32 textIndex = 0; textIndex <= count; textIndex++) {
			readString(file, text[textIndex]);
		}

		result = createMessages(gameState, text, count, selectedIndex);
	}

	return result;
}

Texture readTexture(FILE* file) {
	Texture result = {};
	result.dataIndex = readS32(file);
	return result;
}

AnimNode readAnimNode(FILE* file) {
	AnimNode result = {};
	result.dataIndex = readS32(file);
	return result;
}

CharacterAnim readCharacterAnim(FILE* file) {
	CharacterAnim result = {};
	result.dataIndex = readS32(file);
	return result;
}

void readEntity(FILE* file, GameState* gameState, s32 entityIndex) {
	s32 ref = readS32(file);

	Entity* entity = addEntity(gameState, ref, entityIndex);
	entity->type = (EntityType)readS32(file);
	entity->flags = readU32(file);
	entity->p = readV2(file);
	entity->dP = readV2(file);
	entity->renderSize = readV2(file);
	entity->drawOrder = (DrawOrder)readS32(file);

	s32 numHitboxes = readS32(file);

	for(s32 hitboxIndex = 0; hitboxIndex < numHitboxes; hitboxIndex++) {
		Hitbox* hitbox = createHitbox(gameState);
		hitbox->collisionSize = readV2(file);
		hitbox->collisionOffset = readV2(file);

		hitbox->next = entity->hitboxes;
		entity->hitboxes = hitbox;
	}

	entity->clickBox = readR2(file);
	entity->numFields = readS32(file);

	for(s32 fieldIndex = 0; fieldIndex < entity->numFields; fieldIndex++) {
		entity->fields[fieldIndex] = readConsoleField(file, gameState);
	}

	entity->groundReferenceList = readRefNode(file, gameState);
	entity->ignorePenetrationList = readRefNode(file, gameState);

	entity->emissivity = (float)readDouble(file);
	entity->spotLightAngle = readDouble(file);
	entity->shootTimer = readDouble(file);

	entity->targetRef = readS32(file);
	entity->shooterRef = readS32(file);

	entity->startPos = readV2(file);

	entity->tileXOffset = readS32(file);
	entity->tileYOffset = readS32(file);
	entity->jumpCount = readS32(file);
	entity->timeSinceLastOnGround = readDouble(file);

	entity->messages = readMessages(file, gameState);

	entity->animTime = readDouble(file);
	entity->currentAnim = readAnimNode(file);
	entity->nextAnim = readAnimNode(file);
	entity->defaultTex = readTexture(file);
	entity->characterAnim = readCharacterAnim(file);

	addToSpatialPartition(entity, gameState);
}

Camera readCamera(FILE* file) {
	Camera result = {};

	result.p = readV2(file);
	result.newP = readV2(file);

	result.moveToTarget = readS32(file);
	result.deferredMoveToTarget = readS32(file);
	result.scale = readDouble(file);

	return result;
}

Animation readAnimation(FILE* file, GameState* gameState) {
	Animation result = {};

	result.numFrames = readS32(file);

	if(result.numFrames) {
		result.frames = pushArray(&gameState->permanentStorage, Texture, result.numFrames);

		for(s32 frameIndex = 0; frameIndex < result.numFrames; frameIndex++) {
			result.frames[frameIndex] = readTexture(file);
		}

		result.secondsPerFrame = readDouble(file);
		result.pingPong = readS32(file);
		result.reverse = readS32(file);
	}

	return result;
}

void readButton(FILE* file, Button* button) {
	button->selected = readS32(file);
	button->scale = readDouble(file);
}

void readPauseMenu(FILE* file, PauseMenu* menu) {
	menu->animCounter = readDouble(file);
	readButton(file, &menu->quit);
	readButton(file, &menu->restart);
	readButton(file, &menu->resume);
	readButton(file, &menu->settings);
}

void readDock(FILE* file, Dock* dock) {
	dock->subDockYOffs = readDouble(file);
}

void readMemoryArena(FILE* file, MemoryArena* arena) {
	arena->allocated = readS32(file);

	size_t numElementsRead = fread(arena->base, sizeof(char), arena->allocated, file);

	s32 res = feof(file);

	assert(numElementsRead == arena->allocated);
}

void loadGame(GameState* gameState, char* fileName) {
	FILE* file = fopen(fileName, "r");
	assert(file);

	gameState->screenType = (ScreenType)readS32(file);
	gameState->refCount = readS32(file);
	gameState->camera = readCamera(file);
	
	gameState->targetRefs = readRefNode(file, gameState);
	gameState->consoleEntityRef = readS32(file);
	gameState->playerRef = readS32(file);
	gameState->playerDeathRef = readS32(file);

	gameState->loadNextLevel = readS32(file);
	gameState->doingInitialSim = readS32(file);

	gameState->timeField = readConsoleField(file, gameState);
	gameState->gravityField = readConsoleField(file, gameState);
	gameState->swapField = readConsoleField(file, gameState);
	gameState->swapFieldP = readV2(file);
	gameState->fieldSpec.mouseOffset = readV2(file);
	gameState->fieldSpec.blueEnergy = readS32(file);

	gameState->mapSize = readV2(file);
	gameState->worldSize = readV2(file);
	gameState->gravity = readV2(file);
	gameState->playerStartP = readV2(file);
	gameState->playerDeathStartP = readV2(file);
	gameState->playerDeathSize = readV2(file);

	gameState->playerHack = readAnimNode(file);
	gameState->playerAnim = readCharacterAnim(file);
	gameState->playerDeathAnim = readCharacterAnim(file);
	gameState->virus1Anim = readCharacterAnim(file);
	gameState->flyingVirusAnim = readCharacterAnim(file);

	gameState->bgTex = readTexture(file);
	gameState->mgTex = readTexture(file);
	gameState->blueEnergyTex = readTexture(file);
	gameState->laserBolt = readTexture(file);
	gameState->endPortal = readTexture(file);
	gameState->laserBaseOff = readTexture(file);
	gameState->laserBaseOn = readTexture(file);
	gameState->laserTopOff = readTexture(file);
	gameState->laserTopOn = readTexture(file);
	gameState->laserBeam = readTexture(file);
	gameState->heavyTileTex = readTexture(file);

	gameState->tileAtlasCount = readS32(file);

	if(gameState->tileAtlasCount) {
		//TODO: If there is already a tile atlas this will leak
		gameState->tileAtlas = pushArray(&gameState->permanentStorage, Texture, gameState->tileAtlasCount);

		for(s32 texIndex = 0; texIndex < gameState->tileAtlasCount; texIndex++) {
			gameState->tileAtlas[texIndex] = readTexture(file);
		}
	}

	gameState->textureDataCount = readS32(file);

	//TODO: Many textures that need to be loaded in have probably already been loaded in
	//		If any textures are unecessary delete them
	//		This just orphans all of the already loaded textures
	for(s32 texIndex = 1; texIndex < gameState->textureDataCount; texIndex++) {
		TextureData* data = gameState->textureData + texIndex;
		readString(file, data->fileName);
		data->hasFileName = true;

		bool32 loadNormalMap = readS32(file);
		TextureData* dataToCopy = NULL;

		for(s32 testIndex = 1; testIndex < texIndex; testIndex++) {
			TextureData* testData = gameState->textureData + testIndex;

			if(strcmp(testData->fileName, data->fileName) == 0) {
				dataToCopy = testData;
				break;
			}
		}

		TextureData loadedData;

		if(!dataToCopy) {
			loadedData = loadPNGTexture(gameState, data->fileName, loadNormalMap);
			dataToCopy = &loadedData;
		}

		assert(dataToCopy);

		data->texId = dataToCopy->texId;
		data->normalId = dataToCopy->normalId;
		data->size = dataToCopy->size;

		data->uv = readR2(file);

		assert(data->texId);
		if(loadNormalMap) assert(data->normalId);
	}

	gameState->animDataCount = readS32(file);

	for(s32 nodeIndex = 1; nodeIndex < gameState->animDataCount; nodeIndex++) {
		AnimNodeData* data = gameState->animData + nodeIndex;
		data->intro = readAnimation(file, gameState);
		data->main = readAnimation(file, gameState);
		data->outro = readAnimation(file, gameState);
		data->finishMainBeforeOutro = readS32(file);
	}

	gameState->characterAnimDataCount = readS32(file);

	for(s32 dataIndex = 1; dataIndex < gameState->characterAnimDataCount; dataIndex++) {
		CharacterAnimData* data = gameState->characterAnimData + dataIndex;

		data->standAnim = readAnimNode(file);
		data->jumpAnim = readAnimNode(file);
		data->shootAnim = readAnimNode(file);
		data->walkAnim = readAnimNode(file);
	}

	readPauseMenu(file, &gameState->pauseMenu);
	readDock(file, &gameState->dock);

	initSpatialPartition(gameState);

	gameState->numEntities = readS32(file);

	for(s32 entityIndex = 0; entityIndex < gameState->numEntities; entityIndex++) {
		readEntity(file, gameState, entityIndex);
	}

	if(getEntityByRef(gameState, gameState->consoleEntityRef)) {
		readMemoryArena(file, &gameState->hackSaveStorage);
	}

	fclose(file);
}

void saveConsoleFieldToArena(MemoryArena* arena, ConsoleField* field) {
	if(field) {
		ConsoleField* save = pushStruct(arena, ConsoleField);
		*save = *field;

		for(s32 fieldIndex = 0; fieldIndex < field->numChildren; fieldIndex++) {
			saveConsoleFieldToArena(arena, field->children[fieldIndex]);
		}
	} else {
		pushElem(arena, s32, -1);
	}
}

void saveEntityToArena(MemoryArena* arena, Entity* entity) {
	EntityHackSave* save = pushStruct(arena, EntityHackSave);

	save->p = entity->p;
	save->tileXOffset = entity->tileXOffset;
	save->tileYOffset = entity->tileYOffset;

	if(entity->messages) {
		save->messagesSelectedIndex =  entity->messages->selectedIndex;
	} else {
		save->messagesSelectedIndex = -1;
	}

	save->timeSinceLastOnGround = entity->timeSinceLastOnGround;

	save->numFields = entity->numFields;

	for(s32 fieldIndex = 0; fieldIndex < entity->numFields; fieldIndex++) {
		saveConsoleFieldToArena(arena, entity->fields[fieldIndex]);
	}
}

void saveGameToArena(GameState* gameState) {
	MemoryArena* arena = &gameState->hackSaveStorage;
	arena->allocated = 0;

	pushElem(arena, s32, gameState->fieldSpec.blueEnergy);
	pushElem(arena, V2, gameState->gravity);

	saveConsoleFieldToArena(arena, gameState->timeField);
	saveConsoleFieldToArena(arena, gameState->gravityField);
	saveConsoleFieldToArena(arena, gameState->swapField);

	RefNode* targetNode = gameState->targetRefs;
	s32 targetNodeCount = 0;

	while(targetNode) {
		targetNodeCount++;
		targetNode = targetNode->next;
	}
	pushElem(arena, s32, targetNodeCount);

	targetNode = gameState->targetRefs;

	while(targetNode) {
		pushElem(arena, s32, targetNode->ref);
		targetNode = targetNode->next;
	}

	pushElem(arena, s32, gameState->numEntities);

	for(s32 entityIndex = 0; entityIndex < gameState->numEntities; entityIndex++) {
		saveEntityToArena(arena, gameState->entities + entityIndex);
	}
}

#define readElemPtr(readPtr, type) (type*)(readPtr); (readPtr) = (type*)(readPtr) + 1
#define readElem(readPtr, type) *readElemPtr((readPtr), type)

;
void readConsoleFieldFromArena(void** readPtr, ConsoleField** fieldPtr, GameState* gameState) {
	void* read = *readPtr;

	if(*(s32*)read >= 0) {
		ConsoleField* field = *fieldPtr;

		if(!field) {
			field = createConsoleField_(gameState);
			*fieldPtr = field;
		}

		assert(field);

		*field = *(ConsoleField*)read;
		*readPtr = (ConsoleField*)read + 1;

		for(s32 fieldIndex = 0; fieldIndex < field->numChildren; fieldIndex++) {
			readConsoleFieldFromArena(readPtr, &field->children[fieldIndex], gameState);
		}
	} else {
		*readPtr = (s32*)read + 1;

		if(*fieldPtr) {
			freeConsoleField(*fieldPtr, gameState);
			*fieldPtr = NULL;
		}
	}
}

void readEntityFromArena(void** readPtr, Entity* entity, GameState* gameState) {
	EntityHackSave* save = readElemPtr(*readPtr, EntityHackSave);

	setEntityP(entity, save->p, gameState);
	entity->tileXOffset = save->tileXOffset;
	entity->tileYOffset = save->tileYOffset;
	if(entity->messages)entity->messages->selectedIndex = save->messagesSelectedIndex;
	entity->timeSinceLastOnGround = save->timeSinceLastOnGround;
	entity->numFields = save->numFields;

	for(s32 fieldIndex = 0; fieldIndex < entity->numFields; fieldIndex++) {
		readConsoleFieldFromArena(readPtr, &entity->fields[fieldIndex], gameState);
	}
}

void loadGameFromArena(GameState* gameState) {
	MemoryArena* arena = &gameState->hackSaveStorage;
	void* readPtr = arena->base;

	gameState->fieldSpec.blueEnergy = readElem(readPtr, s32);
	gameState->gravity = readElem(readPtr, V2);

	readConsoleFieldFromArena(&readPtr, &gameState->timeField, gameState);
	readConsoleFieldFromArena(&readPtr, &gameState->gravityField, gameState);
	readConsoleFieldFromArena(&readPtr, &gameState->swapField, gameState);

	if(gameState->targetRefs) {
		freeRefNode(gameState->targetRefs, gameState);
		gameState->targetRefs = NULL;
	}

	s32 targetRefsCount = readElem(readPtr, s32);
	for(s32 targetRefIndex = 0; targetRefIndex < targetRefsCount; targetRefIndex++) {
		s32 ref = readElem(readPtr, s32);
		gameState->targetRefs = refNode(gameState, ref, gameState->targetRefs);
	}

	gameState->numEntities = readElem(readPtr, s32);

	for(s32 entityIndex = 0; entityIndex < gameState->numEntities; entityIndex++) {
		readEntityFromArena(&readPtr, gameState->entities + entityIndex, gameState);
	}

	assert(readPtr == (char*)arena->base + arena->allocated);
}

s32 getEnergyLoss(GameState* gameState) {
	s32 result = 0;

	if(getEntityByRef(gameState, gameState->consoleEntityRef)) {
		MemoryArena* arena = &gameState->hackSaveStorage;
		assert(arena->allocated);

		s32 oldEnergy = *(s32*)arena->base;
		s32 newEnergy = gameState->fieldSpec.blueEnergy;
		result = oldEnergy - newEnergy;
	}

	return result;
}
