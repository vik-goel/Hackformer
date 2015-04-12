void giveEntityRectangularCollisionBounds(Entity* entity, GameState* gameState,
										  double xOffset, double yOffset, double width, double height) {
	Hitbox* hitbox = NULL;

	if (gameState->hitboxFreeList) {
		hitbox = gameState->hitboxFreeList;
		gameState->hitboxFreeList = gameState->hitboxFreeList->next;
	} else {
		hitbox = pushStruct(&gameState->levelStorage, Hitbox);
	}

	*hitbox = {};
	hitbox->collisionSize = v2(width, height);
	hitbox->collisionOffset = v2(xOffset, yOffset);

	hitbox->next = entity->hitboxes;
	entity->hitboxes = hitbox;
}

EntityReference* getEntityReferenceBucket(GameState* gameState, int ref) {
	int bucketIndex = (ref % (arrayCount(gameState->entityRefs_) - 1)) + 1;
	EntityReference* result = gameState->entityRefs_ + bucketIndex;
	return result;
}

EntityReference* getPreciseEntityReferenceBucket(GameState* gameState, int ref) {
	EntityReference* bucket = getEntityReferenceBucket(gameState, ref);

	EntityReference* result = NULL;
	
	while(bucket) {
		if (bucket->entity && bucket->entity->ref == ref) {
			result = bucket;
			break;
		}

		bucket = bucket->next;
	}

	return result;
}

Entity* getEntityByRef(GameState* gameState, int ref) {
	EntityReference* bucket = getPreciseEntityReferenceBucket(gameState, ref);

	Entity* result = NULL;
	if (bucket) result = bucket->entity;

	return result;
}

Entity* getUnremovedEntityByRef(GameState* gameState, int ref) {
	Entity* result = getEntityByRef(gameState, ref);
	if (result && isSet(result, EntityFlag_remove)) result = NULL;
	return result;
}

void storeEntityReferenceInFreeList(EntityReference* reference, GameState* gameState) {
	if (reference->next) {
		storeEntityReferenceInFreeList(reference->next, gameState);
		reference->next = NULL;
	}

	reference->entity = NULL;
	reference->next = gameState->entityRefFreeList;
	gameState->entityRefFreeList = reference;
}

void freeEntityReference(EntityReference* reference, GameState* gameState) {
	EntityReference* nextReference = reference->next;	

	if (reference->next) {
		storeEntityReferenceInFreeList(reference->next, gameState);
	}

	reference->entity = NULL;
	reference->next = NULL;
}

EntityChunk* getSpatialChunk(V2 p, GameState* gameState) {
	int x = (int)(p.x / gameState->chunkSize.x);
	int y = (int)(p.y / gameState->chunkSize.y);

	EntityChunk* result = NULL;

	if (x >= 0 && 
		y >= 0 && 
		x < gameState->chunksWidth &&
		y < gameState->chunksHeight) {
		result = gameState->chunks + y * gameState->chunksWidth + x;
	}

	return result;
}

void addToSpatialPartition(Entity* entity, GameState* gameState) {
	EntityChunk* chunk = getSpatialChunk(entity->p, gameState);

	while(chunk) {
		if (chunk->numRefs < arrayCount(chunk->entityRefs)) {
			chunk->entityRefs[chunk->numRefs++] = entity->ref;
			break;
		} 

		if (!chunk->next) {
			chunk->next = pushStruct(&gameState->levelStorage, EntityChunk);
			*chunk->next = {};
		}

		chunk = chunk->next;
	}
}

bool removeFromSpatialPartition(Entity* entity, GameState* gameState) {
	EntityChunk* chunk = getSpatialChunk(entity->p, gameState);

	bool removed = false;

	while(chunk) {
		for (int refIndex = 0; refIndex < chunk->numRefs; refIndex++) {
			if (chunk->entityRefs[refIndex] == entity->ref) {
				chunk->numRefs--;
				chunk->entityRefs[refIndex] = chunk->entityRefs[chunk->numRefs];
				removed = true;
				break;
			}
		}

		if (removed) break;

		chunk = chunk->next;
	}

	return removed;
}

void setEntityP(Entity* entity, V2 newP, GameState* gameState) {
	removeFromSpatialPartition(entity, gameState);
	entity->p = newP;
	addToSpatialPartition(entity, gameState);
}

void addGroundReference(Entity* top, Entity* ground, GameState* gameState) {
	top->jumpCount = 0;
	setFlags(top, EntityFlag_grounded);
	top->timeSinceLastOnGround = 0;

	RefNode* node = ground->groundReferenceList;

	while(node) {
		if (node->ref == top->ref) return;
		node = node->next;
	}


	RefNode* append = NULL;

	if (gameState->refNodeFreeList) {
		append = gameState->refNodeFreeList;
		gameState->refNodeFreeList = gameState->refNodeFreeList->next;
	} else {
		append = (RefNode*)pushStruct(&gameState->levelStorage, RefNode);
	}

	assert(append);

	*append = {};
	append->ref = top->ref;


	RefNode* last = ground->groundReferenceList;
	RefNode* previous = NULL;

	while (last) {
		previous = last;
		last = last->next;
	}

	if (previous) {
		previous->next = append;
	} else {
		ground->groundReferenceList = append;
	}
}

void freeEntity(Entity* entity, GameState* gameState) {
	switch(entity->type) {
		case EntityType_text: {
			//glDeleteTextures(1, &entity->texture->texId);

			for (int messageIndex = 0; messageIndex < entity->numMessages; messageIndex++) {
				SDL_DestroyTexture(entity->messages[messageIndex].tex);
			}

			//TODO: A free list could be used here instead
			assert(entity->messages);
			free(entity->messages);
			entity->messages = NULL;
			entity->standTex = NULL;
		} break;

		case EntityType_laserTop: {
			Entity* base = getEntityByRef(gameState, entity->ref - 1);
			Entity* beam = getEntityByRef(gameState, entity->ref + 1);

			if(beam) {
				assert(beam->type == EntityType_laserBeam);
				setFlags(beam, EntityFlag_remove);
			} 

			if(base) {
				assert(base->type == EntityType_laserBase);
				setFlags(base, EntityFlag_remove);
			} 
		} break;
	}

	for (int fieldIndex = 0; fieldIndex < entity->numFields; fieldIndex++) {
		freeConsoleField(entity->fields[fieldIndex], gameState);
		entity->fields[fieldIndex] = NULL;
	}
}

void removeEntities(GameState* gameState) {
	//NOTE: Entities are removed here if their remove flag is set
	//NOTE: There is a memory leak here if the entity allocated anything
	//		Ensure this is cleaned up, the console is a big place where leaks can happen
	for (int entityIndex = 0; entityIndex < gameState->numEntities; entityIndex++) {
		Entity* entity = gameState->entities + entityIndex;

		if (isSet(entity, EntityFlag_remove)) {
			freeEntity(entity, gameState);

			EntityReference* bucket = getEntityReferenceBucket(gameState, entity->ref);
			EntityReference* previousBucket = bucket;

			while (bucket) {
				if (bucket->entity->ref == entity->ref) {
					break;
				}

				previousBucket = bucket;
				bucket = bucket->next;
			}

			if (bucket != NULL) previousBucket->next = bucket->next;
			else previousBucket->next = NULL;


			//NOTE: EntityReference's which are inside the entityRefs_ array should not be
			//		added to the free list. This checks the address of an entity reference before 
			//		adding it to the free list to ensure that it is not part of the table.
			size_t bucketAddress = (size_t)bucket;
			size_t entityRefsStartAddress = (size_t)&gameState->entityRefs_;
			size_t entityRefsEndAddress = entityRefsStartAddress + sizeof(gameState->entityRefs_);

			if (bucketAddress < entityRefsStartAddress || bucketAddress >= entityRefsEndAddress) {
				//Only adds if outside of the address space of the table
				bucket->next = gameState->entityRefFreeList;
				gameState->entityRefFreeList = bucket;
			}
			
			bucket->entity = NULL;

			gameState->numEntities--;

			if(entityIndex != gameState->numEntities) {
				Entity* dst = gameState->entities + entityIndex;
				Entity* src = gameState->entities + gameState->numEntities;
				*dst = *src;
				getPreciseEntityReferenceBucket(gameState, dst->ref)->entity -= (gameState->numEntities - entityIndex);
			}

			entityIndex--;

			gameState->entities[gameState->numEntities] = {};
		}
	}
}

Entity* addEntity(GameState* gameState, EntityType type, DrawOrder drawOrder, V2 p, V2 renderSize) {
	assert(gameState->numEntities < arrayCount(gameState->entities));

	Entity* result = gameState->entities + gameState->numEntities;

	*result = {};
	result->ref = gameState->refCount;
	result->type = type;
	result->drawOrder = drawOrder;
	result->p = p;
	result->renderSize = renderSize;

	EntityReference* bucket = getEntityReferenceBucket(gameState, result->ref);
	while(bucket->next) {
		bucket = bucket->next;
	}

	if (bucket->entity) {
		if (gameState->entityRefFreeList) {
			bucket->next = gameState->entityRefFreeList;
			gameState->entityRefFreeList = gameState->entityRefFreeList->next;
		} else {
			bucket->next = pushStruct(&gameState->levelStorage, EntityReference);
			*bucket->next = {};
		}

		bucket = bucket->next;
		bucket->next = NULL;
	}

	bucket->entity = result;

	gameState->numEntities++;
	gameState->refCount++;

	addToSpatialPartition(result, gameState);

	return result;
}

