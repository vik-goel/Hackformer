struct Texture {
	GLuint texId;
	R2 uv;
	V2 size;
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

enum Rotation {
	Degree0,
	Degree90,
	Degree180,
	Degree270,
	RotationCount
};