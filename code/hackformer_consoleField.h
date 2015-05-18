enum ConsoleFieldType {
	ConsoleField_cameraFollows,
	ConsoleField_keyboardControlled,
	ConsoleField_movesBackAndForth,
	ConsoleField_seeksTarget,
	ConsoleField_isShootTarget,
	ConsoleField_shootsAtTarget,
	ConsoleField_givesEnergy,
	ConsoleField_killsOnHit,
	ConsoleField_double,
	ConsoleField_unlimitedInt,
	ConsoleField_s32,
	ConsoleField_bool,
};

enum ConsoleFieldFlags {
	ConsoleFlag_selected = 1 << 0,
	ConsoleFlag_remove = 1 << 1,
	ConsoleFlag_childrenVisible = 1 << 2,
};

struct ConsoleField {
	ConsoleFieldType type;
	u32 flags;

	char name[100];
	
	//NOTE: These attributes are used for fields with many values
	union {
		double doubleValues[5];
		s32 intValues[10];
	};
	s32 numValues;
	
	s32 selectedIndex;
	s32 initialIndex;

	s32 tweakCost;
	
	char valueStr[100];

	V2 p;

	//NOTE: These attributes are used for drag and droppable fields
	V2 offs;

	//NOTE: This is used for storage in the free list
	ConsoleField* next;

	//NOTE: This is used for hierarchal console fields
	ConsoleField* children[4];
	s32 numChildren;
	double childYOffs;
};

struct FieldSpec {
	V2 fieldSize;
	V2 triangleSize;
	V2 valueSize;
	V2 spacing;
	double childInset;

	V2 mouseOffset;

	int blueEnergy;

	Texture consoleTriangle;
	Texture consoleTriangleYellow;
	Texture consoleTriangleGrey;
	Texture consoleTriangleSelected;

	Texture attribute;
	Texture behaviour;

	CachedFont consoleFont;
};

bool isConsoleFieldMovementType(ConsoleField* field) {
	bool result = field->type == ConsoleField_keyboardControlled ||
				  field->type == ConsoleField_movesBackAndForth ||
				  field->type == ConsoleField_seeksTarget;
	return result;
}

//TODO: This could be a flag instead
bool canOnlyHaveOneFieldOfType(ConsoleFieldType type) {
	bool result = type == ConsoleField_isShootTarget ||
				  type == ConsoleField_shootsAtTarget ||
				  type == ConsoleField_cameraFollows || 
				  type == ConsoleField_killsOnHit;

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

bool drawConsoleField(ConsoleField*, RenderGroup*, Input*, FieldSpec*, bool = true);