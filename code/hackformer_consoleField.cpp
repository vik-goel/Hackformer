void freeConsoleField(ConsoleField* field, GameState* gameState) {
	// if (field->next) {
	// 	freeConsoleField(field->next, gameState);
	// 	field->next = NULL;
	// }

	//NOTE: Since the next pointer is only used in the free list and consoles in the free list should
	//		never be freed again, the next pointer should be null. 
	assert(!field->next);

	for (s32 childIndex = 0; childIndex < field->numChildren; childIndex++) {
		freeConsoleField(field->children[childIndex], gameState);
		field->children[childIndex] = NULL;
	}

	field->next = gameState->consoleFreeList;
	gameState->consoleFreeList = field;
}

ConsoleField* createConsoleField_(GameState* gameState) {
	ConsoleField* result = NULL;

	if (gameState->consoleFreeList) {
		result = gameState->consoleFreeList;
		gameState->consoleFreeList = gameState->consoleFreeList->next;
	} else {
		result = (ConsoleField*)pushStruct(&gameState->levelStorage, ConsoleField);
	}

	return result;
}

ConsoleField* createConsoleField(GameState* gameState, ConsoleField* copy) {
	ConsoleField* result = createConsoleField_(gameState);
	*result = *copy;
	return result;
}

ConsoleField* createConsoleField(GameState* gameState, char* name, ConsoleFieldType type, s32 tweakCost) {
	ConsoleField* result = createConsoleField_(gameState);

	*result = {};
	result->type = type;
	strcpy(result->name, name);
	result->tweakCost = tweakCost;

	return result;
}

ConsoleField* createUnlimitedIntField(GameState* gameState, char* name, s32 value, s32 tweakCost) {
	ConsoleField* result = createConsoleField(gameState, name, ConsoleField_unlimitedInt, tweakCost);

	result->selectedIndex = result->initialIndex = value;

	return result;
}

#define createEnumField(enumName, gameState, name, value, tweakCost) createEnumField_(gameState, name, value, enumName##_count, tweakCost, ConsoleField_##enumName)
ConsoleField* createEnumField_(GameState* gameState, char* name, s32 value, s32 numValues, s32 tweakCost, ConsoleFieldType type) {
	ConsoleField* result = createConsoleField(gameState, name, type, tweakCost);
	result->initialIndex = result->selectedIndex = value;
	result->numValues = numValues;
	return result;
}

ConsoleField* createBoolField(GameState* gameState, char* name, bool value, s32 tweakCost) {
	ConsoleField* result = createConsoleField(gameState, name, ConsoleField_bool, tweakCost);

	result->initialIndex = result->selectedIndex = value ? 1 : 0;
	result->numValues = 2;

	return result;
}

#define createPrimitiveField(type, gameState, name, values, numValues, initialIndex, tweakCost) createPrimitiveField_(gameState, name, values, sizeof(type), numValues, initialIndex, tweakCost, ConsoleField_##type)
ConsoleField* createPrimitiveField_(GameState* gameState, char* name, void* values, s32 elemSize,
				s32 numValues, s32 initialIndex, s32 tweakCost, ConsoleFieldType type) {
	assert(numValues > 0);
	assert(initialIndex >= 0 && initialIndex < numValues);

	ConsoleField* result = createConsoleField(gameState, name, type, tweakCost);

	for (s32 valueIndex = 0; valueIndex < numValues; valueIndex++) {
		char* src = (char*)values + elemSize * valueIndex;
		char* dst = (char*)result->doubleValues + elemSize * valueIndex;

		for(s32 byteIndex = 0; byteIndex < elemSize; byteIndex++) {
			*dst++ = *src++;
		}
	}

	result->selectedIndex = result->initialIndex = initialIndex;
	result->numValues = numValues;

	return result;
}

bool hasValues(ConsoleField* field) {
	bool result = field->numValues || field->type == ConsoleField_unlimitedInt;
	return result;
}

double getTotalFieldHeight(FieldSpec* spec, ConsoleField* field) {
	double result = spec->fieldSize.y + spec->spacing.y;
	if(hasValues(field)) result += spec->valueSize.y - spec->valueBackgroundPenetration;
	return result;
}

s32 getNumVisibleFields(ConsoleField** fields, s32 fieldsCount) {
	s32 result = fieldsCount;

	for (s32 fieldIndex = 0; fieldIndex < fieldsCount; fieldIndex++) {
		ConsoleField* field = fields[fieldIndex];

		if (field->numChildren && isSet(field, ConsoleFlag_childrenVisible)) {
			result += getNumVisibleFields(field->children, field->numChildren);
		}
	}

	return result;
}

double getMaxChildYOffset(ConsoleField* field, FieldSpec* spec) {
	double result = 0;

	for(s32 childIndex = 0; childIndex < field->numChildren; childIndex++) {
		ConsoleField* child = field->children[childIndex];
		result += getTotalFieldHeight(spec, child);
	}

	return result;
}

