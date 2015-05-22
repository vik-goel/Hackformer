struct Texture {
	GLuint texId;
	GLuint normalId;
	R2 uv;
	V2 size;
};

struct Animation {
	Texture* frames;
	u32 numFrames;
	double secondsPerFrame;
	bool32 pingPong;
	bool32 reverse;
};

struct AnimNode {
	Animation intro;
	Animation main;
	Animation outro;
	bool32 finishMainBeforeOutro;
};

struct Glyph {
	Texture tex;

	//min for bottom and left
	//max for top and right
	R2 padding; //stored in meters
};

struct CachedFont {
	TTF_Font* font;
	Glyph cache[1 << (sizeof(char) * 8)];
	double scaleFactor;
	double lineHeight;
};

enum DrawType {
	DrawType_RenderBoundedTexture,
	DrawType_RenderEntityTexture,
	DrawType_RenderText,
	DrawType_RenderFillRect,
	DrawType_RenderOutlinedRect,
	DrawType_RenderConsoleField,
	DrawType_RenderDashedLine,
	DrawType_RenderRotatedTexture,
};

struct Color {
	u8 r, g, b, a;
};

struct PointLight {
	V3 p;
	V3 color;
	double range;
};

struct SpotLight {
	PointLight base;
	double angle; //degrees
	double spread;
};

struct PointLightUniforms {
	GLint p;
	GLint color;
	GLint range;
};

struct SpotLightUniforms {
	PointLightUniforms base;
	GLint dir;
	GLint cutoff;
};

struct Shader {
	GLuint program;
	GLint tintUniformLocation;
};

struct ForwardShader {
	Shader shader;

	GLint ambientUniform;
	GLint normalXFlipUniform;

	PointLight pointLights[32];
	s32 numPointLights;

	SpotLight spotLights[32];
	s32 numSpotLights;

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


#define RENDER_HEADER_TYPE_MASK ((1 << 15) - 1)
#define RENDER_HEADER_CLIP_RECT_FLAG (1 << 15)
struct RenderHeader {
//NOTE: The lower 15 bits of this type field is used to store the DrawType
//		The 16th bit is used to store whether or not the elem has a clip rect or not
	u32 type_;
};

enum DrawOrder;

struct RenderTexture {
	DrawOrder drawOrder;
	Texture* texture;
	bool8 flipX;
	Orientation orientation;
	float emissivity;
	Color color;
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

struct RenderRotatedTexture {
	Texture* texture;
	R2 bounds;
	double rad;
	Color color;
};

struct RenderText {
	CachedFont* font;
	char msg[50];
	V2 p;
	Color color;
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

struct ConsoleField;

struct RenderConsoleField {
	ConsoleField* field;
};

struct RenderDashedLine {
	Color color;
	V2 lineStart;
	V2 lineEnd;
	double thickness;
	double dashSize, spaceSize;
};

struct RenderGroup {
	ForwardShader forwardShader;
	Shader basicShader;
	Shader* currentShader;

	Texture whiteTex;
	GLuint nullNormalId;

	double pixelsPerMeter;
	s32 windowWidth;
	s32 windowHeight;
	R2 windowBounds;
	V2 negativeCameraP;

	GLfloat ambient;

	bool32 enabled;
	bool32 rendering;

	R2 clipRect;
	bool32 hasClipRect;

	RenderHeader* sortPtrs[1000];
	s32 numSortPtrs;
	size_t sortAddressCutoff;

	void* base;
	size_t allocated;
	size_t maxSize;
};
