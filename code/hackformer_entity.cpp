Waypoint* allocateWaypoint(GameState* gameState, V2 p, Waypoint* next = NULL) {
	Waypoint* result;

	if(gameState->waypointFreeList) {
		result = gameState->waypointFreeList;
		gameState->waypointFreeList = gameState->waypointFreeList->next;
	} else {
		result = pushStruct(&gameState->levelStorage, Waypoint);
	}

	result->p = p;
	result->next = next;

	return result;
}

void freeWaypoints(Waypoint* waypoint, GameState* gameState) {
	Waypoint* start = waypoint;
	Waypoint* wp = waypoint;

	s32 freeCount = 0;

	while(wp) {
		if(freeCount != 0 && wp == start) break;
		freeCount++;

		Waypoint* next = wp->next;

		wp->next = gameState->waypointFreeList;
		gameState->waypointFreeList = wp;

		wp = next;
	}
}

Hitbox* createUnzeroedHitbox(GameState* gameState) {
	Hitbox* hitbox = NULL;

	if (gameState->hitboxFreeList) {
		hitbox = gameState->hitboxFreeList;
		gameState->hitboxFreeList = gameState->hitboxFreeList->next;
	} else {
		hitbox = pushStruct(&gameState->levelStorage, Hitbox);
	}

	return hitbox;
}

Hitbox* addHitbox(Entity* entity, GameState* gameState) {
	Hitbox* hitbox = createUnzeroedHitbox(gameState);

	hitbox->next = entity->hitboxes;
	entity->hitboxes = hitbox;

	hitbox->storedRotation = INVALID_STORED_HITBOX_ROTATION;
	hitbox->collisionOffset = v2(0, 0);

	return hitbox;
}

bool canEntityRotate(Entity* entity) {
	bool result = entity->type == EntityType_trojan ||
				  entity->type == EntityType_motherShip ||
				  entity->type == EntityType_trawlerBolt || 
				  entity->type == EntityType_trojanBolt ||
				  entity->type == EntityType_motherShipProjectile;

	return result;				  
}

void setHitboxSize(Entity* entity, Hitbox* hitbox, double width, double height) {
	if(canEntityRotate(entity)) {
		double size = length(v2(width, height) * 0.5) * 2;
		hitbox->collisionSize = v2(size, size);
	} else {
		hitbox->collisionSize = v2(width, height);
	}
}

V2 getHitboxCenter(Hitbox* hitbox, Entity* entity) {
	V2 offset =  hitbox->collisionOffset;
	if(isSet(entity, EntityFlag_facesLeft)) offset.x *= -1;
	if(isSet(entity, EntityFlag_flipY)) offset.y *= -1;

	V2 result = entity->p + offset;
	return result;
}

R2 getBoundingBox(Entity* entity, Hitbox* hitbox) {
	assert(hitbox);

	V2 center = getHitboxCenter(hitbox, entity);

	R2 result = rectCenterDiameter(center, hitbox->collisionSize);
	return result;
}

void giveEntityRectangularCollisionBounds(Entity* entity, GameState* gameState,
										  double xOffset, double yOffset, double width, double height) {
	Hitbox* hitbox = createUnzeroedHitbox(gameState);

	setHitboxSize(entity, hitbox, width, height);
	hitbox->collisionOffset = v2(xOffset, yOffset);

	double halfWidth = width / 2.0;
	double halfHeight = height / 2.0;

	hitbox->collisionPointsCount = 4;
	hitbox->originalCollisionPoints[0] = v2(-halfWidth, -halfHeight);
	hitbox->originalCollisionPoints[1] = v2(halfWidth, -halfHeight);
	hitbox->originalCollisionPoints[2] = v2(halfWidth, halfHeight);
	hitbox->originalCollisionPoints[3] = v2(-halfWidth, halfHeight);

	hitbox->storedRotation = INVALID_STORED_HITBOX_ROTATION;

	hitbox->next = entity->hitboxes;
	entity->hitboxes = hitbox;
}

bool canEntityBeFlipped(Entity* entity) {
	bool result = true;

	if(entity->type == EntityType_motherShip ||
	   entity->type == EntityType_trojan) result = false;

	return result;
}

void updateHitboxRotatedPoints(Hitbox* hitbox, Entity* entity) {
	bool32 facesLeft = isSet(entity, EntityFlag_flipX);
	double rotation = entity->rotation;

	if(entity->type == EntityType_trawler) {
		rotation = 0;
	}

	if(!canEntityBeFlipped(entity)) {
		facesLeft = false;
	}

	bool32 flipY = isSet(entity, EntityFlag_flipY);

	if(hitbox->storedRotation == INVALID_STORED_HITBOX_ROTATION || 
	   hitbox->storedRotation != rotation ||
	   hitbox->storedFlippedX != facesLeft) {
		double cosRot = cos(rotation);
		double sinRot = sin(rotation);

		for(s32 pIndex = 0; pIndex < hitbox->collisionPointsCount; pIndex++) {
			V2 original = hitbox->originalCollisionPoints[pIndex];

			if(facesLeft) original.x *= -1;
			if(flipY) original.y *= -1;

			hitbox->rotatedCollisionPoints[pIndex] = v2(original.x * cosRot - original.y * sinRot, 
														original.x * sinRot + original.y * cosRot);
		}

		hitbox->storedRotation = rotation;
		hitbox->storedFlippedX = facesLeft;
	}
}

bool toggleEntityFacingDirection(Entity* entity, GameState* gameState) {
	if(!canEntityBeFlipped(entity)) return true;

	toggleFlags(entity, EntityFlag_facesLeft);

	return true;
}

void setEntityFacingDirection(Entity* entity, bool facesLeft, GameState* gameState) {
	bool currentlyFacingLeft = isSet(entity, EntityFlag_facesLeft) != 0;

	if(facesLeft != currentlyFacingLeft) {
		toggleEntityFacingDirection(entity, gameState);
	}
}

RefNode* refNode(GameState* gameState, s32 ref, RefNode* next) {
	RefNode* result = NULL;

	if (gameState->refNodeFreeList) {
		result = gameState->refNodeFreeList;
		gameState->refNodeFreeList = gameState->refNodeFreeList->next;
	} else {
		result = pushStruct(&gameState->levelStorage, RefNode);
	}

	assert(result);

	result->next = next;
	result->ref = ref;

	return result;
}

void freeRefNode(RefNode* node, GameState* gameState) {
	if(node) {
		if (node->next) {
			freeRefNode(node->next, gameState);
		}

		node->ref = 0;
		node->next = gameState->refNodeFreeList;
		gameState->refNodeFreeList = node;
	}
}

s32 countValidRefNodes(RefNode** nodePtr, GameState* gameState) {
	s32 result = 0;

	RefNode* node = *nodePtr;
	RefNode* prev = NULL;

	while(node) {
		RefNode* next = node->next;

		Entity* entity = getEntityByRef(gameState, node->ref);

		if(entity) {
			prev = node;
			result++;
		}
		else {
			node->next = NULL;
			freeRefNode(node, gameState);
			if(prev) prev->next = next;
			else *nodePtr = next;
		}

		node = next;
	}

	return result;
}

void freeMessages(Messages* messages, GameState* gameState) {
	assert(!messages->next);

	messages->next = gameState->messagesFreeList;
	gameState->messagesFreeList = messages;
}

void addTargetRef(s32 ref, GameState* gameState) {
	gameState->targetRefs = refNode(gameState, ref, gameState->targetRefs);
}

void addGuardedTargetRef(s32 ref, GameState* gameState) {
	gameState->guardTargetRefs = refNode(gameState, ref, gameState->guardTargetRefs);
}

EntityReference* getEntityReferenceBucket(GameState* gameState, s32 ref) {
	s32 bucketIndex = (ref % (arrayCount(gameState->entityRefs_) - 2)) + 1;
	EntityReference* result = gameState->entityRefs_ + bucketIndex;
	return result;
}

