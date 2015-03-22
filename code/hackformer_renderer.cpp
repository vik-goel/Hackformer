Texture defaultTexture() {
	Texture result = {};
	glEnable(GL_TEXTURE_2D);
	glGenTextures(1, &result.texId);
	result.uv = r2(v2(0, 0), v2(1, 1));
	return result;
}

Texture createTexFromSurface(SDL_Surface* image, GameState* gameState) {
	Texture result = {};

	assert(image);

	glEnable(GL_TEXTURE_2D);

	result = defaultTexture();
	glBindTexture(GL_TEXTURE_2D, result.texId);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image->w, image->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, image->pixels);
	 
	result.size = v2 (image->w, image->h) / gameState->pixelsPerMeter;

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
	int frameWidth, int frameHeight, int frameSpacing, uint* numFrames,
	int texWidth, int texHeight) {

	Texture tex = loadPNGTexture(gameState, fileName);

	int numCols = texWidth / (frameWidth + frameSpacing);
	int numRows = texHeight / (frameHeight + frameSpacing);

	assert(frameWidth > 0);
	assert(frameHeight > 0);

	//TODO: Add a way to test this
	//assert(width % numCols == 0);
	//assert(height % numRows == 0);

	*numFrames = numRows * numCols;
	Texture* result = (Texture*)pushArray(&gameState->permanentStorage, Texture, *numFrames);

	V2 frameSize = v2((double)(frameWidth + frameSpacing) /(double) texWidth, 
					  (double)(frameHeight + frameSpacing) / (double)texHeight);

	for (int rowIndex = 0; rowIndex < numRows; rowIndex++) {
		for (int colIndex = 0; colIndex < numCols; colIndex++) {
			int textureIndex = colIndex + rowIndex * numCols;
			Texture* frame = result + textureIndex;

			frame->texId = tex.texId;

			V2 minCorner = v2((colIndex * (frameWidth + frameSpacing)) / (double)texWidth,
				    	      (rowIndex * (frameHeight + frameSpacing)) / (double)texHeight);

			frame->uv = r2(minCorner, minCorner + frameSize);
		}
	}

	return result;
}

Animation loadAnimation(GameState* gameState, char* fileName, 
	int frameWidth, int frameHeight, double secondsPerFrame,
	int texWidth, int texHeight) {
	Animation result = {};

	assert(secondsPerFrame > 0);
	result.secondsPerFrame = secondsPerFrame;
	result.frames = extractTextures(gameState, fileName, frameWidth, frameHeight, 0, &result.numFrames, texWidth, texHeight);

	return result;
}

Texture* getAnimationFrame(Animation* animation, double animTime) {
	int frameIndex = (int)(animTime / animation->secondsPerFrame) % animation->numFrames;
	Texture* result = animation->frames + frameIndex;
	return result;
}

double getAnimationDuration(Animation* animation) {
	double result = animation->numFrames * animation->secondsPerFrame;
	return result;
}

SDL_Rect getPixelSpaceRect(GameState* gameState, R2 rect) {
	SDL_Rect result = {};

	double width = getRectWidth(rect);
	double height = getRectHeight(rect);

	result.w = (int)ceil(width * gameState->pixelsPerMeter);
	result.h = (int)ceil(height * gameState->pixelsPerMeter);
	result.x = (int)ceil(rect.max.x * gameState->pixelsPerMeter) - result.w;
	result.y = (int)ceil(rect.max.y * gameState->pixelsPerMeter) - result.h;

	return result;
}

void addTexCoord(V2 uvMin, V2 uvMax, Rotation rot, int offset) {
	rot = (Rotation)(((int)rot + offset) % RotationCount);

	switch(rot) {
		case Degree0:
			 glTexCoord2f((GLfloat)uvMax.x, (GLfloat)uvMax.y); 
			 break;
		case Degree90:
			 glTexCoord2f((GLfloat)uvMin.x, (GLfloat)uvMax.y); 
			 break;
		case Degree180:
			 glTexCoord2f((GLfloat)uvMin.x, (GLfloat)uvMin.y); 
			 break;
		case Degree270:
			 glTexCoord2f((GLfloat)uvMax.x, (GLfloat)uvMin.y);
			 break;
	}
}

