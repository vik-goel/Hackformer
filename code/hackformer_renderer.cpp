void loadShaderSource(char* fileName, char* source, s32 maxSourceLength) {
	FILE* file = fopen(fileName, "r");
	assert(file);

	char line[1024];
	char* linePtr;
	s32 length = 0;

	while (fgets (line, sizeof(line), file)) {
		linePtr = line;

		while(*linePtr) {
			*source = *linePtr;
			source++;
			linePtr++;
			length++;
			assert(length < maxSourceLength);
		}
	}

	*source = 0;
	fclose(file);
}

GLuint addShader_(GLenum type, char* sourceFileName, Shader shaderProgram) {
	char sourceBuffer[4000];
	char* source = sourceBuffer;
	loadShaderSource(sourceFileName, source, arrayCount(sourceBuffer));

	GLuint shader = glCreateShader(type);
	assert(shader);

	glShaderSource(shader, 1, &source, NULL);

	glCompileShader(shader);
	GLint compileSuccess = 0;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &compileSuccess);

	if (compileSuccess == GL_FALSE) {
		char errorBuffer[1024];
		s32 errorLength = 0;
		glGetShaderInfoLog(shader, arrayCount(errorBuffer), &errorLength, errorBuffer);
		fprintf(stderr, "Failed to compile shader: %s\n", errorBuffer);
		InvalidCodePath;
	}

	glAttachShader(shaderProgram.program, shader);

	return shader;
}

Shader createShader(char* vertexShaderSourceFileName, char* fragmentShaderSourceFileName, V2 windowSize) {
	Shader result = {};

	result.program = glCreateProgram();
	assert(result.program);

	GLuint vertexShader = addShader_(GL_VERTEX_SHADER, vertexShaderSourceFileName, result);
	GLuint fragmentShader = addShader_(GL_FRAGMENT_SHADER, fragmentShaderSourceFileName, result);

	glLinkProgram(result.program);

	GLint linkSuccess = 0;
	glGetProgramiv(result.program, GL_LINK_STATUS, &linkSuccess);

	if (linkSuccess == GL_FALSE) {
		GLchar errorBuffer[1024];
		GLsizei errorLength = 0;
		glGetProgramInfoLog(result.program, (GLsizei)arrayCount(errorBuffer), &errorLength, errorBuffer);
		fprintf(stderr, "Failed to link shader: %s\n", errorBuffer);
		InvalidCodePath;
	}

	glDetachShader(result.program, vertexShader);
	glDetachShader(result.program, fragmentShader);

	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);

	glUseProgram(result.program);
	GLint windowSizeUniformLocation = glGetUniformLocation(result.program, "twoOverScreenSize");
	glUniform2f(windowSizeUniformLocation, (GLfloat)(2.0/windowSize.x), (GLfloat)(2.0/windowSize.y));

	result.tintUniformLocation = glGetUniformLocation(result.program, "tint");

	GLint diffuseLocation = glGetUniformLocation(result.program, "diffuseTexture");
	glUniform1i(diffuseLocation, 0);

	return result;
}

ForwardShader createForwardShader(char* vertexShaderSourceFileName, char* fragmentShaderSourceFileName, V2 windowSize) {
	ForwardShader result = {};

	result.shader = createShader(vertexShaderSourceFileName, fragmentShaderSourceFileName, windowSize);

	result.ambientUniform = glGetUniformLocation(result.shader.program, "ambient");

	char* uniformPrefix = "pointLights[";
	char uniformName[128];

	for(s32 lightIndex = 0; lightIndex < arrayCount(result.pointLightUniforms); lightIndex++) {
		PointLightUniforms* lightUniforms = result.pointLightUniforms + lightIndex;

		sprintf(uniformName, "%s%d].p", uniformPrefix, lightIndex);
		lightUniforms->p = glGetUniformLocation(result.shader.program, uniformName);

		sprintf(uniformName, "%s%d].color", uniformPrefix, lightIndex);
		lightUniforms->color = glGetUniformLocation(result.shader.program, uniformName);

		sprintf(uniformName, "%s%d].range", uniformPrefix, lightIndex);
		lightUniforms->range = glGetUniformLocation(result.shader.program, uniformName);
	}

	uniformPrefix = "spotLights[";

	for(s32 lightIndex = 0; lightIndex < arrayCount(result.spotLightUniforms); lightIndex++) {
		SpotLightUniforms* lightUniforms = result.spotLightUniforms + lightIndex;

		sprintf(uniformName, "%s%d].base.p", uniformPrefix, lightIndex);
		lightUniforms->base.p = glGetUniformLocation(result.shader.program, uniformName);

		sprintf(uniformName, "%s%d].base.color", uniformPrefix, lightIndex);
		lightUniforms->base.color = glGetUniformLocation(result.shader.program, uniformName);

		sprintf(uniformName, "%s%d].dir", uniformPrefix, lightIndex);
		lightUniforms->dir = glGetUniformLocation(result.shader.program, uniformName);

		sprintf(uniformName, "%s%d].cutoff", uniformPrefix, lightIndex);
		lightUniforms->cutoff = glGetUniformLocation(result.shader.program, uniformName);

		sprintf(uniformName, "%s%d].base.range", uniformPrefix, lightIndex);
		lightUniforms->base.range = glGetUniformLocation(result.shader.program, uniformName);
	}

	return result;
}

void freeTexture(Texture* texture) {
	if(texture->texId) {
		glDeleteTextures(1, &texture->texId);
		texture->texId = 0;
	}
}

GLuint generateTexture(SDL_Surface* image, bool srgb = false) {
	glEnable(GL_TEXTURE_2D);

	GLuint result = 0;

	glGenTextures(1, &result);
	assert(result);

	glBindTexture(GL_TEXTURE_2D, result);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	GLenum format;

	if (image->format->BytesPerPixel == 3) {
		format = GL_RGB;
	} else if (image->format->BytesPerPixel == 4) {
		format = GL_RGBA;
	} else {
		InvalidCodePath;
	}

	GLint internalFormal = srgb ? GL_SRGB8_ALPHA8 : GL_RGBA;

	glTexImage2D(GL_TEXTURE_2D, 0, internalFormal, image->w, image->h, 0, format, GL_UNSIGNED_BYTE, image->pixels);

	return result;
}

Texture createTexFromSurface(SDL_Surface* image, RenderGroup* group, bool stencil) {
	Texture result = {};

	result.texId = generateTexture(image, !stencil);
	result.uv = r2(v2(0, 0), v2(1, 1));
	result.size = v2(image->w, image->h) * (1.0 / group->pixelsPerMeter);

	SDL_FreeSurface(image);
	glBindTexture(GL_TEXTURE_2D, 0);

	return result;
}

//stencil is false by default
Texture* loadPNGTexture(RenderGroup* group, char* fileName, bool stencil) {
	assert(strlen(fileName) < 400);

	char diffuseFilePath[500];
	sprintf(diffuseFilePath, "res/%s.png", fileName);

	SDL_RWops* readStream = SDL_RWFromFile(diffuseFilePath, "r");
	SDL_Surface* diffuse = IMG_LoadPNG_RW(readStream);
	SDL_FreeRW(readStream);

	if (!diffuse) {
		fprintf(stderr, "Failed to load diffuse texture from: %s\n", diffuseFilePath);
		InvalidCodePath;
	}

	Texture* result = group->textures + *group->texturesCount;
	*result = createTexFromSurface(diffuse, group, stencil);

	*group->texturesCount = *group->texturesCount + 1;
	assert(*group->texturesCount < MAX_TEXTURES);

	return result;
}

