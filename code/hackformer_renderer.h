struct Texture {
	GLuint texId;
	GLuint normalId;
	R2 uv;
	V2 size;
	// SDL_Texture* tex;
	// SDL_Rect srcRect;
};

struct Animation {
	Texture* frames;
	uint numFrames;
	double secondsPerFrame;
	bool pingPong;
	bool reverse;
};

struct AnimNode {
	Animation intro;
	Animation main;
	Animation outro;
	bool finishMainBeforeOutro;
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
	DrawType_pointLight,
	DrawType_spotLight,
};

struct Color {
	unsigned char r, g, b, a;
};

struct PointLight {
	V3 p;
	V3 color;
	V3 atten; //(constant, linear, exponent)
};

struct SpotLight {
	PointLight base;
	double angle;
	double spread;
};

struct PointLightUniforms {
	GLint p;
	GLint color;
	GLint atten;
};

struct SpotLightUniforms {
	PointLightUniforms base;
	GLint dir;
	GLint cutoff;
};

struct Shader {
	GLuint program;
	GLint tintUniformLocation;
	GLint ambientUniform;
	GLint normalXFlipUniform;

	PointLight pointLights[32];
	int numPointLights;

	SpotLight spotLights[32];
	int numSpotLights;

	PointLightUniforms pointLightUniforms[8];
	SpotLightUniforms spotLightUniforms[8];
};


enum Orientation {
	Orientation_0,
	Orientation_90,
	Orientation_180,
	Orientation_270,

	Orientation_count
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
	Orientation orientation;
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
	Shader shader;
	Texture whiteTex;
	GLuint nullNormalId;

	SDL_Renderer* renderer;
	double pixelsPerMeter;
	int windowWidth;
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
