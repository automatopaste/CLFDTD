#include "sim.h"

#include "fmt/core.h"

constexpr double PI = 3.14159265358979323846;
constexpr double C = 299792458.0; // speed of light
constexpr double EPS0 = 8.854187817e-12; // permittivity of free space
constexpr double MU0 = 4.0 * PI * 1e-7;

inline static void compute_pml_1d(int idx, int N, int npml, double& g2, double& g3, double& f1, double& f2, double& f3)
{
	g2 = 1.0;
	g3 = 1.0;
	f1 = 0.0;
	f2 = 1.0;
	f3 = 1.0;

	// distance from nearest boundary
	int d = -1;

	double pos = (double)idx + 0.5; // half-cell shift
	if (pos < npml) d = npml - pos;
	else if (pos > N - npml) d = pos - (N - npml);

	if (d >= 0) {
		double xd = (double)npml;

		// --- gi2 / gi3 ---
		double xxn = (double)d / xd;
		double xn = 0.25 * xxn * xxn * xxn;

		g2 = 1.0 / (1.0 + xn);
		g3 = (1.0 - xn) / (1.0 + xn);

		// --- fi1 / fi2 / fi3 ---
		xxn = ((double)d - 0.5) / xd;
		xn = 0.25 * xxn * xxn * xxn;

		f1 = xn;
		f2 = 1.0 / (1.0 + xn);
		f3 = (1.0 - xn) / (1.0 + xn);
	}
}

void sim::init_sim(State& state)
{	
	state.dx = 0.01; // 1 cm (rule of thumb is 10 grid points per min wavelength)
	state.dt = state.dx / (2.0 * C);

	fmt::print("Simulation spatial step {} metres\n", state.dx);
	fmt::print("Simulation time step {} seconds\n", state.dt);

	//const double SIMULATION_SIZE = 8.0;
	double numElements = 160;

	state.dimension = (u32)numElements;
	
	state.eField.resize(numElements);
	state.hField.resize(numElements - 1);

	fmt::print("Simulation contains {} spatial elements\n", numElements);

	//stateField[numElements / 2] = 1.0; // initial pulse
}

void sim::step_sim(State& state)
{
	const double ce = state.dt / (EPS0 * state.dx);
	const double ch = state.dt / (MU0 * state.dx);

	auto& eField = state.eField;
	auto& hField = state.hField;

	// update magnetic field

	for (u32 x = 0; x < state.dimension - 1; x++)
	{
		hField[x] -= ch * (eField[x + 1] - eField[x]);
	}

	// update electric field

	for (u32 x = 1; x < state.dimension - 1; x++)
	{
		eField[x] -= ce * (hField[x] - hField[x - 1]);
	}

	double c = 1.0 / sqrt(EPS0 * MU0);
	double k = (c * state.dt - state.dx) / (c * state.dt + state.dx);

	// left boundary
	double newLeft = state.boundaries[0] + k * (eField[1] - eField[0]);
	state.boundaries[0] = eField[1];
	eField[0] = newLeft;

	// right boundary
	double newRight = state.boundaries[1] + k * (eField[state.dimension - 2] - eField[state.dimension - 1]);
	state.boundaries[1] = eField[state.dimension - 2];
	eField[state.dimension - 1] = newRight;

	// gaussian pulse

	const double spread = state.dt * 100.0; // pulse width
	double s = state.t / spread;
	double pulse = exp(-0.5 * s * s);

	eField[state.dimension / 2] += pulse;

	state.t += state.dt;
}

