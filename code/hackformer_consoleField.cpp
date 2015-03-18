ConsoleField createConsoleField(GameState* gameState, char* name, ConsoleFieldType type) {
	ConsoleField result = {};

	result.type = type;
	result.name = name;
	result.nameTex = createText(gameState, gameState->consoleFont, name);

	return result;
}

ConsoleField createUnlimitedIntField(GameState* gameState, char* name, int value) {
	ConsoleField result = createConsoleField(gameState, name, ConsoleField_unlimitedInt);

	result.selectedIndex = result.initialIndex = value;

	return result;
}

ConsoleField createBoolField(GameState* gameState, char* name, bool value) {
	ConsoleField result = createConsoleField(gameState, name, ConsoleField_bool);

	result.initialIndex = result.selectedIndex = value ? 1 : 0;
	result.numValues = 2;

	return result;
}

ConsoleField createFloatField(GameState* gameState, char* name, float* values, int numValues, int initialIndex) {
	assert(numValues > 0);
	assert(initialIndex >= 0 && initialIndex < numValues);

	ConsoleField result = createConsoleField(gameState, name, ConsoleField_float);

	result.values = values;
	result.selectedIndex = result.initialIndex = initialIndex;
	result.numValues = numValues;

	return result;
}

bool moveField(ConsoleField* field, GameState* gameState, R2 bounds) {
	bool result = false;

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
			V2 center = getRectCenter(bounds) + field->offs;

			//TODO: Directly store a reference to the console entity so this is just a lookup
			Entity* consoleEntity = getEntityByRef(gameState, gameState->consoleEntityRef);
			assert(consoleEntity);

			float dstSqToConsoleEntity = dstSq(consoleEntity->p - gameState->cameraP, center);
			float dstSqToSwapField = dstSq(gameState->swapFieldP, center);

			bool addToParent = false;
			bool removeFromParent = false;

			if (gameState->swapField == field) {
				if (dstSqToConsoleEntity < dstSqToSwapField) {
					gameState->swapField = NULL;
					addToParent = true;
				}
			} else {
				if (dstSqToSwapField < dstSqToConsoleEntity) {
					if (gameState->swapField == NULL) {
						gameState->swapField = field;
						removeFromParent = true;
					}
					else if (isConsoleFieldMovementType(gameState->swapField) && 
							 isConsoleFieldMovementType(field)) {
						ConsoleField tempField = *gameState->swapField;
						*gameState->swapField = *field;
						*field = tempField;
					}
				}
			}

			assert(!(addToParent && removeFromParent));

			if (isConsoleFieldMovementType(field)) {
				if (addToParent) {
					ConsoleField* movementField = getMovementField(consoleEntity);

					if (movementField) {
						gameState->swapField = movementField;
						setFlags(movementField, ConsoleFlag_remove);
					}
				}
			}
			else if (field->type == ConsoleField_isShootTarget) {
					if (removeFromParent) gameState->shootTargetRef = 0;
					else if (addToParent) gameState->shootTargetRef = gameState->consoleEntityRef;
			}

			if (addToParent) {
				consoleEntity->fields[consoleEntity->numFields++] = field;
				assert(consoleEntity->numFields < arrayCount(consoleEntity->fields));
			}
			else if (removeFromParent) {
				setFlags(field, ConsoleFlag_remove);
			}

			clearFlags(field, ConsoleFlag_selected);

			//TODO: Smooth movement
			field->offs = v2(0, 0);
		}
	}

	return result;
}

void drawOutlinedConsoleBoxRaw(GameState* gameState, R2 bounds, float stroke) {
	drawFilledRect(gameState, bounds);

	setColor(gameState->renderer, 0, 0, 0, 255);
	drawRect(gameState, bounds, stroke);
}

bool drawOutlinedConsoleBox(ConsoleField* field, GameState* gameState,
							R2 bounds, float stroke, bool nameBox, bool hasValue = true) {
	bool result = false;

	Texture* text = NULL;

	if (nameBox) {
		if (hasValue) {
			setColor(gameState->renderer, 255, 100, 100, 255);
		}
		else {
			setColor(gameState->renderer, 100, 100, 255, 255);
			result = moveField(field, gameState, bounds);
		}

		text = &field->nameTex;
	} else {
		setColor(gameState->renderer, 100, 255, 100, 255);
		text = &field->valueTex;
	}

	bounds = translateRect(bounds, field->offs);
	drawOutlinedConsoleBoxRaw(gameState, bounds, stroke);

	V2 center = getRectCenter(bounds);
	float width = getRectWidth(bounds);

	drawText(gameState, text, gameState->consoleFont,
		 	 center.x, center.y, width - stroke * 2);

	return result;
}

