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
	EntityType_laserBeam,
	EntityType_flyingVirus,
	EntityType_pickupField,
};

enum DrawOrder {
	DrawOrder_background,
	DrawOrder_middleground,
	DrawOrder_text,
	DrawOrder_tile,
	DrawOrder_pickupField,
	DrawOrder_movingTile,
	DrawOrder_endPortal,
	DrawOrder_blueEnergy,
	DrawOrder_laserBeam,
	DrawOrder_laserBase,
	DrawOrder_virus,
	DrawOrder_flyingVirus,
	DrawOrder_heavyTile,
	DrawOrder_laserBolt,
	DrawOrder_playerDeath,
	DrawOrder_player,
	DrawOrder_gui,
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
	EntityFlag_movedByGround = 1 << 9,
	EntityFlag_animIntro = 1 << 10,
	EntityFlag_animOutro = 1 << 11,
};

struct RefNode {
	s32 ref;
	RefNode* next;
};

struct Hitbox {
	V2 collisionSize;
	V2 collisionOffset;
	Hitbox* next;
};

struct Entity {
	EntityType type;
	u32 flags;
	s32 ref;

	V2 p;
	V2 dP;

	V2 renderSize;
	DrawOrder drawOrder;

	Hitbox* hitboxes;
	R2 clickBox;

	ConsoleField* fields[8];
	s32 numFields;

	RefNode* groundReferenceList;
	RefNode* ignorePenetrationList;

	float emissivity;
	double spotLightAngle;

	//Used by any entity that shoots
	double shootTimer;

	//Used by projectiles
	s32 shooterRef;

	//Used by tiles and player death
	V2 startPos;

	//Used by tiles
	s32 tileXOffset;
	s32 tileYOffset;

	//Used by any entity that jumps
	s32 jumpCount;
	double timeSinceLastOnGround;

	//Used by text entity
	Texture* messages;
	s32 numMessages;

	double animTime;
	AnimNode* currentAnim;
	AnimNode* nextAnim;

	Texture* defaultTex;
	AnimNode* standAnim;
	AnimNode* jumpAnim;
	AnimNode* shootAnim;
	AnimNode* walkAnim;
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

void setFlags(Entity* entity, u32 flags) {
	entity->flags |= flags;
} 

void clearFlags(Entity* entity, u32 flags) {
	entity->flags &= ~flags;
} 

void toggleFlags(Entity* entity, u32 toggle) {
	entity->flags ^= toggle;
}

bool32 isSet(Entity* entity, u32 flags) {
	bool32 result = (entity->flags & flags) != 0;
	return result;
}

Entity* getEntityByRef(GameState*, s32 ref);
ConsoleField* getMovementField(Entity* entity);
bool isMouseInside(Entity* entity, Input* input);
void removeTargetRef(int, GameState*);
void addTargetRef(int, GameState*);
void addField(Entity*, ConsoleField*);
void findCurrentPenetratingEntities(Entity*, GameState*);
