void giveEntityRectangularCollisionBounds(Entity* entity, float xOffset, float yOffset, float width, float height) {
	entity->collisionSize = v2(width, height);
	entity->collisionOffset = v2(xOffset, yOffset);
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
		result = (ConsoleField*)malloc(sizeof(ConsoleField));
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
	addChildrenToConsoleField(result, 2);
	
	result->children[0] = createField(gameState, &gameState->playerSpeedField);
	result->children[1] = createField(gameState, &gameState->playerJumpHeightField);

	return result;
}

ConsoleField* createPatrolField(GameState* gameState) {
	ConsoleField* result = createField(gameState, &gameState->movesBackAndForthField);
	addChildrenToConsoleField(result, 1);
	
	result->children[0] = createField(gameState, &gameState->playerSpeedField);

	return result;
}

Entity* addPlayer(GameState* gameState, V2 p) {
	Entity* result = addEntity(gameState, EntityType_player, DrawOrder_player, p, v2(1.4f, 1.4f));

	assert(!gameState->shootTargetRef);
	gameState->shootTargetRef = result->ref;
	assert(!gameState->playerRef);
	gameState->playerRef = result->ref;

	giveEntityRectangularCollisionBounds(result, 0, 0,
										 result->renderSize.x * 0.4f, result->renderSize.y);

	result->texture = &gameState->playerStand;

	setFlags(result, EntityFlag_collidable|
					 EntityFlag_solid|
					 EntityFlag_facesLeft);

	addField(result, gameState, createKeyboardField(gameState));
	addField(result, gameState, createField(gameState, &gameState->isShootTargetField));

	result->p += result->renderSize / 2;

	return result;
}

Entity* addVirus(GameState* gameState, V2 p) {
	Entity* result = addEntity(gameState, EntityType_virus, DrawOrder_virus, p, v2(2.5f, 2.5f));

	giveEntityRectangularCollisionBounds(result, -0.025f, -0.1f,
										 result->renderSize.x * 0.55f, result->renderSize.y * 0.75f);

	result->texture = &gameState->virus1Stand;

	setFlags(result, EntityFlag_collidable|
					 EntityFlag_facesLeft);

	addField(result, gameState, createPatrolField(gameState));
	addField(result, gameState, createField(gameState, &gameState->shootsAtTargetField));

	result->p += result->renderSize / 2;

	return result;
}

Entity* addBlueEnergy(GameState* gameState, V2 p) {
	Entity* result = addEntity(gameState, EntityType_blueEnergy, DrawOrder_blueEnergy, p, v2(0.8f, 0.8f));

	giveEntityRectangularCollisionBounds(result, 0, 0, 
										 result->renderSize.x * 0.5f, result->renderSize.y * 0.65f);

	result->texture = &gameState->blueEnergy;

	setFlags(result, EntityFlag_collidable);

	result->p += result->renderSize / 2;

	return result;
}

Entity* addBackground(GameState* gameState) {
	Entity* result = addEntity(gameState, EntityType_background, DrawOrder_background, 
								  v2(0, 0), v2(gameState->mapSize.x, 
								(float)gameState->windowHeight / (float)gameState->pixelsPerMeter));

	result->p = result->renderSize / 2.0f;
	setFlags(result, EntityFlag_noMovementByDefault);

	return result;
}

Entity* addTile(GameState* gameState, V2 p, Texture* texture) {
	Entity* result = addEntity(gameState, EntityType_tile, DrawOrder_tile, p, v2(gameState->tileSize, gameState->tileSize));

	giveEntityRectangularCollisionBounds(result, 0, 0, 
										 result->renderSize.x, result->renderSize.y);

	result->texture = texture;

	setFlags(result, EntityFlag_collidable|
					 EntityFlag_noMovementByDefault| 
				     EntityFlag_solid);

	addField(result, gameState, createField(gameState, &gameState->tileXOffsetField));
	addField(result, gameState, createField(gameState, &gameState->tileYOffsetField));

	return result;
}

Entity* addText(GameState* gameState, V2 p, char* msg) {
	Entity* result = addEntity(gameState, EntityType_text, DrawOrder_text, p, v2(0, 0));
	result->texture = (Texture*)malloc(sizeof(Texture));
	*(result->texture) = createText(gameState, gameState->textFont, msg);
	result->renderSize = v2((float)result->texture->srcRect.w, (float)result->texture->srcRect.h) / gameState->pixelsPerMeter;
	
	setFlags(result, EntityFlag_noMovementByDefault);

	return result;
}

