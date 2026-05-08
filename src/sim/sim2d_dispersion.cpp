#include "sim2d_dispersion.h"

#include <fmt/core.h>

#include <glm/glm.hpp>

void sim2d_dispersion::state_init(State& state)
{
	state.x = 800;
	state.y = 800;
	state.npml = 16;

	state.hx.resize(state.x * state.y);
	state.hy.resize(state.x * state.y);

	state.ez.resize(state.x * state.y);
	state.ez0.resize(state.x * state.y);

	state.pz.resize(state.x * state.y);
	state.pz0.resize(state.x * state.y);

	state.psi_ex.resize(state.x * state.y);
	state.psi_ey.resize(state.x * state.y);
	state.psi_hx.resize(state.x * state.y);
	state.psi_hy.resize(state.x * state.y);
	state.ax.resize(state.x);
	state.bx.resize(state.x);
	state.ay.resize(state.y);
	state.by.resize(state.y);

	state.samples.clear();

	const float c0 = 299792458.0f;

	state.dx = 0.001f;
	state.dy = 0.001f;
	// Courant number 0.99
	state.dt = state.dx * 0.99f / c0;
	state.t = 0.0f;

	for (uint32_t j = 0; j < state.y; j++) {
		for (uint32_t i = 0; i < state.x; i++) {
			uint32_t idx = j * state.x + i;
			state.hx[idx] = 0.0f;
			state.hy[idx] = 0.0f;
			state.ez[idx] = 0.0f;
			state.ez0[idx] = 0.0f;
			state.pz[idx] = 0.0f;
			state.pz0[idx] = 0.0f;
			state.psi_ex[idx] = 0.0f;
			state.psi_ey[idx] = 0.0f;
			state.psi_hx[idx] = 0.0f;
			state.psi_hy[idx] = 0.0f;
		}
	}

	// init resonator cavity

	/*const float wg_eps = 9.0f;
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
	}*/

	// CPML parameters

	for (uint32_t i = 0; i < state.x; i++) {
		
	}
	for (uint32_t j = 0; j < state.y; j++) {
		
	}

	// empirical tuning
	float sigma_max = 3.0f;
	float kappa_max = 3.0f;
	float d = state.npml;
	float m = 2.0f;

	for (int i = 0; i < state.x; i++) {
		float xnum = (float)std::min(i, (int)state.x - 1 - i);

		if (xnum < state.npml) {
			float x = (state.npml - xnum) / state.npml;

			float sigma = sigma_max * powf(x, m);
			float kappa = 1.0f + (kappa_max - 1.0f) * powf(x, m);

			float sigma_eff = sigma / (1.0f + (kappa - 1.0f));

			state.bx[i] = expf(-sigma_eff * 0.5f);
			state.ax[i] = sigma_eff * (state.bx[i] - 1.0f);
		}
		else {
			state.bx[i] = 1.0f;
			state.ax[i] = 0.0f;
		}
	}

	for (int j = 0; j < state.y; j++) {
		float ynum = (float)std::min(j, (int)state.y - 1 - j);

		if (ynum < state.npml) {
			float y = (state.npml - ynum) / state.npml;
			float sigma = sigma_max * powf(y, m);
			float kappa = 1.0f + (kappa_max - 1.0f) * powf(y, m);

			float sigma_eff = sigma / (1.0f + (kappa - 1.0f));

			state.by[j] = expf(-sigma_eff * 0.5f);
			state.ay[j] = sigma_eff * (state.by[j] - 1.0f);
		}
		else {
			state.by[j] = 1.0f;
			state.ay[j] = 0.0f;
		}
	}
}

