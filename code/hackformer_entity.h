#ifndef HACKFORMER_ENTITY_H
#define HACKFORMER_ENTITY_H

enum ConsoleFieldType {
	ConsoleField_float,
	ConsoleField_string,
	ConsoleField_int
};

struct ConsoleField {
	ConsoleFieldType type;
	char* name;
	void* values;
	int numValues;
	int selectedIndex;
	int initialIndex;
};

enum EntityType {
	EntityType_null,
	EntityType_player,
	EntityType_tile,
	EntityType_background,
	EntityType_blueEnergy,
};

enum DrawOrder {
	DrawOrder_player = 10000,
	DrawOrder_blueEnergy = 10,
	DrawOrder_tile = 0,
	DrawOrder_background = -10000,
};

enum EntityFlag {
	EntityFlag_moveable = 1 << 0,
	EntityFlag_collidable = 1 << 1,
	EntityFlag_solid = 1 << 2,
	EntityFlag_facesLeft = 1 << 3,
	EntityFlag_consoleSelected = 1 << 4,
	EntityFlag_remove = 1 << 5,
};

struct Entity {
	EntityType type;
	uint flags;
	int entityIndex;

	V2 p;
	V2 dP;

	V2 renderSize;
	Texture* texture;
	float animTime;
	DrawOrder drawOrder;

	V2 collisionPoints[32];
	int numCollisionPoints;

	ConsoleField fields[4];
	int numFields;
};

#endif