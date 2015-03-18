enum ConsoleFieldType {
	ConsoleField_keyboardControlled,
	ConsoleField_movesBackAndForth,
	ConsoleField_isShootTarget,
	ConsoleField_shootsAtTarget,
	ConsoleField_float,
	ConsoleField_unlimitedInt,
	ConsoleField_bool,
};

enum ConsoleFieldFlags {
	ConsoleFlag_validValueTex = 1 << 0,
	ConsoleFlag_selected = 1 << 1,
	ConsoleFlag_remove = 1 << 2,
	ConsoleFlag_childrenVisible = 1 << 3,
};

struct ConsoleField {
	ConsoleFieldType type;
	uint flags;

	Texture nameTex;
	char* name;
	
	//NOTE: These attributes are used for fields with many values
	void* values;
	int numValues;
	
	int selectedIndex;
	int initialIndex;
	
	Texture valueTex;

	//NOTE: These attributes are used for drag and droppable fields
	V2 offs;

	//NOTE: This is used for storage in the free list
	ConsoleField* next;

	//NOTE: This is used for hierarchal console fields
	ConsoleField** children;
	int numChildren;
};

bool isConsoleFieldMovementType(ConsoleField* field) {
	bool result = field->type == ConsoleField_keyboardControlled ||
				  field->type == ConsoleField_movesBackAndForth;
	return result;
}

void setFlags(ConsoleField* field, uint flags) {
	field->flags |= flags;
} 

void clearFlags(ConsoleField* field, uint flags) {
	field->flags &= ~flags;
} 

void toggleFlags(ConsoleField* field, uint toggle) {
	field->flags ^= toggle;
}

bool isSet(ConsoleField* field, uint flags) {
	bool result = (field->flags & flags) != 0;
	return result;
}