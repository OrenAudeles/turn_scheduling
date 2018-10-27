#ifndef H_SPLIT_MIX_H
#define H_SPLIT_MIX_H
#pragma once

#include "rand_types.h"

struct split_mix_generator_t split_mix_initialize(uint64_t seed);
uint64_t split_mix_next(struct split_mix_generator_t* gen);

#include "split_mix64.inl"

#endif