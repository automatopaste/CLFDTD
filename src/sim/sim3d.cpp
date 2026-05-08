#include "sim2d.h"

#include <fmt/core.h>

#include <glm/glm.hpp>

void sim2d::state_init(State& state)
{
	state.x = 800;
	state.y = 800;
	state.npml = 8;

	state.ga.resize(state.x * state.y);
	state.dz.resize(state.x * state.y);
	state.ez.resize(state.x * state.y);
	state.hx.resize(state.x * state.y);
	state.hy.resize(state.x * state.y);

	state.gi2.resize(state.x);
	state.gi3.resize(state.x);
	state.gj2.resize(state.y);
	state.gj3.resize(state.y);
	state.fi1.resize(state.x);
	state.fi2.resize(state.x);
	state.fi3.resize(state.x);
	state.fj1.resize(state.y);
	state.fj2.resize(state.y);
	state.fj3.resize(state.y);
	state.ihx.resize(state.x * state.y);
	state.ihy.resize(state.x * state.y);

	state.samples.clear();

	state.ddx = 0.001f;
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

	// init resonator cavity

	const float wg_eps = 9.0f;
	const float wg_w = 20.0f;
	for (uint32_t j = 0; j < state.y; j++) {
		for (uint32_t i = 0; i < state.x; i++) {
			uint32_t idx = j * state.x + i;
			glm::vec2 xy = glm::vec2{ (float)i, (float)j };
			glm::vec2 c = glm::vec2{ (float)(state.x / 2), (float)(state.y / 2) };
			if (glm::length(xy - c) < wg_w) {
				state.ga[idx] = 1.0f / wg_eps;
			}
		}
	}

	// pml parameters

	for (uint32_t i = 0; i < state.x; i++) {
		state.gi2[i] = 1.0f;
		state.gi3[i] = 1.0f;
		state.fi1[i] = 0.0f;
		state.fi2[i] = 1.0f;
		state.fi3[i] = 1.0f;
	}
	for (uint32_t j = 0; j < state.y; j++) {
		state.gj2[j] = 1.0f;
		state.gj3[j] = 1.0f;
		state.fj1[j] = 0.0f;
		state.fj2[j] = 1.0f;
		state.fj3[j] = 1.0f;
	}

	for (uint32_t i = 0; i < state.npml; i++) {
		float xnum = (float)(state.npml - i);
		float xd = (float)state.npml;
		float xxn = xnum / xd;
		float xn = 0.25f * powf(xxn, 3.0f);
		state.gi2[i] = 1.0f / (1.0f + xn);
		state.gi2[state.x - 1 - i] = 1.0f / (1.0f + xn);
		state.gi3[i] = (1.0f - xn) / (1.0f + xn);
		state.gi3[state.x - i - 1] = (1.0f - xn) / (1.0f + xn);
		xxn = (xnum - 0.5f) / xd;
		xn = 0.25f * powf(xxn, 3.0f);
		state.fi1[i] = xn;
		state.fi1[state.x - 2 - i] = xn;
		state.fi2[i] = 1.0f / (1.0f + xn);
		state.fi2[state.x - 2 - i] = 1.0f / (1.0f + xn);
		state.fi3[i] = (1.0f - xn) / (1.0f + xn);
		state.fi3[state.x - 2 - i] = (1.0f - xn) / (1.0f + xn);
	}

	for (uint32_t j = 0; j < state.npml; j++) {
		float xnum = (float)(state.npml - j);
		float xd = (float)state.npml;
		float xxn = xnum / xd;
		float xn = 0.25f * powf(xxn, 3.0f);
		state.gj2[j] = 1.0f / (1.0f + xn);
		state.gj2[state.y - 1 - j] = 1.0f / (1.0f + xn);
		state.gj3[j] = (1.0f - xn) / (1.0f + xn);
		state.gj3[state.y - j - 1] = (1.0f - xn) / (1.0f + xn);
		xxn = (xnum - 0.5f) / xd;
		xn = 0.25f * powf(xxn, 3.0f);
		state.fj1[j] = xn;
		state.fj1[state.y - 2 - j] = xn;
		state.fj2[j] = 1.0f / (1.0f + xn);
		state.fj2[state.y - 2 - j] = 1.0f / (1.0f + xn);
		state.fj3[j] = (1.0f - xn) / (1.0f + xn);
		state.fj3[state.y - 2 - j] = (1.0f - xn) / (1.0f + xn);
	}
}

