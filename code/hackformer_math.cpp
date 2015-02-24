#include <math.h>

#define PI 3.14159265

#define max(a, b) a > b ? a : b

struct V2 {
	double x, y;
};

V2 operator+(V2 &a, V2 &b) {
	V2 result = {};

	result.x = a.x + b.x;
	result.y = a.y + b.y;

	return result;
}

V2 operator-(V2 &a, V2 &b) {
	V2 result = {};

	result.x = a.x - b.x;
	result.y = a.y - b.y;

	return result;
}

V2 operator-(V2 &a) {
	V2 result = {};

	result.x = -a.x;
	result.y = -a.y;

	return result;
}

//Hadamard product
V2 operator*(V2 &a, V2& b) {
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

	result.x = a.x * b;
	result.y = a.y * b;

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

V2 normalize(V2 &a) {
	double len = length(a);
	if (len != 0) a /= len;
	return a;
}

double toRadians(double degrees) {
	double result = degrees * PI / 180.0;
	return result;
}

double toDegrees(double radians) {
	double result = radians / PI * 180.0;
	return result;
}

V2 getVecInDir(double degrees) {
	double radians = toRadians(degrees);

	V2 result = {};
	result.x = cos(radians);
	result.y = sin(radians);

	return result;
}

double getDir(V2 &a) {
	double result = toDegrees(atan2(a.y, a.x));
	return result;
}

V2 rotate(V2& a, double degrees) {
	V2 result = {};

	double rad = toRadians(degrees);
	double cosAngle = cos(rad);
	double sinAngle = sin(rad);

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