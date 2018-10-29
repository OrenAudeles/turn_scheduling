#pragma once

namespace test{
	void test_turn_tables(void);
	void test_no_accumulation(void);
}

namespace test_constants{
	constexpr int MAX_ACCUMULATORS = 1000000;//4096 << 8;
	constexpr int TOTAL_ACTIONS = 80000000;
	constexpr int N_BUCKETS = 4;
	constexpr int TURN_CYCLES = 32;
}