void streamCamera(IOStream* stream, Camera* camera) {
	streamV2(stream, &camera->p);
	streamV2(stream, &camera->dP);
	streamV2(stream, &camera->newP);
	streamElem(stream, camera->moveToTarget);
	streamElem(stream, camera->deferredMoveToTarget);
	streamElem(stream, camera->scale);
	streamV2(stream, &camera->scaleCenter);
}

void streamRandom(IOStream* stream, Random* random) {
	streamElem(stream, random->seed);
}

void streamRefNode(IOStream* stream, RefNode** nodePtr) {
	s32 count;

	if(stream->reading) {
		readElem(stream, count);

		*nodePtr = NULL;
		
		for(s32 i = 0; i < count; i++) {
			s32 ref;
			readElem(stream, ref);
			*nodePtr = refNode(stream->gameState, ref, *nodePtr);
		}
	} else {
		count = 0;

		for(RefNode* n = *nodePtr; n; n = n->next) {
			count++;
		}

		writeElem(stream, count);

		for(RefNode* n = *nodePtr; n; n = n->next) {
			writeElem(stream, n->ref);
		}
	}
}

void streamConsoleField(IOStream* stream, ConsoleField** fieldPtr) {
	GameState* gameState = stream->gameState;

	ConsoleField* field = *fieldPtr;

	if(!field) {
		if(stream->reading) {
			field = createConsoleField_(gameState);
			*fieldPtr = field;
		}
		else {
			streamValue(stream, s32, -1);
			return;
		}
	}

	s32 type;
	if(!stream->reading) type = field->type;
	streamElem(stream, type);

	if(stream->reading) {
		if(type < 0) {
			freeConsoleField(fieldPtr, gameState);
			return;
		} else {
			freeConsoleField_(field, gameState);
			field->type = (ConsoleFieldType)type;
		}
	}

	streamElem(stream, field->flags);
	streamStr(stream, field->name);
	streamElem(stream, field->numValues);

	switch(field->type) {
		case ConsoleField_double: {
			for(s32 i = 0; i < field->numValues; i++) {
				streamElem(stream, field->doubleValues[i]);
			}
		} break;

		case ConsoleField_s32: {
			for(s32 i = 0; i < field->numValues; i++) {
				streamElem(stream, field->s32Values[i]);
			}
		} break;

		case ConsoleField_followsWaypoints: {
			streamWaypoints(stream, &field->curWaypoint, &gameState->levelStorage, false, false);
			streamElem(stream, field->waypointDelay);
		} break;

		case ConsoleField_bobsVertically: {
			streamElem(stream, field->bobHeight);
			streamElem(stream, field->bobbingUp);
			streamElem(stream, field->initialBob);
		} break;

		case ConsoleField_shootsAtTarget: {
			streamElem(stream, field->shootTimer);
			streamElem(stream, field->shootEntityType);
		} break;

		case ConsoleField_spawnsTrawlers:
		case ConsoleField_spawnsShrikes: {
			streamElem(stream, field->spawnTimer);
			streamRefNode(stream, &field->spawnedEntities);
		} break;

		case ConsoleField_light: {
			streamV3(stream, &field->lightColor);
		} break;

		case ConsoleField_scansForTargets: {
			streamElem(stream, field->scanStart);
			streamElem(stream, field->decreasingScanAngle);
		} break;
            
        default: {
        } break;
	}

	streamElem(stream, field->selectedIndex);
	streamElem(stream, field->initialIndex);
	streamElem(stream, field->tweakCost);
	streamV2(stream, &field->p);
	streamV2(stream, &field->offs);
	streamElem(stream, field->childYOffs);
	streamElem(stream, field->alpha);
	streamElem(stream, field->numChildren);

	for(s32 i = 0; i < field->numChildren; i++) {
		if (stream->reading) field->children[i] = NULL;
		streamConsoleField(stream, field->children + i);
		assert(field->children[i]);
	}
}

