#pragma once

#include <vector>
#include <cstdint>

namespace sim2d
{

	struct State {
		std::vector<float> ga;
		std::vector<float> dz;
		std::vector<float> ez;
		std::vector<float> hx;
		std::vector<float> hy;
		float ddx;
		float dt;
		float t;
		uint32_t step;
		uint32_t x;
		uint32_t y;
	};

	void state_init(State& state);
	void state_update(State& state);

}