double getTotalYOffset(ConsoleField** fields, s32 fieldsCount, FieldSpec* spec, bool isPickupField) {
	double result = 0;

	for (s32 fieldIndex = 0; fieldIndex < fieldsCount; fieldIndex++) {
		ConsoleField* field = fields[fieldIndex];
		result += getTotalFieldHeight(spec, field);
	}

	//NOTE: The 0th fieldIndex is skipped when getting the y offset of a pickup field
	//		because those fields can go below where the entity is. 
	for (s32 fieldIndex = isPickupField; fieldIndex < fieldsCount; fieldIndex++) {
		ConsoleField* field = fields[fieldIndex];
		result += field->childYOffs;
	}

	return result; 
}

void moveToEquilibrium(ConsoleField* field, double dt) {
	if(isSet(field, ConsoleFlag_selected)) return;

	V2 delta = field->offs;
	double len = length(delta);
	double maxSpeed = 1 * dt + len / 10;

	if (len) {
		if (len > maxSpeed) {
			delta = normalize(delta) * maxSpeed;
		}

		field->offs -= delta;
	}
}

void rebaseField(ConsoleField* field, V2 newBase) {
	field->offs = field->p - newBase;
}

R2 getRenderBounds(Entity* entity, GameState* gameState) {
	R2 result = translateRect(entity->clickBox, entity->p - gameState->cameraP);
	return result;
}

V2 getBottomFieldP(Entity* entity, GameState* gameState, FieldSpec* spec) {
	R2 renderBounds = getRenderBounds(entity, gameState);

	V2 entityScreenP = (entity->p - gameState->cameraP) * gameState->pixelsPerMeter;
	bool onScreenRight = entityScreenP.x >= gameState->windowWidth / 2;

	bool isPickupField = entity->type == EntityType_pickupField;
	if(isPickupField) onScreenRight = true;

	//TODO: Get better values for these
	V2 fieldOffset;
	if (onScreenRight) {
		fieldOffset = v2(spec->fieldSize.x * -0.75, 0);
	} else {
		fieldOffset = v2(spec->fieldSize.x * 0.75, 0);
	}

	if(entity->type == EntityType_player && !onScreenRight && !isSet(entity, EntityFlag_facesLeft)) {
		fieldOffset.x += getRectWidth(renderBounds) * 1.5;
	}

	V2 result = fieldOffset + getRectCenter(renderBounds);
	return result;
}

void onAddConsoleFieldToEntity(Entity* entity, ConsoleField* field, bool exchanging, GameState* gameState) {
	switch(entity->type) {
		case EntityType_text: {
			if(!exchanging && isConsoleFieldMovementType(field)) {
				ignoreAllPenetratingEntities(entity, gameState);
			}
		} break;
	}
}