void streamFieldSpec(IOStream* stream, FieldSpec* spec) {
	streamElem(stream, spec->hackEnergy);
	streamHackAbilities(stream, &spec->hackAbilities);
}

void streamHitboxes(IOStream* stream, Hitbox** hitboxesPtr) {
	s32 count;
	Hitbox* hitboxes = *hitboxesPtr;

	if(stream->reading) {
		streamElem(stream, count);

		assert(hitboxes == NULL);	

		if(count > 0) {
			hitboxes = pushArray(&stream->gameState->levelStorage, Hitbox, count);

			for(s32 i = 0; i < count - 1; i++) {
				hitboxes[i].next = hitboxes + i + 1;
			}

			hitboxes[count - 1].next = NULL;
		} else {
			hitboxes = NULL;
		}
	}
	else {
		count = 0;

		for(Hitbox* h = hitboxes; h; h = h->next) {
			count++;
		}

		streamElem(stream, count);
	}

	for(Hitbox* h = hitboxes; h; h = h->next) {
		streamV2(stream, &h->collisionSize);
		streamV2(stream, &h->collisionOffset);
		streamElem(stream, h->collisionPointsCount);

		for(s32 i = 0; i < h->collisionPointsCount; i++) {
			streamV2(stream, &h->originalCollisionPoints[i]);
		}

		h->storedRotation = INVALID_STORED_HITBOX_ROTATION;
	}

	*hitboxesPtr = hitboxes;
}

void streamTextureObject_(IOStream* stream, void** nodePtr, void* array, size_t nodeSize) {
	if(stream->reading) {
		s32 index;
		streamElem(stream, index);

		if(index > 0) {
			*nodePtr = (char*)array + index * nodeSize;
		}
		else {
			*nodePtr = NULL;
		}
	}
	else {
		if(*nodePtr) {
			s32 index = (s32)(((size_t)*nodePtr - (size_t)array) / nodeSize);
			streamElem(stream, index);
		}
		else {
			streamValue(stream, s32, -1);
		}
	}
}

void streamAnimNode(IOStream* stream, AnimNode** nodePtr) {
	streamTextureObject_(stream, (void**)nodePtr, stream->gameState->animNodes, sizeof(AnimNode));
}

void streamTexture(IOStream* stream, Texture** texturePtr) {
	streamTextureObject_(stream, (void**)texturePtr, stream->gameState->textures, sizeof(Texture));
}

void streamGlowingTexture(IOStream* stream, GlowingTexture** texturePtr) {
	streamTextureObject_(stream, (void**)texturePtr, stream->gameState->glowingTextures, sizeof(GlowingTexture));
}

void streamCharacterAnim(IOStream* stream, CharacterAnim** animPtr) {
	streamTextureObject_(stream, (void**)animPtr, stream->gameState->characterAnims, sizeof(CharacterAnim));
}