void removeFieldsIfSet(ConsoleField** fields, int* numFields) {
	for (int fieldIndex = 0; fieldIndex < *numFields; fieldIndex++) {
		ConsoleField* field = fields[fieldIndex];

		if (field->children) {
			removeFieldsIfSet(field->children, &field->numChildren);
		}

		if (isSet(field, ConsoleFlag_remove)) {
			clearFlags(field, ConsoleFlag_remove);

			for (int moveIndex = fieldIndex; moveIndex < *numFields - 1; moveIndex++) {
				ConsoleField** dst = fields + moveIndex;
				ConsoleField** src = fields + moveIndex + 1;

				*dst = *src;
			}

			fields[*numFields - 1] = 0;
			(*numFields)--;
		}
	}
}

void addField(Entity* entity, GameState* gameState, ConsoleField* field) {
	entity->fields[entity->numFields] = field;
	entity->numFields++;
	assert(arrayCount(entity->fields) > entity->numFields);
}

void addChildToConsoleField(ConsoleField* parent, ConsoleField* child) {
	parent->children[parent->numChildren] = child;
	parent->numChildren++;
	assert(parent->numChildren < arrayCount(parent->children));
}

//TODO: Clean up speed and jump height values
void addKeyboardField(Entity* entity, GameState* gameState) {
	ConsoleField* result = createConsoleField(gameState, "keyboard_controlled", ConsoleField_keyboardControlled);

	double keyboardSpeedFieldValues[] = {20, 40, 60, 80, 100}; 
	double keyboardJumpHeightFieldValues[] = {1, 3, 5, 7, 9}; 

	addChildToConsoleField(result, createPrimitiveField(double, gameState, "speed", keyboardSpeedFieldValues, 
												   arrayCount(keyboardSpeedFieldValues), 2, 1));
	addChildToConsoleField(result, createPrimitiveField(double, gameState, "jump_height", keyboardJumpHeightFieldValues, 
													    arrayCount(keyboardJumpHeightFieldValues), 2, 1));
	addChildToConsoleField(result, createBoolField(gameState, "double_jump", false, 3));

	addField(entity, gameState, result);
}

void addPatrolField(Entity* entity, GameState* gameState) {
	ConsoleField* result = createConsoleField(gameState, "patrols", ConsoleField_movesBackAndForth);

	double patrolSpeedFieldValues[] = {0, 10, 20, 30, 40}; 
	
	addChildToConsoleField(result, createPrimitiveField(double, gameState, "speed", patrolSpeedFieldValues, 
	 											   arrayCount(patrolSpeedFieldValues), 2, 1));

	addField(entity, gameState, result);
}

void addSeekTargetField(Entity* entity, GameState* gameState) {
	ConsoleField* result = createConsoleField(gameState, "seeks_target", ConsoleField_seeksTarget);

	double seekTargetSpeedFieldValues[] = {5, 10, 15, 20, 25}; 
	double seekTargetRadiusFieldValues[] = {4, 8, 12, 16, 20}; 
	
	addChildToConsoleField(result, createPrimitiveField(double, gameState, "speed", seekTargetSpeedFieldValues, 
	 												    arrayCount(seekTargetSpeedFieldValues), 2, 1));
	addChildToConsoleField(result, createPrimitiveField(double, gameState, "sight_radius", seekTargetRadiusFieldValues, 
													    arrayCount(seekTargetRadiusFieldValues), 2, 2));

	addField(entity, gameState, result);
}

void addShootField(Entity* entity, GameState* gameState) {
	ConsoleField* result = createConsoleField(gameState, "shoots", ConsoleField_shootsAtTarget);

	double bulletSpeedFieldValues[] = {1, 2, 3, 4, 5}; 
	double shootRadiusFieldValues[] = {1, 3, 5, 7, 9}; 
	
	addChildToConsoleField(result, createPrimitiveField(double, gameState, "bullet_speed", bulletSpeedFieldValues, 
													    arrayCount(bulletSpeedFieldValues), 2, 1));
	addChildToConsoleField(result, createPrimitiveField(double, gameState, "detect_radius", shootRadiusFieldValues, 
													    arrayCount(shootRadiusFieldValues), 2, 2));

	addField(entity, gameState, result);
}

Entity* addPlayerDeath(GameState* gameState) {
	assert(!getEntityByRef(gameState, gameState->playerDeathRef));

	Entity* player = getEntityByRef(gameState, gameState->playerRef);
	assert(player);

	V2 p = gameState->playerDeathStartP;
	p += (player->renderSize - gameState->playerDeathSize) * 0.5;

	Entity* result = addEntity(gameState, EntityType_playerDeath, DrawOrder_playerDeath, 
								p, player->renderSize);

	gameState->playerDeathRef = result->ref;

	setFlags(result, EntityFlag_noMovementByDefault);

	result->standTex = &gameState->playerStand;

	return result;
}

Entity* addPlayer(GameState* gameState, V2 p) {
	Entity* result = addEntity(gameState, EntityType_player, DrawOrder_player, p, v2(1, 1) * 1.75);

	assert(!getUnremovedEntityByRef(gameState, gameState->shootTargetRef));
	gameState->shootTargetRef = result->ref;
	assert(!getUnremovedEntityByRef(gameState, gameState->playerRef));
	gameState->playerRef = result->ref;

	giveEntityRectangularCollisionBounds(result, gameState, 0, 0,
										 result->renderSize.x * 0.4, result->renderSize.y);

	result->clickBox = rectCenterDiameter(v2(0, 0), v2(result->renderSize.x * 0.4, result->renderSize.y));

	setFlags(result, EntityFlag_hackable);

	addKeyboardField(result, gameState);
	addField(result, gameState, createConsoleField(gameState, "is_target", ConsoleField_isShootTarget));

	setEntityP(result, result->p + result->renderSize * 0.5, gameState);

	result->standTex = &gameState->playerStand;
	result->jumpTex = &gameState->playerJump;
	result->walkAnim = &gameState->playerWalk;
	result->standWalkTransition = &gameState->playerStandWalkTransition;

	return result;
}

Entity* addVirus(GameState* gameState, V2 p) {
	Entity* result = addEntity(gameState, EntityType_virus, DrawOrder_virus, p, v2(2.5, 2.5));

	giveEntityRectangularCollisionBounds(result, gameState, -0.025, -0.1,
										 result->renderSize.x * 0.55, result->renderSize.y * 0.75);

	result->clickBox = rectCenterDiameter(v2(0, 0), result->renderSize * 0.75);

	setFlags(result, EntityFlag_facesLeft|
					 EntityFlag_hackable);

	addPatrolField(result, gameState);
	addShootField(result, gameState);

	setEntityP(result, result->p + result->renderSize * 0.5, gameState);

	result->standTex = &gameState->virus1Stand;
	result->shootAnim = &gameState->virus1Shoot;

	return result;
}

Entity* addEndPortal(GameState* gameState, V2 p) {
	Entity* result = addEntity(gameState, EntityType_endPortal, DrawOrder_endPortal, p, v2(1, 2));

	giveEntityRectangularCollisionBounds(result, gameState, 0, 0,
										 result->renderSize.x * 0.4f, result->renderSize.y * 0.95f);

	setEntityP(result, result->p + result->renderSize * 0.5, gameState);

	result->standTex = &gameState->endPortal;

	return result;
}

Entity* addBlueEnergy(GameState* gameState, V2 p) {
	Entity* result = addEntity(gameState, EntityType_blueEnergy, DrawOrder_blueEnergy, p, v2(0.8f, 0.8f));

	giveEntityRectangularCollisionBounds(result, gameState, 0, 0, 
										 result->renderSize.x * 0.5f, result->renderSize.y * 0.65f);

	setFlags(result, EntityFlag_hackable);

	setEntityP(result, result->p + result->renderSize * 0.5, gameState);
	result->clickBox = rectCenterDiameter(v2(0, 0), result->renderSize * 0.65);

	result->standTex = &gameState->blueEnergyTex;

	return result;
}

Entity* addBackground(GameState* gameState) {
	Entity* result = addEntity(gameState, EntityType_background, DrawOrder_background, 
								  v2(0, 0), v2(gameState->mapSize.x, 
								(double)gameState->windowHeight / (double)gameState->pixelsPerMeter));

	setEntityP(result, result->renderSize * 0.5, gameState);
	setFlags(result, EntityFlag_noMovementByDefault);

	return result;
}

void initTile(Entity* tile, GameState* gameState) {
	setEntityP(tile, tile->p + v2(0, gameState->tileSize / 2), gameState);

	tile->renderSize = v2(gameState->tileSize, gameState->tileSize * 2);

	double clickBoxHeight = tile->renderSize.x * 1.22;
	double collisionHeight = tile->renderSize.x * 1.16;

	giveEntityRectangularCollisionBounds(tile, gameState, 0, (collisionHeight - tile->renderSize.y) / 2, 
										 tile->renderSize.x, collisionHeight);

	tile->clickBox = rectCenterDiameter(v2(0, 0), v2(tile->renderSize.x, clickBoxHeight));
	tile->clickBox = translateRect(tile->clickBox, v2(0, (clickBoxHeight - tile->renderSize.y) / 2));

	setFlags(tile, EntityFlag_hackable);

	addField(tile, gameState, createUnlimitedIntField(gameState, "x_offset", 0, 1));
	addField(tile, gameState, createUnlimitedIntField(gameState, "y_offset", 0, 1));
}

Entity* addTile(GameState* gameState, V2 p, Texture* texture) {
	Entity* result = addEntity(gameState, EntityType_tile, DrawOrder_tile, 
								p, v2(0, 0));

	initTile(result, gameState);

	setFlags(result, EntityFlag_noMovementByDefault|
					 EntityFlag_removeWhenOutsideLevel);

	result->standTex = texture;

	return result;
}

