struct Texture {
	//GLuint texId;
	//R2 uv;
	SDL_Texture* tex;
	SDL_Rect srcRect;
};

struct Animation {
	Texture* frames;
	uint numFrames;
	double secondsPerFrame;
	bool pingPong;
};

struct CachedFont {
	TTF_Font* font;
	Texture cache[1 << (sizeof(char) * 8)];
	double scaleFactor;
	double lineHeight;
};

enum DrawType {
	DrawType_RenderBoundedTexture,
	DrawType_RenderEntityTexture,
	DrawType_RenderText,
	DrawType_RenderFillRect,
	DrawType_RenderOutlinedRect,
};

struct Color {
	char r, g, b, a;
};

#define RENDER_HEADER_TYPE_MASK ((1 << 16) - 1)
#define RENDER_HEADER_CLIP_RECT_FLAG (1 << 16)
struct RenderHeader {
//NOTE: The lower 15 bits of this type field is used to store the DrawType
//		The 16th bit is used to store whether or not the elem has a clip rect or not
	uint type_;
};

enum DrawOrder;

struct RenderTexture {
	DrawOrder drawOrder;
	Texture* texture;
	bool flipX;
	double degrees;
};

struct RenderEntityTexture {
	RenderTexture tex;
	V2* p;
	V2* renderSize;
};

struct RenderBoundedTexture {
	RenderTexture tex;
	R2 bounds;
};

struct RenderText {
	CachedFont* font;
	char msg[50];
	V2 p;
};

struct RenderFillRect {
	R2 bounds;
	Color color;
};

struct RenderOutlinedRect {
	R2 bounds;
	Color color;
	double thickness;
};

struct RenderGroup {
	SDL_Renderer* renderer;
	double pixelsPerMeter;
	int windowHeight;
	R2 windowBounds;
	V2 negativeCameraP;
	bool enabled;

	R2 clipRect;
	bool hasClipRect;

	RenderHeader* sortPtrs[1000];
	int numSortPtrs;
	int sortAddressCutoff;

	void* base;
	uint allocated;
	uint maxSize;
};

// enum Rotation {
// 	Degree0,
// 	Degree90,
// 	Degree180,
// 	Degree270,
// 	RotationCount
// };

// struct Shader {
// 	GLuint program;
// 	GLint tintUniformLocation;
// };