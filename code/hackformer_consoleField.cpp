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

bool isConsoleFieldMovementType(ConsoleField* field) {
	bool result = field->type == ConsoleField_keyboardControlled ||
				  field->type == ConsoleField_movesBackAndForth;
	return result;
}

bool drawOutlinedConsoleBox(ConsoleField* field, GameState* gameState, Entity* parent,
							R2 bounds, bool nameBox, bool hasValue = true) {
	bool result = false;

	Texture* text;

	if (nameBox) {
		if (hasValue) {
			setColor(gameState->renderer, 255, 100, 100, 255);
		}
		else {
			setColor(gameState->renderer, 100, 100, 255, 255);

			if (gameState->input.leftMouseJustPressed) {
				if (pointInsideRect(bounds, gameState->input.mouseInMeters)) {
					field->selected = true;
					result = true;
				}
			}

			if (field->selected) {
				if (gameState->input.leftMousePressed) {
					field->offs += gameState->input.dMouseMeters;
				} else {
					Entity* newParent = NULL;
					float minDstSqToParent = 100000000.0f;

					V2 boundsCenter = getRectCenter(bounds) + gameState->cameraP + field->offs;

					for (int entityIndex = 0; entityIndex < gameState->numEntities; entityIndex++) {
						Entity* entity = gameState->entities + entityIndex;

						if (isSet(entity, EntityFlag_consoleSelected)) {
							float dstSqToParent =  dstSq(boundsCenter, entity->p);

							if (dstSqToParent < minDstSqToParent) {
								minDstSqToParent = dstSqToParent;
								newParent = entity;
							}
						}
					}

					if (newParent && newParent != parent) {
						if (isConsoleFieldMovementType(field)) {
							int newParentFieldIndex = -1;
							int parentFieldIndex = -1;

							for (int fieldIndex = 0; fieldIndex < newParent->numFields; fieldIndex++) {
								ConsoleField* testNewParentField = newParent->fields[fieldIndex];

								if (isConsoleFieldMovementType(testNewParentField)) {
									newParentFieldIndex = fieldIndex;
									break;
								}
							}

							for (int fieldIndex = 0; fieldIndex < parent->numFields; fieldIndex++) {
								ConsoleField* testParentField = parent->fields[fieldIndex];

								if (isConsoleFieldMovementType(testParentField)) {
									parentFieldIndex = fieldIndex;
									break;
								}
							}

							assert(parentFieldIndex >= 0);

							if (newParentFieldIndex >= 0) {
								parent->fields[parentFieldIndex] = newParent->fields[newParentFieldIndex];
								newParent->fields[newParentFieldIndex] = field;
							} else {
								removeField(parent, field);
								addField(newParent, field);
							}

							EntityMovement tempMovement = parent->movementType;
							parent->movementType = newParent->movementType;
							newParent->movementType = tempMovement;
						} else {
							addField(newParent, field);
							removeField(parent, field);

							if (field->type == ConsoleField_isShootTarget) {
								gameState->shootTargetRef = newParent->ref;
							}
						}
					}

					field->selected = false;

					//TODO: Smooth movement
					field->offs = v2(0, 0);
				}
			}
		}

		text = &field->nameTex;
	} else {
		setColor(gameState->renderer, 100, 255, 100, 255);
		
		text = &field->valueTex;
	}

	bounds = translateRect(bounds, field->offs);

	drawFilledRect(gameState, bounds);

	float thickness = 0.02f;

	setColor(gameState->renderer, 0, 0, 0, 255);
	drawRect(gameState, bounds, thickness);

	V2 center = getRectCenter(bounds);
	float width = getRectWidth(bounds);


	drawText(gameState, text, gameState->consoleFont,
		 	 center.x, center.y, width - thickness * 2);

	return result;
}

void setConsoleFieldSelectedIndex(ConsoleField* field, int newIndex) {
	field->validValueTex = false;

	if (field->type == ConsoleField_unlimitedInt) {
		field->selectedIndex = newIndex;
	} else {
		if (newIndex < 0) newIndex = 0;
		if (newIndex >= field->numValues) newIndex = field->numValues - 1;

		field->selectedIndex = newIndex;
		assert(field->selectedIndex >= 0 && field->selectedIndex < field->numValues);
	}
}

