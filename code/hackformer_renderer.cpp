void loadShaderSource(char* fileName, char* source, int maxSourceLength) {
	FILE* file;
	fopen_s(&file, fileName, "r");
	assert(file);

	char line[1024];
	char* linePtr;
	int length = 0;

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
		int errorLength = 0;
		glGetShaderInfoLog(shader, arrayCount(errorBuffer), &errorLength, errorBuffer);
		fprintf(stderr, "Failed to compile shader: %s\n", errorBuffer);
		assert(false);
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
		assert(false);
	}

	glDetachShader(result.program, vertexShader);
	glDetachShader(result.program, fragmentShader);

	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);

	glUseProgram(result.program);
	GLint windowSizeUniformLocation = glGetUniformLocation(result.program, "twoOverScreenSize");
	glUniform2f(windowSizeUniformLocation, (GLfloat)(2.0/windowSize.x), (GLfloat)(2.0/windowSize.y));

	result.tintUniformLocation = glGetUniformLocation(result.program, "tint");
	result.ambientUniform = glGetUniformLocation(result.program, "ambient");

	std::string uniformPrefix = "pointLights[";

	for(int lightIndex = 0; lightIndex < arrayCount(result.pointLightUniforms); lightIndex++) {
		PointLightUniforms* lightUniforms = result.pointLightUniforms + lightIndex;

		char lightIndexStrMem[5];
		_itoa_s(lightIndex, lightIndexStrMem, 10);

		std::string pUniformName = uniformPrefix + lightIndexStrMem + "].p";
		lightUniforms->p = glGetUniformLocation(result.program, pUniformName.c_str());

		std::string colorUniformName = uniformPrefix + lightIndexStrMem + "].color";
		lightUniforms->color = glGetUniformLocation(result.program, colorUniformName.c_str());

		std::string attenUniformName = uniformPrefix + lightIndexStrMem + "].atten";
		lightUniforms->atten = glGetUniformLocation(result.program, attenUniformName.c_str());
	}

	uniformPrefix = "spotLights[";

	for(int lightIndex = 0; lightIndex < arrayCount(result.spotLightUniforms); lightIndex++) {
		SpotLightUniforms* lightUniforms = result.spotLightUniforms + lightIndex;

		char lightIndexStrMem[5];
		_itoa_s(lightIndex, lightIndexStrMem, 10);

		std::string pUniformName = uniformPrefix + lightIndexStrMem + "].base.p";
		lightUniforms->base.p = glGetUniformLocation(result.program, pUniformName.c_str());

		std::string colorUniformName = uniformPrefix + lightIndexStrMem + "].base.color";
		lightUniforms->base.color = glGetUniformLocation(result.program, colorUniformName.c_str());

		std::string attenUniformName = uniformPrefix + lightIndexStrMem + "].base.atten";
		lightUniforms->base.atten = glGetUniformLocation(result.program, attenUniformName.c_str());

		std::string dirUniformName = uniformPrefix + lightIndexStrMem + "].dir";
		lightUniforms->dir = glGetUniformLocation(result.program, dirUniformName.c_str());
		
		std::string cutoffUniformName = uniformPrefix + lightIndexStrMem + "].cutoff";
		lightUniforms->cutoff = glGetUniformLocation(result.program, cutoffUniformName.c_str());
	}

	result.normalXFlipUniform = glGetUniformLocation(result.program, "normalXFlip");

	GLint diffuseLocation = glGetUniformLocation(result.program, "diffuseTexture");
	GLint normalLocation = glGetUniformLocation(result.program, "normalTexture");

	glUniform1i(diffuseLocation, 0);
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

	// result.srcRect.w = image->w;
	// result.srcRect.h = image->h;

	// result.tex = SDL_CreateTextureFromSurface(renderer, image);
	// assert(result.tex);

	// SDL_SetTextureBlendMode(result.tex, SDL_BLENDMODE_BLEND);
	// SDL_FreeSurface(image);

	return result;
}

