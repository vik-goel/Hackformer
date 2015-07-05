#define writeLinkedListCount(file, list) {s32 listCount = 0; auto listNodePtr = (list); while(listNodePtr) { listCount++; listNodePtr = listNodePtr->next;} writeS32((file), listCount); }

void writeHitboxes(FILE* file, Hitbox* hitboxes) {
	writeLinkedListCount(file, hitboxes);

	while(hitboxes) {
		writeV2(file, hitboxes->collisionSize);
		writeV2(file, hitboxes->collisionOffset);

		writeS32(file, hitboxes->collisionPointsCount);

		for(s32 pIndex = 0; pIndex < hitboxes->collisionPointsCount; pIndex++) {
			writeV2(file, hitboxes->originalCollisionPoints[pIndex]);
		}

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
		} else if(field->type == ConsoleField_bobsVertically) {
			writeDouble(file, field->bobHeight);
			writeS32(file, field->bobbingUp);
			writeS32(file, field->initialBob);
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

void writeTexture(FILE* file, Texture* texture, GameState* gameState) {
	if(texture) {
		s32 dataIndex = ((size_t)texture - (size_t)gameState->textures) / sizeof(Texture);
		assert(dataIndex >= 0 && dataIndex < gameState->texturesCount);

		writeS32(file, dataIndex);
	} else {
		writeS32(file, -1);
	}
}

void writeAnimNode(FILE* file, AnimNode* anim, GameState* gameState) {
	if (anim){
		s32 dataIndex = ((size_t)anim - (size_t)gameState->animNodes) / sizeof(AnimNode);
		assert(dataIndex >= 0 && dataIndex < gameState->animNodesCount);

		writeS32(file, dataIndex);
	} else {
		writeS32(file, -1);
	}
}

void writeCharacterAnim(FILE* file, CharacterAnim* anim, GameState* gameState) {
	if (anim){
		s32 dataIndex = ((size_t)anim - (size_t)gameState->characterAnims) / sizeof(CharacterAnim);
		assert(dataIndex >= 0 && dataIndex < gameState->characterAnimsCount);

		writeS32(file, dataIndex);
	} else {
		writeS32(file, -1);
	}
}

void writeEntity(FILE* file, Entity* entity, GameState* gameState) {
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
	writeAnimNode(file, entity->currentAnim, gameState);
	writeAnimNode(file, entity->nextAnim, gameState);
	writeTexture(file, entity->defaultTex, gameState);
	writeCharacterAnim(file, entity->characterAnim, gameState);
}

void writeFieldSpec(FILE* file, FieldSpec* spec) {
	writeV2(file, spec->mouseOffset);
	writeS32(file, spec->hackEnergy);
}

void writeAnimation(FILE* file, Animation* anim) {
	writeS32(file, anim->numFrames);

	if(anim->numFrames) {
		writeDouble(file, anim->secondsPerFrame);
		writeS32(file, anim->pingPong);
		writeS32(file, anim->reverse);
		writeS32(file, anim->frameWidth);
		writeS32(file, anim->frameHeight);
		writeString(file, anim->fileName);
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
	writeS32(file, gameState->refCount_);
	writeCamera(file, &gameState->camera);
	writeRefNode(file, gameState->targetRefs);
	writeS32(file, gameState->consoleEntityRef);
	writeS32(file, gameState->playerRef);
	writeS32(file, gameState->loadNextLevel);
	writeS32(file, gameState->reloadCurrentLevel);
	writeS32(file, gameState->doingInitialSim);

	writeConsoleField(file, gameState->timeField);
	writeConsoleField(file, gameState->gravityField);
	writeConsoleField(file, gameState->swapField);
	writeV2(file, gameState->swapFieldP);
	writeFieldSpec(file, &gameState->fieldSpec);

	writeV2(file, gameState->mapSize);
	writeV2(file, gameState->worldSize);
	writeV2(file, gameState->gravity);

	writeS32(file, gameState->backgroundTextures.curBackgroundType);

	writePauseMenu(file, &gameState->pauseMenu);
	writeDock(file, &gameState->dock);

	writeS32(file, gameState->numEntities);

	for(s32 entityIndex = 0; entityIndex < gameState->numEntities; entityIndex++) {
		Entity* entity = gameState->entities + entityIndex;
		writeEntity(file, entity, gameState);
	}

	if(getEntityByRef(gameState, gameState->consoleEntityRef)) {
		writeMemoryArena(file, &gameState->hackSaveStorage);
	}

	fclose(file);
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
		} else if(field->type == ConsoleField_bobsVertically) {
			field->bobHeight = readDouble(file);
			field->bobbingUp = readS32(file);
			field->initialBob = readS32(file);
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

Texture* readTexture(FILE* file, GameState* gameState) {
	Texture* result = NULL;

	s32 dataIndex = readS32(file);

	if(dataIndex >= 0) {
		assert(dataIndex < gameState->texturesCount);
		result = gameState->textures + dataIndex;
	}

	return result;
}

AnimNode* readAnimNode(FILE* file, GameState* gameState) {
	AnimNode* result = NULL;

	s32 dataIndex = readS32(file);

	if(dataIndex >= 0) {
		assert(dataIndex < gameState->animNodesCount);
		result = gameState->animNodes + dataIndex;
	}

	return result;
}

CharacterAnim* readCharacterAnim(FILE* file, GameState* gameState) {
	CharacterAnim* result = NULL;

	s32 dataIndex = readS32(file);

	if(dataIndex >= 0) {
		assert(dataIndex < gameState->characterAnimsCount);
		result = gameState->characterAnims + dataIndex;
	}

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
		Hitbox* hitbox = createUnzeroedHitbox(gameState);
		hitbox->collisionSize = readV2(file);
		hitbox->collisionOffset = readV2(file);

		hitbox->collisionPointsCount = readS32(file);

		for(s32 pIndex = 0; pIndex < hitbox->collisionPointsCount; pIndex++) {
			hitbox->originalCollisionPoints[pIndex] = readV2(file);
		}

		hitbox->storedRotation = INVALID_STORED_HITBOX_ROTATION;

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
	entity->currentAnim = readAnimNode(file, gameState);
	entity->nextAnim = readAnimNode(file, gameState);
	entity->defaultTex = readTexture(file, gameState);
	entity->characterAnim = readCharacterAnim(file, gameState);

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

	if(result.numFrames > 0) {
		result.secondsPerFrame = readDouble(file);
		result.pingPong = readS32(file);
		result.reverse = readS32(file);
		result.frameWidth = readS32(file);
		result.frameHeight = readS32(file);

		char fileName[arrayCount(result.fileName)];
		readString(file, fileName);

		strcpy(result.fileName, fileName);

		s32 numFrames = 0;
		result.frames = extractTextures(gameState->renderGroup, &gameState->permanentStorage, fileName, result.frameWidth, 
										result.frameHeight, 0, &numFrames);
		assert(numFrames == result.numFrames);
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
	gameState->refCount_ = readS32(file);
	gameState->camera = readCamera(file);
	
	gameState->targetRefs = readRefNode(file, gameState);
	gameState->consoleEntityRef = readS32(file);
	gameState->playerRef = readS32(file);

	gameState->loadNextLevel = readS32(file);
	gameState->reloadCurrentLevel = readS32(file);
	gameState->doingInitialSim = readS32(file);

	gameState->timeField = readConsoleField(file, gameState);
	gameState->gravityField = readConsoleField(file, gameState);
	gameState->swapField = readConsoleField(file, gameState);
	gameState->swapFieldP = readV2(file);
	gameState->fieldSpec.mouseOffset = readV2(file);
	gameState->fieldSpec.hackEnergy = readS32(file);

	gameState->mapSize = readV2(file);
	gameState->worldSize = readV2(file);
	gameState->gravity = readV2(file);

	gameState->backgroundTextures.curBackgroundType = (BackgroundType)readS32(file);

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

SaveReference* getSaveReference(MemoryArena* arena, s32 saveIndex) {
	SaveMemoryHeader* header = (SaveMemoryHeader*)arena->base;
	SaveReference* result = header->saves + saveIndex;
	return result;
}

s32 getSlotIndex(s32 saveIndex) {
	assert(saveIndex >= 0);

	s32 result = 0;

	if(saveIndex == 1) {
		//The first save should be the same as the 0th save
		result = 0;
	}
	else if(saveIndex > 1) {
		result = (saveIndex - 1) % (MAX_HACK_UNDOS - 1) + 1;
	}

	return result;
}

void updateSaveGameToArena(GameState* gameState) {
	MemoryArena* arena = &gameState->hackSaveStorage;

	s32* numSaveGamesPtr = (s32*)arena->base;
	*numSaveGamesPtr = *numSaveGamesPtr + 1;

	s32 numSaveGames = *numSaveGamesPtr - 1; //Want to 0 index the save games

	if(numSaveGames == 1) {
		//Since we do the save before any changes are made, the first save would be identical to the 
		//0th save. 
		return;
	}

	SaveReference* saveGameReference = getSaveReference(arena, getSlotIndex(numSaveGames));

	s32 slotIndex = numSaveGames;
	size_t totalAllocated = 0;
	size_t oldAllocated = arena->allocated;

	if(numSaveGames >= MAX_HACK_UNDOS) {
		//NOTE: Can't just directly go to the savePtr corresponding to slotIndex
		//		because it may have been corrupted by the last write. Instead, we 
		//		go to the last write and advance by its size to find out where we should be. 
		SaveReference* oneMinusSlotIndexSave = getSaveReference(arena, slotIndex - 1);
		void* saveStart = (char*)oneMinusSlotIndexSave->save + oneMinusSlotIndexSave->size;

		totalAllocated = arena->allocated;
		oldAllocated = arena->allocated = (size_t)saveStart - (size_t)arena->base;
	}

	//NOTE: Update the save game ptr table
	saveGameReference->save = (char*)arena->base + arena->allocated;
	saveGameReference->index = numSaveGames;

	pushElem(arena, s32, gameState->fieldSpec.hackEnergy);
	pushElem(arena, V2, gameState->gravity);

	pushElem(arena, s32, gameState->timeField->selectedIndex);
	pushElem(arena, s32, gameState->gravityField->selectedIndex);

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
		Entity* entity = gameState->entities + entityIndex;
		saveEntityToArena(arena, entity);
	}

	saveGameReference->size = arena->allocated - oldAllocated;

	if(totalAllocated > 0) {
		size_t arenaLimit = (size_t)arena->base + arena->allocated;

		for(s32 checkIndex = slotIndex; checkIndex < MAX_HACK_UNDOS; checkIndex++) {
			SaveReference* checkReference = getSaveReference(arena, checkIndex);

			if((size_t)checkReference->save < arenaLimit) {
				*checkReference = {};
			} else {
				break;
			}
		}

		arena->allocated = totalAllocated;
	}
}

void saveGameToArena(GameState* gameState) {
	MemoryArena* arena = &gameState->hackSaveStorage;
	arena->allocated = 0;

	pushElem(arena, s32, 0); //Reset the number of save games
	arena->allocated += sizeof(void*) * MAX_HACK_UNDOS;

	updateSaveGameToArena(gameState);
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

		clearFlags(field, ConsoleFlag_selected);
		field->offs = v2(0, 0);

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

	for(s32 fieldIndex = save->numFields; fieldIndex < entity->numFields; fieldIndex++) {
		freeConsoleField(entity->fields[fieldIndex], gameState);
	}

	entity->numFields = save->numFields;

	for(s32 fieldIndex = 0; fieldIndex < entity->numFields; fieldIndex++) {
		readConsoleFieldFromArena(readPtr, &entity->fields[fieldIndex], gameState);
	}
}

void readGameFromArena(GameState* gameState, void* readPtr) {
	gameState->fieldSpec.hackEnergy = readElem(readPtr, s32);
	gameState->gravity = readElem(readPtr, V2);

	gameState->timeField->selectedIndex = readElem(readPtr, s32); 
	gameState->gravityField->selectedIndex = readElem(readPtr, s32); 
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
	

	s32 numEntities = readElem(readPtr, s32);
	assert(numEntities == gameState->numEntities);

	for(s32 entityIndex = 0; entityIndex < gameState->numEntities; entityIndex++) {
		readEntityFromArena(&readPtr, gameState->entities + entityIndex, gameState);
	}
}

void loadGameFromArena(GameState* gameState) {
	MemoryArena* arena = &gameState->hackSaveStorage;
	void* readPtr = getSaveReference(arena, 0)->save;
	readGameFromArena(gameState, readPtr);	
}


void undoLastSaveGameFromArena(GameState* gameState) {
	MemoryArena* arena = &gameState->hackSaveStorage;

	s32* numSaveGamesPtr = (s32*)arena->base;
	s32 numSaveGames = *numSaveGamesPtr - 1;

	if(numSaveGames > 0) {
		s32 slotIndex = getSlotIndex(numSaveGames);
		SaveReference* lastSaveReference = getSaveReference(arena, slotIndex);

		bool acceptableReference = numSaveGames == lastSaveReference->index && lastSaveReference->save;

		//they should be the same
		if(numSaveGames == 1) acceptableReference |= lastSaveReference->index == 0;

		if(acceptableReference) {
			readGameFromArena(gameState, lastSaveReference->save);
			*numSaveGamesPtr = *numSaveGamesPtr - 1;
		}
	}
}

s32 getEnergyLoss(GameState* gameState) {
	s32 result = 0;

	if(getEntityByRef(gameState, gameState->consoleEntityRef)) {
		MemoryArena* arena = &gameState->hackSaveStorage;
		assert(arena->allocated);

		s32 oldEnergy = *((s32*)getSaveReference(arena, 0)->save);
		s32 newEnergy = gameState->fieldSpec.hackEnergy;
		result = oldEnergy - newEnergy;
	}

	return result;
}
