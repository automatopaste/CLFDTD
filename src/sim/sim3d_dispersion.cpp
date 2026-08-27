#include "sim3d_dispersion.h"

#include <fmt/core.h>

#include <glm/glm.hpp>

void sim3ddisp::state_init(State& state)
{
	state.resolution = 1;
	state.dim = state.resolution * ivec3{ 200, 200, 100 };
	state.npml = 8;
	state.step = 0;

	state.ga.resize(state.dim);
	state.e.resize(state.dim);
	state.e0.resize(state.dim);
	state.p.resize(state.dim);
	state.p0.resize(state.dim);
	state.h.resize(state.dim);

	state.lastSample = -1.1f;
	state.samples.clear();

	state.dx = 1.0f / (float)state.resolution;
	state.c = 1.0f;
	float courant = 0.5f;
	state.dt = courant * state.dx / state.c;
	state.t = 0.0f;

	for (int k = 0; k < state.dim.z; k++) {
		for (int j = 0; j < state.dim.y; j++) {
			for (int i = 0; i < state.dim.x; i++) {
				state.e.at(i, j, k) = vec3{ 0.0f, 0.0f, 0.0f };
				state.e0.at(i, j, k) = vec3{ 0.0f, 0.0f, 0.0f };
				state.p.at(i, j, k) = vec3{ 0.0f, 0.0f, 0.0f };
				state.p0.at(i, j, k) = vec3{ 0.0f, 0.0f, 0.0f };
				state.h.at(i, j, k) = vec3{ 0.0f, 0.0f, 0.0f };
				state.ga.at(i, j, k) = vec3{ 1.0f, 1.0f, 1.0f };
			}
		}
	}

	// init dipole

	int ic = state.dim.x / 2;
	int jc = state.dim.y / 2;
	int kc = state.dim.z / 2;
	for (int k = kc - 10; k < kc + 10; k++) {
		state.ga.at(ic, jc, k).z = 1.0 / 12.0f;
	}

	//state.d.at(ic, jc, kc).x = 10.0f;
}

void sim3ddisp::state_update(State& state)
{
	float cdt_dx = state.c * state.dt / state.dx;

	float t0 = state.dx / state.c;
	const float dt = state.dt;
	float eps0 = 1.0f;
	float epsInf = 3.7f;
	float a0 = 1.9136e32f;
	float a1 = 0.0f;
	float b0 = 0.0f;
	float b1 = 2.7362e13f;
	float b2 = 1.0f;

	float Ca = 0.25f * b0 * dt * dt + 0.5f * b1 * dt + b2;
	float Cb = 0.5f * b0 * dt * dt - 2.0f * b2;
	float Cc = 0.25f * b0 * dt * dt - 0.5f * b1 * dt + b2;

	float Cd = eps0 * (0.25f * a0 * dt * dt + 0.5f * a1 * dt);
	float Ce = eps0 * (0.5f * a0 * dt * dt);
	float Cf = eps0 * (0.25f * a0 * dt * dt - 0.5f * a1 * dt);

	// E and P update

	for (int k = 1; k < state.dim.z; k++) {
		for (int j = 1; j < state.dim.y; j++) {
			for (int i = 1; i < state.dim.x; i++) {
				vec3 h = state.h.at(i, j, k);
				float chx = h.z - state.h.at(i, j - 1, k).z - h.y + state.h.at(i, j, k - 1).y;
				float chy = h.x - state.h.at(i, j, k - 1).x - h.z + state.h.at(i - 1, j, k).z;
				float chz = h.y - state.h.at(i - 1, j, k).y - h.x + state.h.at(i, j - 1, k).x;
				vec3 curlH = vec3{ chx, chy, chz };

				vec3 ePrevious = state.e0.at(i, j, k);
				vec3 eCurrent = state.e.at(i, j, k);

				vec3 pPrevious = state.p0.at(i, j, k);
				vec3 pCurrent = state.p.at(i, j, k);

				float lhs = (Ca * epsInf + Cd) / (Ca * dt);
				vec3 rhs = (Ca * epsInf - Ce) / (Ca * dt) * eCurrent +
					-(Cf / (Ca * dt)) * ePrevious +
					curlH +
					((Cb + Ca) / (Ca * dt)) * pCurrent +
					(Cc / (Ca * dt)) * pPrevious;
				vec3 eNew = rhs / lhs;

				vec3 pNew = (
					Cd * eNew +
					Ce * eCurrent +
					Cf * ePrevious +
					-pCurrent * Cb +
					-pPrevious * Cc
					) / Ca;

				state.p0.at(i, j, k) = pCurrent;
				state.p.at(i, j, k) = pNew;

				state.e0.at(i, j, k) = eCurrent;
				state.e.at(i, j, k) = eNew;
			}
		}
	}
	
	float frequency = 0.1f;
	float width = 2.0f;
	float cutoff = 1.0f;
	float pi = 3.14159265359f;

	float tPeak = cutoff * width;

	float envelope = expf(-0.5f * powf((state.t - tPeak) / width, 2.0f));
	float pulse = envelope * cosf(2.0f * pi * frequency * (state.t - tPeak));

	int ic = state.dim.x / 2;
	int jc = state.dim.y / 2;
	int kc = state.dim.z / 2;
	state.e.at(ic, jc, kc).z -= cdt_dx * pulse;

	// H update

	for (int k = 1; k < state.dim.z - 1; k++) {
		for (int j = 1; j < state.dim.y - 1; j++) {
			for (int i = 1; i < state.dim.x - 1; i++) {
				vec3 e = state.e.at(i, j, k);
				float cex = state.e.at(i, j, k + 1).y - e.y - state.e.at(i, j + 1, k).z + e.z;
				float cey = state.e.at(i + 1, j, k).z - e.z - state.e.at(i, j, k + 1).x + e.x;
				float cez = state.e.at(i, j + 1, k).x - e.x - state.e.at(i + 1, j, k).y + e.y;
				vec3 curlE = vec3{ cex, cey, cez };

				state.h.at(i, j, k) += cdt_dx * curlE;
			}
		}
	}

	for (int j = 1; j < state.dim.y - 1; j++) {
		for (int k = 1; k < state.dim.z - 1; k++) {
			int i = state.dim.x - 1;
			vec3 e = state.e.at(i, j, k);
			float cex = state.e.at(i, j, k + 1).y - e.y - state.e.at(i, j + 1, k).z + e.z;
			state.h.at(i, j, k).x += cdt_dx * cex;
		}
	}
	for (int k = 1; k < state.dim.z - 1; k++) {
		for (int i = 1; i < state.dim.x - 1; i++) {
			int j = state.dim.y - 1;
			vec3 e = state.e.at(i, j, k);
			float cey = state.e.at(i + 1, j, k).z - e.z - state.e.at(i, j, k + 1).x + e.x;
			state.h.at(i, j, k).y += cdt_dx * cey;
		}
	}
	for (int j = 1; j < state.dim.y - 1; j++) {
		for (int i = 1; i < state.dim.x - 1; i++) {
			int k = state.dim.z - 1;
			vec3 e = state.e.at(i, j, k);
			float cez = state.e.at(i, j + 1, k).x - e.x - state.e.at(i + 1, j, k).y + e.y;
			state.h.at(i, j, k).z += cdt_dx * cez;
		}
	}

	if (state.t - state.lastSample >= 1.0f) {
		state.lastSample = state.t;
		ivec3 p = state.resolution * ivec3{ 10, 10, 10 };
		vec3 sample = state.e.at(ic + p.x, jc + p.y, kc + p.z);
		state.samples.push_back(sample);
	}

	state.t += state.dt;
	state.step++;
}
