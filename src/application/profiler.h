#ifndef H_PROFILER4_H
#define H_PROFILER4_H
#pragma once

namespace prof{ namespace p4{
	void begin_sample(const char*);
	void end_sample(const char*);

	void dump_profile(void);
	void dump_profile_tree(void);

	struct Sample{
		const char* _name;
		inline Sample(const char* name): _name(name){
			begin_sample(_name);
		}
		inline ~Sample(void){
			end_sample(_name);
		}
	};
}}

#define PROFILE(name, str) prof::p4::Sample sample_##name(str)

#endif