bool moveField(ConsoleField* field, GameState* gameState, double dt, FieldSpec* spec) {
	assert(!hasValues(field));

	bool result = false;

	R2 bounds = rectCenterDiameter(field->p, spec->fieldSize);

	if (gameState->input.leftMouse.justPressed) {
		if (pointInsideRect(bounds, gameState->input.mouseInMeters)) {
			if(!isSet(field, ConsoleFlag_selected)) {
				if(gameState->input.shift.pressed) {
					if(!gameState->swapField && field->type != ConsoleField_cameraFollows) {
						if(spec->blueEnergy >= field->tweakCost) {
							spec->blueEnergy -= field->tweakCost;
							gameState->swapField = createConsoleField(gameState, field);
							rebaseField(gameState->swapField, gameState->swapFieldP); 
						}
					}
				} else {
					spec->mouseOffset = (gameState->input.mouseInMeters - field->p);
					setFlags(field, ConsoleFlag_selected);
				}
			}

			result = true;
		}
	}

	if (isSet(field, ConsoleFlag_selected)) {
		if (gameState->input.leftMouse.pressed) {
			field->offs += gameState->input.mouseInMeters - field->p - spec->mouseOffset;
		} else {
			Entity* consoleEntity = getEntityByRef(gameState, gameState->consoleEntityRef);

			if(consoleEntity) {
				double dstSqToConsoleEntity = dstSq(getBottomFieldP(consoleEntity, gameState, spec), field->p);
				double dstSqToSwapField = dstSq(gameState->swapFieldP, field->p);

				bool addToParent = false;
				bool removeFromParent = false;
				bool removeFromSwapField = false;
				bool addToSwapField = false;
				bool exchangeFields = false;

				if (gameState->swapField == field) {
					if (dstSqToConsoleEntity < dstSqToSwapField) {
						removeFromSwapField = true;
						addToParent = true;
					}
				} else {
					if (dstSqToSwapField < dstSqToConsoleEntity) {
						if (gameState->swapField == NULL) {
							addToSwapField = true;
							removeFromParent = true;
						}
						else if (isConsoleFieldMovementType(gameState->swapField) && isConsoleFieldMovementType(field)) {
							exchangeFields = true;
						}
					}
				}

				assert(!(addToParent && removeFromParent));
				assert(!(addToSwapField && removeFromSwapField));
				assert(!(exchangeFields && (addToParent || removeFromParent || addToSwapField || removeFromSwapField)));

				bool exchangeChangedField = false;

				if (gameState->swapField == field && isConsoleFieldMovementType(field)) {
					if (addToParent) {
						ConsoleField* movementField = getMovementField(consoleEntity);

						if (movementField) {
							field = movementField;
							addToParent = removeFromParent = addToSwapField = removeFromSwapField = false;
							exchangeFields = true;
							exchangeChangedField = true;
						}
					}
				}

				if(addToParent) {
					bool alreadyHaveField = canOnlyHaveOneFieldOfType(field->type) && getField(consoleEntity, field->type);
					if(alreadyHaveField) {
						addToParent = false;
						assert(removeFromSwapField);
						removeFromSwapField = false;
					}
				}

				s32 totalCost = 0;
				bool movementAllowed = true;

				if(exchangeFields || removeFromSwapField) {
					assert(gameState->swapField);
					assert(gameState->swapField->tweakCost > 0);
					totalCost += gameState->swapField->tweakCost;
				}
				if(exchangeFields || removeFromParent) {
					assert(field);

					if(field->tweakCost > 0) {
						totalCost += field->tweakCost;
					} else {
						movementAllowed = false;
					}
				}

				if(spec->blueEnergy < totalCost) movementAllowed = false;

				if(movementAllowed) {
					spec->blueEnergy -= totalCost;

					if (field->type == ConsoleField_isShootTarget) {
						if (removeFromParent) removeTargetRef(gameState->consoleEntityRef, gameState);
						else if (addToParent) addTargetRef(gameState->consoleEntityRef, gameState);
					}

					if(exchangeFields) {
						ConsoleField* swapField = gameState->swapField;

						V2 fieldP = field->p;
						V2 fieldOffs = field->offs;

						gameState->swapField = field;
						rebaseField(gameState->swapField, gameState->swapFieldP);

						bool exchanged = false;

						for(s32 fieldIndex = 0; fieldIndex < consoleEntity->numFields; fieldIndex++) {
							ConsoleField* f = consoleEntity->fields[fieldIndex];

							if(f == field) {
								consoleEntity->fields[fieldIndex] = swapField;

								if(exchangeChangedField) {
									rebaseField(consoleEntity->fields[fieldIndex], field->p);
								} else {
									rebaseField(consoleEntity->fields[fieldIndex], fieldP - fieldOffs);	
								}

								exchanged = true;
								break;
							}
						}

						assert(exchanged);

						onAddConsoleFieldToEntity(consoleEntity, swapField, true, gameState);
					}

					if(addToSwapField) {
						gameState->swapField = field;
						rebaseField(field, gameState->swapFieldP);
					}
					else if(removeFromSwapField) {
						assert(gameState->swapField);
						gameState->swapField = NULL;
					}

					if (addToParent) {
						V2 newBaseP = getBottomFieldP(consoleEntity, gameState, spec);

						for (s32 fieldIndex = 0; fieldIndex < consoleEntity->numFields; fieldIndex++) {
							ConsoleField* f = consoleEntity->fields[fieldIndex];
							f->offs = v2(0, -spec->fieldSize.y - field->childYOffs); 
						}

						addField(consoleEntity, field);
						rebaseField(field, newBaseP);

						onAddConsoleFieldToEntity(consoleEntity, consoleEntity->fields[consoleEntity->numFields - 1], false, gameState);
					}
					else if (removeFromParent) {
						setFlags(field, ConsoleFlag_remove);

						if(field->type == ConsoleField_shootsAtTarget) {
							clearFlags(consoleEntity, EntityFlag_shooting|EntityFlag_unchargingAfterShooting);
						}

						bool encounteredField = false;

						for (s32 fieldIndex = 0; fieldIndex < consoleEntity->numFields; fieldIndex++) {
							ConsoleField* f = consoleEntity->fields[fieldIndex];
							if (f == field) encounteredField = true;
							else if (!encounteredField) f->offs = v2(0, spec->fieldSize.y + field->childYOffs); 
						}
					}
				}

			}

			if(gameState->swapField) clearFlags(gameState->swapField, ConsoleFlag_selected);
			clearFlags(field, ConsoleFlag_selected);
		}
	} else {
		moveToEquilibrium(field, dt);
	}

	return result;
}