Texture loadPNGTexture(GameState* gameState, char* fileName, bool loadNormalMap = true) {
	std::string suffix = ".png";
	std::string diffuseFileName = fileName + suffix;

	SDL_Surface* diffuse = IMG_Load(diffuseFileName.c_str());
	if (!diffuse) fprintf(stderr, "Failed to load image from: %s\n", fileName);
	assert(diffuse);


	std::string normalSuffix = "_NRM.png";
	std::string normalFileName = fileName + normalSuffix;

	SDL_Surface* normal = NULL;
	if (loadNormalMap) normal = IMG_Load(normalFileName.c_str());

	Texture result = createTexFromSurface(diffuse, normal, gameState->renderGroup);

	return result;
}

TTF_Font* loadFont(char* fileName, int fontSize) {
	TTF_Font *font = TTF_OpenFont(fileName, fontSize);

	if (!font) {
		fprintf(stderr, "Failed to load font: %s\n", fileName);
		assert(false);
	}

	TTF_SetFontHinting(font, TTF_HINTING_LIGHT);

	return font;
}

CachedFont loadCachedFont(GameState* gameState, char* fileName, int fontSize, double scaleFactor) {
	CachedFont result = {};
	result.font = loadFont(fileName, (int)round(fontSize * scaleFactor));
	result.scaleFactor = scaleFactor;
	result.lineHeight = fontSize / (gameState->pixelsPerMeter * scaleFactor);
	return result;
}