Entity* addLaserBolt(GameState* gameState, V2 p, V2 target, int shooterRef) {
	Entity* result = addEntity(gameState, EntityType_laserBolt, DrawOrder_laserBolt, p, v2(0.8f, 0.8f));
	result->texture = &gameState->laserBolt;

	giveEntityRectangularCollisionBounds(result, 0, 0, 
										 result->renderSize.x * 0.3f, result->renderSize.y * 0.3f);

	float speed = 3.0f;
	result->dP = normalize(target - p) * speed;
	result->shooterRef = shooterRef;

	setFlags(result, EntityFlag_collidable|
					 EntityFlag_ignoresFriction|
					 EntityFlag_ignoresGravity|
					 EntityFlag_removeWhenOutsideLevel);
					 
	return result;
}

bool collidesWith(Entity* a, Entity* b) {
	bool result = true;

	switch(a->type) {
		case EntityType_null:
		case EntityType_background:
		case EntityType_text: {
			result = false;
		} break;

		case EntityType_player: {
			if (b->type == EntityType_virus) result = false;
		} break;
		case EntityType_laserBolt: {
			if (b->ref == a->shooterRef) result = false;
		} break;
		case EntityType_blueEnergy: {
			if (b->type != EntityType_player &&
				b->type != EntityType_tile) result = false;
		}
	}

	return result;
}

void onCollide(Entity* entity, Entity* hitEntity) {
	switch(entity->type) {
		case EntityType_blueEnergy: {
			if (hitEntity->type == EntityType_player) {
				setFlags(entity, EntityFlag_remove);
				clearFlags(entity, EntityFlag_collidable);
			}
		} break;
		case EntityType_laserBolt: {
			setFlags(entity, EntityFlag_remove);
			clearFlags(entity, EntityFlag_collidable);
		} break;
	}
}

R2 getEntityHitbox(Entity* entity) {
	R2 result = rectCenterDiameter(entity->p + entity->collisionOffset, entity->collisionSize);
	return result;
}

void getLineCollisionTime(float lx1, float lx2, float ly, float posX, float posY, 
						  float dPosX, float dPosY, bool top, 
						  GetCollisionTimeResult* result, bool* changed, bool horizontalCollision) {
	float collisionTime;

	if (dPosY == 0) {
		collisionTime = -1;
	} else {
		collisionTime = (ly - posY) / dPosY;

		if (collisionTime == 0 && dPosY > 0 == top) {
			collisionTime = -1;
		} else {
			posX += dPosX * collisionTime;

			if (posX > lx1 && posX < lx2) {

			} else {
				collisionTime = -1;
			}
		}
	}

	if (collisionTime >= 0 && collisionTime < result->collisionTime) {
		result->collisionTime = collisionTime;
		result->horizontalCollision = horizontalCollision;
		*changed = true;
	}
}

GetCollisionTimeResult getCollisionTime(Entity* entity, GameState* gameState, V2 delta) {
	GetCollisionTimeResult result = {};
	result.collisionTime = 1;

	if (isSet(entity, EntityFlag_collidable)) {
		for (int colliderIndex = 0; colliderIndex < gameState->numEntities; colliderIndex++) {
			Entity* collider = gameState->entities + colliderIndex;
			
			if (collider != entity && isSet(collider, EntityFlag_collidable) &&
				collidesWith(entity, collider) && collidesWith(collider, entity)) {

				R2 colliderHitbox = getEntityHitbox(collider);

				//Minkowski sum
				R2 sum = addDiameterTo(colliderHitbox, entity->collisionSize);

				bool changed = false;

				float centerX = entity->p.x + entity->collisionOffset.x;
				float centerY = entity->p.y + entity->collisionOffset.y;

				getLineCollisionTime(sum.min.x, sum.max.x, sum.min.y, centerX, centerY, 
									 delta.x, delta.y, false, &result, &changed, true);

				getLineCollisionTime(sum.min.x, sum.max.x, sum.max.y, centerX, centerY, 
									 delta.x, delta.y, true, &result, &changed, true);

				getLineCollisionTime(sum.min.y, sum.max.y, sum.min.x, centerY, centerX, 
					 				 delta.y, delta.x, false, &result, &changed, false);

				getLineCollisionTime(sum.min.y, sum.max.y, sum.max.x, centerY, centerX, 
			 						 delta.y, delta.x, true, &result, &changed, false);

				if (changed) {
					result.hitEntity = collider;
					result.hitSolidEntity |= isSet(collider, EntityFlag_solid);
				}
			}
		}
	}

	return result;
}

