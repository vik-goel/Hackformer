enum ConsoleFieldType {
	ConsoleField_cameraFollows,

	ConsoleField_keyboardControlled,
	ConsoleField_movesBackAndForth,
	ConsoleField_bobsVertically,
	ConsoleField_seeksTarget,

	ConsoleField_isShootTarget,
	ConsoleField_shootsAtTarget,

	ConsoleField_givesEnergy,
	ConsoleField_killsOnHit,
	ConsoleField_Alertness,
	ConsoleField_followsWaypoints,
	ConsoleField_spotlight,

	ConsoleField_double,
	ConsoleField_unlimitedInt,
	ConsoleField_s32,
	ConsoleField_bool,
};

enum ConsoleFlag {
	ConsoleFlag_selected = 1 << 0,
	ConsoleFlag_remove = 1 << 1,
	ConsoleFlag_childrenVisible = 1 << 2,
	ConsoleFlag_wasLeftArrowSelected = 1 << 3,
	ConsoleFlag_wasRightArrowSelected = 1 << 4,
};


enum Alertness {
	Alertness_asleep,
	Alertness_patrolling,
	Alertness_searching,
	Alertness_detected,

	Alertness_count
};

struct Waypoint {
	V2 p;
	Waypoint* next;
};

struct ConsoleField {
	ConsoleFieldType type; //NOTE: This must NEVER be -1
	u32 flags;

	char name[20];
	
	s32 numValues;

	//NOTE: These attributes are used for fields with many values
	union {
		double doubleValues[5];
		s32 s32Values[10];

		struct {
			Waypoint* curWaypoint;
			double waypointDelay;
		};

		struct {
			double bobHeight;
			bool32 bobbingUp;
			bool32 initialBob;
		};
	};
	
	s32 selectedIndex;
	s32 initialIndex;

	s32 tweakCost;
	
	V2 p;

	//NOTE: These attributes are used for drag and droppable fields
	V2 offs;

	//NOTE: This is used for storage in the free list
	ConsoleField* next;

	//NOTE: This is used for hierarchal console fields
	s32 numChildren;
	ConsoleField* children[4];
	double childYOffs;
};

enum ButtonState {
	ButtonState_cantAfford,
	ButtonState_default,
	ButtonState_hover,
	ButtonState_clicked
};

struct FieldSpec {
	V2 fieldSize;
	V2 tileArrowSize;
	V2 triangleSize;
	V2 valueSize;
	
	V2 spacing;
	double childInset;
	double valueBackgroundPenetration;

	V2 mouseOffset;

	int hackEnergy;

	TextureData attribute;
	TextureData behaviour;
	TextureData valueBackground;
	TextureData leftButtonClicked;
	TextureData leftButtonUnavailable;
	TextureData leftButtonDefault;

	TextureData waypoint;
	TextureData waypointArrow;

	TextureData tileHackShield;
	TextureData tileHackArrow;

	CachedFont consoleFont;
};

bool isConsoleFieldMovementType(ConsoleField* field) {
	bool result = field->type == ConsoleField_keyboardControlled ||
				  field->type == ConsoleField_movesBackAndForth ||
				  field->type == ConsoleField_seeksTarget ||
				  field->type == ConsoleField_followsWaypoints ||
				  field->type == ConsoleField_bobsVertically;
	return result;
}

//TODO: This could be a flag instead
bool canOnlyHaveOneFieldOfType(ConsoleFieldType type) {
	bool result = type == ConsoleField_isShootTarget ||
				  type == ConsoleField_shootsAtTarget ||
				  type == ConsoleField_cameraFollows || 
				  type == ConsoleField_killsOnHit || 
				  type == ConsoleField_spotlight;

 	return result;
}

void setFlags(ConsoleField* field, u32 flags) {
	field->flags |= flags;
} 

void clearFlags(ConsoleField* field, u32 flags) {
	field->flags &= ~flags;
} 

void toggleFlags(ConsoleField* field, u32 toggle) {
	field->flags ^= toggle;
}

bool32 isSet(ConsoleField* field, u32 flags) {
	bool32 result = (field->flags & flags) != 0;
	return result;
}

bool drawConsoleField(ConsoleField*, RenderGroup*, Input*, FieldSpec*, bool, bool, GameState*);