Texture* extractTextures(GameState* gameState, char* fileName, 
	int frameWidth, int frameHeight, int frameSpacing, uint* numFrames) {

	Texture tex = loadPNGTexture(gameState, fileName);

	int texWidth = (int)(tex.size.x * gameState->pixelsPerMeter + 0.5);
	int texHeight = (int)(tex.size.y * gameState->pixelsPerMeter + 0.5);

	int numCols = texWidth / (frameWidth + frameSpacing);
	int numRows = texHeight / (frameHeight + frameSpacing);

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

	for (int rowIndex = 0; rowIndex < numRows; rowIndex++) {
		for (int colIndex = 0; colIndex < numCols; colIndex++) {
			int textureIndex = colIndex + rowIndex * numCols;
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
	int frameWidth, int frameHeight, double secondsPerFrame, bool pingPong) {
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
	bool reverse = animation->reverse;

	int frameIndex = 0;
	if (animation->secondsPerFrame > 0) frameIndex = (int)(animTime / animation->secondsPerFrame) % animation->numFrames;

	if(animation->pingPong) {
		double duration = getAnimationDuration(animation);
		int numTimesPlayed = (int)(animTime / duration);

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

Color createColor(int r, int g, int b, int a) {
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


	glUniform4f(group->shader.tintUniformLocation, r, g, b, a);
}

SDL_Rect getPixelSpaceRect(double pixelsPerMeter, int windowHeight, R2 rect) {
	SDL_Rect result = {};

	double width = getRectWidth(rect);
	double height = getRectHeight(rect);

	result.w = (int)ceil(width * pixelsPerMeter);
	result.h = (int)ceil(height * pixelsPerMeter);
	result.x = (int)ceil(rect.max.x * pixelsPerMeter) - result.w;
	result.y = windowHeight - (int)ceil(rect.max.y * pixelsPerMeter);

	return result;
}

void setClipRect(SDL_Renderer* renderer, double pixelsPerMeter, int windowHeight, R2 rect) {
	// SDL_Rect clipRect = getPixelSpaceRect(pixelsPerMeter, windowHeight, rect);
	// SDL_RenderSetClipRect(renderer, &clipRect);

	GLint x = (GLint)(rect.min.x * pixelsPerMeter);
	GLint y = (GLint)(rect.min.y * pixelsPerMeter);
	GLsizei width = (GLsizei)(getRectWidth(rect) *pixelsPerMeter);
	GLsizei height = (GLsizei)(getRectHeight(rect) *pixelsPerMeter);

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

double getTextWidth(CachedFont* cachedFont, GameState* gameState, char* msg) {
	double result = 0;

	double metersPerPixel = 1.0 / (gameState->pixelsPerMeter * cachedFont->scaleFactor);
	double invScaleFactor = 1.0 / cachedFont->scaleFactor;

	while(*msg) {
		result += getGlyph(cachedFont, gameState->renderGroup, *msg)->size.x * invScaleFactor;
		msg++;
	}

	return result;
}

RenderGroup* createRenderGroup(GameState* gameState, uint size) {
	RenderGroup* result = pushStruct(&gameState->permanentStorage, RenderGroup);

	result->allocated = 0;
	result->numSortPtrs = 0;
	result->sortAddressCutoff = 0;
	result->maxSize = size;
	result->base = pushSize(&gameState->permanentStorage, size);
	result->renderer = gameState->renderer;
	result->pixelsPerMeter = gameState->pixelsPerMeter;
	result->windowWidth = gameState->windowWidth;
	result->windowHeight = gameState->windowHeight;
	result->windowBounds = r2(v2(0, 0), gameState->windowSize);
	result->shader = createShader("shaders/basic.vert", "shaders/basic.frag", gameState->windowSize);

	SDL_Surface* nullNormalSurface = IMG_Load("res/no normal map.png");
	assert(nullNormalSurface);
	result->nullNormalId = generateTexture(nullNormalSurface);
	SDL_FreeSurface(nullNormalSurface);

	gameState->renderGroup = result;

	result->whiteTex = loadPNGTexture(gameState, "res/white");

	return result;
}

void sendTexCoord(V2 uvMin, V2 uvMax, int orientation) {
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

void bindTexture(Texture* texture) {
	assert(texture->texId);
	assert(texture->normalId);

	glEnable(GL_TEXTURE_2D);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, texture->texId);

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, texture->normalId);
}

void drawTexture(RenderGroup* group, Texture* texture, R2 bounds, bool flipX, Orientation orientation) {
	// SDL_Rect dstRect = getPixelSpaceRect(group->pixelsPerMeter, group->windowHeight, bounds);

	// SDL_RendererFlip flip = flipX ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE;
	// SDL_RenderCopyEx(group->renderer, texture->tex, &texture->srcRect, &dstRect, degrees, NULL, flip);

	bindTexture(texture);

	glUniform1f(group->shader.normalXFlipUniform, flipX ? 1.0f : -1.0f);
	
	glColor4f(1, 1, 1, 1);

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
}

void drawText(RenderGroup* group, RenderText* render) {
	CachedFont* cachedFont = render->font;
	char* msg = (char*)render->msg;

	double metersPerPixel = 1.0 / (group->pixelsPerMeter * cachedFont->scaleFactor);
	double invScaleFactor = 1.0 / cachedFont->scaleFactor;

	while(*msg) {
		Texture* texture = getGlyph(cachedFont, group, *msg);
		V2 size = texture->size * invScaleFactor;
		R2 bounds = r2(render->p, render->p + size);

		drawTexture(group, texture, bounds, false, Orientation_0);

		render->p.x += size.x;
		msg++;
	}
}

void drawFillRect(RenderGroup* group, R2 bounds) {
	// SDL_Rect dstRect = getPixelSpaceRect(group->pixelsPerMeter, group->windowHeight, rect);
	// SDL_RenderFillRect(group->renderer, &dstRect);

	bindTexture(&group->whiteTex);

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

void drawOutlinedRect(RenderGroup* group, R2 rect, double thickness) {
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

	drawFillRect(group, r2(bottomMinCorner, bottomMaxCorner));
	drawFillRect(group, r2(topMinCorner, topMaxCorner));
	drawFillRect(group, r2(leftMinCorner, leftMaxCorner));
	drawFillRect(group, r2(rightMinCorner, rightMaxCorner));
}

DrawType getRenderHeaderType(RenderHeader* header) {
	DrawType result = (DrawType)(header->type_ & RENDER_HEADER_TYPE_MASK);
	return result;
}

void setRenderHeaderType(RenderHeader* header, DrawType type) {
	header->type_ = (header->type_ & RENDER_HEADER_CLIP_RECT_FLAG) | type;
}

uint renderElemClipRect(RenderHeader* header) {
	uint result = header->type_ & RENDER_HEADER_CLIP_RECT_FLAG;
	return result;
}

void setRenderElemClipRect(RenderHeader* header) {
	header->type_ |= RENDER_HEADER_CLIP_RECT_FLAG;
}

void clearRenderElemClipRect(RenderHeader* header) {
	header->type_ &= ~RENDER_HEADER_CLIP_RECT_FLAG;
}

#define pushRenderElement(group, type) (type*)pushRenderElement_(group, DrawType_##type, sizeof(type))
void* pushRenderElement_(RenderGroup* group, DrawType type, uint size) {
	int headerBytes = sizeof(RenderHeader);

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

RenderTexture createRenderTexture(DrawOrder drawOrder, Texture* texture, bool flipX, Orientation orientation) {
	RenderTexture result = {};

	result.drawOrder = drawOrder;
	result.texture = texture;
	result.flipX = flipX;
	result.orientation = orientation;

	return result;
}

void pushTexture(RenderGroup* group, Texture* texture, R2 bounds, bool flipX, DrawOrder drawOrder,
					bool moveIntoCameraSpace = false, Orientation orientation = Orientation_0) {

	assert(texture);

	R2 drawBounds = moveIntoCameraSpace ? translateRect(bounds, group->negativeCameraP) : bounds;

	if(rectanglesOverlap(group->windowBounds, drawBounds)) {
		RenderBoundedTexture* render = pushRenderElement(group, RenderBoundedTexture);

		if (render) {
			render->tex = createRenderTexture(drawOrder, texture, flipX, orientation);
			render->bounds = drawBounds;
		}
	}
}

void pushEntityTexture(RenderGroup* group, Texture* texture, Entity* entity, bool flipX, DrawOrder drawOrder, Orientation orientation = Orientation_0) {
	assert(texture);

	R2 drawBounds = scaleRect(translateRect(rectCenterDiameter(entity->p, entity->renderSize), group->negativeCameraP), v2(1.05, 1.05));

	if(rectanglesOverlap(group->windowBounds, drawBounds)) {
		RenderEntityTexture* render = pushRenderElement(group, RenderEntityTexture);

		if (render) {
			render->tex = createRenderTexture(drawOrder, texture, flipX, orientation);
			render->p = &entity->p;
			render->renderSize = &entity->renderSize;
		}
	}
}

void pushText(RenderGroup* group, CachedFont* font, char* msg, V2 p) {
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

void pushFilledRect(RenderGroup* group, R2 bounds, Color color, bool moveIntoCameraSpace = false) {
	R2 drawBounds = moveIntoCameraSpace ? translateRect(bounds, group->negativeCameraP) : bounds;

	if(rectanglesOverlap(group->windowBounds, drawBounds)) {
		RenderFillRect* render = pushRenderElement(group, RenderFillRect);

		if (render) {
			render->color = color;
			render->bounds = drawBounds;
		}
	}
}

void pushOutlinedRect(RenderGroup* group, R2 bounds, double thickness, Color color, bool moveIntoCameraSpace = false) {
	R2 drawBounds = moveIntoCameraSpace ? translateRect(bounds, group->negativeCameraP) : bounds;

	if(rectanglesOverlap(group->windowBounds, addDiameterTo(drawBounds, v2(1, 1) * thickness))) {
		RenderOutlinedRect* render = pushRenderElement(group, RenderOutlinedRect);

		if (render) {
			render->thickness = thickness;
			render->color = color;
			render->bounds = drawBounds;
		}
	}
}

PointLight getRenderablePointLight(PointLight* light, bool moveIntoCameraSpace, V2 negativeCameraP) {
	assert(light->atten.x >= 0);
	assert(light->atten.y >= 0);
	assert(light->atten.z >= 0);

	PointLight result = *light;

	//NOTE: This is to avoid divide by 0 errors in the shader
	if(result.atten.x == 0) result.atten.x = 0.00000001;
	if (moveIntoCameraSpace) result.p += v3(negativeCameraP, 0);

	return result;
}

void pushPointLight(RenderGroup* group, PointLight* pl, bool moveIntoCameraSpace = false) {
	if(group->shader.numPointLights + 1 < arrayCount(group->shader.pointLights)) {
		PointLight* light = group->shader.pointLights + group->shader.numPointLights;
		group->shader.numPointLights++;

		*light = getRenderablePointLight(pl, moveIntoCameraSpace, group->negativeCameraP);
	} else {
		InvalidCodePath;
	}
}

void pushSpotLight(RenderGroup* group, SpotLight* sl, bool moveIntoCameraSpace = false) {
	if(group->shader.numSpotLights + 1 < arrayCount(group->shader.spotLights)) {
		SpotLight* light = group->shader.spotLights + group->shader.numSpotLights;
		group->shader.numSpotLights++;

		light->base = getRenderablePointLight(&sl->base, moveIntoCameraSpace, group->negativeCameraP);
		light->angle = sl->angle;
		light->spread = sl->spread;
	} else {
		InvalidCodePath;
	}
}


void pushClipRect(RenderGroup* group, R2 clipRect) {
	group->clipRect = clipRect;
	group->hasClipRect = true;
}

void pushDefaultClipRect(RenderGroup* group) {
	group->hasClipRect = false;
}

int drawRenderElem(RenderGroup* group, void* elemPtr) {
	RenderHeader* header = (RenderHeader*)elemPtr;
	int elemSize = sizeof(RenderHeader);

	elemPtr = (char*)elemPtr + sizeof(RenderHeader);

	if (renderElemClipRect(header)) {
		R2* clipBounds = (R2*)elemPtr;
		elemPtr = (char*)elemPtr + sizeof(R2);
		elemSize += sizeof(R2);
		setClipRect(group->renderer, group->pixelsPerMeter, group->windowHeight, *clipBounds);
	} else {
		//setClipRect(group->renderer, group->pixelsPerMeter, group->windowHeight, group->windowBounds);
		glDisable(GL_SCISSOR_TEST);
	}

	switch(getRenderHeaderType(header)) {
		case DrawType_RenderBoundedTexture: {
			RenderBoundedTexture* render = (RenderBoundedTexture*)elemPtr;
			setColor(group, createColor(255, 255, 255, 255));
			drawTexture(group, render->tex.texture, render->bounds, render->tex.flipX, render->tex.orientation);
			elemSize += sizeof(RenderBoundedTexture);
		} break;

		case DrawType_RenderEntityTexture: {
			RenderEntityTexture* render = (RenderEntityTexture*)elemPtr;
			R2 bounds = rectCenterDiameter(*render->p + group->negativeCameraP, *render->renderSize);
			setColor(group, createColor(255, 255, 255, 255));
			drawTexture(group, render->tex.texture, bounds, render->tex.flipX, render->tex.orientation);
			elemSize += sizeof(RenderEntityTexture);
		} break;

		case DrawType_RenderText: {
			RenderText* render = (RenderText*)elemPtr;
			setColor(group, createColor(255, 255, 255, 255));
			drawText(group, render);
			elemSize += sizeof(RenderText);
		} break;

		case DrawType_RenderFillRect: {
			RenderFillRect* render = (RenderFillRect*)elemPtr;
			setColor(group, render->color);
			drawFillRect(group, render->bounds);
			elemSize += sizeof(RenderFillRect);
		} break;

		case DrawType_RenderOutlinedRect: {
			RenderOutlinedRect* render = (RenderOutlinedRect*)elemPtr;
			setColor(group, render->color);
			drawOutlinedRect(group, render->bounds, render->thickness);
			elemSize += sizeof(RenderOutlinedRect);
		} break;

		default: {
			assert(false);
		} break;
	}

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

int renderElemCompare(const void* a, const void* b) {
	RenderHeader* e1 = *(RenderHeader**)a;
	RenderHeader* e2 = *(RenderHeader**)b;

	DrawType e1Type = getRenderHeaderType(e1);
	DrawType e2Type = getRenderHeaderType(e2);

	if(isRenderTextureType(e1Type)) {
		if(!isRenderTextureType(e2Type)) return -1;

		RenderTexture* r1 = getRenderTexture(e1);
		RenderTexture* r2 = getRenderTexture(e2);

		if(r1->drawOrder > r2->drawOrder) return 1;
		if(r1->drawOrder < r2->drawOrder) return -1;

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

	if (isRenderTextureType(e2Type)) return 1;

	return 0;
}

bool isPointLightVisible(RenderGroup* group, PointLight* light) {
	double colorDepth = 1.0 / 256.0;
	double range = (1 - colorDepth * light->atten.x) / (colorDepth * (light->atten.y + square(light->atten.z)));

	R2 maxLightBounds = translateRect(addRadiusTo(group->windowBounds, v2(1, 1) * range), -group->negativeCameraP);

	bool result = pointInsideRect(maxLightBounds, light->p.xy);
	return result;
}

void setPointLightUniforms(PointLightUniforms* lightUniforms, PointLight* light) {
	glUniform3f(lightUniforms->p, (GLfloat)light->p.x, (GLfloat)light->p.y, (GLfloat)light->p.z);
	glUniform3f(lightUniforms->color, (GLfloat)light->color.x, (GLfloat)light->color.y, (GLfloat)light->color.z);
	glUniform3f(lightUniforms->atten, (GLfloat)light->atten.x, (GLfloat)light->atten.y, (GLfloat)light->atten.z);
}

void drawRenderGroup(RenderGroup* group) {
	qsort(group->sortPtrs, group->numSortPtrs, sizeof(RenderHeader*), renderElemCompare);

#if 0
	glEnable(GL_DITHER);

	glUseProgram(group->shader.program);

	PointLight light1 = {v3(12, 5, 0), v3(1, 1, 1), v3(1, 0.5, 2)};
	//pushPointLight(group, &light1, true);

	// PointLight light2 = {v3(3, 3, 1), v3(0, 1, 0), v3(1, 1, 0)};
	// pushPointLight(group, &light2, true);

	static float angle = 0;
	angle += 1;

	SpotLight spotLight = {light1, angle, angle};
	pushSpotLight(group, &spotLight, true);

#else
	group->shader.numPointLights = group->shader.numSpotLights = 0;
#endif

	GLfloat ambient = (GLfloat)1;
	glUniform3f(group->shader.ambientUniform, (GLfloat)ambient, (GLfloat)ambient, (GLfloat)ambient);

	int uniformPointLightIndex = 0;
	int uniformSpotLightIndex = 0;

	PointLight defaultPointLight = {};
	defaultPointLight.atten.x = 1000000000;

	for(int lightIndex = 0; lightIndex < group->shader.numPointLights; lightIndex++) {
		PointLight* light = group->shader.pointLights + lightIndex;

		//TODO: account for z
		//TODO: handle case where we run out of lights in a nicer way
		if(isPointLightVisible(group, light)) {
			assert(uniformPointLightIndex < arrayCount(group->shader.pointLightUniforms));
			PointLightUniforms* lightUniforms = group->shader.pointLightUniforms + uniformPointLightIndex++;
			setPointLightUniforms(lightUniforms, light);
		}
	}

	for(int lightIndex = group->shader.numPointLights; lightIndex < arrayCount(group->shader.pointLightUniforms); lightIndex++) {
		PointLightUniforms* lightUniforms = group->shader.pointLightUniforms + lightIndex;
		setPointLightUniforms(lightUniforms, &defaultPointLight);
	}

	for(int lightIndex = 0; lightIndex < group->shader.numSpotLights; lightIndex++) {
		SpotLight* light = group->shader.spotLights + lightIndex;

		//TODO: account for z
		//TODO: handle case where we run out of lights in a nicer way
		if(isPointLightVisible(group, &light->base)) {
			assert(uniformSpotLightIndex < arrayCount(group->shader.spotLightUniforms));
			SpotLightUniforms* lightUniforms = group->shader.spotLightUniforms + uniformSpotLightIndex++;
			setPointLightUniforms(&lightUniforms->base, &light->base);

			V2 lightDir = v2(cos(light->angle), sin(light->angle));
			double cutoff = cos(light->spread / 2);

			glUniform2f(lightUniforms->dir, (GLfloat)lightDir.x, (GLfloat)lightDir.y);
			glUniform1f(lightUniforms->cutoff, (GLfloat)cutoff);
		}
	}

	for(int lightIndex = group->shader.numSpotLights; lightIndex < arrayCount(group->shader.spotLightUniforms); lightIndex++) {
		SpotLightUniforms* lightUniforms = group->shader.spotLightUniforms + lightIndex;
		setPointLightUniforms(&lightUniforms->base, &defaultPointLight);
		glUniform2f(lightUniforms->dir, (GLfloat)1, (GLfloat)0);
		glUniform1f(lightUniforms->cutoff, (GLfloat)1);
	}

	for(int elemIndex = 0; elemIndex < group->numSortPtrs; elemIndex++) {
		void* elemPtr = group->sortPtrs[elemIndex];
		drawRenderElem(group, elemPtr);
	}

	uint groupByteIndex = group->sortAddressCutoff;

	while(groupByteIndex < group->allocated) {
		void* elemPtr = (char*)group->base + groupByteIndex;
		groupByteIndex += drawRenderElem(group, elemPtr);
	}

	group->numSortPtrs = 0;
	group->allocated = 0;
	group->sortAddressCutoff = 0;
	group->shader.numPointLights = 0;
	group->shader.numSpotLights = 0;
	pushDefaultClipRect(group);
}