void setConsoleFieldSelectedIndex(ConsoleField* field, s32 newIndex, FieldSpec* spec) {
	if (field->type == ConsoleField_unlimitedInt) {
		field->selectedIndex = newIndex;
	} 
	else {
		if (field->type == ConsoleField_bool) {
			field->selectedIndex = 1 - field->selectedIndex;
		} else {
			if (newIndex < 0) newIndex = 0;
			if (newIndex >= field->numValues) newIndex = field->numValues - 1;
			field->selectedIndex = newIndex;
		}

		assert(field->selectedIndex >= 0 && field->selectedIndex < field->numValues);
	}
	
	spec->blueEnergy -= field->tweakCost;
}

bool drawConsoleTriangle(V2 triangleP, RenderGroup* group, FieldSpec* spec, Input* input,
						 bool facesRight, bool facesDown, bool facesUp, bool yellow, ConsoleField* field, s32 tweakCost, bool paused) {
	bool result = false;

	V2 triangleSize = v2(1, 1) * 0.4;

	R2 triangleBounds = rectCenterDiameter(triangleP, triangleSize);

	Texture* triangleTex = &spec->consoleTriangleGrey;

	bool enoughEnergyToHack = !yellow && spec->blueEnergy >= tweakCost;
	if (enoughEnergyToHack) {
		triangleTex = &spec->consoleTriangle;
	}

	Orientation orientation = Orientation_0;

	if (facesDown) orientation = Orientation_90;
	if (facesUp) orientation = Orientation_270;

	if(input && !paused) {
		bool mouseOverTriangle = dstSq(input->mouseInMeters, triangleP) < 
							 square(min(triangleSize.x, triangleSize.y) / 2.0f);

		if (mouseOverTriangle) {
			if (enoughEnergyToHack) {
				triangleTex = &spec->consoleTriangleSelected;
			}

			if (input->leftMouse.justPressed) {
				result = true;

				if (field && enoughEnergyToHack) {
					if (facesDown && tweakCost == 0) {
						toggleFlags(field, ConsoleFlag_childrenVisible);
					} else {
						s32 dSelectedIndex = (facesRight || facesDown) ? -1 : 1;
						setConsoleFieldSelectedIndex(field, field->selectedIndex + dSelectedIndex, spec);
					}
				}
			}
		} 
	}
	

	if (yellow) triangleTex = &spec->consoleTriangleYellow;

	pushTexture(group, triangleTex, triangleBounds, facesRight, DrawOrder_gui, false, orientation);

	//NOTE: This draws the cost of tweaking if it is not default (0 or 1)
	if (tweakCost > 1) {
		char tweakCostStr[25];
		sprintf(tweakCostStr, "%d", tweakCost);
		double costStrWidth = getTextWidth(&spec->consoleFont, group, tweakCostStr);
		V2 costP = triangleBounds.min;

		if (facesDown) {
			costP += v2(spec->triangleSize.x / 2 - costStrWidth / 2, spec->consoleFont.lineHeight * 1.5);
		}
		else if (facesUp) {
			costP += v2(spec->triangleSize.x / 2 - costStrWidth / 2, spec->consoleFont.lineHeight / 2);
		}
		else if (facesRight) {
			costP += v2(spec->triangleSize.x / 2 + costStrWidth / 4, spec->consoleFont.lineHeight);
		}
		else {
			costP += v2(costStrWidth, spec->consoleFont.lineHeight);
		}

		pushText(group, &spec->consoleFont, tweakCostStr, costP);
	}

	return result;
}

bool drawValueArrow(V2 p, ConsoleField* field, RenderGroup* group, Input* input, FieldSpec* spec, bool facesRight) {
	bool clickHandled = false;

	R2 triangleBounds = rectCenterDiameter(p, spec->triangleSize);
	Texture* tex = NULL;

	bool canAfford = spec->blueEnergy >= field->tweakCost;

	bool drawArrow = (field->type == ConsoleField_unlimitedInt);

	if(facesRight) {
		if(field->selectedIndex < field->numValues - 1) drawArrow = true;
	} else {
		if(field->selectedIndex > 0) drawArrow = true;
	}

	if(canAfford && drawArrow) {
		R2 clickBounds = triangleBounds;
		clickBounds.max.y -= spec->fieldSize.y;

		ConsoleFlag flag = facesRight ? ConsoleFlag_wasRightArrowSelected : ConsoleFlag_wasLeftArrowSelected;
		bool32 wasClicked = isSet(field, flag);
		clearFlags(field, flag);

		if(wasClicked || pointInsideRect(clickBounds, input->mouseInMeters)) {
			if(input->leftMouse.pressed) {
				tex = &spec->leftButtonClicked;
				clickHandled = true;
				setFlags(field, flag);
			} else {
				if(wasClicked) {
					setConsoleFieldSelectedIndex(field, field->selectedIndex - 1 + 2 * facesRight, spec);
				}

				//TODO: Different image for hovering over here
				tex = &spec->leftButtonDefault;
			}
		} else {
			tex = &spec->leftButtonDefault;
		}
	} else { 
		tex = &spec->leftButtonUnavailable;
	}	

	assert(tex);
	pushTexture(group, tex, triangleBounds, facesRight, DrawOrder_gui);

	return clickHandled;
}

