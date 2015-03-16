ConsoleField createFloatField(char* name, float* values, int numValues, int initialIndex) {
	assert(numValues > 0);
	assert(initialIndex >= 0 && initialIndex < numValues);

	ConsoleField result = {};

	result.type = ConsoleField_float;
	result.name = name;
	result.values = values;
	result.selectedIndex = result.initialIndex = initialIndex;
	result.numValues = numValues;

	return result;
}

void addFlags(Entity* entity, uint flags) {
	entity->flags |= flags;
} 

void clearFlags(Entity* entity, uint flags) {
	entity->flags &= ~flags;
} 

void toggleFlags(Entity* entity, uint toggle) {
	entity->flags ^= toggle;
}

bool isSet(Entity* entity, uint flags) {
	bool result = (entity->flags & flags) != 0;
	return result;
}

void giveEntityRectangularCollisionBounds(Entity* entity, float xOffset, float yOffset, float width, float height) {
	entity->numCollisionPoints = 4;
	assert(arrayCount(entity->collisionPoints) > entity->numCollisionPoints);

	entity->collisionPoints[0].x = -width / 2 + xOffset;
	entity->collisionPoints[0].y = -height / 2 + yOffset;

	entity->collisionPoints[1].x = -width / 2 + xOffset;
	entity->collisionPoints[1].y = height / 2 + yOffset;

	entity->collisionPoints[2].x = width / 2 + xOffset;
	entity->collisionPoints[2].y = height / 2 + yOffset;

	entity->collisionPoints[3].x = width / 2 + xOffset;
	entity->collisionPoints[3].y = -height / 2 + yOffset;
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
		if (bucket->entity->ref == ref) {
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

void removeEntities(GameState* gameState) {
	//NOTE: Entities are removed here if their remove flag is set
	//NOTE: There is a memory leak here if the entity allocated anything
	//		Ensure this is cleaned up, the console is a big place where leaks can happen
	for (int entityIndex = 0; entityIndex < gameState->numEntities; entityIndex++) {
		Entity* entity = gameState->entities + entityIndex;

		if (isSet(entity, EntityFlag_remove)) {
			switch(entity->type) {
				case EntityType_text: {
					SDL_DestroyTexture(entity->texture->tex);
					//TODO: A free list could be used here instead
					free(entity->texture);
				} break;
			}

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

			bucket->next = gameState->entityRefFreeList;
			gameState->entityRefFreeList = bucket;

			for (int srcEntityIndex = entityIndex + 1; srcEntityIndex < gameState->numEntities; srcEntityIndex++) {
				Entity* src = gameState->entities + srcEntityIndex;
				Entity* dst = gameState->entities + srcEntityIndex - 1;
				*dst = *src;
				getPreciseEntityReferenceBucket(gameState, dst->ref)->entity--;
			}

			gameState->entities[gameState->numEntities] = {};

			gameState->numEntities--;
			entityIndex--;
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
	while(bucket->next) bucket = bucket->next;

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

Entity* addPlayer(GameState* gameState, V2 p) {
	Entity* result = addEntity(gameState, EntityType_player, DrawOrder_player, p, v2(1.4f, 1.4f));

	assert(!gameState->playerRef);
	gameState->playerRef = result->ref;

	giveEntityRectangularCollisionBounds(result, 0, 0,
										 result->renderSize.x * 0.4f, result->renderSize.y);

	result->texture = &gameState->playerStand;

	addFlags(result, EntityFlag_moveable|
					 EntityFlag_collidable|
					 EntityFlag_solid|
					 EntityFlag_facesLeft);

	result->numFields = 1;
	assert(arrayCount(result->fields) > result->numFields);

	float playerSpeedValues[] = {3, 8, 13, 18, 23};
	result->fields[0] = createFloatField("speed", playerSpeedValues, arrayCount(playerSpeedValues), 2);

	result->p += result->renderSize / 2;

	return result;
}

Entity* addVirus(GameState* gameState, V2 p) {
	Entity* result = addEntity(gameState, EntityType_virus, DrawOrder_virus, p, v2(2.5f, 2.5f));

	giveEntityRectangularCollisionBounds(result, -0.025f, 0.1f,
										 result->renderSize.x * 0.55f, result->renderSize.y * 0.75f);

	result->texture = &gameState->virus1Stand;

	addFlags(result, EntityFlag_moveable|
					 EntityFlag_collidable|
					 EntityFlag_facesLeft);

	result->p += result->renderSize / 2;

	return result;
}

Entity* addBlueEnergy(GameState* gameState, V2 p) {
	Entity* result = addEntity(gameState, EntityType_blueEnergy, DrawOrder_blueEnergy, p, v2(0.8f, 0.8f));

	giveEntityRectangularCollisionBounds(result, 0, 0, 
										 result->renderSize.x * 0.5f, result->renderSize.y * 0.65f);

	result->texture = &gameState->blueEnergy;

	addFlags(result, EntityFlag_moveable|
					 EntityFlag_collidable);

	result->p += result->renderSize / 2;

	return result;
}

Entity* addBackground(GameState* gameState) {
	Entity* result = addEntity(gameState, EntityType_background, DrawOrder_background, 
								  v2(0, 0), v2(gameState->mapSize.x, 
								(float)gameState->windowHeight / (float)gameState->pixelsPerMeter));

	result->p = result->renderSize / 2.0f;

	return result;
}

Entity* addTile(GameState* gameState, V2 p, Texture* texture) {
	Entity* result = addEntity(gameState, EntityType_tile, DrawOrder_tile, p, v2(gameState->tileSize, gameState->tileSize));

	giveEntityRectangularCollisionBounds(result, 0, 0, 
										 result->renderSize.x, result->renderSize.y);

	result->texture = texture;

	addFlags(result, EntityFlag_collidable| 
				     EntityFlag_solid);

	return result;
}

Entity* addText(GameState* gameState, V2 p, char* msg) {
	Entity* result = addEntity(gameState, EntityType_text, DrawOrder_text, p, v2(0, 0));
	result->texture = (Texture*)malloc(sizeof(Texture));
	*(result->texture) = createText(gameState, gameState->font, msg);
	result->renderSize = v2((float)result->texture->srcRect.w, (float)result->texture->srcRect.h) / gameState->pixelsPerMeter;
	return result;
}

Entity* addLaserBolt(GameState* gameState, V2 p, V2 target) {
	Entity* result = addEntity(gameState, EntityType_laserBolt, DrawOrder_laserBolt, p, v2(1, 1));
	result->texture = &gameState->laserBolt;

	giveEntityRectangularCollisionBounds(result, 0, 0, 
										 result->renderSize.x * 0.3f, result->renderSize.y * 0.3f);

	float speed = 3.0f;
	result->dP = normalize(target - p) * speed;

	addFlags(result, EntityFlag_moveable|
					 EntityFlag_collidable|
					 EntityFlag_ignoresGravity);
					 
	return result;
}

bool collidesWith(Entity* a, Entity* b) {
	bool result = true;

	switch(a->type) {
		case EntityType_player: {
			if (b->type == EntityType_virus) result = false;
		} break;
		case EntityType_laserBolt: {
			if (b->type == EntityType_blueEnergy ||
				b->type == EntityType_virus ||
				b->type == EntityType_laserBolt) result = false;
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
				addFlags(entity, EntityFlag_remove);
				clearFlags(entity, EntityFlag_collidable);
			}
		} break;
		case EntityType_laserBolt: {
			addFlags(entity, EntityFlag_remove);
			clearFlags(entity, EntityFlag_collidable);
		} break;
	}
}

struct GetCollisionTimeResult {
	float collisionTime;
	Entity* hitEntity;
	V2 lineCollider;
	bool hitSolidEntity;
};

GetCollisionTimeResult getCollisionTime(Entity* entity, GameState* gameState, V2 delta) {
	GetCollisionTimeResult result = {};
	result.collisionTime = 1;

	R2 entityDrawBox = rectCenterDiameter(entity->p, entity->renderSize);
	entityDrawBox = addRadiusTo(entityDrawBox, v2(abs(delta.x), abs(delta.y)));

	for (int colliderIndex = 0; colliderIndex < gameState->numEntities; colliderIndex++) {
		Entity* collider = gameState->entities + colliderIndex;
		
		if (collider != entity) {

			if (isSet(collider, EntityFlag_collidable) &&
				collidesWith(entity, collider) && collidesWith(collider, entity)) {

				R2 colliderDrawBox = rectCenterDiameter(collider->p, collider->renderSize);

				if (rectanglesOverlap(entityDrawBox, colliderDrawBox)) {
					int polygonSumCount = 0;

					addPolygons(collider->p, entity->collisionPoints, entity->numCollisionPoints,
								collider->collisionPoints, collider->numCollisionPoints,
								gameState->polygonSum, arrayCount(gameState->polygonSum), &polygonSumCount);

					for (int vertexIndex = 0; vertexIndex < polygonSumCount; vertexIndex++) {
						V2* lp1 = gameState->polygonSum + vertexIndex;
						V2* lp2 = gameState->polygonSum + (vertexIndex + 1) % polygonSumCount;

						float testCollisionTime = raycastLine(entity->p, delta, *lp1, *lp2);

						if (testCollisionTime >= 0) {
							if(testCollisionTime < result.collisionTime) {
								result.collisionTime = testCollisionTime;
								result.lineCollider = *lp1 - *lp2;
								result.hitEntity = collider;
							}

							if(testCollisionTime < 1 && isSet(collider, EntityFlag_solid)) {
								result.hitSolidEntity = true;
							}
						}
					}
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
	if (isSet(entity, EntityFlag_moveable)) {
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
				V2 lineNormal = normalize(rotate90(collisionResult.lineCollider));

				if (isSet(collisionResult.hitEntity, EntityFlag_solid)) {
					delta -= innerProduct(delta, lineNormal) * lineNormal;
        			entity->dP -= innerProduct(entity->dP, lineNormal) * lineNormal;
        			ddP -= innerProduct(ddP, lineNormal) * lineNormal;
				}

				onCollide(entity, collisionResult.hitEntity);
				onCollide(collisionResult.hitEntity, entity);
			}
		}

		entity->dP += ddP * dt;
	}
}

void updateAndRenderEntities(GameState* gameState, float dtForFrame) {
	float dt = gameState->consoleEntityRef ? 0 : dtForFrame;
	V2 oldCameraP = gameState->cameraP;

	for (int entityIndex = 0; entityIndex < gameState->numEntities; entityIndex++) {
		Entity* entity = gameState->entities + entityIndex;
		entity->animTime += dt;

		V2 ddP = {};
		if (!isSet(entity, EntityFlag_ignoresGravity)) ddP = gameState->gravity;

		if(entity->numFields) {
			if (gameState->input.leftMouseJustPressed) {
				R2 clickBox = rectCenterDiameter(entity->p, entity->renderSize);
				bool mouseInside = pointInsideRect(clickBox, gameState->input.mouseInWorld);

				if (mouseInside) {
					addFlags(entity, EntityFlag_consoleSelected);
					gameState->consoleEntityRef = entity->ref;
				} else {
					if (isSet(entity, EntityFlag_consoleSelected)) {
						gameState->consoleEntityRef = 0;
						clearFlags(entity, EntityFlag_consoleSelected);
					}
				}
			}
		}

		switch(entity->type) {
			case EntityType_player: {
				//NOTE: This controls the player with keyboard input
				float xMove = 0;
				float xMoveAcceleration = 60.0f;

				if (gameState->input.rightPressed) xMove += xMoveAcceleration;
				if (gameState->input.leftPressed) xMove -= xMoveAcceleration;

				ddP.x += xMove;

				if (xMove < 0) clearFlags(entity, EntityFlag_facesLeft);
				else if (xMove > 0) addFlags(entity, EntityFlag_facesLeft);

				entity->dP.x *= (float)pow(E, -15.0 * dt);

				if (onGround(entity, gameState, dt) && gameState->input.upPressed) {
					entity->dP.y = 4.5f;
				}

				//NOTE: This centers the player with the camera on x
				float windowWidth = (float)(gameState->windowWidth / gameState->pixelsPerMeter);
				float maxCameraX = gameState->mapSize.x - windowWidth;
				gameState->cameraP.x = clamp((float)(entity->p.x - windowWidth / 2.0), 0, maxCameraX);


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
				//NOTE: This shoots a projectile using the player as a target if the target is nearby
				Entity* player = getEntityByRef(gameState, gameState->playerRef);

				float dstToPlayer = dstSq(player->p, entity->p);
				float shootRadius = square(2.0f);

				bool shouldShoot = isSet(entity, EntityFlag_shooting) ||
								   isSet(entity, EntityFlag_unchargingAfterShooting) || 
								   dstToPlayer <= shootRadius;

				if (shouldShoot) {
					if (player->p.x > entity->p.x) addFlags(entity, EntityFlag_facesLeft);
					else clearFlags(entity, EntityFlag_facesLeft);

					if (isSet(entity, EntityFlag_unchargingAfterShooting)) {
						//NOTE: Two has to be subtracted here because the dt is added once above
						//		This ensure that animTime is decreasing by dt every frame
						entity->animTime -= 2 * dt;

						if (entity->animTime <= 0) {
							entity->animTime = 0;
							clearFlags(entity, EntityFlag_unchargingAfterShooting);
						} 
					} else if (!isSet(entity, EntityFlag_shooting)) {
						entity->animTime = 0;
						addFlags(entity, EntityFlag_shooting);
					}

					entity->texture = getAnimationFrame(&gameState->virus1Shoot, entity->animTime);
					float animationDuration = getAnimationDuration(&gameState->virus1Shoot);

					if (entity->animTime >= animationDuration) {
						addLaserBolt(gameState, entity->p, player->p);
						entity->animTime = animationDuration;
						clearFlags(entity, EntityFlag_shooting);
						addFlags(entity, EntityFlag_unchargingAfterShooting);
					}
				} else {
					//TODO: Move the entity back and forth horizontally

					float xMoveAcceration = 60.0f;
					float friction = -15.0f;
					
					entity->dP.x *= (float)pow(E, friction * dt);

					ddP.x = xMoveAcceration;
					if (!isSet(entity, EntityFlag_facesLeft)) ddP.x *= -1;

					if (entity->dP.x == 0 || !onGround(entity, gameState, dt, entity->dP, ddP)) {
						ddP.x *= -1;
						entity->dP.x = 0;
						toggleFlags(entity, EntityFlag_facesLeft);
					}

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
				float mgWidth = max(gameState->mapSize.x, bgHeight * (mgTexWidth / mgTexHeight));
								
				float bgWidth = mgWidth / bgScrollRate - 1;

				float bgX = gameState->cameraP.x * (1 -  bgScrollRate);

				R2 bgBounds = r2(v2(bgX, 0), v2(bgWidth, bgHeight));
				R2 mgBounds = r2(v2(0, 0), v2(mgWidth, bgHeight));

				drawTexture(gameState, bg, translateRect(bgBounds, -gameState->cameraP), false);
				drawTexture(gameState, mg, translateRect(mgBounds, -gameState->cameraP), false);
			} break;

			case EntityType_blueEnergy:
			case EntityType_tile:
			case EntityType_null: {

			} break;
		}

		move(entity, dt, gameState, ddP);

		/*if (entity->type == EntityType_Player) {
			R2 rect = rectCenterRadius(entity->p, v2(0.05f, 0.05f));
			drawFilledRect(renderer, rect, gameState->oldCameraP);
		}*/

		if (!isSet(entity, EntityFlag_remove) && entity->texture != NULL) { 
			drawTexture(gameState, entity->texture, 
						translateRect(rectCenterDiameter(entity->p, entity->renderSize), -gameState->cameraP), 
						isSet(entity, EntityFlag_facesLeft));
		}

		#if SHOW_COLLISION_BOUNDS
			if (entity->numCollisionPoints) {
				drawPolygon(gameState, entity->collisionPoints, entity->numCollisionPoints,
							 hadamard(getPolygonCenter(entity->collisionPoints, entity->numCollisionPoints), v2(1, -2))
							 + entity->p - gameState->cameraP);
			}
		#endif

		if (gameState->consoleEntityRef) {
			Entity* consoleEntity = getEntityByRef(gameState, gameState->consoleEntityRef);

			for (int fieldIndex = 0; fieldIndex < consoleEntity->numFields; fieldIndex++) {
				ConsoleField* field = consoleEntity->fields + fieldIndex;

				char strBuffer[100];
				_itoa_s((int)(consoleEntity->dP.y * 1000000), strBuffer, 10);
				drawText(gameState, gameState->font, strBuffer, 3, 3, gameState->cameraP);
			}
		}
	}

	removeEntities(gameState);
}
