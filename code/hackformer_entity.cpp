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

bool isSet(Entity* entity, uint flags) {
	bool result = (entity->flags & flags) != 0;
	return result;
}

Entity* addEntity(GameState* gameState, EntityType type, DrawOrder drawOrder, V2 p, V2 renderSize) {
	assert(gameState->numEntities < ARRAY_COUNT(gameState->entities));

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

void giveEntityRectangularCollisionBounds(Entity* entity, float xOffset, float yOffset, float width, float height) {
	entity->numCollisionPoints = 4;
	assert(ARRAY_COUNT(entity->collisionPoints) > entity->numCollisionPoints);

	entity->collisionPoints[0].x = -width / 2 + xOffset;
	entity->collisionPoints[0].y = -height / 2 + yOffset;

	entity->collisionPoints[1].x = -width / 2 + xOffset;
	entity->collisionPoints[1].y = height / 2 + yOffset;

	entity->collisionPoints[2].x = width / 2 + xOffset;
	entity->collisionPoints[2].y = height / 2 + yOffset;

	entity->collisionPoints[3].x = width / 2 + xOffset;
	entity->collisionPoints[3].y = -height / 2 + yOffset;
}

Entity* addPlayer(GameState* gameState, V2 p) {
	Entity* player = addEntity(gameState, EntityType_player, DrawOrder_player, p, v2(1.25f, 1.25f));

	giveEntityRectangularCollisionBounds(player, 0, 0,
										 player->renderSize.x * 0.4f, player->renderSize.y);

	player->texture = &gameState->playerStand;

	addFlags(player, EntityFlag_moveable|
					 EntityFlag_collidable|
					 EntityFlag_solid|
					 EntityFlag_facesLeft);

	player->numFields = 1;
	assert(ARRAY_COUNT(player->fields) > player->numFields);

	float playerSpeedValues[] = {3, 8, 13, 18, 23};
	player->fields[0] = createFloatField("speed", playerSpeedValues, ARRAY_COUNT(playerSpeedValues), 2);

	return player;
}

Entity* addBlueEnergy(GameState* gameState, V2 p) {
	Entity* blueEnergy = addEntity(gameState, EntityType_blueEnergy, DrawOrder_blueEnergy, p, v2(0.8f, 0.8f));

	giveEntityRectangularCollisionBounds(blueEnergy, 0, 0, 
										 blueEnergy->renderSize.x * 0.5f, blueEnergy->renderSize.y * 0.65f);

	blueEnergy->texture = &gameState->blueEnergy;

	addFlags(blueEnergy, EntityFlag_moveable|
						 EntityFlag_collidable);

	return blueEnergy;
}

Entity* addBackground(GameState* gameState) {
	Entity* background = addEntity(gameState, EntityType_background, DrawOrder_background, 
								  v2(0, 0), v2(0, 0));
	background->texture = &gameState->background;

	float textureAspectRatio = (float)background->texture->srcRect.w / (float)background->texture->srcRect.h;
	float height = WINDOW_HEIGHT / PIXELS_PER_METER;
	float width = height * textureAspectRatio;

	background->renderSize = v2(width, height);

	background->p = background->renderSize / 2;
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

void move(Entity* entity, float dt, GameState* gameState, V2 ddP) {
	if (isSet(entity, EntityFlag_moveable)) {
		V2 delta = entity->dP * dt + (float)0.5 * ddP * dt * dt;
		
		float maxCollisionTime = 1;

		for (int moveIteration = 0; moveIteration < 4 && maxCollisionTime > 0; moveIteration++) {
			float collisionTime = maxCollisionTime;

			V2 lineCollider;
			Entity* hitEntity = NULL;

			for (int colliderIndex = 1; colliderIndex < gameState->numEntities; colliderIndex++) {
				if (colliderIndex != entity->entityIndex) {
					Entity* collider = gameState->entities + colliderIndex;

					if (isSet(collider, EntityFlag_collidable)) {
						int polygonSumCount = 0;

						addPolygons(collider->p, entity->collisionPoints, entity->numCollisionPoints,
									collider->collisionPoints, collider->numCollisionPoints,
									gameState->polygonSum, ARRAY_COUNT(gameState->polygonSum), &polygonSumCount);

						for (int vertexIndex = 0; vertexIndex < polygonSumCount; vertexIndex++) {
							V2* lp1 = gameState->polygonSum + vertexIndex;
							V2* lp2 = gameState->polygonSum + (vertexIndex + 1) % polygonSumCount;

							bool hitLine = raycastLine(entity->p, delta, *lp1, *lp2, &collisionTime);

							if (hitLine) {
								lineCollider = *lp1 - *lp2;
								hitEntity = collider;
							}
						}

						/*if(entity->type == EntityType_Player) {
							drawPolygon(gameState->renderer, polygonSum, polygonSumCount, v2(0, 0));
						}*/
					}
				}
			}

			maxCollisionTime -= collisionTime;
			float collisionTimeEpsilon = 0.001f; //TODO: More exact collision detection
			float moveTime = max(0, collisionTime - collisionTimeEpsilon);

			V2 movement = delta * moveTime;
			entity->p += movement;

			if (hitEntity) {
				V2 lineNormal = normalize(rotate90(lineCollider));

				if (isSet(hitEntity, EntityFlag_solid)) {
					delta -= innerProduct(delta, lineNormal) * lineNormal;
        			entity->dP -= innerProduct(entity->dP, lineNormal) * lineNormal;
        			ddP -= innerProduct(ddP, lineNormal) * lineNormal;
				}

				onCollide(entity, hitEntity);
				onCollide(hitEntity, entity);
			}
		}
	}

	entity->dP += ddP * dt;
}

void removeEntities(GameState* gameState) {
	//NOTE: Entities are removed here if their remove flag is set
	//		This may change the order of the entities in the arra
	//NOTE: There is a memory leak here if the entity allocated anything
	//		Ensure this is cleaned up
	for (int entityIndex = 0; entityIndex < gameState->numEntities; entityIndex++) {
		Entity* entity = gameState->entities + entityIndex;

		Entity* last = gameState->entities + (gameState->numEntities - 1);

		if (isSet(entity, EntityFlag_remove)) {
			if (entityIndex != gameState->numEntities - 1) {
				*entity = *last;
			}

			last->type = EntityType_null;

			gameState->numEntities--;
			entityIndex--;
		}
	}
}

int renderOrderCompare(const void* elem1, const void* elem2) {
	DrawCall* a = (DrawCall*)elem1;
	DrawCall* b = (DrawCall*)elem2;

	if (a->drawOrder < b->drawOrder) return -1;
	if (a->drawOrder > b->drawOrder) return 1;
	return 0;
}

void updateAndRenderEntities(GameState* gameState, float dtForFrame) {
	float dt = gameState->consoleEntity ? dtForFrame / 5.0f : dtForFrame;
	int numDrawCalls = 0;

	for (int entityIndex = 1; entityIndex < gameState->numEntities; entityIndex++) {
		Entity* entity = gameState->entities + entityIndex;
		entity->animTime += dt;

		V2 ddP = {0, -9.8f};

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
				float xMoveAcceleration = 17.0f;

				if (gameState->input.rightPressed) xMove += xMoveAcceleration;
				if (gameState->input.leftPressed) xMove -= xMoveAcceleration;

				ddP.x = xMove;

				if (xMove < 0) clearFlags(entity, EntityFlag_facesLeft);
				else if (xMove > 0) addFlags(entity, EntityFlag_facesLeft);

				entity->dP.x *= (float)pow(E, -9.0 * dt);

				float windowWidth = (float)(WINDOW_WIDTH / PIXELS_PER_METER);
				float maxCameraX = gameState->mapWidth - windowWidth;
				gameState->cameraP.x = clamp((float)(entity->p.x - windowWidth / 2.0), 0, maxCameraX);

				//TODO: This will allow players to jump when they hit the ceiling
				//		and when they are at the apex of a jump, need to fix this
				if (entity->dP.y == 0) {
					if (gameState->input.upPressed) {
						entity->dP.y = 4.2f;
						entity->texture = &gameState->playerJump;
					} else {
						if (abs(entity->dP.x) < 0.5f) {
							entity->texture = &gameState->playerStand;
							entity->animTime = 0;
						} else {
							getAnimationFrame(&gameState->playerWalk, entity->animTime, &entity->texture);
						}
					}
				} else {
					entity->texture = &gameState->playerJump;
					entity->animTime = 0;
				}
			} break;
			case EntityType_blueEnergy:
			case EntityType_tile:
			case EntityType_null:
			case EntityType_background: {

			} break;
		}

		move(entity, dt, gameState, ddP);

		/*if (entity->type == EntityType_Player) {
			R2 rect = rectCenterRadius(entity->p, v2(0.05f, 0.05f));
			drawFilledRect(renderer, rect, gameState->oldCameraP);
		}*/

		if (!isSet(entity, EntityFlag_remove)) {
			assert(numDrawCalls + 1 < ARRAY_COUNT(gameState->drawCalls));
			DrawCall* draw = gameState->drawCalls + numDrawCalls++;

			draw->drawOrder = entity->drawOrder;
			draw->flipX = isSet(entity, EntityFlag_facesLeft);
			draw->texture = entity->texture;
			draw->bounds = rectCenterDiameter(entity->p, entity->renderSize);

			#if SHOW_COLLISION_BOUNDS
				draw->collisionPoints = &entity->collisionPoints;
				draw->numCollisionPoints = entity->numCollisionPoints;
			#endif
		}
	}

	removeEntities(gameState);

	qsort(gameState->drawCalls, numDrawCalls, sizeof(DrawCall), renderOrderCompare);

	for (int drawIndex = 0; drawIndex < numDrawCalls; drawIndex++) {
		DrawCall* draw = gameState->drawCalls + drawIndex;
		drawTexture(gameState->renderer, draw->texture, 
					subtractFromRect(draw->bounds, gameState->cameraP), draw->flipX);

		#if SHOW_COLLISION_BOUNDS
			if (draw->collisionPoints) {
				drawPolygon(renderer, *draw->collisionPoints, draw->numCollisionPoints,
							 getRectCenter(draw->bounds) - gameState->cameraP);
			}
		#endif
	}

	if (gameState->consoleEntity) {
		for (int fieldIndex = 0; fieldIndex < gameState->consoleEntity->numFields; fieldIndex++) {
			ConsoleField* field = gameState->consoleEntity->fields + fieldIndex;

			char strBuffer[100];
			_itoa_s((int)(gameState->consoleEntity->dP.y * 1000000), strBuffer, 10);
			drawText(gameState->renderer, gameState->font, strBuffer, 3, 3, gameState->cameraP);
		}
	}
}