inline struct xorshiro128plus_generator_t xorshiro128plus_initialize(uint64_t s0, uint64_t s1){
	struct xorshiro128plus_generator_t gen;
	gen.s[0] = s0;
	gen.s[1] = s1;

	return gen;
}

inline uint64_t xorshiro128plus_next(struct xorshiro128plus_generator_t* gen){
	const uint64_t s0 = gen->s[0];
	uint64_t s1 = gen->s[1];

	const uint64_t ret = s0 + s1;

	s1 ^= s0;
	// rotate_left(x, k) => (x << k) | (x >> (64 - k))
	gen->s[0] = ((s0 << 55) | (s0 >> (64 - 55))) ^ s1 ^ (s1 << 14);
	gen->s[1] = ((s1 << 36) | (s1 >> (64 - 36)));
	
	return ret;
}

inline void xorshiro128plus_jump(struct xorshiro128plus_generator_t* gen){
	static const uint64_t JUMP[] = {0xBEAC0467EBA5FACB, 0xD86B048B86AA9922};

	uint64_t s0 = 0, s1 = 0;
	// Let's just unroll our jump loop
	for (int b = 0; b < 64; ++b){
		if (JUMP[0] & UINT64_C(1) << b){
			s0 ^= gen->s[0];
			s1 ^= gen->s[1];
		}
		xorshiro128plus_next(gen);
	}
	for (int b = 0; b < 64; ++b){
		if (JUMP[1] & UINT64_C(1) << b){
			s0 ^= gen->s[0];
			s1 ^= gen->s[1];
		}
		xorshiro128plus_next(gen);
	}
	
	gen->s[0] = s0;
	gen->s[1] = s1;
}

inline uint64_t xorshiro128plus_range(struct xorshiro128plus_generator_t* gen, uint64_t min, uint64_t max){
	const uint64_t v = xorshiro128plus_next(gen);
	const uint64_t d = max - min;

	return min + d * (v / (double)(0xFFFFFFFFFFFFFFFFUL));
}