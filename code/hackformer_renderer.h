

struct Animation {
	s32 numFrames;
	Texture* frames;
	double secondsPerFrame;
	bool32 pingPong;
	bool32 reverse;
};

struct AnimNode {
	s32 dataIndex;
};

struct AnimNodeData {
	Animation intro;
	Animation main;
	Animation outro;
	bool32 finishMainBeforeOutro;
};

struct CharacterAnim {
	s32 dataIndex;
};

struct CharacterAnimData {
	AnimNode standAnim;
	AnimNode jumpAnim;
	AnimNode shootAnim;
	AnimNode walkAnim;
	AnimNode disappearAnim;
	AnimNode deathAnim;
};

enum TextAlignment {
	TextAlignment_center,
	TextAlignment_bottomLeft,
};

struct Glyph {
	TextureData tex;

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
	DrawType_RenderFilledStencil,
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

	PointLight pointLights[32];
	s32 numPointLights;

	SpotLight spotLights[32];
	s32 numSpotLights;

	PointLightUniforms pointLightUniforms[8];
	SpotLightUniforms spotLightUniforms[8];
};



#define RENDER_HEADER_CLIP_RECT_FLAG (1 << 15)
#define RENDER_HEADER_TYPE_MASK (RENDER_HEADER_CLIP_RECT_FLAG - 1)
struct RenderHeader {
//NOTE: The lower 15 bits of this type field is used to store the DrawType
//		The 16th bit is used to store whether or not the elem has a clip rect or not
	u32 type_;
};

enum DrawOrder;

struct RenderTexture {
	DrawOrder drawOrder;
	TextureData* texture;
	bool8 flipX;
	Orientation orientation;
	float emissivity;
	Color color;
};

struct RenderEntityTexture {
	RenderTexture tex;
	V2* p;
	V2* renderSize;
	double rotation;
};

struct RenderBoundedTexture {
	RenderTexture tex;
	R2 bounds;
};

struct RenderRotatedTexture {
	RenderTexture tex;
	R2 bounds;
	double rad;
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

struct RenderFilledStencil {
	TextureData* stencil;
	R2 bounds;
	double widthPercentage;
	Color color;
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
	Shader stencilShader;
	Shader* currentShader;

	TextureData whiteTex;

	double pixelsPerMeter;
	s32 windowWidth;
	s32 windowHeight;
	R2 windowBounds;
	Camera* camera;

	GLfloat ambient;

	bool32 enabled;
	bool32 rendering;

	R2 defaultClipRect;
	R2 clipRect;
	bool32 hasClipRect;

	RenderHeader* sortPtrs[5000];
	s32 numSortPtrs;
	size_t sortAddressCutoff;

	void* base;
	size_t allocated;
	size_t maxSize;

	TextureData* textureData;
	s32* textureDataCount;
};
