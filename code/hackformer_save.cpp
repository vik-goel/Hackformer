void writeS32(FILE* file, s32 value) {
	fprintf(file, "%d", value);
}

void writeU32(FILE* file, u32 value) {
	fprintf(file, "%u", value);
}

void writeDouble(FILE* file, double value) {
	fprintf(file, "%f", value);
}

void writeString(FILE* file, char* value) {
	fprintf(file, value);
}

void writeV2(FILE* file, V2 value) {
	writeDouble(file, value.x);
	writeDouble(file, value.y);
}

void writeR2(FILE* file, R2 value) {
	writeV2(file, value.min);
	writeV2(file, value.max);
}

#define writeLinkedListCount(file, list) {s32 listCount = 0; auto listNodePtr = (list); while(listNodePtr) { listCount++; listNodePtr = listNodePtr->next;} writeS32(file, listCount); }

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

		//TODO: Something like a memcpy would be better here in case doubleValues isn't the biggest thing in the union
		for(s32 i = 0; i < arrayCount(field->doubleValues); i++) {
			writeDouble(file, field->doubleValues[i]);
		}

		writeS32(file, field->numValues);
		writeS32(file, field->selectedIndex);
		writeS32(file, field->tweakCost);
		writeString(file, field->valueStr);
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

		for(s32 textIndex = 0; textIndex < messages->count; textIndex++) {
			writeString(file, messages->text[textIndex]);
		}
	} else {
		writeS32(file, -1);
	}
}

void saveEntity(FILE* file, Entity* entity) {
	writeS32(file, entity->type);
	writeU32(file, entity->flags);
	writeS32(file, entity->ref);
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
	writeS32(file, entity->currentAnim.dataIndex);
	writeS32(file, entity->nextAnim.dataIndex);
	writeS32(file, entity->defaultTex.dataIndex);
	writeS32(file, entity->characterAnim.dataIndex);
}

void writeFieldSpec(FILE* file, FieldSpec* spec) {
	writeV2(file, spec->mouseOffset);
	writeS32(file, spec->blueEnergy);
}

void writeAnimation(FILE* file, Animation* anim) {
	writeS32(file, anim->numFrames);

	for(s32 frameIndex = 0; frameIndex < anim->numFrames; frameIndex++) {
		writeS32(file, anim->frames[frameIndex].dataIndex);
	}

	writeDouble(file, anim->secondsPerFrame);
	writeS32(file, anim->pingPong);
	writeS32(file, anim->reverse);
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

void saveGame(GameState* gameState, char* fileName) {
	FILE* file = fopen(fileName, "w");
	assert(file);

	writeS32(file, gameState->numEntities);

	for(s32 entityIndex = 0; entityIndex < gameState->numEntities; entityIndex++) {
		Entity* entity = gameState->entities + entityIndex;
		saveEntity(file, entity);
	}

	writeS32(file, gameState->refCount);
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

	writeS32(file, gameState->textureDataCount);

	for(s32 texIndex = 1; texIndex < gameState->textureDataCount; texIndex++) {
		TextureData* data = gameState->textureData + texIndex;
		assert(data->hasFileName);

		writeR2(file, data->uv);
		writeV2(file, data->size);
		writeString(file, data->fileName);
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

		writeS32(file, data->standAnim.dataIndex);
		writeS32(file, data->jumpAnim.dataIndex);
		writeS32(file, data->shootAnim.dataIndex);
		writeS32(file, data->walkAnim.dataIndex);
	}

	writePauseMenu(file, &gameState->pauseMenu);

	fclose(file);
}

s32 readS32(FILE* file) {
	s32 result = 0;
	fscanf (file, "%d", &result);
	return result;  
}

u32 readU32(FILE* file) {
	u32 result = 0;
	fscanf (file, "%u", &result);
	return result;  
}

double readDouble(FILE* file) {
	double result = 0;
	fscanf (file, "%f", &result);
	return result;  
}

V2 readV2(FILE* file) {
	V2 result = {};
	result.x = readDouble(file);
	result.y = readDouble(file);
	return result;
}

Entity readEntity(FILE* file) {
	return {};
}

void loadGame(GameState* gameState, char* fileName) {
	FILE* file = fopen(fileName, "r");
	assert(file);

	gameState->numEntities = readS32(file);

	for(s32 entityIndex = 0; entityIndex < gameState->numEntities; entityIndex++) {
		gameState->entities[entityIndex] = readEntity(file);
	}

	fclose(file);
}