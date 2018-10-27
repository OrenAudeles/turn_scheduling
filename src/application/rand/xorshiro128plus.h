#ifndef H_XORSHIRO128PLUS_H
#define H_XORSHIRO128PLUS_H
#pragma once
#include "rand_types.h"

struct xorshiro128plus_generator_t xorshiro128plus_initialize(uint64_t s0, uint64_t s1);
uint64_t xorshiro128plus_next(struct xorshiro128plus_generator_t* gen);
void xorshiro128plus_jump(struct xorshiro128plus_generator_t* gen);

uint64_t xorshiro128plus_range(struct xorshiro128plus_generator_t* gen, uint64_t min, uint64_t max);

#include "xorshiro128plus.inl"
#endif