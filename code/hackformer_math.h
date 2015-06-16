#define PI 3.14159265
#define TAU (2*PI)
#define E  2.71828182
#define SQRT2 1.41421356237

#define max(a, b) ((a) > (b) ? (a) : (b))
#define min(a, b) ((a) < (b) ? (a) : (b))
#define swap(a, b) { auto temp##a##b = (a); (a) = (b); (b) = temp##a##b; }

struct V2 {
	double x, y;
};

struct V3 {
	union {
		struct {
			double x, y;
		};
		V2 xy;
	};

	double z;
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

bool isDegreesBetween(double testAngle, double minAngle, double maxAngle) {
	testAngle = angleIn0360(testAngle);
	minAngle = angleIn0360(minAngle);
	maxAngle = angleIn0360(maxAngle);

	bool result = (testAngle >= minAngle && testAngle <= maxAngle);
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
	bool result = abs(a.x - b.x) < epsilon && abs(a.y - b.y) < epsilon;
	return result;
}

bool operator==(V2 &a, V2 &b) {
	bool result = (a.x == b.x) && (a.y == b.y);
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
	bool result = abs(a.x - b.x) < epsilon && abs(a.y - b.y) < epsilon && abs(a.z - b.z) < epsilon;
	return result;
}

bool operator==(V3 &a, V3 &b) {
	bool result = (a.x == b.x) && (a.y == b.y) && (a.z == b.z);
	return result;
}

bool operator!=(V3 &a, V3 &b) {
	bool result = !(a == b);
	return result;
}

V3 operator+(V3 &a, V3 &b) {
	V3 result = {};

	result.x = a.x + b.x;
	result.y = a.y + b.y;
	result.z = a.z + b.z;

	return result;
}

V3 operator-(V3 &a) {
	V3 result = {};

	result.x = -a.x;
	result.y = -a.y;
	result.z = -a.z;

	return result;
}

V3 operator-(V3 &a, V3 &b) {
	V3 result = {};

	result.x = a.x - b.x;
	result.y = a.y - b.y;
	result.z = a.z - b.z;

	return result;
}

V3 hadamard(V3 &a, V3& b) {
	V3 result = {};

	result.x = a.x * b.x;
	result.y = a.y * b.y;
	result.z = a.z * b.z;

	return result;
}

V3 operator*(V3 &a, double b) {
	V3 result = {};

	result.x = a.x * b;
	result.y = a.y * b;
	result.z = a.z * b;

	return result;
}

V3 operator*(double b, V3 &a) {
	V3 result = {};
	result = a * b;
	return result;
}

void operator+=(V3 &a, V3 &b) {
	a.x += b.x;
	a.y += b.y;
	a.z += b.z;
}

void operator-=(V3 &a, V3 &b) {
	a.x -= b.x;
	a.y -= b.y;
	a.z -= b.z;
}

void operator*=(V3 &a, double b) {
	a.x *= b;
	a.y *= b;
	a.z *= b;
}

void operator*=(double b, V3 &a) {
	a *= b;
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

	scaledSize.x = abs(scaledSize.x);
	scaledSize.y = abs(scaledSize.y);

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
