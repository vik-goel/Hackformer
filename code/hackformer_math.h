#define PI 3.14159265
#define E  2.71828182
#define SQRT2 1.41421356237

#define max(a, b) a > b ? a : b
#define min(a, b) a < b ? a : b

struct V2 {
	double x, y;
};

struct R2 {
	V2 min;
    V2 max;
};

//NOTE: Scalar operations here

double square(double a) {
	return a * a;
}

double clamp(double value, double min, double max) {
	double result = value;

	if (value < min) result = min;
	else if (value > max) result = max;

	return result;
}

//NOTE: V2 operations here

V2 v2(double x, double y) {
	V2 result = {};

	result.x = x;
	result.y = y;

	return result;
}

bool operator==(V2 &a, V2 &b) {
	bool result = a.x == b.x && a.y == b.y;
	return result;
}

bool operator!=(V2 &a, V2 &b) {
	bool result = !(a == b);
	return result;
}

V2 operator+(V2 &a, V2 &b) {
	V2 result = {};

	result.x = a.x + b.x;
	result.y = a.y + b.y;

	return result;
}

V2 operator-(V2 &a) {
	V2 result = {};

	result.x = -a.x;
	result.y = -a.y;

	return result;
}

V2 operator-(V2 &a, V2 &b) {
	V2 result = {};

	result.x = a.x - b.x;
	result.y = a.y - b.y;

	return result;
}

V2 hadamard(V2 &a, V2& b) {
	V2 result = {};

	result.x = a.x * b.x;
	result.y = a.y * b.y;

	return result;
}

V2 operator*(V2 &a, double b) {
	V2 result = {};

	result.x = a.x * b;
	result.y = a.y * b;

	return result;
}

V2 operator*(double b, V2 &a) {
	V2 result = {};
	result = a * b;
	return result;
}

V2 operator/(V2 &a, double b) {
	V2 result = {};

	result.x = a.x / b;
	result.y = a.y / b;

	return result;
}

void operator+=(V2 &a, V2 &b) {
	a.x += b.x;
	a.y += b.y;
}

void operator-=(V2 &a, V2 &b) {
	a.x -= b.x;
	a.y -= b.y;
}

void operator*=(V2 &a, double b) {
	a.x *= b;
	a.y *= b;
}

void operator*=(double b, V2 &a) {
	a *= b;
}

void operator/=(V2 &a, double b) {
	a.x /= b;
	a.y /= b;
}

double lengthSq(V2 &a) {
	double result = a.x * a.x + a.y * a.y;
	return result;
}

double length(V2 &a) {
	double result = sqrt(lengthSq(a));
	return result;
}

double dst(V2 a, V2 b) {
	double result = length(b - a);
	return result;
}

double dstSq(V2 a, V2 b) {
	double result = lengthSq(b - a);
	return result;
}

V2 normalize(V2 &a) {
	double len = length(a);
	if (len != 0) a /= len;
	return a;
}

double toRadians(double degrees) {
	double result = (double)(degrees * PI / 180.0);
	return result;
}

double toDegrees(double radians) {
	double result = (double)(radians / PI * 180.0);
	return result;
}

V2 getVecInDir(double degrees) {
	double radians = toRadians(degrees);

	V2 result = {};
	result.x = (double)cos(radians);
	result.y = (double)sin(radians);

	return result;
}

double getDir(V2 &a) {
	double result = toDegrees(atan2(a.y, a.x));
	return result;
}

V2 rotate(V2& a, double degrees) {
	V2 result = {};

	double rad = toRadians(degrees);
	double cosAngle = (double)cos(rad);
	double sinAngle = (double)sin(rad);

	result.x = a.x * cosAngle - a.y * sinAngle;
    result.y = a.x * sinAngle + a.y * cosAngle;

    return result;
}

V2 rotate90(V2& a) {
	V2 result = {};	

	result.x = -a.y;
	result.y = a.x;

	return result;
}	

double innerProduct(V2& a, V2& b) {
	double result = a.x * b.x + a.y * b.y;
	return result;
}



//NOTE: R2 operations here

R2 r2(V2 p1, V2 p2) {
	R2 result = {};

	result.min = v2(min(p1.x, p2.x), min(p1.y, p2.y));
	result.max = v2(max(p1.x, p2.x), max(p1.y, p2.y));

	return result;
}

R2 rectCenterRadius(V2 center, V2 radius) {
	R2 result = {};

	result.min = center - radius;
	result.max = center + radius;

	return result;
}

R2 rectCenterDiameter(V2 center, V2 diameter) {
	R2 result = {};
	result = rectCenterRadius(center, diameter * 0.5);
	return result;
}

R2 addRadiusTo(R2 a, V2 radius) {
	R2 result = {};
	result.min = (a.min - radius);
	result.max = (a.max + radius);
	return result;
}

R2 addDiameterTo(R2 a, V2 diameter) {
	R2 result = {};
	result = addRadiusTo(a, diameter * 0.5);
	return result;
}

double getRectWidth(R2 rect) {
	double result = rect.max.x - rect.min.x;
	return result;
}

double getRectHeight(R2 rect) {
	double result = rect.max.y - rect.min.y;
	return result;
}

