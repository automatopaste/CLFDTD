#include "sim3d.h"

#include <fmt/core.h>

#include <glm/glm.hpp>

void sim3d::state_init(State& state)
{
	state.dim = ivec3{ 300, 300, 50 };
	state.npml = 8;

	state.ga.resize(state.dim);
	state.d.resize(state.dim);
	state.e.resize(state.dim);
	state.h.resize(state.dim);

	state.samples.clear();

	state.ddx = 0.001f;
	state.dt = state.ddx / 6e8f;
	state.t = 0.0f;

	for (int k = 0; k < state.dim.z; k++) {
		for (int j = 0; j < state.dim.y; j++) {
			for (int i = 0; i < state.dim.x; i++) {
				state.d.at(i, j, k) = vec3{ 0.0f, 0.0f, 0.0f };
				state.e.at(i, j, k) = vec3{ 0.0f, 0.0f, 0.0f };
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
		state.ga.at(ic, kc, k).z = 0.0f;
	}
	state.ga.at(ic, jc, kc).z = 1.0f;
}

void sim3d::state_update(State& state)
{
	// D update

	for (int k = 1; k < state.dim.z; k++) {
		for (int j = 1; j < state.dim.y; j++) {
			for (int i = 1; i < state.dim.x; i++) {
				vec3 h = state.h.at(i, j, k);
				float chx = h.z - state.h.at(i, j - 1, k).z - h.y + state.h.at(i, j, k - 1).y;
				float chy = h.x - state.h.at(i, j, k - 1).x - h.z + state.h.at(i - 1, j, k).z;
				float chz = h.y - state.h.at(i - 1, j, k).y - h.x + state.h.at(i, j - 1, k).x;
				vec3 curlH = vec3{ chx, chy, chz };

				state.d.at(i, j, k) += 0.5f * curlH;
			}
		}
	}
	
	float t0 = 20.0f;
	float spread = 6.0f;
	float epsz = 8.8e-12f;
	float pi = 3.14159f;
	float pulse = expf(-0.5f * powf((t0 - state.t) / spread, 2.0f));

	int ic = state.dim.x / 2;
	int jc = state.dim.y / 2;
	int kc = state.dim.z / 2;
	state.d.at(ic, jc, kc).z = pulse;

	// E update

	for (int k = 1; k < state.dim.z - 1; k++) {
		for (int j = 1; j < state.dim.y - 1; j++) {
			for (int i = 1; i < state.dim.x - 1; i++) {
				state.e.at(i, j, k) = state.ga.at(i, j, k) * state.d.at(i, j, k);
			}
		}
	}

	// H update

	for (int k = 1; k < state.dim.z - 1; k++) {
		for (int j = 1; j < state.dim.y - 1; j++) {
			for (int i = 1; i < state.dim.x - 1; i++) {
				vec3 e = state.e.at(i, j, k);
				float cex = state.e.at(i, j, k + 1).y - e.y - state.e.at(i, j + 1, k).z + e.z;
				float cey = state.e.at(i + 1, j, k).z - e.z - state.e.at(i, j, k + 1).x + e.x;
				float cez = state.e.at(i, j + 1, k).x - e.x - state.e.at(i + 1, j, k).y + e.y;
				vec3 curlE = vec3{ cex, cey, cez };

				state.h.at(i, j, k) += 0.5f * curlE;
			}
		}
	}

	for (int j = 1; j < state.dim.y - 1; j++) {
		for (int k = 1; k < state.dim.z - 1; k++) {
			int i = state.dim.x - 1;
			vec3 e = state.e.at(i, j, k);
			float cex = state.e.at(i, j, k + 1).y - e.y - state.e.at(i, j + 1, k).z + e.z;
			state.h.at(i, j, k).x += 0.5f * cex;
		}
	}
	for (int k = 1; k < state.dim.z - 1; k++) {
		for (int i = 1; i < state.dim.x - 1; i++) {
			int j = state.dim.y - 1;
			vec3 e = state.e.at(i, j, k);
			float cey = state.e.at(i + 1, j, k).z - e.z - state.e.at(i, j, k + 1).x + e.x;
			state.h.at(i, j, k).y += 0.5f * cey;
		}
	}
	for (int j = 1; j < state.dim.y - 1; j++) {
		for (int i = 1; i < state.dim.x - 1; i++) {
			int k = state.dim.z - 1;
			vec3 e = state.e.at(i, j, k);
			float cez = state.e.at(i, j + 1, k).x - e.x - state.e.at(i + 1, j, k).y + e.y;
			state.h.at(i, j, k).z += 0.5f * cez;
		}
	}

	vec3 sample = state.e.at(ic + 5, jc + 5, kc + 5);
	state.samples.push_back(sample);

	state.t += state.dt;
	state.step++;
}