void streamEntity(IOStream* stream, Entity* entity, s32 entityIndex, bool streamingCheckpoint) {
	GameState* gameState = stream->gameState;

	streamElem(stream, entity->ref);

	if(stream->reading) {
		addEntity(gameState, entity->ref, entityIndex);
	}

	streamElem(stream, entity->type);
	streamElem(stream, entity->flags);

	streamV2(stream, &entity->p);
	streamV2(stream, &entity->dP);
	streamElem(stream, entity->rotation);
	streamElem(stream, entity->wheelRotation);

	streamV2(stream, &entity->renderSize);
	streamElem(stream, entity->drawOrder);

	streamHitboxes(stream, &entity->hitboxes);
	streamR2(stream, &entity->clickBox);

	streamElem(stream, entity->numFields);
	for(int i = 0; i < entity->numFields; i++) {
		streamConsoleField(stream, entity->fields + i);
	}

	streamRefNode(stream, &entity->groundReferenceList);
	streamRefNode(stream, &entity->ignorePenetrationList);

	streamElem(stream, entity->emissivity);
	streamElem(stream, entity->spotLightAngle);
	streamElem(stream, entity->alpha);
	streamElem(stream, entity->cloakFactor);

	streamElem(stream, entity->targetRef);
	streamElem(stream, entity->spawnerRef);
	streamV2(stream, &entity->startPos);
	
	streamElem(stream, entity->tileXOffset);
	streamElem(stream, entity->tileYOffset);

	streamElem(stream, entity->jumpCount);
	streamElem(stream, entity->timeSinceLastOnGround);

	streamMessages(stream, &entity->messages, &gameState->levelStorage);

	if(stream->reading && entity->messages) {
		setSelectedText(entity, entity->messages->selectedIndex, gameState);
	}

	streamElem(stream, entity->animTime);
	streamAnimNode(stream, &entity->currentAnim);
	streamAnimNode(stream, &entity->nextAnim);
	streamTexture(stream, &entity->defaultTex);
	streamGlowingTexture(stream, &entity->glowingTex);
	streamCharacterAnim(stream, &entity->characterAnim);

	streamV2(stream, &entity->groundNormal);

	if(stream->reading) {
		addToSpatialPartition(entity, gameState);
	}

	if(streamingCheckpoint) {
		if(stream->reading) {
			entity->checkPointSave = NULL;
			
			for(CheckPointSave* save = gameState->checkPointSaveList; save; save = save->next) {
				if(save->ref == entity->ref) {
					entity->checkPointSave = save;
				}
			}
		}
	}
	else {
		if(stream->reading) {
			s32 offset; 
			readElem(stream, offset);

			if(offset >= 0) {
				entity->checkPointSave = (CheckPointSave*)((char*)gameState->checkPointStorage.base + offset);
			} else {
				entity->checkPointSave = NULL;
			}
		}
		else {
			s32 offset = -1;

			if(entity->checkPointSave) {
				offset = (s32)((size_t)entity->checkPointSave - (size_t)gameState->checkPointStorage.base);
			}

			writeElem(stream, offset);
		}
	}
}

void streamArena(IOStream* stream, MemoryArena* arena) {
	streamElem(stream, arena->allocated);
	streamElem_(stream, arena->base, arena->allocated);
}

void streamGameChanges(IOStream* stream);

void streamGame(IOStream* stream, bool streamingCheckpoint) {
	GameState* gameState = stream->gameState;

	streamElem(stream, gameState->screenType);
	streamElem(stream, gameState->refCount_);
	streamCamera(stream, &gameState->camera);
	streamRandom(stream, &gameState->random);

	streamRefNode(stream, &gameState->targetRefs);
	streamRefNode(stream, &gameState->guardTargetRefs);
	streamRefNode(stream, &gameState->fadingOutConsoles);
	streamElem(stream, gameState->consoleEntityRef);
	streamElem(stream, gameState->playerRef);
	streamElem(stream, gameState->testEntityRef);

	streamElem(stream, gameState->loadNextLevel);
	streamElem(stream, gameState->reloadCurrentLevel);
	streamElem(stream, gameState->doingInitialSim);
	
	streamConsoleField(stream, &gameState->timeField);
	streamConsoleField(stream, &gameState->gravityField);
	streamConsoleField(stream, &gameState->swapField);
	streamV2(stream, &gameState->swapFieldP);
	streamFieldSpec(stream, &gameState->fieldSpec);

	streamV2(stream, &gameState->mapSize);	
	streamV2(stream, &gameState->worldSize);	
	streamV2(stream, &gameState->gravity);	

	streamElem(stream, gameState->backgroundTextures.curBackgroundType);
	streamElem(stream, gameState->collisionBoundsAlpha);

	if(stream->reading) {
		initSpatialPartition(gameState);
	}

	streamElem(stream, gameState->numEntities);

	for(s32 i = 0; i < gameState->numEntities; i++) {
		streamEntity(stream, gameState->entities + i, i, streamingCheckpoint);
	}

	if(getEntityByRef(gameState, gameState->consoleEntityRef)) {
		MemoryArena* arena = &gameState->hackSaveStorage;
		SaveMemoryHeader* header = (SaveMemoryHeader*)arena->base;

		

		if(!stream->reading) {
			for(s32 i = 0; i < header->numSaveGames; i++) {
				header->saves[i].save = (void*)((size_t)header->saves[i].save - (size_t)arena->base);
			}
		}

		streamArena(stream, arena);

		for(s32 i = 0; i < header->numSaveGames; i++) {
			header->saves[i].save = (void*)((size_t)header->saves[i].save + (size_t)arena->base);
		}
	}

	if(!streamingCheckpoint) {
		if(stream->reading) {
			while(true) {
				bool32 keepReading;
				streamElem(stream, keepReading);
				if(!keepReading) break;

				s32 ref;
				streamElem(stream, ref);
				CheckPointSave* save = allocateCheckPointSave(gameState, ref);
				streamArena(stream, save->arena);
			}
		} else {
			for(CheckPointSave* save = gameState->checkPointSaveList; save; save = save->next) {
				if(save->arena && getEntityByRef(gameState, save->ref)) {
					streamValue(stream, bool32, true);

					streamElem(stream, save->ref);
					streamArena(stream, save->arena);
				}
			}

			streamValue(stream, bool32, false);
		}
	}

	freeIostream(stream);	
}

