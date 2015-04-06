enum EntityType {
	EntityType_background,
	EntityType_player,
	EntityType_playerDeath,
	EntityType_tile,
	EntityType_heavyTile,
	EntityType_blueEnergy,
	EntityType_text,
	EntityType_virus,
	EntityType_laserBolt,
	EntityType_endPortal,
	EntityType_laserBase,
	EntityType_laserTop,
	EntityType_laserBeam,
	EntityType_flyingVirus,
};

enum DrawOrder {
	DrawOrder_background,
	DrawOrder_text,
	DrawOrder_tile,
	DrawOrder_endPortal,
	DrawOrder_blueEnergy,
	DrawOrder_laserController,
	DrawOrder_laserBase,
	DrawOrder_laserTop,
	DrawOrder_laserBeam,
	DrawOrder_virus,
	DrawOrder_flyingVirus,
	DrawOrder_heavyTile,
	DrawOrder_laserBolt,
	DrawOrder_playerDeath,
	DrawOrder_player,
};

enum EntityFlag {
	EntityFlag_facesLeft = 1 << 0,
	EntityFlag_noMovementByDefault = 1 << 1,
	EntityFlag_hackable = 1 << 2,
	EntityFlag_remove = 1 << 3,
	EntityFlag_removeWhenOutsideLevel = 1 << 4,
	EntityFlag_shooting = 1 << 5,
	EntityFlag_unchargingAfterShooting = 1 << 6,
	EntityFlag_grounded = 1 << 7,
	EntityFlag_laserOn = 1 << 8,
};

enum AnimState {
	AnimState_standing, //NOTE: Standing must be 0
	AnimState_shooting,
	AnimState_jumping,
	AnimState_walking,
};

struct RefNode {
	int ref;
	RefNode* next;
};

struct Hitbox {
	V2 collisionSize;
	V2 collisionOffset;
	Hitbox* next;
};

struct Entity {
	EntityType type;
	uint flags;
	int ref;

	V2 p;
	V2 dP;

	V2 renderSize;
	double animTime;
	DrawOrder drawOrder;

	Hitbox* hitboxes;
	R2 clickBox;

	ConsoleField* fields[8];
	int numFields;

	RefNode* groundReferenceList;

	//Used by any entity that shoots
	double shootTimer;

	//Used by projectiles
	int shooterRef;

	//Used by tiles and player death
	V2 startPos;

	//Used by tiles
	int tileXOffset;
	int tileYOffset;

	//Used by any entity that jumps
	int jumpCount;
	double timeSinceLastOnGround;

	//Used by text entity
	Texture* messages;
	int numMessages;

	AnimState lastAnimState;
	AnimState prevAnimState;

	Texture* standTex;
	Texture* jumpTex;
	Animation* shootAnim;
	Animation* walkAnim;
	Animation* standWalkTransition;
};

struct EntityReference {
	Entity* entity;
	EntityReference* next;
};

struct TileMoveNode;
struct TileMove {
	Entity* tile;
	TileMoveNode* children;
	int numParents;
};

struct TileMoveNode {
	TileMove* move;
	TileMoveNode* next;
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