bool drawConsoleTriangle(GameState* gameState, V2 triangleP, V2 triangleSize, bool facesRight, ConsoleField* field) {
	bool result = false;

	R2 triangleBounds = rectCenterDiameter(triangleP, triangleSize);

	Texture* triangleTex;

	if (dstSq(gameState->input.mouseInMeters, triangleP) < square(min(triangleSize.x, triangleSize.y) / 2.0f)) {
		triangleTex = &gameState->consoleTriangleSelected;

		if (gameState->input.leftMouseJustPressed) {
			result = true;
			int dSelectedIndex = facesRight ? -1 : 1;
			setConsoleFieldSelectedIndex(field, field->selectedIndex + dSelectedIndex);
		}
	} else {
		triangleTex = &gameState->consoleTriangle;
	}

	drawTexture(gameState, triangleTex, triangleBounds, facesRight);

	return result;
}

void updateConsole(GameState* gameState) {
	bool clickHandled = false;

	for (int entityIndex = 0; entityIndex < gameState->numEntities; entityIndex++) {
		Entity* entity = gameState->entities + entityIndex; 

		if (isSet(entity, EntityFlag_consoleSelected)) {
			R2 renderBounds = rectCenterDiameter(entity->p - gameState->cameraP, entity->renderSize);
			setColor(gameState->renderer, 255, 0, 0, 255);
			drawRect(gameState, renderBounds, 0.02f);

			V2 fieldSize = v2(2.0f, 0.4f);
			V2 triangleSize = v2(fieldSize.y, fieldSize.y);
			V2 valueSize = v2(fieldSize.x * 0.5f, fieldSize.y);

			V2 fieldOffset;

			V2 entityScreenP = (entity->p - gameState->cameraP) * gameState->pixelsPerMeter;
			bool onScreenRight = entityScreenP.x > gameState->windowWidth / 2;

			if (onScreenRight) {
				fieldOffset = v2(fieldSize.x * -0.75f, 0);
			} else {
				fieldOffset = v2(fieldSize.x * 0.75f, 0);
			}

			V2 fieldP = entity->p - gameState->cameraP;
			float spacing = 0.05f;

			for (int fieldIndex = 0; fieldIndex < entity->numFields; fieldIndex++) {
				ConsoleField* field = entity->fields[fieldIndex];

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
						} else {
							sprintf_s(value, "true");
						}
					} break;

					default:
						hasValue = false;
				}


				fieldP.y += fieldSize.y + spacing;
				R2 fieldBounds = rectCenterDiameter(fieldP, fieldSize);

				if (drawOutlinedConsoleBox(field, gameState, entity, fieldBounds, true, hasValue)) {
					clickHandled = true;
				}

				if (hasValue) {
					bool alwaysDrawTriangles = field->type == ConsoleField_unlimitedInt ||
											   field->type == ConsoleField_bool;

					V2 triangleP = fieldP + v2((fieldSize.x + triangleSize.x) / 2 + spacing, 0);
				
					if ((field->selectedIndex != 0 || alwaysDrawTriangles) && 
						drawConsoleTriangle(gameState, triangleP, triangleSize, true, field)) {
						clickHandled = true;
					}

					if (!field->validValueTex) {
						field->validValueTex = true;
						if (field->valueTex.tex) SDL_DestroyTexture(field->valueTex.tex);
						field->valueTex = createText(gameState, gameState->consoleFont, value);
					}

					V2 valueP = triangleP + v2((triangleSize.x + valueSize.x) / 2.0f + spacing, 0);
					R2 valueBounds = rectCenterDiameter(valueP, valueSize);

					drawOutlinedConsoleBox(field, gameState, entity, valueBounds, false);

					triangleP = valueP + v2((triangleSize.x + valueSize.x) / 2.0f + spacing, 0);

					if ((field->selectedIndex != field->numValues - 1 || alwaysDrawTriangles) &&
						drawConsoleTriangle(gameState, triangleP, triangleSize, false, field)) {
						clickHandled = true;
					}
				}
			}
		}
	}

	if (!clickHandled && gameState->input.leftMouseJustPressed) {
		for (int entityIndex = 0; entityIndex < gameState->numEntities; entityIndex++) {
			Entity* entity = gameState->entities + entityIndex;
				
			if(entity->numFields) {
				if (isMouseInside(entity, &gameState->input)) {
					setFlags(entity, EntityFlag_consoleSelected);
					gameState->numConsoleEntities++;
					clickHandled = true;
					break;
				}
			}
		}
	}

	if (!clickHandled && gameState->input.leftMouseJustPressed) {
		for (int entityIndex = 0; entityIndex < gameState->numEntities; entityIndex++) {
			Entity* entity = gameState->entities + entityIndex;

			if(entity->numFields) {
				if (isSet(entity, EntityFlag_consoleSelected)) {
					gameState->numConsoleEntities--;
					clearFlags(entity, EntityFlag_consoleSelected);
				}
			}
		}
	}	
}