V2 getVelocity(float dt, V2 dP, V2 ddP) {
	V2 result = dt * dP + 0.5f * dt * dt * ddP;
	return result;
}

bool onGround(Entity* entity, GameState* gameState, float dt, V2 dP, V2 ddP) {
	V2 delta = getVelocity(dt, dP, ddP);
	GetCollisionTimeResult collisionResult = getCollisionTime(entity, gameState, delta);
	return collisionResult.hitSolidEntity;
}

bool onGround(Entity* entity, GameState* gameState, float dt) {
	bool result = onGround(entity, gameState, dt, v2(0, 0), gameState->gravity);
	return result;
}

void move(Entity* entity, float dt, GameState* gameState, V2 ddP) {
	V2 delta = getVelocity(dt, entity->dP, ddP);
	entity->dP * dt + (float)0.5 * ddP * dt * dt;
	
	float maxCollisionTime = 1;

	for (int moveIteration = 0; moveIteration < 4 && maxCollisionTime > 0; moveIteration++) {
		GetCollisionTimeResult collisionResult = getCollisionTime(entity, gameState, delta);
		float collisionTime = min(maxCollisionTime, collisionResult.collisionTime);

		maxCollisionTime -= collisionTime;
		float collisionTimeEpsilon = 0.001f; //TODO: More exact collision detection
		float moveTime = max(0, collisionTime - collisionTimeEpsilon);

		entity->p += delta * moveTime;

		if (collisionResult.hitEntity) {
			if (isSet(collisionResult.hitEntity, EntityFlag_solid)) {
    			if (collisionResult.horizontalCollision) {
    				delta.y = 0;
    				entity->dP.y = 0;
    				ddP.y = 0;
    			} else {
					delta.x = 0;
    				entity->dP.x = 0;
    				ddP.x = 0;
    			}
			}

			onCollide(entity, collisionResult.hitEntity);
			onCollide(collisionResult.hitEntity, entity);
		}
	}

	entity->dP += ddP * dt;
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

R2 getHitbox(Entity* entity) {
	R2 result = rectCenterDiameter(entity->p + entity->collisionOffset, entity->collisionSize);
	return result;
}

bool isMouseInside(Entity* entity, Input* input) {
	R2 clickBox = getHitbox(entity);
	bool result = pointInsideRect(clickBox, input->mouseInWorld);
	return result;
}

void updateAndRenderEntities(GameState* gameState, float dtForFrame) {
	float dt = gameState->consoleEntityRef ? 0 : dtForFrame;
	V2 oldCameraP = gameState->cameraP;

	for (int entityIndex = 0; entityIndex < gameState->numEntities; entityIndex++) {
		Entity* entity = gameState->entities + entityIndex;
		entity->animTime += dt;

		removeFieldsIfSet(entity->fields, &entity->numFields);

		bool shootingState = false;
		bool shootsAtTarget = containsField(entity, ConsoleField_shootsAtTarget); 

		if ((gameState->input.leftMouseJustPressed ||
		    (gameState->input.leftMousePressed && isSet(entity, EntityFlag_dragging))) &&
			 isMouseInside(entity, &gameState->input)) {

			setFlags(entity, EntityFlag_dragging);
		} else {
			clearFlags(entity, EntityFlag_dragging);
		}

		//NOTE:This handles shooting if the entity should shoot
		if (shootsAtTarget && gameState->shootTargetRef) {
			Entity* target = getEntityByRef(gameState, gameState->shootTargetRef);

			float dstToTarget = dstSq(target->p, entity->p);
			float shootRadius = square(5.0f);

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
			float friction = -15.0f;
			entity->dP.x *= (float)pow(E, friction * dt);
		}

		ConsoleField* movementField = getMovementField(entity);

		//NOTE: This handles moving the entity
		if (movementField) {
			switch(movementField->type) {
				case ConsoleField_keyboardControlled: {
					float windowWidth = gameState->windowSize.x;
					float maxCameraX = gameState->mapSize.x - windowWidth;
					gameState->cameraP.x = clamp((float)(entity->p.x - windowWidth / 2.0), 0, maxCameraX);

					float xMove = 0;

					ConsoleField* speedField = movementField->children[0];
					float xMoveAcceleration = ((float*)speedField->values)[speedField->selectedIndex];

					ConsoleField* jumpField = movementField->children[1];
					float jumpHeight = ((float*)jumpField->values)[jumpField->selectedIndex];

					if (gameState->input.rightPressed) xMove += xMoveAcceleration;
					if (gameState->input.leftPressed) xMove -= xMoveAcceleration;

					ddP.x += xMove;

					if (xMove > 0) clearFlags(entity, EntityFlag_facesLeft);
					else if (xMove < 0) setFlags(entity, EntityFlag_facesLeft);

					if (onGround(entity, gameState, dt) && gameState->input.upPressed) {
						entity->dP.y = jumpHeight;	
					}

					move(entity, dt, gameState, ddP);
				} break;

				case ConsoleField_movesBackAndForth: {
					ConsoleField* speedField = movementField->children[0];
					float xMoveAcceleration = ((float*)speedField->values)[speedField->selectedIndex];
					
					V2 oldP = entity->p;
					V2 oldCollisionSize = entity->collisionSize;
					V2 oldCollisionOffset = entity->collisionOffset;

					bool initiallyOnGround = onGround(entity, gameState, dt);

					if (isSet(entity, EntityFlag_facesLeft)) {
						xMoveAcceleration *= -1;
						entity->collisionOffset.x -= entity->collisionSize.x / 2;
					} else {
						entity->collisionOffset.x += entity->collisionSize.x / 2;
					}

					entity->collisionSize.x = 0;
					ddP.x = xMoveAcceleration;

					move(entity, dt, gameState, ddP);

					if (initiallyOnGround) {
						bool offOfGround = !onGround(entity, gameState, dt);

						if (offOfGround) {
							entity->p = oldP;
						}

						bool shouldChangeDirection = offOfGround || entity->dP.x == 0;

						if (shouldChangeDirection) {
							toggleFlags(entity, EntityFlag_facesLeft);
						}
					}

					entity->collisionSize = oldCollisionSize;
					entity->collisionOffset = oldCollisionOffset;
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

			if (!rectanglesOverlap(world, getHitbox(entity))) {
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
				if (entity->dP.y != 0) {
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
				//NOTE: This shoots a projectile at the shoottarget if the target is nearby
				if (shootingState) {
					entity->texture = getAnimationFrame(&gameState->virus1Shoot, entity->shootTimer);
				} else {
					entity->texture = &gameState->virus1Stand;
				}

			} break;
			case EntityType_background: {
				Texture* bg = &gameState->sunsetCityBg;
				Texture* mg = &gameState->sunsetCityMg;

				float bgTexWidth = (float)bg->srcRect.w;
				float mgTexWidth = (float)mg->srcRect.w;
				float mgTexHeight = (float)mg->srcRect.h;

				float bgScrollRate = bgTexWidth / mgTexWidth;
				float bgHeight = gameState->windowHeight / gameState->pixelsPerMeter;
				float mgWidth = max(gameState->mapSize.x, mgTexWidth / gameState->pixelsPerMeter);
								
				float bgWidth = mgWidth / bgScrollRate - 1;

				float bgX = gameState->cameraP.x * (1 -  bgScrollRate);

				R2 bgBounds = r2(v2(bgX, 0), v2(bgWidth, bgHeight));
				R2 mgBounds = r2(v2(0, 0), v2(mgWidth, bgHeight));

				drawTexture(gameState, bg, translateRect(bgBounds, -gameState->cameraP), false);
				drawTexture(gameState, mg, translateRect(mgBounds, -gameState->cameraP), false);
			} break;

			case EntityType_tile: {
				assert(entity->numFields >= 2);
				assert(entity->fields[0]->type == ConsoleField_unlimitedInt && 
					   entity->fields[1]->type == ConsoleField_unlimitedInt);

				int fieldXOffset =  entity->fields[0]->selectedIndex;
				int fieldYOffset =  entity->fields[1]->selectedIndex;

				int dXOffset = fieldXOffset - entity->tileXOffset;
				int dYOffset = fieldYOffset - entity->tileYOffset;

				//TODO: Smooth movement with collision detection
				entity->p += hadamard(v2((float)dXOffset, (float)dYOffset), entity->renderSize);

				entity->tileXOffset = fieldXOffset;
				entity->tileYOffset = fieldYOffset;
			} break;
		}

		if (!isSet(entity, EntityFlag_remove) && entity->texture != NULL) { 
			drawTexture(gameState, entity->texture, 
						translateRect(rectCenterDiameter(entity->p, entity->renderSize), -gameState->cameraP), 
						isSet(entity, EntityFlag_facesLeft));
		}

		#if SHOW_COLLISION_BOUNDS
			drawRect(gameState, getEntityHitbox(entity), 0.025f, gameState->cameraP);
		#endif
	}

	removeEntities(gameState);
}
