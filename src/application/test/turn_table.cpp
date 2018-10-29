#include "tests.h"

#include "rand/rand.h"

#include "types.h"
#include "profiler.h"

#include <stdlib.h>
#include <stdio.h>
#include <vector>

namespace turn_table_detail{
	using namespace test_constants;

	struct turn_cycle_t{
		uint16_t action[TURN_CYCLES];
		uint16_t cur_action;
	};

	struct test_data_t{
		using accumulator_v = std::vector<time_accumulator_t>;
		using cycle_v = std::vector<turn_cycle_t>;
		using bucket_v = std::vector<int>;

		accumulator_v  cannonical_accumulator;
		accumulator_v  accumulator;
		cycle_v        turn_cycle;
		bucket_v       bucket[N_BUCKETS + 1];
	};

	test_data_t dataset;

	void make_cannonical_data(xorshiro128plus_generator_t& gen, range_u16_t action_range, range_u16_t speed_range, range_u16_t delay_range);
	void initialize_to_cannonical_data(void);

	xorshiro128plus_generator_t make_generator(void);
	void accumulate_ndx(int ndx);

	void do_action(int ndx);

	int do_turn(void);
	void fill_turn_buckets(void);
	int execute_turn_actions(void);
}


void test::test_turn_tables(void){
	PROFILE(test_start, "Test(Turn Tables)");
	srand(0);

	xorshiro128plus_generator_t crng = turn_table_detail::make_generator();

	// Generate cannonical dataset
	turn_table_detail::make_cannonical_data(crng, {5, 10}, {1, 20}, {10, 30});

	/*{
		PROFILE(test, "Test(Turn Tables) : Single Turn");
		turn_table_detail::initialize_to_cannonical_data();

		int n_actions = 0;
		for( int i = 0; i < 10; ++i ){
			n_actions += turn_table_detail::do_turn();
			printf("Turns: %d | Actions: %d\n", i, n_actions);
		}
	}*/
	{
		PROFILE(test, "Test(Turn Tables) : Start");
		turn_table_detail::initialize_to_cannonical_data();

		{
			PROFILE(loop, "Test(Turn Tables) : Loop");
			int n_actions = 0;
			int turns = 0;
			//int last_actions = 0;
			do {
				//last_actions = n_actions;
				++turns;
				n_actions += turn_table_detail::do_turn();
				//printf("[%3d] #A : %7d + %7d\n", turns, n_actions, n_actions - last_actions);
			} while( n_actions < turn_table_detail::TOTAL_ACTIONS );
			printf("Turns: %d | Actions: %d\n", turns, n_actions);
		}
	}

	(void)crng;
}

