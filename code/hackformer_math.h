#define PI 3.14159265359
#define TAU (2.0*PI)
#define E  2.71828182
#define SQRT2 1.41421356237

#define max(a, b) ((a) > (b) ? (a) : (b))
#define min(a, b) ((a) < (b) ? (a) : (b))
#define swap(a, b) { auto tempSwapVariable_ = (a); (a) = (b); (b) = tempSwapVariable_; }

struct V2 {
	double x, y;
    
    bool operator==(V2 b) {
        bool result = (x == b.x) && (y == b.y);
        return result;
    }
    
    bool operator!=(V2 b) {
        bool result = x != b.x || y != b.y;
        return result;
    }
    
    V2 operator+(V2 b) {
        V2 result = {};
        
        result.x = x + b.x;
        result.y = y + b.y;
        
        return result;
    }
    
    V2 operator-() {
        V2 result = {};
        
        result.x = -x;
        result.y = -y;
        
        return result;
    }
    
    V2 operator-(V2 b) {
        V2 result = {};
        
        result.x = x - b.x;
        result.y = y - b.y;
        
        return result;
    }
    
    
    V2 operator*(double b) {
        V2 result = {};
        
        result.x = x * b;
        result.y = y * b;
        
        return result;
    }
    
    void operator+=(V2 b) {
        x += b.x;
        y += b.y;
    }
    
    void operator-=(V2 b) {
        x -= b.x;
        y -= b.y;
    }
    
    void operator*=(double b) {
        x *= b;
        y *= b;
    }
};

void operator*=(double b, V2 a) {
    a *= b;
}

V2 operator*(double b, V2 a) {
    V2 result = {};
    result = a * b;
    return result;
}

struct V3 {
	union {
		struct {
			double x, y;
		};
		V2 xy;
	};

	double z;
    
    bool operator==(V3 b) {
        bool result = (x == b.x) && (y == b.y) && (z == b.z);
        return result;
    }
    
    bool operator!=(V3 b) {
        bool result = x != b.x || y != b.y || z != b.z;
        return result;
    }
    
    V3 operator+(V3 b) {
        V3 result = {};
        
        result.x = x + b.x;
        result.y = y + b.y;
        result.z = z + b.z;
        
        return result;
    }
    
    V3 operator-() {
        V3 result = {};
        
        result.x = -x;
        result.y = -y;
        result.z = -z;
        
        return result;
    }
    
    V3 operator-(V3 b) {
        V3 result = {};
        
        result.x = x - b.x;
        result.y = y - b.y;
        result.z = z - b.z;
        
        return result;
    }
    
    V3 operator*(double b) {
        V3 result = {};
        
        result.x = x * b;
        result.y = y * b;
        result.z = z * b;
        
        return result;
    }
    
    void operator+=(V3 b) {
        x += b.x;
        y += b.y;
        z += b.z;
    }
    
    void operator-=(V3 b) {
        x -= b.x;
        y -= b.y;
        z -= b.z;
    }
    
    void operator*=(double b) {
        x *= b;
        y *= b;
        z *= b;
    }
};

V3 operator*(double b, V3 a) {
    V3 result = {};
    result = a * b;
    return result;
}

void operator*=(double b, V3 a) {
    a *= b;
}

struct R2 {
	V2 min;
    V2 max;
};

//NOTE: Scalar operations here

bool epsilonEquals(double a, double b, double epsilon) {
	bool result = fabs(a - b) <= epsilon;
	return result;
}

double square(double a) {
	return a * a;
}

double clamp(double value, double min, double max, bool* changed = NULL) {
	double result = value;

	if (value < min) {
		result = min;
		if(changed)*changed = true;
	}
	else if (value > max) {
		result = max;
		if(changed)*changed = true;
	}

	return result;
}

double toRadians(double degrees) {
	double result = degrees * (PI / 180.0);
	return result;
}

double toDegrees(double radians) {
	double result = radians * (180.0 / PI);
	return result;
}

double angleIn0360(double angle) {
	double result = angle;

	while(result < 0) result += 360;
	while(result > 360) result -= 360;

	return result;
}

double angleIn0Tau(double angle) {
	double result = angle;

	while(result < 0) result += TAU;
	while(result > TAU) result -= TAU;

	return result;
}

double yReflectDegrees(double angle) {
	double result = angleIn0360(angle);

	if(result <= 180) result = 180 - result;
	else result = 540 - result;
	
	return result;
}

double yReflectRad(double angle) {
	double result = angleIn0Tau(angle);

	if(result <= PI) result = PI - result;
	else result = (3 * PI) - result;
	
	return result;
}