void sim2d_dispersion::state_update(State& state)
{
	auto& hy = state.hy;
	auto& hx = state.hx;
	auto& ez = state.ez;
	auto& ez0 = state.ez0;
	auto& pz = state.pz;
	auto& pz0 = state.pz0;

	auto& psi_ex = state.psi_ex;
	auto& psi_ey = state.psi_ey;
	auto& psi_hx = state.psi_hx;
	auto& psi_hy = state.psi_hy;
	auto& ax = state.ax;
	auto& bx = state.bx;
	auto& ay = state.ay;
	auto& by = state.by;

	// constants (normalised)
	const float eps0 = 1.0f;
	const float dt = 1.0f;
	const float c0 = 1.0f;
	//const float mu0 = 1.25663706212e-6f;

	// Lorentz model parameters
	float w0 = 2.0f;
	float delta = 0.0f;
	float deltaEps = 1.0f;

	const float M = 1.0f / (deltaEps * w0 * w0);
	const float C = (2.0f * delta) / (deltaEps * w0 * w0);
	const float K = 1.0f / deltaEps;

	const float S = M + 0.5f * C * dt + 0.25f * K * dt * dt;
	const float w1 = 2.0f * M - 0.5f * K * dt * dt;
	const float w2 = -M + 0.5f * C * dt + 0.25f * K * dt * dt;
	const float u0 = 0.25f * eps0 * dt * dt;
	const float u1 = 0.5f * eps0 * dt * dt;
	const float u2 = 0.25f * eps0 * dt * dt;

	// CDS for Ampere's Law
	const float beta = 0.25f;
	const float gamma = 0.5f;

	// material parameters from paper
	/*const float epsInf = 31.1662f;
	const float a0 = 6.9379e21f;
	const float a1 = 1.5057e12f;
	const float b0 = 6.1637e18f;
	const float b1 = 4.5425e10f;
	const float b2 = 1.0f;*/

	// modified lorentz coefficients for free space
	const float epsInf = 3.0f;
	const float a0 = 0.0f;
	const float a1 = 0.0f;
	const float b0 = 0.0f;
	const float b1 = 0.0f;
	const float b2 = 10.0f;

	const float Ca = b0 * beta * dt * dt + b1 * gamma * dt + b2;
	const float Cb = b0 * (0.5f - 2.0f * beta + gamma) * dt * dt - b1 * (2.0f * gamma - 1.0f) * dt + 2.0f * b2;
	const float Cc = b0 * (0.5f + beta - gamma) * dt * dt + b1 * (gamma - 1.0f) * dt + b2;
	const float Cd = eps0 * (a0 * beta * dt * dt + a1 * gamma * dt);
	const float Ce = eps0 * (a0 * (0.5f - 2.0f * beta + gamma) * dt * dt - a1 * (2.0f * gamma - 1.0f) * dt);
	const float Cf = eps0 * (a0 * (0.5f + beta - gamma) * dt * dt + a1 * (gamma - 1.0f) * dt);

	const float dx = state.dx;
	const float dy = state.dy;

	// pz and ez fields

	for (uint32_t j = 1; j < state.y - 1; j++) {
		for (uint32_t i = 1; i < state.x - 1; i++) {
			uint32_t idx = j * state.x + i;
			uint32_t idx_y0 = j * state.x + i - 1;
			uint32_t idx_x0 = (j - 1) * state.x + i;

			float dHy_dx = hy[idx] - hy[idx_y0];
			float dHx_dy = hx[idx] - hx[idx_x0];

			float psi_exNew = bx[i] * psi_ex[idx] + ax[i] * dHy_dx;
			float psi_eyNew = by[j] * psi_ey[idx] + ay[j] * dHx_dy;
			psi_ex[idx] = psi_exNew;
			psi_ey[idx] = psi_eyNew;

			float curl_h = (dHy_dx + psi_exNew) - (dHx_dy + psi_eyNew);

			if (psi_exNew != 0.0f || psi_eyNew != 0.0f) {
				//fmt::print("curl_h: {}, psi_ex: {}, psi_ey: {}\n", curl_h, psi_exNew, psi_eyNew);
			}

			float lhs =	(Ca * epsInf + Cd) / (Ca * dt);
			float rhs = (Ca * epsInf - Ce) / (Ca * dt) * ez[idx] +
				-(Cf / (Ca * dt)) * ez0[idx] +
				curl_h +
				((Cb + Ca) / (Ca * dt)) * pz[idx] +
				(Cc / (Ca * dt)) * pz0[idx];
			float ezNew = rhs / lhs;

			float pzNew = (
				Cd * ezNew +
				Ce * ez[idx] +
				Cf * ez0[idx] +
				-Cb * pz[idx] +
				-Cc * pz0[idx]
			) / Ca;

			/*float pzNew = (
				w1 * pz[idx] +
				w2 * pz0[idx] +
				u0 * ezNew +
				u1 * ez[idx] +
				u2 * ez0[idx]
			) / S;*/
			//pzNew = 0.0f;

			pz0[idx] = pz[idx];
			pz[idx] = pzNew;

			ez0[idx] = ez[idx];
			ez[idx] = ezNew;
		}
	}

	// pulse insertion
	const float pi = 3.14159265358979323846f;
	float t0 = 800.0 * state.dt;
	float t = state.t;
	float spread = 300.0 * state.dt;
	float ex = expf(-0.5f * powf((t0 - t) / spread, 2.0f));
	//float pulse = sinf(2.0f * pi * 500.0f * 1e6f * state.dt * state.step);

	float pulse = sinf(2.0f * pi * 3e3f * 1e6f * state.dt * state.step);

	uint32_t ip = state.x / 2 - 10;
	uint32_t jp = state.y / 2 - 10;
	ez[jp * state.x + ip] += pulse;

	// hx field

	for (uint32_t j = 0; j < state.y - 1; j++) {
		for (uint32_t i = 0; i < state.x; i++) {
			uint32_t idx = j * state.x + i;
			uint32_t idx_z0 = (j + 1) * state.x + i;
			float curl_e = ez[idx] - ez[idx_z0];
			float psi_hxNew = by[j] * psi_hx[idx] + ay[j] * curl_e;
			psi_hx[idx] = psi_hxNew;
			hx[idx] = hx[idx] + 0.5f * (curl_e + psi_hxNew);
		}
	}

	// hy field

	for (uint32_t j = 0; j < state.y - 1; j++) {
		for (uint32_t i = 0; i < state.x - 1; i++) {
			uint32_t idx = j * state.x + i;
			uint32_t idx_z0 = j * state.x + i + 1;
			float curl_e = ez[idx_z0] - ez[idx];
			float psi_hyNew = bx[i] * psi_hy[idx] + ax[i] * curl_e;
			psi_hy[idx] = psi_hyNew;
			hy[idx] = hy[idx] + 0.5f * (curl_e + psi_hyNew);
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