double getAspectRatio(Texture* texture) {
	double result = texture->size.x / texture->size.y;
	return result;
}

V2 getDrawSize(Texture* texture, double height) {
	V2 result = {};

	double aspectRatio = getAspectRatio(texture);
	result.x = height * aspectRatio;
	result.y = height;

	return result;
}

TTF_Font* loadFont(char* fileName, s32 fontSize) {
	TTF_Font *font = TTF_OpenFont(fileName, fontSize);

	if (!font) {
		fprintf(stderr, "Failed to load font: %s\n", fileName);
		InvalidCodePath;
	}

	TTF_SetFontHinting(font, TTF_HINTING_LIGHT);

	return font;
}

CachedFont loadCachedFont(RenderGroup* group, char* fileName, s32 fontSize, double scaleFactor) {
	CachedFont result = {};
	result.font = loadFont(fileName, (s32)round(fontSize * scaleFactor));
	result.scaleFactor = scaleFactor;
	result.lineHeight = fontSize / (group->pixelsPerMeter * scaleFactor);
	return result;
}

Texture* extractTextures(RenderGroup* group, char* fileName, s32 frameWidth, s32 frameHeight, s32 frameSpacing, s32* numFrames) {

	Texture* tex = loadPNGTexture(group, fileName);
	
	s32 texWidth = (s32)(tex->size.x * group->pixelsPerMeter + 0.5);
	s32 texHeight = (s32)(tex->size.y * group->pixelsPerMeter + 0.5);

	s32 numCols = texWidth / (frameWidth + frameSpacing);
	s32 numRows = texHeight / (frameHeight + frameSpacing);

	assert(frameWidth > 0);
	assert(frameHeight > 0);

	//TODO: Add a way to test this
	//assert(width % numCols == 0);
	//assert(height % numRows == 0);

	*numFrames = numRows * numCols;
	Texture* result = tex + 1;

	V2 frameSize = v2((double)(frameWidth + frameSpacing) / (double)texWidth, 
		(double)(frameHeight + frameSpacing) / (double)texHeight);

	V2 texSize = v2((double)frameWidth / (double)texWidth, (double)frameHeight/(double)texHeight);

	V2 frameOffset = v2(frameSpacing / (double)texWidth, frameSpacing / (double)texHeight) * 0.5;

	V2 dataTexSize = hadamard(texSize, tex->size);

	for (s32 rowIndex = 0; rowIndex < numRows; rowIndex++) {
		for (s32 colIndex = 0; colIndex < numCols; colIndex++) {
			s32 textureIndex = colIndex + rowIndex * numCols;

			Texture* data = result + textureIndex;
			data->texId = tex->texId;

			V2 minCorner = hadamard(v2(colIndex, rowIndex), frameSize) + frameOffset;
			data->uv = r2(minCorner, minCorner + texSize);
			data->size = dataTexSize;
		}
	}

	*group->texturesCount = *group->texturesCount + *numFrames;
	assert(*group->texturesCount < MAX_TEXTURES);

	return result;
}

Animation loadAnimation(RenderGroup* group, char* fileName, 
	s32 frameWidth, s32 frameHeight, double secondsPerFrame, bool pingPong) {
	Animation result = {};

	assert(secondsPerFrame > 0);
	result.secondsPerFrame = secondsPerFrame;
	result.frames = extractTextures(group, fileName, frameWidth, frameHeight, 0, &result.numFrames);
	result.pingPong = pingPong;
	result.frameWidth = frameWidth;
	result.frameHeight = frameHeight;
	
	assert(strlen(fileName) < arrayCount(result.fileName));
	strcpy(result.fileName, fileName);

	return result;
}

Animation createAnimation(Texture* tex) {
	Animation result = {};
	result.secondsPerFrame = 0;
	result.frames = tex;
	result.numFrames = 1;
	return result;
}

double getAnimationDuration(Animation* animation) {
	double result = animation->numFrames * animation->secondsPerFrame;
	return result;
}

Texture* getAnimationFrame(Animation* animation, double animTime) {
	bool32 reverse = animation->reverse;

	s32 frameIndex = 0;
	if (animation->secondsPerFrame > 0) {
		frameIndex = (s32)(animTime / animation->secondsPerFrame) % animation->numFrames;
	}

	if(animation->pingPong && animation->numFrames > 1) {
		double duration = getAnimationDuration(animation);

		if(animTime > duration) {
			duration -= animation->secondsPerFrame;
			animTime -= duration;

			frameIndex = (s32)(animTime / animation->secondsPerFrame) % (animation->numFrames - 1);

			s32 numTimesPlayed = (s32)(animTime / duration) + 1;

			if (numTimesPlayed % 2 == 1) {
				reverse = !reverse;
			}
		}
	}

	

	if (reverse) {
		frameIndex = (animation->numFrames - 1) - frameIndex;
	}

	Texture* result = animation->frames + frameIndex;
	return result;
}

#ifdef HACKFORMER_GAME

Animation createReversedAnimation(Animation* anim) {
	Animation result = *anim;
	result.reverse = !result.reverse;
	return result;
}

GlowingTexture* createGlowingTexture(GameState* gameState) {
	GlowingTexture* result = gameState->glowingTextures + gameState->glowingTexturesCount++;
	assert(gameState->glowingTexturesCount < MAX_GLOWING_TEXTURES);
	return result;
}

AnimNode* createAnimNode(GameState* gameState, AnimNode** anim = NULL) {
	AnimNode* result = gameState->animNodes + gameState->animNodesCount++;
	assert(gameState->animNodesCount < MAX_ANIM_NODES);
	if(anim) *anim = result;
	return result;
}

CharacterAnim* createCharacterAnim(GameState* gameState, CharacterAnim** anim = NULL) {
	CharacterAnim* data = gameState->characterAnims + gameState->characterAnimsCount++;
	assert(gameState->characterAnimsCount < MAX_CHARACTER_ANIMS);
	if(anim) *anim = data;
	return data;
}

#endif

void setColor(RenderGroup* group, Color color) {
	GLfloat r = (GLfloat)((color.r + 0.5) / 255.0);
	GLfloat g = (GLfloat)((color.g + 0.5) / 255.0);
	GLfloat b = (GLfloat)((color.b + 0.5) / 255.0);
	GLfloat a = (GLfloat)((color.a + 0.5) / 255.0);


	glUniform4f(group->currentShader->tintUniformLocation, r, g, b, a);
}

SDL_Rect getPixelSpaceRect(double pixelsPerMeter, s32 windowHeight, R2 rect) {
	SDL_Rect result = {};

	double width = getRectWidth(rect);
	double height = getRectHeight(rect);

	result.w = (s32)ceil(width * pixelsPerMeter);
	result.h = (s32)ceil(height * pixelsPerMeter);
	result.x = (s32)ceil(rect.max.x * pixelsPerMeter) - result.w;
	result.y = windowHeight - (s32)ceil(rect.max.y * pixelsPerMeter);

	return result;
}

