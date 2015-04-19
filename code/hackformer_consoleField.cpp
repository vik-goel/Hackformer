void freeConsoleField(ConsoleField* field, GameState* gameState) {
	// if (field->next) {
	// 	freeConsoleField(field->next, gameState);
	// 	field->next = NULL;
	// }

	//NOTE: Since the next pointer is only used in the free list and consoles in the free list should
	//		never be freed again, the next pointer should be null. 
	assert(!field->next);

	for (int childIndex = 0; childIndex < field->numChildren; childIndex++) {
		freeConsoleField(field->children[childIndex], gameState);
		field->children[childIndex] = NULL;
	}

	field->next = gameState->consoleFreeList;
	gameState->consoleFreeList = field;
}

ConsoleField* createConsoleField(GameState* gameState, char* name, ConsoleFieldType type) {
	ConsoleField* result = NULL;

	if (gameState->consoleFreeList) {
		result = gameState->consoleFreeList;
		gameState->consoleFreeList = gameState->consoleFreeList->next;
	} else {
		result = (ConsoleField*)pushStruct(&gameState->levelStorage, ConsoleField);
	}

	*result = {};
	result->type = type;
	strcpy_s(result->name, name);

	return result;
}

ConsoleField* createUnlimitedIntField(GameState* gameState, char* name, int value, int tweakCost) {
	ConsoleField* result = createConsoleField(gameState, name, ConsoleField_unlimitedInt);

	result->selectedIndex = result->initialIndex = value;
	result->tweakCost = tweakCost;

	return result;
}

ConsoleField* createBoolField(GameState* gameState, char* name, bool value, int tweakCost) {
	ConsoleField* result = createConsoleField(gameState, name, ConsoleField_bool);

	result->initialIndex = result->selectedIndex = value ? 1 : 0;
	result->numValues = 2;
	result->tweakCost = tweakCost;

	return result;
}

#define createPrimitiveField(type, gameState, name, values, numValues, initialIndex, tweakCost) createPrimitiveField_(gameState, name, values, sizeof(type), numValues, initialIndex, tweakCost, ConsoleField_##type)
ConsoleField* createPrimitiveField_(GameState* gameState, char* name, void* values, int elemSize,
				int numValues, int initialIndex, int tweakCost, ConsoleFieldType type) {
	assert(numValues > 0);
	assert(initialIndex >= 0 && initialIndex < numValues);

	ConsoleField* result = createConsoleField(gameState, name, type);

	for (int valueIndex = 0; valueIndex < numValues; valueIndex++) {
		char* src = (char*)values + elemSize * valueIndex;
		char* dst = (char*)result->doubleValues + elemSize * valueIndex;

		for(int byteIndex = 0; byteIndex < elemSize; byteIndex++) {
			*dst++ = *src++;
		}
	}

	result->selectedIndex = result->initialIndex = initialIndex;
	result->numValues = numValues;
	result->tweakCost = tweakCost;

	return result;
}

Entity* getEntityByRef(GameState*, int ref);
ConsoleField* getMovementField(Entity* entity);
bool isMouseInside(Entity* entity, Input* input);

int getNumVisibleFields(ConsoleField** fields, int fieldsCount) {
	int result = fieldsCount;

	for (int fieldIndex = 0; fieldIndex < fieldsCount; fieldIndex++) {
		ConsoleField* field = fields[fieldIndex];

		if (field->numChildren && isSet(field, ConsoleFlag_childrenVisible)) {
			result += getNumVisibleFields(field->children, field->numChildren);
		}
	}

	return result;
}

