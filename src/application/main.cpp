#include "types.h"
#include "rand/rand.h"
#include "profiler.h"


#include <cstdlib>
#include <cstdio>
#include <algorithm> // for partial_sort

#include "test/tests.h"

#define USE_QUICKSORT 0

void sort_to_buckets(void);
int do_action_iterate_buckets(void);

constexpr int MAX_ACCUMULATORS = 4096 << 8;
constexpr int TOTAL_ACTIONS = 40000000;
constexpr int N_BUCKETS = 256;

// BEST_N == MAX_ACCUMULATORS is equivalent to reordering the entire acting set.
int BEST_N = MAX_ACCUMULATORS >> 0;
const int MIN_BEST_N = (1 << 8) > (MAX_ACCUMULATORS >> 5) ? (1 << 8) : (MAX_ACCUMULATORS >> 5);

time_accumulator_t accumulators[MAX_ACCUMULATORS] = {0};
time_accumulator_t cannonical_accumulators[MAX_ACCUMULATORS] = {0};

const int TURN_CYCLES = 32;
struct turn_cycle_t{
	uint16_t action[TURN_CYCLES];
	uint16_t cur_action;
};
turn_cycle_t action_cycles[MAX_ACCUMULATORS] = {0};
void reset_turn_cycles(void){
	for( int i = 0; i < MAX_ACCUMULATORS; ++i ) {
		action_cycles[i].cur_action = 0;
	}
}
void initialize_turn_cycles(xorshiro128plus_generator_t& gen, range_u16_t action_range){
	reset_turn_cycles();

	for( int i = 0; i < MAX_ACCUMULATORS; ++i ) {
		turn_cycle_t &cycle = action_cycles[i];
		for( int t = 0; t < TURN_CYCLES; ++t ) {
			cycle.action[t] = xorshiro128plus_range(&gen, action_range.min, action_range.max + 1);
		}
	}
}