namespace turn_table_detail{
	xorshiro128plus_generator_t make_generator(void){
		// make the split_mix generator
		union{
			uint64_t seed;
			uint32_t seed_part[2];
		};
		seed_part[0] = rand();
		seed_part[1] = rand();

		split_mix_generator_t sm_gen = split_mix_initialize(seed);
		uint64_t x128_seeds[2];
		x128_seeds[0] = split_mix_next(&sm_gen);
		x128_seeds[1] = split_mix_next(&sm_gen);

		return xorshiro128plus_initialize(x128_seeds[0], x128_seeds[1]);
	}
	void make_cannonical_data(xorshiro128plus_generator_t& gen, range_u16_t action_range, range_u16_t speed_range, range_u16_t delay_range){
		xorshiro128plus_generator_t *rng = &gen;

		dataset.turn_cycle.reserve(MAX_ACCUMULATORS);
		dataset.cannonical_accumulator.resize(MAX_ACCUMULATORS);
		dataset.accumulator.reserve(MAX_ACCUMULATORS);

		{
			test_data_t::accumulator_v& cannonical_accumulators = dataset.cannonical_accumulator;
			// Reset current
			for( int i = 0; i < MAX_ACCUMULATORS; ++i ) {
				cannonical_accumulators[i].current = 0;
			}
			// Generate speeds
			for( int i = 0; i < MAX_ACCUMULATORS; ++i ) {
				cannonical_accumulators[i].speed = xorshiro128plus_range( rng, speed_range.min, speed_range.max + 1 );
			}
			// Generate delays
			for( int i = 0; i < MAX_ACCUMULATORS; ++i ) {
				cannonical_accumulators[i].next = xorshiro128plus_range( rng, delay_range.min, delay_range.max + 1 );
			}
		}

		for( int i = 0; i < MAX_ACCUMULATORS; ++i ) {
			turn_cycle_t cycle = {0};
			for( int t = 0; t < TURN_CYCLES; ++t ) {
				cycle.action[t] = xorshiro128plus_range(rng, action_range.min, action_range.max + 1);
			}
			dataset.turn_cycle.push_back(cycle);
		}

		dataset.bucket[0].reserve(MAX_ACCUMULATORS);
		for( int i = 1; i < N_BUCKETS + 1; ++i ) {
			dataset.bucket[i].reserve(MAX_ACCUMULATORS / N_BUCKETS + 1);
		}

		// Calculate minimum time slices
		const int min_N = (speed_range.max + action_range.min - 1) / action_range.min;
		printf("Minimum N required = %d\n", min_N);
	}
	void initialize_to_cannonical_data(void){
		for( int i = 0; i < MAX_ACCUMULATORS; ++i ) {
			dataset.turn_cycle[i].cur_action = 0;
		}
		for( int i = 0; i < MAX_ACCUMULATORS; ++i ) {
			dataset.accumulator[i] = dataset.cannonical_accumulator[i];
		}
	}
	void accumulate_ndx(int ndx){
		time_accumulator_t& accum = dataset.accumulator[ndx];

		accum.current += accum.speed;
	}
	int get_bucket(int i){
		const time_accumulator_t& ta = dataset.accumulator[i];
		const int diff = ta.current - ta.next;
		//const int diff_buckets = diff * N_BUCKETS;

		int bucket = 0;

		if( (ta.speed > 0) & (diff >= 0) ) {
			bucket = 1 + (diff * N_BUCKETS) / ta.speed;
		}
		/*
		for( ; bucket < N_BUCKETS + 1; ++bucket ) {
			if( diff_buckets < (bucket * ta.speed) ) {
				break;
			}
		}
		*/

		return bucket;
	}
	void fill_turn_buckets(void){
		PROFILE(fill, "Fill Buckets");
		// Clear buckets
		for( int i = 0; i < N_BUCKETS + 1; ++i ) {
			dataset.bucket[i].clear();
		}

		for( int i = 0; i < MAX_ACCUMULATORS; ++i ) {
			dataset.bucket[get_bucket(i)].push_back(i);
			//dataset.bucket[dataset.bucket_assignment[i]].push_back(i);
		}
	}
	int execute_turn_actions(void){
		PROFILE(exec, "Execute Actions");
		int result = 0;

		for( int i = N_BUCKETS; i > 0; --i ) {
			PROFILE(exec_loop, "Execute Bucket");
			auto& cbucket = dataset.bucket[i];
			//dataset.temp_bucket.clear();
			int sz = cbucket.size();
			{
				PROFILE(act, "Execute Bucket ( Act )");
				for( int ndx = 0; ndx < sz; ++ndx ) {
					++result;
					const int element = cbucket[ndx];
					do_action(element);
					//int bucket = get_bucket(element);

					//sz += (bucket == i);
					//dataset.bucket[bucket].push_back(element);
				}
			}
			{
				PROFILE(push, "Execute Bucket ( Reinsert )");
				for( int ndx = 0; ndx < sz; ++ndx ) {
					const int element = cbucket[ndx];
					int bucket = get_bucket(element);
					dataset.bucket[bucket].push_back(element);
				}
			}
		}

		return result;
	}
	void do_action(int ndx){
		time_accumulator_t& ta = dataset.accumulator[ndx];
		turn_cycle_t& tc = dataset.turn_cycle[ndx];

		ta.current -= ta.next;

		ta.next = tc.action[tc.cur_action];
		tc.cur_action = (tc.cur_action + 1) % TURN_CYCLES;
	}

	void print_buckets(void){
		printf("---BUCKETS---\n");
		for( int i = N_BUCKETS; i >= 0; --i ) {
			printf("%s%3d = %7zu |", i % 4 == 3 ? "\n" : "", i, dataset.bucket[i].size());
		}
		printf("\n");
	}

	int do_turn(void){
		PROFILE(turn, "TURN START");
		// Accumulate energy into each accumulator
		// Implies that characters cannot be buffed to have their
		// action come up sooner within the same turn, nor can the
		// characters be debuffed to lose their action
		{
			PROFILE(accum, "Accumulation");
			for( int i = 0; i < MAX_ACCUMULATORS; ++i ) {
				accumulate_ndx(i);
			}
		}
		fill_turn_buckets();
		//print_buckets();
		return execute_turn_actions();
	}
}