bool drawFields(GameState* gameState, Entity* entity, double dt, V2* fieldP, FieldSpec* spec, bool paused);

bool drawConsoleField(ConsoleField* field, RenderGroup* group, Input* input, FieldSpec* spec, bool paused, bool drawTriangles) {
	bool result = false;

	if (!isSet(field, ConsoleFlag_remove)) {
		bool hasValue = true;

		switch(field->type) {
			case ConsoleField_s32: {
				sprintf(field->valueStr, "%d", field->intValues[field->selectedIndex]);
			} break;

			case ConsoleField_double: {
				sprintf(field->valueStr, "%.1f", field->doubleValues[field->selectedIndex]);
			} break;

			case ConsoleField_unlimitedInt: {
				sprintf(field->valueStr, "%d", field->selectedIndex);
			} break;

			case ConsoleField_bool: {
				if (field->selectedIndex == 0) {
					sprintf(field->valueStr, "false");
				} else if (field->selectedIndex == 1) {
					sprintf(field->valueStr, "true");
				}
			} break;

			case ConsoleField_Alertness: {
				assert(field->selectedIndex >= 0 && field->selectedIndex < Alertness_count);
				Alertness value = (Alertness)field->selectedIndex;

				switch(value) {
					case Alertness_asleep: {
						sprintf(field->valueStr, "asleep");
					} break;
					case Alertness_patrolling: {
						sprintf(field->valueStr, "patrolling");
					} break;
					case Alertness_searching: {
						sprintf(field->valueStr, "searching");
					} break;
					case Alertness_detected: {
						sprintf(field->valueStr, "detected");
					} break;

					InvalidDefaultCase;
				}
			} break;

			default:
				hasValue = false;
		}

		V2 fieldP = field->p;
		if(hasValues(field)) fieldP += v2(0, spec->valueSize.y - spec->valueBackgroundPenetration);
		R2 fieldBounds = rectCenterDiameter(fieldP, spec->fieldSize);

		if (drawTriangles) {
			if(hasValue) {
				V2 valueP = fieldP + v2(0.1, (spec->valueSize.y + spec->fieldSize.y) * -0.5 + spec->valueBackgroundPenetration);
				R2 valueBounds = rectCenterDiameter(valueP, spec->valueSize);
				pushTexture(group, &spec->valueBackground, valueBounds, false, DrawOrder_gui);

				V2 textP = valueP - v2(0, 0.225);
				pushText(group, &spec->consoleFont, field->valueStr, textP, WHITE, TextAlignment_center);

				V2 leftTriangleP = v2(valueP.x - spec->valueSize.x*0.5, fieldP.y - spec->valueBackgroundPenetration * 0.5);
				V2 rightTriangleP = leftTriangleP + v2(spec->valueSize.x, 0);

				if (drawValueArrow(leftTriangleP, field, group, input, spec, false)) result = true;
				if (drawValueArrow(rightTriangleP, field, group, input, spec, true)) result = true;
			} else {
				V2 triangleP = field->p + v2((spec->triangleSize.x + spec->fieldSize.x) / 2.0f + spec->spacing.x, 0);

				if (field->numChildren &&
					drawConsoleTriangle(triangleP, group, spec, input, 
						false, true, false, false, field, 0, paused)) {
					result = true;
				}
			}
		}

		Texture* fieldTex = hasValues(field) ? &spec->attribute : &spec->behaviour;
		pushTexture(group, fieldTex, fieldBounds, false, DrawOrder_gui);
		
		V2 nameP = fieldP + v2(0.14, -0.04);
		pushText(group, &spec->consoleFont, field->name, nameP, WHITE, TextAlignment_center);

		char tweakCostStr[25];
		sprintf(tweakCostStr, "%d", field->tweakCost);

		V2 costP = fieldP + v2(-1.13, -0.05);
		pushText(group, &spec->consoleFont, tweakCostStr, costP, WHITE, TextAlignment_center);
	}

	return result;
}

void moveFieldChildren(ConsoleField** fields, s32 fieldsCount, FieldSpec* spec, double dt) {
	for (s32 fieldIndex = 0; fieldIndex < fieldsCount; fieldIndex++) {
		ConsoleField* field = fields[fieldIndex];

		if (field->numChildren) {
			double fieldSpeed = 6;
			double delta = fieldSpeed * dt;
			double maxY = getTotalYOffset(field->children, field->numChildren, spec, false);
			double minY = 0;

			if (isSet(field, ConsoleFlag_childrenVisible)) {
				field->childYOffs += delta;
			} else {
				field->childYOffs -= delta;
			}

			field->childYOffs = clamp(field->childYOffs, minY, maxY);
		}
	}
}

