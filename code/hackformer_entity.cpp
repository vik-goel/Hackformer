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

void addFlags(Entity* entity, uint32 flags) {
	entity->flags |= flags;
} 

void clearFlags(Entity* entity, uint32 flags) {
	entity->flags &= ~flags;
} 

bool isSet(Entity* entity, uint32 flags) {
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

Entity* addEntity(GameState* gameState, EntityType type, DrawOrder drawOrder, V2 p, V2 renderSize) {
	assert(gameState->numEntities < arrayCount(gameState->entities));

	Entity* result = gameState->entities + gameState->numEntities;

	*result = {};
	result->entityIndex = gameState->numEntities;
	result->type = type;
	result->drawOrder = drawOrder;
	result->p = p;
	result->renderSize = renderSize;

	gameState->numEntities++;

	return result;
}

Entity* addPlayer(GameState* gameState, V2 p) {
	Entity* player = addEntity(gameState, EntityType_player, DrawOrder_player, p, v2(1.4f, 1.4f));

	giveEntityRectangularCollisionBounds(player, 0, 0,
										 player->renderSize.x * 0.4f, player->renderSize.y);

	player->texture = &gameState->playerStand;

	addFlags(player, EntityFlag_moveable|
					 EntityFlag_collidable|
					 EntityFlag_solid|
					 EntityFlag_facesLeft);

	player->numFields = 1;
	assert(arrayCount(player->fields) > player->numFields);

	float playerSpeedValues[] = {3, 8, 13, 18, 23};
	player->fields[0] = createFloatField("speed", playerSpeedValues, arrayCount(playerSpeedValues), 2);

	player->p += player->renderSize / 2;

	return player;
}

Entity* addVirus(GameState* gameState, V2 p) {
	Entity* virus = addEntity(gameState, EntityType_virus, DrawOrder_virus, p, v2(2.5f, 2.5f));

	giveEntityRectangularCollisionBounds(virus, -0.025f, 0.1f,
										 virus->renderSize.x * 0.55f, virus->renderSize.y * 0.75f);

	virus->texture = &gameState->virus1Stand;

	addFlags(virus, EntityFlag_moveable|
					EntityFlag_collidable);

	virus->p += virus->renderSize / 2;

	return virus;
}

Entity* addBlueEnergy(GameState* gameState, V2 p) {
	Entity* blueEnergy = addEntity(gameState, EntityType_blueEnergy, DrawOrder_blueEnergy, p, v2(0.8f, 0.8f));

	giveEntityRectangularCollisionBounds(blueEnergy, 0, 0, 
										 blueEnergy->renderSize.x * 0.5f, blueEnergy->renderSize.y * 0.65f);

	blueEnergy->texture = &gameState->blueEnergy;

	addFlags(blueEnergy, EntityFlag_moveable|
						 EntityFlag_collidable);

	blueEnergy->p += blueEnergy->renderSize / 2;

	return blueEnergy;
}

Entity* addBackground(GameState* gameState) {
	Entity* background = addEntity(gameState, EntityType_background, DrawOrder_background, 
								  v2(0, 0), v2(gameState->mapSize.x, 
								(float)gameState->windowHeight / (float)gameState->pixelsPerMeter));

	background->p = background->renderSize / 2.0f;

	return background;
}

Entity* addTile(GameState* gameState, V2 p, Texture* texture) {
	Entity* tile = addEntity(gameState, EntityType_tile, DrawOrder_tile, p, v2(gameState->tileSize, gameState->tileSize));

	giveEntityRectangularCollisionBounds(tile, 0, 0, 
										 tile->renderSize.x, tile->renderSize.y);

	tile->texture = texture;

	addFlags(tile, EntityFlag_collidable| 
				   EntityFlag_solid);

	return tile;
}

Entity* addText(GameState* gameState, V2 p, char* msg) {
	Entity* text = addEntity(gameState, EntityType_text, DrawOrder_text, p, v2(0, 0));
	text->texture = (Texture*)malloc(sizeof(Texture));
	*(text->texture) = createText(gameState, gameState->font, msg);
	text->renderSize = v2((float)text->texture->srcRect.w, (float)text->texture->srcRect.h) / gameState->pixelsPerMeter;
	return text;
}

int renderOrderCompare(const void* elem1, const void* elem2) {
	//TODO: This should do something when comparing entities with the same 
	//		draw order so that removing an entity from the list doesn't cause
	//		other entities to suddenly change their z order 
	//		for example if two viruses are overlapping, the one which is on top
	//		should be consistent

	DrawCall* a = (DrawCall*)elem1;
	DrawCall* b = (DrawCall*)elem2;

	if (a->drawOrder < b->drawOrder) return -1;
	if (a->drawOrder > b->drawOrder) return 1;

	return 0;
}

void drawEntities(GameState* gameState) {
	qsort(gameState->drawCalls, gameState->numDrawCalls, sizeof(DrawCall), renderOrderCompare);

	for (int drawIndex = 0; drawIndex < gameState->numDrawCalls; drawIndex++) {
		DrawCall* draw = gameState->drawCalls + drawIndex;
		drawTexture(gameState, draw->texture, 
					subtractFromRect(draw->bounds, gameState->cameraP), draw->flipX);

		#if SHOW_COLLISION_BOUNDS
			if (draw->numCollisionPoints) {
				drawPolygon(gameState, draw->collisionPoints, draw->numCollisionPoints,
							 hadamard(getPolygonCenter(draw->collisionPoints, draw->numCollisionPoints), v2(1, -2))
							 + draw->p - gameState->cameraP);
			}
		#endif
	}

	if (gameState->consoleEntity) {
		for (int fieldIndex = 0; fieldIndex < gameState->consoleEntity->numFields; fieldIndex++) {
			ConsoleField* field = gameState->consoleEntity->fields + fieldIndex;

			char strBuffer[100];
			_itoa_s((int)(gameState->consoleEntity->dP.y * 1000000), strBuffer, 10);
			drawText(gameState, gameState->font, strBuffer, 3, 3, gameState->cameraP);
		}
	}
}

bool collidesWith(Entity* a, Entity* b) {
	bool result = true;

	switch(a->type) {
		case EntityType_player: {
			if (b->type == EntityType_virus) result = false;
		} break;
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

	for (int colliderIndex = 1; colliderIndex < gameState->numEntities; colliderIndex++) {
		if (colliderIndex != entity->entityIndex) {
			Entity* collider = gameState->entities + colliderIndex;

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

bool onGround(Entity* entity, GameState* gameState, float dt) {
	V2 gravityDelta = 0.5f * dt * dt * gameState->gravity;
	GetCollisionTimeResult collisionResult = getCollisionTime(entity, gameState, gravityDelta);
	return collisionResult.hitSolidEntity;
}

void move(Entity* entity, float dt, GameState* gameState, V2 ddP) {
	if (isSet(entity, EntityFlag_moveable)) {
		V2 delta = entity->dP * dt + (float)0.5 * ddP * dt * dt;
		
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

void removeEntities(GameState* gameState) {
	//NOTE: Entities are removed here if their remove flag is set
	//		This may change the order of the entities in the arra
	//NOTE: There is a memory leak here if the entity allocated anything
	//		Ensure this is cleaned up, the console is a big place where leaks can happen
	for (int entityIndex = 0; entityIndex < gameState->numEntities; entityIndex++) {
		Entity* entity = gameState->entities + entityIndex;
		Entity* last = gameState->entities + (gameState->numEntities - 1);

		if (isSet(entity, EntityFlag_remove)) {
			switch(entity->type) {
				case EntityType_text: {
					SDL_DestroyTexture(entity->texture->tex);
					free(entity->texture);
				} break;
			}

			*entity = *last;
			entity->entityIndex = entityIndex;
			last->type = EntityType_null;

			gameState->numEntities--;
			entityIndex--;
		}
	}
}

void drawTexture(Entity* entity, GameState* gameState, Texture* texture, R2 bounds, int drawOrder) {
	assert(getRectWidth(bounds) > 0 && getRectHeight(bounds) > 0);

	DrawCall* draw = gameState->drawCalls + gameState->numDrawCalls++;
	assert(gameState->numDrawCalls < arrayCount(gameState->drawCalls));

	draw->drawOrder = drawOrder;
	draw->flipX = isSet(entity, EntityFlag_facesLeft);
	draw->texture = texture;
	draw->bounds = bounds;

	#if SHOW_COLLISION_BOUNDS
		draw->p = entity->p;
		draw->collisionPoints = entity->collisionPoints;
		draw->numCollisionPoints = entity->numCollisionPoints;
	#endif
}

void drawTexture(Entity* entity, GameState* gameState, Texture* texture) {
	drawTexture(entity, gameState, texture, rectCenterDiameter(entity->p, entity->renderSize), entity->drawOrder);
}

void updateAndRenderEntities(GameState* gameState, float dtForFrame) {
	float dt = gameState->consoleEntity ? dtForFrame / 5.0f : dtForFrame;
	gameState->numDrawCalls = 0;

	V2 oldCameraP = gameState->cameraP;

	for (int entityIndex = 1; entityIndex < gameState->numEntities; entityIndex++) {
		Entity* entity = gameState->entities + entityIndex;
		entity->animTime += dt;

		V2 ddP = gameState->gravity;

		if(entity->numFields) {
			if (gameState->input.leftMouseJustPressed) {
				R2 clickBox = rectCenterDiameter(entity->p, entity->renderSize);
				bool mouseInside = isPointInsideRect(clickBox, gameState->input.mouseInWorld);

				if (mouseInside) {
					addFlags(entity, EntityFlag_consoleSelected);
					gameState->consoleEntity = entity;
				} else {
					if (isSet(entity, EntityFlag_consoleSelected)) {
						gameState->consoleEntity = NULL;
						clearFlags(entity, EntityFlag_consoleSelected);
					}
				}
			}
		}

		switch(entity->type) {
			case EntityType_player: {
				float xMove = 0;
				float xMoveAcceleration = 60.0f;

				if (gameState->input.rightPressed) xMove += xMoveAcceleration;
				if (gameState->input.leftPressed) xMove -= xMoveAcceleration;

				ddP.x += xMove;

				if (xMove < 0) clearFlags(entity, EntityFlag_facesLeft);
				else if (xMove > 0) addFlags(entity, EntityFlag_facesLeft);

				entity->dP.x *= (float)pow(E, -15.0 * dt);

				float windowWidth = (float)(gameState->windowWidth / gameState->pixelsPerMeter);
				float maxCameraX = gameState->mapSize.x - windowWidth;
				gameState->cameraP.x = clamp((float)(entity->p.x - windowWidth / 2.0), 0, maxCameraX);

				if (onGround(entity, gameState, dt)) {
					if (gameState->input.upPressed) {
						entity->dP.y = 4.5f;
						entity->texture = &gameState->playerJump;
					} else {
						if (abs(entity->dP.x) < 0.25f) {
							entity->texture = &gameState->playerStand;
							entity->animTime = 0;
						} else {
							entity->texture = getAnimationFrame(&gameState->playerWalk, entity->animTime);
						}
					}
				} else {
					entity->texture = &gameState->playerJump;
					entity->animTime = 0;
				}
			} break;
			case EntityType_virus: {
				
			} break;
			case EntityType_background: {
				Texture* bg = &gameState->sunsetCityBg;
				Texture* mg = &gameState->sunsetCityMg;

				float bgTexWidth = (float)bg->srcRect.w;
				float mgTexWidth = (float)mg->srcRect.w;
				float mgTexHeight = (float)mg->srcRect.h;

				float bgScrollRate = bgTexWidth / mgTexWidth;
				float bgHeight = gameState->windowHeight / gameState->pixelsPerMeter;
				float mgWidth = gameState->mapSize.x;//max(gameState->mapSize.x, bgHeight * (mgTexWidth / mgTexHeight));
								
				float bgWidth = mgWidth / bgScrollRate - 1;

				float bgX = gameState->cameraP.x * (1 -  bgScrollRate);

				drawTexture(entity, gameState, bg, r2(v2(bgX, 0), v2(bgWidth, bgHeight)), entity->drawOrder - 1);
				drawTexture(entity, gameState, mg, r2(v2(0, 0), v2(mgWidth, bgHeight)), entity->drawOrder);
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
			drawTexture(entity, gameState, entity->texture);
		}
	}

	removeEntities(gameState);
	drawEntities(gameState);	
}