void setConsoleFieldSelectedIndex(ConsoleField* field, int newIndex) {
	clearFlags(field, ConsoleFlag_validValueTex);

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
}

bool drawConsoleTriangle(GameState* gameState, V2 triangleP, V2 triangleSize, 
						 bool facesRight, bool facesDown, ConsoleField* field) {
	bool result = false;

	R2 triangleBounds = rectCenterDiameter(triangleP, triangleSize);

	Texture* triangleTex;

	if (dstSq(gameState->input.mouseInMeters, triangleP) < square(min(triangleSize.x, triangleSize.y) / 2.0f)) {
		if (facesDown) triangleTex = &gameState->consoleTriangleDownSelected;
		else triangleTex = &gameState->consoleTriangleSelected;

		if (gameState->input.leftMouseJustPressed) {
			result = true;

			if (facesDown) {
				toggleFlags(field, ConsoleFlag_childrenVisible);
			} else {
				int dSelectedIndex = facesRight ? -1 : 1;
				setConsoleFieldSelectedIndex(field, field->selectedIndex + dSelectedIndex);
			}
		}
	} else {
		if (facesDown) triangleTex = &gameState->consoleTriangleDown;
		else triangleTex = &gameState->consoleTriangle;
	}

	drawTexture(gameState, triangleTex, triangleBounds, facesRight);

	return result;
}

bool drawFields(GameState* gameState, ConsoleField** fields, int fieldsCount, V2* fieldP,
				V2 fieldSize, V2 triangleSize, V2 valueSize, float spacing, float stroke);

bool drawConsoleField(ConsoleField* field, GameState* gameState, V2* fieldP, 
					  V2 fieldSize, V2 triangleSize, V2 valueSize, float spacing, float stroke) {
	bool result = false;

	char value[120];
	bool hasValue = true;

	switch(field->type) {
		case ConsoleField_float: {
			sprintf_s(value, "%.1f", ((float*)field->values)[field->selectedIndex]);
		} break;

		case ConsoleField_unlimitedInt: {
			sprintf_s(value, "%d", field->selectedIndex);
		} break;

		case ConsoleField_bool: {
			if (field->selectedIndex == 0) {
				sprintf_s(value, "false");
			} else if (field->selectedIndex == 1) {
				sprintf_s(value, "true");
			}
		} break;

		default:
			hasValue = false;
	}

	R2 fieldBounds = rectCenterDiameter(*fieldP, fieldSize);

	if (drawOutlinedConsoleBox(field, gameState, fieldBounds, stroke, true, hasValue)) {
		result = true;
	}

	if (hasValue) {
		if (!isSet(field, ConsoleFlag_validValueTex)) {
			setFlags(field, ConsoleFlag_validValueTex);
			if (field->valueTex.tex) SDL_DestroyTexture(field->valueTex.tex);
			field->valueTex = createText(gameState, gameState->consoleFont, value);
		}

		bool alwaysDrawTriangles = field->type == ConsoleField_unlimitedInt;

		V2 triangleP = *fieldP + v2((fieldSize.x + triangleSize.x) / 2 + spacing, 0);
	
		if ((field->selectedIndex != 0 || alwaysDrawTriangles) && 
			drawConsoleTriangle(gameState, triangleP, triangleSize, true, false, field)) {
			result = true;
		}

		V2 valueP = triangleP + v2((triangleSize.x + valueSize.x) / 2.0f + spacing, 0);
		R2 valueBounds = rectCenterDiameter(valueP, valueSize);

		drawOutlinedConsoleBox(field, gameState, valueBounds, stroke, false);

		triangleP = valueP + v2((triangleSize.x + valueSize.x) / 2.0f + spacing, 0);

		if ((field->selectedIndex != field->numValues - 1 || alwaysDrawTriangles) &&
			drawConsoleTriangle(gameState, triangleP, triangleSize, false, false, field)) {
			result = true;
		}
	} else {
		V2 triangleP = *fieldP + v2((triangleSize.x + fieldSize.x) / 2.0f + spacing, 0) + field->offs;

		if (field->children &&
			drawConsoleTriangle(gameState, triangleP, triangleSize, false, true, field)) {
			result = true;
		}
	}

	if (field->children && isSet(field, ConsoleFlag_childrenVisible)) {
		float inset = fieldSize.x / 4.0f;

		fieldP->x += inset;
		*fieldP += field->offs;

		result |= drawFields(gameState, field->children, field->numChildren, fieldP,
				  			 fieldSize, triangleSize, valueSize, spacing, stroke);

		*fieldP -= field->offs;
		fieldP->x -= inset;
	}

	return result;
}

