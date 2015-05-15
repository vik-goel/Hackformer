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

	result.normalXFlipUniform = glGetUniformLocation(result.shader.program, "normalXFlip");

	GLint normalLocation = glGetUniformLocation(result.shader.program, "normalTexture");
	glUniform1i(normalLocation, 1);

	return result;
}

void freeTexture(Texture texture) {
	glDeleteTextures(1, &texture.texId);
	texture.texId = 0;

	//TODO: Free the normal map if it is not the null normal

	//SDL_DestroyTexture(texture.tex);
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

		// char* pixelPtr = (char*)image->pixels;

		// for(s32 y = 0; y < image->h; y++) {
		// 	for(s32 x = 0; x < image->w; x++) {
		// 		u32* pixel = (u32*)(pixelPtr + x * 4);

		// 		s32 r = (*pixel >> 24) & 0xFF;
		// 		s32 g = (*pixel >> 16) & 0xFF;
		// 		s32 b = (*pixel >> 8) & 0xFF;
		// 		s32 a = (*pixel) & 0xFF;

		// 		float aFloat = (a + 0.5f) / 255.0f;
		// 		float rFloat = ((r + 0.5f) / 255.0f) * aFloat;
		// 		float gFloat = ((g + 0.5f) / 255.0f) * aFloat;
		// 		float bFloat = ((b + 0.5f) / 255.0f) * aFloat;

		// 		s32 newR = (s32)(rFloat * 255.5f);
		// 		s32 newG = (s32)(gFloat * 255.5f);
		// 		s32 newB = (s32)(bFloat * 255.5f);

		// 		*pixel = (newR << 24) | (newG << 16) | (newB << 8) | a;
		// 	}

		// 	pixelPtr += image->pitch;
		//}

	} else {
		InvalidCodePath;
	}

	GLint internalFormal = srgb ? GL_SRGB8_ALPHA8 : GL_RGBA;

	glTexImage2D(GL_TEXTURE_2D, 0, internalFormal, image->w, image->h, 0, format, GL_UNSIGNED_BYTE, image->pixels);

	return result;
}

Texture createTexFromSurface(SDL_Surface* image, SDL_Surface* normal, RenderGroup* group) {
	Texture result = {};

	result.texId = generateTexture(image, true);

	if(normal) {
		result.normalId = generateTexture(normal);
		SDL_FreeSurface(normal);
	} else {
		result.normalId = group->nullNormalId;
	}

	result.uv = r2(v2(0, 0), v2(1, 1));
	result.size = v2(image->w, image->h) * (1.0 / group->pixelsPerMeter);

	SDL_FreeSurface(image);
	glBindTexture(GL_TEXTURE_2D, 0);

	return result;
}

void addFileExtension(char* result, size_t resultSize, char* fileName, char* extension) {
	assert(strlen(fileName) + strlen(extension) < resultSize);
	result[0] = 0;
	strcat(result, fileName);
	strcat(result, extension);
}

