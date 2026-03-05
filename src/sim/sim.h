#pragma once

#include <cstdint>
#include <vector>

typedef uint32_t u32;
typedef uint64_t u64;

namespace sim
{

	struct EFieldElement
	{
		double e{ 0 };
	};

	struct HFieldElement
	{
		double h{ 0 };
	};

	struct State
	{
		double dx;
		double dt;
		double t;

		u32 dimension;
		std::vector<EFieldElement> eField0;
		std::vector<EFieldElement> eField1;
		std::vector<HFieldElement> hField0;
		std::vector<HFieldElement> hField1;
	};

	void init_sim(State& state);

	void run_sim(State& state, u32 numSteps);

}