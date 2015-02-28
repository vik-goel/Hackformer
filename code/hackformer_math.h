#include <math.h>

#define PI 3.14159265
#define E  2.71828182

#define max(a, b) a > b ? a : b

struct V2 {
	float x, y;
};

struct R2 {
	V2 min;
    V2 max;
};

//NOTE: V2 operations here

V2 v2(float x, float y) {
	V2 result = {};

	result.x = x;
	result.y = y;

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

//Hadamard product
V2 hadamard(V2 &a, V2& b) {
	V2 result = {};

	result.x = a.x * b.x;
	result.y = a.y * b.y;

	return result;
}

V2 operator*(V2 &a, float b) {
	V2 result = {};

	result.x = a.x * b;
	result.y = a.y * b;

	return result;
}

V2 operator*(float b, V2 &a) {
	V2 result = {};
	result = a * b;
	return result;
}

V2 operator/(V2 &a, float b) {
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

void operator*=(V2 &a, float b) {
	a.x *= b;
	a.y *= b;
}

void operator*=(float b, V2 &a) {
	a *= b;
}

void operator/=(V2 &a, float b) {
	a.x /= b;
	a.y /= b;
}

float lengthSq(V2 &a) {
	float result = a.x * a.x + a.y * a.y;
	return result;
}

float length(V2 &a) {
	float result = sqrt(lengthSq(a));
	return result;
}

V2 normalize(V2 &a) {
	float len = length(a);
	if (len != 0) a /= len;
	return a;
}

float toRadians(float degrees) {
	float result = (float)(degrees * PI / 180.0);
	return result;
}

float toDegrees(float radians) {
	float result = (float)(radians / PI * 180.0);
	return result;
}

V2 getVecInDir(float degrees) {
	float radians = toRadians(degrees);

	V2 result = {};
	result.x = (float)cos(radians);
	result.y = (float)sin(radians);

	return result;
}

float getDir(V2 &a) {
	float result = toDegrees(atan2(a.y, a.x));
	return result;
}

V2 rotate(V2& a, float degrees) {
	V2 result = {};

	float rad = toRadians(degrees);
	float cosAngle = (float)cos(rad);
	float sinAngle = (float)sin(rad);

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

float innerProduct(V2& a, V2& b) {
	float result = a.x * b.x + a.y * b.y;
	return result;
}



//NOTE: R2 operations here

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

float getRectWidth(R2 rect) {
	float result = rect.max.x - rect.min.x;
	return result;
}

float getRectHeight(R2 rect) {
	float result = rect.max.y - rect.min.y;
	return result;
}

R2 subtractFromRect(R2 rect, V2 amt) {
	R2 result = {};

	result.min = rect.min - amt;
	result.max = rect.max - amt;

	return result;
}

void debugPrintRectangle(R2 rect) {
	printf("x: %f, y: %f, width: %f, height: %f\n", rect.min.x, rect.min.y, getRectWidth(rect), getRectHeight(rect));
}

//NOTE: Scalar operations here

float square(float a) {
	return a * a;
}

float clamp(float value, float min, float max) {
	float result = value;

	if (value < min) result = min;
	else if (value > max) result = max;

	return result;
}