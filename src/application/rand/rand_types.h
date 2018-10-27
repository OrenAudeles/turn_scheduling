#ifndef H_RAND_TYPES_H
#define H_RAND_TYPES_H
#pragma once
#include <inttypes.h>

typedef struct split_mix_generator_t{
	uint64_t x;
} split_mix_generator_t;

typedef struct xorshiro128plus_generator_t{
	uint64_t s[2];
} xorshiro128plus_generator_t;

#endif