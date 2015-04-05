// void loadShaderSource(char* fileName, char* source, int maxSourceLength) {
// 	FILE* file;
// 	fopen_s(&file, fileName, "r");
// 	assert(file);

// 	char line[1024];
// 	char* linePtr;
// 	int length = 0;

// 	while (fgets (line, sizeof(line), file)) {
// 		linePtr = line;

// 		while(*linePtr) {
// 			*source = *linePtr;
// 			source++;
// 			linePtr++;
// 			length++;
// 			assert(length < maxSourceLength);
// 		}
// 	}

// 	*source = 0;
// }

// GLuint addShader_(GLenum type, char* sourceFileName, Shader shaderProgram) {
// 	char sourceBuffer[1000];
// 	char* source = sourceBuffer;
// 	loadShaderSource(sourceFileName, source, arrayCount(sourceBuffer));

// 	GLuint shader = glCreateShader(type);
// 	assert(shader);

// 	glShaderSource(shader, 1, &source, NULL);

// 	glCompileShader(shader);
// 	GLint compileSuccess = 0;
// 	glGetShaderiv(shader, GL_COMPILE_STATUS, &compileSuccess);

// 	if (compileSuccess == GL_FALSE) {
// 		char errorBuffer[1024];
// 		int errorLength = 0;
// 		glGetShaderInfoLog(shader, arrayCount(errorBuffer), &errorLength, errorBuffer);
// 		fprintf(stderr, "Failed to compile shader: %s\n", errorBuffer);
// 		assert(false);
// 	}

// 	glAttachShader(shaderProgram.program, shader);

// 	return shader;
// }

// Shader createShader(char* vertexShaderSourceFileName, char* fragmentShaderSourceFileName, V2 windowSize) {
// 	Shader result = {};

// 	result.program = glCreateProgram();
// 	assert(result.program);

// 	GLuint vertexShader = addShader_(GL_VERTEX_SHADER, vertexShaderSourceFileName, result);
// 	GLuint fragmentShader = addShader_(GL_FRAGMENT_SHADER, fragmentShaderSourceFileName, result);

// 	glLinkProgram(result.program);

// 	GLint linkSuccess = 0;
// 	glGetProgramiv(result.program, GL_LINK_STATUS, &linkSuccess);

// 	if (linkSuccess == GL_FALSE) {
// 		GLchar errorBuffer[1024];
// 		GLsizei errorLength = 0;
// 		glGetProgramInfoLog(result.program, (GLsizei)arrayCount(errorBuffer), &errorLength, errorBuffer);
// 		fprintf(stderr, "Failed to link shader: %s\n", errorBuffer);
// 		assert(false);
// 	}

// 	glDetachShader(result.program, vertexShader);
// 	glDetachShader(result.program, fragmentShader);

// 	glDeleteShader(vertexShader);
// 	glDeleteShader(fragmentShader);

// 	glUseProgram(result.program);
// 	GLint windowSizeUniformLocation = glGetUniformLocation(result.program, "twoOverScreenSize");
// 	glUniform2f(windowSizeUniformLocation, (GLfloat)(2.0/windowSize.x), (GLfloat)(2.0/windowSize.y));

// 	result.tintUniformLocation = glGetUniformLocation(result.program, "tint");

// 	return result;
// }

Texture createTexFromSurface(SDL_Surface* image, GameState* gameState) {
	Texture result = {};

	// glEnable(GL_TEXTURE_2D);

	// glGenTextures(1, &result.texId);
	// assert(result.texId);

	// glBindTexture(GL_TEXTURE_2D, result.texId);

	// glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	// glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	// glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	// glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	// glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image->w, image->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, image->pixels);

	// result.uv = r2(v2(0, 0), v2(1, 1));
	// result.size = v2 (image->w, image->h) / gameState->pixelsPerMeter;

	// SDL_FreeSurface(image);
	// glBindTexture(GL_TEXTURE_2D, 0);

	result.srcRect.w = image->w;
	result.srcRect.h = image->h;

	result.tex = SDL_CreateTextureFromSurface(gameState->renderer, image);
	assert(result.tex);

	SDL_SetTextureBlendMode(result.tex, SDL_BLENDMODE_BLEND);
	SDL_FreeSurface(image);

	return result;
}

