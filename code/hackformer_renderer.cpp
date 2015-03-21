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
						int frameWidth, int frameHeight, double secondsPerFrame) {
	Animation result = {};

	assert(secondsPerFrame > 0);
	result.secondsPerFrame = secondsPerFrame;
	result.frames = extractTextures(gameState, fileName, frameWidth, frameHeight, 0, &result.numFrames);

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

	result.w = (int)ceil(width * gameState->pixelsPerMeter + 0.5f);
	result.h = (int)ceil(height * gameState->pixelsPerMeter + 0.5f);
	result.x = (int)ceil(rect.min.x * gameState->pixelsPerMeter);
	result.y = (int)ceil(gameState->windowHeight - rect.max.y * gameState->pixelsPerMeter);

	return result;
}

void drawTexture(GameState* gameState, Texture* texture, R2 bounds, bool flipX) {
	SDL_Rect dstRect = getPixelSpaceRect(gameState, bounds);

	SDL_RendererFlip flip = flipX ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE;
	SDL_RenderCopyEx(gameState->renderer, texture->tex, &texture->srcRect, &dstRect, 0, NULL, flip);
}

void setColor(SDL_Renderer* renderer, int r, int g, int b, int a) {
	SDL_SetRenderDrawColor(renderer, r, g, b, a);
}

//TODO: Since the cameraP is in the gameState, this could be used instead
void drawFilledRect(GameState* gameState, R2 rect, V2 cameraP = v2(0, 0)) {
	R2 r = translateRect(rect, -cameraP);
	SDL_Rect dstRect = getPixelSpaceRect(gameState, r);
	SDL_RenderFillRect(gameState->renderer, &dstRect);
}

void drawRect(GameState* gameState, R2 rect, double thickness, V2 cameraP = v2(0, 0)) {
	V2 halfThickness = v2(thickness, thickness) / 2.0f;
	double width = getRectWidth(rect);
	double height = getRectHeight(rect);

	V2 p1 = rect.min - halfThickness;
	V2 p2 = v2(rect.max.x, rect.min.y) + halfThickness;
	drawFilledRect(gameState, r2(p1, p2), cameraP);

	p1.y += height;
	p2.y += height;
	drawFilledRect(gameState, r2(p1, p2), cameraP);

	p1 = rect.min - halfThickness;
	p2 = v2(rect.min.x, rect.max.y) + halfThickness;
	drawFilledRect(gameState, r2(p1, p2), cameraP);

	p1.x += width;
	p2.x += width;
	drawFilledRect(gameState, r2(p1, p2), cameraP);
}

void drawLine(GameState* gameState, V2 p1, V2 p2) {
	int x1 = (int)(p1.x * gameState->pixelsPerMeter);
	int y1 = (int)(gameState->windowHeight - p1.y * gameState->pixelsPerMeter);
	int x2 = (int)(p2.x * gameState->pixelsPerMeter);
	int y2 = (int)(gameState->windowHeight - p2.y * gameState->pixelsPerMeter);

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

void drawText(GameState* gameState, Texture* texture, TTF_Font* font, double x, double y, double maxWidth, V2 camera = v2(0, 0)) {
	V2 defaultSize = v2((double)texture->srcRect.w, (double)texture->srcRect.h) / gameState->pixelsPerMeter;

	double width = min(defaultSize.x, maxWidth);
	double height = width * (double)texture->srcRect.h / (double)texture->srcRect.w;

	R2 fontBounds = rectCenterDiameter(v2(x, y), v2(width, height));
	fontBounds = translateRect(fontBounds, -camera);

//	SDL_Rect dstRect = getPixelSpaceRect(gameState, fontBounds);

	drawTexture(gameState, texture, fontBounds, false);
}	


