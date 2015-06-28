void saveGame(GameState* gameState, char* fileName);
void loadGame(GameState* gameState, char* fileName);
void saveGameToArena(GameState* gameState);
void loadGameFromArena(GameState* gameState);
void updateSaveGameToArena(GameState* gameState);
void undoLastSaveGameFromArena(GameState* gameState);
s32  getEnergyLoss(GameState* gameState);

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