void sim::init_sim_2d(State2D& state)
{
	state.dx = 0.01; // 1 cm (rule of thumb is 10 grid points per min wavelength)
	state.dt = state.dx / (2.0 * C);

	fmt::print("Simulation spatial step {} metres\n", state.dx);
	fmt::print("Simulation time step {} seconds\n", state.dt);

	const double SIMULATION_WIDTH = 8.0;
	double numElementsWidth = SIMULATION_WIDTH / state.dx;

	state.dimensions.x = (u32)(numElementsWidth);
	state.dimensions.y = (u32)(numElementsWidth);
	u32 n = numElementsWidth * numElementsWidth;

	state.Ez.resize(state.dimensions.x * state.dimensions.y);
	state.Hx.resize(state.dimensions.x * (state.dimensions.y - 1));
	state.Hy.resize((state.dimensions.x - 1) * state.dimensions.y);

	fmt::print("Simulation contains {} spatial elements\n", state.dimensions.x * state.dimensions.y);

	// pml initialisation

	const u32 pml = 8;
	u32 Nx = state.dimensions.x;
	u32 Ny = state.dimensions.y;
	u32 nHx = Nx * (Ny - 1);
	u32 nHy = (Nx - 1) * Ny;

	state.ihx.resize(n, 0.0);
	state.ihy.resize(n, 0.0);

	state.gi2.resize(n, 1.0);
	state.gi3.resize(n, 1.0);
	state.gj2.resize(n, 1.0);
	state.gj3.resize(n, 1.0);

	state.fi1_Hx.resize(nHx, 0.0);
	state.fi2_Hx.resize(nHx, 1.0);
	state.fi3_Hx.resize(nHx, 1.0);
	state.fj2_Hx.resize(nHx, 1.0);
	state.fj3_Hx.resize(nHx, 1.0);

	state.fj1_Hy.resize(nHy, 0.0);
	state.fj2_Hy.resize(nHy, 1.0);
	state.fj3_Hy.resize(nHy, 1.0);
	state.fi2_Hy.resize(nHy, 1.0);
	state.fi3_Hy.resize(nHy, 1.0);

	const double profile = 0.8;

	for (u32 i = 0; i < Nx; i++) {
		double xd = (double)pml;
		double xxnL = i < pml ? (pml - i) / xd : 0.0;
		double xxnR = i >= Nx - pml ? (i - (Nx - pml - 1)) / xd : 0.0;
		double xnL = profile * xxnL * xxnL * xxnL;
		double xnR = profile * xxnR * xxnR * xxnR;

		for (u32 j = 0; j < Ny; j++) {
			u32 idx = j * Nx + i;
			state.gi2[idx] = i < pml ? 1.0 / (1.0 + xnL) : (i >= Nx - pml ? 1.0 / (1.0 + xnR) : 1.0);
			state.gi3[idx] = i < pml ? (1.0 - xnL) / (1.0 + xnL) : (i >= Nx - pml ? (1.0 - xnR) / (1.0 + xnR) : 1.0);
		}
	}

	for (u32 j = 0; j < Ny; j++) {
		double xd = (double)pml;
		double xxnB = j < pml ? (pml - j) / xd : 0.0;
		double xxnT = j >= Ny - pml ? (j - (Ny - pml - 1)) / xd : 0.0;
		double xnB = profile * xxnB * xxnB * xxnB;
		double xnT = profile * xxnT * xxnT * xxnT;

		for (u32 i = 0; i < Nx; i++) {
			u32 idx = j * Nx + i;
			state.gj2[idx] = j < pml ? 1.0 / (1.0 + xnB) : (j >= Ny - pml ? 1.0 / (1.0 + xnT) : 1.0);
			state.gj3[idx] = j < pml ? (1.0 - xnB) / (1.0 + xnB) : (j >= Ny - pml ? (1.0 - xnT) / (1.0 + xnT) : 1.0);
		}
	}

	for (u32 j = 0; j < Ny - 1; j++) {
		double posY = j + 0.5;
		double xd = (double)pml;
		double xxnB = posY < pml ? (pml - posY) / xd : 0.0;
		double xxnT = posY > Ny - pml - 1 ? (posY - (Ny - pml - 1)) / xd : 0.0;
		double xnB = profile * xxnB * xxnB * xxnB;
		double xnT = profile * xxnT * xxnT * xxnT;

		for (u32 i = 0; i < Nx; i++) {
			u32 idx = j * Nx + i;
			state.fj2_Hx[idx] = 1.0 / (1.0 + (xxnB > 0 ? xnB : (xxnT > 0 ? xnT : 0.0)));
			state.fj3_Hx[idx] = 1.0 - (xxnB > 0 ? xnB : (xxnT > 0 ? xnT : 0.0)) / (1.0 + (xxnB > 0 ? xnB : (xxnT > 0 ? xnT : 0.0)));
		}
	}

	for (u32 i = 0; i < Nx - 1; i++) {
		double posX = i + 0.5;
		double xd = (double)pml;
		double xxnL = posX < pml ? (pml - posX) / xd : 0.0;
		double xxnR = posX > Nx - pml - 1 ? (posX - (Nx - pml - 1)) / xd : 0.0;
		double xnL = profile * xxnL * xxnL * xxnL;
		double xnR = profile * xxnR * xxnR * xxnR;

		for (u32 j = 0; j < Ny; j++) {
			u32 idx = j * (Nx - 1) + i;
			state.fi2_Hy[idx] = 1.0 / (1.0 + (xxnL > 0 ? xnL : (xxnR > 0 ? xnR : 0.0)));
			state.fi3_Hy[idx] = 1.0 - (xxnL > 0 ? xnL : (xxnR > 0 ? xnR : 0.0)) / (1.0 + (xxnL > 0 ? xnL : (xxnR > 0 ? xnR : 0.0)));
		}
	}
}