void sim2d::state_update(State& state)
{
	auto& dz = state.dz;
	auto& hy = state.hy;
	auto& hx = state.hx;
	auto& ez = state.ez;
	auto& ga = state.ga;
	auto& gi2 = state.gi2;
	auto& gi3 = state.gi3;
	auto& gj2 = state.gj2;
	auto& gj3 = state.gj3;
	auto& fi1 = state.fi1;
	auto& fi2 = state.fi2;
	auto& fi3 = state.fi3;
	auto& fj1 = state.fj1;
	auto& fj2 = state.fj2;
	auto& fj3 = state.fj3;
	auto& ihx = state.ihx;
	auto& ihy = state.ihy;

	// dz field

	for (uint32_t j = 1; j < state.y - 1; j++) {
		for (uint32_t i = 1; i < state.x - 1; i++) {
			uint32_t idx = j * state.x + i;
			uint32_t idx_y0 = j * state.x + i - 1;
			uint32_t idx_x0 = (j - 1) * state.x + i;
			dz[idx] =
				gi3[i] * gj3[j] * dz[idx]
				+ gi2[i] * gj2[j] * 0.5f * (
					hy[idx] - hy[idx_y0] - hx[idx] + hx[idx_x0]
					);
		}
	}

	// ez field

	for (uint32_t j = 1; j < state.y - 1; j++) {
		for (uint32_t i = 1; i < state.x - 1; i++) {
			uint32_t idx = j * state.x + i;
			ez[idx] = ga[idx] * dz[idx];
		}
	}

	// pulse insertion
	const float pi = 3.14159265359f;

	//float pulse = 100.0 * expf(-0.5 * powf((t0 - t) / spread, 2.0f));
	/*float t0 = 800.0 * state.dt;
	float t = state.t;
	float spread = 300.0 * state.dt;
	float ex = expf(-0.5f * powf((t0 - t) / spread, 2.0f));
	float pulse = sinf(2.0f * pi * 500.0f * 1e6f * state.dt * state.step);
	uint32_t ip = state.x / 2 - 10;
	uint32_t jp = state.y / 2 - 10;
	dz[jp * state.x + ip] += pulse * ex;*/

	float pulse = sinf(2.0f * pi * 3e3f * 1e6f * state.dt * state.step);
	uint32_t ip = state.x / 2 - 10;
	uint32_t jp = state.y / 2 - 10;
	dz[jp * state.x + ip] += pulse;

	// hx field

	for (uint32_t j = 0; j < state.y - 1; j++) {
		for (uint32_t i = 0; i < state.x; i++) {
			uint32_t idx = j * state.x + i;
			uint32_t idx_z0 = (j + 1) * state.x + i;
			float curl_e = ez[idx] - ez[idx_z0];
			ihx[idx] = ihx[idx] + fi1[i] * curl_e;
			hx[idx] = fj3[j] * hx[idx]
				+ fj2[j] * 0.5f * (curl_e + ihx[idx]);
		}
	}

	// hy field

	for (uint32_t j = 0; j < state.y - 1; j++) {
		for (uint32_t i = 0; i < state.x - 1; i++) {
			uint32_t idx = j * state.x + i;
			uint32_t idx_z0 = j * state.x + i + 1;
			float curl_e = ez[idx_z0] - ez[idx];
			ihy[idx] = ihy[idx] + fj1[j] * curl_e;
			hy[idx] = fi3[i] * hy[idx]
				+ fi2[i] * 0.5f * (curl_e + ihy[idx]);
		}
	}

	uint32_t ic = state.x / 2 - 5;
	uint32_t jc = state.y / 2 - 5;
	uint32_t sampleIdx = jc * state.x + ic;
	float sample = ez[sampleIdx];
	state.samples.push_back(sample);

	state.t += state.dt;
	state.step++;
}