void saveCompleteGame(GameState* gameState, char* fileName) {
	IOStream stream = createIostream(gameState, fileName, false);
	streamGame(&stream, false);	
}

void loadCompleteGame(GameState* gameState, char* fileName) {
	IOStream stream = createIostream(gameState, fileName, true);
	streamGame(&stream, false);	
}

void saveCompleteGame(GameState* gameState, MemoryArena* arena) {
	arena->allocated = 0;
	IOStream stream = createIostream(gameState, arena, NULL);
	streamGame(&stream, true);	
}

void loadCompleteGame(GameState* gameState, MemoryArena* arena) {
	freeLevel(gameState, true);
	void* readPtr = arena->base;
	IOStream stream = createIostream(gameState, arena, readPtr);
	streamGame(&stream, true);	
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

void streamConsoleFieldChanges(IOStream* stream, ConsoleField** fieldPtr) {
	streamConsoleField(stream, fieldPtr);

	if(stream->reading && *fieldPtr) {
		clearFlags(*fieldPtr, ConsoleFlag_selected);
		(*fieldPtr)->offs = v2(0, 0);
	}
}

void streamEntityChanges(IOStream* stream, Entity* entity) {
	streamV2(stream, &entity->p);
	streamElem(stream, entity->rotation);
	streamElem(stream, entity->tileXOffset);
	streamElem(stream, entity->tileYOffset);
	
	if(entity->messages) {
		streamElem(stream, entity->messages->selectedIndex);
	}

	streamElem(stream, entity->timeSinceLastOnGround);

	if(stream->reading) {
		for(s32 i = 0; i < entity->numFields; i++) {
			freeConsoleField(entity->fields + i, stream->gameState);
		}
	}

	streamElem(stream, entity->numFields);

	for(s32 i = 0; i < entity->numFields; i++) {
		streamConsoleFieldChanges(stream, entity->fields + i);
	}
}

void streamGameChanges(IOStream* stream) {
	GameState* gameState = stream->gameState;

	streamElem(stream, gameState->fieldSpec.hackEnergy);
	streamV2(stream, &gameState->gravity);
	streamElem(stream, gameState->timeField->selectedIndex);
	streamElem(stream, gameState->gravityField->selectedIndex);

	streamConsoleFieldChanges(stream, &gameState->swapField);

	streamRefNode(stream, &gameState->targetRefs);
	streamRefNode(stream, &gameState->guardTargetRefs);

	streamElem(stream, gameState->numEntities);

	for(s32 entityIndex = 0; entityIndex < gameState->numEntities; entityIndex++) {
		streamEntityChanges(stream, gameState->entities + entityIndex);
	}

	freeIostream(stream);
}

void updateSaveGame(GameState* gameState, bool initialSave) {
	MemoryArena* arena = &gameState->hackSaveStorage;

	if(initialSave) {
		arena->allocated = 0;
		SaveMemoryHeader* header = pushStruct(arena, SaveMemoryHeader);
		header->numSaveGames = -1;
	}

	SaveMemoryHeader* header = (SaveMemoryHeader*)arena->base;
	header->numSaveGames++;

	if(header->numSaveGames == 1) {
		//This would just be a duplicate of the 0th save game
		return;
	}

	SaveReference* saveGameReference = header->saves + getSlotIndex(header->numSaveGames);

	s32 slotIndex = header->numSaveGames;
	size_t totalAllocated = 0;
	size_t oldAllocated = arena->allocated;

	if(header->numSaveGames >= MAX_HACK_UNDOS) {
		//NOTE: Can't just directly go to the savePtr corresponding to slotIndex
		//		because it may have been corrupted by the last write. Instead, we 
		//		go to the last write and advance by its size to find out where we should be. 
		SaveReference* oneMinusSlotIndexSave = header->saves +  slotIndex - 1;
		void* saveStart = (char*)oneMinusSlotIndexSave->save + oneMinusSlotIndexSave->size;

		totalAllocated = arena->allocated;
		oldAllocated = arena->allocated = (size_t)saveStart - (size_t)arena->base;
	}

	//NOTE: Update the save game ptr table
	saveGameReference->save = (char*)arena->base + arena->allocated;
	saveGameReference->index = header->numSaveGames;


	IOStream stream = createIostream(gameState, arena, NULL); 
	streamGameChanges(&stream);


	saveGameReference->size = arena->allocated - oldAllocated;

	if(totalAllocated > 0) {
		size_t arenaLimit = (size_t)arena->base + arena->allocated;

		for(s32 checkIndex = slotIndex; checkIndex < MAX_HACK_UNDOS; checkIndex++) {
			SaveReference* checkReference = header->saves + checkIndex;

			if((size_t)checkReference->save < arenaLimit) {
				*checkReference = {};
			} else {
				break;
			}
		}

		arena->allocated = totalAllocated;
	}
}

void undoLastSaveGame(GameState* gameState) {
	MemoryArena* arena = &gameState->hackSaveStorage;
	SaveMemoryHeader* header = (SaveMemoryHeader*)arena->base;

	if(header->numSaveGames > 0) {
		s32 slotIndex = getSlotIndex(header->numSaveGames);
		SaveReference* lastSaveReference = header->saves + slotIndex;

		bool acceptableReference = header->numSaveGames == lastSaveReference->index && lastSaveReference->save;

		//they should be the same
		if(header->numSaveGames == 1) acceptableReference |= lastSaveReference->index == 0;

		if(acceptableReference) {
			freeRefNode(gameState->fadingOutConsoles, gameState);
			gameState->fadingOutConsoles = NULL;

			IOStream stream = createIostream(gameState, arena, lastSaveReference->save); 
			streamGameChanges(&stream);

			header->numSaveGames--;
		}
	}
}

void undoAllHackMoves(GameState* gameState) {
	MemoryArena* arena = &gameState->hackSaveStorage;
	SaveMemoryHeader* header = (SaveMemoryHeader*)arena->base;

	if(header->numSaveGames > 0) {
		void* readPtr = header->saves->save;
		IOStream stream = createIostream(gameState, arena, readPtr); 
		streamGameChanges(&stream);
	}
}

double getEnergyLoss(GameState* gameState) {
	double result = 0;

	MemoryArena* arena = &gameState->hackSaveStorage;
	SaveMemoryHeader* header = (SaveMemoryHeader*)arena->base;

	if(header->numSaveGames > 0 && getEntityByRef(gameState, gameState->consoleEntityRef)) {
		assert(arena->allocated);

		double oldEnergy = *((double*)header->saves->save);
		double newEnergy = gameState->fieldSpec.hackEnergy;
		result = oldEnergy - newEnergy;
	}

	return result;
}