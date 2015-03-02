#ifndef HACKFORMER_RENDERER_H
#define HACKFORMER_RENDERER_H

struct Texture {
	SDL_Texture* tex;
	SDL_Rect srcRect;
};

struct Animation {
	Texture* frames;
	uint numFrames;
	float secondsPerFrame;
};

#endif