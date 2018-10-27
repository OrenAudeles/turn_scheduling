inline struct split_mix_generator_t split_mix_initialize(uint64_t seed){
	struct split_mix_generator_t ret;
	ret.x = seed;
	return ret;
}
inline uint64_t split_mix_next(struct split_mix_generator_t* gen){
	uint64_t z = (gen->x += UINT64_C(0x9E3779B97F4A7C15));
	z = (z ^ (z >> 30)) * UINT64_C(0xBF58476D1CE4E5B9);
	z = (z ^ (z >> 27)) * UINT64_C(0x94D049BB133111EB);
	return z ^ (z >> 31);
}