double clampPeriodicBetween(double value, double minValue, double maxValue, bool* changed = NULL) {
	if(maxValue >= minValue) {
		value = clamp(value, minValue, maxValue, changed);
	}
	else if(value < minValue && value > maxValue) {
		double minDst = fabs(value - minValue);
		double maxDst = fabs(value - maxValue);

		if(minDst < maxDst) {
			value = minValue;
			if(changed)*changed = true;
		}
		else {
			value = maxValue;
			if(changed)*changed = true;
		}
	}

	return value;
}

double clampRadiansBetween(double angle, double minAngle, double maxAngle, bool* changed = NULL) {
	angle = angleIn0Tau(angle);
	minAngle = angleIn0Tau(minAngle);
	maxAngle = angleIn0Tau(maxAngle);

	angle = clampPeriodicBetween(angle, minAngle, maxAngle, changed);

	return angle;
}

double clampDegreesBetween(double angle, double minAngle, double maxAngle, bool* changed = NULL) {
	angle = angleIn0360(angle);
	minAngle = angleIn0360(minAngle);
	maxAngle = angleIn0360(maxAngle);

	angle = clampPeriodicBetween(angle, minAngle, maxAngle, changed);

	return angle;
}

bool isDegreesBetween(double testAngle, double minAngle, double maxAngle) {
	testAngle = angleIn0360(testAngle);
	minAngle = angleIn0360(minAngle);
	maxAngle = angleIn0360(maxAngle);

	bool result;

	if(minAngle > maxAngle) {
		result = (testAngle > minAngle || testAngle < maxAngle);
	} else {
		result = (testAngle >= minAngle && testAngle <= maxAngle);
	}

	return result;
}

bool isRadiansBetween(double testAngle, double minAngle, double maxAngle) {
	testAngle = angleIn0Tau(testAngle);
	minAngle = angleIn0Tau(minAngle);
	maxAngle = angleIn0Tau(maxAngle);

	bool result;

	if(minAngle > maxAngle) {
		result = (testAngle > minAngle || testAngle < maxAngle);
	} else {
		result = (testAngle >= minAngle && testAngle <= maxAngle);
	}

	return result;
}

bool rangesOverlap(double min1, double max1, double min2, double max2) {
	assert(max1 >= min1);
	assert(max2 >= min2);

	bool result = (max1 > min2 && min1 < max2) || (max2 > min1 && min2 < max1);
	return result;
}

//NOTE: V2 operations here

V2 v2(double x, double y) {
	V2 result = {};

	result.x = x;
	result.y = y;

	return result;
}

bool epsilonEquals(V2 a, V2 b, double epsilon) {
	bool result = fabs(a.x - b.x) < epsilon && fabs(a.y - b.y) < epsilon;
	return result;
}

V2 hadamard(V2 a, V2 b) {
	V2 result = {};

	result.x = a.x * b.x;
	result.y = a.y * b.y;

	return result;
}

double lengthSq(V2 a) {
	double result = a.x * a.x + a.y * a.y;
	return result;
}