void sim::step_sim_2d(State2D& state)
{
	auto& Ez = state.Ez;
	auto& Hx = state.Hx;
	auto& Hy = state.Hy;

	u32 Nx = state.dimensions.x;
	u32 Ny = state.dimensions.y;

	double dt = state.dt;
	double dx = state.dx;
	double dy = state.dx;

	// update electric field

	for (int i = 1; i < Nx - 1; i++)
	{
		for (int j = 1; j < Ny - 1; j++)
		{
			u32 e = j * Nx + i;

			u32 hy0 = j * (Nx - 1) + i;
			u32 hy1 = j * (Nx - 1) + (i - 1);

			u32 hx0 = j * Nx + i;
			u32 hx1 = (j - 1) * Nx + i;

			// permittivity

			glm::vec2 v = glm::vec2{ (float)i, (float)j };
			glm::vec2 c = glm::vec2{ Nx / 2.0f, Ny / 2.0f };

			u32 width = 20;

			double Er = 1.0;
			if (glm::length(v - c) < width) {
				Er = 9.0;
			}

			//if (glm::length(v - c) < width) {
			//	Ez[e] = 100.0; // DEBUG visualization
			//}

			/*if (x < 0.3 || x > 0.7) {
				Er = 8.0;
			}*/

			// dielectric cylinder


			double sigma = 0.0;
			//if ((x > 0.3 && x < 0.4) || (x > 0.6 && x < 0.7)) {
			//	//sigma = 5.96e7; // conductivity of copper
			//	sigma = 0.1;
			//}

			const double ca = (1 - sigma * dt / (2 * EPS0 * Er)) / (1 + sigma * dt / (2 * EPS0 * Er));
			const double cb = dt / (EPS0 * Er) / (1 + sigma * dt / (2 * EPS0 * Er));

			/*Ez[e] = ca * Ez[e] +
				cb * (
				(Hy[hy0] - Hy[hy1]) / dx -
				(Hx[hx0] - Hx[hx1]) / dy
				);*/

			Ez[e] = state.gi3[e] * state.gj3[e] * ca * Ez[e] + state.gi2[e] * state.gj2[e] * cb * ((Hy[hy0] - Hy[hy1]) / dx - (Hx[hx0] - Hx[hx1]) / dy);
			//Ez[e] = ca * Ez[e] + cb * ((Hy[hy0] - Hy[hy1]) / dx - (Hx[hx0] - Hx[hx1]) / dy);
		}
	}

	// gaussian pulse

	const double spread = state.dt * 30.0; // pulse width
	const double t0 = state.dt * 800.0;
	double s = (t0 - state.t) / spread;
	double pulse = exp(-0.5 * s * s);

	//double channel = 0.2 * Nx * dx; // width (metres)
	//double fc = C / (2.0 * channel); // cutoff freq
	//double freq = 2.0 * fc;

	double packet = 1.0 * sin(2.0 * PI * 500.0 * 1e6 * state.t) * pulse;

	u32 cx = state.dimensions.x / 2 - 10;
	u32 cy = state.dimensions.y / 2 - 10;

	Ez[cy * Nx + cx] += packet;

	// update magnetic field

	const int pml = 16;

	for (int i = 0; i < Nx; i++)
	{
		for (int j = 0; j < Ny - 1; j++)
		{
			u32 h = j * Nx + i;
			u32 e0 = j * Nx + i;
			u32 e1 = (j + 1) * Nx + i;

			double curlE = Ez[e0] - Ez[e1];

			state.ihx[h] += state.fi1_Hx[h] * curlE;
			double c = dt / (MU0 * dy);
			Hx[h] = state.fj3_Hx[h] * Hx[h] + state.fj2_Hx[h] * c * (curlE + state.ihx[h]);
			//Hx[h] = Hx[h] + c * (curlE);
		}
	}
	for (int i = 0; i < Nx - 1; i++)
	{
		for (int j = 0; j < Ny; j++)
		{
			u32 h = j * (Nx - 1) + i;
			u32 e0 = j * Nx + i;
			u32 e1 = j * Nx + i + 1;

			double curlE = (Ez[e1] - Ez[e0]);

			state.ihy[h] += state.fj1_Hy[h] * curlE;
			double c = dt / (MU0 * dx);
			Hy[h] = state.fi3_Hy[h] * Hy[h] + state.fi2_Hy[h] * c * (curlE + state.ihy[h]);
			//Hy[h] = Hy[h] + c * (curlE);
		}
	}

	state.t += state.dt;
	state.step++;
}
