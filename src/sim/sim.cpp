#include "sim.h"

#include "fmt/core.h"

constexpr double PI = 3.14159265358979323846;
constexpr double C = 299792458.0; // speed of light
constexpr double EPS0 = 8.854187817e-12; // permittivity of free space
constexpr double MU0 = 4.0 * PI * 1e-7;

void sim::init_sim(State& state)
{	
	state.dx = 0.001; // 1 cm (rule of thumb is 10 grid points per wavelength)
	state.dt = state.dx / (2.0 * C);

	fmt::print("Simulation spatial step {} metres\n", state.dx);
	fmt::print("Simulation time step {} seconds\n", state.dt);

	const double SIMULATION_SIZE = 1.0; // 0.1 metres
	double numElements = SIMULATION_SIZE / state.dx;

	state.dimension = (u32)numElements;
	
	state.eField0.resize(numElements);
	state.eField1.resize(numElements);
	state.hField0.resize(numElements);
	state.hField1.resize(numElements);

	fmt::print("Simulation contains {} spatial elements\n", numElements);
}

void sim::run_sim(State& state, u32 numSteps)
{
	const double m = state.dt / (sqrt(EPS0 * MU0) * state.dx);

	for (u32 i = 0; i < numSteps; i++)
	{
		auto& eField = i % 2 == 0 ? state.eField0 : state.eField1;
		auto& eFieldPrev = i % 2 == 1 ? state.eField0 : state.eField1;
		auto& hField = i % 2 == 0 ? state.hField0 : state.hField1;
		auto& hFieldPrev = i % 2 == 1 ? state.hField0 : state.hField1;

		fmt::print("Simulation step {}:\n", i);

		// apply gaussian pulse

		const double spread = 0.05; // pulse width
		double s = state.t / spread;
		double pulse = exp(-0.5 * s * s);

		eField[9].e = pulse;

		fmt::print("E field:\n", i);

		// update E field

		for (u32 x = 0; x < state.dimension - 1; x++)
		{
			EFieldElement& ex = eField[x];

			HFieldElement& hk0 = hFieldPrev[x];
			HFieldElement& hk1 = hFieldPrev[x + 1];

			ex.e = ex.e - m * (hk1.h - hk0.h);

			fmt::print("[{:.4f}] ", ex.e);
		}

		fmt::print("\nH field:\n");
		
		// update H field

		for (u32 x = 0; x < state.dimension - 1; x++)
		{
			HFieldElement& hx = hField[x];

			EFieldElement& ek0 = eFieldPrev[x];
			EFieldElement& ek1 = eFieldPrev[x + 1];

			hx.h = hx.h - m * (ek1.e - ek0.e);

			fmt::print("[{:.4f}] ", hx.h);
		}

		fmt::print("\n");

		state.t += state.dt;
	}
}