void bindShader(RenderGroup* group, Shader* shader) {
	if(group->currentShader != shader) {
		group->currentShader = shader;
		glUseProgram(shader->program);
	}
}

void setClipRect(double pixelsPerMeter, R2 rect) {
	GLint x = (GLint)(rect.min.x * pixelsPerMeter);
	GLint y = (GLint)(rect.min.y * pixelsPerMeter);
	GLsizei width = (GLsizei)(getRectWidth(rect) * pixelsPerMeter);
	GLsizei height = (GLsizei)(getRectHeight(rect) * pixelsPerMeter);

	glEnable(GL_SCISSOR_TEST);
	glScissor(x, y, width, height);
}

Texture createText(RenderGroup* group, TTF_Font* font, char* msg) {
	SDL_Color fontColor = {0, 0, 0, 255};
	SDL_Surface* fontSurface = TTF_RenderText_Blended(font, msg, fontColor);

	Texture result = createTexFromSurface(fontSurface, group, false);

	return result;
}

Glyph* getGlyph(CachedFont* cachedFont, RenderGroup* group, char c, double metersPerPixel) {
	if (!cachedFont->cache[c].tex.texId) {	
		SDL_Color white = {255, 255, 255, 255};

		SDL_Surface* glyphSurface = TTF_RenderGlyph_Blended(cachedFont->font, c, white);
		assert(glyphSurface);

		SDL_PixelFormat* format = glyphSurface->format;
		s32 maxX = 0, minX = glyphSurface->w - 1, maxY = 0, minY = glyphSurface->h - 1;

		for(s32 y = 0; y < glyphSurface->h; y++) {
			for(s32 x = 0; x < glyphSurface->w; x++) {
				void* pixelPtr = (u8*)glyphSurface->pixels + x * format->BytesPerPixel + y * glyphSurface->pitch; 
				assert(format->BytesPerPixel == 4);

				u32 pixel = *(u32*)pixelPtr;
				u32 alpha = pixel & format-> Amask;

				if(alpha) {
					if(x > maxX) maxX = x;
					if (x < minX) minX = x;
					if(y > maxY) maxY = y;
					if(y < minY) minY = y;
				}
			}
		}

		cachedFont->cache[c].padding.max.y = minY * metersPerPixel;
		cachedFont->cache[c].padding.min.y = ((glyphSurface->h - 1) * metersPerPixel) - (maxY * metersPerPixel);
		cachedFont->cache[c].padding.max.x = minX * metersPerPixel;
		cachedFont->cache[c].padding.min.x = ((glyphSurface->w - 1) * metersPerPixel) - (maxX * metersPerPixel);

		cachedFont->cache[c].tex = createTexFromSurface(glyphSurface, group, false);
		assert(cachedFont->cache[c].tex.texId);
	}

	Glyph* result = cachedFont->cache + c;
	return result;
}

double getTextWidth(CachedFont* cachedFont, RenderGroup* group, char* msg) {
	double result = 0;

	double metersPerPixel = 1.0 / (group->pixelsPerMeter * cachedFont->scaleFactor);
	double invScaleFactor = 1.0 / cachedFont->scaleFactor;

	while(*msg) {
		result += getGlyph(cachedFont, group, *msg, metersPerPixel)->tex.size.x * invScaleFactor;
		msg++;
	}

	return result;
}

double getTextHeight(CachedFont* cachedFont, RenderGroup* group, char* msg) {
	double result = 0;

	double metersPerPixel = 1.0 / (group->pixelsPerMeter * cachedFont->scaleFactor);
	double invScaleFactor = 1.0 / cachedFont->scaleFactor;

	while(*msg) {
		Glyph* glyph = getGlyph(cachedFont, group, *msg, metersPerPixel);
		double glyphHeight = glyph->tex.size.y * invScaleFactor - glyph->padding.min.y - glyph->padding.max.y;
		if(glyphHeight > result) result = glyphHeight;

		msg++;
	}

	return result;
}

V2 getTextSize(CachedFont* cachedFont, RenderGroup* group, char* msg) {
	V2 result = v2(getTextWidth(cachedFont, group, msg), getTextHeight(cachedFont, group, msg));
	return result;
}

R2 getTextPadding(CachedFont* cachedFont, RenderGroup* group, char* msg) {
	R2 result = {v2(-1, -1), v2(-1, -1)};

	double metersPerPixel = 1.0 / (group->pixelsPerMeter * cachedFont->scaleFactor);

	while(char c = *msg++) {
		Glyph* glyph = getGlyph(cachedFont, group, c, metersPerPixel);

		if(result.min.x < 0 || glyph->padding.min.x < result.min.x) result.min.x = glyph->padding.min.x;
		if(result.max.x < 0 || glyph->padding.max.x < result.max.x) result.max.x = glyph->padding.max.x;
		if(result.min.y < 0 || glyph->padding.min.y < result.min.y) result.min.y = glyph->padding.min.y;
		if(result.max.y < 0 || glyph->padding.max.y < result.max.y) result.max.y = glyph->padding.max.y;
	}

	if(result.min.x < 0) result.min.x = 0;
	if(result.max.x < 0) result.max.x = 0;
	if(result.min.y < 0) result.min.y = 0;
	if(result.max.y < 0) result.max.y = 0;

	return result;
}

bool validTexture(Texture* texture) {
	bool result = texture && texture->texId;
	return result;
}

RenderGroup* createRenderGroup(size_t size, MemoryArena* arena, double pixelsPerMeter, s32 windowWidth, s32 windowHeight, 
								Camera* camera, Texture* textures, s32* texturesCount) {
	RenderGroup* result = pushStruct(arena, RenderGroup);

	result->allocated = 0;
	result->numSortPtrs = 0;
	result->sortAddressCutoff = 0;
	result->maxSize = size;
	result->base = pushSize(arena, size);
	result->pixelsPerMeter = pixelsPerMeter;
	result->windowWidth = windowWidth;
	result->windowHeight = windowHeight;

	V2 windowSize = v2(windowWidth, windowHeight) * (1.0 / pixelsPerMeter);

	result->windowBounds = r2(v2(0, 0), windowSize);
	result->camera = camera;
	result->textures = textures;
	result->texturesCount = texturesCount;
	result->forwardShader = createForwardShader("shaders/forward.vert", "shaders/forward.frag", windowSize);

	//TODO: This compiles basic.vert twice (maybe it can be re-used for both programs)
	result->basicShader = createShader("shaders/basic.vert", "shaders/basic.frag", windowSize);
	result->stencilShader = createShader("shaders/basic.vert", "shaders/stencil.frag", windowSize);

	result->defaultClipRect = result->windowBounds;

	result->whiteTex = loadPNGTexture(result, "white", false);

	return result;
}

void sendTexCoord(V2 uvMin, V2 uvMax, s32 orientation) {
	switch(orientation) {
		case Orientation_0: {
			glTexCoord2f((GLfloat)uvMax.x, (GLfloat)uvMax.y); 
		} break;

		case Orientation_90: {
			glTexCoord2f((GLfloat)uvMin.x, (GLfloat)uvMax.y); 
		} break;

		case Orientation_180: {
			glTexCoord2f((GLfloat)uvMin.x, (GLfloat)uvMin.y); 
		} break;

		case Orientation_270: {
			glTexCoord2f((GLfloat)uvMax.x, (GLfloat)uvMin.y); 
		} break;

		InvalidDefaultCase;		
	}
}

