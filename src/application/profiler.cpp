#include "profiler.h"

#include <vector>
#include <chrono>
#include <cstdio>

namespace prof{ namespace timer{
	using clock = std::chrono::high_resolution_clock;
	using nanoseconds = std::chrono::nanoseconds;

	using timepoint = clock::time_point;
	inline uint64_t duration_nanoseconds(timepoint s, timepoint e){
		return std::chrono::duration_cast<nanoseconds>(e - s).count();
	}

	inline timepoint now(void){ return clock::now(); }
}}
namespace prof{ namespace detail{
	struct SampleEvent{
		const char* name;
		prof::timer::timepoint time;
	};

	struct CategorizedSample{
		enum MODE{ SAMPLE_START = 0, SAMPLE_END = 1 };
		MODE mode;
		SampleEvent event;
	};

	struct SampleStorage{
		std::vector<SampleEvent> start, end;

		SampleStorage(int reserve = 128){
			start.reserve(reserve);
			end.reserve(reserve);

			begin_sample("<BEGIN>");
		}

		void begin_sample(const char* name){
			start.push_back({name, prof::timer::now()});
		}
		void end_sample(const char* name){
			end.push_back({name, prof::timer::now()});
		}
	};

	SampleStorage sampler;

	void merge_events(std::vector<CategorizedSample>& output);
}}

namespace prof{ namespace p4{
	void begin_sample(const char* name){
		prof::detail::sampler.begin_sample(name);
	}
	void end_sample(const char* name){
		prof::detail::sampler.end_sample(name);
	}
	void dump_profile(void){
		std::vector<prof::detail::CategorizedSample> ordered_events;
		prof::detail::merge_events(ordered_events);

		printf("PROFILE RESULTS\n");
		prof::timer::timepoint beginning = ordered_events[0].event.time;
		for(const auto& event : ordered_events ){
			uint64_t duration = prof::timer::duration_nanoseconds(beginning, event.event.time);
			printf("[%s] @ %12lu ns : %s\n", (event.mode == prof::detail::CategorizedSample::SAMPLE_START ? "BEG" : "END"), duration, event.event.name);
		}
	}
	void dump_profile_tree(void){
		std::vector<prof::detail::CategorizedSample> ordered_events;
		prof::detail::merge_events(ordered_events);

		printf("PROFILE RESULTS\n");
		// Here we need a stack, to keep track of how things are called
		int sample_stack[256];
		int sample_stack_sz = 0;

		struct CombinedSample{
			const char* name;
			prof::timer::timepoint time_start, time_end;
			int n_children, par_ndx;

			CombinedSample(const prof::detail::CategorizedSample& sample):
				name(sample.event.name), time_start(sample.event.time), n_children(0), par_ndx(0){}
		};

		std::vector<CombinedSample> samples;
		samples.reserve(prof::detail::sampler.start.size());

		prof::timer::timepoint beginning = ordered_events[0].event.time;

		samples.push_back(CombinedSample(ordered_events[0]));
		sample_stack[sample_stack_sz++] = 0;

		for( size_t ndx = 1, event_sz = ordered_events.size(); ndx < event_sz; ++ndx ) {
			const prof::detail::CategorizedSample& event = ordered_events[ndx];
			// Push on SAMPLE_BEGIN
			// Pop on SAMPLE_END
			switch( event.mode ) {
				case prof::detail::CategorizedSample::SAMPLE_START:{
					CombinedSample new_sample(event);
					new_sample.par_ndx = sample_stack[sample_stack_sz - 1];

					++samples[new_sample.par_ndx].n_children;
					//sample_stack[sample_stack_sz++] = ndx;
					sample_stack[sample_stack_sz++] = samples.size();
					samples.push_back(new_sample);
					break;
				}
				case prof::detail::CategorizedSample::SAMPLE_END:{
					// const prof::detail::CategorizedSample& stk_event = ordered_events[sample_stack[--sample_stack_sz]];

					// uint64_t duration = prof::timer::duration_nanoseconds(stk_event.event.time, event.event.time);
					// uint64_t s_dur = prof::timer::duration_nanoseconds(beginning, stk_event.event.time);
					// uint64_t e_dur = prof::timer::duration_nanoseconds(beginning, event.event.time);

					// printf("[%12lu ns - %12lu ns] (%12lu ns) %*.*s%s\n", s_dur, e_dur, duration, sample_stack_sz * 2, sample_stack_sz * 2, "", stk_event.event.name);
					CombinedSample& old_sample = samples[sample_stack[sample_stack_sz - 1]];
					old_sample.time_end = event.event.time;
					--sample_stack_sz;

					break;
				}
			}
		}


		for( auto& sample : samples ) {
			uint64_t duration = prof::timer::duration_nanoseconds(sample.time_start, sample.time_end);
			uint64_t s_dur = prof::timer::duration_nanoseconds(beginning, sample.time_start);
			uint64_t e_dur = prof::timer::duration_nanoseconds(beginning, sample.time_end);

			printf("[%12lu ns - %12lu ns] (%12lu ns) %s\n", s_dur, e_dur, duration, sample.name);
		}
	}
}}

namespace prof{ namespace detail{
	void merge_events(std::vector<CategorizedSample>& output){
		const std::vector<SampleEvent>& a = sampler.start;
		const std::vector<SampleEvent>& b = sampler.end;

		output.clear();
		output.reserve(a.size() + b.size());

		const size_t a_sz = a.size();
		const size_t b_sz = b.size();

		size_t a_ndx = 0;
		size_t b_ndx = 0;

		while( a_ndx != a_sz && b_ndx != b_sz ) {
			if( a[a_ndx].time < b[b_ndx].time ) {
				output.push_back({CategorizedSample::SAMPLE_START, a[a_ndx]});
				++a_ndx;
			} else if( a[a_ndx].time > b[b_ndx].time ) {
				output.push_back({CategorizedSample::SAMPLE_END, b[b_ndx]});
				++b_ndx;
			} else {
				output.push_back({CategorizedSample::SAMPLE_END, b[b_ndx]});
				output.push_back({CategorizedSample::SAMPLE_START, a[a_ndx]});
				++a_ndx;
				++b_ndx;
			}
		}
		for( ; a_ndx != a_sz; ++a_ndx ) {
			output.push_back({CategorizedSample::SAMPLE_START, a[a_ndx]});
		}
		for( ; b_ndx != b_sz; ++b_ndx ) {
			output.push_back({CategorizedSample::SAMPLE_END, b[b_ndx]});
		}
	}
}}