bool calcFieldPositions(GameState* gameState, ConsoleField** fields, s32 fieldsCount, double dt, V2* fieldP, FieldSpec* spec, bool isPickupField, bool paused) {
	bool result = false;

	for (s32 fieldIndex = 0; fieldIndex < fieldsCount; fieldIndex++) {
		ConsoleField* field = fields[fieldIndex];
		double fieldHeight = getTotalFieldHeight(spec, field);
		fieldP->y -= fieldHeight;
		field->p = *fieldP + field->offs;

		//NOTE: Currently, fields with values like speed cannot be moved.
		if (!paused && !hasValues(field) && !(isPickupField && fieldIndex == fieldsCount - 1)) {
			if(moveField(field, gameState, dt, spec)) result = true;
		} else {
			moveToEquilibrium(field, dt);
		}

		field->p = *fieldP + field->offs;

		if (!isSet(field, ConsoleFlag_remove)) {
			if (field->numChildren && field->childYOffs) {

				V2 startFieldP = *fieldP;
				fieldP->x += spec->childInset;
				*fieldP += field->offs;

				calcFieldPositions(gameState, field->children, field->numChildren, dt, fieldP, spec, isPickupField, paused);

				*fieldP = startFieldP - v2(0, field->childYOffs);
			}
		} else {
			fieldP->y -= field->childYOffs;
		}
	}

	return result;
}

bool drawFieldsRaw(RenderGroup* group, Input* input, ConsoleField** fields, s32 fieldsCount, FieldSpec* spec, bool paused) {
	bool result = false;

	for (s32 fieldIndex = 0; fieldIndex < fieldsCount; fieldIndex++) {
		ConsoleField* field = fields[fieldIndex];
		if (drawConsoleField(field, group, input, spec, paused)) result = true;

		if (field->numChildren && field->childYOffs) {
			V2 rectSize = v2(spec->fieldSize.x, field->childYOffs);//getTotalYOffset(field->children, field->numChildren, spec, false));
			V2 clipTopRight = field->p + v2(spec->fieldSize.x, -spec->fieldSize.y) * 0.5 + v2(spec->childInset, 0);

			R2 clipRect = r2(clipTopRight, clipTopRight - rectSize);
			pushClipRect(group, clipRect);

			//pushFilledRect(group, clipRect, MAGENTA, false);
			if (drawFieldsRaw(group, input, field->children, field->numChildren, spec, paused)) result = true;

			pushDefaultClipRect(group);
		}
	}

	return result;
}

bool drawFields(GameState* gameState, Entity* entity, s32 fieldSkip, double dt, V2* fieldP, FieldSpec* spec, bool paused) {
	bool result = false;

	for(s32 fieldIndex = 0; fieldIndex < fieldSkip; fieldIndex++) {
		ConsoleField* field = entity->fields[fieldIndex];
		fieldP->y -= getTotalFieldHeight(spec, field);
	}

	if(calcFieldPositions(gameState, entity->fields + fieldSkip, entity->numFields - fieldSkip, dt, fieldP, spec, entity->type == EntityType_pickupField, paused)) {
		result = true;
	}

	if(drawFieldsRaw(gameState->renderGroup, &gameState->input, entity->fields + fieldSkip, entity->numFields - fieldSkip, spec, paused)) {
		result = true;
	}

	return result;
}

void drawWaypointInformation(ConsoleField* field, RenderGroup* group, FieldSpec* spec) {
	if(field->type == ConsoleField_followsWaypoints) {
		if(field->childYOffs > 0) {
			Waypoint* w = field->curWaypoint;
			assert(w);

			int alpha = (int)(field->childYOffs / getMaxChildYOffset(field, spec) * 255.0 + 0.5);

			double waypointSize = 0.125;
			double lineThickness = 0.025;
			double lineDashSize = 0.3;
			double lineSpaceSize = lineDashSize * 0.5;
			Color lineColor = createColor(105, 255, 126, alpha);
			Color currentLineColor = createColor(105, 154, 255, alpha);
			double arrowSize = 0.15;
			V2 arrowDimens = v2(1, 1) * arrowSize;

			Texture* waypointTex = &spec->waypoint;
			Texture* waypointArrowTex = &spec->waypointArrow;

			while(w) {
				Waypoint* next = w->next;
				assert(next);
				
				if(next == field->curWaypoint) {
					lineColor = currentLineColor;
				}

				R2 waypointBounds = rectCenterRadius(w->p, v2(1, 1) * waypointSize);

				pushTexture(group, waypointTex, waypointBounds, false, DrawOrder_gui, true, 
							Orientation_0, lineColor);

				V2 lineStart = w->p;
				V2 lineEnd = next->p;

				V2 lineDir = normalize(lineEnd - lineStart);
				V2 waypointOffset = lineDir * waypointSize;

				lineStart += 2*waypointOffset;
				lineEnd -= waypointOffset + lineDir * arrowSize;

				double rad = getRad(lineDir);
				V2 arrowCenter = lineEnd;
				R2 arrowBounds = rectCenterRadius(arrowCenter, arrowDimens);

				pushDashedLine(group, lineColor, lineStart, lineEnd, lineThickness, lineDashSize, lineSpaceSize, true);
				pushRotatedTexture(group, waypointArrowTex, arrowBounds, rad, lineColor, true);

				w = next;
				if(w == field->curWaypoint) break;
			}
		}
	}
}

