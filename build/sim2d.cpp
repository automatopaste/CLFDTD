#include "sim2d.h"

void sim2d::state_init(State& state)
{
	state.x = 60;
	state.y = 60;
	state.ga.resize(state.x * state.y);
	state.dz.resize(state.x * state.y);
	state.ez.resize(state.x * state.y);
	state.hx.resize(state.x * state.y);
	state.hy.resize(state.x * state.y);

	state.ddx = 0.01f;
	state.dt = state.ddx / 6e8f;
	state.t = 0.0f;

	for (uint32_t j = 0; j < state.y; j++) {
		for (uint32_t i = 0; i < state.x; i++) {
			uint32_t idx = j * state.x + i;
			state.dz[idx] = 0.0f;
			state.ez[idx] = 0.0f;
			state.hx[idx] = 0.0f;
			state.hy[idx] = 0.0f;
			state.ga[idx] = 1.0f;
		}
	}
}

void sim2d::state_update(State& state)
{
	auto& dz = state.dz;
	auto& hy = state.hy;
	auto& hx = state.hx;
	auto& ez = state.ez;
	auto& ga = state.ga;

	float t0 = 20.0;
	float t = state.t;
	float spread = 6.0;

	// dz field

	for (uint32_t j = 1; j < state.y; j++) {
		for (uint32_t i = 1; i < state.x; i++) {
			uint32_t idx = j * state.x + i;
			uint32_t idx_y0 = j * state.x + i - 1;
			uint32_t idx_x0 = (j - 1) * state.x + i;
			dz[idx] = dz[idx] + 0.5f * (hy[idx] - hy[idx_y0] - hx[idx] + hx[idx_x0]);
		}
	}

	// gaussian pulse

	float pulse = expf(-0.5 * powf((t0 - t) / spread, 2.0f));
	uint32_t ic = state.x / 2;
	uint32_t jc = state.y / 2;
	dz[jc * state.x + ic] = pulse;

	// ez field

	for (uint32_t j = 1; j < state.y; j++) {
		for (uint32_t i = 1; i < state.x; i++) {
			uint32_t idx = j * state.x + i;
			ez[idx] = ga[idx] * dz[idx];
		}
	}

	// hx field

	for (uint32_t j = 1; j < state.y - 1; j++) {
		for (uint32_t i = 0; i < state.x - 1; i++) {
			uint32_t idx = j * state.x + i;
			uint32_t idx_z0 = (j + 1) * state.x + i;
			hx[idx] = hx[idx] + 0.5f * (ez[idx] - ez[idx_z0]);
		}
	}

	// hy field

	for (uint32_t j = 0; j < state.y - 1; j++) {
		for (uint32_t i = 0; i < state.x - 1; i++) {
			uint32_t idx = j * state.x + i;
			uint32_t idx_z0 = j * state.x + i + 1;
			hy[idx] = hy[idx] + 0.5f * (ez[idx_z0] - ez[idx]);
		}
	}
}