int getNumVisibleFields(ConsoleField** fields, int fieldsCount) {
	int result = fieldsCount;

	for (int fieldIndex = 0; fieldIndex < fieldsCount; fieldIndex++) {
		ConsoleField* field = fields[fieldIndex];

		if (field->children && isSet(field, ConsoleFlag_childrenVisible)) {
			result += getNumVisibleFields(field->children, field->numChildren);
		}
	}

	return result;
}

bool drawFields(GameState* gameState, ConsoleField** fields, int fieldsCount, V2* fieldP,
				V2 fieldSize, V2 triangleSize, V2 valueSize, float spacing, float stroke) {
	bool result = false;

	for (int fieldIndex = 0; fieldIndex < fieldsCount; fieldIndex++) {
		fieldP->y -= fieldSize.y + spacing;
		ConsoleField* field = fields[fieldIndex];
		result |= drawConsoleField(field, gameState, fieldP, 
										 fieldSize, triangleSize, valueSize, spacing, stroke);				
	}

	return result;
}

void updateConsole(GameState* gameState) {
	bool clickHandled = false;

	V2 fieldSize = v2(2.0f, 0.4f);
	V2 triangleSize = v2(fieldSize.y, fieldSize.y);
	V2 valueSize = v2(fieldSize.x * 0.5f, fieldSize.y);
	float spacing = 0.05f;
	float stroke = 0.02f;

	if (gameState->consoleEntityRef) {
		Entity* entity = getEntityByRef(gameState, gameState->consoleEntityRef);
		assert(entity);

		R2 renderBounds = rectCenterDiameter(entity->p - gameState->cameraP, entity->renderSize);
		setColor(gameState->renderer, 255, 0, 0, 255);
		drawRect(gameState, renderBounds, 0.02f);

		V2 fieldOffset;

		V2 entityScreenP = (entity->p - gameState->cameraP) * gameState->pixelsPerMeter;
		bool onScreenRight = entityScreenP.x > gameState->windowWidth / 2;

		if (onScreenRight) {
			fieldOffset = v2(fieldSize.x * -0.75f, 0);
		} else {
			fieldOffset = v2(fieldSize.x * 0.75f, 0);
		}

		V2 fieldP = entity->p - gameState->cameraP + fieldOffset;

		int numFields = getNumVisibleFields(entity->fields, entity->numFields);
		fieldP.y += (fieldSize.y + spacing) * numFields;

		clickHandled |= drawFields(gameState, entity->fields, entity->numFields, &fieldP,
				   				   fieldSize, triangleSize, valueSize, spacing, stroke);
	}


	//NOTE: This draws the swap field
	if (gameState->consoleEntityRef) {
		if (gameState->swapField) {
			clickHandled |= drawConsoleField(gameState->swapField, gameState, &gameState->swapFieldP, 
										 	 fieldSize, triangleSize, valueSize, spacing, stroke);	
		} else {
			setColor(gameState->renderer, 255, 255, 100, 255);
			drawOutlinedConsoleBoxRaw(gameState, rectCenterDiameter(gameState->swapFieldP, fieldSize), stroke);
		}
	}

	

	//NOTE: This deselects the console entity if somewhere else is clicked
	if (!clickHandled && gameState->input.leftMouseJustPressed) {
		gameState->consoleEntityRef = 0;
	}


	//NOTE: This selects a new console entity if there isn't one and a click occurred
	if (!gameState->consoleEntityRef) {
		if (!clickHandled && gameState->input.leftMouseJustPressed) {
			for (int entityIndex = 0; entityIndex < gameState->numEntities; entityIndex++) {
				Entity* entity = gameState->entities + entityIndex;
					
				if(entity->numFields) {
					if (isMouseInside(entity, &gameState->input)) {
						gameState->consoleEntityRef = entity->ref;
						clickHandled = true;
						break;
					}
				}
			}
		}
	}
}