V2 getRectCenter(R2 rect) {
	V2 result = {};
	result = (rect.min + rect.max) / 2;
	return result;
}

R2 translateRect(R2 rect, V2 amt) {
	R2 result = {};

	result.min = rect.min + amt;
	result.max = rect.max + amt;

	return result;
}

void debugPrintRectangle(R2 rect) {
	printf("x: %f, y: %f, width: %f, height: %f\n", 
		    rect.min.x, rect.min.y, getRectWidth(rect), getRectHeight(rect));
}

bool pointInsideRect(R2 rect, V2 point) {
	bool result = point.x >= rect.min.x &&
				  point.y >= rect.min.y &&
				  point.x <= rect.max.x &&
				  point.y <= rect.max.y;
				  
	return result;
}

bool rectanglesOverlap(R2 a, R2 b) {
	bool result =  (a.min.x < b.max.x && 
					a.max.x > b.min.x &&
   				    a.min.y < b.max.y && 
   				    a.max.y > b.min.y);

 	return result;
}



//NOTE: Polygonal operations here

V2 getPolygonCenter(V2* vertices, int verticesCount) {
	assert(vertices);
	assert(verticesCount > 0);

	V2 result = {};

	for (int vertexIndex = 0; vertexIndex < verticesCount; vertexIndex++) {
		result += vertices[vertexIndex];
	}

	result /= (double)verticesCount;

	return result;
}

double raycastLine(V2 p, V2 dP, V2 lp1, V2 lp2) {
	double time = 100000;

	if (lp2.x == lp1.x) { //Vertical Line
		if (dP.x == 0) { //Not moving horizontally
			//TODO: If on line then time should equal 0
		} else {
			time = (lp2.x - p.x) / dP.x;
		}
	} else {
		double m = (lp2.y - lp1.y) / (lp2.x - lp1.x);
		double b = lp1.y - m * lp1.x;

		double denominator = dP.y - m * dP.x;

		if (denominator == 0) { //Not moving at all or moving colinearly to the line
			//TODO: If on line then time should equal 0
		} else {
			time = (m * p.x + b - p.y) / denominator;
		}
	}

	double result = -1;

	if (time >= 0) {
		V2 newP = p + time * dP;
		if (pointInsideRect(r2(lp1, lp2), newP)) {
			//TODO: Do not collide if the result is 0, but we are trying to escape
			result = time;
		}
	}

	return result;
}

void addPolygons(V2 translation, V2* a, int aCount, V2* b, int bCount, V2* dst, int dstSize, int* dstCount) {
	int minkowskiSumCount = aCount * bCount;
	int numDuplicatePoints = 0;
	//TODO: Use transient storage
	V2* minkowskiSum = (V2*)malloc(sizeof(V2) * minkowskiSumCount);

	for(int aIndex = 0; aIndex < aCount; aIndex++) {
		for(int bIndex = 0; bIndex < bCount; bIndex++) {
			int pointIndex = bIndex + aIndex * aCount - numDuplicatePoints;
			V2 point = a[aIndex] + b[bIndex] + translation;

			bool addPoint = true;

			for (int testIndex = 0; testIndex < pointIndex && addPoint; testIndex++) {
				if (minkowskiSum[testIndex] == point) addPoint = false;
			}

			if (addPoint) {
				minkowskiSum[pointIndex] = point;
			} else {
				numDuplicatePoints++;
			}
		}
	}

	minkowskiSumCount -= numDuplicatePoints;

	double lowestX = 5000000000;
	int currentPointIndex, firstPointIndex;

	for (int pointIndex = 0; pointIndex < minkowskiSumCount; pointIndex++) {
		V2* point = minkowskiSum + pointIndex;

		if (point->x < lowestX) {
			lowestX = point->x;
			*dst = *point;
			currentPointIndex = firstPointIndex = pointIndex;
		}
	}

	*dstCount = 1;

	while (true) {
		V2 previousPoint = minkowskiSum[currentPointIndex];
		V2 nextPoint;
		bool foundPoint = false;
		int nextPointIndex = 0;
										
		for (;nextPointIndex < minkowskiSumCount; nextPointIndex++) {
			if(nextPointIndex == currentPointIndex) continue;

			V2 linePoint = minkowskiSum[nextPointIndex];
			bool validPoint = true;

			V2 lineNormal = rotate90(previousPoint - linePoint);
			V2 lineMid = (linePoint + previousPoint) / 2;

			for (int testPointIndex = 0; testPointIndex < minkowskiSumCount && validPoint; testPointIndex++) {
				if(testPointIndex == nextPointIndex || testPointIndex == currentPointIndex) continue;
				V2 testPoint = minkowskiSum[testPointIndex];

				V2 pointToMid = testPoint - lineMid;
				double dot = innerProduct(lineNormal, pointToMid);
				validPoint &= dot >= 0;
			}

			if (validPoint) {
				foundPoint = true;
				nextPoint = linePoint;
				break;
			}
		}

		assert(foundPoint);

		if (nextPointIndex == firstPointIndex) {
			break;
		} else {
			currentPointIndex = nextPointIndex;
			dst[*dstCount] = nextPoint;
			(*dstCount)++;
			assert(*dstCount < dstSize);
		}
	}

	free(minkowskiSum);
	minkowskiSum = NULL;
}
