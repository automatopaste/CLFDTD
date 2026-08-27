#pragma once

#include <vector>
#include <array>

#include "types.h"

namespace sim
{

	struct State
	{
		double dx{ 1.0 };
		double dt{ 0.0 };
		double t{ 0.0 };
		u32 step{ 0 };

		u32 dimension{ 0 };

		std::vector<double> eField;
		std::vector<double> hField;

		std::array<double, 2> boundaries{ 0 };
	};

	void init_sim(State& state);

	void step_sim(State& state);

	struct State2D
	{
		double dx{ 1.0 };
		double dt{ 0.0 };
		double t{ 0.0 };
		u32 step{ 0 };

		uvec2 dimensions{ 0, 0 };

		std::vector<double> Ez;
		std::vector<double> Hx;
		std::vector<double> Hy;

		std::vector<double> ihx;
		std::vector<double> ihy;

		std::vector<double> gi2;
		std::vector<double> gi3;
		std::vector<double> gj2;
		std::vector<double> gj3;

		std::vector<double> fi1_Hx;
		std::vector<double> fi2_Hx;
		std::vector<double> fi3_Hx;
		std::vector<double> fj2_Hx;
		std::vector<double> fj3_Hx;

		std::vector<double> fj1_Hy;
		std::vector<double> fj2_Hy;
		std::vector<double> fj3_Hy;
		std::vector<double> fi2_Hy;
		std::vector<double> fi3_Hy;
	};

	void init_sim_2d(State2D& state);

	void step_sim_2d(State2D& state);

}