void bindTexture(Texture* texture, RenderGroup* group) {
	assert(validTexture(texture));

	glEnable(GL_TEXTURE_2D);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, texture->texId);
}

void setAmbient(RenderGroup* group, GLfloat ambient) {
	glUniform3f(group->forwardShader.ambientUniform, ambient, ambient, ambient);
}

void drawTexture(RenderGroup* group, Texture* texture, R2 bounds, bool flipX, bool flipY, Orientation orientation, float emissivity, 
				GLfloat ambient, Color color) {
	if(emissivity) {
		setAmbient(group, ambient + (1 - ambient) * emissivity);
	}

	bindTexture(texture, group);

	setColor(group, color);

	V2 uvMin = texture->uv.min;
	V2 uvMax = texture->uv.max;

	if (!flipX) {
		swap(uvMin.x, uvMax.x);
	}

	if(flipY) {
		swap(uvMin.y, uvMax.y);
	}

	glBegin(GL_QUADS);
		sendTexCoord(uvMin, uvMax, orientation);
		glVertex2f((GLfloat)bounds.min.x, (GLfloat)bounds.min.y);

		sendTexCoord(uvMin, uvMax, (orientation + 1) % Orientation_count); 
		glVertex2f((GLfloat)bounds.max.x, (GLfloat)bounds.min.y);

		sendTexCoord(uvMin, uvMax, (orientation + 2) % Orientation_count); 
		glVertex2f((GLfloat)bounds.max.x, (GLfloat)bounds.max.y);

		sendTexCoord(uvMin, uvMax, (orientation + 3) % Orientation_count); 
		glVertex2f((GLfloat)bounds.min.x, (GLfloat)bounds.max.y);
	glEnd();

	if(emissivity) {
		setAmbient(group, ambient);
	}
}

void drawTexture(RenderGroup* group, Texture* texture, R2 bounds, double rot, Color color, bool flipX, bool flipY, float emissivity,
				 GLfloat ambient) {
	if(emissivity) {
		setAmbient(group, ambient + (1 - ambient) * emissivity);
	}

	bindTexture(texture, group);
	setColor(group, color);

	V2 uvMin = texture->uv.min;
	V2 uvMax = texture->uv.max;

	if (!flipX) {
		swap(uvMin.x, uvMax.x);
	}

	if(flipY) {
		swap(uvMin.y, uvMax.y);
	}

	V2 center = getRectCenter(bounds);

	V2 originRelativeMin = bounds.min - center;
	V2 originRelativeMax = bounds.max - center;

	V2 originRelativeP2 = v2(originRelativeMax.x, originRelativeMin.y);
	V2 originRelativeP3 = v2(originRelativeMin.x, originRelativeMax.y);

	V2 p[4];

	p[0] = rotate(originRelativeMin, rot) + center;
	p[1] = rotate(originRelativeP2, rot) + center;
	p[2] = rotate(originRelativeMax, rot) + center;
	p[3] = rotate(originRelativeP3, rot) + center;

	glBegin(GL_QUADS);
		for(s32 pIndex = 0; pIndex < arrayCount(p); pIndex++) {
			sendTexCoord(uvMin, uvMax, pIndex);
			glVertex2f((GLfloat)p[pIndex].x, (GLfloat)p[pIndex].y);
		}
	glEnd();

	if(emissivity) {
		setAmbient(group, ambient);
	}
}

void drawFilledStencil(RenderGroup* group, Texture* stencil, R2 bounds, double widthPercentage, Color color) {
	double uvWidth = getRectWidth(stencil->uv);
	uvWidth *= widthPercentage;

	double uMax = stencil->uv.min.x + uvWidth;

	V2 uvMax = v2(uMax, stencil->uv.max.y);
	V2 uvMin = stencil->uv.min;

	Shader* oldShader = group->currentShader;
	bindShader(group, &group->stencilShader);

	bindTexture(stencil, group);
	setColor(group, color);

	glBegin(GL_QUADS);
		glTexCoord2f((GLfloat)uvMin.x, (GLfloat)uvMax.y);
		glVertex2f((GLfloat)bounds.min.x, (GLfloat)bounds.min.y);

		glTexCoord2f((GLfloat)uvMax.x, (GLfloat)uvMax.y);
		glVertex2f((GLfloat)bounds.max.x, (GLfloat)bounds.min.y);

		glTexCoord2f((GLfloat)uvMax.x, (GLfloat)uvMin.y);
		glVertex2f((GLfloat)bounds.max.x, (GLfloat)bounds.max.y);

		glTexCoord2f((GLfloat)uvMin.x, (GLfloat)uvMin.y);
		glVertex2f((GLfloat)bounds.min.x, (GLfloat)bounds.max.y);
	glEnd();

	bindShader(group, oldShader);
}

void drawText(RenderGroup* group, CachedFont* cachedFont, char* msg, V2 p, Color color) {
	double metersPerPixel = 1.0 / (group->pixelsPerMeter * cachedFont->scaleFactor);
	double invScaleFactor = 1.0 / cachedFont->scaleFactor;

	while(*msg) {
		Texture* texture = &getGlyph(cachedFont, group, *msg, metersPerPixel)->tex;
		V2 size = texture->size * invScaleFactor;
		R2 bounds = r2(p, p + size);

		drawTexture(group, texture, bounds, false, false, Orientation_0, 0, 1, color);

		p.x += size.x;
		msg++;
	}
}

void drawFillRect(RenderGroup* group, R2 bounds, Color color) {
	// SDL_Rect dstRect = getPixelSpaceRect(group->pixelsPerMeter, group->windowHeight, rect);
	// SDL_RenderFillRect(group->renderer, &dstRect);

	bindTexture(group->whiteTex, group);
	setColor(group, color);
			
	glBegin(GL_QUADS);
		glTexCoord2f(0, 0);
		glVertex2f((GLfloat)bounds.min.x, (GLfloat)bounds.min.y);

		glTexCoord2f(1, 0);
		glVertex2f((GLfloat)bounds.max.x, (GLfloat)bounds.min.y);

		glTexCoord2f(1, 1);
		glVertex2f((GLfloat)bounds.max.x, (GLfloat)bounds.max.y);

		glTexCoord2f(0, 1);
		glVertex2f((GLfloat)bounds.min.x, (GLfloat)bounds.max.y);
	glEnd();
}

void drawFillQuad(RenderGroup* group, V2 p1, V2 p2, V2 p3, V2 p4) {
	bindTexture(group->whiteTex, group);

	glBegin(GL_QUADS);
		glTexCoord2f(0, 0);
		glVertex2f((GLfloat)p1.x, (GLfloat)p1.y);

		glTexCoord2f(1, 0);
		glVertex2f((GLfloat)p2.x, (GLfloat)p2.y);

		glTexCoord2f(1, 1);
		glVertex2f((GLfloat)p3.x, (GLfloat)p3.y);

		glTexCoord2f(0, 1);
		glVertex2f((GLfloat)p4.x, (GLfloat)p4.y);
	glEnd();
}

