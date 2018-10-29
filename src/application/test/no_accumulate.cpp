#include "tests.h"

#include "profiler.h"
#include "types.h"

namespace no_accumulate_detail{
	using namespace test_constants;
	// Bucket list:
	// Buckets are labeled by which timeslice they exist in
	// as (N * Turn) + timeslice.
	// Buckets exist only when they have contents so we don't have
	// a massive number of buckets to deal with for the worst-case
	// scenario of some action getting pushed far into the future

	// _next, _prev : next and previous list node indices
	// data         : data index associated with this node
	struct list_node_t{
		int _next;
		int _prev;

		int data;
	};

	struct time_slice_t{
		
	};

	// Since it's still useful to have turn-delineation we'll make
	// each node in the Bucket list a set of N time-slices.
	// Not useful to spend memory on empty slices, so we'll have
	// a large vector of time slices and store indexes.
	struct turn_bucket_t{
		int turn_id;
		int timeslice_ndx[N_BUCKETS];
	};
}

void test::test_no_accumulation(void){
	PROFILE(test_start, "Test(No Accumulation)");
}