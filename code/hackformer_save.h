#define MAX_HACK_UNDOS 100

struct SaveReference {
	s32 index;
	size_t size;
	void* save;
};

struct SaveMemoryHeader {
	s32 numSaveGames;
	SaveReference saves[MAX_HACK_UNDOS];
};

//Savegames
void saveCompleteGame(GameState* gameState, char* fileName);
void loadCompleteGame(GameState* gameState, char* fileName);

//Checkpoints
void saveCompleteGame(GameState* gameState, MemoryArena* arena);
void loadCompleteGame(GameState* gameState, MemoryArena* arena);

//Hacking
void updateSaveGame(GameState* gameState, bool initialSave = false);
void undoLastSaveGame(GameState* gameState);
void undoAllHackMoves(GameState* gameState);
double getEnergyLoss(GameState* gameState);