Texture loadPNGTexture(GameState* gameState, char* fileName) {
	SDL_Surface* image = IMG_Load(fileName);
	if (!image) fprintf(stderr, "Failed to load image from: %s\n", fileName);
	assert(image);

	Texture result = createTexFromSurface(image, gameState);

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

	int texWidth = tex.srcRect.w;//(int)(tex.size.x * gameState->pixelsPerMeter);
	int texHeight = tex.srcRect.h;//(int)(tex.size.y * gameState->pixelsPerMeter);

	int numCols = texWidth / (frameWidth + frameSpacing);
	int numRows = texHeight / (frameHeight + frameSpacing);

	assert(frameWidth > 0);
	assert(frameHeight > 0);

	//TODO: Add a way to test this
	//assert(width % numCols == 0);
	//assert(height % numRows == 0);

	*numFrames = numRows * numCols;
	Texture* result = (Texture*)pushArray(&gameState->permanentStorage, Texture, *numFrames);

	// V2 frameSize = v2((double)(frameWidth + frameSpacing) / (double)texWidth, 
	// 				  (double)(frameHeight + frameSpacing) / (double)texHeight);

	// V2 texSize = v2((double)frameWidth / (double)texWidth, (double)frameHeight/(double)texHeight);

	// V2 frameOffset = v2(frameSpacing / (double)texWidth, frameSpacing / (double)texHeight) / 2;

	for (int rowIndex = 0; rowIndex < numRows; rowIndex++) {
		for (int colIndex = 0; colIndex < numCols; colIndex++) {
			int textureIndex = colIndex + rowIndex * numCols;
			Texture* frame = result + textureIndex;

			//frame->texId = tex.texId;
			frame->tex = tex.tex;

			SDL_Rect* rect = &frame->srcRect;
 
			rect->x = colIndex * (frameWidth + frameSpacing) + frameSpacing / 2;
			rect->y = rowIndex * (frameHeight + frameSpacing) + frameSpacing / 2;
			rect->w = frameWidth;
			rect->h = frameHeight;

			// V2 minCorner = hadamard(v2(colIndex, rowIndex), frameSize) + frameOffset;

			// frame->uv = r2(minCorner, minCorner + texSize);
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

double getAnimationDuration(Animation* animation) {
	double result = animation->numFrames * animation->secondsPerFrame;
	return result;
}

Texture* getAnimationFrame(Animation* animation, double animTime, bool reverse = false) {
	int frameIndex = frameIndex = (int)(animTime / animation->secondsPerFrame) % animation->numFrames;

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

// void setColor(Shader shader, double r, double g, double b, double a) {
// 	glColor4f((GLfloat)r, (GLfloat)g, (GLfloat)b, (GLfloat)a);
// 	glUniform4f(shader.tintUniformLocation, (GLfloat)r, (GLfloat)g, (GLfloat)b, (GLfloat)a);
// }

void setColor(SDL_Renderer* renderer, int r, int g, int b, int a) {
	assert(r >= 0 && r <= 255);
	assert(g >= 0 && g <= 255);
	assert(b >= 0 && b <= 255);
	assert(a >= 0 && a <= 255);
	SDL_SetRenderDrawColor(renderer, r, g, b, a);
}

// void addTexCoord(V2 uvMin, V2 uvMax, Rotation rot, int offset) {
// 	rot = (Rotation)(((int)rot + offset) % RotationCount);

// 	switch(rot) {
// 		case Degree0:
// 			 glTexCoord2f((GLfloat)uvMax.x, (GLfloat)uvMax.y); 
// 			 break;
// 		case Degree90:
// 			 glTexCoord2f((GLfloat)uvMin.x, (GLfloat)uvMax.y); 
// 			 break;
// 		case Degree180:
// 			 glTexCoord2f((GLfloat)uvMin.x, (GLfloat)uvMin.y);	
// 			 break;
// 		case Degree270:
// 			 glTexCoord2f((GLfloat)uvMax.x, (GLfloat)uvMin.y);
// 			 break;
// 		default:
// 			assert(false);
// 	}
// }

SDL_Rect getPixelSpaceRect(GameState* gameState, R2 rect) {
	SDL_Rect result = {};

	double width = getRectWidth(rect);
	double height = getRectHeight(rect);

	result.w = (int)ceil(width * gameState->pixelsPerMeter);
	result.h = (int)ceil(height * gameState->pixelsPerMeter);
	result.x = (int)ceil(rect.max.x * gameState->pixelsPerMeter) - result.w;
	result.y = gameState->windowHeight - (int)ceil(rect.max.y * gameState->pixelsPerMeter);

	return result;
}

void setClipRect(GameState* gameState, R2 rect) {
	SDL_Rect clipRect = getPixelSpaceRect(gameState, rect);
	SDL_RenderSetClipRect(gameState->renderer, &clipRect);
}

void setDefaultClipRect(GameState* gameState) {
	setClipRect(gameState, r2(v2(0, 0), gameState->windowSize));
}

void drawTexture(GameState* gameState, Texture* texture, R2 bounds, bool flipX, double degrees = 0) {
	if (!gameState->doingInitialSim) {
		SDL_Rect dstRect = getPixelSpaceRect(gameState, bounds);
 
		SDL_RendererFlip flip = flipX ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE;
		SDL_RenderCopyEx(gameState->renderer, texture->tex, &texture->srcRect, &dstRect, degrees, NULL, flip);

		// glEnable(GL_TEXTURE_2D);
		// glEnable(GL_BLEND);
	 //    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		// glBindTexture(GL_TEXTURE_2D, texture->texId);

		// setColor(gameState->basicShader, 1, 1, 1, 1);

		// assert(texture->texId);

		// V2 uvMin = texture->uv.min;
		// V2 uvMax = texture->uv.max;

		// if (!flipX) {
		// 	double temp = uvMin.x;
		// 	uvMin.x = uvMax.x;
		// 	uvMax.x = temp;
		// }

		// glBegin(GL_QUADS);
		//    	addTexCoord(uvMin, uvMax, rot, 0);
		//     glVertex2f((GLfloat)bounds.min.x, (GLfloat)bounds.min.y);

		//     addTexCoord(uvMin, uvMax, rot, 1);
		//     glVertex2f((GLfloat)bounds.max.x, (GLfloat)bounds.min.y);

		//     addTexCoord(uvMin, uvMax, rot, 2);
		//     glVertex2f((GLfloat)bounds.max.x, (GLfloat)bounds.max.y);

		//     addTexCoord(uvMin, uvMax, rot, 3);
		//     glVertex2f((GLfloat)bounds.min.x, (GLfloat)bounds.max.y);
		// glEnd();

		// glBindTexture(GL_TEXTURE_2D, 0);


	}
}

//TODO: Since the cameraP is in the gameState, this could be used instead
void drawFilledRect(GameState* gameState, R2 rect, V2 cameraP = v2(0, 0)) {
	if (!gameState->doingInitialSim) {

		SDL_Rect dstRect = getPixelSpaceRect(gameState, rect);
		SDL_RenderFillRect(gameState->renderer, &dstRect);
                     
                
                

		// R2 r = translateRect(rect, -cameraP);

		// glEnable(GL_TEXTURE_2D);
		// glEnable(GL_BLEND);
	 //    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		// glBindTexture(GL_TEXTURE_2D, gameState->whiteTexture.texId);

		// glBegin(GL_QUADS);
		// 	glTexCoord2f(1, 1);
		//     glVertex2f((GLfloat)r.min.x, (GLfloat)r.min.y);
			
		// 	glTexCoord2f(0, 1);
		//     glVertex2f((GLfloat)r.max.x, (GLfloat)r.min.y);
			
		// 	glTexCoord2f(0, 0);
		//     glVertex2f((GLfloat)r.max.x, (GLfloat)r.max.y);
			
		// 	glTexCoord2f(1, 0);
		//     glVertex2f((GLfloat)r.min.x, (GLfloat)r.max.y);
		// glEnd();

		// glBindTexture(GL_TEXTURE_2D, 0);

	}
}

void drawRect(GameState* gameState, R2 rect, double thickness, V2 cameraP = v2(0, 0)) {
	V2 halfThickness = v2(thickness, thickness) / 2.0f;
	double width = getRectWidth(rect);
	double height = getRectHeight(rect);

	rect = translateRect(rect, cameraP);

	V2 bottomMinCorner = rect.min - halfThickness;
	V2 bottomMaxCorner = v2(rect.max.x, rect.min.y) + halfThickness;

	V2 topMinCorner = v2(rect.min.x, rect.max.y) - halfThickness;
	V2 topMaxCorner = rect.max + halfThickness;

	V2 leftMinCorner = bottomMinCorner + v2(0, thickness);
	V2 leftMaxCorner = topMinCorner + v2(thickness, 0);

	V2 rightMinCorner = bottomMaxCorner - v2(thickness, 0);
	V2 rightMaxCorner = topMaxCorner - v2(0, thickness);

	drawFilledRect(gameState, r2(bottomMinCorner, bottomMaxCorner));
	drawFilledRect(gameState, r2(topMinCorner, topMaxCorner));
	drawFilledRect(gameState, r2(leftMinCorner, leftMaxCorner));
	drawFilledRect(gameState, r2(rightMinCorner, rightMaxCorner));
}

Texture createText(GameState* gameState, TTF_Font* font, char* msg) {
	SDL_Color fontColor = {0, 0, 0, 255};
	SDL_Surface* fontSurface = TTF_RenderText_Blended(font, msg, fontColor);

	Texture result = createTexFromSurface(fontSurface, gameState);

	return result;
}

void drawText(GameState* gameState, Texture* texture, TTF_Font* font, double x, double y, double maxWidth, V2 camera = v2(0, 0)) {
	//V2 defaultSize = texture->size;

	double width = texture->srcRect.w;//min(defaultSize.x, maxWidth);
	double height = texture->srcRect.h;//width * (defaultSize.y / defaultSize.x);

	R2 fontBounds = rectCenterDiameter(v2(x, y), v2(width, height) / gameState->pixelsPerMeter);
	fontBounds = translateRect(fontBounds, -camera);

	drawTexture(gameState, texture, fontBounds, false);
}	

Texture* getGlyph(CachedFont* cachedFont, GameState* gameState, char c) {
	if (!cachedFont->cache[c].tex) {	
		SDL_Color black = {0, 0, 0, 255};

		SDL_Surface* glyphSurface = TTF_RenderGlyph_Blended(cachedFont->font, c, black);
		assert(glyphSurface);

		cachedFont->cache[c] = createTexFromSurface(glyphSurface, gameState);
		assert(cachedFont->cache[c].tex);
	}

	Texture* result = cachedFont->cache + c;
	return result;
}

double getTextWidth(CachedFont* cachedFont, GameState* gameState, char* msg) {
	double result = 0;

	double metersPerPixel = 1.0 / (gameState->pixelsPerMeter * cachedFont->scaleFactor);

	while(*msg) {
		result += getGlyph(cachedFont, gameState, *msg)->srcRect.w * metersPerPixel;
		msg++;
	}

	return result;
}

void drawCachedText(CachedFont* cachedFont, GameState* gameState, char* msg, V2 p) {
	double metersPerPixel = 1.0 / (gameState->pixelsPerMeter * cachedFont->scaleFactor);

	while(*msg) {
		Texture* texture = getGlyph(cachedFont, gameState, *msg);
		V2 size = v2(texture->srcRect.w, texture->srcRect.h) * metersPerPixel;
		R2 bounds = r2(p, p + size);

		drawTexture(gameState, texture, bounds, false);

		p.x += size.x;
		msg++;
	}
}