void drawOutlinedRect(RenderGroup* group, R2 rect, Color color, double thickness) {
	V2 halfThickness = v2(thickness, thickness) * 0.5;
	double width = getRectWidth(rect);
	double height = getRectHeight(rect);

	V2 bottomMinCorner = rect.min - halfThickness;
	V2 bottomMaxCorner = v2(rect.max.x, rect.min.y) + halfThickness;

	V2 topMinCorner = v2(rect.min.x, rect.max.y) - halfThickness;
	V2 topMaxCorner = rect.max + halfThickness;

	V2 leftMinCorner = bottomMinCorner + v2(0, thickness);
	V2 leftMaxCorner = topMinCorner + v2(thickness, 0);

	V2 rightMinCorner = bottomMaxCorner - v2(thickness, 0);
	V2 rightMaxCorner = topMaxCorner - v2(0, thickness);

	drawFillRect(group, r2(bottomMinCorner, bottomMaxCorner), color);
	drawFillRect(group, r2(topMinCorner, topMaxCorner), color);
	drawFillRect(group, r2(leftMinCorner, leftMaxCorner), color);
	drawFillRect(group, r2(rightMinCorner, rightMaxCorner), color);
}

void drawDashedLine(RenderGroup* group, Color color, V2 lineStart, V2 lineEnd,
					 double thickness, double dashSize, double spaceSize) {
	setColor(group, color);
	if((lineStart.x > lineEnd.x) || (lineStart.x == lineEnd.x && lineStart.y > lineEnd.y)) {
		swap(lineStart, lineEnd);
	}

	V2 dir = normalize(lineEnd - lineStart);
	V2 normal = perp(dir) * thickness;

	V2 dashSkip = dashSize * dir;
	V2 spaceSkip = spaceSize * dir;

	V2 start = lineStart;

	while(true) {
		bool doneDrawing = false;
		V2 end = start + dashSkip;

		if(end.x > lineEnd.x || (lineStart.x == lineEnd.x && end.y > lineEnd.y)) {
			doneDrawing = true;
			end = lineEnd;
		}

		if((dashSkip.y > 0 && end.y > lineEnd.y) || (dashSkip.y < 0 && end.y < lineEnd.y)) {
			doneDrawing = true;
			end = lineEnd;
		}

		V2 p1 = start + normal;
		V2 p2 = start - normal;
		V2 p3 = end - normal;
		V2 p4 = end + normal;

		drawFillQuad(group, p1, p2, p3, p4);

		if(doneDrawing) break;

		start = end + spaceSkip;
	}
}

#ifdef HACKFORMER_GAME
void renderDrawConsoleField(RenderGroup* group, FieldSpec* fieldSpec, ConsoleField* field) {
	field->p -= group->camera->p;
	double alpha = field->alpha;
	field->alpha = 1;
	drawConsoleField(field, group, NULL, fieldSpec, false, true, NULL);
	field->alpha = alpha;
	field->p += group->camera->p;
}
#endif

DrawType getRenderHeaderType(RenderHeader* header) {
	DrawType result = (DrawType)(header->type_ & RENDER_HEADER_TYPE_MASK);
	return result;
}

bool32 renderElemClipRect(RenderHeader* header) {
	bool32 result = header->type_ & RENDER_HEADER_CLIP_RECT_FLAG;
	return result;
}

void setRenderElemClipRect(RenderHeader* header) {
	header->type_ |= RENDER_HEADER_CLIP_RECT_FLAG;
}

#define pushRenderElement(group, type) (type*)pushRenderElement_(group, DrawType_##type, sizeof(type))
void* pushRenderElement_(RenderGroup* group, DrawType type, size_t size) {
	size_t headerBytes = sizeof(RenderHeader);

	if (group->hasClipRect) {
		headerBytes += sizeof(R2);
	}

	size += headerBytes;

	void* result = NULL;

	if(group->enabled) {
		if (group->allocated + size < group->maxSize) {
				result = (char*)group->base + group->allocated;
				group->allocated += size;

				RenderHeader* header = (RenderHeader*)result;
				header->type_ = type;

				if(group->hasClipRect) {
					setRenderElemClipRect(header);
					R2* clipRectPtr = (R2*)((char*)result + sizeof(RenderHeader));
					*clipRectPtr = group->clipRect;
				} 

				if(!group->sortAddressCutoff) {
					assert(group->numSortPtrs + 1 < arrayCount(group->sortPtrs));
					group->sortPtrs[group->numSortPtrs++] = (RenderHeader*)result;
				}

				result = (char*)result + headerBytes;
		} else {
			InvalidCodePath;
		}
	}

	return result;
}

void pushSortEnd(RenderGroup* group) {
	group->sortAddressCutoff = group->allocated;
}

RenderTexture createRenderTexture(DrawOrder drawOrder, Texture* texture, bool flipX, bool flipY,
								Orientation orientation, float emissivity, Color color) {
	RenderTexture result = {};

	assert(texture->texId);

	result.drawOrder = drawOrder;
	result.texture = texture;
	result.flipX = flipX;
	result.flipY = flipY;
	result.orientation = orientation;
	result.emissivity = emissivity;
	result.color = color;

	return result;
}


#ifdef HACKFORMER_GAME
//TODO: Cull console fields which are not currently visible
void pushConsoleField(RenderGroup* group, FieldSpec* fieldSpec, ConsoleField* field) {
	assert(field);

	if(group->rendering) {
		renderDrawConsoleField(group, fieldSpec, field);
	} else {
		RenderConsoleField* render = pushRenderElement(group, RenderConsoleField);

		if(render) {
			render->field = field;
		}
	}
} 
#endif

void pushFilledStencil(RenderGroup* group, Texture* stencil, R2 bounds, double widthPercentage, Color color) {
	assert(validTexture(stencil));
	assert(widthPercentage >= 0 && widthPercentage <= 1);

	bounds = scaleRect(bounds, v2(1, 1) * group->camera->scale);
	bounds.max.x = bounds.min.x + getRectWidth(bounds) * widthPercentage;

	if(rectanglesOverlap(group->windowBounds, bounds)) {
		if(group->rendering) {
			drawFilledStencil(group, stencil, bounds, widthPercentage, color);
		} else {
			RenderFilledStencil* render = pushRenderElement(group, RenderFilledStencil);

			if (render) {
				render->stencil = stencil;
				render->bounds = bounds;
				render->widthPercentage = widthPercentage;
				render->color = color;
			}
		}
	}
}

