void giveEntityRectangularCollisionBounds(Entity* entity, GameState* gameState,
										  double xOffset, double yOffset, double width, double height) {
	Hitbox* hitbox = (Hitbox*)pushStruct(&gameState->permanentStorage, Hitbox);

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

void freeConsoleField(ConsoleField* field, GameState* gameState) {
	if (field->next) {
		freeConsoleField(field->next, gameState);
		field->next = NULL;
	}

	for (int childIndex = 0; childIndex < field->numChildren; childIndex++) {
		freeConsoleField(field->children[childIndex], gameState);
		field->children[childIndex] = NULL;
	}

	if (field->children) {
		free(field->children);
		field->children = NULL;
	}

	field->next = gameState->consoleFreeList;
	gameState->consoleFreeList = field;
}

void freeEntity(Entity* entity, GameState* gameState) {
	switch(entity->type) {
		case EntityType_text: {
			SDL_DestroyTexture(entity->texture->tex);
			//TODO: A free list could be used here instead
			free(entity->texture);
		} break;
	}

	for (int fieldIndex = 0; fieldIndex < entity->numFields; fieldIndex++) {
		freeConsoleField(entity->fields[fieldIndex], gameState);
	}
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


			//TODO: size_t?
			//NOTE: EntityReference's which are inside the entityRefs_ array should not be
			//		added to the free list. This checks the address of an entity reference before 
			//		adding it to the free list to ensure that it is not part of the table.
			uint bucketAddress = (uint)bucket;
			uint entityRefsStartAddress = (uint)&gameState->entityRefs_;
			uint entityRefsEndAddress = entityRefsStartAddress + sizeof(gameState->entityRefs_);

			if (bucketAddress < entityRefsStartAddress || bucketAddress >= entityRefsEndAddress) {
				//Only adds if outside of the address space of the table
				bucket->next = gameState->entityRefFreeList;
				gameState->entityRefFreeList = bucket;
			}
			
			bucket->entity = NULL;

			for (int srcEntityIndex = entityIndex + 1; srcEntityIndex < gameState->numEntities; srcEntityIndex++) {
				Entity* src = gameState->entities + srcEntityIndex;
				Entity* dst = gameState->entities + srcEntityIndex - 1;
				*dst = *src;
				getPreciseEntityReferenceBucket(gameState, dst->ref)->entity--;
			}

			gameState->numEntities--;
			entityIndex--;

			gameState->entities[gameState->numEntities] = {};
		}
	}
}

Entity* addEntity(GameState* gameState, EntityType type, DrawOrder drawOrder, V2 p, V2 renderSize) {
	assert(gameState->numEntities < arrayCount(gameState->entities));

	Entity* result = gameState->entities + gameState->numEntities;

	for (int testIndex = 0; testIndex < gameState->numEntities; testIndex++) {
		Entity* testEntity = gameState->entities + testIndex;

		if (testEntity->drawOrder >= drawOrder) {
			result = testEntity;

			for (int moveSrcIndex = gameState->numEntities - 1; moveSrcIndex >= testIndex; moveSrcIndex--) {
				Entity* src = gameState->entities + moveSrcIndex;
				Entity* dst = gameState->entities + moveSrcIndex + 1;
				*dst = *src;
				getPreciseEntityReferenceBucket(gameState, dst->ref)->entity++;
			}

			break;
		}
	}

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
			bucket->next = (EntityReference*) pushStruct(&gameState->permanentStorage, EntityReference);
		}

		bucket = bucket->next;
		bucket->next = NULL;
	}

	bucket->entity = result;

	gameState->numEntities++;
	gameState->refCount++;

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

ConsoleField* createField(GameState* gameState) {
	ConsoleField* result = NULL;

	if (gameState->consoleFreeList) {
		result = gameState->consoleFreeList;
		gameState->consoleFreeList = gameState->consoleFreeList->next;
		result->next = NULL;
	} else {
		result = (ConsoleField*)pushStruct(&gameState->permanentStorage, ConsoleField);
	}

	return result;
}

ConsoleField* createField(GameState* gameState, ConsoleField* field) {
	ConsoleField* result = createField(gameState);
	*result = *field;
	return result;
}

void addField(Entity* entity, GameState* gameState, ConsoleField* field) {
	entity->fields[entity->numFields] = field;
	entity->numFields++;
	assert(arrayCount(entity->fields) > entity->numFields);
}

void addChildrenToConsoleField(ConsoleField* field, int numChildren) {
	field->children = (ConsoleField**)malloc(numChildren * sizeof(ConsoleField*));
	field->numChildren = numChildren;
}

ConsoleField* createKeyboardField(GameState* gameState) {
	ConsoleField* result = createField(gameState, &gameState->keyboardControlledField);
	addChildrenToConsoleField(result, 3);
	
	result->children[0] = createField(gameState, &gameState->keyboardSpeedField);
	result->children[1] = createField(gameState, &gameState->keyboardJumpHeightField);
	result->children[2] = createField(gameState, &gameState->keyboardDoubleJumpField);

	return result;
}

ConsoleField* createPatrolField(GameState* gameState) {
	ConsoleField* result = createField(gameState, &gameState->movesBackAndForthField);
	addChildrenToConsoleField(result, 1);
	
	result->children[0] = createField(gameState, &gameState->patrolSpeedField);

	return result;
}