double length(V2 a) {
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

V2 normalize(V2 a) {
	double len = length(a);
	if (len != 0) a *= (1.0 / len);
	return a;
}

double dot(V2 a, V2 b) {
	double result = a.x * b.x + a.y * b.y;
	return result; 
}

//NOTE: This assumes that b is unit length
V2 vectorProjection(V2 a, V2 b) {
	V2 result = dot(a, b) * b;
	return result;
}

void debugPrintV2(V2 v) {
	printf("x: %f, y: %f\n", v.x, v.y);
}

double getDegreesBetween(V2 a, V2 b) {
	V2 v = b - a;
	double result = toDegrees(atan2(v.y, v.x));
	return result;
}

V2 perp(V2 a) {
	V2 result = v2(-a.y, a.x);
	return result;
}

V2 rotate(V2 a, double rad) {
	V2 result = {};

	double cosAngle = cos(rad);
	double sinAngle = sin(rad);

	result.x = a.x * cosAngle - a.y * sinAngle;
	result.y = a.x * sinAngle + a.y * cosAngle;

	return result;
}

double getRad(V2 a) {
	double result = atan2(a.y, a.x);
	return result;
}

double getDegrees(V2 a) {
	double result = toDegrees(getRad(a));
	return result;
}

V2 maxComponents(V2 a, V2 b) {
	V2 result = v2(max(a.x, b.x), max(a.y, b.y));
	return result;
}

V2 minComponents(V2 a, V2 b) {
	V2 result = v2(min(a.x, b.x), min(a.y, b.y));
	return result;
}

V2 swapComponents(V2 a) {
	V2 result = v2(a.y, a.x);
	return result;
}

//NOTE: Random operations here

#define RANDOM_MAX (0x7FFFFFFF)

struct Random {
	u32 seed;
};

Random createRandom(u32 seed) {
	Random result = {};
	result.seed = seed;
	return result;
}

u32 nextRandom(Random* random) {
	random->seed = (214013 * random->seed + 2531011) % RANDOM_MAX;
	return random->seed;
}

double randomBetween(Random* random, double min, double max) {
	double range = max - min;

	u32 value = nextRandom(random);

	double result = value * (range / RANDOM_MAX) + min;
	return result;
}

double randomRadian(Random* random) {
	double result = randomBetween(random, 0, TAU);
	return result;
}

V2 randomUnitV2(Random* random) {
	V2 result;
	#if 0
	result.x = randomBetween(random, -1, 1);
	result.y = randomBetween(random, -1, 1);
	result = normalize(result);
	#endif

	double rad = randomRadian(random);
	result = v2(cos(rad), sin(rad));

	return result;
}

//NOTE: V3 operations here

V3 v3(double x, double y, double z) {
	V3 result = {};

	result.x = x;
	result.y = y;
	result.z = z;

	return result;
}

V3 v3(V2 xy, double z) {
	V3 result = {};

	result.xy = xy;
	result.z = z;

	return result;
}

bool epsilonEquals(V3 a, V3 b, double epsilon) {
	bool result = fabs(a.x - b.x) < epsilon && fabs(a.y - b.y) < epsilon && fabs(a.z - b.z) < epsilon;
	return result;
}

V3 hadamard(V3 a, V3 b) {
    V3 result = {};
    
    result.x = a.x * b.x;
    result.y = a.y * b.y;
    result.z = a.z * b.z;
    
    return result;
}

double lengthSq(V3 a) {
	double result = a.x * a.x + a.y * a.y + a.z * a.z;
	return result;
}

double length(V3 a) {
	double result = sqrt(lengthSq(a));
	return result;
}

double dst(V3 a, V3 b) {
	double result = length(b - a);
	return result;
}

double dstSq(V3 a, V3 b) {
	double result = lengthSq(b - a);
	return result;
}

V3 normalize(V3 a) {
	double len = length(a);
	if (len != 0) a *= (1.0 / len);
	return a;
}

void debugPrintV3(V3 v) {
	printf("x: %f, y: %f, z: %f\n", v.x, v.y, v.z);
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

V2 getRectSize(R2 rect) {
	V2 result = v2(getRectWidth(rect), getRectHeight(rect));
	return result;
}

V2 getRectCenter(R2 rect) {
	V2 result = {};
	result = (rect.min + rect.max) * 0.5;
	return result;
}

R2 translateRect(R2 rect, V2 amt) {
	R2 result = {};

	result.min = rect.min + amt;
	result.max = rect.max + amt;

	return result;
}

R2 scaleRect(R2 rect, V2 amt) {
	V2 originalSize = getRectSize(rect);
	V2 scaledSize = hadamard(originalSize, amt);

	scaledSize.x = fabs(scaledSize.x);
	scaledSize.y = fabs(scaledSize.y);

	V2 center = getRectCenter(rect);

	R2 result = rectCenterDiameter(center, scaledSize);

	return result;
}

V2 clampToRect(V2 a, R2 rect) {
	V2 result = {};

	result.x = clamp(a.x, rect.min.x, rect.max.x);
	result.y = clamp(a.y, rect.min.y, rect.max.y);

	return result;
}

void debugPrintRect(R2 rect) {
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

bool pointInsideRectExclusive(R2 rect, V2 point) {
	double epsilon = 0.00000001;

	bool result = point.x > rect.min.x + epsilon &&
				  point.y > rect.min.y + epsilon &&
				  point.x < rect.max.x - epsilon &&
				  point.y < rect.max.y - epsilon;
				  
	return result;
}

bool rectanglesOverlap(R2 a, R2 b) {
	bool result =  (a.min.x < b.max.x && 
					a.max.x > b.min.x &&
   				    a.min.y < b.max.y && 
   				    a.max.y > b.min.y);

 	return result;
}

R2 intersect(R2 a, R2 b) {
	R2 result = {};

	result.min = maxComponents(a.min, b.min);
	result.max = minComponents(a.max, b.max);

	return result;
}

R2 reCenterRect(R2 a, V2 center) {
	V2 size = getRectSize(a);
	R2 result = rectCenterDiameter(center, size);
	return result;
}
