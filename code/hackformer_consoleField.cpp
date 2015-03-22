ConsoleField createConsoleField(GameState* gameState, char* name, ConsoleFieldType type) {
	ConsoleField result = {};

	result.type = type;
	strcpy_s(result.name, name);

	return result;
}

ConsoleField createUnlimitedIntField(GameState* gameState, char* name, int value, int tweakCost) {
	ConsoleField result = createConsoleField(gameState, name, ConsoleField_unlimitedInt);

	result.selectedIndex = result.initialIndex = value;
	result.tweakCost = tweakCost;

	return result;
}

ConsoleField createBoolField(GameState* gameState, char* name, bool value, int tweakCost) {
	ConsoleField result = createConsoleField(gameState, name, ConsoleField_bool);

	result.initialIndex = result.selectedIndex = value ? 1 : 0;
	result.numValues = 2;
	result.tweakCost = tweakCost;

	return result;
}

ConsoleField createDoubleField(GameState* gameState, char* name, double* values, int numValues, int initialIndex, int tweakCost) {
	assert(numValues > 0);
	assert(initialIndex >= 0 && initialIndex < numValues);

	ConsoleField result = createConsoleField(gameState, name, ConsoleField_double);

	result.values = values;
	result.selectedIndex = result.initialIndex = initialIndex;
	result.numValues = numValues;
	result.tweakCost = tweakCost;

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

			double dstSqToConsoleEntity = dstSq(consoleEntity->p - gameState->cameraP, center);
			double dstSqToSwapField = dstSq(gameState->swapFieldP, center);

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
					consoleEntity->fields[consoleEntity->numFields++] = field;
					assert(consoleEntity->numFields < arrayCount(consoleEntity->fields));
				} else {
					assert(!gameState->swapField);
					gameState->swapField = field;
				}
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

void drawOutlinedConsoleBoxRaw(GameState* gameState, R2 bounds, double stroke) {
	drawFilledRect(gameState, bounds);

	setColor(gameState->renderer, 0, 0, 0, 255);
	drawRect(gameState, bounds, stroke);
}