void initialize_cannonical_accumulators(xorshiro128plus_generator_t& gen, range_u16_t speed_range, range_u16_t delay_range){
	xorshiro128plus_generator_t *rng = &gen;

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

void initialize_accumulators(void){
	for( int i = 0; i < MAX_ACCUMULATORS; ++i ) {
		accumulators[i] = cannonical_accumulators[i];
	}
}

void print_accumulators(void){
	printf("ACCUMULATORS [%d]\n", MAX_ACCUMULATORS);
	for( int i = 0; i < MAX_ACCUMULATORS; ++i ) {
		const time_accumulator_t *ta = accumulators + i;
		printf(" [%3d] {%3u, %3u, %3u} : Will Act Next = %u\n", i, ta->speed, ta->current, ta->next, ta->current + ta->speed >= ta->next);
	}
}

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

void advance_time(void){
	for( int i = 0; i < MAX_ACCUMULATORS; ++i ) {
		accumulators[i].current += accumulators[i].speed;
	}
}

int turn_actors[MAX_ACCUMULATORS] = {0};
int n_turn_actors = 0;
void get_turn_actors(void){
	n_turn_actors = 0;
	for( int i = 0; i < MAX_ACCUMULATORS; ++i ) {
		if( accumulators[i].current >= accumulators[i].next ) {
			turn_actors[n_turn_actors++] = i;
			// Reduce .next to 0
			// This will take care of needing to ever check for rollover
			accumulators[i].current -= accumulators[i].next;
			accumulators[i].next = 0;
		}
	}
}
int aux_stack[MAX_ACCUMULATORS];

inline void swap_index(int* a, int* b){
	int t = *a;
	*a = *b;
	*b = t;
}

inline bool actor_compare( int a, int b ) {
	const time_accumulator_t* ta_a = accumulators + a;
	const time_accumulator_t* ta_b = accumulators + b;

	const int16_t ta_a_energy = ta_a->current - ta_a->next;
	const int16_t ta_b_energy = ta_b->current - ta_b->next;

	return (ta_a_energy * ta_b->speed) > (ta_b_energy * ta_a->speed);

	//const int32_t ta_a_fractional = (0x0FFFFFFFU * ta_a_energy) / ta_a->speed;
	//const int32_t ta_b_fractional = (0x0FFFFFFFU * ta_b_energy) / ta_b->speed;
	// (ta_a.current / ta_a.speed) <=> (ta_b.current / ta_b.speed)
	// float eval_a = (ta_a_energy / (float)ta_a->speed);
	// float eval_b = (ta_b_energy / (float)ta_b->speed);

	// if(eval_a > eval_b){ return true; }
	// else if (eval_a == eval_b && a < b){ return true; }
	// else{ return false; }
	//return (ta_a_energy / (float)ta_a->speed) > (ta_b_energy / (float)ta_b->speed);
	//return ta_a_fractional > ta_b_fractional;
}

void order_turn_actors(int start, int end){
	auto bubblesort = [&](int start, int end){
		printf("BUBBLE\n");
		for( int i = start; i < end - 1; ++i ) {
			printf("%d: {", i);
			for( int j = start; j < end - 1; ++j){
				printf("%d, ", turn_actors[j]);
			}
			printf("%d} SWAPS:", turn_actors[end - 1]);

			for( int j = start; j < end - i - 1; ++j ) {
				if( actor_compare(turn_actors[j + 1], turn_actors[j]) ) {
					printf("{%d, %d} ", turn_actors[j], turn_actors[j + 1]);
					swap_index(&turn_actors[j], &turn_actors[j + 1]);
				}
			}
			printf("\n");
		}
		printf("BUBBLE[%d - %d]: {", start, end);
		for( int i = start; i < end - 1; ++i){
			printf("%d, ", turn_actors[i]);
		}
		printf("%d}\n", turn_actors[end - 1]);
	};
	
	auto quicksort = [&](int low, int high){
		auto partition = [&](int low, int high){
			int x = turn_actors[high];
			int i = (low - 1);

			for( int j = low; j <= high - 1; ++j){
				if( actor_compare(turn_actors[j], x) ) {
					++i;
					swap_index(&turn_actors[i], &turn_actors[j]);
				}
			}
			swap_index(&turn_actors[i + 1], &turn_actors[high]);
			return (i + 1);
		};

		int top = -1;

		aux_stack[++top] = low;
		aux_stack[++top] = high;

		while(top >= 0){
			high = aux_stack[top--];
			low = aux_stack[top--];

			int p = partition(low, high);
			if( p-1 > low ) {
				aux_stack[++top] = low;
				aux_stack[++top] = p - 1;
			}

			if( p + 1 < high ) {
				aux_stack[++top] = p + 1;
				aux_stack[++top] = high;
			}
		}
	};

#if 0
#if USE_QUICKSORT
	quicksort(start, end);
#else
	bubblesort(start, end);
#endif
#else
	std::sort (turn_actors + start, turn_actors + end, actor_compare);
#endif
	(void)bubblesort;
	(void)quicksort;
}

void order_turn_actors(void){
	order_turn_actors(0, n_turn_actors);
}

int actor_actions[MAX_ACCUMULATORS] = {0};

int do_actions(xorshiro128plus_generator_t& gen, range_u16_t action_range){
	int actions_taken = 0;

	for( int i = 0; i < n_turn_actors; ++i ) {
		time_accumulator_t *actor = accumulators + turn_actors[i];
		do{
			++actor_actions[turn_actors[i]];
			++actions_taken;
			actor->next += xorshiro128plus_range(&gen, action_range.min, action_range.max + 1);
		}while(actor->next < actor->current);
	}
	return actions_taken;
}

int do_actions_reordering(xorshiro128plus_generator_t& gen, range_u16_t action_range){
	int actions_taken = 0;

	for( int i = 0; i < n_turn_actors; ) {
		time_accumulator_t *actor = accumulators + turn_actors[i];

		++actor_actions[turn_actors[i]];
		++actions_taken;

		actor->next += xorshiro128plus_range(&gen, action_range.min, action_range.max + 1);

		if( actor->next < actor->current ) {
			// Need to reorder the actor within remaining actors
			order_turn_actors(i, n_turn_actors);
		}
		else{
			++i;
		}
	}

	return actions_taken;
}

int best_N2(int n, int start, int end){
	int distance = end - start;
	int min = n > distance ? distance : n;

	printf("BEST_N2 [%d, %d, %d] => [%d, %d]\n", n, start, end, start, start + min);

	order_turn_actors(start, start + min);

	for( int i = start + min; i < end; ++i ) {
		// Compare against worst (start + min - 1)
		if( actor_compare(turn_actors[i], turn_actors[start + min - 1]) ) {
			swap_index(&turn_actors[i], &turn_actors[start + min - 1]);
			// resort "best" section
			order_turn_actors(start, start + min);
		}
	}

	return start + min;
}

int best_N(int n, int start, int end){
	int distance = end - start;
	int min = n > distance ? distance : n;

	//printf("BEST_N [%d, %d, %d] => [%d, %d]\n", n, start, end, start, start + min);

	std::partial_sort (turn_actors + start, turn_actors + start + min, turn_actors + end, actor_compare);

	return start + min;
}

void print_subset_by_index(int cycle, int start, int end, const char* note = nullptr){
	if(!note){
		note = "Cycle Start";
	}
	else{
		printf("-->");
	}
	printf("[%3d] SUBSET[%d - %d] [%s]\n", cycle, start, end, note);
	for( int i = start; i < end; ++i ) {
		const time_accumulator_t* ta = accumulators + turn_actors[i];
		printf("  [%3d] (%3d) {%3u, %3u, %3u} <%+f>\n", i, turn_actors[i], ta->speed, ta->current, ta->next, (ta->current - ta->next) / (float)ta->speed);
	}
}

int do_actions_reordering2(xorshiro128plus_generator_t& gen, range_u16_t action_range){
	int actions_taken = 0;

	int i = 0;

	//printf("TURN START [%d]\n", n_turn_actors);
	if(n_turn_actors > 0){
		int cycle = 0;
		do{
			++cycle;
			int best_end = best_N(BEST_N, i, n_turn_actors);
			//printf("  CYCLE START<%02d>[%d, %d] - %d\n", cycle, i, n_turn_actors, best_end);
			//print_subset_by_index(cycle, i, best_end);

			for( ; i < best_end; ) {
				//printf("  : Iteration [%d | %d]\n", i, best_end);
				// Take action with "best"
				time_accumulator_t *actor = accumulators + turn_actors[i];
				++actor_actions[turn_actors[i]];
				++actions_taken;

				turn_cycle_t *tcycle = action_cycles + turn_actors[i];

				//actor->next += xorshiro128plus_range(&gen, action_range.min, action_range.max + 1);
				actor->next += tcycle->action[tcycle->cur_action]; 
				tcycle->cur_action = (tcycle->cur_action + 1) % TURN_CYCLES;
				//print_subset_by_index(cycle, i, best_end, "Action Taken");

				if( actor->next > actor->current ) {
					// Actor can no longer act this turn, just increment i
					//printf("  - Incrementing\n");
					++i;
				} else { // There is an error, and this is the only logical area it could be in...
					//print_subset_by_index(cycle, i, best_end, "Pre Resorting");
					// Actor can act again this turn, but may not be able to act this cycle
					int new_end;
					// i > (best_end - 1)
					//if( actor_compare(turn_actors[i], turn_actors[best_end - 1]) ) {
					// (best_end - 1) <= i
					if( !actor_compare(turn_actors[best_end - 1], turn_actors[i]) ) {
						//printf("  - Reinserting Before Last [%d]\n", i);
						// Would sort higher than the worst-best, we can't reduce local iterable size
						new_end = best_end;
					} else {
						// Does not sort higher than the worst-best, swap with worst and decrement best_end, then re-sort
						new_end = best_end - 1;
						swap_index(&turn_actors[i], &turn_actors[new_end]);
						//printf("  - Swapping with Last [%d <-> %d]\n", i, new_end);
					}
					// Do the re-sort from i to new_end
					order_turn_actors(i, new_end);
					best_end = new_end;

					//print_subset_by_index(cycle, i, best_end, "Post Resorting");
				}
				//print_subset_by_index(cycle, i, best_end, "Post Action");
			}
		}while(i < n_turn_actors);
	}

	return actions_taken;
}

int handle_actions(xorshiro128plus_generator_t& gen, range_u16_t action_range){
	get_turn_actors();
	// Initial ordering
	//order_turn_actors();
	return do_actions_reordering2(gen, action_range);
}

void print_actions(void){
#if USE_QUICKSORT
	char c = 'Q';
#else
	char c = 'B';
#endif
	printf("ACTION COUNTS [%d]\n", MAX_ACCUMULATORS);
	for( int i = 0; i < MAX_ACCUMULATORS; ++i ) {
		printf(" [%3d%c] {%7d}\n", i, c, actor_actions[i]);
	}
}

void main_test(void){
	srand(0);

	xorshiro128plus_generator_t crng = make_generator();
	initialize_cannonical_accumulators(crng, {1, 20}, {10, 30});
	initialize_turn_cycles(crng, {5, 10});

	printf("%d Characters | %d actions | %d buckets\n", MAX_ACCUMULATORS, TOTAL_ACTIONS, N_BUCKETS);

	for(; BEST_N != MIN_BEST_N; BEST_N >>= 1){
		initialize_accumulators();
		reset_turn_cycles();
		//xorshiro128plus_generator_t rng = crng;

		//print_accumulators();
		// set [0] to speed 0
		//accumulators[0].speed = 0;

		int total_actions = 0;
		//int last_total_actions = 0;

		int turns = 0;
		PROFILE(loop, "Test Loop");
		while(total_actions < TOTAL_ACTIONS){
			//last_total_actions = total_actions;
			++turns;
			advance_time();
			//get_turn_actors();
			//order_turn_actors();
			//total_actions += do_actions(rng, {5, 10});
			//total_actions += handle_actions(rng, {5, 10});
			total_actions += do_action_iterate_buckets();

			//printf("[%4d] %7d / %d : %+d\n", turns, total_actions, TOTAL_ACTIONS, total_actions - last_total_actions);
		}

		//print_accumulators();
		//print_actions();

		//printf("BEST_N == %d TOTAL = %d\n", BEST_N, MAX_ACCUMULATORS);
		//printf("%d turns to reach threshold (%d actions)\n", turns, total_actions);
	}
}

int main(int argc, const char** argv){
	(void)(argc);
	(void)(argv);
	
	//main_test();
	test::test_turn_tables();
	//test::test_no_accumulation();

	prof::p4::dump_profile_tree();
	//prof::p4::dump_profile();
	return 0;
}

int bucket_counts[N_BUCKETS + 1]; // bucket 0 = "in the future", bucket N_BUCKETS = first bucket
int bucket_id[MAX_ACCUMULATORS];

int get_bucket(const time_accumulator_t& ta){
	int bucket = 0;

	const int diff = ta.current - ta.next;
	const int diff_buckets = diff * N_BUCKETS;
	// diff / speed < bucket / N_BUCKETS
	// diff * N_BUCKETS < bucket * speed
	for( ; bucket < N_BUCKETS + 1; ++bucket ) {
		if( diff_buckets < (bucket * ta.speed) ) {
			return bucket;
		}
	}

	return 0;
}
void place_in_bucket(const int i){
	int bucket = get_bucket(accumulators[i]);
	++bucket_counts[bucket];
	bucket_id[i] = bucket;
}
void sort_to_buckets(void){
	for( int i = 0; i < N_BUCKETS + 1; ++i ) {
		bucket_counts[i] = 0;
	}

	for( int i = 0; i < MAX_ACCUMULATORS; ++i ) {
		place_in_bucket(i);
	}
}

void print_buckets(void){
	printf("BUCKET COUNTS\n");
	for( int i = N_BUCKETS; i >= 0; --i ) {
		if( bucket_counts[i] == 0 ) { continue; }
		float min = (i - 1) / (float)N_BUCKETS;
		float max = (i) / (float)N_BUCKETS;

		printf("BUCKET[%2d] (%+f - %+f) : %d\n", i,
			min < 0 ? -1 : min,
			max,
			bucket_counts[i]);
	}
}

int do_action_iterate_buckets(void){
	sort_to_buckets();
	//print_buckets();

	int actions_taken = 0;
	for( int b = N_BUCKETS; b > 0; --b ) {
		if( bucket_counts[b] > 0 ) {
			// Fill "turn" where bucket == i
			n_turn_actors = 0;
			for( int i = 0; i < MAX_ACCUMULATORS; ++i ) {
				if( bucket_id[i] == b ) {
					turn_actors[n_turn_actors++] = i;
					accumulators[i].current -= accumulators[i].next;
					accumulators[i].next = 0;
				}
			}

			// Do actions within bucket
			int ndx = 0;
			do{
				int best_end = best_N(BEST_N, ndx, n_turn_actors);
				for( ; ndx < best_end; ) {
					const int actor_ndx = turn_actors[ndx];
					time_accumulator_t *actor = accumulators + actor_ndx;
					++actor_actions[actor_ndx];
					++actions_taken;

					// Do action
					turn_cycle_t *tcycle = action_cycles + actor_ndx;
					actor->next += tcycle->action[tcycle->cur_action];
					tcycle->cur_action = ( tcycle->cur_action + 1 ) % TURN_CYCLES;

					place_in_bucket(actor_ndx);
					int new_bucket = bucket_id[actor_ndx];
					if( new_bucket == b ) {
						// Still in the same bucket so we need to re-sort the bucket
						order_turn_actors(ndx, best_end);
					} else {
						// Not in the same bucket, increment ndx
						++ndx;
					}
				}
			} while( ndx < n_turn_actors );
		}
	}
	return actions_taken;
}
/*
int do_actions_reordering2(xorshiro128plus_generator_t& gen, range_u16_t action_range){
	int actions_taken = 0;

	int i = 0;

	//printf("TURN START [%d]\n", n_turn_actors);
	if(n_turn_actors > 0){
		int cycle = 0;
		do{
			++cycle;
			int best_end = best_N(BEST_N, i, n_turn_actors);
			//printf("  CYCLE START<%02d>[%d, %d] - %d\n", cycle, i, n_turn_actors, best_end);
			//print_subset_by_index(cycle, i, best_end);

			for( ; i < best_end; ) {
				//printf("  : Iteration [%d | %d]\n", i, best_end);
				// Take action with "best"
				time_accumulator_t *actor = accumulators + turn_actors[i];
				++actor_actions[turn_actors[i]];
				++actions_taken;

				turn_cycle_t *tcycle = action_cycles + turn_actors[i];

				//actor->next += xorshiro128plus_range(&gen, action_range.min, action_range.max + 1);
				actor->next += tcycle->action[tcycle->cur_action]; 
				tcycle->cur_action = (tcycle->cur_action + 1) % TURN_CYCLES;
				//print_subset_by_index(cycle, i, best_end, "Action Taken");

				if( actor->next > actor->current ) {
					// Actor can no longer act this turn, just increment i
					//printf("  - Incrementing\n");
					++i;
				} else { // There is an error, and this is the only logical area it could be in...
					//print_subset_by_index(cycle, i, best_end, "Pre Resorting");
					// Actor can act again this turn, but may not be able to act this cycle
					int new_end;
					// i > (best_end - 1)
					//if( actor_compare(turn_actors[i], turn_actors[best_end - 1]) ) {
					// (best_end - 1) <= i
					if( !actor_compare(turn_actors[best_end - 1], turn_actors[i]) ) {
						//printf("  - Reinserting Before Last [%d]\n", i);
						// Would sort higher than the worst-best, we can't reduce local iterable size
						new_end = best_end;
					} else {
						// Does not sort higher than the worst-best, swap with worst and decrement best_end, then re-sort
						new_end = best_end - 1;
						swap_index(&turn_actors[i], &turn_actors[new_end]);
						//printf("  - Swapping with Last [%d <-> %d]\n", i, new_end);
					}
					// Do the re-sort from i to new_end
					order_turn_actors(i, new_end);
					best_end = new_end;

					//print_subset_by_index(cycle, i, best_end, "Post Resorting");
				}
				//print_subset_by_index(cycle, i, best_end, "Post Action");
			}
		}while(i < n_turn_actors);
	}

	return actions_taken;
}
*/