ConsoleField* createSeekTargetField(GameState* gameState) {
	ConsoleField* result = createField(gameState, &gameState->seeksTargetField);
	addChildrenToConsoleField(result, 2);
	
	result->children[0] = createField(gameState, &gameState->seekTargetSpeedField);
	result->children[1] = createField(gameState, &gameState->seekTargetRadiusField);

	return result;
}

ConsoleField* createShootField(GameState* gameState) {
	ConsoleField* result = createField(gameState, &gameState->shootsAtTargetField);
	addChildrenToConsoleField(result, 2);
	
	result->children[0] = createField(gameState, &gameState->bulletSpeedField);
	result->children[1] = createField(gameState, &gameState->shootRadiusField);

	return result;
}

Entity* addPlayer(GameState* gameState, V2 p) {
	Entity* result = addEntity(gameState, EntityType_player, DrawOrder_player, p, v2(1.4f, 1.4f));

	assert(!gameState->shootTargetRef);
	gameState->shootTargetRef = result->ref;
	assert(!gameState->playerRef);
	gameState->playerRef = result->ref;

	giveEntityRectangularCollisionBounds(result, gameState, 0, result->renderSize.y * -0.01,
										 result->renderSize.x * 0.4, result->renderSize.y);

	result->texture = &gameState->playerStand;

	setFlags(result, EntityFlag_facesLeft|
					 EntityFlag_hackable);

	addField(result, gameState, createKeyboardField(gameState));
	addField(result, gameState, createField(gameState, &gameState->isShootTargetField));

	result->p += result->renderSize / 2;

	return result;
}

Entity* addVirus(GameState* gameState, V2 p) {
	Entity* result = addEntity(gameState, EntityType_virus, DrawOrder_virus, p, v2(2.5, 2.5));

	giveEntityRectangularCollisionBounds(result, gameState, -0.025f, -0.1f,
										 result->renderSize.x * 0.55f, result->renderSize.y * 0.75f);

	result->texture = &gameState->virus1Stand;

	setFlags(result, EntityFlag_facesLeft|
					 EntityFlag_hackable);

	addField(result, gameState, createPatrolField(gameState));
	addField(result, gameState, createShootField(gameState));

	result->p += result->renderSize / 2;

	return result;
}

Entity* addEndPortal(GameState* gameState, V2 p) {
	Entity* result = addEntity(gameState, EntityType_endPortal, DrawOrder_endPortal, p, v2(1, 2));

	giveEntityRectangularCollisionBounds(result, gameState, 0, 0,
										 result->renderSize.x * 0.4f, result->renderSize.y * 0.95f);

	result->texture = &gameState->endPortal;

	result->p += result->renderSize / 2;

	return result;
}

Entity* addBlueEnergy(GameState* gameState, V2 p) {
	Entity* result = addEntity(gameState, EntityType_blueEnergy, DrawOrder_blueEnergy, p, v2(0.8f, 0.8f));

	giveEntityRectangularCollisionBounds(result, gameState, 0, 0, 
										 result->renderSize.x * 0.5f, result->renderSize.y * 0.65f);

	result->texture = &gameState->blueEnergy;

	setFlags(result, EntityFlag_hackable);

	result->p += result->renderSize / 2;

	return result;
}

Entity* addBackground(GameState* gameState) {
	Entity* result = addEntity(gameState, EntityType_background, DrawOrder_background, 
								  v2(0, 0), v2(gameState->mapSize.x, 
								(double)gameState->windowHeight / (double)gameState->pixelsPerMeter));

	result->p = result->renderSize / 2.0f;
	setFlags(result, EntityFlag_noMovementByDefault);

	return result;
}

Entity* addTile(GameState* gameState, V2 p, Texture* texture) {
	Entity* result = addEntity(gameState, EntityType_tile, DrawOrder_tile, p, v2(gameState->tileSize, gameState->tileSize));

	giveEntityRectangularCollisionBounds(result, gameState, 0, 0, 
										 result->renderSize.x, result->renderSize.y);

	result->texture = texture;

	setFlags(result, EntityFlag_noMovementByDefault|
					 EntityFlag_hackable|
					 EntityFlag_removeWhenOutsideLevel);

	addField(result, gameState, createField(gameState, &gameState->tileXOffsetField));
	addField(result, gameState, createField(gameState, &gameState->tileYOffsetField));

	return result;
}

Entity* addText(GameState* gameState, V2 p, char* msg) {
	Entity* result = addEntity(gameState, EntityType_text, DrawOrder_text, p, v2(0, 0));
	result->texture = (Texture*)malloc(sizeof(Texture));
	*(result->texture) = createText(gameState, gameState->textFont, msg);
	result->renderSize = v2((double)result->texture->srcRect.w, (double)result->texture->srcRect.h) / gameState->pixelsPerMeter;
	
	giveEntityRectangularCollisionBounds(result, gameState, 0, 0, 
										 result->renderSize.x, result->renderSize.y);

	setFlags(result, EntityFlag_noMovementByDefault|
					 EntityFlag_hackable);

	return result;
}