bool drawOutlinedConsoleBox(ConsoleField* field, GameState* gameState,
							R2 bounds, double stroke, bool nameBox, bool hasValue = true) {
	bool result = false;

	//Texture* text = NULL;

	char* text = NULL;

	if (nameBox) {
		if (hasValue) {
			setColor(gameState->renderer, 255, 100, 100, 255);
		}
		else {
			setColor(gameState->renderer, 100, 100, 255, 255);
			result = moveField(field, gameState, bounds);
		}

		//text = &field->nameTex;
		text = field->name;
	} else {
		setColor(gameState->renderer, 100, 255, 100, 255);
		//text = &field->valueTex;
		text = field->valueStr;
	}

	bounds = translateRect(bounds, field->offs);
	drawOutlinedConsoleBoxRaw(gameState, bounds, stroke);

	double strWidth = getTextWidth(&gameState->consoleFont, gameState, text);
	V2 textP = bounds.min + v2((getRectWidth(bounds) - strWidth) / 2, 
							   (getRectHeight(bounds) - gameState->consoleFont.lineHeight) / 4);

	drawCachedText(&gameState->consoleFont, gameState, text, textP);

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

bool drawConsoleTriangle(GameState* gameState, V2 triangleP, V2 triangleSize, 
						 bool facesRight, bool facesDown, bool facesUp, bool yellow, ConsoleField* field, int tweakCost) {
	bool result = false;

	R2 triangleBounds = rectCenterDiameter(triangleP, triangleSize);

	Texture* triangleTex = NULL;
	double angle = 0;

	if (facesDown) angle = 90;
	if (facesUp) angle = 270;

	bool enoughEnergyToHack = !yellow && gameState->blueEnergy >= tweakCost;

	if (enoughEnergyToHack) {
		bool mouseOverTriangle = dstSq(gameState->input.mouseInMeters, triangleP) < 
								 square(min(triangleSize.x, triangleSize.y) / 2.0f);

		if (mouseOverTriangle) {
			triangleTex = &gameState->consoleTriangleSelected;

			if (gameState->input.leftMouseJustPressed) {
				result = true;

				if (field) {
					if (facesDown) {
						toggleFlags(field, ConsoleFlag_childrenVisible);
					} else {
						int dSelectedIndex = facesRight ? -1 : 1;
						setConsoleFieldSelectedIndex(field, field->selectedIndex + dSelectedIndex, gameState);
					}
				}
			}
		} else {
			triangleTex = &gameState->consoleTriangle;
		}
	} else {
		triangleTex = &gameState->consoleTriangleGrey;
	}

	if (yellow) triangleTex = &gameState->consoleTriangleYellow;

	drawTexture(gameState, triangleTex, triangleBounds, facesRight, angle);

	//NOTE: This draws the cost of tweaking if it is not default
	if (tweakCost > 1) {
		char tweakCostStr[25];
		sprintf_s(tweakCostStr, "%d", tweakCost);
		double costStrWidth = getTextWidth(&gameState->consoleFont, gameState, tweakCostStr);
		V2 costP = triangleBounds.min;

		if (facesDown) {
			costP += v2(triangleSize.x / 2 - costStrWidth / 2, gameState->consoleFont.lineHeight);
		}
		else if (facesUp) {
			costP += v2(triangleSize.x / 2 - costStrWidth / 2, gameState->consoleFont.lineHeight / 4);
		}
		else if (facesRight) {
			costP += v2(triangleSize.x / 2 + costStrWidth / 4, gameState->consoleFont.lineHeight / 2);
		}
		else {
			costP += v2(costStrWidth / 2, gameState->consoleFont.lineHeight / 2);
		}

		drawCachedText(&gameState->consoleFont, gameState, tweakCostStr, costP);
	}

	return result;
}

bool drawFields(GameState* gameState, ConsoleField** fields, int fieldsCount, V2* fieldP,
				V2 fieldSize, V2 triangleSize, V2 valueSize, double spacing, double stroke);

bool drawConsoleField(ConsoleField* field, GameState* gameState, V2* fieldP, 
					  V2 fieldSize, V2 triangleSize, V2 valueSize, double spacing, double stroke) {
	bool result = false;

	bool hasValue = true;

	switch(field->type) {
		case ConsoleField_double: {
			sprintf_s(field->valueStr, "%.1f", ((double*)field->values)[field->selectedIndex]);
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

	R2 fieldBounds = rectCenterDiameter(*fieldP, fieldSize);

	if (drawOutlinedConsoleBox(field, gameState, fieldBounds, stroke, true, hasValue)) {
		result = true;
	}

	if (hasValue) {
		bool alwaysDrawTriangles = field->type == ConsoleField_unlimitedInt;

		V2 triangleP = *fieldP + v2((fieldSize.x + triangleSize.x) / 2 + spacing, 0);
	
		if ((field->selectedIndex != 0 || alwaysDrawTriangles) && 
			drawConsoleTriangle(gameState, triangleP, triangleSize, true, false, false, false, field, field->tweakCost)) {
			result = true;
		}

		V2 valueP = triangleP + v2((triangleSize.x + valueSize.x) / 2.0f + spacing, 0);
		R2 valueBounds = rectCenterDiameter(valueP, valueSize);

		drawOutlinedConsoleBox(field, gameState, valueBounds, stroke, false);

		triangleP = valueP + v2((triangleSize.x + valueSize.x) / 2.0f + spacing, 0);

		if ((field->selectedIndex != field->numValues - 1 || alwaysDrawTriangles) &&
			drawConsoleTriangle(gameState, triangleP, triangleSize, false, false, false, false, field, field->tweakCost)) {
			result = true;
		}
	} else {
		V2 triangleP = *fieldP + v2((triangleSize.x + fieldSize.x) / 2.0f + spacing, 0) + field->offs;

		if (field->children &&
			drawConsoleTriangle(gameState, triangleP, triangleSize, false, true, false, false, field, field->tweakCost)) {
			result = true;
		}
	}

	if (field->children && isSet(field, ConsoleFlag_childrenVisible)) {
		double inset = fieldSize.x / 4.0f;

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
				V2 fieldSize, V2 triangleSize, V2 valueSize, double spacing, double stroke) {
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

	V2 fieldSize = v2(2.1f, 0.4f);
	V2 triangleSize = v2(fieldSize.y, fieldSize.y);
	V2 valueSize = v2(fieldSize.x * 0.5f, fieldSize.y);
	double spacing = 0.05f;
	double stroke = 0.02f;

	if (gameState->consoleEntityRef) {
		Entity* entity = getEntityByRef(gameState, gameState->consoleEntityRef);
		assert(entity);

		R2 renderBounds = translateRect(entity->clickBox, entity->p - gameState->cameraP);
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

		V2 fieldP = getRectCenter(renderBounds) + fieldOffset;

		int numFields = getNumVisibleFields(entity->fields, entity->numFields);
		fieldP.y += (fieldSize.y + spacing) * numFields;

		if (entity->type == EntityType_tile) {
			//NOTE: This draws the first two fields of the tile, xOffset and yOffset,
			//		differently
			ConsoleField* xOffsetField = entity->fields[0];
			ConsoleField* yOffsetField = entity->fields[1];

			bool showArrows = getMovementField(entity) == NULL;

			bool yellow = xOffsetField->selectedIndex != entity->tileXOffset ||
						  yOffsetField->selectedIndex != entity->tileYOffset;

			bool drawLeft = !yellow || xOffsetField->selectedIndex < entity->tileXOffset;
			bool drawRight = !yellow || xOffsetField->selectedIndex > entity->tileXOffset;
			bool drawTop = !yellow || yOffsetField->selectedIndex < entity->tileYOffset;
			bool drawBottom = !yellow || yOffsetField->selectedIndex > entity->tileYOffset;

			if (showArrows) {			   	  
				double halfTriangleOffset = (entity->renderSize.x + triangleSize.x) / 2.0 + spacing;
				V2 triangleP = entity->p - gameState->cameraP - v2(halfTriangleOffset, 0);

				if (drawLeft && drawConsoleTriangle(gameState, triangleP, triangleSize, 
									true, false, false, yellow, NULL, xOffsetField->tweakCost)) {
					setConsoleFieldSelectedIndex(xOffsetField, xOffsetField->selectedIndex - 1, gameState);
					clickHandled = true;
				}

				triangleP += v2(halfTriangleOffset * 2, 0);

				if (drawRight && drawConsoleTriangle(gameState, triangleP, triangleSize, 
									false, false, false, yellow, NULL, xOffsetField->tweakCost)) {
					setConsoleFieldSelectedIndex(xOffsetField, xOffsetField->selectedIndex + 1, gameState);
					clickHandled = true;
				}

				triangleP = entity->p - gameState->cameraP - v2(0, halfTriangleOffset);

				if (drawTop && drawConsoleTriangle(gameState, triangleP, triangleSize, 
								false, true, false, yellow, NULL, yOffsetField->tweakCost)) {
					setConsoleFieldSelectedIndex(yOffsetField, yOffsetField->selectedIndex - 1, gameState);
					clickHandled = true;
				}

				triangleP += v2(0, halfTriangleOffset * 2);

				if (drawBottom && drawConsoleTriangle(gameState, triangleP, triangleSize, 
								false, false, true, yellow, NULL, yOffsetField->tweakCost)) {
					setConsoleFieldSelectedIndex(yOffsetField, yOffsetField->selectedIndex + 1, gameState);
					clickHandled = true;
				}
			}								   

			if (numFields > 2) {
				clickHandled |= drawFields(gameState, entity->fields + 2, entity->numFields - 2, &fieldP,
										   fieldSize, triangleSize, valueSize, spacing, stroke);
			}
		} else {
			clickHandled |= drawFields(gameState, entity->fields, entity->numFields, &fieldP,
				   				   	   fieldSize, triangleSize, valueSize, spacing, stroke);
		}
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
					
				if(isSet(entity, EntityFlag_hackable)) {
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
