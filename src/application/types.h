#ifndef H_TYPES_H
#define H_TYPES_H
#pragma once

#include <inttypes.h>

struct time_accumulator_t{
	uint16_t speed;
	uint16_t current;
	uint16_t next;
};

struct range_u16_t{
	uint16_t min;
	uint16_t max;
};

#endif