void updateConsole(GameState* gameState, double dt, bool paused) {
	bool clickHandled = false;

	FieldSpec* spec = &gameState->fieldSpec;

	{ //NOTE: This draws the gravity field
		ConsoleField* gravityField = gameState->gravityField;
		assert(gravityField);
		if (drawFieldsRaw(gameState->renderGroup, &gameState->input, &gravityField, 1, spec, paused)) clickHandled = true;

		gameState->gravity = v2(0, gravityField->doubleValues[gravityField->selectedIndex]);
	}

	{ //NOTE: This draws the time field
		ConsoleField* timeField = gameState->timeField;
		assert(timeField);
		if (drawFieldsRaw(gameState->renderGroup, &gameState->input, &timeField, 1, spec, paused)) clickHandled = true;
	}

		//NOTE: This draws the swap field
	if (gameState->swapField) {
		moveFieldChildren(&gameState->swapField, 1, spec, dt);

		V2 swapFieldP = gameState->swapFieldP + v2(0, spec->fieldSize.y);

		if (calcFieldPositions(gameState, &gameState->swapField, 1, dt, &swapFieldP, spec, false, paused))
			clickHandled = true;

		if (gameState->swapField) {
			if (drawFieldsRaw(gameState->renderGroup, &gameState->input, &gameState->swapField, 1, spec, paused))
				clickHandled = true;
		}
	}  


	Entity* entity = getEntityByRef(gameState, gameState->consoleEntityRef);

	if (entity) {
		R2 renderBounds = getRenderBounds(entity, gameState);
		
		if(entity->type != EntityType_player) {
			pushOutlinedRect(gameState->renderGroup, renderBounds, 0.02, createColor(255, 0, 0, 255));
		}
		
		V2 fieldP = getBottomFieldP(entity, gameState, spec);

		moveFieldChildren(entity->fields, entity->numFields, spec, dt);
		double totalYOffs = getTotalYOffset(entity->fields, entity->numFields, spec, entity->type == EntityType_pickupField);
		fieldP.y += totalYOffs;



		if (entity->type == EntityType_tile || entity->type == EntityType_heavyTile) {
			//NOTE: This draws the first two fields of the tile, xOffset and yOffset,
			//		differently
			ConsoleField* xOffsetField = entity->fields[0];
			ConsoleField* yOffsetField = entity->fields[1];

			bool showArrows = (getMovementField(entity) == NULL);

			bool yellow = xOffsetField->selectedIndex != entity->tileXOffset ||
						  yOffsetField->selectedIndex != entity->tileYOffset;

			bool drawLeft = !yellow || xOffsetField->selectedIndex < entity->tileXOffset;
			bool drawRight = !yellow || xOffsetField->selectedIndex > entity->tileXOffset;
			bool drawTop = !yellow || yOffsetField->selectedIndex < entity->tileYOffset;
			bool drawBottom = !yellow || yOffsetField->selectedIndex > entity->tileYOffset;

			if (showArrows) {
				V2 clickBoxSize = getRectSize(entity->clickBox);
				V2 clickBoxCenter = getRectCenter(entity->clickBox);

				V2 halfTriangleOffset = (clickBoxSize + spec->triangleSize) * 0.5 + spec->spacing;

				V2 triangleP = entity->p + clickBoxCenter - gameState->cameraP - v2(halfTriangleOffset.x, 0);

				if (drawLeft && drawConsoleTriangle(triangleP, gameState->renderGroup, spec, &gameState->input,  
									true, false, false, yellow, xOffsetField, xOffsetField->tweakCost, paused)) {
					clickHandled = true;
				}

				triangleP += v2(halfTriangleOffset.x * 2, 0);

				if (drawRight && drawConsoleTriangle(triangleP, gameState->renderGroup, spec, &gameState->input, 
									false, false, false, yellow, xOffsetField, xOffsetField->tweakCost, paused)) {
					clickHandled = true;
				}

				triangleP = entity->p + clickBoxCenter - gameState->cameraP - v2(0, halfTriangleOffset.y);

				if (drawTop && drawConsoleTriangle(triangleP, gameState->renderGroup, spec, &gameState->input,  
								false, true, false, yellow, yOffsetField, yOffsetField->tweakCost, paused)) {
					clickHandled = true;
				}

				triangleP += v2(0, halfTriangleOffset.y * 2);

				if (drawBottom && drawConsoleTriangle(triangleP, gameState->renderGroup, spec, &gameState->input,  
								false, false, true, yellow, yOffsetField, yOffsetField->tweakCost, paused)) {
					clickHandled = true;
				}
			}								   

			fieldP.y += getTotalFieldHeight(spec, xOffsetField);
			clickHandled |= drawFields(gameState, entity, 2, dt, &fieldP, spec, paused);
		} 



		else if (entity->type == EntityType_text) {
			ConsoleField* selectedField = entity->fields[0];

			double widthOffs = spec->triangleSize.x * 0.6;
			double heightOffs = getRectHeight(renderBounds) / 2;
			V2 rightTriangleP = renderBounds.max + v2(widthOffs, -heightOffs);

			bool drawRight = selectedField->selectedIndex > 0;
			bool drawLeft = selectedField->selectedIndex < selectedField->numValues - 1;

			if (drawLeft && drawConsoleTriangle(rightTriangleP, gameState->renderGroup, spec, &gameState->input, 
									false, false, false, false, selectedField, selectedField->tweakCost, paused)) {
					clickHandled = true;
			}

			V2 leftTriangleP = renderBounds.min + v2(-widthOffs, heightOffs);

			if (drawRight && drawConsoleTriangle(leftTriangleP, gameState->renderGroup, spec, &gameState->input, 
									true, false, false, false, selectedField, selectedField->tweakCost, paused)) {
					clickHandled = true;
			}

			clickHandled |= drawFields(gameState, entity, 1, dt, &fieldP, spec, paused);
		}


		
		else if (entity->type == EntityType_pickupField) {
			//NOTE: The 0th field of a pickupField is ensured to be all the end of its field list
			//		so that it is rendered at the bottom. It is the actual field that would be 
			//		picked up with this entity
			ConsoleField* data = entity->fields[0];

			for(s32 fieldIndex = 0; fieldIndex < entity->numFields - 1; fieldIndex++)
				entity->fields[fieldIndex] = entity->fields[fieldIndex + 1];

			entity->fields[entity->numFields - 1] = data;

			fieldP.x += spec->fieldSize.x * 0.5 + spec->triangleSize.x + spec->spacing.x * 3;
			clickHandled |= drawFields(gameState, entity, 0, dt, &fieldP, spec, paused);

			for(s32 fieldIndex = entity->numFields - 1; fieldIndex >= 1; fieldIndex--)
				entity->fields[fieldIndex] = entity->fields[fieldIndex - 1];

			entity->fields[0] = data;
		}

		else {
			clickHandled |= drawFields(gameState, entity, 0, dt, &fieldP, spec, paused);
		}


		//NOTE: Draw the waypoints
		for(s32 fieldIndex = 0; fieldIndex < entity->numFields; fieldIndex++) {
			ConsoleField* field = entity->fields[fieldIndex];
			drawWaypointInformation(field, gameState->renderGroup, spec);
		}	
	}

	if(gameState->swapField) drawWaypointInformation(gameState->swapField, gameState->renderGroup, spec);

	if(!paused) {
		bool wasConsoleEntity = getEntityByRef(gameState, gameState->consoleEntityRef) != NULL;

		//NOTE: This deselects the console entity if somewhere else is clicked
		if (!clickHandled && gameState->input.leftMouse.justPressed) {
			gameState->consoleEntityRef = 0;
		}



		//NOTE: This selects a new console entity if there isn't one and a click occurred
		Entity* player = getEntityByRef(gameState, gameState->playerRef);
		ConsoleField* playerMovementField = player ? getMovementField(player) : NULL;

		 //Don't allow hacking while the player is mid-air (doesn't apply when the player is not being keyboard controlled)
		bool playerCanHack = wasConsoleEntity || 
							 (player && isSet(player, EntityFlag_grounded)) || 
							 (!playerMovementField || playerMovementField->type != ConsoleField_keyboardControlled);
		bool noConsoleEntity = getEntityByRef(gameState, gameState->consoleEntityRef) == NULL;
		bool newConsoleEntityRequested = !clickHandled && gameState->input.leftMouse.justPressed;

		//TODO: The spatial partition could be used to make this faster (no need to loop through every entity in the game)
		if(playerCanHack && noConsoleEntity && newConsoleEntityRequested) {
			Entity* newConsoleEntity = NULL;

			for (s32 entityIndex = 0; entityIndex < gameState->numEntities; entityIndex++) {
				Entity* testEntity = gameState->entities + entityIndex;

				bool clicked = isSet(testEntity, EntityFlag_hackable) && isMouseInside(testEntity, &gameState->input);
				bool onTop = newConsoleEntity == NULL || testEntity->drawOrder > newConsoleEntity->drawOrder;
					
				if(clicked && onTop) {
					newConsoleEntity = testEntity;
					clickHandled = true;
				}
			}

			if(newConsoleEntity) {
				gameState->consoleEntityRef = newConsoleEntity->ref;
			}
		}
	}
}