Entity* addHeavyTile(GameState* gameState, V2 p) {
	Entity* result = addEntity(gameState, EntityType_heavyTile, DrawOrder_heavyTile, 
								p, v2(0, 0));
	initTile(result, gameState);
	result->standTex = &gameState->heavyTileTex;

	return result;
}

void setSelectedText(Entity* text, int selectedIndex, GameState* gameState) {
	assert(text->type == EntityType_text);

	text->standTex = text->messages + selectedIndex;
	text->renderSize = v2(text->standTex->srcRect.w, text->standTex->srcRect.h) * (1.0 / gameState->pixelsPerMeter);
	text->clickBox = rectCenterDiameter(v2(0, 0), text->renderSize);
}

Entity* addText(GameState* gameState, V2 p, char values[10][100], int numValues, int selectedIndex) {
	Entity* result = addEntity(gameState, EntityType_text, DrawOrder_text, p, v2(0, 0));

	result->numMessages = numValues;
	result->messages = (Texture*)malloc((numValues + 1) * sizeof(Texture));

	for(int valueIndex = 0; valueIndex <= numValues; valueIndex++) {
		Texture* tex = result->messages + valueIndex;
		*tex = createText(gameState, gameState->textFont, values[valueIndex]);
	}

	setSelectedText(result, selectedIndex, gameState);

	giveEntityRectangularCollisionBounds(result, gameState, 0, 0, 
										 result->renderSize.x, result->renderSize.y);

	setFlags(result, EntityFlag_noMovementByDefault|
					 EntityFlag_hackable);

	int selectedIndexValues[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
	addField(result, gameState, 
		createPrimitiveField(int, gameState, "selected_index", selectedIndexValues, numValues + 1, selectedIndex, 1));

	return result;
}

Entity* addLaserBolt(GameState* gameState, V2 p, V2 target, int shooterRef, double speed) {
	Entity* result = addEntity(gameState, EntityType_laserBolt, DrawOrder_laserBolt, p, v2(0.8f, 0.8f));

	giveEntityRectangularCollisionBounds(result, gameState, 0, 0, 
										 result->renderSize.x * 0.3f, result->renderSize.y * 0.3f);

	result->clickBox = rectCenterDiameter(v2(0, 0), result->renderSize * 0.6);

	result->dP = normalize(target - p) * speed;
	result->shooterRef = shooterRef;

	setFlags(result, EntityFlag_removeWhenOutsideLevel|
					 EntityFlag_hackable);

	addField(result, gameState, createBoolField(gameState, "hurts_entities", true, 2));

	result->standTex = &gameState->laserBolt;
					 
	return result;
}

Entity* addLaserPiece_(GameState* gameState, V2 p, bool base) {
	V2 renderSize = v2(1.5, 1.5);

	Entity* result = NULL;

	if (base) {
		result = addEntity(gameState, EntityType_laserBase, DrawOrder_laserBase, p, renderSize);
		result->standTex = &gameState->laserBaseOff;

		addField(result, gameState, createBoolField(gameState, "enabled", true, 2));
		addPatrolField(result, gameState);

		setFlags(result, EntityFlag_hackable);

		giveEntityRectangularCollisionBounds(result, gameState, result->renderSize.x * 0.01, 0, 
												 result->renderSize.x * 0.5, result->renderSize.y * 0.35);
	} else {
		result = addEntity(gameState, EntityType_laserTop, DrawOrder_laserTop, p, renderSize);
		result->standTex = &gameState->laserTopOff;

		giveEntityRectangularCollisionBounds(result, gameState, result->renderSize.x * 0.01, result->renderSize.y * -0.02,
								 	 result->renderSize.x * 0.5, result->renderSize.y * 0.42);
	}


	setFlags(result, EntityFlag_noMovementByDefault|
					 EntityFlag_laserOn);

	return result;
}

Entity* addLaserBeam_(GameState* gameState, V2 p, V2 renderSize) {
	Entity* result = addEntity(gameState, EntityType_laserBeam, DrawOrder_laserBeam, p, renderSize);
	result->standTex = &gameState->laserBeam;

	giveEntityRectangularCollisionBounds(result, gameState, 0, 0, 
									 	 result->renderSize.x * 0.8, result->renderSize.y);

	setFlags(result, EntityFlag_noMovementByDefault|
					 EntityFlag_laserOn);

	return result;
}

Entity* addLaserController(GameState* gameState, V2 baseP, double height) {
	//NOTE: The order which all the parts are added: base first, then top next, then finally the beam
	//		is necessary. This is so that all of the other pieces of the laser can be easily found if you just
	//		have one to start. This is mainly used for collision detection (eg. avoiding collisions between the base
	//		and the other laser pieces)

	Entity* base = addLaserPiece_(gameState, baseP, true);
	V2 topP = baseP + v2(0, height);
	Entity* top = addLaserPiece_(gameState, topP, false);

	V2 p = (baseP + topP) * 0.5;

	V2 renderSize = (top->p + top->hitboxes->collisionSize * 0.5) - (base->p  - base->hitboxes->collisionSize * 0.5);

	V2 laserSize = v2(base->hitboxes->collisionSize.x, 
					  renderSize.y - base->hitboxes->collisionSize.y - top->hitboxes->collisionSize.y);

	base->clickBox = rectCenterDiameter(p - base->p + v2(0.03, 0.02), 
										renderSize + v2(0.03, -0.02));
	base->clickBox = addRadiusTo(base->clickBox, v2(0.1, 0.1));

	V2 laserP = getRectCenter(base->clickBox) + base->p + v2(0, -0.04);

	Entity* beam = addLaserBeam_(gameState, laserP, laserSize);

	addGroundReference(beam, base, gameState);
	addGroundReference(top, beam, gameState);

	return base;
}

Entity* addFlyingVirus(GameState* gameState, V2 p) {
	Entity* result = addEntity(gameState, EntityType_flyingVirus, DrawOrder_flyingVirus, p, v2(1.5, 1.5));

	result->standTex = &gameState->flyingVirus;
	result->shootAnim = &gameState->flyingVirusShoot;

	setFlags(result, EntityFlag_hackable|
					 EntityFlag_noMovementByDefault);

	giveEntityRectangularCollisionBounds(result, gameState, 0, result->renderSize.y * -0.04, 
										 result->renderSize.x * 0.47, result->renderSize.y * 0.49);

	result->clickBox = rectCenterDiameter(v2(0, -result->renderSize.y * 0.05), result->renderSize * 0.5);

	addSeekTargetField(result, gameState);
	addShootField(result, gameState);

	setEntityP(result, result->p + result->renderSize  * 0.5, gameState);

	return result;
}

ConsoleField* getMovementField(Entity* entity) {
	ConsoleField* result = NULL;

	for (int fieldIndex = 0; fieldIndex < entity->numFields; fieldIndex++) {
		ConsoleField* testField = entity->fields[fieldIndex];
		if (isConsoleFieldMovementType(testField)) {
			result = testField;
			break;
		}
	}

	return result;
}

bool isTileType(Entity* entity) {
	bool result = entity->type == EntityType_tile || entity->type == EntityType_heavyTile;
	return result;
}

bool isLaserEntityType(Entity* entity) {
	bool result = entity->type == EntityType_laserTop || 
				  entity->type == EntityType_laserBase ||
				  entity->type == EntityType_laserBeam;
	return result;
}

bool collidesWithRaw(Entity* a, Entity* b, GameState* gameState) {
	bool result = true;

	switch(a->type) {
		case EntityType_background: {
			result = false;
		} break;

		case EntityType_text: {
			result = getMovementField(a) != NULL;
		} break;

		case EntityType_player: {
			if (b->type == EntityType_virus ||
				b->type == EntityType_flyingVirus) result = false;
		} break;

		case EntityType_laserBolt: {
			if (b->ref == a->shooterRef) result = false;
			else {
				//NOTE: If a bullet is shot from a laser base/top, it should not collide with the laser beam
				//		This requires the base, top, and beam to all be added in a specific order to work
				Entity* shooter = getEntityByRef(gameState, a->shooterRef);
				if(shooter &&
				   ((shooter->type == EntityType_laserBase && shooter->ref + 2 == b->ref) ||
				    (shooter->type == EntityType_laserTop && shooter->ref + 1 == b->ref)))
				    result = false;
			}
		} break;

		case EntityType_blueEnergy: {
			if (b->type == EntityType_virus ||
				b->type == EntityType_flyingVirus ||
				b->type == EntityType_laserBolt) result = false;
		} break;

		case EntityType_endPortal: {
			if (b->type != EntityType_player && 
				b->type != EntityType_tile &&
				b->type != EntityType_heavyTile) result = false;
		} break;

		case EntityType_laserBeam: {
			//NOTE: These are the 2 other pieces of the laser (base, top)
			if (a->ref == b->ref + 1 || 
				a->ref == b->ref + 2 ||
				!isSet(a, EntityFlag_laserOn)) result = false;
		} break;

		case EntityType_tile: {
			if (b->type == EntityType_tile && 
				getMovementField(a) == NULL && 
				getMovementField(b) == NULL) result = false;
		} break;
	}

	return result;
}

bool collidesWith(Entity* a, Entity* b, GameState* gameState) {
	bool result = !isSet(a, EntityFlag_remove) && !isSet(b, EntityFlag_remove);

	result &= collidesWithRaw(a, b, gameState);
	result &= collidesWithRaw(b, a, gameState);

	return result;
}

bool isSolidCollisionRaw(Entity* a, Entity* b) {
	bool result = true;

	switch(a->type) {
		case EntityType_laserBolt: 
		case EntityType_laserBeam: {
			result = false;
		} break;

		case EntityType_player: {
			if (b->type == EntityType_blueEnergy || 
				b->type == EntityType_endPortal) result = false;
		} break;

		case EntityType_heavyTile: {
			if (b->type != EntityType_heavyTile &&
				b->type != EntityType_tile &&
				!isLaserEntityType(b) &&
				a->dP.y < 0) result = false; 
		} break;
	}

	return result;
}

bool isSolidCollision(Entity* a, Entity* b) {
	bool result = isSolidCollisionRaw(a, b) && isSolidCollisionRaw(b, a);
	return result;
}

V2 moveRaw(Entity*, GameState*, V2, V2* ddP = 0);

void onCollide(Entity* entity, Entity* hitEntity, GameState* gameState, bool solid, V2 entityVel) {
	switch(entity->type) {
		case EntityType_blueEnergy: {
			if (hitEntity->type == EntityType_player) {
				setFlags(entity, EntityFlag_remove);
				gameState->blueEnergy++;
			}
		} break;



		case EntityType_endPortal: {
			if (hitEntity->type == EntityType_player) {
				gameState->loadNextLevel = true;
			}
		} break;



		case EntityType_laserBolt: {
			if(
			   hitEntity->type == EntityType_virus 
			   || hitEntity->type == EntityType_flyingVirus 
			   //|| hitEntity->type == EntityType_player
			 ) setFlags(hitEntity, EntityFlag_remove);

			setFlags(entity, EntityFlag_remove);
		} break;



		case EntityType_laserBeam: {
			setFlags(hitEntity, EntityFlag_remove);
		} break;


		case EntityType_heavyTile: {
			if(!solid) {
				V2 moveDelta = v2(0, entityVel.y);
				V2 totalMovement = moveRaw(hitEntity, gameState, moveDelta);

				double squishY = entityVel.y - totalMovement.y;
				double squishFactor = (hitEntity->renderSize.y + squishY) / hitEntity->renderSize.y;

				hitEntity->renderSize.y += squishY;
				hitEntity->clickBox = scaleRect(hitEntity->clickBox, v2(1, squishFactor));

				bool hasNonZeroHitbox = false;

				Hitbox* hitboxList = hitEntity->hitboxes;

				while(hitboxList) {
					hitboxList->collisionSize.y += squishY;
					if (hitboxList->collisionSize.y > 0) hasNonZeroHitbox = true;
					hitboxList = hitboxList->next;
				}

				if (!hasNonZeroHitbox || hitEntity->renderSize.y <= 0) setFlags(hitEntity, EntityFlag_remove);

				moveRaw(hitEntity, gameState, v2(0, squishY));
			}
		} break;
	}
}

void freeRefNode(RefNode* node, GameState* gameState) {
	if (node->next) {
		freeRefNode(node->next, gameState);
	}

	node->ref = 0;
	node->next = gameState->refNodeFreeList;
	gameState->refNodeFreeList = node;
}

R2 getHitbox(Entity* entity, Hitbox* hitbox) {
	assert(hitbox);

	V2 offset = hitbox->collisionOffset;
	if (isSet(entity, EntityFlag_facesLeft)) offset.x *= -1;

	R2 result = rectCenterDiameter(entity->p + offset, hitbox->collisionSize);
	return result;
}

R2 getMaxCollisionExtents(Entity* entity) {
	R2 result = rectCenterRadius(entity->p, v2(0, 0));

	Hitbox* hitboxList = entity->hitboxes;

	while(hitboxList) {
		R2 hitbox = getHitbox(entity, hitboxList);

		if (hitbox.min.x < result.min.x) result.min.x = hitbox.min.x;
		if (hitbox.min.y < result.min.y) result.min.y = hitbox.min.y;
		if (hitbox.max.x > result.max.x) result.max.x = hitbox.max.x;
		if (hitbox.max.y > result.max.y) result.max.y = hitbox.max.y;

		hitboxList = hitboxList->next;
	}

	return result;
}

void getLineCollisionTime(double lx1, double lx2, double ly, double posX, double posY, 
						  double dPosX, double dPosY, bool top, 
						  GetCollisionTimeResult* result, bool* hit, bool* solidHit, bool horizontalCollision) {
	double collisionTime;

	double epsilon = 0.000000000001;

	if (dPosY == 0) {
		collisionTime = -1;
	} else {
		collisionTime = (ly - posY) / dPosY;

		if (collisionTime <= epsilon && ((dPosY > 0 && top) || (dPosY < 0 && !top))) {
			collisionTime = -1;
		} else {
			posX += dPosX * collisionTime;

			if (posX > lx1 && posX < lx2) {

			} else {
				collisionTime = -1;
			}
		}
	}

	if (collisionTime >= 0 && collisionTime < result->solidCollisionTime && horizontalCollision) {
		result->solidCollisionTime = collisionTime;
		result->solidHorizontalCollision = horizontalCollision;
		*solidHit = true;
	}

	if (collisionTime >= 0 && collisionTime < result->collisionTime) {
		result->collisionTime = collisionTime;
		result->horizontalCollision = horizontalCollision;
		*hit = true;
	}
}

void checkCollision(V2 delta, R2 colliderHitbox, Entity* entity, Entity* collider, Hitbox* colliderHitboxList, 
					Hitbox* entityHitboxList, GetCollisionTimeResult* result) {
	//Minkowski sum
	R2 sum = addDiameterTo(colliderHitbox, entityHitboxList->collisionSize);

	V2 center = entity->p + entityHitboxList->collisionOffset;

	bool hit = false;
	bool solidHit = false;

	getLineCollisionTime(sum.min.x, sum.max.x, sum.min.y, center.x, center.y, 
						 delta.x, delta.y, false, result, &hit, &solidHit, true);

	getLineCollisionTime(sum.min.x, sum.max.x, sum.max.y, center.x, center.y, 
						 delta.x, delta.y, true, result, &hit, &solidHit, true);

	getLineCollisionTime(sum.min.y, sum.max.y, sum.min.x, center.y, center.x, 
		 				 delta.y, delta.x, false, result, &hit, &solidHit, false);

	getLineCollisionTime(sum.min.y, sum.max.y, sum.max.x, center.y, center.x, 
 						 delta.y, delta.x, true, result, &hit, &solidHit, false);

	if (hit) {
		result->hitEntity = collider;
	}

	if (solidHit && isSolidCollision(entity, collider)) { 
		result->solidEntity = collider;
	}
}

GetCollisionTimeResult getCollisionTime(Entity* entity, GameState* gameState, V2 delta) {
	GetCollisionTimeResult result = {};
	result.collisionTime = 1;
	result.solidCollisionTime = 1;

	int partitionCenterX = (int)(entity->p.x / gameState->chunkSize.x);
	int partitionCenterY = (int)(entity->p.y / gameState->chunkSize.y);

	for (int partitionXOffs = -1; partitionXOffs <= 1; partitionXOffs++) {
		for (int partitionYOffs = -1; partitionYOffs <= 1; partitionYOffs++) {
			int partitionX = partitionCenterX + partitionXOffs;
			int partitionY = partitionCenterY + partitionYOffs;

			if (partitionX >= 0 && 
				partitionY >= 0 && 
				partitionX < gameState->chunksWidth && 
				partitionY < gameState->chunksHeight) {

				EntityChunk* chunk = gameState->chunks + partitionY * gameState->chunksWidth + partitionX;

				while(chunk) {
					for (int colliderIndex = 0; colliderIndex < chunk->numRefs; colliderIndex++) {
						Entity* collider = getEntityByRef(gameState, chunk->entityRefs[colliderIndex]);

						if (collider && collider != entity &&
							collidesWith(entity, collider, gameState)) {

							if (isTileType(collider) && isTileType(entity)) {
								Hitbox tileHitbox = {};
								tileHitbox.collisionSize = v2(1, 1) * gameState->tileSize;
								tileHitbox.collisionOffset = v2(0, -gameState->tileSize / 2);
								R2 colliderHitbox = getHitbox(collider, &tileHitbox);
								checkCollision(delta, colliderHitbox, entity, collider,
											&tileHitbox, &tileHitbox, &result);
							} else {
								Hitbox* colliderHitboxList = collider->hitboxes;

								while(colliderHitboxList) {
									R2 colliderHitbox = getHitbox(collider, colliderHitboxList);

									Hitbox* entityHitboxList = entity->hitboxes;

									while (entityHitboxList) {
										checkCollision(delta, colliderHitbox, entity, collider,
											colliderHitboxList, entityHitboxList, &result);
										entityHitboxList = entityHitboxList->next;
									}

									colliderHitboxList = colliderHitboxList->next;
								}
							}
						}
					}
					chunk = chunk->next;
				}
			}
		}
	}

	return result;
}

V2 getVelocity(double dt, V2 dP, V2 ddP) {
	V2 result = dt * dP + 0.5f * dt * dt * ddP;
	return result;
}

Entity* onGround(Entity* entity, GameState* gameState, double dt, V2 dP, V2 ddP) {
	V2 delta = getVelocity(dt, dP, ddP);
	GetCollisionTimeResult collisionResult = getCollisionTime(entity, gameState, delta);
	return collisionResult.solidEntity;
}

Entity* onGround(Entity* entity, GameState* gameState) {
	double moveTime = 1.0 / 60.0;
	Entity* result = onGround(entity, gameState, moveTime, v2(0, 0), gameState->gravity);
	return result;
}

Entity* getAbove(Entity* entity, GameState* gameState) {
	double moveTime = 1.0 / 60.0;
	Entity* result = onGround(entity, gameState, moveTime, v2(0, 0), -gameState->gravity);
	return result;
}

//Returns the total movement that is made
V2 moveRaw(Entity* entity, GameState* gameState, V2 delta, V2* ddP) {
	double maxCollisionTime = 1;
	V2 totalMovement = {};

	for (int moveIteration = 0; moveIteration < 4 && maxCollisionTime > 0; moveIteration++) {
		GetCollisionTimeResult collisionResult = getCollisionTime(entity, gameState, delta);
		double collisionTime = min(maxCollisionTime, collisionResult.collisionTime);

		maxCollisionTime -= collisionTime;

		double collisionTimeEpsilon = 0.001;
		double moveTime = max(0, collisionTime - collisionTimeEpsilon); 

		V2 movement = delta * moveTime;
		setEntityP(entity, entity-> p + movement, gameState);
		totalMovement += movement;

		if (collisionResult.hitEntity) {
			bool solid = false;

			if (isSolidCollision(collisionResult.hitEntity, entity)) {
    			if (collisionResult.horizontalCollision) {
    				solid = true;
					bool hitGround = false;

    				//NOTE: If moving and you would hit an entity which is using you as ground
    				//		then we try moving that entity first. Then retrying this move. 
    				// 		This doesn't work between tiles since they are placed right next to
    				//		each other and this causes numerical precision errors which allow them
    				//		to intersect.
    				if (!isTileType(entity) || !isTileType(collisionResult.hitEntity)) {
    					RefNode* nextTopEntity = entity->groundReferenceList;
						RefNode* prevTopEntity = NULL;

	    				while (nextTopEntity) {
							if (nextTopEntity->ref == collisionResult.hitEntity->ref) {
								V2 pushAmount = maxCollisionTime * delta;
								moveRaw(collisionResult.hitEntity, gameState, pushAmount);

								if (prevTopEntity) {
									prevTopEntity->next = nextTopEntity->next;
								} else {
									entity->groundReferenceList = NULL;
								}

								nextTopEntity->next = NULL;
								freeRefNode(nextTopEntity, gameState);

								hitGround = true;
								break;
							}

							prevTopEntity = nextTopEntity;
							nextTopEntity = nextTopEntity->next;
						}
    				}

					if (!hitGround) {
						delta.y = 0;
	    				entity->dP.y = 0;
	    				if (ddP) ddP->y = 0;
	    			} 
    			} else {
					delta.x = 0;
    				entity->dP.x = 0;
    				if (ddP) ddP->x = 0;
    			}
			}

			onCollide(entity, collisionResult.hitEntity, gameState, solid, maxCollisionTime * delta);
			onCollide(collisionResult.hitEntity, entity, gameState, solid, v2(0, 0));
		}
	}

	//NOTE: This moves all of the entities which are supported by this one (as the ground)
	RefNode* nextTopEntity = entity->groundReferenceList;

	if (lengthSq(totalMovement) > 0) {
		while (nextTopEntity) {
			Entity* top = getEntityByRef(gameState, nextTopEntity->ref);
			if (top) {
				moveRaw(top, gameState, totalMovement);
			}
			nextTopEntity = nextTopEntity->next;
		}
	}

	return totalMovement;
}

void move(Entity* entity, double dt, GameState* gameState, V2 ddP) {
	V2 delta = getVelocity(dt, entity->dP, ddP);
	moveRaw(entity, gameState, delta, &ddP);
	entity->dP += ddP * dt;
}

bool moveTowardsTarget(Entity* entity, GameState* gameState, double dt,
					   V2 target, double initialDstToTarget, double maxSpeed) {
	V2 delta = target - entity->p;
	double dstToTarget = length(delta);

	double percentageToTarget = (initialDstToTarget - dstToTarget) / initialDstToTarget;

	double initialTime = 0.1;
	double speedEquationCoefficient = maxSpeed / ((0.5 + initialTime) * (-0.5 - initialTime));
	double speed = dt * speedEquationCoefficient * (percentageToTarget + initialTime) * (percentageToTarget - 1 - initialTime);

	V2 movement;

	if (speed > dstToTarget) {
		movement = delta;
	} else {
		movement = speed * normalize(delta);
	}

	moveRaw(entity, gameState, movement);

	return entity->p == target;
}

ConsoleField* getField(Entity* entity, ConsoleFieldType type) {
	ConsoleField* result = false;

	for (int fieldIndex = 0; fieldIndex < entity->numFields; fieldIndex++) {
		if (entity->fields[fieldIndex]->type == type) {
			result = entity->fields[fieldIndex];
			break;
		}
	}

	return result;
}

bool isMouseInside(Entity* entity, Input* input) {
	bool result = pointInsideRect(translateRect(entity->clickBox, entity->p), input->mouseInWorld);
	return result;
}

bool inSolidGridBounds(GameState* gameState, int xTile, int yTile) {
	bool result = xTile >= 0 &&
				  yTile >= 0 &&
				  xTile < gameState->solidGridWidth &&
				  yTile < gameState->solidGridHeight;

	return result;
}

void addSolidLocation(double xPos, double yPos, GameState* gameState) {
	int xTile = (int)(xPos / gameState->solidGridSquareSize);
	int yTile = (int)(yPos / gameState->solidGridSquareSize);

	if (inSolidGridBounds(gameState, xTile, yTile)) {
		gameState->solidGrid[xTile * gameState->solidGridHeight + yTile].solid = true;
	}
}

void testLocationAsClosestNonSolidNode(double xPos, double yPos, double* minDstSq, GameState* gameState, 
									   Entity* entity, PathNode** result) {
	#if 0
		setColor(gameState->renderer, 0, 255, 0, 255);
		R2 rect = rectCenterRadius(v2(xPos, yPos), v2(0.01, 0.01));
		drawFilledRect(gameState, rect, gameState->cameraP);
	#endif

	int xTile = (int)floor(xPos / gameState->solidGridSquareSize);
	int yTile = (int)floor(yPos / gameState->solidGridSquareSize);

	if (inSolidGridBounds(gameState, xTile, yTile)) {
		PathNode* node = gameState->solidGrid + xTile * gameState->solidGridHeight + yTile;

		if (!node->solid) {
			double testDstSq = dstSq(node->p, entity->p);

			if (testDstSq < *minDstSq) {
				*minDstSq = testDstSq;
				*result = node;
			}
		}
	}
}

PathNode* getClosestNonSolidNode(Entity* entity, GameState* gameState, Entity* other = NULL) {
	PathNode* result = NULL;
	double minDstSq = 100000000000.0;

	Hitbox* hitboxList = entity->hitboxes;

	V2 otherSize;
	int radiusFudge = 5;

	if(other) {
		R2 otherHitbox = getMaxCollisionExtents(other);
		otherSize = getRectSize(otherHitbox);
		radiusFudge = 1;
	}

	testLocationAsClosestNonSolidNode(entity->p.x, entity->p.y, &minDstSq, gameState, entity, &result);

	while (hitboxList) {
		R2 hitbox = getHitbox(entity, hitboxList);

		if(other) {
			hitbox = addDiameterTo(hitbox, otherSize);
		}

		V2 hitboxCenter = getRectCenter(hitbox);

		int radiusX = (int)ceil(getRectWidth(hitbox) / (2 * gameState->solidGridSquareSize)) + radiusFudge;
		int radiusY = (int)ceil(getRectHeight(hitbox) / (2 * gameState->solidGridSquareSize)) + radiusFudge;

		for(int xOffs = -radiusX; xOffs <= radiusX; xOffs++) {
			for (int yOffs = -radiusY; yOffs <= radiusY; yOffs++) {
				double xPos = hitboxCenter.x + xOffs * gameState->solidGridSquareSize;
				double yPos = hitboxCenter.y + yOffs * gameState->solidGridSquareSize;

				
				testLocationAsClosestNonSolidNode(xPos, yPos, &minDstSq, gameState, entity, &result);
			}
		}

		hitboxList = hitboxList->next;
	}

	return result;
}

bool pathLineClear(V2 p1, V2 p2, GameState* gameState) {
	double minX, maxX, minY, maxY;

	if (p1.x < p2.x) {
		minX = p1.x;
		maxX = p2.x;
	} else {
		minX = p2.x;
		maxX = p1.x;
	}

	if (p1.y < p2.y) {
		minY = p1.y;
		maxY = p2.y;
	} else {
		minY = p2.y;
		maxY = p1.y;
	}

	double increment = gameState->solidGridSquareSize / 6.0;
	V2 line = p2 - p1;
	double lineLength = length(p2 - p1);
	V2 delta = normalize(line);

	for(double linePos = 0; linePos < lineLength; linePos += increment) {
		V2 p = p1 + delta * linePos;

		int xTile = (int)floor(p.x / gameState->solidGridSquareSize);
		int yTile = (int)floor(p.y / gameState->solidGridSquareSize);

		if (inSolidGridBounds(gameState, xTile, yTile)) {
			if (gameState->solidGrid[xTile * gameState->solidGridHeight + yTile].solid) return false;
		}
	}

	int xTile = (int)floor(maxX / gameState->solidGridSquareSize);
	int yTile = (int)floor(maxY / gameState->solidGridSquareSize);	

	if (inSolidGridBounds(gameState, xTile, yTile)) {
		if (gameState->solidGrid[xTile * gameState->solidGridHeight + yTile].solid) return false;
	}

	return true;
}

V2 computePath(GameState* gameState, Entity* start, Entity* goal) {
	for (int tileX = 0; tileX < gameState->solidGridWidth; tileX++) {
		for (int tileY = 0; tileY < gameState->solidGridHeight; tileY++) {
			PathNode* node = gameState->solidGrid + tileX * gameState->solidGridHeight + tileY;
			node->solid = false;
			node->open = false;
			node->closed = false;
			node->parent = NULL;
			node->costToHere = 0;
			node->costToGoal = 0;
		}
	}

	gameState->openPathNodesCount = 0;

	for (int colliderIndex = 0; colliderIndex < gameState->numEntities; colliderIndex++) {
		Entity* collider = gameState->entities + colliderIndex;

		if (collider != start && collider != goal &&
			collidesWith(start, collider, gameState) &&
			collider->shooterRef != goal->ref) {

			Hitbox* colliderHitboxList = collider->hitboxes;

			while(colliderHitboxList) {
				R2 colliderHitbox = getHitbox(collider, colliderHitboxList);

				Hitbox* entityHitboxList = start->hitboxes;

				while (entityHitboxList) {
					R2 sum = addDiameterTo(colliderHitbox, entityHitboxList->collisionSize);
					sum = translateRect(sum, entityHitboxList->collisionOffset);

					for (double xPos = sum.min.x; xPos < sum.max.x; xPos += gameState->solidGridSquareSize) {
						for (double yPos = sum.min.y; yPos < sum.max.y; yPos += gameState->solidGridSquareSize) {
							addSolidLocation(xPos, yPos, gameState);
						}
					}

					for (double xPos = sum.min.x; xPos < sum.max.x; xPos += gameState->solidGridSquareSize) {
						addSolidLocation(xPos, sum.max.y, gameState);
					}

					for (double yPos = sum.min.y; yPos < sum.max.y; yPos += gameState->solidGridSquareSize) {
						addSolidLocation(sum.max.x, yPos, gameState);
					}

					addSolidLocation(sum.max.x, sum.max.y, gameState);

					entityHitboxList = entityHitboxList->next;
				}

				colliderHitboxList = colliderHitboxList->next;
			}
		}
	}

#if 0
	SDL_SetRenderDrawBlendMode(gameState->renderer, SDL_BLENDMODE_BLEND);
	setColor(gameState->renderer, 0, 0, 0, 100);

	for (int tileX = 0; tileX < gameState->solidGridWidth; tileX++) {
		for (int tileY = 0; tileY < gameState->solidGridHeight; tileY++) {
			PathNode* node = gameState->solidGrid[tileX] + tileY;

			if (node->solid) {
				drawFilledRect(gameState, rectCenterDiameter(node->p, 
					v2(gameState->solidGridSquareSize, gameState->solidGridSquareSize)), gameState->cameraP);
			}
		}
	}
#endif

	if (pathLineClear(start->p, goal->p, gameState)) {
		return goal->p;
	}

	PathNode* startNode = getClosestNonSolidNode(start, gameState);
	PathNode* goalNode = getClosestNonSolidNode(goal, gameState, start);

	if (startNode && goalNode) {
		startNode->open = true;
		gameState->openPathNodes[0] = startNode;
		gameState->openPathNodesCount = 1;
		
		while(gameState->openPathNodesCount) {
			PathNode* current = NULL;
			int currentIndex = 0;
			double minEstimatedTotalCost = 1000000000000.0;

			for (int nodeIndex = 0; nodeIndex < gameState->openPathNodesCount; nodeIndex++) {
				PathNode* testNode = gameState->openPathNodes[nodeIndex];

				if (testNode->open) {
					double testEstimatedCost = testNode->costToHere + testNode->costToGoal;

					if (testEstimatedCost < minEstimatedTotalCost) {
						minEstimatedTotalCost = testEstimatedCost;
						current = testNode;
						currentIndex = nodeIndex;
					}
				}
			}

			assert(current);
			current->open = false;
			current->closed = true;
			gameState->openPathNodesCount--;

			if (gameState->openPathNodesCount) {
				gameState->openPathNodes[currentIndex] = gameState->openPathNodes[gameState->openPathNodesCount];
			} else {
				gameState->openPathNodes[0] = NULL;
			}

			if (current == goalNode) {
				V2 prevPoint = goal->p;
				PathNode* node = current;
				PathNode* prevNode = NULL;

				//NOTE: This is to smooth the path
				//TODO: Can optimize this by automatically removing co-linear points without checking pathLineClear
				while(node && node != startNode) {
					if (prevNode) {
						if (pathLineClear(node->p, prevNode->p, gameState)) {
							prevNode->parent = node->parent;
							node = prevNode;
						} else {
							prevNode = node;
						}
					} else {
						prevNode = node;
					}
					
					node = node->parent;
				}

				#if 0
				//NOTE: This draws the path
				node = current;
				setColor(gameState->renderer, 100, 75, 200, 255);

				while(node) {
					drawLine(gameState, prevPoint - gameState->cameraP, node->p - gameState->cameraP);
					prevPoint = node->p;
					node = node->parent;
				}
				#endif

				//TODO: Return path here
				node = current;
				PathNode* firstNode = NULL;
				PathNode* secondNode = NULL;

				while(node) {
					secondNode = firstNode;
					firstNode = node;
					node = node->parent;
				}

				if (secondNode) {
					return secondNode->p;
				}

				break;
			}

			for(int xOffs = -1; xOffs <= 1; xOffs++) {
				for (int yOffs = -1; yOffs <= 1; yOffs++) {
					if (xOffs != 0 || yOffs != 0) {
						int tileX = current->tileX + xOffs;
						int tileY = current->tileY + yOffs;

						if (inSolidGridBounds(gameState, tileX, tileY)) {
							PathNode* testNode = gameState->solidGrid + tileX * gameState->solidGridHeight + tileY;

							if (!testNode->solid) {
								#if 0
								if (current->parent) {
									int dx = current->tileX - current->parent->tileX;
									int dy = current->tileY - current->parent->tileY;

									if (dy == 0) {
										if (xOffs != dx ||
										    (yOffs == 1 && tileY + 1 < gameState->solidGridHeight &&
										    	!gameState->solidGrid[tileX][tileY + 1].solid) ||

											(yOffs == -1 && tileY + 1 >= 0 && 
												!gameState->solidGrid[tileX][tileY - 1].solid)) continue;
									}
									else if (dx == 0) {
										if (yOffs != dy ||
										   (xOffs == 1 && tileX + 1 < gameState->solidGridWidth && 
										   	!gameState->solidGrid[tileX + 1][tileY].solid) ||

										   (xOffs == -1 && tileX - 1 >= 0 &&
										   	!gameState->solidGrid[tileX - 1][tileY].solid)) continue;
									}
									else {
										if ((xOffs == -dx && tileX - dx >= 0 && tileX - dx < gameState->solidGridWidth &&
											!(yOffs == dy && gameState->solidGrid[tileX - dx][tileY].solid)) ||

										   ( yOffs == -dy && tileY - dy >= 0 && tileY - dy < gameState->solidGridHeight &&
										   	!(xOffs == dx && gameState->solidGrid[tileX][tileY - dy].solid)))
											continue;
									}
								}
								#endif
							
								double costToHere = current->costToHere;

								if(xOffs == 0 || yOffs == 0) {
									costToHere += 1;
								}
								else {
									costToHere += SQRT2;
								}

								if ((testNode->open || testNode->closed) && testNode->costToHere <= costToHere)
									continue;

								testNode->costToHere = costToHere;
								testNode->costToGoal = abs(goalNode->tileY - tileY) +
													   abs(goalNode->tileX - tileX);
								testNode->closed = false;
								testNode->parent = current;

								if (!testNode->open) {
									testNode->open = true;
									gameState->openPathNodes[gameState->openPathNodesCount++] = testNode;
									assert(gameState->openPathNodesCount < arrayCount(gameState->openPathNodes));
								}
							}
						}
					}
				}
			}
		}
	}

	return start->p;
}

void centerCameraAround(Entity* entity, GameState* gameState) {
	double maxCameraX = gameState->mapSize.x - gameState->windowSize.x;
	gameState->newCameraP.x = clamp((double)(entity->p.x - gameState->windowSize.x / 2.0), 0, maxCameraX);
}

bool canEntityShoot(Entity* entity, GameState* gameState) {
	bool result = false;

	ConsoleField* shootField = getField(entity, ConsoleField_shootsAtTarget);
	result = shootField && gameState->shootTargetRef && gameState->shootTargetRef != entity->ref;

	return result;
}

void updateAndRenderEntities(GameState* gameState, double dtForFrame) {
	double dt = getEntityByRef(gameState, gameState->consoleEntityRef) ? 0 : dtForFrame;

	if(!gameState->doingInitialSim) {
		//TODO: It might be possible to combine the three loops which handle ground reference lists later
		//TODO: An entities ground reference list could be reset right after it is done being updated and rendered
		//NOTE: This is to reset all of the entities ground reference lists
		for (int entityIndex = 0; entityIndex < gameState->numEntities; entityIndex++) {
			Entity* entity = gameState->entities + entityIndex;

			entity->timeSinceLastOnGround += dt;
			clearFlags(entity, EntityFlag_grounded);

			//NOTE: A ground reference betweeen the base and the beam and between the beam and the top
			//		always persists. 
			if(entity->type == EntityType_laserBase || entity->type == EntityType_laserBeam) {
				if (entity->groundReferenceList->next) {
					freeRefNode(entity->groundReferenceList->next, gameState);
					entity->groundReferenceList->next = NULL;
				}
			}
			else {
				if (entity->groundReferenceList) {
					freeRefNode(entity->groundReferenceList, gameState);
					entity->groundReferenceList = NULL;
				}
			}
		}

		//NOTE: This loops though all the entities to set if they are on the ground at the beginning of the frame
		//		and to setup their groundReferenceList's
		for (int entityIndex = 0; entityIndex < gameState->numEntities; entityIndex++) {
			Entity* entity = gameState->entities + entityIndex;

			Entity* above = getAbove(entity, gameState);

			if (above) {
				addGroundReference(above, entity, gameState);
			}

			Entity* below = onGround(entity, gameState);

			if (below) {
					addGroundReference(entity, below, gameState);
			}
		}
	}

	double frictionGroundCoefficient = -15.0f;
	double groundFriction = pow(E, frictionGroundCoefficient * dt);

	//NOTE: This loops through all of the entities in the game state to update and render them
	for (int entityIndex = 0; entityIndex < gameState->numEntities; entityIndex++) {
		Entity* entity = gameState->entities + entityIndex;

		bool showPlayerHackingAnim = entity->type == EntityType_player && getEntityByRef(gameState, gameState->consoleEntityRef);

		if (showPlayerHackingAnim) {
			entity->animTime += dtForFrame;
		} else {
			entity->animTime += dt;
		}

		removeFieldsIfSet(entity->fields, &entity->numFields);




		bool shootingState = false;
		ConsoleField* shootField = NULL;
		bool shootingEnabled = false;

		if (entity->type == EntityType_laserTop) {
			Entity* laserBase = getEntityByRef(gameState, entity->ref - 1);
			if(laserBase) {
				assert(laserBase->type == EntityType_laserBase && 
					   getEntityByRef(gameState, laserBase->groundReferenceList->ref)->groundReferenceList->ref == entity->ref);

				shootingEnabled = canEntityShoot(laserBase, gameState); 
				shootField = getField(laserBase, ConsoleField_shootsAtTarget);
			}
		} else {
			shootingEnabled = canEntityShoot(entity, gameState);
			shootField = getField(entity, ConsoleField_shootsAtTarget);
		}


		//NOTE:This handles shooting if the entity should shoot
		if (shootingEnabled) {
			Entity* target = getEntityByRef(gameState, gameState->shootTargetRef);

			if (target) {
				double dstToTarget = dstSq(target->p, entity->p);

				ConsoleField* bulletSpeedField = shootField->children[0];
				double bulletSpeed = bulletSpeedField->doubleValues[bulletSpeedField->selectedIndex];

				ConsoleField* radiusField = shootField->children[1];
				double radius = radiusField->doubleValues[radiusField->selectedIndex];

				double shootRadius = square(radius);

				V2 spawnOffset = v2(0, 0);

				switch(entity->type) {
					case EntityType_virus:
						spawnOffset = v2(0, -0.3);
						break;
					case EntityType_flyingVirus:
						spawnOffset = v2(0.2, -0.29);
				}

				if (isSet(entity, EntityFlag_facesLeft)) {
					spawnOffset.x *= -1;
				}

				bool shouldShoot = isSet(entity, EntityFlag_shooting) ||
								   isSet(entity, EntityFlag_unchargingAfterShooting) || 
								   dstToTarget <= shootRadius;

				if (shouldShoot) {
					shootingState = true;

					if (target->p.x < entity->p.x) setFlags(entity, EntityFlag_facesLeft);
					else clearFlags(entity, EntityFlag_facesLeft);

					if (isSet(entity, EntityFlag_unchargingAfterShooting)) {
						entity->shootTimer -= dt;

						if (entity->shootTimer <= 0) {
							entity->shootTimer = 0;
							clearFlags(entity, EntityFlag_unchargingAfterShooting);
						} 
					} else if (!isSet(entity, EntityFlag_shooting)) {
						entity->shootTimer = 0;
						if (!showPlayerHackingAnim) entity->animTime = 0;
						setFlags(entity, EntityFlag_shooting);
					} else {
						entity->shootTimer += dt;

						if (entity->shootTimer >= gameState->shootDelay) {
							entity->shootTimer = gameState->shootDelay;
							clearFlags(entity, EntityFlag_shooting);
							setFlags(entity, EntityFlag_unchargingAfterShooting);
							addLaserBolt(gameState, entity->p + spawnOffset, target->p, entity->ref, bulletSpeed);
						}
					}
				}
			}
		}


		//NOTE: This is a hack to make the gravity feel better. There is more gravity when an object
		//		like the player is falling to make the gravity seem less 'floaty'.
		V2 gravity = gameState->gravity;
		if (entity->dP.y < 0) gravity *= 2;		
		V2 ddP = gravity;

		ConsoleField* movementField = getMovementField(entity);

		if (entity->type != EntityType_laserBolt) {
			entity->dP.x *= groundFriction;

			if (movementField && movementField->type == ConsoleField_seeksTarget) {
				entity->dP.y *= groundFriction;
			}
		}


		//NOTE: This handles moving the entity
		if (movementField) {
			ConsoleField* speedField = movementField->children[0];
			double xMoveAcceleration = speedField->doubleValues[speedField->selectedIndex];

			switch(movementField->type) {




				case ConsoleField_keyboardControlled: {
					if (!gameState->doingInitialSim && !isSet(entity, EntityFlag_remove)) {
						centerCameraAround(entity, gameState);
						// double numTexels = gameState->newCameraP.x / gameState->texel.x;
						// double overflow = (numTexels - (int)numTexels) * gameState->texel.x;
						// gameState->newCameraP.x -= overflow;

						double xMove = 0;

						ConsoleField* jumpField = movementField->children[1];
						double jumpHeight = jumpField->doubleValues[jumpField->selectedIndex];

						ConsoleField* doubleJumpField = movementField->children[2];
						bool canDoubleJump = doubleJumpField->selectedIndex != 0;

						if (gameState->input.rightPressed) xMove += xMoveAcceleration;
						if (gameState->input.leftPressed) xMove -= xMoveAcceleration;

						ddP.x += xMove;

						if (xMove > 0) clearFlags(entity, EntityFlag_facesLeft);
						else if (xMove < 0) setFlags(entity, EntityFlag_facesLeft);

						bool canJump = entity->jumpCount == 0 && 
									   entity->timeSinceLastOnGround < 0.15 && 
									   gameState->input.upPressed;

						bool attemptingDoubleJump = false;
						
						if (!canJump) {
							attemptingDoubleJump = true;
							canJump = entity->jumpCount < 2 && canDoubleJump && gameState->input.upJustPressed;
						}

						if (canJump) {
							entity->dP.y = jumpHeight;
							if (attemptingDoubleJump) entity->jumpCount = 2;
							else entity->jumpCount++;
						}
					}

					move(entity, dt, gameState, ddP);

					Hitbox* hitbox = entity->hitboxes;
					while(hitbox) {
						double minX = hitbox->collisionSize.x / 2;
						double maxX = gameState->mapSize.x - minX;

						double xPos = hitbox->collisionOffset.x + entity->p.x;

						if (xPos < minX) moveRaw(entity, gameState, v2(minX - xPos, 0));
						else if (xPos > maxX) moveRaw(entity, gameState, v2(maxX - xPos, 0));

						hitbox = hitbox->next;
					}
				} break;




				case ConsoleField_movesBackAndForth: {
					if (!shootingState) {
						V2 oldP = entity->p;
						V2 oldCollisionSize = entity->hitboxes->collisionSize;
						V2 oldCollisionOffset = entity->hitboxes->collisionOffset;

						bool initiallyOnGround = isSet(entity, EntityFlag_grounded);

						if (isSet(entity, EntityFlag_facesLeft)) {
							xMoveAcceleration *= -1;
							entity->hitboxes->collisionOffset.x -= entity->hitboxes->collisionSize.x / 2;
						} else {
							entity->hitboxes->collisionOffset.x += entity->hitboxes->collisionSize.x / 2;
						}

						entity->hitboxes->collisionSize.x = 0;
						ddP.x = xMoveAcceleration;

						if (isSet(entity, EntityFlag_noMovementByDefault)) {
							ddP.y = 0;
						}

						double startX = entity->p.x;
						move(entity, dt, gameState, ddP);
						double endX = entity->p.x;

						bool shouldChangeDirection = false;

						if (isSet(entity, EntityFlag_noMovementByDefault)) {
							shouldChangeDirection = startX == endX && dt > 0;
						} else {
							if (initiallyOnGround) {
								bool offOfGround = onGround(entity, gameState) == NULL;

								if (offOfGround) {
									entity->p = oldP;
								}

								shouldChangeDirection = offOfGround || entity->dP.x == 0;
							}
						}

						if (shouldChangeDirection) {
							toggleFlags(entity, EntityFlag_facesLeft);
						}

						entity->hitboxes->collisionSize = oldCollisionSize;
						entity->hitboxes->collisionOffset = oldCollisionOffset;
					}
				} break;




				case ConsoleField_seeksTarget: {
					if (!shootingState && !gameState->doingInitialSim) {
						ConsoleField* sightRadiusField = movementField->children[1];
						double sightRadius = sightRadiusField->doubleValues[sightRadiusField->selectedIndex];

						Entity* targetEntity = getEntityByRef(gameState, gameState->shootTargetRef);
						if (targetEntity) {
							double dstToTarget = dst(targetEntity->p, entity->p);

							if (dstToTarget <= sightRadius && dstToTarget > 0.1) {
								V2 wayPoint = computePath(gameState, entity, targetEntity);
								V2 delta = entity->p - wayPoint;

								if (lengthSq(delta) > 0) {
									ddP = normalize(delta) * -xMoveAcceleration;
									move(entity, dt, gameState, ddP);

									if (delta.x && delta.x < 0 == isSet(entity, EntityFlag_facesLeft)) {
										toggleFlags(entity, EntityFlag_facesLeft);
									}
								}
							}
						}
					}
				} break;
			}
		} else {
			if (!isSet(entity, EntityFlag_noMovementByDefault)) {
				if(entity->type == EntityType_laserBolt ||
				   entity->type == EntityType_heavyTile && entity->startPos != entity->p) {
					ddP.y = 0;
				} 

				move(entity, dt, gameState, ddP);
			}
		}


		//NOTE: This removes the entity if they are outside of the world
		if (isSet(entity, EntityFlag_removeWhenOutsideLevel)) {
			R2 world = r2(v2(0, 0), gameState->worldSize);

			bool insideLevel = false;

			Hitbox* hitboxList = entity->hitboxes;

			while(hitboxList) {
				R2 hitbox = getHitbox(entity, hitboxList);

				if (rectanglesOverlap(world, hitbox)) {
					insideLevel = true;
					break;
				}

				hitboxList = hitboxList->next;
			}

			if (!insideLevel) {
				setFlags(entity, EntityFlag_remove);
			}
		}

		//Remove entity if they are below the bottom of the world
		//The laser base and laser beams are not removed when the bottom is reached because their ground references
		//are used to pull down the laser top. The top will remove both the base and the beam when it is removed. 
		if (entity->p.y < -entity->renderSize.y
			&& entity->type != EntityType_laserBase && entity->type != EntityType_laserBeam) {
			setFlags(entity, EntityFlag_remove);
		}


		//NOTE: Individual entity logic here
		switch(entity->type) {
			 case EntityType_player: {
				if (!isSet(entity, EntityFlag_remove) && !gameState->doingInitialSim) {
					gameState->playerDeathStartP = entity->p;
					gameState->playerDeathSize = entity->renderSize;
				}
			} break;
		


			case EntityType_background: {
				Texture* bg = &gameState->bgTex;
				Texture* mg = &gameState->mgTex;

				double bgTexWidth = (double)bg->srcRect.w;
				double mgTexWidth = (double)mg->srcRect.w;
				double mgTexHeight = (double)mg->srcRect.h;

				double bgScrollRate = bgTexWidth / mgTexWidth;

				double bgHeight = gameState->windowHeight / gameState->pixelsPerMeter;
				double mgWidth = max(gameState->mapSize.x, mgTexWidth / gameState->pixelsPerMeter);
								
				double bgWidth = mgWidth / bgScrollRate - 1;

				double bgX = gameState->cameraP.x * (1 -  bgScrollRate);

				R2 bgBounds = r2(v2(bgX, 0), v2(bgWidth, bgHeight));
				R2 mgBounds = r2(v2(0, 0), v2(mgWidth, bgHeight));

				pushTexture(gameState->renderGroup, bg, translateRect(bgBounds, -gameState->cameraP), false, DrawOrder_background);
				pushTexture(gameState->renderGroup, mg, translateRect(mgBounds, -gameState->cameraP), false, DrawOrder_middleground);
			} break;


			case EntityType_heavyTile:
			case EntityType_tile: {
				assert(entity->numFields >= 2);
				assert(entity->fields[0]->type == ConsoleField_unlimitedInt && 
					   entity->fields[1]->type == ConsoleField_unlimitedInt);

				int fieldXOffset = entity->fields[0]->selectedIndex;
				int fieldYOffset = entity->fields[1]->selectedIndex;

				if(movementField) {
					entity->tileXOffset = fieldXOffset;
					entity->tileYOffset = fieldYOffset;
				}

				int dXOffset = fieldXOffset - entity->tileXOffset;
				int dYOffset = fieldYOffset - entity->tileYOffset;

				if (dXOffset != 0 || dYOffset != 0) {
					V2 target = entity->startPos + hadamard(v2((double)dXOffset, (double)dYOffset), 
						v2(1, 1) * gameState->tileSize/*getRectSize(entity->clickBox)*/);

					//double initialDst = dXOffset == 0 ? getRectHeight(entity->clickBox) : getRectWidth(entity->clickBox);

					double initialDst = max(gameState->tileSize, length(target - entity->p));

					if (moveTowardsTarget(entity, gameState, dtForFrame, target, initialDst, 6.0f)) {
						entity->tileXOffset = fieldXOffset;
						entity->tileYOffset = fieldYOffset;
					}
				} else {
					entity->startPos = entity->p;
				}

			} break;



			case EntityType_text: {
				assert(entity->numFields >= 1);
				assert(entity->fields[0]->type == ConsoleField_int);

				int selectedIndex = entity->fields[0]->intValues[entity->fields[0]->selectedIndex];
				setSelectedText(entity, selectedIndex, gameState);
			} break;




			case EntityType_playerDeath: {
				double initialDstToTarget = length(gameState->playerDeathStartP - gameState->playerStartP);
				double maxSpeed = 7.5 * cbrt(initialDstToTarget);

				if (moveTowardsTarget(entity, gameState, dt, gameState->playerStartP, 
										initialDstToTarget, maxSpeed)) {
					setFlags(entity, EntityFlag_remove);
					Entity* player = addPlayer(gameState, gameState->playerStartP - 0.5 * entity->renderSize);
					setFlags(player, EntityFlag_grounded);
					player->dP = v2(0, 0);
				}

				centerCameraAround(entity, gameState);
			} break;



			case EntityType_laserBase: {
				assert(entity->numFields >= 1);
				assert(entity->fields[0]->type == ConsoleField_bool);

				bool laserOn = (entity->fields[0]->selectedIndex != 0);

				Entity* beam = getEntityByRef(gameState, entity->ref + 2);
				assert(!beam || beam->type == EntityType_laserBeam);
				Entity* top = getEntityByRef(gameState, entity->ref + 1);
				assert(!top || top->type == EntityType_laserTop);

				if(laserOn) {
					entity->standTex = &gameState->laserBaseOn;
					setFlags(entity, EntityFlag_laserOn);

					if (beam) setFlags(beam, EntityFlag_laserOn);

					if(top) {
						top->standTex = &gameState->laserTopOn;
						setFlags(top, EntityFlag_laserOn);
					}
				}
				else {
					entity->standTex = &gameState->laserBaseOff;
					clearFlags(entity, EntityFlag_laserOn);

					if (beam) clearFlags(beam, EntityFlag_laserOn);

					if(top) {
						clearFlags(top, EntityFlag_laserOn);
						top->standTex = &gameState->laserTopOff;
					}
				}
			} break;
		}



		//NOTE: This handles rendering all the entities
		Texture* texture = NULL;
		AnimState curAnimState;

		if (entity->shootAnim && shootingState) {
		 	texture = getAnimationFrame(entity->shootAnim, entity->animTime);
		 	curAnimState = AnimState_shooting;
		}
		else if (entity->jumpTex && (entity->dP.y != 0 || !isSet(entity, EntityFlag_grounded))) {
			if (!showPlayerHackingAnim) entity->animTime = 0;
			texture = entity->jumpTex;
			curAnimState = AnimState_jumping;
		}
		else if (entity->walkAnim && abs(entity->dP.x) > 0.05f) {
			texture = getAnimationFrame(entity->walkAnim, entity->animTime);
			curAnimState = AnimState_walking;
		} else {
			texture = entity->standTex;
			curAnimState = AnimState_standing;
		}

		if (entity->lastAnimState != curAnimState) {
			
			bool standToWalk = (curAnimState == AnimState_walking && entity->lastAnimState == AnimState_standing);
			bool walkToStand = (curAnimState == AnimState_standing && entity->lastAnimState == AnimState_walking);

			if (standToWalk || walkToStand) {
				if (entity->prevAnimState != curAnimState) {
					if (!showPlayerHackingAnim) entity->animTime = 0;
				}

				if (entity->standWalkTransition) {
					double duration = getAnimationDuration(entity->standWalkTransition);

					if (entity->animTime >= duration) {
						entity->animTime -= duration;
						entity->lastAnimState = curAnimState;
					} else {
						texture = getAnimationFrame(entity->standWalkTransition, entity->animTime, walkToStand);
					}
				} else {
					entity->lastAnimState = curAnimState;
				}
			} else {
				entity->lastAnimState = curAnimState;
			}
	
		}

		entity->prevAnimState = curAnimState;

		if (showPlayerHackingAnim) {
			texture = getAnimationFrame(&gameState->playerHackingAnimation, entity->animTime, false);
		}

		if (texture != NULL) {
			bool drawLeft = isSet(entity, EntityFlag_facesLeft);

			if (entity->type == EntityType_laserBase) drawLeft = false;

			if (entity->type != EntityType_laserBeam || isSet(entity, EntityFlag_laserOn)) {
				// drawTexture(gameState, texture, 
				// 	translateRect(rectCenterDiameter(entity->p, entity->renderSize), -gameState->cameraP), 
				// 	drawLeft);

				pushTexture(gameState->renderGroup, texture, 
				 	translateRect(rectCenterDiameter(entity->p, entity->renderSize), -gameState->cameraP), 
					drawLeft, entity->drawOrder);
			}
		}

		#if SHOW_COLLISION_BOUNDS
			Hitbox* hitbox = entity->hitboxes;

			while (hitbox) {
				pushOutlinedRect(gameState->renderGroup, getHitbox(entity, hitbox),
								 0.005f, createColor(255, 127, 255, 255), true);
				hitbox = hitbox->next;
			}

		#endif
	}

	removeEntities(gameState);
}