void pushTexture(RenderGroup* group, Texture* texture, R2 bounds, double rotation, bool flipX = false, bool flipY = false, 
	DrawOrder drawOrder = DrawOrder_gui, bool moveIntoCameraSpace = false, Color color = WHITE, float emissivity = 0) {

	assert(validTexture(texture));

	R2 drawBounds = bounds;
	if(moveIntoCameraSpace) drawBounds = translateRect(bounds, -group->camera->p);
	drawBounds = scaleRect(drawBounds, v2(1, 1) * group->camera->scale);

	R2 clipBounds = drawBounds;

	if(rotation) {
		V2 halfSize = getRectSize(clipBounds) * 0.5;
		double size = length(halfSize);

		clipBounds = rectCenterDiameter(getRectCenter(clipBounds), v2(size, size));
	}

	if(rectanglesOverlap(group->windowBounds, clipBounds)) {
		if(group->rendering) {
			if(rotation) {
				drawTexture(group, texture, drawBounds, rotation, color, flipX, flipY, emissivity, group->ambient);
			} else {
				drawTexture(group, texture, drawBounds, flipX, flipY, Orientation_0, emissivity, group->ambient, color);
			}
		} else {
			if(rotation) {
				RenderRotatedTexture* render = pushRenderElement(group, RenderRotatedTexture);

				if (render) {
					render->tex = createRenderTexture(drawOrder, texture, flipX, flipY, Orientation_0, emissivity, color);
					render->bounds = drawBounds;
					render->rad = rotation;
				}
			} else {
				RenderBoundedTexture* render = pushRenderElement(group, RenderBoundedTexture);

				if (render) {
					render->tex = createRenderTexture(drawOrder, texture, flipX, flipY, Orientation_0, emissivity, color);
					render->bounds = drawBounds;
				}
			}
		}
	}
}

void pushTexture(RenderGroup* group, Texture* texture, R2 bounds, bool flipX, bool flipY, DrawOrder drawOrder, bool moveIntoCameraSpace,
	 		     Orientation orientation, Color color, float emissivity) {

	assert(validTexture(texture));

	R2 drawBounds = moveIntoCameraSpace ? translateRect(bounds, -group->camera->p) : bounds;
	drawBounds = scaleRect(drawBounds, v2(1, 1) * group->camera->scale);

	if(rectanglesOverlap(group->windowBounds, drawBounds)) {
		if(group->rendering) {
			drawTexture(group, texture, drawBounds, flipX, flipY, orientation, emissivity, group->ambient, color);
		} else {
			RenderBoundedTexture* render = pushRenderElement(group, RenderBoundedTexture);

			if (render) {
				render->tex = createRenderTexture(drawOrder, texture, flipX, flipY, orientation, emissivity, color);
				render->bounds = drawBounds;
			}
		}
	}
}

void pushDashedLine(RenderGroup* group, Color color, V2 lineStart, V2 lineEnd, double thickness,
					 double dashSize, double spaceSize, bool moveIntoCameraSpace = false) {
	if(lineStart == lineEnd) return;

	V2 lineCenter = (lineStart + lineEnd) * 0.5;
	lineStart = lineCenter + (lineStart - lineCenter) * group->camera->scale;
	lineEnd = lineCenter + (lineEnd - lineCenter) * group->camera->scale;

	if(moveIntoCameraSpace) {
		lineStart -= group->camera->p;
		lineEnd -= group->camera->p;
	}

	//TODO: No need to push lines on if they aren't visible

	if(group->rendering) {
		drawDashedLine(group, color, lineStart, lineEnd, thickness, dashSize, spaceSize);
	} else {
		RenderDashedLine* render = pushRenderElement(group, RenderDashedLine);

		if (render) {
			render->color = color;
			render->lineStart = lineStart;
			render->lineEnd = lineEnd;
			render->thickness = thickness;
			render->dashSize = dashSize;
			render->spaceSize = spaceSize;
		}
	}
}

#ifdef HACKFORMER_GAME
void pushEntityTexture(RenderGroup* group, Texture* texture, Entity* entity, bool flipX, bool flipY, DrawOrder drawOrder,
					Orientation orientation = Orientation_0, Color color = WHITE) {
	assert(texture);

	R2 drawBounds = translateRect(rectCenterDiameter(entity->p, entity->renderSize), -group->camera->p);
	drawBounds = scaleRect(drawBounds, v2(1, 1) * group->camera->scale);

	R2 clipBounds = scaleRect(drawBounds, v2(1, 1) * 1.05);

	color.a = (u8)(color.a * entity->alpha);

	double rotation = entity->rotation;

	if(rotation) {
		V2 halfSize = getRectSize(clipBounds) * 0.5;
		double size = length(halfSize);

		clipBounds = rectCenterDiameter(getRectCenter(clipBounds), v2(size, size));
	}

	if(rectanglesOverlap(group->windowBounds, clipBounds)) {
		if(group->rendering) {
			if(rotation) {
				drawTexture(group, texture, drawBounds, rotation, color, flipX, flipY, entity->emissivity, group->ambient);
			} else {
				drawTexture(group, texture, drawBounds, flipX, flipY, orientation, entity->emissivity, group->ambient, color);
			}
		} else {
			RenderEntityTexture* render = pushRenderElement(group, RenderEntityTexture);

			if (render) {
				render->tex = createRenderTexture(drawOrder, texture, flipX, flipY, orientation, entity->emissivity, color);
				render->p = &entity->p;
				render->renderSize = &entity->renderSize;
				render->rotation = rotation;
			}
		}
	}
}

#endif

void pushText(RenderGroup* group, CachedFont* font, char* msg, V2 p, Color color = BLACK,
			  TextAlignment alignment = TextAlignment_bottomLeft) {

	switch(alignment) {
		case TextAlignment_center: {
			V2 strSize = getTextSize(font, group, msg);
			p -= v2(strSize.x, strSize.y) * 0.5;
		} break;

		case TextAlignment_bottomLeft:
			break;

		InvalidDefaultCase;
	}


	if(group->rendering) {
		drawText(group, font, msg, p, color);
	} else {
		RenderText* render = pushRenderElement(group, RenderText);

		if (render) {
			assert(strlen(msg) < arrayCount(render->msg) - 1);

			char* dstPtr = (char*)render->msg;

			while(*msg) {
				*dstPtr++ = *msg++;
			}
			*dstPtr = 0;

			render->p = p;
			render->font = font;
			render->color = color;
		}
	}
}

void pushFilledRect(RenderGroup* group, R2 bounds, Color color, bool moveIntoCameraSpace = false) {
	R2 drawBounds = moveIntoCameraSpace ? translateRect(bounds, -group->camera->p) : bounds;
	drawBounds = scaleRect(drawBounds, v2(1, 1) * group->camera->scale);

	if(rectanglesOverlap(group->windowBounds, drawBounds)) {
		if(group->rendering) {
			drawFillRect(group, drawBounds, color);
		} else {
			RenderFillRect* render = pushRenderElement(group, RenderFillRect);

			if (render) {
				render->color = color;
				render->bounds = drawBounds;
			}
		}
	}
}

void pushOutlinedRect(RenderGroup* group, R2 bounds, double thickness, Color color, bool moveIntoCameraSpace = false) {
	R2 drawBounds = moveIntoCameraSpace ? translateRect(bounds, -group->camera->p) : bounds;
	drawBounds = scaleRect(drawBounds, v2(1, 1) * group->camera->scale);

	if(rectanglesOverlap(group->windowBounds, addDiameterTo(drawBounds, v2(1, 1) * thickness))) {
		if(group->rendering) {
			drawOutlinedRect(group, drawBounds, color, thickness);
		} else {

			RenderOutlinedRect* render = pushRenderElement(group, RenderOutlinedRect);

			if (render) {
				render->thickness = thickness;
				render->color = color;
				render->bounds = drawBounds;
			}
		}
	}
}