void drawTexture(GameState* gameState, Texture* texture, R2 bounds, bool flipX, Rotation rot = Degree0) {
	SDL_Rect dstRect = getPixelSpaceRect(gameState, bounds);

	glEnable(GL_TEXTURE_2D);
	glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glBindTexture(GL_TEXTURE_2D, texture->texId);

	glColor4f(1, 1, 1, 1);

	assert(texture->texId);

	V2 uvMin = texture->uv.min;
	V2 uvMax = texture->uv.max;

	if (!flipX) {
		double temp = uvMin.x;
		uvMin.x = uvMax.x;
		uvMax.x = temp;
	}

	glBegin(GL_QUADS);
	   	addTexCoord(uvMin, uvMax, rot, 0);
	    glVertex2f((GLfloat)bounds.min.x, (GLfloat)bounds.min.y);

	    addTexCoord(uvMin, uvMax, rot, 1);
	    glVertex2f((GLfloat)bounds.max.x, (GLfloat)bounds.min.y);

	    addTexCoord(uvMin, uvMax, rot, 2);
	    glVertex2f((GLfloat)bounds.max.x, (GLfloat)bounds.max.y);

	    addTexCoord(uvMin, uvMax, rot, 3);
	    glVertex2f((GLfloat)bounds.min.x, (GLfloat)bounds.max.y);
	glEnd();
}

//TODO: Since the cameraP is in the gameState, this could be used instead
void drawFilledRect(GameState* gameState, R2 rect, V2 cameraP = v2(0, 0)) {
	R2 r = translateRect(rect, -cameraP);

	glDisable(GL_TEXTURE_2D);

	glBegin(GL_QUADS);
	    glVertex2f((GLfloat)r.min.x, (GLfloat)r.min.y);
	    glVertex2f((GLfloat)r.max.x, (GLfloat)r.min.y);
	    glVertex2f((GLfloat)r.max.x, (GLfloat)r.max.y);
	    glVertex2f((GLfloat)r.min.x, (GLfloat)r.max.y);
	glEnd();
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
	V2 defaultSize = texture->size;
	//v2((double)texture->srcRect.w, (double)texture->srcRect.h) / gameState->pixelsPerMeter;

	double width = min(defaultSize.x, maxWidth);
	double height = width * (defaultSize.y / defaultSize.x);

	R2 fontBounds = rectCenterDiameter(v2(x, y), v2(width, height));
	fontBounds = translateRect(fontBounds, -camera);

	drawTexture(gameState, texture, fontBounds, false);
}	

Texture* getGlyph(CachedFont* cachedFont, GameState* gameState, char c) {
	if (!cachedFont->cache[c].texId) {	
		SDL_Color black = {0, 0, 0, 255};

		SDL_Surface* glyphSurface = TTF_RenderGlyph_Blended(cachedFont->font, c, black);
		assert(glyphSurface);

		cachedFont->cache[c] = createTexFromSurface(glyphSurface, gameState);
		assert(cachedFont->cache[c].texId);
	}

	Texture* result = cachedFont->cache + c;
	return result;
}

double getTextWidth(CachedFont* cachedFont, GameState* gameState, char* msg) {
	double result = 0;

	double metersPerPixel = 1.0 / (gameState->pixelsPerMeter * cachedFont->scaleFactor);

	while(*msg) {
		result += getGlyph(cachedFont, gameState, *msg)->size.x / cachedFont->scaleFactor;
		msg++;
	}

	return result;
}

void drawCachedText(CachedFont* cachedFont, GameState* gameState, char* msg, V2 p) {
	double metersPerPixel = 1.0 / (gameState->pixelsPerMeter * cachedFont->scaleFactor);

	while(*msg) {
		Texture* texture = getGlyph(cachedFont, gameState, *msg);
		R2 bounds = r2(p, p + texture->size / cachedFont->scaleFactor);

		drawTexture(gameState, texture, bounds, false);

		p.x += texture->size.x / cachedFont->scaleFactor;
		msg++;
	}
}

