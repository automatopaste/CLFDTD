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

		std::vector<float> gi2;
		std::vector<float> gi3;
		std::vector<float> gj2;
		std::vector<float> gj3;

		std::vector<float> fi1;
		std::vector<float> fi2;
		std::vector<float> fi3;

		std::vector<float> fj1;
		std::vector<float> fj2;
		std::vector<float> fj3;

		std::vector<float> ihx;
		std::vector<float> ihy;

		std::vector<float> samples;

		float ddx;
		float dt;
		float t;
		uint32_t step;
		uint32_t x;
		uint32_t y;
		uint32_t npml;
	};

	void state_init(State& state);
	void state_update(State& state);

}