Texture loadPNGTexture(GameState* gameState, char* fileName, bool loadNormalMap = true) {
	char* extension = ".png";
	
	char filePath[1024];
	addFileExtension(filePath, sizeof(filePath), fileName, ".png");

	SDL_Surface* diffuse = IMG_Load(filePath);
	if (!diffuse) fprintf(stderr, "Failed to load image from: %s\n", filePath);
	assert(diffuse);

	addFileExtension(filePath, sizeof(filePath), fileName, "_NRM.png");

	SDL_Surface* normal = NULL;
	if (loadNormalMap) normal = IMG_Load(filePath);

	Texture result = createTexFromSurface(diffuse, normal, gameState->renderGroup);

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

CachedFont loadCachedFont(GameState* gameState, char* fileName, s32 fontSize, double scaleFactor) {
	CachedFont result = {};
	result.font = loadFont(fileName, (s32)round(fontSize * scaleFactor));
	result.scaleFactor = scaleFactor;
	result.lineHeight = fontSize / (gameState->pixelsPerMeter * scaleFactor);
	return result;
}

Texture* extractTextures(GameState* gameState, char* fileName, 
	s32 frameWidth, s32 frameHeight, s32 frameSpacing, u32* numFrames) {

	Texture tex = loadPNGTexture(gameState, fileName);

	s32 texWidth = (s32)(tex.size.x * gameState->pixelsPerMeter + 0.5);
	s32 texHeight = (s32)(tex.size.y * gameState->pixelsPerMeter + 0.5);

	s32 numCols = texWidth / (frameWidth + frameSpacing);
	s32 numRows = texHeight / (frameHeight + frameSpacing);

	assert(frameWidth > 0);
	assert(frameHeight > 0);

	//TODO: Add a way to test this
	//assert(width % numCols == 0);
	//assert(height % numRows == 0);

	*numFrames = numRows * numCols;
	Texture* result = (Texture*)pushArray(&gameState->permanentStorage, Texture, *numFrames);

	V2 frameSize = v2((double)(frameWidth + frameSpacing) / (double)texWidth, 
		(double)(frameHeight + frameSpacing) / (double)texHeight);

	V2 texSize = v2((double)frameWidth / (double)texWidth, (double)frameHeight/(double)texHeight);

	V2 frameOffset = v2(frameSpacing / (double)texWidth, frameSpacing / (double)texHeight) * 0.5;

	for (s32 rowIndex = 0; rowIndex < numRows; rowIndex++) {
		for (s32 colIndex = 0; colIndex < numCols; colIndex++) {
			s32 textureIndex = colIndex + rowIndex * numCols;
			Texture* frame = result + textureIndex;

			frame->texId = tex.texId;
			frame->normalId = tex.normalId;
			//frame->tex = tex.tex;

			// SDL_Rect* rect = &frame->srcRect;

			// rect->x = colIndex * (frameWidth + frameSpacing) + frameSpacing / 2;
			// rect->y = rowIndex * (frameHeight + frameSpacing) + frameSpacing / 2;
			// rect->w = frameWidth;
			// rect->h = frameHeight;

			V2 minCorner = hadamard(v2(colIndex, rowIndex), frameSize) + frameOffset;

			frame->uv = r2(minCorner, minCorner + texSize);
		}
	}

	return result;
}

Animation loadAnimation(GameState* gameState, char* fileName, 
	s32 frameWidth, s32 frameHeight, double secondsPerFrame, bool pingPong) {
	Animation result = {};

	assert(secondsPerFrame > 0);
	result.secondsPerFrame = secondsPerFrame;
	result.frames = extractTextures(gameState, fileName, frameWidth, frameHeight, 0, &result.numFrames);
	result.pingPong = pingPong;

	return result;
}

Animation createAnimation(Texture* tex) {
	Animation result = {};
	result.secondsPerFrame = 0;
	result.frames = tex;
	result.numFrames = 1;
	return result;
}

Animation createReversedAnimation(Animation* anim) {
	Animation result = *anim;
	result.reverse = !result.reverse;
	return result;
}

AnimNode createAnimNode(Texture* stand) {
	AnimNode node = {};
	node.main = createAnimation(stand);
	return node;
}

double getAnimationDuration(Animation* animation) {
	double result = animation->numFrames * animation->secondsPerFrame;
	return result;
}

Texture* getAnimationFrame(Animation* animation, double animTime) {
	bool32 reverse = animation->reverse;

	s32 frameIndex = 0;
	if (animation->secondsPerFrame > 0) frameIndex = (s32)(animTime / animation->secondsPerFrame) % animation->numFrames;

	if(animation->pingPong) {
		double duration = getAnimationDuration(animation);
		s32 numTimesPlayed = (s32)(animTime / duration);

		if (numTimesPlayed % 2 == 1) {
			reverse = !reverse;
		}
	}

	if (reverse) {
		frameIndex = (animation->numFrames - 1) - frameIndex;
	}

	Texture* result = animation->frames + frameIndex;
	return result;
}

Color createColor(u8 r, u8 g, u8 b, u8 a) {
	Color result = {};

	result.r = r;
	result.g = g;
	result.b = b;
	result.a = a;

	return result;
}

void setColor(RenderGroup* group, Color color) {
	// assert(color.r >= 0 && color.r <= 255);
	// assert(color.g >= 0 && color.g <= 255);
	// assert(color.b >= 0 && color.b <= 255);
	// assert(color.a >= 0 && color.a <= 255);
	//SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);

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

void setClipRect(double pixelsPerMeter, s32 windowHeight, R2 rect) {
	GLint x = (GLint)(rect.min.x * pixelsPerMeter);
	GLint y = (GLint)(rect.min.y * pixelsPerMeter);
	GLsizei width = (GLsizei)(getRectWidth(rect) * pixelsPerMeter);
	GLsizei height = (GLsizei)(getRectHeight(rect) * pixelsPerMeter);

	glEnable(GL_SCISSOR_TEST);
	glScissor(x, y, width, height);
}

Texture createText(GameState* gameState, TTF_Font* font, char* msg) {
	SDL_Color fontColor = {0, 0, 0, 255};
	SDL_Surface* fontSurface = TTF_RenderText_Blended(font, msg, fontColor);

	Texture result = createTexFromSurface(fontSurface, NULL, gameState->renderGroup);

	return result;
}

Texture* getGlyph(CachedFont* cachedFont, RenderGroup* group, char c) {
	if (!cachedFont->cache[c].texId) {	
		SDL_Color black = {0, 0, 0, 255};

		SDL_Surface* glyphSurface = TTF_RenderGlyph_Blended(cachedFont->font, c, black);
		assert(glyphSurface);

		cachedFont->cache[c] = createTexFromSurface(glyphSurface, NULL, group);
		assert(cachedFont->cache[c].texId);
	}

	Texture* result = cachedFont->cache + c;
	return result;
}

double getTextWidth(CachedFont* cachedFont, RenderGroup* group, char* msg) {
	double result = 0;

	double metersPerPixel = 1.0 / (group->pixelsPerMeter * cachedFont->scaleFactor);
	double invScaleFactor = 1.0 / cachedFont->scaleFactor;

	while(*msg) {
		result += getGlyph(cachedFont, group, *msg)->size.x * invScaleFactor;
		msg++;
	}

	return result;
}

RenderGroup* createRenderGroup(GameState* gameState, size_t size) {
	RenderGroup* result = pushStruct(&gameState->permanentStorage, RenderGroup);

	result->allocated = 0;
	result->numSortPtrs = 0;
	result->sortAddressCutoff = 0;
	result->maxSize = size;
	result->base = pushSize(&gameState->permanentStorage, size);
	result->pixelsPerMeter = gameState->pixelsPerMeter;
	result->windowWidth = gameState->windowWidth;
	result->windowHeight = gameState->windowHeight;
	result->windowBounds = r2(v2(0, 0), gameState->windowSize);
	result->forwardShader = createForwardShader("shaders/forward.vert", "shaders/forward.frag", gameState->windowSize);
	result->basicShader = createShader("shaders/basic.vert", "shaders/basic.frag", gameState->windowSize);

	#if 0
	SDL_Surface* nullNormalSurface = IMG_Load("res/no normal map.png");
	assert(nullNormalSurface);
	result->nullNormalId = generateTexture(nullNormalSurface);
	SDL_FreeSurface(nullNormalSurface);
	#else
	result->nullNormalId = 0;
	#endif

	gameState->renderGroup = result;

	result->whiteTex = loadPNGTexture(gameState, "res/white");

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

void bindTexture(Texture* texture, RenderGroup* group, bool flipX) {
	assert(texture->texId);

	glEnable(GL_TEXTURE_2D);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, texture->texId);

	if(group->currentShader == (Shader*)&group->forwardShader) {
		if(texture->normalId) {
			glUniform1f(group->forwardShader.normalXFlipUniform, flipX ? 1.0f : -1.0f);

			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, texture->normalId);
		} else {
			glUniform1f(group->forwardShader.normalXFlipUniform, 0);
		}
	}
}

void setAmbient(RenderGroup* group, GLfloat ambient) {
	glUniform3f(group->forwardShader.ambientUniform, ambient, ambient, ambient);
}

void drawTexture(RenderGroup* group, Texture* texture, R2 bounds, bool flipX, Orientation orientation, float emissivity, GLfloat ambient) {
	if(emissivity) {
		setAmbient(group, ambient + (1 - ambient) * emissivity);
	}

	bindTexture(texture, group, flipX);

	setColor(group, createColor(255, 255, 255, 255));

	V2 uvMin = texture->uv.min;
	V2 uvMax = texture->uv.max;

	if (!flipX) {
		double temp = uvMin.x;
		uvMin.x = uvMax.x;
		uvMax.x = temp;
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

void drawText(RenderGroup* group, CachedFont* cachedFont, char* msg, V2 p) {
	//CachedFont* cachedFont = render->font;
	//char* msg = (char*)render->msg;

	double metersPerPixel = 1.0 / (group->pixelsPerMeter * cachedFont->scaleFactor);
	double invScaleFactor = 1.0 / cachedFont->scaleFactor;

	while(*msg) {
		Texture* texture = getGlyph(cachedFont, group, *msg);
		V2 size = texture->size * invScaleFactor;
		R2 bounds = r2(p, p + size);

		drawTexture(group, texture, bounds, false, Orientation_0, 0, 1);

		p.x += size.x;
		msg++;
	}
}

void drawFillRect(RenderGroup* group, R2 bounds, Color color) {
	// SDL_Rect dstRect = getPixelSpaceRect(group->pixelsPerMeter, group->windowHeight, rect);
	// SDL_RenderFillRect(group->renderer, &dstRect);

	bindTexture(&group->whiteTex, group, false);
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

void renderDrawConsoleField(RenderGroup* group, FieldSpec* fieldSpec, ConsoleField* field) {
	field->p += group->negativeCameraP;
	drawConsoleField(field, group, NULL, fieldSpec, false);
	field->p -= group->negativeCameraP;
}

DrawType getRenderHeaderType(RenderHeader* header) {
	DrawType result = (DrawType)(header->type_ & RENDER_HEADER_TYPE_MASK);
	return result;
}

void setRenderHeaderType(RenderHeader* header, DrawType type) {
	header->type_ = (header->type_ & RENDER_HEADER_CLIP_RECT_FLAG) | type;
}

bool32 renderElemClipRect(RenderHeader* header) {
	bool32 result = header->type_ & RENDER_HEADER_CLIP_RECT_FLAG;
	return result;
}

void setRenderElemClipRect(RenderHeader* header) {
	header->type_ |= RENDER_HEADER_CLIP_RECT_FLAG;
}

void clearRenderElemClipRect(RenderHeader* header) {
	header->type_ &= ~RENDER_HEADER_CLIP_RECT_FLAG;
}

#define pushRenderElement(group, type) (type*)pushRenderElement_(group, DrawType_##type, sizeof(type))
void* pushRenderElement_(RenderGroup* group, DrawType type, size_t size) {
	size_t headerBytes = sizeof(RenderHeader);

	if (group->hasClipRect) {
		headerBytes += sizeof(R2);
	}

	size += headerBytes;

	void* result = NULL;

	if (group->allocated + size < group->maxSize) {
		if (group->enabled) {
			result = (char*)group->base + group->allocated;
			group->allocated += size;

			RenderHeader* header = (RenderHeader*)result;
			setRenderHeaderType(header, type);

			if(group->hasClipRect) {
				setRenderElemClipRect(header);
				R2* clipRectPtr = (R2*)((char*)result + sizeof(RenderHeader));
				*clipRectPtr = group->clipRect;
			} else {
				clearRenderElemClipRect(header);
			}

			if(!group->sortAddressCutoff) {
				assert(group->numSortPtrs + 1 < arrayCount(group->sortPtrs));
				group->sortPtrs[group->numSortPtrs++] = (RenderHeader*)result;
			}

			result = (char*)result + headerBytes;
		}
	} else {
		assert(false);
	}

	return result;
}

void pushSortEnd(RenderGroup* group) {
	group->sortAddressCutoff = group->allocated;
}

RenderTexture createRenderTexture(DrawOrder drawOrder, Texture* texture, bool flipX, Orientation orientation, float emissivity) {
	RenderTexture result = {};

	result.drawOrder = drawOrder;
	result.texture = texture;
	result.flipX = flipX;
	result.orientation = orientation;
	result.emissivity = emissivity;

	return result;
}


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


void pushTexture(RenderGroup* group, Texture* texture, R2 bounds, bool flipX, DrawOrder drawOrder,
	bool moveIntoCameraSpace = false, Orientation orientation = Orientation_0, float emissivity = 0) {

	assert(texture);

	R2 drawBounds = moveIntoCameraSpace ? translateRect(bounds, group->negativeCameraP) : bounds;

	if(rectanglesOverlap(group->windowBounds, drawBounds)) {
		if(group->rendering) {
			drawTexture(group, texture, drawBounds, flipX, orientation, emissivity, group->ambient);
		} else {
			RenderBoundedTexture* render = pushRenderElement(group, RenderBoundedTexture);

			if (render) {
				render->tex = createRenderTexture(drawOrder, texture, flipX, orientation, emissivity);
				render->bounds = drawBounds;
			}
		}
	}
}

void pushEntityTexture(RenderGroup* group, Texture* texture, Entity* entity, bool flipX, DrawOrder drawOrder, Orientation orientation = Orientation_0) {
	assert(texture);

	R2 drawBounds = scaleRect(translateRect(rectCenterDiameter(entity->p, entity->renderSize), group->negativeCameraP), v2(1.05, 1.05));

	if(rectanglesOverlap(group->windowBounds, drawBounds)) {
		if(group->rendering) {
			drawTexture(group, texture, drawBounds, flipX, orientation, entity->emissivity, group->ambient);
		} else {
			RenderEntityTexture* render = pushRenderElement(group, RenderEntityTexture);

			if (render) {
				render->tex = createRenderTexture(drawOrder, texture, flipX, orientation, entity->emissivity);
				render->p = &entity->p;
				render->renderSize = &entity->renderSize;
			}
		}
	}
}

void pushText(RenderGroup* group, CachedFont* font, char* msg, V2 p) {
	if(group->rendering) {
		drawText(group, font, msg, p);
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
		}
	}
}

void pushFilledRect(RenderGroup* group, R2 bounds, Color color, bool moveIntoCameraSpace = false) {
	R2 drawBounds = moveIntoCameraSpace ? translateRect(bounds, group->negativeCameraP) : bounds;

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
	R2 drawBounds = moveIntoCameraSpace ? translateRect(bounds, group->negativeCameraP) : bounds;

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

			*light = getRenderablePointLight(pl, moveIntoCameraSpace, group->negativeCameraP);
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

			light->base = getRenderablePointLight(&sl->base, moveIntoCameraSpace, group->negativeCameraP);
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

size_t drawRenderElem(RenderGroup* group, FieldSpec* fieldSpec, void* elemPtr, GLfloat ambient) {
	RenderHeader* header = (RenderHeader*)elemPtr;
	size_t elemSize = sizeof(RenderHeader);

	elemPtr = (char*)elemPtr + sizeof(RenderHeader);

	if (renderElemClipRect(header)) {
		R2* clipBounds = (R2*)elemPtr;
		elemPtr = (char*)elemPtr + sizeof(R2);
		elemSize += sizeof(R2);
		setClipRect(group->pixelsPerMeter, group->windowHeight, *clipBounds);
	} else {
		//setClipRect(group->renderer, group->pixelsPerMeter, group->windowHeight, group->windowBounds);
		glDisable(GL_SCISSOR_TEST);
	}

	#define START_CASE(type) case DrawType_##type: { type* render = (type*)elemPtr
	#define END_CASE(type) elemSize += sizeof(type); } break

	switch(getRenderHeaderType(header)) {

		START_CASE(RenderBoundedTexture);
			drawTexture(group, render->tex.texture, render->bounds, render->tex.flipX != 0, render->tex.orientation, render->tex.emissivity, ambient);
		END_CASE(RenderBoundedTexture);

		START_CASE(RenderEntityTexture);
			R2 bounds = rectCenterDiameter(*render->p + group->negativeCameraP, *render->renderSize);
			drawTexture(group, render->tex.texture, bounds, render->tex.flipX != 0, render->tex.orientation, render->tex.emissivity, ambient);
		END_CASE(RenderEntityTexture);

		START_CASE(RenderText);
			drawText(group, render->font, render->msg, render->p);
		END_CASE(RenderText);

		START_CASE(RenderFillRect);
			drawFillRect(group, render->bounds, render->color);
		END_CASE(RenderFillRect);

		START_CASE(RenderOutlinedRect);
			drawOutlinedRect(group, render->bounds, render->color, render->thickness);
		END_CASE(RenderOutlinedRect);

		START_CASE(RenderConsoleField);
			renderDrawConsoleField(group, fieldSpec, render->field);
		END_CASE(RenderConsoleField);

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

	if(e1IsRenderTexture || e1IsConsoleField) {
		if(!e2IsRenderTexture && !e2IsConsoleField) return -1;

		RenderTexture* r1 = NULL;
		if(e1IsRenderTexture) r1 = getRenderTexture(e1);
		RenderTexture* r2 = NULL;
		if(e2IsRenderTexture) r2 = getRenderTexture(e2);

		DrawOrder r1DrawOrder = (r1 == NULL ? DrawOrder_pickupField : r1->drawOrder);
		DrawOrder r2DrawOrder = (r2 == NULL ? DrawOrder_pickupField : r2->drawOrder);

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

	if (e2IsRenderTexture || e2IsConsoleField) return 1;

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

void bindShader(RenderGroup* group, Shader* shader) {
	group->currentShader = shader;
	glUseProgram(shader->program);
}

void drawRenderGroup(RenderGroup* group, FieldSpec* fieldSpec) {
	group->rendering = true;
	qsort(group->sortPtrs, group->numSortPtrs, sizeof(RenderHeader*), renderElemCompare);
	bindShader(group, &group->forwardShader.shader);

#if 0
	static float iter = 0;
	iter += 0.01f;
	float tempXOffset = 3 * sin(iter);

	PointLight light1 = {v3(15 + tempXOffset, 7, 0), v3(1, 0.3, 0.3), 6};
	pushPointLight(group, &light1, true);

	group->ambient = (GLfloat)0.25;
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

			double angle = toRadians(light->angle);
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
		drawRenderElem(group, fieldSpec, elemPtr, group->ambient);
	}

	u32 groupByteIndex = group->sortAddressCutoff;

	bindShader(group, &group->basicShader);

	while(groupByteIndex < group->allocated) {
		void* elemPtr = (char*)group->base + groupByteIndex;
		groupByteIndex += drawRenderElem(group, fieldSpec, elemPtr, group->ambient);
	}

	group->numSortPtrs = 0;
	group->allocated = 0;
	group->sortAddressCutoff = 0;
	group->forwardShader.numPointLights = 0;
	group->forwardShader.numSpotLights = 0;
	pushDefaultClipRect(group);
	group->rendering = false;
}
