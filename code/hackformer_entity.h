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
	EntityType_text,
	EntityType_virus,
	EntityType_laserBolt,
};

enum DrawOrder {
	DrawOrder_background = -10000,
	DrawOrder_text = -50,
	DrawOrder_tile = 0,
	DrawOrder_blueEnergy = 10,
	DrawOrder_virus = 100,
	DrawOrder_laserBolt = 250,
	DrawOrder_player = 10000,
};

enum EntityFlag {
	EntityFlag_moveable = 1 << 0,
	EntityFlag_collidable = 1 << 1,
	EntityFlag_solid = 1 << 2,
	EntityFlag_facesLeft = 1 << 3,
	EntityFlag_consoleSelected = 1 << 4,
	EntityFlag_remove = 1 << 5,
	EntityFlag_ignoresGravity = 1 << 6,
	EntityFlag_shooting = 1 << 7,
	EntityFlag_unchargingAfterShooting = 1 << 8,
};

struct Entity {
	EntityType type;
	uint flags;
	int ref;

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
