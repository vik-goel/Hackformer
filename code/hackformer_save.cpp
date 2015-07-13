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
		} else if(field->type == ConsoleField_shootsAtTarget) {
			writeDouble(file, field->shootTimer);
			writeS32(file, field->shootEntityType);
		} else if(field->type == ConsoleField_spawnsTrawlers) {
			writeDouble(file, field->spawnTimer);
			writeRefNode(file, field->spawnedEntities);
		} else if(field->type == ConsoleField_light) {
			writeV3(file, field->lightColor);
		} else if(field->type == ConsoleField_scansForTargets) {
			writeDouble(file, field->scanStart);
			writeS32(file, field->decreasingScanAngle);
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
		writeDouble(file, field->alpha); 
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

void writeGlowingTexture(FILE* file, GlowingTexture* texture, GameState* gameState) {
	if(texture) {
		s32 dataIndex = ((size_t)texture - (size_t)gameState->glowingTextures) / sizeof(GlowingTexture);
		assert(dataIndex >= 0 && dataIndex < gameState->glowingTexturesCount);

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
	writeDouble(file, entity->rotation);
	writeDouble(file, entity->wheelRotation);
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
	writeDouble(file, entity->alpha);
	writeDouble(file, entity->cloakFactor);
	writeS32(file, entity->targetRef);
	writeS32(file, entity->spawnerRef);
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
	writeGlowingTexture(file, entity->glowingTex, gameState);
	writeCharacterAnim(file, entity->characterAnim, gameState);
}

void writeFieldSpec(FILE* file, FieldSpec* spec) {
	writeV2(file, spec->mouseOffset);
	writeDouble(file, spec->hackEnergy);
}

void writeAnimation(FILE* file, Animation* anim) {
	writeS32(file, anim->numFrames);

	if(anim->numFrames) {
		writeDouble(file, anim->secondsPerFrame);
		writeS32(file, anim->pingPong);
		writeS32(file, anim->reverse);
		writeS32(file, anim->frameWidth);
		writeS32(file, anim->frameHeight);
		writeS32(file, anim->assetId);
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

void offsetSavePtrs(MemoryArena* arena, bool forwards) {
	SaveMemoryHeader* header = (SaveMemoryHeader*)arena->base;

	size_t movement = (size_t)arena->base;
	if(!forwards) movement *= -1;

	for(s32 saveIndex = 0; saveIndex < header->numSaveGames; saveIndex++) {
		SaveReference* ref = header->saves + saveIndex;
		ref->save = (char*)ref->save + movement;
	}
}

void writeMemoryArena(FILE* file, MemoryArena* arena) {
	offsetSavePtrs(arena, false);

	writeSize_t(file, arena->allocated);

	size_t numElementsWritten = fwrite(arena->base, sizeof(char), arena->allocated, file);
	assert(numElementsWritten == arena->allocated);

	offsetSavePtrs(arena, true);
}

void saveGame(GameState* gameState, char* fileName) {
	FILE* file = fopen(fileName, "wb");
	assert(file);

	writeS32(file, gameState->screenType);
	writeS32(file, gameState->refCount_);
	writeCamera(file, &gameState->camera);
	writeRefNode(file, gameState->targetRefs);
	writeRefNode(file, gameState->guardTargetRefs);
	writeS32(file, gameState->consoleEntityRef);
	writeRefNode(file, gameState->fadingOutConsoles);
	writeS32(file, gameState->playerRef);
	writeS32(file, gameState->testEntityRef);
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

	writeDouble(file, gameState->collisionBoundsAlpha);

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
		} else if(field->type == ConsoleField_shootsAtTarget) {
			field->shootTimer = readDouble(file);
			field->shootEntityType = (EntityType)readS32(file);
		} else if(field->type == ConsoleField_spawnsTrawlers) {
			field->spawnTimer = readDouble(file);
			field->spawnedEntities = readRefNode(file, gameState);
		} else if(field->type == ConsoleField_light) {
			field->lightColor = readV3(file);
		} else if(field->type == ConsoleField_scansForTargets) {
			field->scanStart = readDouble(file);
			field->decreasingScanAngle = readS32(file);
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
		field->alpha = readDouble(file);
	}

	return field;
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

GlowingTexture* readGlowingTexture(FILE* file, GameState* gameState) {
	GlowingTexture* result = NULL;

	s32 dataIndex = readS32(file);

	if(dataIndex >= 0) {
		assert(dataIndex < gameState->texturesCount);
		result = gameState->glowingTextures + dataIndex;
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
	entity->rotation = readDouble(file);
	entity->wheelRotation = readDouble(file);
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
	entity->alpha = readDouble(file);
	entity->cloakFactor = readDouble(file);

	entity->targetRef = readS32(file);
	entity->spawnerRef = readS32(file);

	entity->startPos = readV2(file);

	entity->tileXOffset = readS32(file);
	entity->tileYOffset = readS32(file);
	entity->jumpCount = readS32(file);
	entity->timeSinceLastOnGround = readDouble(file);

	entity->messages = readMessages(file, &gameState->levelStorage, &gameState->messagesFreeList);

	entity->animTime = readDouble(file);
	entity->currentAnim = readAnimNode(file, gameState);
	entity->nextAnim = readAnimNode(file, gameState);
	entity->defaultTex = readTexture(file, gameState);
	entity->glowingTex = readGlowingTexture(file, gameState);
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
		result.assetId = (AssetId)readS32(file);

		s32 numFrames = 0;
		result.frames = extractTextures(gameState->renderGroup, result.assetId, result.frameWidth, result.frameHeight, 0, &numFrames);
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
	arena->allocated = readSize_t(file);
	assert(arena->allocated < arena->size);
	size_t numElementsRead = fread(arena->base, sizeof(char), arena->allocated, file);
	assert(numElementsRead == arena->allocated);

	offsetSavePtrs(arena, true);
}

void freeLevel(GameState*);

void loadGame(GameState* gameState, char* fileName) {
	FILE* file = fopen(fileName, "rb");
	assert(file);

	freeLevel(gameState);

	gameState->screenType = (ScreenType)readS32(file);
	gameState->refCount_ = readS32(file);
	gameState->camera = readCamera(file);
	
	gameState->targetRefs = readRefNode(file, gameState);
	gameState->guardTargetRefs = readRefNode(file, gameState);
	gameState->consoleEntityRef = readS32(file);
	gameState->fadingOutConsoles = readRefNode(file, gameState);
	gameState->playerRef = readS32(file);
	gameState->testEntityRef = readS32(file);

	gameState->loadNextLevel = readS32(file);
	gameState->reloadCurrentLevel = readS32(file);
	gameState->doingInitialSim = readS32(file);

	gameState->timeField = readConsoleField(file, gameState);
	gameState->gravityField = readConsoleField(file, gameState);
	gameState->swapField = readConsoleField(file, gameState);
	gameState->swapFieldP = readV2(file);
	gameState->fieldSpec.mouseOffset = readV2(file);
	gameState->fieldSpec.hackEnergy = readDouble(file);

	gameState->mapSize = readV2(file);
	gameState->worldSize = readV2(file);
	gameState->gravity = readV2(file);

	gameState->backgroundTextures.curBackgroundType = (BackgroundType)readS32(file);

	readPauseMenu(file, &gameState->pauseMenu);
	readDock(file, &gameState->dock);

	gameState->collisionBoundsAlpha = readDouble(file);

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

void saveRefNodeToArena(MemoryArena* arena, RefNode* refNode) {
	RefNode* node = refNode;
	s32 count = 0;

	while(node) {
		count++;
		node = node->next;
	}

	pushElem(arena, s32, count);

	node = refNode;

	while(node) {
		pushElem(arena, s32, node->ref);
		node = node->next;
	}
}

void saveConsoleFieldToArena(MemoryArena* arena, ConsoleField* field) {
	if(field) {
		ConsoleFieldHackSave* save = pushStruct(arena, ConsoleFieldHackSave);

		save->type = field->type;
		save->flags = field->flags;
		strcpy(save->name, field->name);
		save->numValues = field->numValues;
		save->selectedIndex = field->selectedIndex;
		save->initialIndex = field->initialIndex;
		save->tweakCost = field->tweakCost;
		save->p = field->p;
		save->offs = field->offs;
		save->numChildren = field->numChildren;
		save->childYOffs = field->childYOffs;

		if(save->type == ConsoleField_double) {
			for(s32 i = 0; i < save->numValues; i++) {
				pushElem(arena, double, field->doubleValues[i]);
			}
		} 
		else if(save->type == ConsoleField_s32) {
			for(s32 i = 0; i < save->numValues; i++) {
				pushElem(arena, s32, field->s32Values[i]);
			}
		}
		else if(save->type == ConsoleField_followsWaypoints) {
			s32 waypointCount = 0;

			Waypoint* wp = field->curWaypoint;

			while(wp) {
				if(wp == field->curWaypoint && waypointCount != 0) break;
				waypointCount++;

				wp = wp->next;
			}

			wp = field->curWaypoint;

			pushElem(arena, s32, waypointCount);

			for(s32 wpIndex = 0; wpIndex < waypointCount; wpIndex++) {
				pushElem(arena, V2, wp->p);
				wp = wp->next;
			}

			pushElem(arena, double, field->waypointDelay);
		}
		else if(save->type == ConsoleField_bobsVertically) {
			pushElem(arena, double, field->bobHeight);
			pushElem(arena, s32, field->bobbingUp);
			pushElem(arena, s32, field->initialBob);
		}
		else if(save->type == ConsoleField_shootsAtTarget) {
			pushElem(arena, double, field->shootTimer);
			pushElem(arena, s32, field->shootEntityType);
		}
		else if(save->type == ConsoleField_spawnsTrawlers) {
			pushElem(arena, double, field->spawnTimer);
			saveRefNodeToArena(arena, field->spawnedEntities);
		} else if(save->type == ConsoleField_light) {
			pushElem(arena, V3, field->lightColor);
		} else if(save->type == ConsoleField_scansForTargets) {
			pushElem(arena, double, field->scanStart);
			pushElem(arena, s32, field->decreasingScanAngle);
		}

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
	save->rotation = entity->rotation;
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

	pushElem(arena, double, gameState->fieldSpec.hackEnergy);
	pushElem(arena, V2, gameState->gravity);

	pushElem(arena, s32, gameState->timeField->selectedIndex);
	pushElem(arena, s32, gameState->gravityField->selectedIndex);

	saveConsoleFieldToArena(arena, gameState->swapField);

	saveRefNodeToArena(arena, gameState->targetRefs);
	saveRefNodeToArena(arena, gameState->guardTargetRefs);

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

void readRefNodeFromArena(GameState* gameState, RefNode** nodePtr, void** readPtr) {
	if(*nodePtr) {
		freeRefNode(*nodePtr, gameState);
		*nodePtr = NULL;
	}

	s32 targetRefsCount = readElem(*readPtr, s32);
	for(s32 targetRefIndex = 0; targetRefIndex < targetRefsCount; targetRefIndex++) {
		s32 ref = readElem(*readPtr, s32);
		*nodePtr = refNode(gameState, ref, *nodePtr);
	}
}

void readConsoleFieldFromArena(void** readPtr, ConsoleField** fieldPtr, GameState* gameState) {
	if(**(s32**)readPtr >= 0) {
		ConsoleField* field = *fieldPtr;

		s32 originalChildrenCount = 0;

		if(field) {
			originalChildrenCount = field->numChildren;
			freeConsoleField_(field, gameState);
		}
		else {
			*fieldPtr = createConsoleField_(gameState);
			field = *fieldPtr;
		}

		ConsoleFieldHackSave* save = readElemPtr(*readPtr, ConsoleFieldHackSave);

		field->type = save->type;
		field->flags = save->flags;
		strcpy(field->name, save->name);
		field->numValues = save->numValues;
		field->selectedIndex = save->selectedIndex;
		field->initialIndex = save->initialIndex;
		field->tweakCost = save->tweakCost;
		field->p = save->p;
		field->offs = save->offs;
		field->numChildren = save->numChildren;
		field->childYOffs = save->childYOffs;

		for(s32 childIndex = save->numChildren; childIndex < originalChildrenCount; childIndex++) {
			freeConsoleField(field->children[childIndex], gameState);
			field->children[childIndex] = NULL;
		}

		if(field->type == ConsoleField_double) {
			for(s32 i = 0; i < field->numValues; i++) {
				field->doubleValues[i] = readElem(*readPtr, double);
			}
		}
		else if(field->type == ConsoleField_s32) {
			for(s32 i = 0; i < field->numValues; i++) {
				field->s32Values[i] = readElem(*readPtr, s32);
			}
		}
		else if(field->type == ConsoleField_followsWaypoints) {
			s32 numWaypoints = readElem(*readPtr, s32);

			Waypoint* firstWp = NULL;
			Waypoint* prevWp = NULL;

			for(s32 wpIndex = 0; wpIndex < numWaypoints; wpIndex++) {
				V2 p = readElem(*readPtr, V2);
				Waypoint* wp = allocateWaypoint(gameState, p);

				if(firstWp) {
					prevWp->next = wp;
				} else {
					firstWp = wp;
				}

				prevWp = wp;
			}

			prevWp->next = firstWp;
			field->curWaypoint = firstWp;

			field->waypointDelay = readElem(*readPtr, double);
		} 
		else if(field->type == ConsoleField_bobsVertically) {
			field->bobHeight = readElem(*readPtr, double);
			field->bobbingUp = readElem(*readPtr, s32);
			field->initialBob = readElem(*readPtr, s32);
		}
		else if(field->type == ConsoleField_shootsAtTarget) {
			field->shootTimer = readElem(*readPtr, double);
			field->shootEntityType = (EntityType)readElem(*readPtr, s32);
		}
		else if(field->type == ConsoleField_spawnsTrawlers) {
			field->spawnTimer = readElem(*readPtr, double);
			readRefNodeFromArena(gameState, &field->spawnedEntities, readPtr); 
		}
		else if(field->type == ConsoleField_light) {
			field->lightColor = readElem(*readPtr, V3);
		} 
		else if(field->type == ConsoleField_scansForTargets) {
			field->scanStart = readElem(*readPtr, double);
			field->decreasingScanAngle = readElem(*readPtr, s32);
		}

		clearFlags(field, ConsoleFlag_selected);
		field->offs = v2(0, 0);

		for(s32 fieldIndex = 0; fieldIndex < field->numChildren; fieldIndex++) {
			readConsoleFieldFromArena(readPtr, field->children + fieldIndex, gameState);
		}
	} else {
		*readPtr = (s32*)(*readPtr) + 1;

		if(*fieldPtr) {
			freeConsoleField(*fieldPtr, gameState);
			*fieldPtr = NULL;
		}
	}
}

void readEntityFromArena(void** readPtr, Entity* entity, GameState* gameState) {
	EntityHackSave* save = readElemPtr((*readPtr), EntityHackSave);

	setEntityP(entity, save->p, gameState);
	entity->rotation = save->rotation;
	entity->tileXOffset = save->tileXOffset;
	entity->tileYOffset = save->tileYOffset;
	if(entity->messages)entity->messages->selectedIndex = save->messagesSelectedIndex;
	entity->timeSinceLastOnGround = save->timeSinceLastOnGround;

	for(s32 fieldIndex = save->numFields; fieldIndex < entity->numFields; fieldIndex++) {
		freeConsoleField(entity->fields[fieldIndex], gameState);
		entity->fields[fieldIndex] = NULL;
	}

	entity->numFields = save->numFields;
	assert(entity->numFields < 8);

	if(entity->type == EntityType_player) {
		s32 breakHere = 5;
	}

	for(s32 fieldIndex = 0; fieldIndex < entity->numFields; fieldIndex++) {
		readConsoleFieldFromArena(readPtr, entity->fields + fieldIndex, gameState);
	}
}

void readGameFromArena(GameState* gameState, void* readPtr) {
	gameState->fieldSpec.hackEnergy = readElem(readPtr, double);
	gameState->gravity = readElem(readPtr, V2);

	gameState->timeField->selectedIndex = readElem(readPtr, s32); 
	gameState->gravityField->selectedIndex = readElem(readPtr, s32); 
	readConsoleFieldFromArena(&readPtr, &gameState->swapField, gameState);

	readRefNodeFromArena(gameState, &gameState->targetRefs, &readPtr);
	readRefNodeFromArena(gameState, &gameState->guardTargetRefs, &readPtr);

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
			freeRefNode(gameState->fadingOutConsoles, gameState);
			gameState->fadingOutConsoles = NULL;

			readGameFromArena(gameState, lastSaveReference->save);
			*numSaveGamesPtr = *numSaveGamesPtr - 1;
		}
	}
}

double getEnergyLoss(GameState* gameState) {
	double result = 0;

	if(getEntityByRef(gameState, gameState->consoleEntityRef)) {
		MemoryArena* arena = &gameState->hackSaveStorage;
		assert(arena->allocated);

		double oldEnergy = *((double*)getSaveReference(arena, 0)->save);
		double newEnergy = gameState->fieldSpec.hackEnergy;
		result = oldEnergy - newEnergy;
	}

	return result;
}
