#pragma once

#include <vector>
#include <cstdint>

namespace sim2d_dispersion
{

	struct State {
		std::vector<float> hx;
		std::vector<float> hy;

		std::vector<float> ez;	// E n
		std::vector<float> ez0;	// E n-1

		std::vector<float> pz;	// P n
		std::vector<float> pz0;	// P n-1

		// CPML
		std::vector<float> psi_ex;
		std::vector<float> psi_ey;
		std::vector<float> psi_hx;
		std::vector<float> psi_hy;
		std::vector<float> ax;
		std::vector<float> bx;
		std::vector<float> ay;
		std::vector<float> by;

		std::vector<float> samples;

		float dx;
		float dy;
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