PointLight createPointLight(V3 p, V3 color, double range) {
	PointLight result = {};

	result.p = p;
	result.color = color;
	result.range = range;

	return result;
}

SpotLight createSpotLight(V2 p, V3 color, double range, double angle, double spread) {
	SpotLight result = {};

	result.base = createPointLight(v3(p, 0), color, range);
	result.angle = angle;
	result.spread = spread;

	return result;
}

PointLight getRenderablePointLight(PointLight* light, bool moveIntoCameraSpace, V2 negativeCameraP) {
	assert(light->range > 0);

	PointLight result = *light;
	if (moveIntoCameraSpace) result.p += v3(negativeCameraP, 0);

	return result;
}

void pushPointLight(RenderGroup* group, PointLight* pl, bool moveIntoCameraSpace = false) {
	if(group->enabled) {
		if(group->forwardShader.numPointLights + 1 < arrayCount(group->forwardShader.pointLights)) {
			PointLight* light = group->forwardShader.pointLights + group->forwardShader.numPointLights;
			group->forwardShader.numPointLights++;

			*light = getRenderablePointLight(pl, moveIntoCameraSpace, -group->camera->p);
		} else {
			InvalidCodePath;
		}
	}
}

void pushSpotLight(RenderGroup* group, SpotLight* sl, bool moveIntoCameraSpace = false) {
	if(group->enabled) {
		if(group->forwardShader.numSpotLights + 1 < arrayCount(group->forwardShader.spotLights)) {
			SpotLight* light = group->forwardShader.spotLights + group->forwardShader.numSpotLights;
			group->forwardShader.numSpotLights++;

			light->base = getRenderablePointLight(&sl->base, moveIntoCameraSpace, -group->camera->p);
			light->angle = sl->angle;
			light->spread = sl->spread;
		} else {
			InvalidCodePath;
		}
	}
}


void pushClipRect(RenderGroup* group, R2 clipRect) {
	group->clipRect = clipRect;
	group->hasClipRect = true;
}

void pushDefaultClipRect(RenderGroup* group) {
	group->hasClipRect = false;
}

size_t drawRenderElem(RenderGroup* group, FieldSpec* fieldSpec, void* elemPtr, GLfloat ambient, bool pastSortEnd) {
	RenderHeader* header = (RenderHeader*)elemPtr;
	size_t elemSize = sizeof(RenderHeader);

	elemPtr = (char*)elemPtr + sizeof(RenderHeader);

	if (renderElemClipRect(header)) {
		R2* clipBounds = (R2*)elemPtr;
		elemPtr = (char*)elemPtr + sizeof(R2);
		elemSize += sizeof(R2);
		setClipRect(group->pixelsPerMeter, *clipBounds);
	} else if(pastSortEnd) {
		glDisable(GL_SCISSOR_TEST);
	} else {
		setClipRect(group->pixelsPerMeter, group->defaultClipRect);
	}

	#define START_CASE(type) case DrawType_##type: { type* render = (type*)elemPtr; elemSize += sizeof(type);
	#define END_CASE } break

	switch(getRenderHeaderType(header)) {

		START_CASE(RenderBoundedTexture);
			drawTexture(group, render->tex.texture, render->bounds, render->tex.flipX != 0, render->tex.flipY != 0, 
						render->tex.orientation, render->tex.emissivity, ambient, render->tex.color);
		END_CASE;

		START_CASE(RenderEntityTexture);
			R2 bounds = rectCenterDiameter(*render->p - group->camera->p, *render->renderSize);

			if(render->rotation) {
				drawTexture(group, render->tex.texture, bounds, render->rotation, render->tex.color, render->tex.flipX != 0, 
							render->tex.flipY != 0, render->tex.emissivity, group->ambient);
			} else {
				drawTexture(group, render->tex.texture, bounds, render->tex.flipX != 0, render->tex.flipY != 0,
							render->tex.orientation, render->tex.emissivity, ambient, render->tex.color);
			}
		END_CASE;

		START_CASE(RenderText);
			drawText(group, render->font, render->msg, render->p, render->color);
		END_CASE;

		START_CASE(RenderFillRect);
			drawFillRect(group, render->bounds, render->color);
		END_CASE;

		START_CASE(RenderOutlinedRect);
			drawOutlinedRect(group, render->bounds, render->color, render->thickness);
		END_CASE;

		#ifdef HACKFORMER_GAME
		START_CASE(RenderConsoleField);
			renderDrawConsoleField(group, fieldSpec, render->field);
		END_CASE;
		#endif

		START_CASE(RenderDashedLine);
			drawDashedLine(group, render->color, render->lineStart, render->lineEnd, render->thickness, render->dashSize, render->spaceSize);
		END_CASE;

		START_CASE(RenderRotatedTexture);
			drawTexture(group, render->tex.texture, render->bounds, render->rad, render->tex.color,
					    render->tex.flipX != 0, false, render->tex.emissivity, group->ambient);
		END_CASE;

		START_CASE(RenderFilledStencil);
			drawFilledStencil(group, render->stencil, render->bounds, render->widthPercentage, render->color);
		END_CASE;

		InvalidDefaultCase;
	}

	#undef START_CASE
	#undef END_CASE

	return elemSize;
}

bool isRenderTextureType(DrawType type) {
	bool result = (type == DrawType_RenderEntityTexture) || (type == DrawType_RenderBoundedTexture);
	return result;
}

RenderTexture* getRenderTexture(RenderHeader* header) {
	assert(isRenderTextureType(getRenderHeaderType(header)));

	RenderTexture* result = (RenderTexture*)((char*)header + sizeof(RenderHeader));

	if(renderElemClipRect(header)) {
		result = (RenderTexture*)((char*)result + sizeof(R2));
	}

	return result;
}

ConsoleField* getConsoleFieldFromRenderHeader(RenderHeader* header) {
	assert(getRenderHeaderType(header) == DrawType_RenderConsoleField);

	void* render = (char*)header + sizeof(RenderHeader);

	if(renderElemClipRect(header)) {
		render = (char*)render + sizeof(R2);
	}

	ConsoleField* result = ((RenderConsoleField*)render)->field;

	return result;
}

