#ifndef HACKFORMER_MATH_H
#define HACKFORMER_MATH_H

#include <math.h>

#define PI 3.14159265
#define E  2.71828182

#define max(a, b) a > b ? a : b
#define min(a, b) a < b ? a : b

struct V2 {
	float x, y;
};

struct R2 {
	V2 min;
    V2 max;
};

#endif