SDL_Texture* loadPNGTexture_(SDL_Renderer* renderer, char* fileName) {
	SDL_Surface *image = IMG_Load(fileName);
	assert(image);
	SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, image);
	assert(texture);
	SDL_FreeSurface(image);
	SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);

	return texture;
}

Texture loadPNGTexture(SDL_Renderer* renderer, char* fileName) {
	Texture result = {};

	result.tex = loadPNGTexture_(renderer, fileName);
	SDL_QueryTexture(result.tex, NULL, NULL, &result.srcRect.w, &result.srcRect.h);

	return result;
}

TTF_Font* loadFont(char* fileName, int fontSize) {
	TTF_Font *font = TTF_OpenFont(fileName, fontSize);

	if (!font) {
		fprintf(stderr, "Failed to load font: ");
		fprintf(stderr, fileName);
		assert(false);
	}

	return font;
}

Texture* extractTextures(GameState* gameState, char* fileName, 
						int frameWidth, int frameHeight, int frameSpacing, uint* numFrames) {

	SDL_Texture* tex = loadPNGTexture_(gameState->renderer, fileName);

	int width, height;
	SDL_QueryTexture(tex, NULL, NULL, &width, &height);

	int numCols = width / (frameWidth + frameSpacing);
	int numRows = height / (frameHeight + frameSpacing);

	assert(frameWidth > 0);
	assert(frameHeight > 0);

	//TODO: Add a way to test this
	//assert(width % numCols == 0);
	//assert(height % numRows == 0);

	*numFrames = numRows * numCols;
	Texture* result = (Texture*)pushArray(&gameState->permanentStorage, Texture, *numFrames);

	for (int rowIndex = 0; rowIndex < numRows; rowIndex++) {
		for (int colIndex = 0; colIndex < numCols; colIndex++) {
			int textureIndex = colIndex + rowIndex * numCols;
			Texture* frame = result + textureIndex;

			frame->tex = tex;

			SDL_Rect* rect = &frame->srcRect;

			rect->x = colIndex * (frameWidth + frameSpacing);
			rect->y = rowIndex * (frameHeight + frameSpacing);
			rect->w = frameWidth;
			rect->h = frameHeight;
		}
	}

	return result;
}

Animation loadAnimation(GameState* gameState, char* fileName, 
						int frameWidth, int frameHeight, float secondsPerFrame) {
	Animation result = {};

	assert(secondsPerFrame > 0);
	result.secondsPerFrame = secondsPerFrame;
	result.frames = extractTextures(gameState, fileName, frameWidth, frameHeight, 0, &result.numFrames);

	return result;
}

Texture* getAnimationFrame(Animation* animation, float animTime) {
	int frameIndex = (int)(animTime / animation->secondsPerFrame) % animation->numFrames;
	Texture* result = animation->frames + frameIndex;
	return result;
}

float getAnimationDuration(Animation* animation) {
	float result = animation->numFrames * animation->secondsPerFrame;
	return result;
}

SDL_Rect getPixelSpaceRect(GameState* gameState, R2 rect) {
	SDL_Rect result = {};

	float width = getRectWidth(rect);
	float height = getRectHeight(rect);

	result.w = (int)ceil(width * gameState->pixelsPerMeter);
	result.h = (int)ceil(height * gameState->pixelsPerMeter);
	result.x = (int)ceil(rect.min.x * gameState->pixelsPerMeter);
	result.y = (int)ceil(gameState->windowHeight - rect.max.y * gameState->pixelsPerMeter);

	return result;
}

void drawTexture(GameState* gameState, Texture* texture, R2 bounds, bool flipX) {
	SDL_Rect dstRect = getPixelSpaceRect(gameState, bounds);

	SDL_RendererFlip flip = flipX ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE;
	SDL_RenderCopyEx(gameState->renderer, texture->tex, &texture->srcRect, &dstRect, 0, NULL, flip);
}

void drawFilledRect(GameState* gameState, R2 rect, V2 cameraP = v2(0, 0)) {
	R2 r = translateRect(rect, -cameraP);
	SDL_Rect dstRect = getPixelSpaceRect(gameState, r);
	SDL_SetRenderDrawColor(gameState->renderer, 0, 255, 0, 255);
	SDL_RenderFillRect(gameState->renderer, &dstRect);
}

void drawLine(GameState* gameState, V2 p1, V2 p2) {
	int x1 = (int)(p1.x * gameState->pixelsPerMeter);
	int y1 = (int)(gameState->windowHeight - p1.y * gameState->pixelsPerMeter);
	int x2 = (int)(p2.x * gameState->pixelsPerMeter);
	int y2 = (int)(gameState->windowHeight - p2.y * gameState->pixelsPerMeter);

	SDL_SetRenderDrawColor(gameState->renderer, 255, 0, 255, 255);
	SDL_RenderDrawLine(gameState->renderer, x1, y1, x2, y2);
}

void drawPolygon(GameState* gameState, V2* vertices, int numVertices, V2 center) {
	for (int vertexIndex = 0; vertexIndex < numVertices; vertexIndex++) {
		V2* p1 = vertices + vertexIndex;
		V2* p2 = vertices + (vertexIndex + 1) % numVertices;
		drawLine(gameState, *p1 + center, *p2 + center);
	}
}

Texture createText(GameState* gameState, TTF_Font* font, char* msg) {
	Texture result = {};

	SDL_Color fontColor = {};
	SDL_Surface *fontSurface = TTF_RenderText_Blended(font, msg, fontColor);
	assert(fontSurface);
	
	result.tex = SDL_CreateTextureFromSurface(gameState->renderer, fontSurface);
	assert(result.tex);

	SDL_FreeSurface(fontSurface);

	SDL_QueryTexture(result.tex, NULL, NULL, &result.srcRect.w, &result.srcRect.h);

	return result;
}

void drawText(GameState* gameState, TTF_Font* font, char* msg, float x, float y, V2 camera = v2(0, 0)) {
	Texture texture = createText(gameState, font, msg);

	float widthInMeters = (float)texture.srcRect.w / (float)gameState->pixelsPerMeter;
	float heightInMeters = (float)texture.srcRect.h / (float)gameState->pixelsPerMeter;

	R2 fontBounds = rectCenterDiameter(v2(x + widthInMeters / 2, y + heightInMeters / 2) - camera, 
									   v2(widthInMeters, heightInMeters));
	SDL_Rect dstRect = getPixelSpaceRect(gameState, fontBounds);

	SDL_RenderCopy(gameState->renderer, texture.tex, NULL, &dstRect);
}	
