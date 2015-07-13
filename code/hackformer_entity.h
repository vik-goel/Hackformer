#define KEYBOARD_JUMP_AIR_TOLERANCE_TIME 0.15
#define INVALID_START_POS (v2(1, 1) * -9999999999)

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
	EntityFlag_wasSolid = 1 << 12, //Used by pickup fields
	EntityFlag_cloaked = 1 << 13,
	EntityFlag_togglingCloak = 1 << 14,
	EntityFlag_flipX = 1 << 15,
	EntityFlag_flipY = 1 << 16,
	EntityFlag_isCornerTile = 1 << 17,
	EntityFlag_jumped = 1 << 18,
};

struct RefNode {
	s32 ref;
	RefNode* next;
};

#define MAX_COLLISION_POINTS 15
#define INVALID_STORED_HITBOX_ROTATION -9999999999.0

struct Hitbox {
	V2 collisionSize; //Broad phase

	s32 collisionPointsCount;
	V2 originalCollisionPoints[MAX_COLLISION_POINTS]; //Narrow phase

	bool32 storedFlippedX;

	double storedRotation;
	V2 rotatedCollisionPoints[MAX_COLLISION_POINTS];

	V2 collisionOffset;
	Hitbox* next;
};

struct Entity {
	s32 ref;
	EntityType type;
	u32 flags;

	V2 p;
	V2 dP;
	double rotation;

	//Used by trawler
	double wheelRotation;

	V2 renderSize;
	DrawOrder drawOrder;

	Hitbox* hitboxes;
	R2 clickBox;

	s32 numFields;
	ConsoleField* fields[8];

	RefNode* groundReferenceList;
	RefNode* ignorePenetrationList;

	float emissivity;
	double spotLightAngle;
	double alpha;
	double cloakFactor;

	//Used by any entity that shoots
	s32 targetRef;

	//Used by projectiles, spawned entities
	s32 spawnerRef;

	//Used by tiles and bodyguards
	V2 startPos;

	//Used by tiles
	s32 tileXOffset;
	s32 tileYOffset;

	//Used by any entity that jumps
	s32 jumpCount;
	double timeSinceLastOnGround;

	//Used by text entity
	Messages* messages;

	double animTime;
	AnimNode* currentAnim;
	AnimNode* nextAnim;

	Texture* defaultTex;
	GlowingTexture* glowingTex;
	CharacterAnim* characterAnim;

	V2 groundNormal;
};

struct EntityHackSave {
	V2 p;
	double rotation;
	s32 tileXOffset;
	s32 tileYOffset;
	s32 messagesSelectedIndex;
	double timeSinceLastOnGround;
	s32 numFields;
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

struct ProjectPointResult {
	double hitTime;
	V2 hitLineNormal;
	bool collisionNormalFromMovingEntity;
};

struct GetCollisionTimeResult {
	Entity* hitEntity;
	double collisionTime;
	V2 collisionNormal; //Set to (0, 0) if colliding with something inside of you

	Entity* solidEntity;
	double solidCollisionTime;
	V2 solidCollisionNormal;

	bool collisionNormalFromHitEntity;
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


bool isTileType(Entity* entity) {
	bool result = entity->type == EntityType_tile || 
				  entity->type == EntityType_heavyTile ||
				  entity->type == EntityType_disappearingTile ||
				  entity->type == EntityType_droppingTile;
	return result;
}

bool isNonHeavyTile(Entity* entity) {
	bool result = isTileType(entity) && entity->type != EntityType_heavyTile;
	return result;
}

RefNode* refNode(GameState* gameState, s32 ref, RefNode* next = NULL);
void removeFromRefNodeList(RefNode** list, s32 ref, GameState* gameState);
void freeRefNode(RefNode* node, GameState* gameState);

void addGuardedTargetRef(s32 ref, GameState* gameState);
void removeGuardedTargetRef(s32 ref, GameState* gameState);

void freeWaypoints(Waypoint* waypoint, GameState* gameState);
GetCollisionTimeResult getCollisionTime(Entity*, GameState*, V2, bool actuallyMoving, double maxCollisionTime = 1, bool ignorePenetrationEntities = false);

Entity* getEntityByRef(GameState*, s32 ref);
ConsoleField* getMovementField(Entity* entity, s32* = NULL);
ConsoleField* getField(Entity* entity, ConsoleFieldType type, s32* = NULL);
bool isMouseInside(Entity* entity, Input* input);
void removeTargetRef(int, GameState*);
void addTargetRef(int, GameState*);
void addField(Entity*, ConsoleField*);
void ignoreAllPenetratingEntities(Entity*, GameState*);