s32 renderElemCompare(const void* a, const void* b) {
	RenderHeader* e1 = *(RenderHeader**)a;
	RenderHeader* e2 = *(RenderHeader**)b;

	DrawType e1Type = getRenderHeaderType(e1);
	DrawType e2Type = getRenderHeaderType(e2);

	bool e1IsRenderTexture = isRenderTextureType(e1Type);
	bool e2IsRenderTexture = isRenderTextureType(e2Type);

	bool e1IsRenderTextureOrConsoleField = e1IsRenderTexture;
	bool e2IsRenderTextureOrConsoleField = e2IsRenderTexture;

	#ifdef HACKFORMER_GAME
	bool e1IsConsoleField = e1Type == DrawType_RenderConsoleField;
	bool e2IsConsoleField = e2Type == DrawType_RenderConsoleField;

	if(e1IsConsoleField && e2IsConsoleField) {
		ConsoleField* f1 = getConsoleFieldFromRenderHeader(e1);
		ConsoleField* f2 = getConsoleFieldFromRenderHeader(e2);

		if(f1->p.y > f2->p.y) return 1;
		if(f1->p.y < f2->p.y) return -1;

		//NOTE: Arbitrary comparisons done to help ensure a consistent render order across frames
		if(f1->p.x > f2->p.x) return 1;
		if(f1->p.x < f2->p.x) return -1;
		return *f1->name > *f2->name;
	}

	e1IsRenderTextureOrConsoleField |= e1IsConsoleField;
	e2IsRenderTextureOrConsoleField |= e2IsConsoleField;
	#endif

	if(e1IsRenderTextureOrConsoleField) {
		if(!e2IsRenderTextureOrConsoleField) return -1;

		RenderTexture* r1 = NULL;
		if(e1IsRenderTexture) r1 = getRenderTexture(e1);
		RenderTexture* r2 = NULL;
		if(e2IsRenderTexture) r2 = getRenderTexture(e2);

		#ifdef HACKFORMER_GAME
		DrawOrder r1DrawOrder = (r1 == NULL ? DrawOrder_pickupField : r1->drawOrder);
		DrawOrder r2DrawOrder = (r2 == NULL ? DrawOrder_pickupField : r2->drawOrder);
		#else 
		DrawOrder r1DrawOrder = r1->drawOrder;
		DrawOrder r2DrawOrder = r2->drawOrder;
		#endif

		if(r1DrawOrder > r2DrawOrder) return 1;
		if(r1DrawOrder < r2DrawOrder) return -1;

		assert(r1);
		assert(r2);

		double r1MinY, r2MinY;

		if(e1Type == DrawType_RenderEntityTexture) {
			RenderEntityTexture* entityTex = (RenderEntityTexture*)r1;
			r1MinY = entityTex->p->y - entityTex->p->y / 2;
		} else {
			RenderBoundedTexture* boundedTex = (RenderBoundedTexture*)r1;
			r1MinY = boundedTex->bounds.min.y;
		}

		if(e2Type == DrawType_RenderEntityTexture) {
			RenderEntityTexture* entityTex = (RenderEntityTexture*)r2;
			r2MinY = entityTex->p->y - entityTex->p->y / 2;
		} else {
			RenderBoundedTexture* boundedTex = (RenderBoundedTexture*)r2;
			r2MinY = boundedTex->bounds.min.y;
		}

		if(r1MinY > r2MinY) return 1;
		if(r1MinY < r2MinY) return -1;

		//NOTE: This comparison is done to ensure that entity which is placed on top is consistent across frames
		//		For example, if two tiles are overlapping and have exactly the same position and draw order, we 
		//		always want the same one to be on top.
		return r1->texture > r2->texture;
	}

	if (e2IsRenderTextureOrConsoleField) return 1;

	return 0;
}

bool isPointLightVisible(RenderGroup* group, PointLight* light) {
	double colorDepth = 1.0 / 256.0;

	R2 maxLightBounds = addRadiusTo(group->windowBounds, v2(1, 1) * light->range);

	//TODO: Account for z
	bool result = pointInsideRect(maxLightBounds, light->p.xy);
	return result;
}

void setPointLightUniforms(PointLightUniforms* lightUniforms, PointLight* light) {
	glUniform3f(lightUniforms->p, (GLfloat)light->p.x, (GLfloat)light->p.y, (GLfloat)light->p.z);
	glUniform3f(lightUniforms->color, (GLfloat)light->color.x, (GLfloat)light->color.y, (GLfloat)light->color.z);
	glUniform1f(lightUniforms->range, (GLfloat)light->range);
}

void drawRenderGroup(RenderGroup* group, FieldSpec* fieldSpec) {
	group->rendering = true;
	qsort(group->sortPtrs, group->numSortPtrs, sizeof(RenderHeader*), renderElemCompare);
	bindShader(group, &group->forwardShader.shader);

#if ENABLE_LIGHTING
	group->ambient = (GLfloat)0.35;//0.43;
#else
	group->ambient = (GLfloat)1;
#endif

	setAmbient(group, group->ambient);

	s32 uniformPointLightIndex = 0;
	s32 uniformSpotLightIndex = 0;

	PointLight defaultPointLight = {};

	for(s32 lightIndex = 0; lightIndex < group->forwardShader.numPointLights; lightIndex++) {
		PointLight* light = group->forwardShader.pointLights + lightIndex;

		//TODO: account for z
		//TODO: handle case where we run out of lights in a nicer way
		if(isPointLightVisible(group, light)) {
			assert(uniformPointLightIndex < arrayCount(group->forwardShader.pointLightUniforms));
			PointLightUniforms* lightUniforms = group->forwardShader.pointLightUniforms + uniformPointLightIndex;
			uniformPointLightIndex++;
			setPointLightUniforms(lightUniforms, light);
		}
	}

	for(s32 uniformIndex = uniformPointLightIndex; uniformIndex < arrayCount(group->forwardShader.pointLightUniforms); uniformIndex++) {
		PointLightUniforms* lightUniforms = group->forwardShader.pointLightUniforms + uniformIndex;
		setPointLightUniforms(lightUniforms, &defaultPointLight);
	}

	for(s32 lightIndex = 0; lightIndex < group->forwardShader.numSpotLights; lightIndex++) {
		SpotLight* light = group->forwardShader.spotLights + lightIndex;

		//TODO: account for z
		//TODO: handle case where we run out of lights in a nicer way
		if(isPointLightVisible(group, &light->base)) {
			assert(uniformSpotLightIndex < arrayCount(group->forwardShader.spotLightUniforms));
			SpotLightUniforms* lightUniforms = group->forwardShader.spotLightUniforms + uniformSpotLightIndex;
			uniformSpotLightIndex++;
			setPointLightUniforms(&lightUniforms->base, &light->base);

			double angle = light->angle;
			V2 lightDir = v2(cos(angle), sin(angle));
			double cutoff = cos(toRadians(light->spread / 2));

			glUniform2f(lightUniforms->dir, (GLfloat)lightDir.x, (GLfloat)lightDir.y);
			glUniform1f(lightUniforms->cutoff, (GLfloat)cutoff);
		}
	}

	for(s32 uniformIndex = uniformSpotLightIndex; uniformIndex < arrayCount(group->forwardShader.spotLightUniforms); uniformIndex++) {
		SpotLightUniforms* lightUniforms = group->forwardShader.spotLightUniforms + uniformIndex;
		setPointLightUniforms(&lightUniforms->base, &defaultPointLight);
	}

	for(s32 elemIndex = 0; elemIndex < group->numSortPtrs; elemIndex++) {
		void* elemPtr = group->sortPtrs[elemIndex];
		drawRenderElem(group, fieldSpec, elemPtr, group->ambient, false);
	}

	u32 groupByteIndex = group->sortAddressCutoff;

	bindShader(group, &group->basicShader);

	while(groupByteIndex < group->allocated) {
		void* elemPtr = (char*)group->base + groupByteIndex;
		groupByteIndex += drawRenderElem(group, fieldSpec, elemPtr, group->ambient, true);
	}

	group->numSortPtrs = 0;
	group->allocated = 0;
	group->sortAddressCutoff = 0;
	group->forwardShader.numPointLights = 0;
	group->forwardShader.numSpotLights = 0;
	pushDefaultClipRect(group);
	group->rendering = false;
}
