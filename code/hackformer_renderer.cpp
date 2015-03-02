SDL_Texture* loadPNGTexture(SDL_Renderer* renderer, char* fileName) {
	SDL_Surface *image = IMG_Load(fileName);
	assert(image);
	SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, image);
	assert(texture);
	SDL_FreeSurface(image);
	SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);

	return texture;
}

void loadPNGTexture(Texture* texture, SDL_Renderer* renderer, char* fileName) {
	texture->tex = loadPNGTexture(renderer, fileName);
	SDL_QueryTexture(texture->tex, NULL, NULL, &texture->srcRect.w, &texture->srcRect.h);
	texture->srcRect.x = texture->srcRect.y = 0;
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

Animation loadAnimation(GameState* gameState, char* fileName, 
						int frameWidth, int frameHeight, float secondsPerFrame) {
	Animation result = {};

	assert(secondsPerFrame > 0);
	result.secondsPerFrame = secondsPerFrame;
	SDL_Texture* tex = loadPNGTexture(gameState->renderer, fileName);

	int width, height;
	SDL_QueryTexture(tex, NULL, NULL, &width, &height);

	int numCols = width / frameWidth;
	int numRows = height / frameHeight;

	assert(frameWidth > 0);
	assert(frameHeight > 0);
	assert(width % numCols == 0);
	assert(height % numRows == 0);

	result.numFrames = numRows * numCols;
	result.frames = (Texture*)pushArray(&gameState->permanentStorage, Texture, result.numFrames);

	for (int rowIndex = 0; rowIndex < numRows; rowIndex++) {
		for (int colIndex = 0; colIndex < numCols; colIndex++) {
			int textureIndex = colIndex + rowIndex * numCols;
			Texture* frame = result.frames + textureIndex;

			frame->tex = tex;

			SDL_Rect* rect = &frame->srcRect;

			rect->x = colIndex * frameWidth;
			rect->y = rowIndex * frameHeight;
			rect->w = frameWidth;
			rect->h = frameHeight;
		}
	}

	return result;
}

void getAnimationFrame(Animation* animation, float animTime, Texture** texture) {
	int frameIndex = (int)(animTime / animation->secondsPerFrame) % animation->numFrames;
	*texture = &animation->frames[frameIndex];
}

SDL_Rect getPixelSpaceRect(R2 rect) {
	SDL_Rect result = {};

	float width = getRectWidth(rect);
	float height = getRectHeight(rect);

	result.w = (int)ceil(width * PIXELS_PER_METER);
	result.h = (int)ceil(height * PIXELS_PER_METER);
	result.x = (int)floor(rect.min.x * PIXELS_PER_METER);
	result.y = (int)floor(WINDOW_HEIGHT - rect.max.y * PIXELS_PER_METER);

	return result;
}

void drawTexture(SDL_Renderer* renderer, Texture* texture, R2 bounds, bool flipX) {
	SDL_Rect dstRect = getPixelSpaceRect(bounds);

	SDL_RendererFlip flip = flipX ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE;
	SDL_RenderCopyEx(renderer, texture->tex, &texture->srcRect, &dstRect, 0, NULL, flip);
}

void drawFilledRect(SDL_Renderer* renderer, R2 rect, V2 cameraP = v2(0, 0)) {
	R2 r = subtractFromRect(rect, cameraP);
	SDL_Rect dstRect = getPixelSpaceRect(r);
	SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
	SDL_RenderFillRect(renderer, &dstRect);
}

void drawLine(SDL_Renderer* renderer, V2 p1, V2 p2) {
	int x1 = (int)(p1.x * PIXELS_PER_METER);
	int y1 = (int)(WINDOW_HEIGHT - p1.y * PIXELS_PER_METER);
	int x2 = (int)(p2.x * PIXELS_PER_METER);
	int y2 = (int)(WINDOW_HEIGHT - p2.y * PIXELS_PER_METER);

	SDL_SetRenderDrawColor(renderer, 255, 0, 255, 255);
	SDL_RenderDrawLine(renderer, x1, y1, x2, y2);
}

void drawPolygon(SDL_Renderer* renderer, V2* vertices, int numVertices, V2 center) {
	for (int vertexIndex = 0; vertexIndex < numVertices; vertexIndex++) {
		V2* p1 = vertices + vertexIndex;
		V2* p2 = vertices + (vertexIndex + 1) % numVertices;
		drawLine(renderer, *p1 + center, *p2 + center);
	}
}

void drawText(SDL_Renderer* renderer, TTF_Font* font, char* msg, float x, float y, V2 camera = v2(0, 0)) {
	SDL_Color fontColor = {};
	SDL_Surface *fontSurface = TTF_RenderText_Blended(font, msg, fontColor);
	assert(fontSurface);
	
	SDL_Texture *fontTexture = SDL_CreateTextureFromSurface(renderer, fontSurface);
	assert(fontTexture);

	SDL_FreeSurface(fontSurface);

	int width, height;
	SDL_QueryTexture(fontTexture, NULL, NULL, &width, &height);

	float widthInMeters = (float)width / (float)PIXELS_PER_METER;
	float heightInMeters = (float)height / (float)PIXELS_PER_METER;

	R2 fontBounds = rectCenterDiameter(v2(x + widthInMeters / 2, y + heightInMeters / 2) - camera, 
									   v2(widthInMeters, heightInMeters));
	SDL_Rect dstRect = getPixelSpaceRect(fontBounds);

	SDL_RenderCopy(renderer, fontTexture, NULL, &dstRect);
}	