double getTotalYOffset(ConsoleField** fields, int fieldsCount, FieldSpec* spec) {
	double result = fieldsCount * (spec->fieldSize.y + spec->spacing.y);

	for (int fieldIndex = 0; fieldIndex < fieldsCount; fieldIndex++) {
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

bool moveField(ConsoleField* field, GameState* gameState, double dt, FieldSpec* spec) {
	bool result = false;

	R2 bounds = rectCenterDiameter(field->p, spec->fieldSize);

	if (gameState->input.leftMouseJustPressed) {
		if (pointInsideRect(bounds, gameState->input.mouseInMeters)) {
			setFlags(field, ConsoleFlag_selected);
			result = true;
		}
	}

	if (isSet(field, ConsoleFlag_selected)) {
		if (gameState->input.leftMousePressed) {
			field->offs += gameState->input.dMouseMeters;
		} else {
			V2 center = getRectCenter(bounds);

			Entity* consoleEntity = getEntityByRef(gameState, gameState->consoleEntityRef);
			assert(consoleEntity);

			double dstSqToConsoleEntity = dstSq(getBottomFieldP(consoleEntity, gameState, spec), center);
			double dstSqToSwapField = dstSq(gameState->swapFieldP, center);

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
					else if (isConsoleFieldMovementType(gameState->swapField) && 
							 isConsoleFieldMovementType(field)) {
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
			else if (field->type == ConsoleField_isShootTarget) {
				if (removeFromParent) gameState->shootTargetRef = 0;
				else if (addToParent) gameState->shootTargetRef = gameState->consoleEntityRef;
			}

			if(exchangeFields) {
				ConsoleField* swapField = gameState->swapField;

				V2 fieldP = field->p;
				V2 fieldOffs = field->offs;

				gameState->swapField = field;
				rebaseField(gameState->swapField, gameState->swapFieldP);

				bool exchanged = false;

				for(int fieldIndex = 0; fieldIndex < consoleEntity->numFields; fieldIndex++) {
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
				bool shouldAddField = true;

				if (canOnlyHaveOneFieldOfType(field->type)) {
					for (int fieldIndex = 0; fieldIndex < consoleEntity->numFields; fieldIndex++) {
						ConsoleField* testField = consoleEntity->fields[fieldIndex];

						if (testField->type == field->type) {
							shouldAddField = false;
							break;
						}
					}
				}

				if (shouldAddField) {
					V2 newBaseP = getBottomFieldP(consoleEntity, gameState, spec);

					for (int fieldIndex = 0; fieldIndex < consoleEntity->numFields; fieldIndex++) {
						ConsoleField* f = consoleEntity->fields[fieldIndex];
						f->offs = v2(0, -spec->fieldSize.y - field->childYOffs); 
					}

					consoleEntity->fields[consoleEntity->numFields++] = field;
					assert(consoleEntity->numFields < arrayCount(consoleEntity->fields));
					rebaseField(consoleEntity->fields[consoleEntity->numFields - 1], newBaseP);
				} else {
					assert(!gameState->swapField);
					gameState->swapField = field;
					rebaseField(gameState->swapField, gameState->swapFieldP);
				}
			}
			else if (removeFromParent) {
				setFlags(field, ConsoleFlag_remove);

				bool encounteredField = false;

				for (int fieldIndex = 0; fieldIndex < consoleEntity->numFields; fieldIndex++) {
					ConsoleField* f = consoleEntity->fields[fieldIndex];
					if (f == field) encounteredField = true;
					else if (!encounteredField) f->offs = v2(0, spec->fieldSize.y + field->childYOffs); 
				}
			}

			clearFlags(field, ConsoleFlag_selected);
		}
	} else {
		moveToEquilibrium(field, dt);
	}

	return result;
}

bool drawOutlinedConsoleBox(ConsoleField* field, GameState* gameState,
							R2 bounds, FieldSpec* spec, bool nameBox, bool hasValue = true) {
	bool result = false;
	char* text = NULL;

	if (nameBox) {
		Texture* texture = NULL;


		if (hasValue) {
			texture = &gameState->attribute;
			//color = createColor(255, 100, 100, 255);
		}
		else {
			texture = &gameState->behaviour;
			//color = createColor(100, 100, 255, 255);
		}

		assert(texture);

		pushTexture(gameState->renderGroup, texture, bounds, false, DrawOrder_gui);

		//NOTE: This takes into account the dot at the beginning and the feather at the end of the behaviour 
		//	    and attribute sprites when positioning the text
		bounds.min.x += getRectWidth(bounds) * 0.15;

		text = field->name;
	} else {
		Color color = createColor(100, 255, 100, 255); //green

		R2 strokedBounds = scaleRect(bounds, 0.9 * v2(1, 1));
		double stroke = 0.02;

		pushFilledRect(gameState->renderGroup, strokedBounds, color);
		pushOutlinedRect(gameState->renderGroup, strokedBounds, stroke, createColor(0, 0, 0, 255));

		text = field->valueStr;
	}

	double strWidth = getTextWidth(&gameState->consoleFont, gameState, text);
	V2 textP = bounds.min + v2((getRectWidth(bounds) - strWidth) / 2, 
							   (getRectHeight(bounds) - gameState->consoleFont.lineHeight) / 4);

	pushText(gameState->renderGroup, &gameState->consoleFont, text, textP);

	return result;
}

void setConsoleFieldSelectedIndex(ConsoleField* field, int newIndex, GameState* gameState) {
	if (field->type == ConsoleField_unlimitedInt) {
		field->selectedIndex = newIndex;
	} else {
		if (field->type == ConsoleField_bool) {
			field->selectedIndex = 1 - field->selectedIndex;
		} else {
			if (newIndex < 0) newIndex = 0;
			if (newIndex >= field->numValues) newIndex = field->numValues - 1;
			field->selectedIndex = newIndex;
		}

		
		assert(field->selectedIndex >= 0 && field->selectedIndex < field->numValues);
	}

	gameState->blueEnergy -= field->tweakCost;
}

bool drawConsoleTriangle(GameState* gameState, V2 triangleP, FieldSpec* spec, 
						 bool facesRight, bool facesDown, bool facesUp, bool yellow, ConsoleField* field, int tweakCost) {
	bool result = false;

	R2 triangleBounds = rectCenterDiameter(triangleP, spec->triangleSize);

	Texture* triangleTex = &gameState->consoleTriangleGrey;
	//Rotation angle = Degree0;
	//double angle = 0;
	Orientation orientation = Orientation_0;

	if (facesDown) orientation = Orientation_90;//angle = 90;//Degree90;
	if (facesUp) orientation = Orientation_270;//angle = 270;//Degree270;

	bool mouseOverTriangle = dstSq(gameState->input.mouseInMeters, triangleP) < 
							 square(min(spec->triangleSize.x, spec->triangleSize.y) / 2.0f);

	bool enoughEnergyToHack = !yellow && gameState->blueEnergy >= tweakCost;

	if (mouseOverTriangle) {
		if (enoughEnergyToHack) {
			triangleTex = &gameState->consoleTriangleSelected;
		}

		if (gameState->input.leftMouseJustPressed) {
			result = true;

			if (field && enoughEnergyToHack) {
				if (facesDown && tweakCost == 0) {
					toggleFlags(field, ConsoleFlag_childrenVisible);
				} else {
					int dSelectedIndex = facesRight ? -1 : 1;
					setConsoleFieldSelectedIndex(field, field->selectedIndex + dSelectedIndex, gameState);
				}
			}
		}
	} else if (enoughEnergyToHack) {
		triangleTex = &gameState->consoleTriangle;
	}

	if (yellow) triangleTex = &gameState->consoleTriangleYellow;

	pushTexture(gameState->renderGroup, triangleTex, triangleBounds, facesRight, DrawOrder_gui, false, orientation);

	//NOTE: This draws the cost of tweaking if it is not default (0 or 1)
	if (tweakCost > 1) {
		char tweakCostStr[25];
		sprintf_s(tweakCostStr, "%d", tweakCost);
		double costStrWidth = getTextWidth(&gameState->consoleFont, gameState, tweakCostStr);
		V2 costP = triangleBounds.min;

		if (facesDown) {
			costP += v2(spec->triangleSize.x / 2 - costStrWidth / 2, gameState->consoleFont.lineHeight);
		}
		else if (facesUp) {
			costP += v2(spec->triangleSize.x / 2 - costStrWidth / 2, gameState->consoleFont.lineHeight / 4);
		}
		else if (facesRight) {
			costP += v2(spec->triangleSize.x / 2 + costStrWidth / 4, gameState->consoleFont.lineHeight / 2);
		}
		else {
			costP += v2(costStrWidth / 2, gameState->consoleFont.lineHeight / 2);
		}

		pushText(gameState->renderGroup, &gameState->consoleFont, tweakCostStr, costP);
	}

	return result;
}

bool drawFields(GameState* gameState, Entity* entity, double dt, V2* fieldP, FieldSpec* spec);

bool drawConsoleField(ConsoleField* field, GameState* gameState, double dt, FieldSpec* spec) {
	// if (isSet(field, ConsoleFlag_fixOffset)) {
	// 	field->offs = field->tempCenter - *fieldP;
	// 	clearFlags(field, ConsoleFlag_fixOffset);
	// }

	bool result = false;

	if (!isSet(field, ConsoleFlag_remove)) {
		bool hasValue = true;

		switch(field->type) {
			case ConsoleField_int: {
				sprintf_s(field->valueStr, "%d", field->intValues[field->selectedIndex]);
			} break;

			case ConsoleField_double: {
				sprintf_s(field->valueStr, "%.1f", field->doubleValues[field->selectedIndex]);
			} break;

			case ConsoleField_unlimitedInt: {
				sprintf_s(field->valueStr, "%d", field->selectedIndex);
			} break;

			case ConsoleField_bool: {
				if (field->selectedIndex == 0) {
					sprintf_s(field->valueStr, "false");
				} else if (field->selectedIndex == 1) {
					sprintf_s(field->valueStr, "true");
				}
			} break;

			default:
				hasValue = false;
		}

		R2 fieldBounds = rectCenterDiameter(field->p, spec->fieldSize);

		if (drawOutlinedConsoleBox(field, gameState, fieldBounds, spec, true, hasValue)) {
			result = true;
		}

		V2 triangleP = field->p + v2((spec->triangleSize.x + spec->fieldSize.x) / 2.0f + spec->spacing.x, 0);// + field->offs;

		if (hasValue) {
			bool alwaysDrawTriangles = (field->type == ConsoleField_unlimitedInt);

			if ((field->selectedIndex != 0 || alwaysDrawTriangles) && 
				drawConsoleTriangle(gameState, triangleP, spec, 
					true, false, false, false, field, field->tweakCost)) {
				result = true;
			}

			V2 valueOffset = v2((spec->triangleSize.x + spec->valueSize.x) / 2.0f + spec->spacing.x, 0);
			V2 valueP = triangleP + valueOffset;
			R2 valueBounds = rectCenterDiameter(valueP, spec->valueSize);

			drawOutlinedConsoleBox(field, gameState, valueBounds, spec, false);

			triangleP = valueP + valueOffset;

			if ((field->selectedIndex != field->numValues - 1 || alwaysDrawTriangles) &&
				drawConsoleTriangle(gameState, triangleP, spec,
					 false, false, false, false, field, field->tweakCost)) {
				result = true;
			}
		} else {
			if (field->numChildren &&
				drawConsoleTriangle(gameState, triangleP, spec, false, true, false, false, field, field->tweakCost)) {
				result = true;
			}
		}
	}

	return result;
}

void moveFieldChildren(ConsoleField** fields, int fieldsCount, FieldSpec* spec, double dt) {
	for (int fieldIndex = 0; fieldIndex < fieldsCount; fieldIndex++) {
		ConsoleField* field = fields[fieldIndex];

		if (field->numChildren) {
			double fieldSpeed = 4;
			double delta = fieldSpeed * dt;
			double maxY = field->numChildren * (spec->fieldSize.y + spec->spacing.y);
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

bool calcFieldPositions(GameState* gameState, ConsoleField** fields, int fieldsCount, double dt, V2* fieldP, FieldSpec* spec) {
	bool result = false;
	double fieldHeight = spec->fieldSize.y + spec->spacing.y;

	for (int fieldIndex = 0; fieldIndex < fieldsCount; fieldIndex++) {
		fieldP->y -= fieldHeight;
		ConsoleField* field = fields[fieldIndex];
		field->p = *fieldP + field->offs;

		if (moveField(field, gameState, dt, spec)) result = true;

		field->p = *fieldP + field->offs;

		if (!isSet(field, ConsoleFlag_remove)) {
			if (field->numChildren && field->childYOffs) {

				V2 startFieldP = *fieldP;
				fieldP->x += spec->childInset;
				*fieldP += field->offs;

				calcFieldPositions(gameState, field->children, field->numChildren, dt, fieldP, spec);

				*fieldP = startFieldP - v2(0, field->childYOffs);
			}
		} else {
			fieldP->y -= field->childYOffs;
		}
	}

	return result;
}

bool drawFieldsRaw(GameState* gameState, ConsoleField** fields, int fieldsCount, double dt, FieldSpec* spec) {
	bool result = false;

	for (int fieldIndex = 0; fieldIndex < fieldsCount; fieldIndex++) {
		ConsoleField* field = fields[fieldIndex];
		if (drawConsoleField(field, gameState, dt, spec)) result = true;

		if (field->numChildren && field->childYOffs) {
			//NOTE: 0.1 and 2.5 are just arbitrary values which make the clipRect big enough
			V2 clipPoint1 = field->p - v2(spec->fieldSize.x / 2 + 0.1 - spec->childInset, field->childYOffs + spec->fieldSize.y / 2 + spec->spacing.y);
			V2 clipPoint2 = field->p + v2(spec->fieldSize.x / 2 + 2.5 + spec->childInset, -spec->fieldSize.y / 2);

			R2 clipRect = r2(clipPoint1, clipPoint2);
			pushClipRect(gameState->renderGroup, clipRect);

			//pushFilledRect(gameState->renderGroup, clipRect, createColor(255, 0, 255, 255), false);

			if (drawFieldsRaw(gameState, field->children, field->numChildren, dt, spec)) result = true;

			pushDefaultClipRect(gameState->renderGroup);
		}
	}

	return result;
}

bool drawFields(GameState* gameState, Entity* entity, int fieldSkip, double dt, V2* fieldP, FieldSpec* spec) {
	bool result = false;

	double fieldHeight = spec->spacing.y + spec->fieldSize.y;
	fieldP->y -= fieldHeight * fieldSkip;

	if(calcFieldPositions(gameState, entity->fields + fieldSkip, entity->numFields - fieldSkip, dt, fieldP, spec)) {
		result = true;
	}

	if(drawFieldsRaw(gameState, entity->fields + fieldSkip, entity->numFields - fieldSkip, dt, spec)) {
		result = true;
	}

	return result;
}

void updateConsole(GameState* gameState, double dt) {
	bool clickHandled = false;

	FieldSpec fieldSpec;
	FieldSpec* spec = &fieldSpec;

	fieldSpec.fieldSize = v2(2.1, 0.4);
	fieldSpec.triangleSize = v2(fieldSpec.fieldSize.y, fieldSpec.fieldSize.y);
	fieldSpec.valueSize = v2(fieldSpec.fieldSize.x * 0.5, fieldSpec.fieldSize.y);
	fieldSpec.spacing = v2(0.05, 0);
	fieldSpec.childInset = fieldSpec.fieldSize.x * 0.25;

	Entity* entity = getEntityByRef(gameState, gameState->consoleEntityRef);

	if (entity) {
		R2 renderBounds = getRenderBounds(entity, gameState);
		
		if(entity->type != EntityType_player) {
			pushOutlinedRect(gameState->renderGroup, renderBounds, 0.02, createColor(255, 0, 0, 255));
		}
		
		V2 fieldP = getBottomFieldP(entity, gameState, spec);

		moveFieldChildren(entity->fields, entity->numFields, spec, dt);
		double totalYOffs = getTotalYOffset(entity->fields, entity->numFields, spec);
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

				if (drawLeft && drawConsoleTriangle(gameState, triangleP, spec, 
									true, false, false, yellow, xOffsetField, xOffsetField->tweakCost)) {
					clickHandled = true;
				}

				triangleP += v2(halfTriangleOffset.x * 2, 0);

				if (drawRight && drawConsoleTriangle(gameState, triangleP, spec, 
									false, false, false, yellow, xOffsetField, xOffsetField->tweakCost)) {
					clickHandled = true;
				}

				triangleP = entity->p + clickBoxCenter - gameState->cameraP - v2(0, halfTriangleOffset.y);

				if (drawTop && drawConsoleTriangle(gameState, triangleP, spec, 
								true, false, true, yellow, yOffsetField, yOffsetField->tweakCost)) {
					clickHandled = true;
				}

				triangleP += v2(0, halfTriangleOffset.y * 2);

				if (drawBottom && drawConsoleTriangle(gameState, triangleP, spec, 
								false, false, true, yellow, yOffsetField, yOffsetField->tweakCost)) {
					clickHandled = true;
				}
			}								   

			clickHandled |= drawFields(gameState, entity, 2, dt, &fieldP, spec);
		} 



		else if (entity->type == EntityType_text) {
			ConsoleField* selectedField = entity->fields[0];

			double widthOffs = spec->triangleSize.x * 0.6;
			double heightOffs = getRectHeight(renderBounds) / 2;
			V2 rightTriangleP = renderBounds.max + v2(widthOffs, -heightOffs);

			bool drawRight = selectedField->selectedIndex > 0;
			bool drawLeft = selectedField->selectedIndex < selectedField->numValues - 1;

			if (drawLeft && drawConsoleTriangle(gameState, rightTriangleP, spec, 
									false, false, false, false, selectedField, selectedField->tweakCost)) {
					clickHandled = true;
			}

			V2 leftTriangleP = renderBounds.min + v2(-widthOffs, heightOffs);

			if (drawRight && drawConsoleTriangle(gameState, leftTriangleP, spec, 
									true, false, false, false, selectedField, selectedField->tweakCost)) {
					clickHandled = true;
			}

			clickHandled |= drawFields(gameState, entity, 1, dt, &fieldP, spec);
		}



		else {
			clickHandled |= drawFields(gameState, entity, 0, dt, &fieldP, spec);
		}
	}




	//NOTE: This draws the swap field
	if (gameState->consoleEntityRef) {
		// if (!gameState->swapField || lengthSq(gameState->swapField->offs) > 0) {
		// 	V2 swapFieldP = gameState->swapFieldP;

		// 	drawOutlinedConsoleBoxRaw(gameState, rectCenterDiameter(gameState->swapFieldP, spec->fieldSize),
		// 							 spec, createColor(255, 255, 100, 255));
		// }

		if (gameState->swapField) {
			moveFieldChildren(&gameState->swapField, 1, spec, dt);

			V2 swapFieldP = gameState->swapFieldP + v2(0, spec->fieldSize.y);

			if (calcFieldPositions(gameState, &gameState->swapField, 1, dt, &swapFieldP, spec))
				clickHandled = true;

			if (gameState->swapField) {
				if (drawFieldsRaw(gameState, &gameState->swapField, 1, dt, spec))
					clickHandled = true;
			}
		}  
	}

	bool wasConsoleEntity = getEntityByRef(gameState, gameState->consoleEntityRef) != NULL;

	//NOTE: This deselects the console entity if somewhere else is clicked
	if (!clickHandled && gameState->input.leftMouseJustPressed) {
		gameState->consoleEntityRef = 0;
	}



	//NOTE: This selects a new console entity if there isn't one and a click occurred
	Entity* player = getEntityByRef(gameState, gameState->playerRef);
	bool playerCanHack = wasConsoleEntity || (player && isSet(player, EntityFlag_grounded)); //Don't allow hacking while the player is mid-air
	bool noConsoleEntity = getEntityByRef(gameState, gameState->consoleEntityRef) == NULL;
	bool newConsoleEntityRequested = !clickHandled && gameState->input.leftMouseJustPressed;

	//TODO: The spatial partition could be used to make this faster (no need to loop through every entity in the game)
	if(playerCanHack && noConsoleEntity && newConsoleEntityRequested) {
		for (int entityIndex = 0; entityIndex < gameState->numEntities; entityIndex++) {
			Entity* entity = gameState->entities + entityIndex;
				
			if(isSet(entity, EntityFlag_hackable) && isMouseInside(entity, &gameState->input)) {
				gameState->consoleEntityRef = entity->ref;
				clickHandled = true;
				break;
			}
		}
	}
}