Entity* addLaserBolt(GameState* gameState, V2 p, V2 target, int shooterRef) {
	Entity* result = addEntity(gameState, EntityType_laserBolt, DrawOrder_laserBolt, p, v2(0.8f, 0.8f));
	result->texture = &gameState->laserBolt;

	giveEntityRectangularCollisionBounds(result, gameState, 0, 0, 
										 result->renderSize.x * 0.3f, result->renderSize.y * 0.3f);

	double speed = 3.0f;
	result->dP = normalize(target - p) * speed;
	result->shooterRef = shooterRef;

	setFlags(result, EntityFlag_ignoresFriction|
					 EntityFlag_ignoresGravity|
					 EntityFlag_removeWhenOutsideLevel);
					 
	return result;
}

Entity* addLaserPiece(GameState* gameState, V2 p, bool base) {
	V2 renderSize = v2(1.5, 1.5);

	Entity* result = NULL;
	double collisionSizeY = 0, collisionOffsetY = 0;

	if (base) {
		result = addEntity(gameState, EntityType_laserBase, DrawOrder_laserBase, p, v2(1.5, 1.5));
		result->texture = &gameState->laserBaseOff;
		collisionSizeY = result->renderSize.y * 0.35;
	} else {
		result = addEntity(gameState, EntityType_laserTop, DrawOrder_laserTop, p, v2(1.5, 1.5));
		result->texture = &gameState->laserTopOff;
		collisionOffsetY = result->renderSize.y * -0.02;
		collisionSizeY = result->renderSize.y * 0.42;
	}


	giveEntityRectangularCollisionBounds(result, gameState, result->renderSize.x * 0.01, collisionOffsetY, 
												 result->renderSize.x * 0.5, collisionSizeY);

	setFlags(result, EntityFlag_noMovementByDefault|
					 EntityFlag_ignoresFriction|
					 EntityFlag_ignoresGravity);

	return result;
}

Entity* addLaserBeam(GameState* gameState, V2 p, V2 renderSize) {
	Entity* result = addEntity(gameState, EntityType_laserBeam, DrawOrder_laserBeam, p, renderSize);
	result->texture = &gameState->laserBeam;

	giveEntityRectangularCollisionBounds(result, gameState, 0, 0, 
									 	 result->renderSize.x * 0.8, result->renderSize.y);

	setFlags(result, EntityFlag_noMovementByDefault|
					 EntityFlag_ignoresFriction|
					 EntityFlag_ignoresGravity);

	return result;
}

void addGroundReference(Entity* top, Entity* ground, GameState* gameState) {
	RefNode* node = ground->groundReferenceList;

	while(node) {
		if (node->ref == top->ref) return;
		node = node->next;
	}


	RefNode* append = NULL;

	if (gameState->refNodeFreeList) {
		append = gameState->refNodeFreeList;
		gameState->refNodeFreeList = gameState->refNodeFreeList->next;
		append->next = NULL;
	} else {
		append = (RefNode*)pushStruct(&gameState->permanentStorage, RefNode);
	}

	assert(append);

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

Entity* addLaserController(GameState* gameState, V2 baseP, double height) {
	Entity* result = addEntity(gameState, EntityType_laserController, DrawOrder_laserController, baseP, v2(0, 0));

	Entity* base = addLaserPiece(gameState, baseP, true);
	V2 topP = baseP + v2(0, height);
	Entity* top = addLaserPiece(gameState, topP, false);
					 
	result->renderSize = (base->hitboxes->collisionSize + base->hitboxes->collisionOffset) / 2.0 + 
						 (top->hitboxes->collisionSize + top->hitboxes->collisionOffset) / 2.0 + 
						 v2(0, topP.y - baseP.y);

	result->p = (baseP + topP) / 2;

	//giveEntityRectangularCollisionBounds(result, gameState, 0, 0, 
	//								 	 result->renderSize.x, result->renderSize.y);

	V2 laserSize = v2(base->hitboxes->collisionSize.x, 
					  result->renderSize.y - base->hitboxes->collisionSize.y - top->hitboxes->collisionSize.y);

	double topPoint = top->p.y - top->hitboxes->collisionSize.y / 2 + top->hitboxes->collisionOffset.y;
	double basePoint = base->p.y + base->hitboxes->collisionSize.y / 2 - base->hitboxes->collisionOffset.y;

	V2 laserP = v2(result->p.x + base->hitboxes->collisionOffset.x, (topPoint + basePoint) / 2.0);
	Entity* beam = addLaserBeam(gameState, laserP, laserSize);

	setFlags(result, EntityFlag_noMovementByDefault|
					 EntityFlag_hackable|
					 EntityFlag_ignoresGravity);

	giveEntityRectangularCollisionBounds(result, gameState, 
										 base->hitboxes->collisionOffset.x + base->p.x - result->p.x, 
										 base->hitboxes->collisionOffset.y + base->p.y - result->p.y,
										 base->hitboxes->collisionSize.x, base->hitboxes->collisionSize.y);

	giveEntityRectangularCollisionBounds(result, gameState, 
										 top->hitboxes->collisionOffset.x + top->p.x - result->p.x, 
										 top->hitboxes->collisionOffset.y + top->p.y - result->p.y,
										 top->hitboxes->collisionSize.x, top->hitboxes->collisionSize.y);

	addField(result, gameState, createPatrolField(gameState));

	addGroundReference(base, result, gameState);
	addGroundReference(top, result, gameState);
	addGroundReference(beam, result, gameState);

	return result;
}

Entity* addFlyingVirus(GameState* gameState, V2 p) {
	Entity* result = addEntity(gameState, EntityType_flyingVirus, DrawOrder_flyingVirus, p, v2(1.5, 1.5));

	result->texture = &gameState->flyingVirus;

	setFlags(result, EntityFlag_hackable|
					 EntityFlag_ignoresGravity);

	giveEntityRectangularCollisionBounds(result, gameState, 0, result->renderSize.y * -0.04, 
										 result->renderSize.x * 0.45, result->renderSize.y * 0.45);

	addField(result, gameState, createSeekTargetField(gameState));
	addField(result, gameState, createShootField(gameState));

	result->p += result->renderSize / 2.0f;

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

bool isLaserEnemyType(Entity* entity) {
	bool result = entity->type == EntityType_laserTop ||
				  entity->type == EntityType_laserBase ||
				  entity->type == EntityType_laserBeam ||
				  entity->type == EntityType_laserController;

	return result;
}

bool collidesWithRaw(Entity* a, Entity* b, GameState* gameState) {
	bool result = true;

	switch(a->type) {
		case EntityType_null:
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
		} break;
		case EntityType_blueEnergy: {
			if (b->type == EntityType_virus ||
				b->type == EntityType_flyingVirus ||
				b->type == EntityType_laserBolt) result = false;
		} break;
		case EntityType_endPortal: {
			if (b->type != EntityType_player && 
				b->type != EntityType_tile) result = false;
		} break;
		case EntityType_laserController: {
			if (b == getEntityByRef(gameState, a->groundReferenceList->ref) ||
				b == getEntityByRef(gameState, a->groundReferenceList->next->ref) ||
				b == getEntityByRef(gameState, a->groundReferenceList->next->next->ref))
				result = false;
		} break;
	}

	return result;
}

bool collidesWith(Entity* a, Entity* b, GameState* gameState) {
	bool result = collidesWithRaw(a, b, gameState) && collidesWithRaw(b, a, gameState);
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
	}

	return result;
}

