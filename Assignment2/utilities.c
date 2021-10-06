#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>

/* Random float value generator */
float rand_float(unsigned int seed, float min, float max) {
    float rand_float = (float)(rand_r(&seed) % (int)(max - min) + min);
    return rand_float;
}