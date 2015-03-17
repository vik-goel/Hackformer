enum ConsoleFieldType {
	ConsoleField_keyboardControlled,
	ConsoleField_movesBackAndForth,
	ConsoleField_isShootTarget,
	ConsoleField_shootsAtTarget,
	ConsoleField_float,
	ConsoleField_unlimitedInt,
	ConsoleField_bool,
};

struct Entity;

struct ConsoleField {
	ConsoleFieldType type;

	Texture nameTex;
	char* name;
	
	//NOTE: These attributes are used for fields with many values
	void* values;
	int numValues;
	
	int selectedIndex;
	int initialIndex;
	
	Texture valueTex;
	bool validValueTex;

	//NOTE: These attributes are used for drag and droppable fields
	V2 offs;
	bool selected;
};