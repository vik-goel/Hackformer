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
	DrawOrder_background,
	DrawOrder_text,
	DrawOrder_tile,
	DrawOrder_blueEnergy,
	DrawOrder_virus,
	DrawOrder_laserBolt,
	DrawOrder_player,
};

enum EntityFlag {
	EntityFlag_facesLeft = 1 << 0,
	EntityFlag_noMovementByDefault = 1 << 1,
	EntityFlag_hackable = 1 << 2,
	EntityFlag_remove = 1 << 3,
	EntityFlag_removeWhenOutsideLevel = 1 << 4,
	EntityFlag_ignoresGravity = 1 << 5,
	EntityFlag_ignoresFriction = 1 << 6,
	EntityFlag_shooting = 1 << 7,
	EntityFlag_unchargingAfterShooting = 1 << 8,
	EntityFlag_grounded = 1 << 9,
};

struct RefNode {
	int ref;
	RefNode* next;
};

struct Entity {
	EntityType type;
	uint flags;
	int ref;

	V2 p;
	V2 dP;

	V2 renderSize;
	Texture* texture;
	double animTime;
	DrawOrder drawOrder;

	V2 collisionSize;
	V2 collisionOffset;

	ConsoleField* fields[8];
	int numFields;

	RefNode* groundReferenceList;

	//Used by any entity that shoots
	double shootTimer;

	//Used by projectiles
	int shooterRef;

	//Used by tiles
	V2 tileStartPos;
	int tileXOffset;
	int tileYOffset;
};

struct EntityReference {
	Entity* entity;
	EntityReference* next;
};

struct TileMove {
	Entity* tile;
	Entity** children;
	int numChildren;
	int numParents;
};

struct GetCollisionTimeResult {
	double collisionTime;
	Entity* hitEntity;
	bool horizontalCollision;

	Entity* solidEntity;
	double solidCollisionTime;
	bool solidHorizontalCollision;
};

void setFlags(Entity* entity, uint flags) {
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