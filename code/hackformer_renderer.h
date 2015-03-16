struct Texture {
	SDL_Texture* tex;
	SDL_Rect srcRect;
};

struct Animation {
	Texture* frames;
	uint numFrames;
	float secondsPerFrame;
};