EntityReference* getPreciseEntityReferenceBucket(GameState* gameState, s32 ref) {
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

Entity* getEntityByRef(GameState* gameState, s32 ref) {
	EntityReference* bucket = getPreciseEntityReferenceBucket(gameState, ref);

	Entity* result = NULL;
	if (bucket) result = bucket->entity;

	return result;
}

Entity* getUnremovedEntityByRef(GameState* gameState, s32 ref) {
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

	if (nextReference) {
		storeEntityReferenceInFreeList(nextReference, gameState);
	}

	reference->entity = NULL;
	reference->next = NULL;
}

EntityChunk* getSpatialChunk(V2 p, GameState* gameState) {
	s32 x = (s32)(p.x / gameState->chunkSize.x);
	s32 y = (s32)(p.y / gameState->chunkSize.y);

	EntityChunk* result = NULL;

	if (x >= 0 && 
		y >= 0 && 
		x < gameState->chunksWidth &&
		y < gameState->chunksHeight) {
		
		result = gameState->chunks + (y * gameState->chunksWidth + x);
	}

	return result;
}

void addToSpatialPartition(Entity* entity, GameState* gameState) {
	for(EntityChunk* chunk = getSpatialChunk(entity->p, gameState); chunk; chunk = chunk->next) {
		if (chunk->numRefs < arrayCount(chunk->entityRefs)) {
			chunk->entityRefs[chunk->numRefs++] = entity->ref;
			break;
		} 

		if (!chunk->next) {
			chunk->next = pushStruct(&gameState->levelStorage, EntityChunk);
			*chunk->next = {};
		}
	}
}

bool removeFromSpatialPartition(Entity* entity, GameState* gameState) {
	for(EntityChunk* chunk = getSpatialChunk(entity->p, gameState); chunk; chunk = chunk->next) {
		for (s32 refIndex = 0; refIndex < chunk->numRefs; refIndex++) {
			if (chunk->entityRefs[refIndex] == entity->ref) {
				chunk->entityRefs[refIndex] = chunk->entityRefs[--chunk->numRefs];
				return true;
			}
		}
	}

	return false;
}

bool isProjectile(Entity* entity) {
	bool result = entity->type == EntityType_motherShipProjectile ||
				  entity->type == EntityType_trawlerBolt ||
				  entity->type == EntityType_trojanBolt;

	return result;
}

void attemptToRemovePenetrationReferences(Entity*, GameState*);

void setEntityP(Entity* entity, V2 newP, GameState* gameState) {
	removeFromSpatialPartition(entity, gameState);
	entity->p = newP;
	addToSpatialPartition(entity, gameState);

	attemptToRemovePenetrationReferences(entity, gameState);
}

bool affectedByGravity(Entity* entity, ConsoleField* movementField) {
	bool result = true;

	bool32 noMovementByDefault = isSet(entity, EntityFlag_noMovementByDefault) && !getField(entity, ConsoleField_crushesEntities);

	if(noMovementByDefault) {
		result = false;

		if(movementField && entity->type != EntityType_bootUp) {
			if(movementField->type == ConsoleField_keyboardControlled) {
				result = true;
			}
		}
	} else {
		if(movementField && entity->type != EntityType_bootUp) {
			if(movementField->type == ConsoleField_bobsVertically ||
			   movementField->type == ConsoleField_followsWaypoints ||
			   movementField->type == ConsoleField_seeksTarget) {
				result = false;
			}
		}
	}

	// if(movementField) {
	// 	switch(movementField->type) {
	// 		case ConsoleField_keyboardControlled: {
	// 			result = true;
	// 		} break;
	// 		case ConsoleField_movesBackAndForth: {
	// 			return !noMovementByDefault;
	// 		} break;
	// 		case ConsoleField_bobsVertically:
	// 		case ConsoleField_followsWaypoints:
	// 		case ConsoleField_seeksTarget: {
	// 			result = false;
	// 		} break;

	// 		InvalidDefaultCase;
	// 	}
	// } else {
	// 	result = !noMovementByDefault;
	// }

	return result;
}

bool refNodeListContainsRef(RefNode* list, s32 ref) {
	RefNode* node = list;

	while(node) {
		if (node->ref == ref) return true;
		node = node->next;
	}

	return false;
}

void removeFromRefNodeList(RefNode** list, s32 ref, GameState* gameState) {
	RefNode* node = *list;
	RefNode* prevNode = NULL;

	while(node) {
		RefNode* next = node->next;

		if(node->ref == ref) {
			node->next = NULL;
			freeRefNode(node, gameState);

			if(prevNode) prevNode->next = next;
			else *list = next;
		} else {
			prevNode = node;
		}

		node = next;
	}
}

void removeTargetRef(s32 ref, GameState* gameState) {
	removeFromRefNodeList(&gameState->targetRefs, ref, gameState);
}

void removeGuardedTargetRef(s32 ref, GameState* gameState) {
	removeFromRefNodeList(&gameState->guardTargetRefs, ref, gameState);
}

void addGroundReference(Entity* top, Entity* ground, GameState* gameState, bool isLaserBaseToBeamReference = false) {
	ConsoleField* topMovementField = getMovementField(top);

	if(isNonHeavyTile(top) && !topMovementField) return;
	if(!affectedByGravity(top, topMovementField) && !isLaserBaseToBeamReference) return;

	top->jumpCount = 0;
	setFlags(top, EntityFlag_grounded);
	clearFlags(top, EntityFlag_jumped);
	top->timeSinceLastOnGround = 0;

	if(refNodeListContainsRef(ground->groundReferenceList, top->ref)) return;
	if(refNodeListContainsRef(top->groundReferenceList, ground->ref)) return;

	RefNode* append = refNode(gameState, top->ref);

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

bool createsPickupFieldsOnDeath(Entity* entity) {
	bool result = true;

	if(entity->type == EntityType_player ||
	   entity->type == EntityType_pickupField) result = false;

	return result;
}

Entity* addPickupField(GameState*, Entity*, ConsoleField*);
Entity* addDeath(GameState*, V2, V2, DrawOrder, double, CharacterAnim*);
Entity* addMotherShipProjectileDeath(GameState* gameState, Entity* motherShipProjectile);

void freeEntityAtLevelEnd(Entity* entity, GameState* gameState, bool loadingFromCheckpoint) {
	Messages* messages = entity->messages;
	if(messages) {
		for (s32 messageIndex = 0; messageIndex < messages->count; messageIndex++) {
			freeTexture(messages->textures + messageIndex);
		}
	}

	if(entity->checkPointSave && !loadingFromCheckpoint) {

	}
}

void freeHitboxes(Entity* entity, GameState* gameState) {
	Hitbox* box = entity->hitboxes;

	if(box) {
		while(box->next) {
			box = box->next;
		}

		box->next = gameState->hitboxFreeList;
		gameState->hitboxFreeList = entity->hitboxes;
		entity->hitboxes = NULL;
	}
}

void toggleCheckpoint(Entity*, GameState*, bool);

void addToCheckPointSaveList(GameState* gameState, CheckPointSave* save) {
	assert(save->next == NULL);
	save->next = gameState->checkPointSaveList;
	gameState->checkPointSaveList = save;
}

void freeCheckPointSave(GameState* gameState, CheckPointSave* save) {
	if(save) {
		save->ref = 0;
	}
}

CheckPointSave* allocateCheckPointSave(GameState* gameState, s32 ref) {
	CheckPointSave* prevSave = NULL;
	CheckPointSave* save = NULL;

	for(CheckPointSave* c = gameState->checkPointSaveList; c; c = c->next) {
		if(!c->arena || !getEntityByRef(gameState, c->ref)) {
			save = c;
		}

		if(c->arena) {
			break;
		}

		prevSave = c;
	}

	if(save) {
		if(prevSave) {
			prevSave->next = save->next;
		}
		else {
			gameState->checkPointSaveList = save->next;
		}

		save->next = NULL;
	}
	else {
		save = pushStruct(&gameState->checkPointStorage, CheckPointSave);
		save->arena = NULL;
		save->next = NULL;
	}

	save->ref = ref;
	if(!save->arena) {
		save->arena = subArena(&gameState->checkPointStorage, MEGABYTES(2));
	}

	addToCheckPointSaveList(gameState, save);

	return save;
}

void freeEntityDuringLevel(Entity* entity, GameState* gameState, bool createdThisFrame = false) {
	if(isSet(entity, EntityFlag_checkPointReached)) {
		toggleCheckpoint(entity, gameState, true);
	}

	freeCheckPointSave(gameState, entity->checkPointSave);
	entity->checkPointSave = NULL;

	Messages* messages = entity->messages;
	if(messages) {
		for (s32 messageIndex = 0; messageIndex < messages->count; messageIndex++) {
			freeTexture(messages->textures + messageIndex);
		}

		freeMessages(messages, gameState);
	}

	//NOTE: At the end of the level no pickup fields should be created and no memory needs to be free
	//		because the level storage will just be reset. 
	if(!createdThisFrame) {
		s32 startFieldIndex = entity->type == EntityType_pickupField ? 1 : 0;
		for(s32 fieldIndex = startFieldIndex; fieldIndex < entity->numFields; fieldIndex++) {
			ConsoleField* field = entity->fields[fieldIndex];

			if(field->type == ConsoleField_givesEnergy) {
				gameState->fieldSpec.hackEnergy += field->children[0]->selectedIndex;
			}
		}
	}

	if(gameState->fieldSpec.hackAbilities.moveFields && createsPickupFieldsOnDeath(entity) && !createdThisFrame) {
		for (s32 fieldIndex = 0; fieldIndex < entity->numFields; fieldIndex++) {
			ConsoleField* field = entity->fields[fieldIndex];

			if(field) {
				bool createField = !hasValues(field);
				createField &= field->type != ConsoleField_givesEnergy;
				//createField &= !(entity->type == EntityType_hackEnergy && field->type == ConsoleField_bobsVertically);

				if(createField) {
					Entity* pickupField = addPickupField(gameState, entity, field);

					pickupField->dP.x = (fieldIndex / 2 + 1) * 30;
					pickupField->dP.y = (fieldIndex / 2 + 1) * 4;

					if(fieldIndex % 2 == 0) pickupField->dP.x *= -1;
				} else {
					freeConsoleField(entity->fields + fieldIndex, gameState);
				}

				entity->fields[fieldIndex] = NULL;
			}
		}
	} else {
		for (s32 fieldIndex = 0; fieldIndex < entity->numFields; fieldIndex++) {
			freeConsoleField(entity->fields + fieldIndex, gameState);
		}
	}

	if(entity->groundReferenceList) {
		freeRefNode(entity->groundReferenceList, gameState);
		entity->groundReferenceList = NULL;
	}

	if(entity->ignorePenetrationList) {
		freeRefNode(entity->ignorePenetrationList, gameState);
		entity->ignorePenetrationList = NULL;
	}

	freeHitboxes(entity, gameState);

	entity->numFields = 0;
	entity->spawnerRef = 0;
	entity->cloakFactor = 0;

	if(!createdThisFrame) {
		if(entity->type == EntityType_death) {
			if(entity->drawOrder == DrawOrder_player) {
				gameState->reloadCurrentLevel = true;
			}
		}
		else if(entity->type == EntityType_motherShipProjectile) {
			addMotherShipProjectileDeath(gameState, entity);
		} 
		else {
			CharacterAnim* charAnim = entity->characterAnim;

			if(charAnim) {
				AnimNode* deathAnim = charAnim->death;

				if(deathAnim) {
					addDeath(gameState, entity->p, entity->renderSize, entity->drawOrder, entity->rotation, entity->characterAnim);
				}
			}
		}
	}
}

void removeEntities(GameState* gameState) {

	//NOTE: No removes are allowed while in console mode because this would break updating
	//		entities with their old saved values. 
	if(getEntityByRef(gameState, gameState->consoleEntityRef)) return;

	//NOTE: Entities are removed here if their remove flag is set
	//NOTE: There is a memory leak here if the entity allocated anything
	//		Ensure this is cleaned up, the console is a big place where leaks can happen
	for (s32 entityIndex = 0; entityIndex < gameState->numEntities; entityIndex++) {
		Entity* entity = gameState->entities + entityIndex;

		if (isSet(entity, EntityFlag_remove)) {
			EntityReference* bucket = getEntityReferenceBucket(gameState, entity->ref);
			EntityReference* previousBucket = NULL;

			while (bucket) {
				assert(bucket->entity);

				if (bucket->entity && bucket->entity->ref == entity->ref) {
					break;
				}

				previousBucket = bucket;
				bucket = bucket->next;
			}

			assert(bucket);

			if(bucket) {
				if(previousBucket) {
					bucket->entity = NULL;

					previousBucket->next = bucket->next;
					bucket->next = gameState->entityRefFreeList;
					gameState->entityRefFreeList = bucket;
				} else if(bucket->next) {
					*bucket = *bucket->next;
				} else {
					bucket->entity = NULL;
				}
			}

			freeEntityDuringLevel(entity, gameState);

			gameState->numEntities--;

			if(entityIndex != gameState->numEntities) {
				Entity* dst = gameState->entities + entityIndex;
				Entity* src = gameState->entities + gameState->numEntities;
				*dst = *src;
				EntityReference* dstBucket = getPreciseEntityReferenceBucket(gameState, dst->ref);
				assert(dstBucket);
				dstBucket->entity = dst;
			}

			entityIndex--;

			gameState->entities[gameState->numEntities] = {};
		}
	}
}

EntityReference* allocateEntityReference(GameState* gameState, Entity* entity, EntityReference* next = NULL) {
	EntityReference* result = NULL;

	if (gameState->entityRefFreeList) {
		result = gameState->entityRefFreeList;
		gameState->entityRefFreeList = gameState->entityRefFreeList->next;
	} else {
		result = pushStruct(&gameState->levelStorage, EntityReference);
	}

	result->entity = entity;
	result->next = next;

	return result;
}

void initEntityReference(Entity* entity, GameState* gameState) {
	EntityReference* bucket = getEntityReferenceBucket(gameState, entity->ref);

	if(bucket->entity) bucket->next = allocateEntityReference(gameState, entity, bucket->next);
	else bucket->entity = entity;
}

Entity* addEntity(GameState* gameState, s32 ref, s32 entityIndex) {
	assert(entityIndex >= 0);	
	assert(entityIndex < arrayCount(gameState->entities));

	Entity* result = gameState->entities + entityIndex;
	*result = {};

	result->alpha = 1;
	result->ref = ref;
	initEntityReference(result, gameState);

	return result;
}

Entity* addEntity(GameState* gameState, EntityType type, DrawOrder drawOrder, V2 p, V2 renderSize) {
	assert(gameState->numEntities < arrayCount(gameState->entities));

	Entity* result = gameState->entities + gameState->numEntities;

	*result = {};
	result->ref = gameState->refCount_++;
	result->type = type;
	result->drawOrder = drawOrder;
	result->p = p;
	result->renderSize = renderSize;
	result->alpha = 1;

	initEntityReference(result, gameState);

	gameState->numEntities++;

	addToSpatialPartition(result, gameState);

	return result;
}

void removeField(ConsoleField** fields, s32 numFields, s32 fieldIndex) {
	assert(fieldIndex >= 0 && fieldIndex < numFields);

	clearFlags(fields[fieldIndex], ConsoleFlag_remove);

	for (s32 moveIndex = fieldIndex; moveIndex < numFields - 1; moveIndex++) {
		ConsoleField** dst = fields + moveIndex;
		ConsoleField** src = fields + moveIndex + 1;

		*dst = *src;
	}

	fields[numFields - 1] = NULL;
}

void removeFieldsIfSet(ConsoleField** fields, s32* numFields) {
	for (s32 fieldIndex = 0; fieldIndex < *numFields; fieldIndex++) {
		ConsoleField* field = fields[fieldIndex];

		if (field->numChildren) {
			removeFieldsIfSet(field->children, &field->numChildren);
		}

		if (isSet(field, ConsoleFlag_remove)) {
			removeField(fields, *numFields, fieldIndex);
			*numFields = *numFields - 1;
		}
	}
}

void addField(Entity* entity, ConsoleField* field) {
	entity->fields[entity->numFields] = field;
	entity->numFields++;
	assert(arrayCount(entity->fields) > entity->numFields);
}

void addDeferredField(Entity* entity, ConsoleField* field, GameState* gameState) {
	bool add = true;
	s32 fieldIndex = 0;


	if(isConsoleFieldMovementType(field)) {
		if(getMovementField(entity, &fieldIndex) != NULL) {
			add = false;
		}
	}
	else if(canOnlyHaveOneFieldOfType(field->type)) {
		if(getField(entity, field->type, &fieldIndex) != NULL) {
			add = false;
		}
	}

	if(add) {
		addField(entity, field);
	} else {
		freeConsoleField(entity->fields + fieldIndex, gameState);
		entity->fields[fieldIndex] = field;
	}
}

void addChildToConsoleField(ConsoleField* parent, ConsoleField* child) {
	parent->children[parent->numChildren] = child;
	parent->numChildren++;
	assert(parent->numChildren <= arrayCount(parent->children));
}

void addKeyboardField(Entity* entity, GameState* gameState) {
	ConsoleField* result = createConsoleField(gameState, (char*)"keyboard_controlled", ConsoleField_keyboardControlled, 5);

	double keyboardSpeedFieldValues[] = {25, 40, 50, 60, 75}; 
	double keyboardJumpHeightFieldValues[] = {2, 3.5, 5, 6.5, 8}; 

	addChildToConsoleField(result, createPrimitiveField(double, gameState, (char*)"speed", keyboardSpeedFieldValues,
												   arrayCount(keyboardSpeedFieldValues), 2, 3));
	addChildToConsoleField(result, createPrimitiveField(double, gameState, (char*)"jump_height", keyboardJumpHeightFieldValues,
													    arrayCount(keyboardJumpHeightFieldValues), 2, 4));
	addChildToConsoleField(result, createBoolField(gameState, (char*)"double_jump", false, 8));

	addField(entity, result);
}

void addBodyguardField(Entity* entity, GameState* gameState) {
	ConsoleField* result = createConsoleField(gameState, (char*)"bodyguard", ConsoleField_bodyguard, 10);

	double speedFieldValues[] = {0, 2, 4, 6, 8}; 

	addChildToConsoleField(result, createPrimitiveField(double, gameState, (char*)"speed", speedFieldValues,
												   arrayCount(speedFieldValues), 2, 1));

	entity->startPos = INVALID_START_POS;

	addField(entity, result);
}

void addCrushesEntitiesField(Entity* entity, GameState* gameState) {
	ConsoleField* result = createConsoleField(gameState, (char*)"crushes_entities", ConsoleField_crushesEntities, 20);
	addField(entity, result);
}

void addDisappearsField(Entity* entity, GameState* gameState) {
	ConsoleField* result = createConsoleField(gameState, (char*)"disappears_on_hit", ConsoleField_disappearsOnHit, 10);

	double time[] = {0.1, 0.25, 0.5, 1, 1.5}; 

	addChildToConsoleField(result, createPrimitiveField(double, gameState, (char*)"disappear_time", time,
												   arrayCount(time), 2, 1));

	addField(entity, result);
}

void addDroppingField(Entity* entity, GameState* gameState) {
	ConsoleField* result = createConsoleField(gameState, (char*)"drops_on_hit", ConsoleField_dropsOnHit, 10);

	addField(entity, result);
}

void addCloaksField(Entity* entity, GameState* gameState) {
	ConsoleField* result = createConsoleField(gameState, (char*)"cloaks", ConsoleField_cloaks, 10);

	double cloakTime[] = {0.1, 0.5, 1, 1.5, 2}; 
	double uncloakTime[] = {0.1, 0.5, 1, 1.5, 2}; 

	addChildToConsoleField(result, createPrimitiveField(double, gameState, (char*)"cloak_time", cloakTime,
												   arrayCount(cloakTime), 2, 1));

	addChildToConsoleField(result, createPrimitiveField(double, gameState, (char*)"uncloak_time", uncloakTime,
											   arrayCount(uncloakTime), 2, 1));

	addField(entity, result);
}

void addBobsVerticallyField(Entity* entity, GameState* gameState) {
	ConsoleField* result = createConsoleField(gameState, (char*)"bobs_vertically", ConsoleField_bobsVertically, 4);

	double speedFieldValues[] = {0, 1.5, 3, 4.5, 6}; 
	double maxBobHeight[] = {0, 0.5, 1, 1.5, 2}; 

	addChildToConsoleField(result, createPrimitiveField(double, gameState, (char*)"speed", speedFieldValues,
												   arrayCount(speedFieldValues), 2, 1));
	addChildToConsoleField(result, createPrimitiveField(double, gameState, (char*)"max_bob_height", maxBobHeight,
													    arrayCount(maxBobHeight), 2, 1));

	result->bobbingUp = true;
	result->initialBob = true;

	addField(entity, result);
}

void addPatrolField(Entity* entity, GameState* gameState) {
	ConsoleField* result = createConsoleField(gameState, (char*)"patrols", ConsoleField_movesBackAndForth, 3);

	double patrolSpeedFieldValues[] = {0, 20, 40, 60, 80}; 
	
	addChildToConsoleField(result, createPrimitiveField(double, gameState, (char*)"speed", patrolSpeedFieldValues,
	 											   arrayCount(patrolSpeedFieldValues), 2, 1));

	//addChildToConsoleField(result, createEnumField(Alertness, gameState, "alertness", Alertness_patrolling, 5));

	addField(entity, result);
}

void addScansForTargetsField(Entity* entity, GameState* gameState) {
	ConsoleField* result = createConsoleField(gameState, (char*)"scans_for_targets", ConsoleField_scansForTargets, 10);

	double scanSpeed[] = {0, 45, 90, 135, 180};
	s32 scanFOV[] = {0, 45, 90, 135, 180, 225, 270, 315, 360}; 
	
	addChildToConsoleField(result, createPrimitiveField(double, gameState, (char*)"scan_speed", scanSpeed,
	 											   arrayCount(scanSpeed), 2, 1));

	addChildToConsoleField(result, createPrimitiveField(s32, gameState, (char*)"swivel_extent", scanFOV,
 											   arrayCount(scanFOV), 3, 1));

	result->scanStart = entity->spotLightAngle;

	addField(entity, result);
}

void addSeekTargetField(Entity* entity, GameState* gameState) {
	ConsoleField* result = createConsoleField(gameState, (char*)"seeks_target", ConsoleField_seeksTarget, 3);

	double seekTargetSpeedFieldValues[] = {5, 10, 15, 20, 25}; 
	double seekTargetRadiusFieldValues[] = {4, 8, 12, 16, 20}; 
	
	addChildToConsoleField(result, createPrimitiveField(double, gameState, (char*)"speed", seekTargetSpeedFieldValues,
	 												    arrayCount(seekTargetSpeedFieldValues), 2, 1));
	addChildToConsoleField(result, createPrimitiveField(double, gameState, (char*)"sight_radius", seekTargetRadiusFieldValues,
													    arrayCount(seekTargetRadiusFieldValues), 2, 2));
	//addChildToConsoleField(result, createEnumField(Alertness, gameState, "alertness", Alertness_patrolling, 5));

	addField(entity, result);
}

Waypoint* createWaypoint(GameState* gameState, V2 p, Waypoint* next = NULL) {
	Waypoint* result = pushStruct(&gameState->levelStorage, Waypoint);
	*result = {};
	result->p = p;
	result->next = next;
	return result;
}

ConsoleField* addFollowsWaypointsField(Entity* entity, GameState* gameState) {
	ConsoleField* result = createConsoleField(gameState, (char*)"follows_waypoints", ConsoleField_followsWaypoints, 3);

	double followsWaypointsSpeedFieldValues[] = {1, 2, 3, 4, 5}; 
	double waypointDelays[] = {0, 0.5, 1, 1.5, 2};
	
	addChildToConsoleField(result, createPrimitiveField(double, gameState, (char*)"speed", followsWaypointsSpeedFieldValues,
	 												    arrayCount(followsWaypointsSpeedFieldValues), 2, 1));
	//addChildToConsoleField(result, createEnumField(Alertness, gameState, "alertness", Alertness_patrolling, 5));
	addChildToConsoleField(result, createPrimitiveField(double, gameState, (char*)"waypoint_delay", waypointDelays,
	 												    arrayCount(waypointDelays), 2, 1));

	result->curWaypoint = NULL;
	result->waypointDelay = 0;

	addField(entity, result);

	return result;
}

void addGivesEnergyField(Entity* entity, GameState* gameState) {
	ConsoleField* result = createConsoleField(gameState, (char*)"gives_energy", ConsoleField_givesEnergy, 1);

	addChildToConsoleField(result, createUnlimitedIntField(gameState, (char*)"amount", 5, 1));

	addField(entity, result);
}

void addSpawnsTrawlersField(Entity* entity, GameState* gameState) {
	ConsoleField* result = createConsoleField(gameState, (char*)"spawns_trawlers", ConsoleField_spawnsTrawlers, 20);

	double spawnDelays[] = {0.25, 0.5, 1, 2, 3};
	s32 spawnLimits[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};

	addChildToConsoleField(result, createPrimitiveField(double, gameState, (char*)"spawn_delay", spawnDelays,
 												    arrayCount(spawnDelays), 2, 4));

	addChildToConsoleField(result, createPrimitiveField(s32, gameState, (char*)"spawn_limit", spawnLimits,
												    arrayCount(spawnLimits), 3, 4));

	result->spawnTimer = getDoubleValue(result->children[0]);

	addField(entity, result);
}

void addSpawnsShrikesField(Entity* entity, GameState* gameState) {
	ConsoleField* result = createConsoleField(gameState, (char*)"spawns_shrikes", ConsoleField_spawnsShrikes, 20);

	double spawnDelays[] = {0.25, 0.5, 1, 2, 3};
	s32 spawnLimits[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};

	addChildToConsoleField(result, createPrimitiveField(double, gameState, (char*)"spawn_delay", spawnDelays,
 												    arrayCount(spawnDelays), 2, 4));

	addChildToConsoleField(result, createPrimitiveField(s32, gameState, (char*)"spawn_limit", spawnLimits,
												    arrayCount(spawnLimits), 3, 4));

	result->spawnTimer = getDoubleValue(result->children[0]);

	addField(entity, result);
}

void addShootField(Entity* entity, GameState* gameState, double speedModifier, EntityType type) {
	ConsoleField* result = createConsoleField(gameState, (char*)"shoots_at_target", ConsoleField_shootsAtTarget, 4);

	double bulletSpeedFieldValues[] = {2, 4, 6, 8, 10}; 
	double sightRadii[] = {1, 2, 3, 4, 5};

	for(s32 i = 0; i < arrayCount(bulletSpeedFieldValues); i++) {
		bulletSpeedFieldValues[i] *= speedModifier;
	}

	addChildToConsoleField(result, createPrimitiveField(double, gameState, (char*)"bullet_speed", bulletSpeedFieldValues,
													    arrayCount(bulletSpeedFieldValues), 2, 1));

	addChildToConsoleField(result, createPrimitiveField(double, gameState, (char*)"detect_radius", sightRadii,
													    arrayCount(sightRadii), 2, 1));

	result->shootEntityType = type;

	addField(entity, result);
}

void addSpotlightField(Entity* entity, GameState* gameState) {
	ConsoleField* result = createConsoleField(gameState, (char*)"spotlight", ConsoleField_spotlight, 15);

	double sightRadii[] = {2, 5, 7, 9, 12};
	double fovs[] = {30, 45, 60, 75, 90};
	
	addChildToConsoleField(result, createPrimitiveField(double, gameState, (char*)"sight_radius", sightRadii,
													    arrayCount(sightRadii), 2, 2));
	addChildToConsoleField(result, createPrimitiveField(double, gameState, (char*)"field_of_view", fovs,
													    arrayCount(fovs), 2, 1));
	addField(entity, result);
}

void addLightField(Entity* entity, GameState* gameState, V3 color) {
	ConsoleField* result = createConsoleField(gameState, (char*)"light", ConsoleField_light, 3);

	result->lightColor = color;

	double radii[] = {1.5, 3, 4.5, 6, 7.5};
	
	addChildToConsoleField(result, createPrimitiveField(double, gameState, (char*)"radius", radii,
													    arrayCount(radii), 2, 2));

	addChildToConsoleField(result, createBoolField(gameState, (char*)"enabled", true, 4));

	addField(entity, result);
}

void addIsTargetField(Entity* entity, GameState* gameState) {
	ConsoleField* field = createConsoleField(gameState, (char*)"is_target", ConsoleField_isShootTarget, 60);

	addField(entity, field);
	addTargetRef(entity->ref, gameState);
}

void addGuardTargetField(Entity* entity, GameState* gameState) {
	ConsoleField* result = createConsoleField(gameState, (char*)"guarded", ConsoleField_guarded, 30);

	addField(entity, result);
	addGuardedTargetRef(entity->ref, gameState);
}

Entity* addDeath(GameState* gameState, V2 p, V2 renderSize, DrawOrder drawOrder, double rotation, CharacterAnim* anim) {
	Entity* result = addEntity(gameState, EntityType_death, drawOrder, p, renderSize);

	setFlags(result, EntityFlag_noMovementByDefault);

	result->characterAnim = anim;
	result->rotation = rotation;

	assert(anim);
	assert(anim->death);

	return result;
}

Entity* addPlayer(GameState* gameState, V2 p) {
	Entity* result = addEntity(gameState, EntityType_player, DrawOrder_player, p, v2(1, 1) * 2);

	assert(!getUnremovedEntityByRef(gameState, gameState->playerRef));
	gameState->playerRef = result->ref;

	double hitboxWidth = result->renderSize.x;
	double hitboxHeight = result->renderSize.y;
	double halfHitboxWidth = hitboxWidth * 0.5;
	double halfHitboxHeight = hitboxHeight * 0.5;
	Hitbox* hitbox = addHitbox(result, gameState);
	setHitboxSize(result, hitbox, hitboxWidth * 1, hitboxHeight * 1);
	hitbox->collisionPointsCount = 8;
	hitbox->originalCollisionPoints[0] = v2(-0.164931 * halfHitboxWidth, -0.950521 * halfHitboxHeight);
	hitbox->originalCollisionPoints[1] = v2(0.186632 * halfHitboxWidth, -0.950521 * halfHitboxHeight);
	hitbox->originalCollisionPoints[2] = v2(0.243056 * halfHitboxWidth, -0.850694 * halfHitboxHeight);
	hitbox->originalCollisionPoints[3] = v2(0.243056 * halfHitboxWidth, 0.438368 * halfHitboxHeight);
	hitbox->originalCollisionPoints[4] = v2(0.147569 * halfHitboxWidth, 0.863715 * halfHitboxHeight);
	hitbox->originalCollisionPoints[5] = v2(-0.143229 * halfHitboxWidth, 0.863715 * halfHitboxHeight);
	hitbox->originalCollisionPoints[6] = v2(-0.240000 * halfHitboxWidth, 0.438368 * halfHitboxHeight);
	hitbox->originalCollisionPoints[7] = v2(-0.240000 * halfHitboxWidth, -0.850694 * halfHitboxHeight);

	result->clickBox = rectCenterDiameter(v2(0, 0), v2(result->renderSize.x * 0.4, result->renderSize.y));

	setFlags(result, EntityFlag_hackable);

	addKeyboardField(result, gameState);
	
	addField(result, createConsoleField(gameState, (char*)"camera_followed", ConsoleField_cameraFollows, 2));
	addIsTargetField(result, gameState);

	result->characterAnim = gameState->playerAnim;

	return result;
}

Entity* addCheckPoint(GameState* gameState, V2 p) {
	Entity* result = addEntity(gameState, EntityType_checkPoint, DrawOrder_checkPoint, p, v2(0.59, 1.25));

	double hitboxWidth = result->renderSize.x;
	double hitboxHeight = result->renderSize.y;
	double halfHitboxWidth = hitboxWidth * 0.5;
	double halfHitboxHeight = hitboxHeight * 0.5;
	Hitbox* hitbox = addHitbox(result, gameState);
	setHitboxSize(result, hitbox, hitboxWidth * 1, hitboxHeight * 1);
	hitbox->collisionPointsCount = 13;
	hitbox->originalCollisionPoints[0] = v2(-0.799329 * halfHitboxWidth, -0.998264 * halfHitboxHeight);
	hitbox->originalCollisionPoints[1] = v2(0.319418 * halfHitboxWidth, -0.998264 * halfHitboxHeight);
	hitbox->originalCollisionPoints[2] = v2(0.310248 * halfHitboxWidth, -0.442708 * halfHitboxHeight);
	hitbox->originalCollisionPoints[3] = v2(0.777921 * halfHitboxWidth, -0.373264 * halfHitboxHeight);
	hitbox->originalCollisionPoints[4] = v2(0.970493 * halfHitboxWidth, -0.182292 * halfHitboxHeight);
	hitbox->originalCollisionPoints[5] = v2(0.988833 * halfHitboxWidth, 0.516493 * halfHitboxHeight);
	hitbox->originalCollisionPoints[6] = v2(0.622030 * halfHitboxWidth, 0.855035 * halfHitboxHeight);
	hitbox->originalCollisionPoints[7] = v2(0.044317 * halfHitboxWidth, 1 * halfHitboxHeight);
	hitbox->originalCollisionPoints[8] = v2(-0.643438 * halfHitboxWidth, 1 * halfHitboxHeight);
	hitbox->originalCollisionPoints[9] = v2(-0.863519 * halfHitboxWidth, 0.915799 * halfHitboxHeight);
	hitbox->originalCollisionPoints[10] = v2(-0.991900 * halfHitboxWidth, 0.755208 * halfHitboxHeight);
	hitbox->originalCollisionPoints[11] = v2(-0.982730 * halfHitboxWidth, -0.264757 * halfHitboxHeight);
	hitbox->originalCollisionPoints[12] = v2(-0.808499 * halfHitboxWidth, -0.421007 * halfHitboxHeight);

	result->clickBox = rectCenterDiameter(v2(0, 0), result->renderSize);

	setFlags(result, EntityFlag_hackable);

	result->defaultTex = gameState->checkPointUnreached;
	result->emissivity = 1;

	addField(result, createBoolField(gameState, (char*)"reached", false, 5));

	return result;
}

Texture* getStandTex(CharacterAnim* characterAnim, GameState* gameState) {
	assert(gameState);
	assert(characterAnim);
	AnimNode* standAnimNode = characterAnim->stand;
	assert(standAnimNode);
	Animation* standAnim = &standAnimNode->main;
	assert(standAnim->frames > 0);
	Texture* standTex = standAnim->frames + 0;
	assert(validTexture(standTex));
	return standTex;
}

Entity* addEndPortal(GameState* gameState, V2 p, bool loadSameLevel) {
	Entity* result = addEntity(gameState, EntityType_endPortal, DrawOrder_endPortal, p, v2(2, 2));

	double hitboxWidth = result->renderSize.x;
	double hitboxHeight = result->renderSize.y;
	double halfHitboxWidth = hitboxWidth * 0.5;
	double halfHitboxHeight = hitboxHeight * 0.5;
	Hitbox* hitbox = addHitbox(result, gameState);
	setHitboxSize(result, hitbox, hitboxWidth * 1, hitboxHeight * 1);
	hitbox->collisionPointsCount = 12;
	hitbox->originalCollisionPoints[0] = v2(-0.464410 * halfHitboxWidth, -0.937500 * halfHitboxHeight);
	hitbox->originalCollisionPoints[1] = v2(0.442708 * halfHitboxWidth, -0.937500 * halfHitboxHeight);
	hitbox->originalCollisionPoints[2] = v2(0.616319 * halfHitboxWidth, -0.694444 * halfHitboxHeight);
	hitbox->originalCollisionPoints[3] = v2(0.438368 * halfHitboxWidth, -0.394965 * halfHitboxHeight);
	hitbox->originalCollisionPoints[4] = v2(0.598958 * halfHitboxWidth, -0.073785 * halfHitboxHeight);
	hitbox->originalCollisionPoints[5] = v2(0.533854 * halfHitboxWidth, 0.282118 * halfHitboxHeight);
	hitbox->originalCollisionPoints[6] = v2(0.251736 * halfHitboxWidth, 0.538194 * halfHitboxHeight);
	hitbox->originalCollisionPoints[7] = v2(-0.130208 * halfHitboxWidth, 0.577257 * halfHitboxHeight);
	hitbox->originalCollisionPoints[8] = v2(-0.442708 * halfHitboxWidth, 0.394965 * halfHitboxHeight);
	hitbox->originalCollisionPoints[9] = v2(-0.603299 * halfHitboxWidth, -0.034722 * halfHitboxHeight);
	hitbox->originalCollisionPoints[10] = v2(-0.434028 * halfHitboxWidth, -0.399306 * halfHitboxHeight);
	hitbox->originalCollisionPoints[11] = v2(-0.594618 * halfHitboxWidth, -0.677083 * halfHitboxHeight);

	if(loadSameLevel) {
		result->clickBox = rectCenterDiameter(v2(0, 0), result->renderSize);
		setFlags(result, EntityFlag_hackable);
		addField(result, createBoolField(gameState, (char*)"load_same_level", loadSameLevel, 1));
	}

	result->defaultTex = gameState->endPortal;
	result->emissivity = 0.75f;

	return result;
}

Entity* addPickupField(GameState* gameState, Entity* parent, ConsoleField* field) {
	Entity* result = addEntity(gameState, EntityType_pickupField, DrawOrder_pickupField, parent->p, gameState->fieldSpec.fieldSize);

	giveEntityRectangularCollisionBounds(result, gameState, 0, 0, result->renderSize.x, result->renderSize.y);
	result->clickBox = rectCenterDiameter(v2(0, 0), result->renderSize);

	addField(result, field);
	addField(result, createBoolField(gameState, (char*)"solid", false, 1));
	addField(result, createBoolField(gameState, (char*)"can_pickup", false, 1));

	result->emissivity = 1.0f;
	setFlags(result, EntityFlag_removeWhenOutsideLevel|
					 EntityFlag_hackable);

	ignoreAllPenetratingEntities(result, gameState);

	return result;
}

Entity* addHackEnergy(GameState* gameState, V2 p) {
	Entity* result = addEntity(gameState, EntityType_hackEnergy, DrawOrder_hackEnergy, p, v2(1, 1) * 0.6);

	double hitboxWidth = result->renderSize.x;
	double hitboxHeight = result->renderSize.y;
	double halfHitboxWidth = hitboxWidth * 0.5;
	double halfHitboxHeight = hitboxHeight * 0.5;
	Hitbox* hitbox = addHitbox(result, gameState);
	setHitboxSize(result, hitbox, hitboxWidth * 1.2, hitboxHeight * 1.2);
	hitbox->collisionPointsCount = 8;
	hitbox->originalCollisionPoints[0] = v2(-0.004340 * halfHitboxWidth, -1.046007 * halfHitboxHeight);
	hitbox->originalCollisionPoints[1] = v2(0.763889 * halfHitboxWidth, -0.815972 * halfHitboxHeight);
	hitbox->originalCollisionPoints[2] = v2(1.063368 * halfHitboxWidth, -0.013021 * halfHitboxHeight);
	hitbox->originalCollisionPoints[3] = v2(0.802951 * halfHitboxWidth, 0.763889 * halfHitboxHeight);
	hitbox->originalCollisionPoints[4] = v2(-0.017361 * halfHitboxWidth, 1.059028 * halfHitboxHeight);
	hitbox->originalCollisionPoints[5] = v2(-0.907118 * halfHitboxWidth, 0.681424 * halfHitboxHeight);
	hitbox->originalCollisionPoints[6] = v2(-1.041667 * halfHitboxWidth, -0.351563 * halfHitboxHeight);
	hitbox->originalCollisionPoints[7] = v2(-0.638021 * halfHitboxWidth, -0.876736 * halfHitboxHeight);

	// giveEntityRectangularCollisionBounds(result, gameState, 0, 0, 
	// 									 result->renderSize.x, result->renderSize.y - 0.18);

	addGivesEnergyField(result, gameState);
	addBobsVerticallyField(result, gameState);

	setFlags(result, EntityFlag_hackable);

	result->clickBox = rectCenterDiameter(v2(0, 0), result->renderSize);

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

Hitbox getTileHitboxWithoutOverhang(Entity* entity) {
	Hitbox result = {};

	double offset = (TILE_HEIGHT_WITHOUT_OVERHANG_IN_METERS - TILE_HEIGHT_IN_METERS) * 0.5;
	result.collisionOffset = v2(0, offset);

	V2 tileSize = v2(TILE_WIDTH_IN_METERS, TILE_HEIGHT_WITHOUT_OVERHANG_IN_METERS);
	setHitboxSize(entity, &result, tileSize.x, tileSize.y);

	result.collisionPointsCount = 4;
	result.storedRotation = INVALID_STORED_HITBOX_ROTATION;

	V2 halfSize = tileSize * 0.5;

	result.originalCollisionPoints[0] = -halfSize;
	result.originalCollisionPoints[1] = v2(halfSize.x, -halfSize.y);
	result.originalCollisionPoints[2] = halfSize;
	result.originalCollisionPoints[3] = v2(-halfSize.x, halfSize.y);

	return result;
}

void initTile(Entity* tile, GameState* gameState, s32 tileIndex, bool32 flipX, bool32 flipY, bool ignoresGravity) {
	setFlags(tile, EntityFlag_hackable);

	addField(tile, createUnlimitedIntField(gameState, (char*)"x_offset", 0, 1));
	addField(tile, createUnlimitedIntField(gameState, (char*)"y_offset", 0, 1));

	if(ignoresGravity) {
		setFlags(tile, EntityFlag_noMovementByDefault);
	}

	if(flipX) {
		setFlags(tile, EntityFlag_facesLeft|EntityFlag_flipX);
	}

	if(flipY) {
		setFlags(tile, EntityFlag_flipY);
	}

	tile->glowingTex = gameState->tileAtlas + tileIndex;

	TileData* data = globalTileData + tileIndex;

	if(data->type == Tile_corner) {
		tile->renderSize = v2(TILE_WIDTH_IN_METERS, TILE_HEIGHT_IN_METERS);

		double hitboxWidth = tile->renderSize.x;
		double hitboxHeight = tile->renderSize.y;
		double halfHitboxWidth = hitboxWidth * 0.5;
		double halfHitboxHeight = hitboxHeight * 0.5;
		Hitbox* hitbox = addHitbox(tile, gameState);
		setHitboxSize(tile, hitbox, hitboxWidth * 1, hitboxHeight * 1);
		hitbox->collisionPointsCount = 4;
		hitbox->originalCollisionPoints[0] = v2(-1 * halfHitboxWidth, -1 * halfHitboxHeight);
		hitbox->originalCollisionPoints[1] = v2(1 * halfHitboxWidth, -1 * halfHitboxHeight);
		hitbox->originalCollisionPoints[2] = v2(1 * halfHitboxWidth, 1 * halfHitboxHeight);
		hitbox->originalCollisionPoints[3] = v2(-1 * halfHitboxWidth, -0.776910 * halfHitboxHeight);

		setFlags(tile, EntityFlag_isCornerTile);
	}
	else {
		if(data->tall) {
			tile->renderSize = v2(TILE_WIDTH_IN_METERS, TILE_HEIGHT_IN_METERS);
		} else {
			tile->renderSize = v2(TILE_WIDTH_IN_METERS, TILE_HEIGHT_WITHOUT_OVERHANG_IN_METERS);
		}

		giveEntityRectangularCollisionBounds(tile, gameState, 0, 0, tile->renderSize.x,  tile->renderSize.y);
	}

	tile->clickBox = rectCenterDiameter(v2(0, 0), tile->renderSize);
}

Entity* addTile(GameState* gameState, V2 p, s32 tileIndex, bool32 flipX, bool32 flipY) {
	Entity* result = addEntity(gameState, EntityType_tile, DrawOrder_tile, 
								p, v2(0, 0));
	initTile(result, gameState, tileIndex, flipX, flipY, true);

	setFlags(result, EntityFlag_removeWhenOutsideLevel);
	return result;
}

Entity* addHeavyTile(GameState* gameState, V2 p, bool32 flipX, bool32 flipY) {
	Entity* result = addEntity(gameState, EntityType_heavyTile, DrawOrder_heavyTile, 
								p, v2(0, 0));

	initTile(result, gameState, Tile_heavy, flipX, flipY, false);
	addCrushesEntitiesField(result, gameState);

	return result;
}

Entity* addDisappearingTile(GameState* gameState, V2 p, bool32 flipX, bool32 flipY) {
	Entity* result = addEntity(gameState, EntityType_disappearingTile, DrawOrder_tile, 
								p, v2(0, 0));

	setFlags(result, EntityFlag_removeWhenOutsideLevel);

	initTile(result, gameState, Tile_disappear, flipX, flipY, true);
	addDisappearsField(result, gameState);

	return result;
}

Entity* addDroppingTile(GameState* gameState, V2 p, bool32 flipX, bool32 flipY) {
	InvalidCodePath;

	Entity* result = addEntity(gameState, EntityType_droppingTile, DrawOrder_tile, 
								p, v2(0, 0));

	setFlags(result, EntityFlag_removeWhenOutsideLevel);

	initTile(result, gameState, Tile_middle, flipX, flipY, true);
	addDroppingField(result, gameState);

	return result;
}

void setSelectedText(Entity* text, s32 selectedIndex, GameState* gameState) {
	assert(text->type == EntityType_text);

	Messages* messages = text->messages;
	assert(messages);

	messages->selectedIndex = selectedIndex;

	Texture* texture = messages->textures + selectedIndex;

	if(!texture->texId) {
		*texture = createText(gameState->renderGroup, gameState->textFont, messages->text[selectedIndex]);
	}

	text->renderSize = texture->size;
	text->clickBox = rectCenterDiameter(v2(0, 0), text->renderSize);

	freeHitboxes(text, gameState);
	giveEntityRectangularCollisionBounds(text, gameState, 0, 0, text->renderSize.x, text->renderSize.y);
}

Entity* addText(GameState* gameState, V2 p, Messages* messages) {
	Entity* result = addEntity(gameState, EntityType_text, DrawOrder_text, p, v2(0, 0));

	result->messages = messages;
	setSelectedText(result, messages->selectedIndex, gameState);

	setFlags(result, EntityFlag_noMovementByDefault|
					 EntityFlag_hackable);

	s32 selectedIndexValues[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
	addField(result, createPrimitiveField(s32, gameState, (char*)"selected_index", selectedIndexValues, messages->count,
										 messages->selectedIndex, 1));


	return result;
}

void initProjectile(Entity* entity, V2 target, s32 shooterRef, double speed, Texture* texture, GameState* gameState) {
	entity->clickBox = rectCenterDiameter(v2(0, 0), entity->renderSize);

	entity->dP = normalize(target - entity->p) * speed;
	entity->spawnerRef = shooterRef;

	if(entity->type == EntityType_trojanBolt ||
	   entity->type == EntityType_trawlerBolt) {
		entity->rotation = getRad(entity->dP) + PI;
	}

	setFlags(entity, EntityFlag_removeWhenOutsideLevel|
					 EntityFlag_hackable);

	addField(entity, createConsoleField(gameState, (char*)"kills_enemies", ConsoleField_killsOnHit, 5));

	entity->defaultTex = texture;
}

void addTrojanBoltHitbox(Entity* result, GameState* gameState) {
	double hitboxWidth = result->renderSize.x;
	double hitboxHeight = result->renderSize.y;
	double halfHitboxWidth = hitboxWidth * 0.5;
	double halfHitboxHeight = hitboxHeight * 0.5;
	Hitbox* hitbox = addHitbox(result, gameState);
	setHitboxSize(result, hitbox, hitboxWidth * 1, hitboxHeight * 1);
	hitbox->collisionPointsCount = 8;
	hitbox->originalCollisionPoints[0] = v2(-0.516493 * halfHitboxWidth, -0.290799 * halfHitboxHeight);
	hitbox->originalCollisionPoints[1] = v2(0.503472 * halfHitboxWidth, -0.277778 * halfHitboxHeight);
	hitbox->originalCollisionPoints[2] = v2(0.711806 * halfHitboxWidth, -0.138889 * halfHitboxHeight);
	hitbox->originalCollisionPoints[3] = v2(0.707465 * halfHitboxWidth, 0.117187 * halfHitboxHeight);
	hitbox->originalCollisionPoints[4] = v2(0.499132 * halfHitboxWidth, 0.277778 * halfHitboxHeight);
	hitbox->originalCollisionPoints[5] = v2(-0.507813 * halfHitboxWidth, 0.286458 * halfHitboxHeight);
	hitbox->originalCollisionPoints[6] = v2(-0.737847 * halfHitboxWidth, 0.112847 * halfHitboxHeight);
	hitbox->originalCollisionPoints[7] = v2(-0.733507 * halfHitboxWidth, -0.147569 * halfHitboxHeight);
}

Entity* addTrojanBolt(GameState* gameState, V2 p, V2 target, s32 shooterRef, double speed) {
	Entity* result = addEntity(gameState, EntityType_trojanBolt, DrawOrder_trojanBolt, p, v2(1, 1) * TROJAN_BOLT_SIZE);

	addTrojanBoltHitbox(result, gameState);

	initProjectile(result, target, shooterRef, speed, gameState->trojanImages.projectile, gameState);

	result->emissivity = 1;
	result->characterAnim = gameState->trojanImages.projectileDeath;
					 
	return result;
}

void addTrawlerBoltHitbox(Entity* result, GameState* gameState) {
	double hitboxWidth = result->renderSize.x;
	double hitboxHeight = result->renderSize.y;
	double halfHitboxWidth = hitboxWidth * 0.5;
	double halfHitboxHeight = hitboxHeight * 0.5;
	Hitbox* hitbox = addHitbox(result, gameState);
	setHitboxSize(result, hitbox, hitboxWidth * 1, hitboxHeight * 1);
	hitbox->collisionPointsCount = 8;
	hitbox->originalCollisionPoints[0] = v2(-0.499132 * halfHitboxWidth, -0.173611 * halfHitboxHeight);
	hitbox->originalCollisionPoints[1] = v2(0.516493 * halfHitboxWidth, -0.169271 * halfHitboxHeight);
	hitbox->originalCollisionPoints[2] = v2(0.668403 * halfHitboxWidth, -0.078125 * halfHitboxHeight);
	hitbox->originalCollisionPoints[3] = v2(0.668403 * halfHitboxWidth, 0.060764 * halfHitboxHeight);
	hitbox->originalCollisionPoints[4] = v2(0.512153 * halfHitboxWidth, 0.160590 * halfHitboxHeight);
	hitbox->originalCollisionPoints[5] = v2(-0.512153 * halfHitboxWidth, 0.186632 * halfHitboxHeight);
	hitbox->originalCollisionPoints[6] = v2(-0.655382 * halfHitboxWidth, 0.060764 * halfHitboxHeight);
	hitbox->originalCollisionPoints[7] = v2(-0.655382 * halfHitboxWidth, -0.108507 * halfHitboxHeight);
}

	

Entity* addTrawlerBolt(GameState* gameState, V2 p, V2 target, s32 shooterRef, double speed, double size) {
	Entity* result = addEntity(gameState, EntityType_trawlerBolt, DrawOrder_trawlerBolt, p, v2(1, 1) * size);

	addTrawlerBoltHitbox(result, gameState);

	initProjectile(result, target, shooterRef, speed, gameState->trawlerImages.projectile, gameState);

	result->emissivity = 1;
	result->characterAnim = gameState->trawlerImages.projectileDeath;
					 
	return result;
}

void addMotherShipProjectileHitbox(Entity* result, GameState* gameState) {
	double hitboxWidth = result->renderSize.x;
	double hitboxHeight = result->renderSize.y;
	double halfHitboxWidth = hitboxWidth * 0.5;
	double halfHitboxHeight = hitboxHeight * 0.5;
	Hitbox* hitbox = addHitbox(result, gameState);
	setHitboxSize(result, hitbox, hitboxWidth * 1, hitboxHeight * 1);
	hitbox->collisionPointsCount = 9;
	hitbox->originalCollisionPoints[0] = v2(-0.117188 * halfHitboxWidth, -0.486111 * halfHitboxHeight);
	hitbox->originalCollisionPoints[1] = v2(0.199653 * halfHitboxWidth, -0.447049 * halfHitboxHeight);
	hitbox->originalCollisionPoints[2] = v2(0.464410 * halfHitboxWidth, -0.182292 * halfHitboxHeight);
	hitbox->originalCollisionPoints[3] = v2(0.490451 * halfHitboxWidth, 0.164931 * halfHitboxHeight);
	hitbox->originalCollisionPoints[4] = v2(0.260417 * halfHitboxWidth, 0.434028 * halfHitboxHeight);
	hitbox->originalCollisionPoints[5] = v2(-0.130208 * halfHitboxWidth, 0.494792 * halfHitboxHeight);
	hitbox->originalCollisionPoints[6] = v2(-0.421007 * halfHitboxWidth, 0.303819 * halfHitboxHeight);
	hitbox->originalCollisionPoints[7] = v2(-0.490451 * halfHitboxWidth, -0.034722 * halfHitboxHeight);
	hitbox->originalCollisionPoints[8] = v2(-0.373264 * halfHitboxWidth, -0.325521 * halfHitboxHeight);
}

Entity* addMotherShipProjectile(GameState* gameState, V2 p, V2 target, s32 shooterRef, double speed) {
	Entity* result = addEntity(gameState, EntityType_motherShipProjectile, DrawOrder_motherShipProjectile, p, MOTHERSHIP_PROJECTILE_SIZE * v2(1, 1));

	setFlags(result, EntityFlag_noMovementByDefault);

	addMotherShipProjectileHitbox(result, gameState);

	initProjectile(result, target, shooterRef, speed, NULL, gameState);

	return result;
}

Entity* addMotherShipProjectileDeath(GameState* gameState, Entity* motherShipProjectile) {
	Entity* result = addEntity(gameState, EntityType_motherShipProjectileDeath, DrawOrder_motherShipProjectile,
	 							motherShipProjectile->p, motherShipProjectile->renderSize);

	setFlags(result, EntityFlag_noMovementByDefault);

	Animation* anim = &gameState->motherShipImages.projectileMoving;
	double duration = getAnimationDuration(anim);
	double shorterDuration = duration - anim->secondsPerFrame;

	result->animTime = motherShipProjectile->animTime;
	result->rotation = motherShipProjectile->rotation;

	if(result->animTime > duration) {
		result->animTime -= duration;
		s32 numTimesPlayed = (s32)(result->animTime / shorterDuration);

		result->animTime -= numTimesPlayed * shorterDuration;

		if(numTimesPlayed % 2 == 0) {
			result->animTime = duration - result->animTime;
		}
	}
	
	return result;
}

Entity* addLaserBase_(GameState* gameState, V2 baseP, double height) {
	Entity* result = addEntity(gameState, EntityType_laserBase, DrawOrder_laserBase, baseP, v2(0.9, 0.65));

	addField(result, createBoolField(gameState, (char*)"enabled", true, 2));
	addPatrolField(result, gameState);

	giveEntityRectangularCollisionBounds(result, gameState, 0, 0, result->renderSize.x, result->renderSize.y);
	giveEntityRectangularCollisionBounds(result, gameState, 0, height, result->renderSize.x, result->renderSize.y);

	result->clickBox = rectCenterDiameter(v2(0, height / 2), v2(result->renderSize.x, height + result->renderSize.y));
	result->clickBox = addRadiusTo(result->clickBox, v2(0.1, 0.1));

	setFlags(result, EntityFlag_noMovementByDefault|
					 EntityFlag_laserOn|
					 EntityFlag_hackable);

	if(result->ref % 2) {
		setFlags(result, EntityFlag_facesLeft); 
	}

	result->emissivity = 0.1f;

	return result;
}

Entity* addLaserBeam_(GameState* gameState, V2 p, V2 renderSize) {
	renderSize.x *= 0.8;

	Entity* result = addEntity(gameState, EntityType_laserBeam, DrawOrder_laserBeam, p, renderSize);
	result->defaultTex = gameState->laserImages.beam;

	giveEntityRectangularCollisionBounds(result, gameState, 0, 0, result->renderSize.x, result->renderSize.y);

	setFlags(result, EntityFlag_noMovementByDefault|
					 EntityFlag_laserOn);

	result->emissivity = 0.25f;

	return result;
}

Entity* addLaserController(GameState* gameState, V2 baseP, double height) {
	//NOTE: The order which all the parts are added: base first then beam
	//		is necessary. This is so that all of the other pieces of the laser can be easily found if you just
	//		have one to start. This is mainly used for collision detection (eg. avoiding collisions between the base
	//		and the other laser pieces)

	Entity* base = addLaserBase_(gameState, baseP, height);
	//V2 topP = baseP + v2(0, height);
	// Entity* top = addLaserPiece_(gameState, topP, false);

	//V2 p = (baseP + topP) * 0.5;

	V2 laserSize = v2(base->renderSize.x, height - base->renderSize.y / 2);
	V2 laserP = base->p + v2(0, height / 2);

	Entity* beam = addLaserBeam_(gameState, laserP, laserSize);

	addGroundReference(beam, base, gameState, true);

	return base;
}

Entity* addTrojan(GameState* gameState, V2 p) {
	V2 size = v2(2, 2);
	Entity* result = addEntity(gameState, EntityType_trojan, DrawOrder_trojan, p, size);

	result->characterAnim = gameState->trojanImages.trojanAnim;

	setFlags(result, EntityFlag_hackable|
					 EntityFlag_noMovementByDefault);

	double hitboxWidth = result->renderSize.x;
	double hitboxHeight = result->renderSize.y;
	double halfHitboxWidth = hitboxWidth * 0.5;
	double halfHitboxHeight = hitboxHeight * 0.5;
	Hitbox* hitbox = addHitbox(result, gameState);
	setHitboxSize(result, hitbox, hitboxWidth * 1, hitboxHeight * 1);
	hitbox->collisionPointsCount = 12;
	hitbox->originalCollisionPoints[0] = v2(-0.091146 * halfHitboxWidth, -0.707465 * halfHitboxHeight);
	hitbox->originalCollisionPoints[1] = v2(0.073785 * halfHitboxWidth, -0.707465 * halfHitboxHeight);
	hitbox->originalCollisionPoints[2] = v2(0.711806 * halfHitboxWidth, -0.095486 * halfHitboxHeight);
	hitbox->originalCollisionPoints[3] = v2(0.698785 * halfHitboxWidth, 0.082465 * halfHitboxHeight);
	hitbox->originalCollisionPoints[4] = v2(0.447049 * halfHitboxWidth, 0.303819 * halfHitboxHeight);
	hitbox->originalCollisionPoints[5] = v2(0.377604 * halfHitboxWidth, 0.451389 * halfHitboxHeight);
	hitbox->originalCollisionPoints[6] = v2(0.099826 * halfHitboxWidth, 0.598958 * halfHitboxHeight);
	hitbox->originalCollisionPoints[7] = v2(-0.108507 * halfHitboxWidth, 0.603299 * halfHitboxHeight);
	hitbox->originalCollisionPoints[8] = v2(-0.373264 * halfHitboxWidth, 0.464410 * halfHitboxHeight);
	hitbox->originalCollisionPoints[9] = v2(-0.455729 * halfHitboxWidth, 0.299479 * halfHitboxHeight);
	hitbox->originalCollisionPoints[10] = v2(-0.690104 * halfHitboxWidth, 0.117187 * halfHitboxHeight);
	hitbox->originalCollisionPoints[11] = v2(-0.716146 * halfHitboxWidth, -0.065104 * halfHitboxHeight);

	result->clickBox = rectCenterDiameter(v2(0, 0), 0.7 * result->renderSize);

	addCloaksField(result, gameState);
	addShootField(result, gameState, 1, EntityType_trojan);
	addSpotlightField(result, gameState);

	return result;
}

Entity* addLamp(GameState* gameState, V2 p, double height, Texture* texture, V3 color) {
	Entity* result = addEntity(gameState, EntityType_lamp, DrawOrder_lamp, p, getDrawSize(texture, height));

	setFlags(result, EntityFlag_hackable);

	result->clickBox = rectCenterDiameter(v2(0, 0), result->renderSize);
	result->defaultTex = texture;

	addLightField(result, gameState, color);

	result->emissivity = 1;

	return result;
}

Entity* addMotherShip(GameState* gameState, V2 p) {
	V2 size = v2(1, 1) * 6;
	Entity* result = addEntity(gameState, EntityType_motherShip, DrawOrder_motherShip, p, size);

	setFlags(result, EntityFlag_hackable|
					 EntityFlag_noMovementByDefault);

	double hitboxWidth = result->renderSize.x;
	double hitboxHeight = result->renderSize.y;
	double halfHitboxWidth = hitboxWidth * 0.5;
	double halfHitboxHeight = hitboxHeight * 0.5;
	Hitbox* hitbox = addHitbox(result, gameState);
	setHitboxSize(result, hitbox, hitboxWidth * 1, hitboxHeight * 1);
	hitbox->collisionPointsCount = 10;
	hitbox->originalCollisionPoints[0] = v2(-0.056424 * halfHitboxWidth, -0.798611 * halfHitboxHeight);
	hitbox->originalCollisionPoints[1] = v2(0.047743 * halfHitboxWidth, -0.798611 * halfHitboxHeight);
	hitbox->originalCollisionPoints[2] = v2(0.373264 * halfHitboxWidth, -0.577257 * halfHitboxHeight);
	hitbox->originalCollisionPoints[3] = v2(0.529514 * halfHitboxWidth, -0.321181 * halfHitboxHeight);
	hitbox->originalCollisionPoints[4] = v2(0.581597 * halfHitboxWidth, -0.021701 * halfHitboxHeight);
	hitbox->originalCollisionPoints[5] = v2(0.590278 * halfHitboxWidth, 0.312500 * halfHitboxHeight);
	hitbox->originalCollisionPoints[6] = v2(0.073785 * halfHitboxWidth, 0.616319 * halfHitboxHeight);
	hitbox->originalCollisionPoints[7] = v2(-0.429688 * halfHitboxWidth, 0.490451 * halfHitboxHeight);
	hitbox->originalCollisionPoints[8] = v2(-0.594618 * halfHitboxWidth, -0.134549 * halfHitboxHeight);
	hitbox->originalCollisionPoints[9] = v2(-0.312500 * halfHitboxWidth, -0.551215 * halfHitboxHeight);

	result->clickBox = rectCenterDiameter(v2(0, 0), 0.7 * result->renderSize);

	result->spotLightAngle = -PI * 0.5;

	addShootField(result, gameState, 0.5, EntityType_motherShip);
	addSpotlightField(result, gameState);
	addScansForTargetsField(result, gameState);
	addSpawnsTrawlersField(result, gameState);
	addSpawnsShrikesField(result, gameState);
	addGuardTargetField(result, gameState);

	return result;
}

#define TRAWLER_SIZE (v2(1, 1) * 2)

Entity* addBootUp(GameState* gameState, V2 p, DrawOrder drawOrder, V2 size) {
	Entity* result = addEntity(gameState, EntityType_bootUp, drawOrder, p, size);

	setFlags(result, EntityFlag_noMovementByDefault|EntityFlag_hackable);
	result->clickBox = rectCenterDiameter(v2(0, 0), result->renderSize);

	return result;
}

void addTrawlerHitbox(Entity* entity, GameState* gameState) {
	double hitboxWidth = entity->renderSize.x;
	double hitboxHeight = entity->renderSize.y;
	double halfHitboxWidth = hitboxWidth * 0.5;
	double halfHitboxHeight = hitboxHeight * 0.5;
	Hitbox* hitbox = addHitbox(entity, gameState);
	setHitboxSize(entity, hitbox, hitboxWidth * 1, hitboxHeight * 1);
	hitbox->collisionPointsCount = 12;
	hitbox->originalCollisionPoints[0] = v2(-0.000000 * halfHitboxWidth, -0.776910 * halfHitboxHeight);
	hitbox->originalCollisionPoints[1] = v2(0.368924 * halfHitboxWidth, -0.694444 * halfHitboxHeight);
	hitbox->originalCollisionPoints[2] = v2(0.655382 * halfHitboxWidth, -0.434028 * halfHitboxHeight);
	hitbox->originalCollisionPoints[3] = v2(0.776910 * halfHitboxWidth, -0.008681 * halfHitboxHeight);
	hitbox->originalCollisionPoints[4] = v2(0.703125 * halfHitboxWidth, 0.342882 * halfHitboxHeight);
	hitbox->originalCollisionPoints[5] = v2(0.399306 * halfHitboxWidth, 0.677083 * halfHitboxHeight);
	hitbox->originalCollisionPoints[6] = v2(-0.000000 * halfHitboxWidth, 0.772569 * halfHitboxHeight);
	hitbox->originalCollisionPoints[7] = v2(-0.381944 * halfHitboxWidth, 0.677083 * halfHitboxHeight);
	hitbox->originalCollisionPoints[8] = v2(-0.694444 * halfHitboxWidth, 0.377604 * halfHitboxHeight);
	hitbox->originalCollisionPoints[9] = v2(-0.794271 * halfHitboxWidth, -0.008681 * halfHitboxHeight);
	hitbox->originalCollisionPoints[10] = v2(-0.681424 * halfHitboxWidth, -0.412326 * halfHitboxHeight);
	hitbox->originalCollisionPoints[11] = v2(-0.386285 * halfHitboxWidth, -0.698785 * halfHitboxHeight);
}

Entity* addTrawlerBootUp(GameState* gameState, V2 p) {
	Entity* result = addBootUp(gameState, p, DrawOrder_trawler, TRAWLER_SIZE);
	addTrawlerHitbox(result, gameState);
	return result;
}

void initTrawler(GameState* gameState, Entity* entity, V2 p) {
	entity->type = EntityType_trawler;
	entity->drawOrder = DrawOrder_trawler;
	setEntityP(entity, p, gameState);
	entity->renderSize = TRAWLER_SIZE;

	addTrawlerHitbox(entity, gameState);
	ignoreAllPenetratingEntities(entity, gameState);
}

Entity* addTrawler(GameState* gameState, V2 p) {
	Entity* result = addEntity(gameState, EntityType_trawler, DrawOrder_trawler, p, TRAWLER_SIZE);

	addTrawlerHitbox(result, gameState);

	setFlags(result, EntityFlag_hackable);
	if(result->ref % 2) setFlags(result, EntityFlag_facesLeft);

	result->clickBox = rectCenterDiameter(v2(0, 0), result->renderSize);

	addShootField(result, gameState, 1, EntityType_trawler);
	addSpotlightField(result, gameState);
	addPatrolField(result, gameState);

	ignoreAllPenetratingEntities(result, gameState);

	return result;
}

#define SHRIKE_SIZE (v2(1, 1) * 1.5)

void addShrikeHitbox(Entity* entity, GameState* gameState) {
	double hitboxWidth = entity->renderSize.x;
	double hitboxHeight = entity->renderSize.y;
	double halfHitboxWidth = hitboxWidth * 0.5;
	double halfHitboxHeight = hitboxHeight * 0.5;
	Hitbox* hitbox = addHitbox(entity, gameState);
	setHitboxSize(entity, hitbox, hitboxWidth * 1, hitboxHeight * 1);
	hitbox->collisionPointsCount = 9;
	hitbox->originalCollisionPoints[0] = v2(-0.933160 * halfHitboxWidth, -0.538194 * halfHitboxHeight);
	hitbox->originalCollisionPoints[1] = v2(-0.5551215 * halfHitboxWidth, -0.538194 * halfHitboxHeight);
	hitbox->originalCollisionPoints[2] = v2(-0.551215 * halfHitboxWidth, -0.212674 * halfHitboxHeight);
	hitbox->originalCollisionPoints[3] = v2(0.551215 * halfHitboxWidth, -0.195313 * halfHitboxHeight);
	hitbox->originalCollisionPoints[4] = v2(0.551215 * halfHitboxWidth, -0.538194 * halfHitboxHeight);
	hitbox->originalCollisionPoints[5] = v2(0.933160 * halfHitboxWidth, -0.538194 * halfHitboxHeight);
	hitbox->originalCollisionPoints[6] = v2(0.933160 * halfHitboxWidth, 0.039062 * halfHitboxHeight);
	hitbox->originalCollisionPoints[7] = v2(-0.00000 * halfHitboxWidth, 0.690104 * halfHitboxHeight);
	hitbox->originalCollisionPoints[8] = v2(-0.933160 * halfHitboxWidth, 0.030382 * halfHitboxHeight);
}

Entity* addShrikeBootUp(GameState* gameState, V2 p) {
	Entity* result = addBootUp(gameState, p, DrawOrder_shrike, SHRIKE_SIZE);
	addShrikeHitbox(result, gameState);
	return result;
}

Entity* addShrike(GameState* gameState, V2 p) {
	Entity* result = addEntity(gameState, EntityType_shrike, DrawOrder_shrike, p, SHRIKE_SIZE);

	setFlags(result, EntityFlag_hackable|
					 EntityFlag_noMovementByDefault);

	addShrikeHitbox(result, gameState);

	result->clickBox = rectCenterDiameter(v2(0, 0), result->renderSize);
	result->characterAnim = gameState->shrikeAnim;

	addShootField(result, gameState, 0.5, EntityType_shrike);
	addSpotlightField(result, gameState);
	addBodyguardField(result, gameState);

	ignoreAllPenetratingEntities(result, gameState);

	return result;
}

void initTestProjectile(Entity* result, Entity* shooter, GameState* gameState) {
	result->spawnerRef = shooter->ref;
	result->renderSize = v2(1, 1);

	switch(shooter->type) {
		case EntityType_trawler: {
			result->renderSize *= TRAWLER_BOLT_SIZE;
			result->type = EntityType_trawlerBolt;
			addTrawlerBoltHitbox(result, gameState);
		} break;

		case EntityType_shrike: {
			result->renderSize *= SHRIKE_BOLT_SIZE;
			result->type = EntityType_trawlerBolt;
			addTrawlerBoltHitbox(result, gameState);
		} break;

		case EntityType_trojan: {
			result->renderSize *= TROJAN_BOLT_SIZE;
			result->type = EntityType_trojanBolt;
			addTrojanBoltHitbox(result, gameState);
		} break;

		case EntityType_motherShip: {
			result->renderSize *= MOTHERSHIP_PROJECTILE_SIZE;
			result->type = EntityType_motherShipProjectile;
			addMotherShipProjectileHitbox(result, gameState);
		} break;

		InvalidDefaultCase;
	}
} 

ConsoleField* getMovementField(Entity* entity, s32* index) {
	ConsoleField* result = NULL;

	if(!isSet(entity, EntityFlag_remove)) {
		s32 firstFieldIndex = entity->type == EntityType_pickupField ? 1 : 0;
		for (s32 fieldIndex = firstFieldIndex; fieldIndex < entity->numFields; fieldIndex++) {
			ConsoleField* testField = entity->fields[fieldIndex];
			if (isConsoleFieldMovementType(testField)) {
				result = testField;
				if(index) *index = fieldIndex;
				break;
			}
		}
	}

	return result;
}

bool isLaserEntityType(Entity* entity) {
	bool result = entity->type == EntityType_laserBase ||
				  entity->type == EntityType_laserBeam;
	return result;
}

bool canPickupField(Entity* pickupField, Entity* collider, GameState* gameState) {
	assert(pickupField->type == EntityType_pickupField);

	bool result = false;

	if (!isSet(pickupField, EntityFlag_remove) && !gameState->swapField) {
		assert(pickupField->type == EntityType_pickupField);
		assert(pickupField->numFields >= 3);

		ConsoleField* field = pickupField->fields[2];
		bool32 canPickup = (field && field->selectedIndex);

		if(canPickup) {
			ConsoleField* field = getMovementField(collider);
			if(field && field->type == ConsoleField_keyboardControlled) result = true;
		}
	}

	return result;
}

bool32 isPickupFieldSolid(Entity* pickupField) {
	if(isSet(pickupField, EntityFlag_remove)) return false;

	assert(pickupField->type == EntityType_pickupField);
	assert(pickupField->numFields >= 2);

	ConsoleField* solidField = pickupField->fields[1];
	bool32 solid = (solidField && solidField->selectedIndex);

	return solid;
}

bool isMob(Entity* entity) {
	bool result = entity->type == EntityType_shrike ||
				  entity->type == EntityType_motherShip ||
				  entity->type == EntityType_trawler ||
				  entity->type == EntityType_trojan ||
				  entity->type == EntityType_player;

	return result;
}

bool collidesWithRaw(Entity* a, Entity* b, GameState* gameState, bool penetrationTest) {
	bool result = true;

	switch(a->type) {
		case EntityType_test:
		case EntityType_motherShipProjectileDeath:
		case EntityType_bootUp:
		case EntityType_death:
		case EntityType_background: {
			result = false;
		} break;

		case EntityType_text: {
			result = getMovementField(a) != NULL;
		} break;

		case EntityType_hackEnergy: {
			if (b->type != EntityType_player) result = false;
		} break;

		case EntityType_endPortal: {
			if (b->type != EntityType_player && 
				!isTileType(b)) result = false;
		} break;

		case EntityType_laserBeam: {
			//NOTE: These are the 2 other pieces of the laser (base, top)
			if (a->ref == b->ref + 1 || 
				!isSet(a, EntityFlag_laserOn)) result = false;
		} break;

		case EntityType_pickupField: {
			result = false;

			if(isPickupFieldSolid(a) ||
			   canPickupField(a, b, gameState) || 
			  (isNonHeavyTile(b) && getMovementField(b) == NULL)) result = true;

			if( b->type == EntityType_laserBeam || 
				isProjectile(b)) result = false;

			if(b->type == EntityType_pickupField && !penetrationTest) result = true;
		} break;

		case EntityType_checkPoint: {
			result = isTileType(b) || (b->type == EntityType_player && !isSet(a, EntityFlag_checkPointReached));
		} break;

		default:
			if(isTileType(a) && isTileType(b)) {
				result = getMovementField(a) || 
						 getMovementField(b) || 
						 affectedByGravity(a, NULL) || 
						 affectedByGravity(b, NULL);
			}
			else if(isProjectile(a)) {
				if (b->ref == a->spawnerRef) result = false;
				else {
					//NOTE: If a bullet is shot from a laser base/top, it should not collide with the laser beam
					//		This requires the base, top, and beam to all be added in a specific order to work
					Entity* shooter = getEntityByRef(gameState, a->spawnerRef);
					if(shooter && shooter->type == EntityType_laserBase && shooter->ref + 1 == b->ref)
					    result = false;
				}
			}
			else if(isMob(a) && isMob(b)) {
				result = false;
			}
		break;
	}

	

	return result;
}

bool isIgnoringPenetrationRaw(Entity* a, Entity* b) {
	bool result = refNodeListContainsRef(a->ignorePenetrationList, b->ref);
	return result;
}

bool isIgnoringPenetration(Entity* a, Entity* b) {
	bool result = isIgnoringPenetrationRaw(a, b) || isIgnoringPenetrationRaw(b, a);
	return result;
}

bool collidesWith(Entity* a, Entity* b, GameState* gameState, bool penetrationTest = false) {
	bool result = !isSet(a, EntityFlag_remove) && !isSet(b, EntityFlag_remove);

	result &= !isIgnoringPenetration(a, b);
	result &= collidesWithRaw(a, b, gameState, penetrationTest);
	result &= collidesWithRaw(b, a, gameState, penetrationTest);

	return result;
}

bool isEntityAbove(Entity* top, Entity* bottom) {
	bool result = true;

	Hitbox* topHitboxes = top->hitboxes;

	while(topHitboxes && result) {
		Hitbox* bottomHitboxes = bottom->hitboxes;

		while(bottomHitboxes) {
			double bottomMax = bottom->p.y + bottomHitboxes->collisionOffset.y + bottomHitboxes->collisionSize.y / 2;
			double topMin = top->p.y + topHitboxes->collisionOffset.y - topHitboxes->collisionSize.y / 2;

			if (bottomMax > topMin) {
				result = false;
				break;
			}

			bottomHitboxes = bottomHitboxes->next;
		}

		topHitboxes = topHitboxes->next;
	}

	return result;
}

bool isSolidCollisionRaw(Entity* a, Entity* b, GameState* gameState, bool actuallyMoving) {
	bool result = true;

	switch(a->type) {
		case EntityType_laserBeam: {
			result = false;
		} break;

		case EntityType_player: {
			if (b->type == EntityType_hackEnergy || 
				b->type == EntityType_endPortal) result = false;
		} break;

		case EntityType_pickupField: {
			result = !canPickupField(a, b, gameState);
		} break;

		case EntityType_checkPoint: {
			result = b->type != EntityType_player;
		} break;

		default: {
			if(isProjectile(a)) {
				result = false;
			}
		} break;
	}

	if(actuallyMoving) {
		if(getField(a, ConsoleField_dropsOnHit)) {
			result = false;
		}

		if(getField(a, ConsoleField_crushesEntities)) {
			if(!(isNonHeavyTile(b) && getMovementField(b) == NULL)) result = false;
			if(getField(b, ConsoleField_crushesEntities)) result = true;
		}
	}

	return result;
}

bool isSolidCollision(Entity* a, Entity* b, GameState* gameState, bool actuallyMoving) {
	bool result = isSolidCollisionRaw(a, b, gameState, actuallyMoving) && 
			      isSolidCollisionRaw(b, a, gameState, actuallyMoving);
	return result;
}

V2 moveRaw(Entity*, GameState*, V2, V2* ddP);

V2 getCollisionSize(Entity* entity) {
	R2 bounds;
	bounds.min = v2(99999999, 99999999);
	bounds.max = v2(-9999999, -9999999);

	Hitbox* hitbox = entity->hitboxes;

	updateHitboxRotatedPoints(hitbox, entity);

	while(hitbox) {	
		V2 center = getHitboxCenter(hitbox, entity);

		for(s32 pIndex = 0; pIndex < hitbox->collisionPointsCount; pIndex++) {
			V2 hitP = hitbox->rotatedCollisionPoints[pIndex] + center;

			if(hitP.x < bounds.min.x) {
				bounds.min.x = hitP.x;
			} else if(hitP.x > bounds.max.x) {
				bounds.max.x = hitP.x;
			}

			if(hitP.y < bounds.min.y) {
				bounds.min.y = hitP.y;
			} else if(hitP.y > bounds.max.y) {
				bounds.max.y = hitP.y;
			}
		}

		hitbox = hitbox->next;
	}

	V2 result = getRectSize(bounds);
	return result;
}

enum HitboxPointType {
	HPT_lowest,
	HPT_highest,
};

V2 getHitboxPoint(Entity* entity, HitboxPointType type) {
	V2 result = entity->p;

	Hitbox* hitbox = entity->hitboxes;

	updateHitboxRotatedPoints(hitbox, entity);

	while(hitbox) {
		V2 center = getHitboxCenter(hitbox, entity);

		for(s32 pIndex = 0; pIndex < hitbox->collisionPointsCount; pIndex++) {
			V2 hitP = hitbox->rotatedCollisionPoints[pIndex] + center;

			switch(type) {
				case HPT_lowest: {
					if(hitP.y < result.y) result = hitP;
				} break;

				case HPT_highest: {
					if(hitP.y > result.y) result = hitP;
				} break;

				InvalidDefaultCase;
			}
			
		}

		hitbox = hitbox->next;
	}

	return result;
}

bool crushEntity(Entity* entity, double amt, Entity* heavyTile, GameState* gameState, bool onlyDoMovement, 
				 V2 collisionNormal, bool collisionNormalIsFromHeavyTile) {
	// V2 lowestHeavyTilePoint = getHitboxPoint(heavyTile, HPT_lowest);
	// V2 highestEntityPoint = getHitboxPoint(entity, HPT_highest);

	// if(highestEntityPoint.y <= lowestHeavyTilePoint.y) {


	// if(heavyTile->p.y > entity->p.y && 
	// 	((collisionNormal.y > 0 && !collisionNormalIsFromHeavyTile) || 
	// 	(collisionNormal.y < 0 && collisionNormalIsFromHeavyTile))) {
	if(heavyTile->p.y > entity->p.y) {
		V2 movement = moveRaw(entity, gameState, v2(0, -amt), NULL);

		if(movement.y < 0) {
			setFlags(heavyTile, EntityFlag_grounded);
			heavyTile->timeSinceLastOnGround = 0;
		}

		if(!onlyDoMovement) {
			amt += movement.y;

			double height = getCollisionSize(entity).y;
			double squishAmount = min(height, amt);

			Hitbox* hitbox = entity->hitboxes;
			hitbox->storedRotation = INVALID_STORED_HITBOX_ROTATION;

			double squishedHeight = height - squishAmount;

			if(squishedHeight > 0.01) {
				double heightRatio = squishedHeight / height;
				entity->renderSize.y *= heightRatio;

				while(hitbox) {
					hitbox->collisionOffset *= heightRatio;

					for(s32 pIndex = 0; pIndex < hitbox->collisionPointsCount; pIndex++) {
						hitbox->originalCollisionPoints[pIndex].y *= heightRatio;
					}

					hitbox = hitbox->next;
				}
			} else {
				setFlags(entity, EntityFlag_remove);			
			}
		}

		return true;
	}

	return false;
}

void tryKeyboardJump(Entity* entity, GameState* gameState, ConsoleField* keyboardField) {
	assert(keyboardField->type == ConsoleField_keyboardControlled);

	ConsoleField* jumpField = keyboardField->children[1];
	double jumpHeight = jumpField->doubleValues[jumpField->selectedIndex];

	ConsoleField* doubleJumpField = keyboardField->children[2];
	bool canDoubleJump = doubleJumpField->selectedIndex != 0;

	bool canJump = entity->jumpCount == 0 && 
				   entity->timeSinceLastOnGround < KEYBOARD_JUMP_AIR_TOLERANCE_TIME && 
				   gameState->input.up.pressed 
				   ;//&& affectedByGravity(entity, keyboardField);

	bool attemptingDoubleJump = false;
	
	if (!canJump) {
		attemptingDoubleJump = true;
		canJump = entity->jumpCount < 2 && canDoubleJump && gameState->input.up.justPressed;
	}

	if (canJump) {
		clearFlags(entity, EntityFlag_grounded);
		setFlags(entity, EntityFlag_jumped);

		if(gameState->gravity.y <= 0) {
			entity->dP.y = jumpHeight;
		}
		else {
			entity->dP.y = -jumpHeight;
		}

		if (attemptingDoubleJump) entity->jumpCount = 2;
		else entity->jumpCount++;
	}
}

void toggleCheckpoint(Entity* entity, GameState* gameState, bool toggleReachedField) {
	toggleFlags(entity, EntityFlag_checkPointReached);

	if(toggleReachedField) {
		ConsoleField* reachedField = getField(entity, ConsoleField_bool);
		assert(reachedField);

		if(isSet(entity, EntityFlag_checkPointReached)) {
			reachedField->selectedIndex = 1;
		} else {
			reachedField->selectedIndex = 0;
		}
	}

	if(isSet(entity, EntityFlag_checkPointReached)) {
		entity->defaultTex = gameState->checkPointReached;

		freeCheckPointSave(gameState, entity->checkPointSave);
		entity->checkPointSave = allocateCheckPointSave(gameState, entity->ref);

		saveCompleteGame(gameState, entity->checkPointSave->arena);
	}
	else {
		entity->defaultTex = gameState->checkPointUnreached;
		freeCheckPointSave(gameState, entity->checkPointSave);
		entity->checkPointSave = NULL;
	}
}

void onCollide(Entity* entity, Entity* hitEntity, GameState* gameState, bool* solid, V2 entityVel, V2 collisionNormal, 
			   bool collisionNormalFromHitEntity) {
	ConsoleField* killField = getField(entity, ConsoleField_killsOnHit); 
	bool killed = false;

	if(killField) {
		if(entity->type == EntityType_laserBeam 
		   || hitEntity->type == EntityType_text
		   || hitEntity->type == EntityType_trawler
		   || hitEntity->type == EntityType_trojan
		   || hitEntity->type == EntityType_shrike
		   || hitEntity->type == EntityType_motherShip
		   || hitEntity->type == EntityType_player
		 ) {
		 	killed = true;
			setFlags(hitEntity, EntityFlag_remove);
		}
	}

	switch(entity->type) {
		case EntityType_pickupField: {
			if(canPickupField(entity, hitEntity, gameState)) {
				assert(!gameState->swapField);
				assert(entity->numFields >= 1);
				gameState->swapField = entity->fields[0];
				gameState->swapField->p -= gameState->camera.p;
				rebaseField(gameState->swapField, gameState->swapFieldP);

				removeField(entity->fields, entity->numFields, 0);
				entity->numFields--;
				setFlags(entity, EntityFlag_remove);
			}
			else if(hitEntity->type == EntityType_pickupField) {
				if(hitEntity->fields[0]->type == entity->fields[0]->type) {
					bool removeHitEntity = false;

					if(hitEntity->numFields != entity->numFields) {
						removeHitEntity = hitEntity->numFields < entity->numFields;
					}
					else if(hitEntity->alpha != entity->alpha) {
						removeHitEntity = hitEntity->alpha < entity->alpha;
					}
					else if(hitEntity->p.y != entity->p.y) {
						removeHitEntity = hitEntity->p.y < entity->p.y;
					}
					else {
						removeHitEntity = hitEntity->ref < entity->ref;
					}

					if(removeHitEntity) {
						setFlags(hitEntity, EntityFlag_remove);	
					} else {
						setFlags(entity, EntityFlag_remove);
					}
				}
			}
		} break;

		case EntityType_hackEnergy: {
			if (hitEntity->type == EntityType_player) {
				setFlags(entity, EntityFlag_remove);
			}
		} break;



		case EntityType_endPortal: {
			if (hitEntity->type == EntityType_player) {
				ConsoleField* loadSameLevelField = getField(entity, ConsoleField_bool);
				if(loadSameLevelField && loadSameLevelField->selectedIndex) {
					gameState->reloadCurrentLevel = true;
				} else {
					gameState->loadNextLevel = true;
				}
			}
		} break;


		case EntityType_laserBeam: {
			setFlags(hitEntity, EntityFlag_remove);
		} break;

		case EntityType_checkPoint: {
			if(hitEntity->type == EntityType_player) {
				toggleCheckpoint(entity, gameState, true);
			}

		} break;

		default: {
			if(isProjectile(entity)) {
				setFlags(entity, EntityFlag_remove);
			}
		} break;

	}

	// if(!killed && killField && isSet(entity, EntityFlag_remove)) {
	// 	setFlags(killField, ConsoleFlag_remove);
	// 	removeFieldsIfSet(entity->fields, &entity->numFields);
	// }

	if(getField(entity, ConsoleField_crushesEntities)) {
		if(!(*solid)) {
			bool toggleSolid = true;

			if(entityVel.y < 0 && !getEntityByRef(gameState, gameState->consoleEntityRef)) {
				if(crushEntity(hitEntity, -entityVel.y, entity, gameState, false, collisionNormal, !collisionNormalFromHitEntity)) toggleSolid = false;
			}

			if(toggleSolid) {
				*solid = true;
			}
		}
	}

	if(getField(hitEntity, ConsoleField_dropsOnHit)) {
		bool toggleSolid = true;

		if(entityVel.y < 0) {
			double timeSinceGrounded = entity->timeSinceLastOnGround;
			if (crushEntity(hitEntity, -entityVel.y, entity, gameState, true, collisionNormal, !collisionNormalFromHitEntity)) toggleSolid = false;

			if(timeSinceGrounded > KEYBOARD_JUMP_AIR_TOLERANCE_TIME && !entity->timeSinceLastOnGround) {
				ConsoleField* keyboardField = getField(entity, ConsoleField_keyboardControlled);

				if(keyboardField) {
					tryKeyboardJump(entity, gameState, keyboardField);
				}
			}
		}

		if(toggleSolid) {
			*solid = true;
		}
	}
}

R2 getMaxCollisionExtents(Entity* entity) {
	R2 result = rectCenterRadius(entity->p, v2(0, 0));

	Hitbox* hitboxList = entity->hitboxes;

	while(hitboxList) {
		R2 hitbox = getBoundingBox(entity, hitboxList);

		if (hitbox.min.x < result.min.x) result.min.x = hitbox.min.x;
		if (hitbox.min.y < result.min.y) result.min.y = hitbox.min.y;
		if (hitbox.max.x > result.max.x) result.max.x = hitbox.max.x;
		if (hitbox.max.y > result.max.y) result.max.y = hitbox.max.y;

		hitboxList = hitboxList->next;
	}

	return result;
}

bool isEntityInsideBroadPhase(Entity* entity, Entity* collider) {
	Hitbox* entityHitbox = entity->hitboxes;

	while(entityHitbox) {
		R2 entityBounds = getBoundingBox(entity, entityHitbox);

		Hitbox* colliderHitbox = collider->hitboxes;

		while(colliderHitbox) {
			R2 colliderBounds = getBoundingBox(collider, colliderHitbox);

			if(rectanglesOverlap(entityBounds, colliderBounds)) {
				return true;
			}

			colliderHitbox = colliderHitbox->next;
		}

		entityHitbox = entityHitbox->next;
	}

	return false;
}

void attemptToRemovePenetrationReferences(Entity* entity, GameState* gameState) {
	RefNode* node = entity->ignorePenetrationList;
	RefNode* prevNode = NULL;

	while(node) {
		Entity* collider = getEntityByRef(gameState, node->ref);
		bool removeRef = true;

		if(collider && isEntityInsideBroadPhase(entity, collider)) {
			removeRef = false;
		}

		if(removeRef) {
			if(prevNode) prevNode->next = node->next;
			else entity->ignorePenetrationList = node->next;

			RefNode* nextPtr = node->next;

			node->next = NULL;
			freeRefNode(node, gameState);

			node = nextPtr;
		} else {
			prevNode = node;
			node = node->next;
		}

	}
}

void ignoreAllPenetratingEntities(Entity* entity, GameState* gameState) {
	if(entity->ignorePenetrationList) {
		freeRefNode(entity->ignorePenetrationList, gameState);
		entity->ignorePenetrationList = NULL;
	}

	V2 delta = v2(0.001, -0.001);

	getCollisionTime(entity, gameState, delta, false, 1, true);

//	RefNode* ref = entity->ignorePenetrationList;

	// NOTE: Most of this code is pasted from getCollisionTime
	// s32 partitionCenterX = (s32)(entity->p.x / gameState->chunkSize.x);
	// s32 partitionCenterY = (s32)(entity->p.y / gameState->chunkSize.y);

	// for (s32 partitionXOffs = -1; partitionXOffs <= 1; partitionXOffs++) {
	// 	for (s32 partitionYOffs = -1; partitionYOffs <= 1; partitionYOffs++) {
	// 		s32 partitionX = partitionCenterX + partitionXOffs;
	// 		s32 partitionY = partitionCenterY + partitionYOffs;

	// 		if (partitionX >= 0 && 
	// 			partitionY >= 0 && 
	// 			partitionX < gameState->chunksWidth && 
	// 			partitionY < gameState->chunksHeight) {

	// 			EntityChunk* chunk = gameState->chunks + partitionY * gameState->chunksWidth + partitionX;

	// 			while(chunk) {
	// 				for (s32 colliderIndex = 0; colliderIndex < chunk->numRefs; colliderIndex++) {
	// 					Entity* collider = getEntityByRef(gameState, chunk->entityRefs[colliderIndex]);


	// 					if (collider && collider != entity &&
	// 						collidesWith(entity, collider, gameState)) {

	// 						// if(isEntityInside(entity, collider)) {
	// 						// 	RefNode* node = refNode(gameState, collider->ref, entity->ignorePenetrationList);
	// 						// 	entity->ignorePenetrationList = node;
	// 						// }
									
	// 					}
	// 				}
	// 				chunk = chunk->next;
	// 			}
	// 		}
	// 	}
	// }
}

void projectPointOntoHitbox(V2 point, Hitbox* hitbox, V2 hitboxOffset, V2 direction, ProjectPointResult* result, 
							bool projectingOntoMovingEntity) {
	if(hitbox->collisionPointsCount < 2) {
		//can't project onto a point
		return;
	}

	//ProjectPointResult originalResult = *result;
	//bool insideHitbox = true;

	for(s32 pIndex = 0; pIndex < hitbox->collisionPointsCount; pIndex++) {
		V2 p1 = hitbox->rotatedCollisionPoints[pIndex];
		V2 p2 = hitbox->rotatedCollisionPoints[(pIndex + 1) % hitbox->collisionPointsCount];

		V2 line = p2 - p1;
		assert(line != v2(0, 0));

		V2 lineNormal = perp(line);

		V2 relativeOffset = p1 + hitboxOffset - point;

		double hitTimeDivisor = direction.x * line.y - direction.y * line.x;

		if(hitTimeDivisor) {
			double hitTime = (relativeOffset.x * line.y - relativeOffset.y * line.x) / hitTimeDivisor;

			if(hitTime > 0 && hitTime <= result->hitTime) {
				double lineExtent;

				if(line.x) {
					lineExtent = (hitTime * direction.x - relativeOffset.x) / line.x;		
				} else {
					lineExtent = (hitTime * direction.y - relativeOffset.y) / line.y;
				}

				if(lineExtent >= 0 && lineExtent <= 1) {
					result->hitTime = hitTime;
					result->hitLineNormal = lineNormal;
					result->collisionNormalFromMovingEntity = projectingOntoMovingEntity;
				}
			}
		} else {
			//we are moving parallel to the line
			//TODO: can still collide with the line if we are already on it

			// point = (p1 + hitboxOffset) + t * line
			// (0, 0) = relativeOffset + t * line
			// t * line.x = -relativeOffset.x
			// t = - 
			// t * line.y = -relativeOffset.y

			bool onLine = false;
			double epsilon = 0;

			if(line.x == 0) {
				onLine = relativeOffset.x == 0 && relativeOffset.y >= 0 && p2.y + hitboxOffset.y <= point.y;
			}
			else if(line.y == 0) {
				onLine = relativeOffset.y == 0 && relativeOffset.x >= 0 && p2.x + hitboxOffset.x <= point.x;
			}
			else {
				onLine = epsilonEquals(-relativeOffset.x / line.x, -relativeOffset.y / line.y, epsilon);
			}

			if(onLine) {
				if(dot(direction, lineNormal) > 0) {
					result->hitTime = 0;
					result->hitLineNormal = lineNormal;
					result->collisionNormalFromMovingEntity = projectingOntoMovingEntity;
				}
			}
		}

		// if(insideHitbox) {
		// 	V2 lineCenterToPoint = 0.5 * line + relativeOffset;
		// 	insideHitbox = dot(lineCenterToPoint, lineNormal) < 0;
		// }
	}

	// if(insideHitbox) {
	// 	*result = originalResult;
	// }
} 

void getPolygonCollisionTime(Hitbox* moving, Hitbox* fixed, Entity* movingEntity, Entity* fixedEntity, GameState* gameState,
							 GetCollisionTimeResult* result, V2 delta, bool solidCollision) {

	V2 movingOffset = getHitboxCenter(moving, movingEntity);
	V2 fixedOffset = getHitboxCenter(fixed, fixedEntity);

	ProjectPointResult projectResult = {};
	projectResult.hitTime = 1;

	updateHitboxRotatedPoints(moving, movingEntity);
	updateHitboxRotatedPoints(fixed, fixedEntity);

	for(s32 pIndex = 0; pIndex < moving->collisionPointsCount; pIndex++) {
		V2 movingPoint = moving->rotatedCollisionPoints[pIndex] + movingOffset;
		projectPointOntoHitbox(movingPoint, fixed, fixedOffset, delta, &projectResult, true);
	}

	delta *= -1;

	for(s32 pIndex = 0; pIndex < fixed->collisionPointsCount; pIndex++) {
		V2 movingPoint = fixed->rotatedCollisionPoints[pIndex] + fixedOffset;
		projectPointOntoHitbox(movingPoint, moving, movingOffset, delta, &projectResult, false);
	}

	if(projectResult.hitTime < result->collisionTime) {
		result->hitEntity = fixedEntity;
		result->collisionTime = projectResult.hitTime;
		result->collisionNormal = projectResult.hitLineNormal;

		result->collisionNormalFromHitEntity = projectResult.collisionNormalFromMovingEntity;

		if(solidCollision) {
			result->solidEntity = fixedEntity;
			result->solidCollisionTime = projectResult.hitTime;
			result->solidCollisionNormal = projectResult.hitLineNormal;
		}
	}
}

GetCollisionTimeResult getCollisionTime(Entity* entity, GameState* gameState, V2 delta, bool actuallyMoving, 
										double maxCollisionTime, bool ignorePenetratingEntities) {
	GetCollisionTimeResult result = {};
	result.collisionTime = maxCollisionTime;
	result.solidCollisionTime = maxCollisionTime;

	if(delta == v2(0, 0)) return result;

	s32 partitionCenterX = (s32)(entity->p.x / gameState->chunkSize.x);
	s32 partitionCenterY = (s32)(entity->p.y / gameState->chunkSize.y);

	for (s32 partitionXOffs = -1; partitionXOffs <= 1; partitionXOffs++) {
		for (s32 partitionYOffs = -1; partitionYOffs <= 1; partitionYOffs++) {
			s32 partitionX = partitionCenterX + partitionXOffs;
			s32 partitionY = partitionCenterY + partitionYOffs;

			if (partitionX >= 0 && 
				partitionY >= 0 && 
				partitionX < gameState->chunksWidth && 
				partitionY < gameState->chunksHeight) {

				EntityChunk* chunk = gameState->chunks + partitionY * gameState->chunksWidth + partitionX;

				while(chunk) {
					for (s32 colliderIndex = 0; colliderIndex < chunk->numRefs; colliderIndex++) {
						Entity* collider = getEntityByRef(gameState, chunk->entityRefs[colliderIndex]);
						
						if (collider && collider != entity && collidesWith(entity, collider, gameState, ignorePenetratingEntities)) {
							bool solidCollision = isSolidCollision(entity, collider, gameState, actuallyMoving);

							Hitbox* colliderHitboxList = collider->hitboxes;

							while(colliderHitboxList) {
								R2 colliderHitbox = getBoundingBox(collider, colliderHitboxList);
								R2 paddedColliderHitbox = addRadiusTo(colliderHitbox, v2(fabs(delta.x), fabs(delta.y)));

								Hitbox* entityHitboxList = entity->hitboxes;

								while (entityHitboxList) {
									R2 entityHitbox = getBoundingBox(entity, entityHitboxList);
								
									//Broad phase
									if(rectanglesOverlap(paddedColliderHitbox, entityHitbox)) {
										if(ignorePenetratingEntities) {
											entity->ignorePenetrationList = refNode(gameState, collider->ref, entity->ignorePenetrationList);
										} else {
											//Narrow phase
											getPolygonCollisionTime(entityHitboxList, colliderHitboxList, entity, collider, 
																	gameState, &result, delta, solidCollision);
										}

								
									}
										
									entityHitboxList = entityHitboxList->next;
								}
							

								colliderHitboxList = colliderHitboxList->next;
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

GetCollisionTimeResult onGround(Entity* entity, GameState* gameState, double dt, V2 dP, V2 ddP) {
	V2 delta = getVelocity(dt, dP, ddP);

	GetCollisionTimeResult collisionResult = getCollisionTime(entity, gameState, delta, false);

	//Entity* result = collisionResult.solidEntity;

	return collisionResult;
}

GetCollisionTimeResult onGround(Entity* entity, GameState* gameState) {
	double moveTime = 1.0 / 60.0;

	//NOTE: This is so that ground references can still be made in 0 gravity
	V2 ddP = gameState->gravity;
	if(ddP.y == 0) {
		ddP.y = -1;
	}

	GetCollisionTimeResult result = onGround(entity, gameState, moveTime, v2(0, 0), ddP);
	return result;
}

Entity* getAbove(Entity* entity, GameState* gameState) {
	double moveTime = 1.0 / 60.0;

	//NOTE: This is so that ground references can still be made in 0 gravity
	V2 ddP = -gameState->gravity;
	if(ddP.y == 0) ddP.y = 0.01;

	Entity* result = onGround(entity, gameState, moveTime, v2(0, 0), ddP).solidEntity;
	return result;
}

void adjustVelocitiesFromHit(V2 normal, V2* delta, V2* dP, V2* ddP) {
	if(normal == v2(0, 0)) {
		*delta = v2(0, 0);
		*dP = v2(0, 0);
		if(ddP) *ddP = v2(0, 0);
	} else {
		//normalizing twice is necessary because of precision errors
		normal = normalize(normalize(-normal));

		V2 deltaProjection = vectorProjection(*delta, normal);
		*delta -= deltaProjection;

		V2 dPProjection = vectorProjection(*dP, normal);
		*dP -= dPProjection;

		if(ddP) {
			V2 ddPProjection = vectorProjection(*ddP, normal);
			*ddP -= ddPProjection;
		}
	}
}

//Returns the total movement that is made
V2 moveRaw(Entity* entity, GameState* gameState, V2 delta, V2* ddP, double rotation) {
	V2 startP = entity->p;

	double epsilon = 0.005;

	if(rotation != entity->rotation) {
		double oldRotation = entity->rotation;

		V2 move = v2(0.0001, 0);
		entity->rotation = rotation;

		GetCollisionTimeResult collisionResult = getCollisionTime(entity, gameState, move, false);

		if(collisionResult.solidEntity) {
			entity->rotation = oldRotation;
		}
	}

	double remainingCollisionTime = 1;

	for (s32 moveIteration = 0; moveIteration < 4 && remainingCollisionTime > 0; moveIteration++) {
		if(delta == v2(0, 0)) break;

		GetCollisionTimeResult collisionResult = getCollisionTime(entity, gameState, delta, true, remainingCollisionTime);
		assert(collisionResult.collisionTime >= 0 && collisionResult.collisionTime <= remainingCollisionTime);

		bool hitGround = false;

		if (collisionResult.solidEntity) {
			//NOTE: If moving and you would hit an entity which is using you as ground
			//		then we try moving that entity first.
			RefNode* nextTopEntity = entity->groundReferenceList;
			RefNode* prevTopEntity = NULL;

			while (nextTopEntity) {
				if (nextTopEntity->ref == collisionResult.solidEntity->ref) {
					moveRaw(collisionResult.solidEntity, gameState, delta * remainingCollisionTime, NULL);
					hitGround = true;

					if (prevTopEntity) {
						prevTopEntity->next = nextTopEntity->next;
					} else {
						entity->groundReferenceList = NULL;
					}

					nextTopEntity->next = NULL;
					freeRefNode(nextTopEntity, gameState);

					break;
				}

				prevTopEntity = nextTopEntity;
				nextTopEntity = nextTopEntity->next;
			}
		}

		if(!hitGround) {
			double collisionTime = collisionResult.collisionTime;
			remainingCollisionTime -= collisionTime;

			collisionTime = max(0, collisionTime - epsilon);

			V2 movement = collisionTime * delta;
			setEntityP(entity, entity->p + movement, gameState);

			V2 entityVelOriginal = delta;

			if(collisionResult.solidEntity) {
				adjustVelocitiesFromHit(collisionResult.solidCollisionNormal, &delta, &entity->dP, ddP);
			}

			if(collisionResult.hitEntity) {
				bool solid1 = collisionResult.solidEntity != NULL;
				bool solid2 = solid1;

				onCollide(entity, collisionResult.hitEntity, gameState, &solid1, entityVelOriginal, 
						  collisionResult.collisionNormal, collisionResult.collisionNormalFromHitEntity);
				onCollide(collisionResult.hitEntity, entity, gameState, &solid2, v2(0, 0), 
					      collisionResult.collisionNormal, collisionResult.collisionNormalFromHitEntity);

				if(!collisionResult.solidEntity && (solid1 || solid2)) {
					adjustVelocitiesFromHit(collisionResult.collisionNormal, &delta, &entity->dP, ddP);
				}
			}
		}
	}

	//NOTE: This moves all of the entities which are supported by this one (as the ground)
	RefNode* nextTopEntity = entity->groundReferenceList;

	V2 totalMovement = (entity->p - startP) * (1 + epsilon);
	double totalMovementLenSq = lengthSq(totalMovement); 

	if (totalMovementLenSq > 0) {
		while (nextTopEntity) {
			Entity* top = getEntityByRef(gameState, nextTopEntity->ref);
			if (top && !isSet(top, EntityFlag_movedByGround)) {
				setFlags(top, EntityFlag_movedByGround);
				moveRaw(top, gameState, totalMovement, NULL);
			}
			nextTopEntity = nextTopEntity->next;
		}

		if(entity->type == EntityType_trawler) {
			double totalMovementLen = sqrt(totalMovementLenSq);

			double diameter = (entity->renderSize.x + entity->renderSize.y) * 0.5;
			double circumference = PI * diameter;

			double rotationFudge = 20;

			double rot = totalMovementLen / circumference * rotationFudge;
			if(totalMovement.x > 0) rot *= -1;

			entity->wheelRotation += rot;
		}
	}

	return totalMovement;
}

V2 moveRaw(Entity* entity, GameState* gameState, V2 delta, V2* ddP) {
	V2 result = moveRaw(entity, gameState, delta, ddP, entity->rotation);
	return result;
}

V2 move(Entity* entity, double dt, GameState* gameState, V2 ddP, double rotation) {
	V2 delta = getVelocity(dt, entity->dP, ddP);
	V2 movement = moveRaw(entity, gameState, delta, &ddP, rotation);
	entity->dP += ddP * dt;
	return movement;
}

V2 move(Entity* entity, double dt, GameState* gameState, V2 ddP) {
	V2 result = move(entity, dt, gameState, ddP, entity->rotation);
	return result;
}

bool moveTowardsTargetParabolic(Entity* entity, GameState* gameState, double dt, V2 target, double initialDstToTarget, 
								double maxSpeed) {
	V2 delta = target - entity->p;
	double dstToTarget = length(delta);

	double percentageToTarget = (initialDstToTarget - dstToTarget) / initialDstToTarget;
	if(initialDstToTarget == 0) percentageToTarget = 0;

	double initialTime = 0.1;
	double speedEquationCoefficient = maxSpeed / ((0.5 + initialTime) * (-0.5 - initialTime));
	double speed = dt * speedEquationCoefficient * (percentageToTarget + initialTime) * (percentageToTarget - 1 - initialTime);

	V2 movement;

	if (speed > dstToTarget) {
		movement = delta;
	} else {
		movement = speed * normalize(delta);
	}

	if(dstToTarget) {
		V2 change = moveRaw(entity, gameState, movement, NULL);

		if(!isTileType(entity)) {
			if(change.x < 0) {
				setEntityFacingDirection(entity, true, gameState);
			} 
			else if (change.x > 0) {
				setEntityFacingDirection(entity, false, gameState);
			}
		}
	}

	return entity->p == target;
}

bool moveTowardsWaypoint(Entity* entity, GameState* gameState, double dt, V2 target, double xMoveAcceleration) {
	V2 delta = target - entity->p;
	double deltaLen = length(delta);

	if (deltaLen > 0) {
		V2 ddP = normalize(delta) * xMoveAcceleration;

		V2 velocity = getVelocity(dt, entity->dP, ddP);
		double velocityLen = length(velocity);

		if(deltaLen <= velocityLen) {
			entity->dP = v2(0, 0);
			moveRaw(entity, gameState, delta, NULL);
		} else {
			move(entity, dt, gameState, ddP);
		}		

		if(!isTileType(entity)) {
			if(delta.x < 0) {
				setEntityFacingDirection(entity, true, gameState);
			} 
			else if (delta.x > 0) {
				setEntityFacingDirection(entity, false, gameState);
			}
		}

		return false;
	}

	return true;
}

void changeSpotLightAngle(Entity* entity, double wantedSpotLightAngle, double dt) {
	if(canEntityRotate(entity)) {
		wantedSpotLightAngle = angleIn0Tau(wantedSpotLightAngle);
	} else {
		wantedSpotLightAngle = angleIn0Tau(wantedSpotLightAngle - entity->rotation);
	}

	double spotlightAngleMoveSpeed = PI * dt;

	double clockwise = angleIn0Tau(wantedSpotLightAngle - entity->spotLightAngle);
	double counterClockwise = angleIn0Tau(entity->spotLightAngle - wantedSpotLightAngle);

	if(clockwise < counterClockwise) {
		double movement = min(clockwise, spotlightAngleMoveSpeed);
		entity->spotLightAngle = angleIn0Tau(entity->spotLightAngle + movement);
	} else {
		double movement = min(counterClockwise, spotlightAngleMoveSpeed);
		entity->spotLightAngle = angleIn0Tau(entity->spotLightAngle - movement);
	}
}

ConsoleField* getField(ConsoleField** fields, int numFields, ConsoleFieldType type, s32* index = NULL) {
	ConsoleField* result = NULL;

	for (s32 fieldIndex = 0; fieldIndex < numFields; fieldIndex++) {
		if (fields[fieldIndex]->type == type) {
			result = fields[fieldIndex];
			if(index) *index = fieldIndex;
			break;
		}
	}

	return result;
}

ConsoleField* getField(Entity* entity, ConsoleFieldType type, s32* index) {
	ConsoleField* result = NULL;

	if(!isSet(entity, EntityFlag_remove)) {
		s32 firstFieldIndex = entity->type == EntityType_pickupField ? 1 : 0;
		result = getField(entity->fields + firstFieldIndex, entity->numFields - firstFieldIndex, type, index);
	}

	return result;
}

bool isMouseInside(Entity* entity, Input* input) {
	bool result = pointInsideRect(translateRect(entity->clickBox, entity->p), input->mouseInWorld);
	return result;
}

bool inSolidGridBounds(GameState* gameState, s32 xTile, s32 yTile) {
	bool result = xTile >= 0 &&
				  yTile >= 0 &&
				  xTile < gameState->solidGridWidth &&
				  yTile < gameState->solidGridHeight;

	return result;
}

void addSolidLocation(double xPos, double yPos, GameState* gameState) {
	s32 xTile = (s32)(xPos / gameState->solidGridSquareSize);
	s32 yTile = (s32)(yPos / gameState->solidGridSquareSize);

	if (inSolidGridBounds(gameState, xTile, yTile)) {
		gameState->solidGrid[xTile * gameState->solidGridHeight + yTile].solid = true;
	}
}

void testLocationAsClosestNonSolidNode(double xPos, double yPos, double* minDstSq, GameState* gameState, Entity* entity, PathNode** result) {
	s32 xTile = (s32)floor(xPos / gameState->solidGridSquareSize);
	s32 yTile = (s32)floor(yPos / gameState->solidGridSquareSize);

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

	//TODO: Find a more robust way of doing this so entities don't get stuck
	s32 radiusFudge = 5;

	if(other) {
		R2 otherHitbox = getMaxCollisionExtents(other);
		otherSize = getRectSize(otherHitbox);
		radiusFudge = 1;
	}

	testLocationAsClosestNonSolidNode(entity->p.x, entity->p.y, &minDstSq, gameState, entity, &result);

	while (hitboxList) {
		R2 hitbox = getBoundingBox(entity, hitboxList);

		if(other) {
			hitbox = addDiameterTo(hitbox, otherSize);
		}

		V2 hitboxCenter = getRectCenter(hitbox);

		s32 radiusX = (s32)ceil(getRectWidth(hitbox) / (2 * gameState->solidGridSquareSize)) + radiusFudge;
		s32 radiusY = (s32)ceil(getRectHeight(hitbox) / (2 * gameState->solidGridSquareSize)) + radiusFudge;

		for(s32 xOffs = -radiusX; xOffs <= radiusX; xOffs++) {
			for (s32 yOffs = -radiusY; yOffs <= radiusY; yOffs++) {
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

		s32 xTile = (s32)floor(p.x / gameState->solidGridSquareSize);
		s32 yTile = (s32)floor(p.y / gameState->solidGridSquareSize);

		if (inSolidGridBounds(gameState, xTile, yTile)) {
			if (gameState->solidGrid[xTile * gameState->solidGridHeight + yTile].solid) return false;
		}
	}

	s32 xTile = (s32)floor(maxX / gameState->solidGridSquareSize);
	s32 yTile = (s32)floor(maxY / gameState->solidGridSquareSize);	

	if (inSolidGridBounds(gameState, xTile, yTile)) {
		if (gameState->solidGrid[xTile * gameState->solidGridHeight + yTile].solid) return false;
	}

	return true;
}

V2 computePath(GameState* gameState, Entity* start, Entity* goal) {
	for (s32 tileX = 0; tileX < gameState->solidGridWidth; tileX++) {
		for (s32 tileY = 0; tileY < gameState->solidGridHeight; tileY++) {
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

	for (s32 colliderIndex = 0; colliderIndex < gameState->numEntities; colliderIndex++) {
		Entity* collider = gameState->entities + colliderIndex;

		if (collider != start && collider != goal &&
			collidesWith(start, collider, gameState) &&
			collider->spawnerRef != goal->ref) {

			Hitbox* colliderHitboxList = collider->hitboxes;

			while(colliderHitboxList) {
				R2 colliderHitbox = getBoundingBox(collider, colliderHitboxList);

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
			s32 currentIndex = 0;
			double minEstimatedTotalCost = 1000000000000.0;

			for (s32 nodeIndex = 0; nodeIndex < gameState->openPathNodesCount; nodeIndex++) {
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

			for(s32 xOffs = -1; xOffs <= 1; xOffs++) {
				for (s32 yOffs = -1; yOffs <= 1; yOffs++) {
					if (xOffs != 0 || yOffs != 0) {
						s32 tileX = current->tileX + xOffs;
						s32 tileY = current->tileY + yOffs;

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
	double maxCameraX = max(0, gameState->mapSize.x - gameState->windowSize.x);
	double x = clamp((double)(entity->p.x - gameState->windowSize.x / 2.0), 0, maxCameraX);

	gameState->camera.newP = v2(x, 0);
}

bool isTarget(Entity* entity, GameState* gameState) {
	bool result = refNodeListContainsRef(gameState->targetRefs, entity->ref);
	return result;
}

bool isValidTarget(Entity* shooter, Entity* target, GameState* gameState) {
	bool valid = false;

	if(target && target->ref != shooter->ref && target->cloakFactor != 1) {
		ConsoleField* guardedField = getField(target, ConsoleField_guarded);

		if(guardedField) {
			ConsoleField* bodyguardField = getField(shooter, ConsoleField_bodyguard);

			if(!bodyguardField) {
				valid = true;
			}
		} else {
			valid = true;
		}
	}

	return valid;
}

Entity* getClosestTarget(Entity* entity, GameState* gameState, double* targetDst) {
	Entity* result = NULL;
	double minDstSq = 99999999999;

	for(RefNode* node = gameState->targetRefs; node; node = node->next) {
		Entity* testTarget = getEntityByRef(gameState, node->ref);

		if(isValidTarget(entity, testTarget, gameState)) {
			double testDstSq = dstSq(entity->p, testTarget->p);

			if(testDstSq < minDstSq) {
				minDstSq = testDstSq;
				result = testTarget;
			}
		}
	}

	*targetDst = minDstSq;

	return result;
}

Entity* getClosestGuardTarget(Entity* entity, GameState* gameState, double* targetDst) {
	Entity* result = NULL;
	double minDstSq = 99999999999;

	RefNode* node = gameState->guardTargetRefs;

	while(node) {
		Entity* testTarget = getEntityByRef(gameState, node->ref);

		if(testTarget && testTarget->ref != entity->ref && testTarget->cloakFactor != 1) {
			double testDstSq = dstSq(entity->p, testTarget->p);

			if(testDstSq < minDstSq) {
				minDstSq = testDstSq;
				result = testTarget;
			}
		}

		node = node->next;
	}

	*targetDst = minDstSq;

	return result;
}

double getSpotLightAngle(Entity* entity) {
	double lightAngle;

	if(entity->type == EntityType_trojan) {
		lightAngle = entity->spotLightAngle;
	} else {
		lightAngle = entity->rotation + entity->spotLightAngle;
	}

	return lightAngle;
}

Entity* getTestEntity(GameState* gameState) {
	Entity* testEntity = getEntityByRef(gameState, gameState->testEntityRef);
	assert(testEntity);
	testEntity->flags = 0;

	return testEntity;
}

void releaseTestEntity(GameState* gameState, Entity* testEntity) {
	assert(testEntity->ref == gameState->testEntityRef);
	testEntity->type = EntityType_test;
	setFlags(testEntity, EntityFlag_noMovementByDefault);
	freeEntityDuringLevel(testEntity, gameState, true);
}

Entity* getClosestTargetInSight(Entity* entity, GameState* gameState, double sightRadius, double fov) {
	fov = toRadians(fov);
	double halfFov = fov * 0.5;

	double lightAngle = getSpotLightAngle(entity);
	double minSightAngle = angleIn0Tau(lightAngle - halfFov);
	double maxSightAngle = angleIn0Tau(lightAngle + halfFov);

	RefNode* targetNode = gameState->targetRefs;
	Entity* target = NULL;
	double targetDst = 0;

	while(targetNode) {
		Entity* testTarget = getEntityByRef(gameState, targetNode->ref);

		if(isValidTarget(entity, testTarget, gameState)) {
			V2 toTestEntity = testTarget->p - entity->p;
			double dstToEntity = length(toTestEntity);

			if(dstToEntity <= sightRadius) {
				double dir = getRad(toTestEntity);
				bool canSee = isRadiansBetween(dir, minSightAngle, maxSightAngle);

				Hitbox* hitboxes = testTarget->hitboxes;

				//TODO: Find a more robust way of determining if the entity is inside of the view arc
				while(hitboxes && !canSee) {
					V2 hitboxCenter = getHitboxCenter(hitboxes, testTarget) - entity->p;

					updateHitboxRotatedPoints(hitboxes, testTarget);

					for(s32 pIndex = 0; pIndex < hitboxes->collisionPointsCount; pIndex++) {
						toTestEntity = hitboxes->rotatedCollisionPoints[pIndex] + hitboxCenter;
						if (isRadiansBetween(getRad(toTestEntity), minSightAngle, maxSightAngle)) {
							canSee = true;
							break;
						}
					}

					hitboxes = hitboxes->next;
				}

				if(canSee) {
					#if 1
					Entity* testEntity = getTestEntity(gameState);
					testEntity->p = entity->p;
					initTestProjectile(testEntity, entity, gameState);

					// Hitbox* entityHitboxes = entity->hitboxes;

					// //TODO: Use the hitbox of a projectile to do the raycast
					// Hitbox raycastOrigin = {};
					// raycastOrigin.collisionPointsCount = 1;
					// raycastOrigin.storedRotation = entity->rotation;
					// entity->hitboxes = &raycastOrigin;

					GetCollisionTimeResult collisionResult = getCollisionTime(testEntity, gameState, toTestEntity, false);
					releaseTestEntity(gameState, testEntity);

					//entity->hitboxes = entityHitboxes;

					bool occluded = (collisionResult.hitEntity && collisionResult.hitEntity != testTarget);

					#else				
					bool occluded = false;
					#endif
	
					if(!occluded) {
						if(!target || dstToEntity < targetDst) {
							targetDst = dstToEntity;
							target = testTarget;
						}
					}
				}
			}
		}

		targetNode = targetNode->next;
	}

	return target;
}

Entity* updateSpotlightBasedOnSpotlightField(Entity* entity, GameState* gameState, ConsoleField* spotLightField) {
	assert(spotLightField->type == ConsoleField_spotlight);

	ConsoleField* radiusField = spotLightField->children[0];
	double sightRadius = radiusField->doubleValues[radiusField->selectedIndex];

	ConsoleField* fovField = spotLightField->children[1];
	double fov = fovField->doubleValues[fovField->selectedIndex];

	double lightAngle = getSpotLightAngle(entity);

	V3 lightColor = v3(1, 1, .9);
	
#if ENABLE_LIGHTING
	lightColor *= (1 - entity->cloakFactor);
	SpotLight spotLight = createSpotLight(entity->p, lightColor, sightRadius, lightAngle, fov);
	pushSpotLight(gameState->renderGroup, &spotLight, true);
#else
	u8 r = (u8)(255 * lightColor.x);
	u8 g = (u8)(255 * lightColor.y);
	u8 b = (u8)(255 * lightColor.z);
	u8 a = (u8)(25 * (1 - entity->cloakFactor));

	Color color = createColor(r, g, b, a);

	V2 boundsOffset = v2(0.5, 0) * sightRadius;
	boundsOffset = rotate(boundsOffset, lightAngle);

	R2 lightBounds = rectCenterDiameter(entity->p + boundsOffset, v2(1, 1) * sightRadius);

	pushTexture(gameState->renderGroup, gameState->lightTriangle, lightBounds, lightAngle, false, false, DrawOrder_light,
				true, color, 1);
#endif

	Entity* target = getClosestTargetInSight(entity, gameState, sightRadius, fov); 

	if(target) {
		entity->targetRef = target->ref;
	} else {
		entity->targetRef = 0;
	}

	return target;
}

bool shootBasedOnShootingField(Entity* entity, GameState* gameState, double dt, ConsoleField* shootField) {
	bool shootingEnabled = shootField && !gameState->doingInitialSim;
	bool shootingState = shootingEnabled;

	//NOTE:This handles shooting if the entity should shoot
	if (shootingEnabled) {
		if (isSet(entity, EntityFlag_unchargingAfterShooting)) {
			shootField->shootTimer -= dt;

			if (shootField->shootTimer <= 0) {
				shootField->shootTimer = 0;
				clearFlags(entity, EntityFlag_unchargingAfterShooting);
				shootingState = false;
			} else {
				Entity* target = getEntityByRef(gameState, entity->targetRef); 
				if(target) {
					changeSpotLightAngle(entity, getRad(target->p - entity->p), dt);
				}
			}
		} else {
			assert(entity->targetRef != entity->ref);
			Entity* target = getEntityByRef(gameState, entity->targetRef); 

			if (target) {
				if (target->p.x < entity->p.x) {
					setEntityFacingDirection(entity, true, gameState);
				}
				else if (target->p.x > entity->p.x) {
					setEntityFacingDirection(entity, false, gameState);
				}

				changeSpotLightAngle(entity, getRad(target->p - entity->p), dt);
			} else {
				clearFlags(entity, EntityFlag_shooting);
				setFlags(entity, EntityFlag_unchargingAfterShooting);
			}

			if(isSet(entity, EntityFlag_shooting)) {
				shootField->shootTimer += dt;

				if (shootField->shootTimer >= gameState->shootDelay) {
					shootField->shootTimer = gameState->shootDelay;
					clearFlags(entity, EntityFlag_shooting);
					setFlags(entity, EntityFlag_unchargingAfterShooting);

					V2 spawnOffset = v2(0, 0);

					switch(entity->type) {
						case EntityType_trojan: 
							spawnOffset = v2(0, -0.46);
							break;
						case EntityType_motherShip:
							spawnOffset = v2(0, -1.75);
							break;
                        default: {
                        } break;
					}

					if (isSet(entity, EntityFlag_facesLeft)) {
						spawnOffset.x *= -1;
					}

					spawnOffset = rotate(spawnOffset, entity->rotation);
					
					V2 spawnP = entity->p + spawnOffset;

					if(target) {
						ConsoleField* bulletSpeedField = shootField->children[0];
						double bulletSpeed = bulletSpeedField->doubleValues[bulletSpeedField->selectedIndex];

						if(shootField->shootEntityType == EntityType_motherShip) {
							addMotherShipProjectile(gameState, spawnP, target->p, entity->ref, bulletSpeed);
						} 
						else if(shootField->shootEntityType == EntityType_trojan) {
							addTrojanBolt(gameState, spawnP, target->p, entity->ref, bulletSpeed);
						}
						else if(shootField->shootEntityType == EntityType_trawler) {
							addTrawlerBolt(gameState, spawnP, target->p, entity->ref, bulletSpeed, TRAWLER_BOLT_SIZE);
						}
						else if(shootField->shootEntityType == EntityType_shrike) {
							addTrawlerBolt(gameState, spawnP, target->p, entity->ref, bulletSpeed, SHRIKE_BOLT_SIZE);
						}
						else {
							InvalidCodePath;
						}
					}
				}
			} else { 
				if (target) {
					shootField->shootTimer = 0;
					setFlags(entity, EntityFlag_shooting);
					changeSpotLightAngle(entity, getRad(target->p - entity->p), dt);
				} else {
					//No target was found (not in shooting state anymore)
					shootingState = false;
				}
			} 
		} 
	}

	return shootingState;
}

V2 accelerateEntity(Entity* entity, double xMoveAcceleration) {
	V2 result = {};

	if(entity->groundNormal == v2(0, 0)) {
		result.x = xMoveAcceleration;
	} else {
		V2 groundDir = normalize(normalize(-perp(entity->groundNormal))) * xMoveAcceleration;

		if((groundDir.x < 0 && xMoveAcceleration > 0) || (groundDir.x > 0 && xMoveAcceleration < 0)) {
			groundDir *= -1;
		}

		result += groundDir;
	}

	return result;
}

bool tryPatrolMove(Entity* entity, GameState* gameState, double xMoveAcceleration, double dt, V2 ddP, bool undergoesGravity) {
	V2 oldP = entity->p;
	double oldWheelRotation = entity->wheelRotation;

	bool32 movingLeft = isSet(entity, EntityFlag_facesLeft);
	if (movingLeft) {
		xMoveAcceleration *= -1;
	}

	ddP.x += xMoveAcceleration;
	//ddP += accelerateEntity(entity, xMoveAcceleration);

	V2 movement = move(entity, dt, gameState, ddP);

	bool shouldChangeDirection = (movement.x == 0);

	if(!shouldChangeDirection && undergoesGravity) {
		Hitbox* hitbox = entity->hitboxes;
		R2 bounds = r2(entity->p, entity->p);

		while(hitbox) {
			V2 offset = getHitboxCenter(hitbox, entity);

			updateHitboxRotatedPoints(hitbox, entity);

			for(s32 pIndex = 0; pIndex < hitbox->collisionPointsCount; pIndex++) {
				V2 hitP = hitbox->rotatedCollisionPoints[pIndex] + offset;

				if(hitP.x > bounds.max.x) bounds.max.x = hitP.x;
				else if(hitP.x < bounds.min.x) bounds.min.x = hitP.x;
				if(hitP.y < bounds.min.y) bounds.min.y = hitP.y;
				else if(hitP.y > bounds.max.y) bounds.max.y = hitP.y;
			}

			hitbox = hitbox->next;
		}

		V2 dP = v2(0, - getRectHeight(bounds));
		V2 ddP = v2(0, 0);
		bool offOfGround = onGround(entity, gameState, 1, dP, ddP).solidEntity == NULL;

		if (!offOfGround) {
			V2 entityP = entity->p;

			double width = getRectWidth(bounds) * 0.66;
			if(movingLeft) width *= -1;
			V2 translation = v2(width, 0);
			setEntityP(entity, entityP + translation, gameState);

			double height = getRectHeight(bounds);
			V2 delta = v2(0, -height * 2);

			GetCollisionTimeResult collisionResult = getCollisionTime(entity, gameState, delta, false, 1); 
			offOfGround = !collisionResult.solidEntity;

			if(offOfGround) {
				setEntityP(entity, oldP, gameState);
				entity->wheelRotation = oldWheelRotation;
			} else {
				setEntityP(entity, entityP, gameState);
			}
		} 

		shouldChangeDirection = offOfGround;
	}

	if(!shouldChangeDirection) {
		double spotLightDir = getRad(movement);
		changeSpotLightAngle(entity, spotLightDir, dt);
	}

	return shouldChangeDirection;
}

void drawCollisionBounds(Entity* entity, RenderGroup* renderGroup, double alpha) {
	Hitbox* hitbox = entity->hitboxes;

	u8 a = (u8)(255.5 * alpha);
	Color color = createColor(255, 0, 0, a);

	while (hitbox) {
		V2 hitboxOffset = getHitboxCenter(hitbox, entity);

		updateHitboxRotatedPoints(hitbox, entity);

		#if 0
		pushOutlinedRect(renderGroup, getBoundingBox(entity, hitbox),
						 0.02f, createColor(255, 127, 255, 255), true);
		#endif

		assert(hitbox->collisionPointsCount > 0 && hitbox->collisionPointsCount < MAX_COLLISION_POINTS);

		for(s32 pIndex = 0; pIndex < hitbox->collisionPointsCount; pIndex++) {
			V2 p1 = hitbox->rotatedCollisionPoints[pIndex] + hitboxOffset;
			V2 p2 = hitbox->rotatedCollisionPoints[(pIndex + 1) % hitbox->collisionPointsCount] + hitboxOffset;

			pushDashedLine(renderGroup, color, p1, p2, 0.02, 0.05, 0.05, true);
			// pushSortEnd(renderGroup);

			// drawRenderGroup(renderGroup, NULL);
		}

		hitbox = hitbox->next;
	}
}

void moveEntityBasedOnMovementField(Entity* entity, GameState* gameState, double dt, double groundFriction, 
									bool doingOtherAction, bool canMoveSpotLight) {
	//NOTE: This is a hack to make the gravity feel better. There is more gravity when an object
	//		like the player is falling to make the gravity seem less 'floaty'.
	V2 gravity = gameState->gravity;
	if (entity->dP.y < 0 && isSet(entity, EntityFlag_jumped)) gravity *= 2;		
	V2 ddP;	


	ConsoleField* movementField = getMovementField(entity);
	
	if(affectedByGravity(entity, movementField)) {
		ddP = gravity;
	} else {
		ddP = v2(0, 0);
	}


	if (!isProjectile(entity)) {
		entity->dP.x *= groundFriction;

		bool airFriction = (movementField && 
						   (movementField->type == ConsoleField_seeksTarget ||
						   	movementField->type == ConsoleField_bodyguard ||
						   	movementField->type == ConsoleField_followsWaypoints));

		if (airFriction) {
			entity->dP.y *= groundFriction;
		}
	}

	bool defaultMove = true;

	//NOTE: This handles moving the entity
	if (movementField) {
		ConsoleField* speedField = movementField->children[0];
		double xMoveAcceleration = speedField->doubleValues[speedField->selectedIndex];

		defaultMove = false;

		double wantedSpotLightAngle = entity->spotLightAngle;

		switch(movementField->type) {




			case ConsoleField_keyboardControlled: {
				if (!gameState->doingInitialSim && !isSet(entity, EntityFlag_remove) && dt > 0) {
					wantedSpotLightAngle = entity->spotLightAngle = getRad(gameState->input.mouseInWorld - entity->p);

					double xMove = 0;

					if (gameState->input.right.pressed) xMove += xMoveAcceleration;
					if (gameState->input.left.pressed) xMove -= xMoveAcceleration;

					ddP += accelerateEntity(entity, xMove);

					if (xMove > 0) {
						setEntityFacingDirection(entity, false, gameState);
					}
					else if (xMove < 0) {
						setEntityFacingDirection(entity, true, gameState);
					}

					tryKeyboardJump(entity, gameState, movementField);					
				}

				move(entity, dt, gameState, ddP);
			} break;




			case ConsoleField_movesBackAndForth: {
				bool undergoesGravity = affectedByGravity(entity, movementField);
				bool shouldPatrol = !doingOtherAction && dt > 0 && (isSet(entity, EntityFlag_grounded) || !undergoesGravity); 

				canMoveSpotLight = false;

				if (shouldPatrol) {
					bool shouldChangeDirection = tryPatrolMove(entity, gameState, xMoveAcceleration, dt, ddP, undergoesGravity);

					if (shouldChangeDirection) {
						if(toggleEntityFacingDirection(entity, gameState)) {
							shouldChangeDirection = tryPatrolMove(entity, gameState, xMoveAcceleration, dt, ddP, undergoesGravity);
							
							if (shouldChangeDirection) {
								toggleFlags(entity, EntityFlag_facesLeft);
							} else {
								entity->spotLightAngle = yReflectRad(entity->spotLightAngle);
							}
						}
					}
				} else {
					defaultMove = true;

					if(entity->groundNormal.x && entity->groundNormal.y) {
						V2 ground = normalize(normalize(perp(entity->groundNormal)));
						V2 velocity = hadamard(getVelocity(dt, entity->dP, ddP), ground);

						//if(velocity.y < 0) velocity *= -1;

						velocity *= (2.0 / (dt * dt));

						double velLength = length(velocity);

						if(velLength > xMoveAcceleration) {
							velocity = normalize(velocity) * velLength;
						}

						ddP += velocity;

						canMoveSpotLight = false;
					}
				}
			} break;




			case ConsoleField_seeksTarget: {
				//ConsoleField* alertnessField = getField(movementField->children, movementField->numChildren, ConsoleField_Alertness);
				//assert(alertnessField);

				//Alertness alertness = (Alertness)alertnessField->selectedIndex;
				Alertness alertness = Alertness_patrolling;

				if (!doingOtherAction && !gameState->doingInitialSim && alertness > Alertness_asleep) {
					ConsoleField* sightRadiusField = movementField->children[1];
					double sightRadius = sightRadiusField->doubleValues[sightRadiusField->selectedIndex];

					//TODO: If obstacles were taken into account, this might not actually be the closest entity
					//		Maybe something like a bfs should be used here to find the actual closest entity
					double dstToTarget;
					Entity* targetEntity = getClosestTarget(entity, gameState, &dstToTarget);
					if (targetEntity) {
						if (dstToTarget <= sightRadius && dstToTarget > 0.1) {
							V2 wayPoint = computePath(gameState, entity, targetEntity);
							moveTowardsWaypoint(entity, gameState, dt, wayPoint, xMoveAcceleration);
						}
					}
				}
			} break;


			case ConsoleField_followsWaypoints: {
				// ConsoleField* alertnessField = getField(movementField->children, movementField->numChildren, ConsoleField_Alertness);
				// assert(alertnessField);

				// Alertness alertness = (Alertness)alertnessField->selectedIndex;

				Alertness alertness = Alertness_patrolling;

				if(!doingOtherAction && alertness > Alertness_asleep) {
					Waypoint* cur = movementField->curWaypoint;
					assert(cur);

					Waypoint* prev = cur;

					while(prev->next != cur) {
						prev = prev->next;
						assert(prev);
					}

					double initialDstToWaypoint = max(length(entity->p - cur->p), length(cur->p - prev->p));

					bool canMove = true;

					if(movementField->waypointDelay > 0) {
						movementField->waypointDelay += dt;

						assert(movementField->numChildren >= 2);
						ConsoleField* delayField = movementField->children[1];
						double delay = delayField->doubleValues[delayField->selectedIndex];

						if(movementField->waypointDelay >= delay) {
							movementField->waypointDelay = 0;
						} else {
							canMove = false;
						}
					}

					if(canMove) {
						if(moveTowardsTargetParabolic(entity, gameState, dt, cur->p, initialDstToWaypoint, xMoveAcceleration)) {
							movementField->curWaypoint = cur->next;
							if (dt > 0) movementField->waypointDelay += dt;
							else movementField->waypointDelay += 0.0001;
						}
					} else {
						defaultMove = true;
					}

					V2 toWaypoint = movementField->curWaypoint->p - entity->p;

					wantedSpotLightAngle = getRad(toWaypoint);
					double percentToTarget = (initialDstToWaypoint - length(toWaypoint)) / initialDstToWaypoint; 
					double sinInput = percentToTarget * TAU;

					double angleVariance = min(75, initialDstToWaypoint * 7) / TAU;
					wantedSpotLightAngle += sin(sinInput) * angleVariance;
				}
			} break;



			case ConsoleField_bobsVertically: {
				ConsoleField* speedField = movementField->children[0];
				ConsoleField* maxBobHeightField = movementField->children[1];

				double speed = speedField->doubleValues[speedField->selectedIndex];
				double maxBobHeight = maxBobHeightField->doubleValues[maxBobHeightField->selectedIndex];

				V2 oldP = entity->p;

				double initialDstToTarget = movementField->initialBob ? maxBobHeight : maxBobHeight * 2;
				double maxBobHeightSigned = maxBobHeight * (movementField->bobbingUp * 2 - 1);
				V2 target = v2(entity->p.x, entity->p.y - movementField->bobHeight + maxBobHeightSigned);

				bool reachedTarget = moveTowardsTargetParabolic(entity, gameState, dt, target, initialDstToTarget, speed);

				double deltaHeight = entity->p.y - oldP.y;
				movementField->bobHeight += deltaHeight;

				if(reachedTarget) {
					movementField->bobbingUp = !movementField->bobbingUp;
					movementField->initialBob = false;
				} 
				else if (deltaHeight == 0 && dt) {
					movementField->bobbingUp = !movementField->bobbingUp;
					movementField->initialBob = false;
					movementField->bobHeight = 0;
				}
			} break;



			case ConsoleField_bodyguard: {
				bool setStartPos = false;

				double targetDst;
				Entity* target = getClosestGuardTarget(entity, gameState, &targetDst);

				double transitionDst = 3;
				double maxDst = 4;

				if(target && targetDst > maxDst) {
					//go to target
					double initialDstToTarget = targetDst;

					if(!epsilonEquals(entity->startPos, INVALID_START_POS, 0.001)) {
						double dst = length(entity->startPos - target->p);
						if(dst > initialDstToTarget) {
							initialDstToTarget = dst;
						}
					} else {
						setStartPos = true;
					}

					moveTowardsTargetParabolic(entity, gameState, dt, target->p, initialDstToTarget, xMoveAcceleration);
				} else {
					//wander
					setStartPos = true;

					double circleRadius = 0.7;

					V2 toCircleCenter = normalize(entity->dP);
					V2 toCircleEdge = randomUnitV2(&gameState->random) * circleRadius;

					V2 movement = toCircleCenter + toCircleEdge;
				
					if(target && targetDst > transitionDst) {
						V2 toTarget = (target->p - entity->p) * (targetDst - transitionDst);
						movement += toTarget;
					}

					ddP = normalize(movement) * xMoveAcceleration * 2;

					move(entity, dt, gameState, ddP);
				}

				if(target) wantedSpotLightAngle = getDegrees(entity->p - target->p);
				else wantedSpotLightAngle = getDegrees(entity->dP);

				if(setStartPos) {
					entity->startPos = entity->p;
				}
			} break;



			InvalidDefaultCase;
		} //end of movement field switch 

		if(gameState->screenType != ScreenType_pause && !defaultMove) {
			if(movementField->type != ConsoleField_followsWaypoints &&
			   movementField->type != ConsoleField_keyboardControlled &&
			   movementField->type != ConsoleField_bodyguard) {
				wantedSpotLightAngle = getDegrees(entity->dP);
			}

			if (!doingOtherAction && canMoveSpotLight) changeSpotLightAngle(entity, wantedSpotLightAngle, dt);
		}
	} 

	if(defaultMove) {
		if(isProjectile(entity) ||
		   (entity->type == EntityType_heavyTile && entity->startPos != entity->p)) {
			ddP.y = 0;
		} 

		if (dt > 0) {
			if(entity->type == EntityType_motherShipProjectile) {
				move(entity, dt, gameState, ddP, entity->rotation - dt * 2.0);
			} else {
				move(entity, dt, gameState, ddP);
			}
		}
	}

	if(getField(entity, ConsoleField_cameraFollows) && gameState->screenType == ScreenType_game && !getEntityByRef(gameState, gameState->consoleEntityRef)) {
		centerCameraAround(entity, gameState);
	}
}

s32 isSpawnPossible(ConsoleField* field, Entity* entity, GameState* gameState, bool trawler) {
	s32 numSpawned = countValidRefNodes(&field->spawnedEntities, gameState);
	ConsoleField* maxSpawnField = field->children[1];
	s32 maxSpawned = getS32Value(maxSpawnField);

	if(numSpawned >= maxSpawned) {
		return 0;
	}

	if(trawler) {
		
		Entity* testEntity = getTestEntity(gameState);
		initTrawler(gameState, testEntity, entity->p);
		V2 delta = v2(0, -gameState->mapSize.y * 2);

		GetCollisionTimeResult collisionResult = getCollisionTime(testEntity, gameState, delta, false);
		releaseTestEntity(gameState, testEntity);

		if(!collisionResult.solidEntity) {
			return 0;
		}
	}

	return maxSpawned - numSpawned;
}

void updateAndRenderEntities(GameState* gameState, double dtForFrame) {
	bool hacking = getEntityByRef(gameState, gameState->consoleEntityRef) != NULL;

	// {
	// 	Entity* player = getEntityByRef(gameState, gameState->playerRef);
	// 	if(player && player->currentAnim == gameState->playerHack) hacking = true;
	// }

	bool shouldDrawCollisionBounds = getEntityByRef(gameState, gameState->consoleEntityRef) != NULL;

	double collisionBoundsFadeTime = 0.25;
	double collisionBoundsFadeAmt = dtForFrame / collisionBoundsFadeTime;

	if(shouldDrawCollisionBounds) {
		gameState->collisionBoundsAlpha = min(1, gameState->collisionBoundsAlpha + collisionBoundsFadeAmt);
	} else {
		gameState->collisionBoundsAlpha = max(0, gameState->collisionBoundsAlpha - collisionBoundsFadeAmt);
	}

	double dtForPlayer = hacking ? 0 : dtForFrame;
	double dtForEntities = dtForPlayer * getDoubleValue(gameState->timeField);

	if(!gameState->doingInitialSim) {
		//TODO: It might be possible to combine the three loops which handle ground reference lists later
		//TODO: An entities ground reference list could be reset right after it is done being updated and rendered
		//NOTE: This is to reset all of the entities ground reference lists
		for (s32 entityIndex = 0; entityIndex < gameState->numEntities; entityIndex++) {
			Entity* entity = gameState->entities + entityIndex;

			entity->timeSinceLastOnGround += dtForEntities;
			clearFlags(entity, EntityFlag_grounded|EntityFlag_movedByGround);

			//NOTE: A ground reference between the base and the beam always persists.
			if(entity->type == EntityType_laserBase) {
				assert(entity->groundReferenceList);
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
		for (s32 entityIndex = 0; entityIndex < gameState->numEntities; entityIndex++) {
			Entity* entity = gameState->entities + entityIndex;

			Entity* above = getAbove(entity, gameState);

			if (above) {
				addGroundReference(above, entity, gameState);
			}

			GetCollisionTimeResult belowResult = onGround(entity, gameState);

			if (belowResult.solidEntity) {
				addGroundReference(entity, belowResult.solidEntity, gameState);
				entity->groundNormal = belowResult.solidCollisionNormal;
			} else {
				entity->groundNormal = v2(0, 0);
			}
		}
	} else {
		for (s32 entityIndex = 0; entityIndex < gameState->numEntities; entityIndex++) {
			Entity* entity = gameState->entities + entityIndex;
			clearFlags(entity, EntityFlag_movedByGround);
		}
	}

	double frictionGroundCoefficient = -15.0;

	double groundFrictionForPlayer = pow(E, frictionGroundCoefficient * dtForEntities);
	double groundFrictionForEntities = pow(E, frictionGroundCoefficient * dtForPlayer);

	//NOTE: This loops through all of the entities in the game state to update and render them
	for (s32 entityIndex = 0; entityIndex < gameState->numEntities; entityIndex++) {
		Entity* entity = gameState->entities + entityIndex;

		double dt, groundFriction;

		if(entity->type == EntityType_player || (entity->type == EntityType_death && entity->drawOrder == DrawOrder_player)) {
			dt = dtForPlayer;
			groundFriction = groundFrictionForPlayer;
		}
		else {
			dt = dtForEntities;
			groundFriction = groundFrictionForEntities;
		}

		removeFieldsIfSet(entity->fields, &entity->numFields);

		ConsoleField* spotLightField = NULL;
		ConsoleField* scanField = NULL;
		ConsoleField* shootField = NULL;
		ConsoleField* cloakField = NULL;
		ConsoleField* spawnTrawlersField = NULL;
		ConsoleField* spawnShrikesField = NULL;

		s32 fieldIndexStart = 0;
		if(entity->type == EntityType_pickupField) fieldIndexStart = 1;

		for(s32 fieldIndex = fieldIndexStart; fieldIndex < entity->numFields; fieldIndex++) {
			ConsoleField* field = entity->fields[fieldIndex];

			switch(field->type) {
				case ConsoleField_spotlight: {
					spotLightField = field;
				} break;

				case ConsoleField_scansForTargets: {
					scanField = field;
				} break;

				case ConsoleField_shootsAtTarget: {
					shootField = field;
				} break;

				case ConsoleField_cloaks: {
					cloakField = field;
				} break;

				case ConsoleField_spawnsTrawlers: {
					spawnTrawlersField = field;
				} break;

				case ConsoleField_spawnsShrikes: {
					spawnShrikesField = field;
				} break;

				case ConsoleField_light: {
					ConsoleField* enabledField = field->children[1];

					if(enabledField->selectedIndex) {
						ConsoleField* radiusField = field->children[0];
						double radius = getDoubleValue(radiusField);

					#if ENABLE_LIGHTING
						PointLight light = createPointLight(v3(entity->p, 0), field->lightColor * (1 - entity->cloakFactor), radius);
						pushPointLight(gameState->renderGroup, &light, true);
					#else
						Texture* circle = gameState->lightCircle;
						R2 lightBounds = rectCenterRadius(entity->p, v2(1, 1) * radius);

						V3 lightColor = field->lightColor;
						u8 r = (u8)(255 * lightColor.x);
						u8 g = (u8)(255 * lightColor.y);
						u8 b = (u8)(255 * lightColor.z);
						u8 a = (u8)(25 * (1 - entity->cloakFactor));

						Color color = createColor(r, g, b, a);
						pushTexture(gameState->renderGroup, circle, lightBounds, false, false, DrawOrder_light, true, 
									Orientation_0, color, 1);
					#endif
					}
				} break;
                    
                default: {
                } break;
			}
		}

		
		Entity* target = NULL;

		if(spotLightField) {
			target = updateSpotlightBasedOnSpotlightField(entity, gameState, spotLightField);
		} else {
			entity->targetRef = 0;
		}

		bool canMoveSpotLight = true;

		if(scanField && !target) {
			ConsoleField* scanSpeedField = scanField->children[0];
			double scanSpeed = toRadians(getDoubleValue(scanSpeedField)) * dt;

			if(scanField->decreasingScanAngle) scanSpeed *= -1;

			ConsoleField* scanFOV = scanField->children[1];
			double fov = toRadians(getS32Value(scanFOV));
			double halfFOV = fov * 0.5;

			double minAngle = scanField->scanStart - halfFOV;
			double maxAngle = scanField->scanStart + halfFOV;

			double curAngle = entity->spotLightAngle + scanSpeed;

			bool changed = false;
			curAngle = clampRadiansBetween(curAngle, minAngle, maxAngle, &changed);

			entity->spotLightAngle = curAngle;
			if(changed) {
				scanField->decreasingScanAngle = !scanField->decreasingScanAngle;
			}

			canMoveSpotLight = false;
		}

		if(!target) {
			if(shootField) {
				double targetDst;
				target = getClosestTarget(entity, gameState, &targetDst);			
				
				ConsoleField* detectRadiusField = shootField->children[1];
				double detectRadius = getDoubleValue(detectRadiusField);

				if(target && targetDst <= detectRadius) {
					entity->targetRef = target->ref;
				} else {
					target = NULL;
				}								
			}
		}

		bool shootingState = false;
		bool doingOtherAction = false;

		s32 remainingTrawlers = 0;
		s32 remainingShrikes = 0;

		if(!gameState->doingInitialSim && target && dt > 0) {
			if(spawnTrawlersField) {
				remainingTrawlers = isSpawnPossible(spawnTrawlersField, entity, gameState, true);
			}

			if(spawnShrikesField) {
				remainingShrikes = isSpawnPossible(spawnShrikesField, entity, gameState, false);
			}
		}	

		if(cloakField) {
			bool wantsToDoOtherAction = target || remainingTrawlers || remainingShrikes;

			if(wantsToDoOtherAction) {
				if(isSet(entity, EntityFlag_cloaked)) {
					setFlags(entity, EntityFlag_togglingCloak);
					clearFlags(entity, EntityFlag_cloaked);
				}
			} else {
				if(!isSet(entity, EntityFlag_cloaked)) {
					setFlags(entity, EntityFlag_cloaked|EntityFlag_togglingCloak);
				}
			}

			bool32 togglingCloak = isSet(entity, EntityFlag_togglingCloak);
			bool32 cloaked = isSet(entity, EntityFlag_cloaked);

			if(togglingCloak || cloaked) {
				shootField = NULL;
				spawnTrawlersField = NULL;
				spawnShrikesField = NULL;

				if(togglingCloak && !gameState->doingInitialSim) {
					doingOtherAction = true;

					if(cloaked) {
						ConsoleField* cloakTimeField = cloakField->children[0];
						double cloakTime = getDoubleValue(cloakTimeField);
						double cloakAmt = dt / cloakTime;
						entity->cloakFactor += cloakAmt;

						if(entity->cloakFactor >= 1) {
							entity->cloakFactor = 1;
							clearFlags(entity, EntityFlag_togglingCloak);
						}
					} else {
						ConsoleField* cloakTimeField = cloakField->children[1];
						double cloakTime = getDoubleValue(cloakTimeField);
						double cloakAmt = -dt / cloakTime;
						entity->cloakFactor += cloakAmt;

						if(entity->cloakFactor <= 0) {
							entity->cloakFactor = 0;
							clearFlags(entity, EntityFlag_togglingCloak);
						}
					}
				}
			}
		} else {
			clearFlags(entity, EntityFlag_cloaked|EntityFlag_togglingCloak);
			entity->cloakFactor = 0;
		}

		if(remainingTrawlers && remainingShrikes) {
			if(remainingTrawlers >= remainingShrikes) {
				remainingShrikes = 0;
			} else {
				remainingTrawlers = 0;
			}
		}

		ConsoleField* spawnField = NULL;

		if(remainingTrawlers) {
			spawnField = spawnTrawlersField;
		}
		else if(remainingShrikes) {
			spawnField = spawnShrikesField;
		}

		if(spawnField) {
			ConsoleField* spawnDelayField = spawnField->children[0];
			double spawnDelay = getDoubleValue(spawnDelayField);

			spawnField->spawnTimer += dt;

			if(spawnField->spawnTimer >= spawnDelay) {
				if(spawnTrawlersField) spawnTrawlersField->spawnTimer = 0;
				if(spawnShrikesField) spawnShrikesField->spawnTimer = 0;

				Entity* spawn = NULL;

				if(remainingTrawlers) {
					assert(spawnField->type == ConsoleField_spawnsTrawlers);
					spawn = addTrawlerBootUp(gameState, entity->p);
				}
				else if(remainingShrikes) {
					assert(spawnField->type == ConsoleField_spawnsShrikes);
					spawn = addShrikeBootUp(gameState, entity->p);
				}
				else {
					InvalidCodePath;
				}

				spawn->spawnerRef = entity->ref;
				spawnField->spawnedEntities = refNode(gameState, spawn->ref, spawnField->spawnedEntities);
			}

			doingOtherAction = true;
		}

		if(shootField && !doingOtherAction) {
			shootingState = shootBasedOnShootingField(entity, gameState, dt, shootField);

			if(shootingState) {
				canMoveSpotLight = false;
				doingOtherAction = true;
			}
		}

		moveEntityBasedOnMovementField(entity, gameState, dt, groundFriction, doingOtherAction, canMoveSpotLight);

		bool insideLevel = false;

		//NOTE: This removes the entity if they are outside of the world
		if (isSet(entity, EntityFlag_removeWhenOutsideLevel) && !getField(entity, ConsoleField_bobsVertically)) {
			R2 world = r2(v2(0, 0), gameState->worldSize);

			Hitbox* hitboxList = entity->hitboxes;

			while(hitboxList) {
				R2 hitbox = getBoundingBox(entity, hitboxList);

				if (rectanglesOverlap(world, hitbox)) {
					insideLevel = true;
					break;
				}

				hitboxList = hitboxList->next;
			}
		} else {
			insideLevel = true;
		}

		//NOTE: Remove entity if it is below the bottom of the world
		//The laser base and laser beams are not removed when the bottom is reached because their ground references
		//are used to pull down the laser top.
		//TODO: The laser base still needs to be removed somewhere though (once both hitboxes are outside of the level)
		if(entity->type != EntityType_laserBase) {
			if((gameState->gravity.y <= 0 && entity->p.y < -entity->renderSize.y / 2) ||
			  (gameState->gravity.y >= 0 && entity->p.y > gameState->windowSize.y + entity->renderSize.y / 2)) {
				insideLevel = false;
			}
		}

		if (!insideLevel) {
			// bool removeFields = false;

			// for(s32 fieldIndex = 0; fieldIndex < entity->numFields; fieldIndex++) {
			// 	ConsoleField* field = entity->fields[fieldIndex];

			// 	if(field->type == ConsoleField_killsOnHit || field->type == ConsoleField_givesEnergy) {
			// 		removeFields = true;
			// 		setFlags(field, ConsoleFlag_remove);
			// 	}
			// }

			// if(removeFields) removeFieldsIfSet(entity->fields, &entity->numFields);

			setFlags(entity, EntityFlag_remove);
		}

		//NOTE: Individual entity logic here
		switch(entity->type) {

			case EntityType_background: {
				#if DRAW_BACKGROUND
				BackgroundTexture* backgroundTexture = getBackgroundTexture(&gameState->backgroundTextures);
				drawBackgroundTexture(backgroundTexture, gameState->renderGroup, &gameState->camera, gameState->windowSize, gameState->mapSize.x);				
				#endif
			} break;


			case EntityType_disappearingTile:
			case EntityType_droppingTile:
			case EntityType_heavyTile:
			case EntityType_tile: {
				assert(entity->numFields >= 2);
				assert(entity->fields[0]->type == ConsoleField_unlimitedInt && 
					   entity->fields[1]->type == ConsoleField_unlimitedInt);

				s32 fieldXOffset = entity->fields[0]->selectedIndex;
				s32 fieldYOffset = entity->fields[1]->selectedIndex;

				if(getMovementField(entity)) {
					entity->tileXOffset = fieldXOffset;
					entity->tileYOffset = fieldYOffset;
				}

				s32 dXOffset = fieldXOffset - entity->tileXOffset;
				s32 dYOffset = fieldYOffset - entity->tileYOffset;

				if (dXOffset != 0 || dYOffset != 0) {
					V2 tileSize = v2(TILE_WIDTH_IN_METERS, TILE_HEIGHT_WITHOUT_OVERHANG_IN_METERS);

					V2 target = entity->startPos + hadamard(v2((double)dXOffset, (double)dYOffset), 
						tileSize/*getRectSize(entity->clickBox)*/);

					//double initialDst = dXOffset == 0 ? getRectHeight(entity->clickBox) : getRectWidth(entity->clickBox);

					double initialDstTest = dXOffset != 0 ? tileSize.x : tileSize.y;
					double initialDst = max(initialDstTest, length(target - entity->p));

					if (moveTowardsTargetParabolic(entity, gameState, dtForFrame, target, initialDst, 6.0f)) {
						entity->tileXOffset = fieldXOffset;
						entity->tileYOffset = fieldYOffset;
					}
				} else {
					entity->startPos = entity->p;
				}

			} break;



			case EntityType_text: {
				assert(entity->numFields >= 1);
				assert(entity->fields[0]->type == ConsoleField_s32);

				s32 selectedIndex = entity->fields[0]->s32Values[entity->fields[0]->selectedIndex];
				setSelectedText(entity, selectedIndex, gameState);
			} break;



			case EntityType_checkPoint: {
				ConsoleField* reachedField = getField(entity, ConsoleField_bool);
				assert(reachedField);
				bool reached = reachedField->selectedIndex != 0;
				bool storedReached = isSet(entity, EntityFlag_checkPointReached) != 0;

				if(reached != storedReached) {
					toggleCheckpoint(entity, gameState, false);
				}
			} break;



			case EntityType_laserBase: {
				assert(entity->numFields >= 1);
				assert(entity->fields[0]->type == ConsoleField_bool);

				bool laserOn = (entity->fields[0]->selectedIndex != 0);

				Entity* beam = getEntityByRef(gameState, entity->ref + 1);
				assert(!beam || beam->type == EntityType_laserBeam);

				Texture* topTexture = NULL;
				Texture* baseTexture = NULL;

				if(laserOn) {
					setFlags(entity, EntityFlag_laserOn);

					if (beam) setFlags(beam, EntityFlag_laserOn);

					topTexture = gameState->laserImages.topOn;
					baseTexture = gameState->laserImages.baseOn;
				}
				else {
					clearFlags(entity, EntityFlag_laserOn);

					if (beam) clearFlags(beam, EntityFlag_laserOn);

					topTexture = gameState->laserImages.topOff;
					baseTexture = gameState->laserImages.baseOff;
				}
	
				assert(topTexture && baseTexture);

				R2 baseBounds = rectCenterDiameter(entity->p, entity->renderSize);

				//NOTE: entity->hitboxes->collisionOffset is the height of the laser entity
				R2 topBounds = translateRect(baseBounds, entity->hitboxes->collisionOffset);

				pushTexture(gameState->renderGroup, topTexture, topBounds, false, false, entity->drawOrder, 
							true, Orientation_0, createColor(255, 255, 255, 255), entity->emissivity);
				pushTexture(gameState->renderGroup, baseTexture, baseBounds, false, false, entity->drawOrder,
							 true, Orientation_0, createColor(255, 255, 255, 255), entity->emissivity);

				//pushFilledRect(gameState->renderGroup, topBounds, createColor(255, 0, 0, 100), true);
				//pushFilledRect(gameState->renderGroup, baseBounds, createColor(255, 0, 0, 100), true);
			} break;



			case EntityType_pickupField: {
				double secondsToFadeOut = 10;
				double fadePerSecond = 1.0 / secondsToFadeOut;
				double fade = dt * fadePerSecond;

				entity->alpha -= fade;

				if(entity->alpha <= 0) {
					entity->alpha = 0;
					setFlags(entity, EntityFlag_remove);
				}

				//NOTE: If the pickupField has been removed and the field is transferred to the swap field
				//		then it will have 1 field (the solid field)
				if(entity->numFields >= 1 && !isSet(entity, EntityFlag_remove)) {
					bool32 wasSolid = isSet(entity, EntityFlag_wasSolid);
					if(isPickupFieldSolid(entity) != wasSolid) {
						toggleFlags(entity, EntityFlag_wasSolid);

						if(!wasSolid) ignoreAllPenetratingEntities(entity, gameState);
					}

					if(gameState->consoleEntityRef != entity->ref) {
						entity->fields[0]->p = entity->p;
						pushConsoleField(gameState->renderGroup, &gameState->fieldSpec, entity->fields[0], entity->alpha);
					}
				}
			} break;

			case EntityType_trojan: {
				entity->rotation = entity->spotLightAngle - PI / 2.0;
			} break;

			// case EntityType_motherShip: {
			// 	entity->rotation = entity->spotLightAngle + PI / 2.0;
			// } break;

            default: {
            } break;
		} //End of switch statement on entity type



		ConsoleField* disappearField = getField(entity, ConsoleField_disappearsOnHit);
		if(disappearField) {
			ConsoleField* timeField = disappearField->children[0];
			double timeToDisappear = timeField->doubleValues[timeField->selectedIndex];
			double rate = dt / timeToDisappear;

			if(entity->groundReferenceList) {
				entity->alpha -= rate;

				if(entity->alpha <= 0) {
					entity->alpha = 0;
					setFlags(entity, EntityFlag_remove);
				}
			}
		}



		//NOTE: This handles rendering all the entities
		bool showPlayerHackingAnim = entity->type == EntityType_player && getEntityByRef(gameState, gameState->consoleEntityRef);
		CharacterAnim* characterAnim = entity->characterAnim;

		if (entity->type == EntityType_player && hacking) {
			entity->animTime += dtForFrame;
		} 
		else if(entity->type == EntityType_motherShipProjectileDeath) {
			entity->animTime -= dt * 2;

			if(entity->animTime < 0) {
				entity->animTime = 0;
				setFlags(entity, EntityFlag_remove);

				addDeath(gameState, entity->p, entity->renderSize, entity->drawOrder, entity->rotation,
						 gameState->motherShipImages.projectileDeath);
			}
		} 
		else {
			entity->animTime += dt;
		}

		bool fadeAlphaFromDisappearing = isSet(entity, EntityFlag_togglingCloak) && 
										   (!characterAnim || !characterAnim->disappear);

		if(characterAnim) {
			if(entity->type == EntityType_death) {
				entity->currentAnim = characterAnim->death;
				clearFlags(entity, EntityFlag_animIntro|EntityFlag_animOutro);

				AnimNode* deathAnimNode = characterAnim->death;
				assert(deathAnimNode);

				Animation* deathAnim = &deathAnimNode->main;

				double duration = getAnimationDuration(deathAnim);

				if(entity->animTime > duration) {
					entity->animTime = duration - deathAnim->secondsPerFrame / 2;

					setFlags(entity, EntityFlag_remove);

					if(entity->drawOrder == DrawOrder_player) {
						gameState->reloadCurrentLevel = true;
					}
				}
			} else {
				bool32 disappearing = false;

				if(isSet(entity, EntityFlag_togglingCloak)) {
					if(characterAnim->disappear) {
						entity->currentAnim = entity->nextAnim = characterAnim->disappear;
						clearFlags(entity, EntityFlag_animIntro|EntityFlag_animOutro);
						disappearing = true;

						double duration = getAnimationDuration(&entity->currentAnim->main);
						entity->animTime = duration * entity->cloakFactor;

						if(!isSet(entity, EntityFlag_cloaked)) entity->animTime = duration - entity->animTime;
					}
				}

				if(!disappearing) {
					if(isSet(entity, EntityFlag_animIntro)) {
						assert(!isSet(entity, EntityFlag_animOutro));
						assert(entity->currentAnim);

						AnimNode* currentAnim = entity->currentAnim;
						assert(currentAnim);
						assert(currentAnim->intro.frames);
						double duration = getAnimationDuration(&currentAnim->intro);

						if(entity->animTime >= duration) {
							clearFlags(entity, EntityFlag_animIntro);
							entity->animTime -= duration;
						}
					}

					else if(isSet(entity, EntityFlag_animOutro)) {
						assert(entity->currentAnim && entity->nextAnim);

						AnimNode* currentAnim = entity->currentAnim;
						assert(currentAnim->outro.frames);

						double duration = getAnimationDuration(&currentAnim->outro);

						if(entity->animTime >= duration) {
							clearFlags(entity, EntityFlag_animOutro);
							entity->animTime -= duration;
							entity->currentAnim = entity->nextAnim;
						}
					}


					if(showPlayerHackingAnim) {
						entity->nextAnim = gameState->playerHack;
					}
					else if(shootingState && characterAnim->shoot) {
						entity->nextAnim = characterAnim->shoot;
					}
					else if(isSet(entity, EntityFlag_jumped) && characterAnim->jump) {
						entity->nextAnim = characterAnim->jump;
					}
					else if(fabs(entity->dP.x) > 0.1f && characterAnim->walk) {
						entity->nextAnim = characterAnim->walk;
					}
					else if (characterAnim->stand) {
						entity->nextAnim = characterAnim->stand;
					}

					if(entity->nextAnim == entity->currentAnim) {
						entity->nextAnim = NULL;
					}

					if (entity->nextAnim) {
						if(isSet(entity, EntityFlag_animIntro)) {
							assert(entity->currentAnim);
							AnimNode* currentAnim = entity->currentAnim;
							assert(currentAnim);
							assert(currentAnim->intro.frames);
							if(currentAnim->intro.frames &&
							    currentAnim->intro.frames == currentAnim->outro.frames) {
								clearFlags(entity, EntityFlag_animIntro);
								setFlags(entity, EntityFlag_animOutro);
								entity->animTime = getAnimationDuration(&currentAnim->intro) - entity->animTime;
							}
						} else if(!isSet(entity, EntityFlag_animOutro)) {
							bool transitionToOutro = true;
							bool transitionToNext = (entity->nextAnim == characterAnim->jump);

							if(entity->currentAnim){
								AnimNode* currentAnim = entity->currentAnim;
								assert(currentAnim);

								if(currentAnim->finishMainBeforeOutro) {
									transitionToOutro = (entity->animTime >= getAnimationDuration(&currentAnim->main));
								}
							}

							if(transitionToOutro) {
								entity->animTime = 0;

								if(entity->currentAnim) {
									AnimNode* currentAnim = entity->currentAnim;
									assert(currentAnim);

									if(currentAnim->outro.numFrames) {
										setFlags(entity, EntityFlag_animOutro);
									} else {
										transitionToNext = true;
									}
								}
								else {
									transitionToNext = true;
								}
							}

							if(transitionToNext) {
								clearFlags(entity, EntityFlag_animOutro);
								entity->animTime = 0;
								entity->currentAnim = entity->nextAnim;
								assert(entity->currentAnim);

								AnimNode* currentAnim = entity->currentAnim;
								assert(currentAnim);

								if(currentAnim->intro.frames) {
									setFlags(entity, EntityFlag_animIntro);
								} else {
									clearFlags(entity, EntityFlag_animIntro);
								}
							}
						}
					} else {
						clearFlags(entity, EntityFlag_animOutro);
					}

					AnimNode* currentAnim = entity->currentAnim;

					if((!entity->nextAnim || !entity->currentAnim || !currentAnim->outro.frames) && 
						isSet(entity, EntityFlag_animOutro)) {
						InvalidCodePath;
					}

					if((!entity->currentAnim || !currentAnim->intro.frames) && 
						isSet(entity, EntityFlag_animIntro)) {
						InvalidCodePath;
					}
				}
			}
		}

		//NOTE: This can disable the player's animation
		#if 0
		if (entity->type == EntityType_player) {
			entity->currentAnim = characterAnim->standAnim;
			clearFlags(entity, EntityFlag_animIntro|EntityFlag_animOutro);
		}
		#endif

		Texture* texture = NULL;

		if(entity->currentAnim) {
			Animation* anim = NULL;

			AnimNode* currentAnim = entity->currentAnim;
			assert(currentAnim);

			if(isSet(entity, EntityFlag_animIntro)) {
				anim = &currentAnim->intro;
			}
			else if(isSet(entity, EntityFlag_animOutro)) {
				anim = &currentAnim->outro;
			}
			else {
				anim = &currentAnim->main;
			}

			assert(anim);
			texture = getAnimationFrame(anim, entity->animTime);

			assert(validTexture(texture));
		}
		else if(entity->defaultTex) {
			texture = entity->defaultTex;
		}
		else if(entity->messages) {
			texture = entity->messages->textures + entity->messages->selectedIndex;
		} else if(entity->type == EntityType_hackEnergy) {
			texture = getAnimationFrame(&gameState->hackEnergyAnim, entity->animTime);
		} else if(entity->type == EntityType_motherShipProjectile) {
			texture = getAnimationFrame(&gameState->motherShipImages.projectileMoving, entity->animTime);
		} 
		else if(entity->type == EntityType_motherShipProjectileDeath) {
			texture = getAnimationFrame(&gameState->motherShipImages.projectileMoving, entity->animTime);
		}
		else if(entity->type == EntityType_bootUp) {
			Animation* anim = NULL;

			if(entity->drawOrder == DrawOrder_trawler) {
				anim = &gameState->trawlerImages.bootUp;
			}
			else if(entity->drawOrder == DrawOrder_shrike) {
				anim = &gameState->shrikeBootUp;
			} 
			else {
				InvalidCodePath;
			}

			double duration = getAnimationDuration(anim);

			if(gameState->doingInitialSim) {
				entity->animTime = 0;
			}
			else if(entity->animTime < duration) {
				texture = getAnimationFrame(anim, entity->animTime);
			} 
			else if (!isSet(entity, EntityFlag_remove)) {
				Entity* spawn = NULL;

				if(entity->drawOrder == DrawOrder_trawler) {
					spawn = addTrawler(gameState, entity->p);
				}
				else if(entity->drawOrder == DrawOrder_shrike) {
					spawn = addShrike(gameState, entity->p);
				} 
				else {
					InvalidCodePath;
				}

				for(s32 fieldIndex = 0; fieldIndex < entity->numFields; fieldIndex++) {
					addDeferredField(spawn, entity->fields[fieldIndex], gameState);
					entity->fields[fieldIndex] = NULL;
				}

				entity->numFields = 0;

				Entity* spawner = getEntityByRef(gameState, entity->spawnerRef);

				if(spawner) {
					spawn->spawnerRef = entity->spawnerRef;
					
					ConsoleField* spawnerField = NULL;

					if(spawn->type == EntityType_trawler) {
						spawnerField = getField(spawner, ConsoleField_spawnsTrawlers);
					}
					else if(spawn->type == EntityType_shrike) {
						spawnerField = getField(spawner, ConsoleField_spawnsShrikes);
					}
					else {
						InvalidCodePath;
					}
					
					assert(spawnerField);

					RefNode* node = spawnerField->spawnedEntities;
					bool updatedNode = false;

					while(node) {
						if(node->ref == entity->ref) {
							node->ref = spawn->ref;
							updatedNode = true;
							break;
						}						

						node = node->next;
					}

					assert(updatedNode);
				}

				setFlags(entity, EntityFlag_remove);
			}
		}

		#if DRAW_ENTITIES

		if(entity->type == EntityType_shrike) {
			Texture* tex = getAnimationFrame(&gameState->shrikeStand, entity->animTime);
			pushEntityTexture(gameState->renderGroup, tex, entity, entity->drawOrder, fadeAlphaFromDisappearing);
		}

		if (texture != NULL) {
			assert(texture->texId);
			pushEntityTexture(gameState->renderGroup, texture, entity, entity->drawOrder, fadeAlphaFromDisappearing);
		}

		if(entity->glowingTex) {
			DrawOrder drawOrder = entity->drawOrder;


			float emissivity = entity->emissivity;

			entity->emissivity = 0;
			pushEntityTexture(gameState->renderGroup, entity->glowingTex->regular, entity, drawOrder, fadeAlphaFromDisappearing);
			entity->emissivity = (float)((sin(entity->animTime) + 1.0) * 0.5);
			pushEntityTexture(gameState->renderGroup, entity->glowingTex->glowing, entity, drawOrder, fadeAlphaFromDisappearing);

			entity->emissivity = emissivity;
		}

		if(entity->type == EntityType_motherShip) {
			double rotation = entity->rotation;

			MotherShipImages* images = &gameState->motherShipImages;

			pushEntityTexture(gameState->renderGroup, images->emitter, entity, DrawOrder_motherShip_0, fadeAlphaFromDisappearing);
			pushEntityTexture(gameState->renderGroup, images->base, entity, DrawOrder_motherShip_1, fadeAlphaFromDisappearing);

			if(shootingState) {
				Animation* shootAnim = &images->spawning;
				double duration = getAnimationDuration(shootAnim);
				double animTime = duration * shootField->shootTimer;

				Texture* shootTex = getAnimationFrame(shootAnim, animTime);
				pushEntityTexture(gameState->renderGroup, shootTex, entity, DrawOrder_motherShip_5, fadeAlphaFromDisappearing);
			}

			entity->rotation = 0.5 * entity->animTime;
			pushEntityTexture(gameState->renderGroup, images->rotators[0], entity, DrawOrder_motherShip_2, fadeAlphaFromDisappearing);

			entity->rotation = -1 * entity->animTime;
			pushEntityTexture(gameState->renderGroup, images->rotators[1], entity, DrawOrder_motherShip_3, fadeAlphaFromDisappearing);

			entity->rotation = 1.5 * entity->animTime;
			pushEntityTexture(gameState->renderGroup, images->rotators[2], entity, DrawOrder_motherShip_4, fadeAlphaFromDisappearing);

			entity->rotation = rotation;
		}
		else if(entity->type == EntityType_trawler) {
			double rotation = entity->rotation;

			TrawlerImages* images = &gameState->trawlerImages;

			pushEntityTexture(gameState->renderGroup, images->frame, entity, DrawOrder_trawler_0, fadeAlphaFromDisappearing);
			pushEntityTexture(gameState->renderGroup, images->body, entity, DrawOrder_trawler_2, fadeAlphaFromDisappearing);

			if(shootingState) {
				Animation* shootAnim = &images->shoot;
				double duration = getAnimationDuration(shootAnim);
				double animTime = duration * shootField->shootTimer;

				Texture* shootTex = getAnimationFrame(shootAnim, animTime);
				pushEntityTexture(gameState->renderGroup, shootTex, entity, DrawOrder_trawler_3, fadeAlphaFromDisappearing);
			}

			entity->rotation = entity->wheelRotation;
			pushEntityTexture(gameState->renderGroup, images->wheel, entity, DrawOrder_trawler_1, fadeAlphaFromDisappearing);

			entity->rotation = rotation;
		}

		#endif

		double collisionBoundsAlpha = gameState->collisionBoundsAlpha;
		if((isTileType(entity) && getMovementField(entity) == NULL && gameState->fieldSpec.hackAbilities.moveTiles) 
			|| entity->type == EntityType_player || 
			entity->ref != gameState->consoleEntityRef) collisionBoundsAlpha = 0;

		#if SHOW_COLLISION_BOUNDS
			shouldDrawCollisionBounds = true;
			collisionBoundsAlpha = 1;
		#endif
		
		if(collisionBoundsAlpha > 0) {
			drawCollisionBounds(entity, gameState->renderGroup, collisionBoundsAlpha);
		}

		#if SHOW_CLICK_BOUNDS
			if(isSet(entity, EntityFlag_hackable)) {
				R2 clickBox = translateRect(entity->clickBox, entity->p);
				pushOutlinedRect(gameState->renderGroup, clickBox, 0.02f, createColor(127, 255, 255, 255), true);
			}
		#endif
	}
}