bool isSolidCollision(Entity* a, Entity* b) {
	bool result = isSolidCollisionRaw(a, b) && isSolidCollisionRaw(b, a);
	return result;
}

void onCollide(Entity* entity, Entity* hitEntity, GameState* gameState) {
	switch(entity->type) {
		case EntityType_blueEnergy: {
			if (hitEntity->type == EntityType_player) {
				setFlags(entity, EntityFlag_remove);
			}
		} break;
		case EntityType_endPortal: {
			if (hitEntity->type == EntityType_player) {
				gameState->loadNextLevel = true;
			}
		} break;
		case EntityType_laserBolt: {
			setFlags(entity, EntityFlag_remove);
		} break;
		case EntityType_laserBeam: {
			setFlags(hitEntity, EntityFlag_remove);
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

void getLineCollisionTime(double lx1, double lx2, double ly, double posX, double posY, 
						  double dPosX, double dPosY, bool top, 
						  GetCollisionTimeResult* result, bool* hit, bool* solidHit, bool horizontalCollision) {
	double collisionTime;

	if (dPosY == 0) {
		collisionTime = -1;
	} else {
		collisionTime = (ly - posY) / dPosY;

		double epsilon = 0.000000000001;

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

GetCollisionTimeResult getCollisionTime(Entity* entity, GameState* gameState, V2 delta) {
	GetCollisionTimeResult result = {};
	result.collisionTime = 1;
	result.solidCollisionTime = 1;

	for (int colliderIndex = 0; colliderIndex < gameState->numEntities; colliderIndex++) {
		Entity* collider = gameState->entities + colliderIndex;

		if (collider != entity &&
			collidesWith(entity, collider, gameState)) {

			Hitbox* colliderHitboxList = collider->hitboxes;

			while(colliderHitboxList) {
				R2 colliderHitbox = getHitbox(collider, colliderHitboxList);

				Hitbox* entityHitboxList = entity->hitboxes;

				while (entityHitboxList) {
					//Minkowski sum
					R2 sum = addDiameterTo(colliderHitbox, entityHitboxList->collisionSize);

					double centerX = entity->p.x + entityHitboxList->collisionOffset.x;
					double centerY = entity->p.y + entityHitboxList->collisionOffset.y;

					bool hit = false;
					bool solidHit = false;

					getLineCollisionTime(sum.min.x, sum.max.x, sum.min.y, centerX, centerY, 
										 delta.x, delta.y, false, &result, &hit, &solidHit, true);

					getLineCollisionTime(sum.min.x, sum.max.x, sum.max.y, centerX, centerY, 
										 delta.x, delta.y, true, &result, &hit, &solidHit, true);

					getLineCollisionTime(sum.min.y, sum.max.y, sum.min.x, centerY, centerX, 
						 				 delta.y, delta.x, false, &result, &hit, &solidHit, false);

					getLineCollisionTime(sum.min.y, sum.max.y, sum.max.x, centerY, centerX, 
				 						 delta.y, delta.x, true, &result, &hit, &solidHit, false);

					if (hit) {
						result.hitEntity = collider;
					}

					if (solidHit && isSolidCollision(entity, collider)) { 
						result.solidEntity = collider;
					}

					entityHitboxList = entityHitboxList->next;
				}

				colliderHitboxList = colliderHitboxList->next;
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

void moveRaw(Entity* entity, GameState* gameState, V2 delta, V2* ddP = 0) {
	double maxCollisionTime = 1;
	V2 totalMovement = {};

	for (int moveIteration = 0; moveIteration < 4 && maxCollisionTime > 0; moveIteration++) {
		GetCollisionTimeResult collisionResult = getCollisionTime(entity, gameState, delta);
		double collisionTime = min(maxCollisionTime, collisionResult.collisionTime);

		maxCollisionTime -= collisionTime;

		V2 movement = delta * collisionTime;
		entity->p += movement;
		totalMovement += movement;

		if (collisionResult.hitEntity) {
			if (isSolidCollision(collisionResult.hitEntity, entity)) {
    			if (collisionResult.horizontalCollision) {
    				//NOTE: If moving and you would hit an entity which is using you as ground
    				//		then we try moving that entity first. Then retrying this move. 
					RefNode* nextTopEntity = entity->groundReferenceList;
					RefNode* prevTopEntity = NULL;

					bool hitGround = false;

    				while (nextTopEntity) {
						if (nextTopEntity->ref == collisionResult.hitEntity->ref &&
							!(entity->type == EntityType_tile && collisionResult.hitEntity->type == EntityType_tile &&
							  getMovementField(entity) == NULL && getMovementField(collisionResult.hitEntity) == NULL)) {
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

			onCollide(entity, collisionResult.hitEntity, gameState);
			onCollide(collisionResult.hitEntity, entity, gameState);
		}
	}

	//NOTE: This moves all of the entities which are supported by this one (as the ground)
	RefNode* nextTopEntity = entity->groundReferenceList;

	if (lengthSq(totalMovement) > 0) {
		while (nextTopEntity) {
			Entity* top = getEntityByRef(gameState, nextTopEntity->ref);
			if (top) {

				#if 0
					bool moveTopEntity = !(entity->type == EntityType_tile && getMovementField(entity) == NULL &&
										   top->type == EntityType_tile && getMovementField(top) == NULL);	
				#else
					bool moveTopEntity = true;
				#endif

				if (moveTopEntity) {
					moveRaw(top, gameState, totalMovement);
				}
			}
			nextTopEntity = nextTopEntity->next;
		}
	}
}

void move(Entity* entity, double dt, GameState* gameState, V2 ddP) {
	V2 delta = getVelocity(dt, entity->dP, ddP);
	moveRaw(entity, gameState, delta, &ddP);
	entity->dP += ddP * dt;
}

TileMoveNode* createMoveListNode(Entity* tile) {
	assert(tile->type == EntityType_tile);

	TileMoveNode* result = (TileMoveNode*)calloc(1, sizeof(TileMoveNode));
	result->move = (TileMove*)calloc(1, sizeof(TileMove));
	result->move->tile = tile;

	return result;
}

TileMoveNode* addTileMovesToList(TileMoveNode* tileMoveList, TileMove* move, GameState* gameState, V2 vel);
TileMoveNode* addTileMoveDependency(TileMoveNode* tileMoveList, TileMove* move, GameState* gameState, 
									V2 vel, Entity* aboveEntity) {
	move->numParents++;

	TileMoveNode* moveListNode = tileMoveList;

	while(moveListNode) {
		if (moveListNode->move->tile->ref == aboveEntity->ref) {
			break;
		}

		moveListNode = moveListNode->next;
	}

	if (!moveListNode) {
		moveListNode = createMoveListNode(aboveEntity);

		moveListNode->next = tileMoveList;
		tileMoveList = moveListNode;

		tileMoveList = addTileMovesToList(tileMoveList, moveListNode->move, gameState, vel);
	}

	TileMoveNode* childNode = (TileMoveNode*)calloc(1, sizeof(TileMoveNode)); 
	childNode->move = move;
	childNode->next = moveListNode->move->children;
	moveListNode->move->children = childNode;

	return tileMoveList;
}

TileMoveNode* addTileMovesToList(TileMoveNode* tileMoveList, TileMove* move, GameState* gameState, V2 vel) {
	/*RefNode* above = move->tile->groundReferenceList;
	while(above) {
		Entity* aboveEntity = getEntityByRef(gameState, above->ref);

		if (aboveEntity && aboveEntity->type == EntityType_tile) {
			tileMoveList = addTileMoveDependency(tileMoveList, move, gameState, vel, aboveEntity);
		}

		above = above->next;
	}*/

	if (vel.y == 0) {
		GetCollisionTimeResult collisionResult = getCollisionTime(move->tile, gameState, -gameState->gravity / 120.0);

		if (collisionResult.hitEntity && 
			collisionResult.hitEntity->type == EntityType_tile &&
			getMovementField(collisionResult.hitEntity) == NULL) {

			V2 moveVel = vel * (1 - collisionResult.collisionTime);

			tileMoveList = addTileMoveDependency(tileMoveList, move, gameState, 
												 moveVel, collisionResult.hitEntity);
		}
	}

	if (vel.x != 0 || vel.y != 0) {
		GetCollisionTimeResult collisionResult = getCollisionTime(move->tile, gameState, vel);

		if (collisionResult.hitEntity && 
			collisionResult.hitEntity->type == EntityType_tile &&
			getMovementField(collisionResult.hitEntity) == NULL) {

			V2 moveVel = vel * (1 - collisionResult.collisionTime);

			tileMoveList = addTileMoveDependency(tileMoveList, move, gameState, 
												 moveVel, collisionResult.hitEntity);
		}
	}

	return tileMoveList;
}

//TODO: Clean up memory
//TODO: Handle tiles which have other types of movement (eg. keyboard)
void moveTiles(Entity* entity, double dt, GameState* gameState) {
	assert(entity->type == EntityType_tile);

	V2 delta = entity->dP * dt;

	TileMoveNode* tileMoveList = createMoveListNode(entity);
	tileMoveList = addTileMovesToList(tileMoveList, tileMoveList->move, gameState, delta);

	while(tileMoveList) {
		TileMoveNode* moveNode = tileMoveList;
		TileMoveNode* previousMoveNode = NULL;

		bool madeChange = false;

		while(moveNode) {
			TileMove* move = moveNode->move;

			if (move->numParents == 0) {
				madeChange = true;

				moveRaw(move->tile, gameState, delta, &v2(0, 0));

				TileMoveNode* child = move->children;

				while (child) {
					child->move->numParents--;					
					child = child->next;
				}

				if (previousMoveNode) {
					previousMoveNode->next = moveNode->next;
					moveNode = previousMoveNode;
				} else {
					tileMoveList = moveNode->next;
					moveNode = tileMoveList;
				}
			}

			previousMoveNode = moveNode;
			if (moveNode) moveNode = moveNode->next;
		}

		if (!madeChange && tileMoveList) {
			assert(false);
			break;
		}
	}


}

bool moveTowardsTarget(Entity* entity, GameState* gameState, double dt,
					   V2 target, double initialDstToTarget, double maxSpeed) {
	V2 delta = target - entity->p;
	double dstToTarget = length(delta);

	double percentageToTarget = (initialDstToTarget - dstToTarget) / initialDstToTarget;

	double initialTime = 0.1;
	double speedEquationCoefficient = maxSpeed / ((0.5 + initialTime) * (-0.5 - initialTime));
	double speed = speedEquationCoefficient * (percentageToTarget + initialTime) * (percentageToTarget - 1 - initialTime);

	if (speed * dt > dstToTarget) {
		entity->dP = delta / dt;
	} else {
		entity->dP = speed * normalize(delta);
	}

	moveTiles(entity, dt, gameState/*, v2(0, 0)*/);
	return entity->p == target;
}

bool containsField(Entity* entity, ConsoleFieldType type) {
	bool result = false;

	for (int fieldIndex = 0; fieldIndex < entity->numFields; fieldIndex++) {
		if (entity->fields[fieldIndex]->type == type) {
			result = true;
			break;
		}
	}

	return result;
}

bool isMouseInside(Entity* entity, Input* input) {
	bool result = false;

	Hitbox* hitboxList = entity->hitboxes;

	while (hitboxList) {
		R2 clickBox = getHitbox(entity, hitboxList);

		if (pointInsideRect(clickBox, input->mouseInWorld)) {
			result = true;
			break;
		}

		hitboxList = hitboxList->next;
	}

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
		gameState->solidGrid[xTile][yTile].solid = true;
	}
}

PathNode* getClosestNonSolidNode(V2 p, GameState* gameState) {
	PathNode* result = NULL;
	double minDstSq = 100000000000.0;

	int radius = 6;

	double minX = p.x - gameState->solidGridSquareSize * radius;
	double minY = p.y - gameState->solidGridSquareSize * radius;

	for(int xOffs = -radius; xOffs <= radius; xOffs++) {
		for (int yOffs = -radius; yOffs <= radius; yOffs++) {
			double xPos = minX + xOffs * gameState->solidGridSquareSize;
			double yPos = minY + yOffs * gameState->solidGridSquareSize;

			int xTile = (int)(xPos / gameState->solidGridSquareSize);
			int yTile = (int)(yPos / gameState->solidGridSquareSize);

			if (inSolidGridBounds(gameState, xTile, yTile)) {
				PathNode* node = gameState->solidGrid[xTile] + yTile;

				if (!node->solid) {
					double testDstSq = dstSq(node->p, p);

					if (testDstSq < minDstSq) {
						minDstSq = testDstSq;
						result = node;
					}
				}
			}
		}
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

		int xTile = (int)(p.x / gameState->solidGridSquareSize);
		int yTile = (int)(p.y / gameState->solidGridSquareSize);

		if (gameState->solidGrid[xTile][yTile].solid) return false;
	}

	int xTile = (int)(maxX / gameState->solidGridSquareSize);
	int yTile = (int)(maxY / gameState->solidGridSquareSize);	

	if (gameState->solidGrid[xTile][yTile].solid) return false;

	return true;
}

V2 computePath(GameState* gameState, Entity* start, Entity* goal) {
	for (int tileX = 0; tileX < gameState->solidGridWidth; tileX++) {
		for (int tileY = 0; tileY < gameState->solidGridHeight; tileY++) {
			PathNode* node = gameState->solidGrid[tileX] + tileY;
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
			collidesWith(start, collider, gameState)) {

			Hitbox* colliderHitboxList = collider->hitboxes;

			while(colliderHitboxList) {
				R2 colliderHitbox = getHitbox(collider, colliderHitboxList);

				Hitbox* entityHitboxList = start->hitboxes;

				while (entityHitboxList) {
					R2 sum = addDiameterTo(colliderHitbox, entityHitboxList->collisionSize);

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

	PathNode* startNode = getClosestNonSolidNode(start->p, gameState);
	PathNode* goalNode = getClosestNonSolidNode(goal->p, gameState);

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
							PathNode* testNode = gameState->solidGrid[tileX] + tileY;

							if (!testNode->solid) {
								#if 0
								if (current->parent) {
									int dx = current->tileX - current->parent->tileX;
									int dy = current->tileY - current->parent->tileY;

									if (dy == 0) {
										if (xOffs != dx ||
										    (yOffs == 1 && !gameState->solidGrid[tileX][tileY + 1].solid) ||
											(yOffs == -1 && !gameState->solidGrid[tileX][tileY - 1].solid)) continue;
									}
									else if (dx == 0) {
										if (yOffs != dy ||
										   (xOffs == 1 && !gameState->solidGrid[tileX + 1][tileY].solid) ||
										   (xOffs == -1 && !gameState->solidGrid[tileX - 1][tileY].solid)) continue;
									}
									else {
										if ((xOffs == -dx && !(yOffs == dy && gameState->solidGrid[tileX - dx][tileY].solid)) ||
										   ( yOffs == -dy && !(xOffs == dx && gameState->solidGrid[tileX][tileY - dy].solid)))
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

bool canBeGrounded(Entity* entity) {
	bool result = !isSet(entity, EntityFlag_noMovementByDefault) || getMovementField(entity) != NULL;
	return result;
}

void updateAndRenderEntities(GameState* gameState, double dtForFrame) {
	double dt = gameState->consoleEntityRef ? 0 : dtForFrame;

	//TODO: It might be possible to combine the three loops which handle ground reference lists later
	//NOTE: This is to reset all of the entities ground reference lists
	for (int entityIndex = 0; entityIndex < gameState->numEntities; entityIndex++) {
		Entity* entity = gameState->entities + entityIndex;

		clearFlags(entity, EntityFlag_grounded);

		//The laser controller uses its ground references to move its components (top, base, beam)
		if (entity->type != EntityType_laserController) {
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

		//The laser controllers components can move all of the other entities which are on top of it
		if (entity->type != EntityType_laserController) {
			Entity* above = getAbove(entity, gameState);

			if (above) {
				if (canBeGrounded(above)) {
					above->jumpCount = 0;
					setFlags(above, EntityFlag_grounded);
					addGroundReference(above, entity, gameState);
				}
			}
		}

		if (canBeGrounded(entity)) {
			Entity* below = onGround(entity, gameState);

			if (below) {
				setFlags(entity, EntityFlag_grounded);
				addGroundReference(entity, below, gameState);
			}
		}
	}

	//NOTE: This loops through all of the entities in the game state to update and render them
	for (int entityIndex = 0; entityIndex < gameState->numEntities; entityIndex++) {
		Entity* entity = gameState->entities + entityIndex;
		entity->animTime += dt;

		removeFieldsIfSet(entity->fields, &entity->numFields);

		bool shootingState = false;
		bool shootsAtTarget = containsField(entity, ConsoleField_shootsAtTarget); 

		//NOTE:This handles shooting if the entity should shoot
		if (shootsAtTarget && gameState->shootTargetRef) {
			Entity* target = getEntityByRef(gameState, gameState->shootTargetRef);

			double dstToTarget = dstSq(target->p, entity->p);
			double shootRadius = square(5.0f);

			//TODO: Set this depending on the entitytype
			V2 spawnOffset = v2(0, -0.3f);

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
					setFlags(entity, EntityFlag_shooting);
				} else {
					entity->shootTimer += dt;

					if (entity->shootTimer >= gameState->shootDelay) {
						entity->shootTimer = gameState->shootDelay;
						clearFlags(entity, EntityFlag_shooting);
						setFlags(entity, EntityFlag_unchargingAfterShooting);
						addLaserBolt(gameState, entity->p + spawnOffset, target->p, entity->ref);
					}
				}
			}
		}

		V2 ddP = {};
		if (!isSet(entity, EntityFlag_ignoresGravity)) {
			ddP = gameState->gravity;
		}

		if (!isSet(entity, EntityFlag_ignoresFriction)) {
			double frictionCoefficient = -15.0f;
			double friction = pow(E, frictionCoefficient * dt);
			entity->dP.x *= friction;
			if (isSet(entity, EntityFlag_ignoresGravity)) entity->dP.y *= friction;
		}

		ConsoleField* movementField = getMovementField(entity);

		//NOTE: This handles moving the entity
		if (movementField && dt > 0) {
			ConsoleField* speedField = movementField->children[0];
			double xMoveAcceleration = ((double*)speedField->values)[speedField->selectedIndex];

			switch(movementField->type) {



				case ConsoleField_keyboardControlled: {
					double windowWidth = gameState->windowSize.x;
					double maxCameraX = gameState->mapSize.x - windowWidth;
					gameState->cameraP.x = clamp((double)(entity->p.x - windowWidth / 2.0), 0, maxCameraX);

					double xMove = 0;

					ConsoleField* jumpField = movementField->children[1];
					double jumpHeight = ((double*)jumpField->values)[jumpField->selectedIndex];

					ConsoleField* doubleJumpField = movementField->children[2];
					bool canDoubleJump = doubleJumpField->selectedIndex != 0;

					if (gameState->input.rightPressed) xMove += xMoveAcceleration;
					if (gameState->input.leftPressed) xMove -= xMoveAcceleration;

					ddP.x += xMove;

					if (xMove > 0) clearFlags(entity, EntityFlag_facesLeft);
					else if (xMove < 0) setFlags(entity, EntityFlag_facesLeft);

					bool canJump = false;

					if (!isSet(entity, EntityFlag_ignoresGravity)) {
						canJump = isSet(entity, EntityFlag_grounded) && gameState->input.upPressed;
						
						if (!canJump) {
							canJump = entity->jumpCount < 2 && canDoubleJump && gameState->input.upJustPressed;
						}
					}

					if (canJump) {
						entity->dP.y = jumpHeight;
						entity->jumpCount++;
					}

					move(entity, dt, gameState, ddP);
				} break;




				case ConsoleField_movesBackAndForth: {
					
					
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

					double startX = entity->p.x;
					move(entity, dt, gameState, ddP);
					double endX = entity->p.x;

					bool shouldChangeDirection = false;

					if (isSet(entity, EntityFlag_ignoresGravity)) {
						shouldChangeDirection = startX == endX;
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
				} break;




				case ConsoleField_seeksTarget: {
					if (!shootingState) {
						Entity* targetEntity = getEntityByRef(gameState, gameState->shootTargetRef);
						if (targetEntity) {
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
				} break;
			}
		} else {
			if (!isSet(entity, EntityFlag_noMovementByDefault)) {
				move(entity, dt, gameState, ddP);
			}
		}


		//NOTE: This removes the entity if they are outside of the world
		if (isSet(entity, EntityFlag_removeWhenOutsideLevel)) {
			R2 world = r2(v2(0, 0), gameState->worldSize);

			bool insideLevel = false;

			Hitbox* hitboxList = entity->hitboxes;

			while(hitboxList) {
				R2 hitbox = rectCenterDiameter(entity->p + hitboxList->collisionOffset, hitboxList->collisionSize);

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

		//Remove entity if the are below the bottom of the world
		if (entity->p.y < -entity->renderSize.y) {
			setFlags(entity, EntityFlag_remove);
		}


		//NOTE: Individual entity logic here
		switch(entity->type) {
			case EntityType_player: {
				//NOTE: This handles changing the player's texture based on his state
				if (entity->dP.y != 0 || !isSet(entity, EntityFlag_grounded)) {
					entity->animTime = 0;
					entity->texture = &gameState->playerJump;
				}
				else if (abs(entity->dP.x) > 0.25f) {
					entity->texture = getAnimationFrame(&gameState->playerWalk, entity->animTime);
				} else {
					entity->animTime = 0;
					entity->texture = &gameState->playerStand;
				}

			} break;
			case EntityType_virus: {
				//NOTE: This shoots a projectile at the shoot target if the target is nearby
				if (shootingState) {
					entity->texture = getAnimationFrame(&gameState->virus1Shoot, entity->shootTimer);
				} else {
					entity->texture = &gameState->virus1Stand;
				}

			} break;
			case EntityType_flyingVirus: {
	
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

				drawTexture(gameState, bg, translateRect(bgBounds, -gameState->cameraP), false);
				drawTexture(gameState, mg, translateRect(mgBounds, -gameState->cameraP), false);
			} break;

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
					V2 target = entity->tileStartPos + hadamard(v2((double)dXOffset, (double)dYOffset), entity->renderSize);

					if (moveTowardsTarget(entity, gameState, dtForFrame, target, entity->renderSize.x, 6.0f)) {
						entity->tileXOffset = fieldXOffset;
						entity->tileYOffset = fieldYOffset;
					}
				} else {
					entity->tileStartPos = entity->p;
				}
			} break;
		}

		if (!isSet(entity, EntityFlag_remove) && entity->texture != NULL) { 
			drawTexture(gameState, entity->texture, 
						translateRect(rectCenterDiameter(entity->p, entity->renderSize), -gameState->cameraP), 
						isSet(entity, EntityFlag_facesLeft));
		}

		#if SHOW_COLLISION_BOUNDS
			Hitbox* hitbox = entity->hitboxes;

			while (hitbox) {
				setColor(gameState->renderer, 255, 100, 255, 255);
				drawRect(gameState, rectCenterDiameter(hitbox->collisionOffset + entity->p, hitbox->collisionSize),
													   0.005f, gameState->cameraP);
				hitbox = hitbox->next;
			}

		#endif
	}

	removeEntities(gameState);
}
