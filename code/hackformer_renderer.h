struct Texture {
	SDL_Texture* tex;
	SDL_Rect srcRect;
};

struct Animation {
	Texture* frames;
	uint numFrames;
	double secondsPerFrame;
};

struct CachedFont {
	TTF_Font* font;
	Texture cache[1 << (sizeof(char) * 8)];
	double scaleFactor;
	double lineHeight;
};