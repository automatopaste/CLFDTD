#include "sim3d_saturable_gain.h"

#include <fmt/core.h>

#include <glm/glm.hpp>

void sim3dsat::state_init(State& state)
{
	state.resolution = 1;
	state.dim = state.resolution * ivec3{ 200, 200, 100 };
	state.npml = 8;
	state.step = 0;

	//state.epsR.resize(state.dim);
	state.E.resize(state.dim);
	state.EPeak.resize(state.dim);
	state.F.resize(state.dim);
	state.J.resize(state.dim);
	state.H.resize(state.dim);
	state.s.resize(state.dim);

	state.lastSample = -1.1f;
	state.samples.clear();

	state.dx = 0.62e-9f / (float)state.resolution;
	state.c = 299792458.0f;
	float courant = 0.5f;
	state.dt = courant * state.dx / state.c;
	state.t = 0.0f;

	for (int k = 0; k < state.dim.z; k++) {
		for (int j = 0; j < state.dim.y; j++) {
			for (int i = 0; i < state.dim.x; i++) {
				state.E.at(i, j, k) = vec3{ 0.0f, 0.0f, 0.0f };
				state.EPeak.at(i, j, k) = 0.0f;
				state.F.at(i, j, k) = vec3{ 0.0f, 0.0f, 0.0f };
				state.J.at(i, j, k) = vec3{ 0.0f, 0.0f, 0.0f };
				state.H.at(i, j, k) = vec3{ 0.0f, 0.0f, 0.0f };
				state.s.at(i, j, k) = 0.0f;
				//state.epsR.at(i, j, k) = 1.0f;
			}
		}
	}

	// init dipole

	int ic = state.dim.x / 2;
	int jc = state.dim.y / 2;
	int kc = state.dim.z / 2;
	for (int k = kc - 10; k < kc + 10; k++) {
		//state.ga.at(ic, jc, k).z = 1.0 / 12.0f;
	}

	//state.d.at(ic, jc, kc).x = 10.0f;
}

void sim3dsat::state_update(State& state)
{
	const float c = state.c;
	const float dt = state.dt;
	const float dx = state.dx;
	const float t = state.t;
	const float eps0 = 1.0f;

	// material properties
	const float lambda0 = 0.89e-6f;
	const float T2 = 0.07e-12f;
	const float n = 3.59f;

	const float sigma0 = -5000.0f; // S/m
	const float Is = 1.0f;

	const float omega0 = (2.0f * 3.14159265359f * state.c) / lambda0;
	const float epsR = n * n;
	const float epsilon = eps0 * epsR;

	// E update

	for (int k = 1; k < state.dim.z; k++) {
		for (int j = 1; j < state.dim.y; j++) {
			for (int i = 1; i < state.dim.x; i++) {
				vec3 h = state.H.at(i, j, k);
				float chx = h.z - state.H.at(i, j - 1, k).z - h.y + state.H.at(i, j, k - 1).y;
				float chy = h.x - state.H.at(i, j, k - 1).x - h.z + state.H.at(i - 1, j, k).z;
				float chz = h.y - state.H.at(i - 1, j, k).y - h.x + state.H.at(i, j - 1, k).x;
				vec3 curlH = vec3{ chx, chy, chz };

				// material constants

				//float epsR = state.epsR.at(i, j, k);
				
				float s = state.s.at(i, j, k);

				float Z2 = (1.0f + omega0 * omega0 * T2 * T2);

				float B = 8.0f * epsilon * T2 * (dt + T2) + dt * dt * (2.0f * epsilon * Z2 + s * omega0 * (dt + 2.0f * T2));

				float A1 = (4.0f * t * s * sigma0 * (dt + 2.0f * T2)) / (B * dx);
				float A2 = (8.0f * epsilon * s * sigma0 * dt) / B;
				float A3 = -(4.0f * dt * (2.0f * epsilon * Z2 + s * omega0 * (dt + 2.0f * T2))) / B;
				float A4 = (-(8.0f * epsilon * T2 * (dt - T2)) / B) - ((dt * dt * (2.0f * epsilon * Z2 + s * omega0 * (dt + 2.0f * T2))) / B);

				vec3 E = state.E.at(i, j, k);
				vec3 J = state.J.at(i, j, k);
				vec3 F = state.F.at(i, j, k);

				vec3 fNew = A1 * curlH + A2 * E + A3 * J + A4 * F;
				vec3 jNew = J + (dt / 2.0f) * (fNew + F);
				vec3 eNew = E - (dt / (2.0f * epsilon)) * (jNew + J) + (dt / (epsilon * dx)) + (dt / (epsilon * dx)) * curlH;

				float eMag = sqrtf(eNew.x * eNew.x + eNew.y * eNew.y + eNew.z * eNew.z);
				float ePeakNew = fmaxf(eMag, state.EPeak.at(i, j, k));

				float I = 0.5f * c * n * eps0 * ePeakNew * ePeakNew;
				float sNew = 1.0f / (1.0f + I / Is);

				state.F.at(i, j, k) = fNew;
				state.J.at(i, j, k) = jNew;
				state.F.at(i, j, k) = fNew;
				state.s.at(i, j, k) = sNew;
				state.E.at(i, j, k) = eNew;
				state.EPeak.at(i, j, k) = ePeakNew;
			}
		}
	}
	
	float frequency = 0.1f;
	float width = 2.0f;
	float cutoff = 1.0f;
	float pi = 3.14159265359f;

	float tPeak = cutoff * width;

	const float amplitude = 1.0f;
	const float tau = 50e-15f;
	const float t0 = 4.0f * tau;
	const float source_omega0 = 2.1e15f;
	float envelope = expf(-0.5f * powf((t - t0) / tau, 2.0f));
	float source = amplitude * envelope * cosf(source_omega0 * (t - t0));

	int ic = state.dim.x / 2;
	int jc = state.dim.y / 2;
	int kc = state.dim.z / 2;
	state.E.at(ic, jc, kc).z += source;

	float cdt_dx = state.c * state.dt / state.dx;

	// H update

	for (int k = 1; k < state.dim.z - 1; k++) {
		for (int j = 1; j < state.dim.y - 1; j++) {
			for (int i = 1; i < state.dim.x - 1; i++) {
				vec3 e = state.E.at(i, j, k);
				float cex = state.E.at(i, j, k + 1).y - e.y - state.E.at(i, j + 1, k).z + e.z;
				float cey = state.E.at(i + 1, j, k).z - e.z - state.E.at(i, j, k + 1).x + e.x;
				float cez = state.E.at(i, j + 1, k).x - e.x - state.E.at(i + 1, j, k).y + e.y;
				vec3 curlE = vec3{ cex, cey, cez };

				state.H.at(i, j, k) += cdt_dx * curlE;
			}
		}
	}

	for (int j = 1; j < state.dim.y - 1; j++) {
		for (int k = 1; k < state.dim.z - 1; k++) {
			int i = state.dim.x - 1;
			vec3 e = state.E.at(i, j, k);
			float cex = state.E.at(i, j, k + 1).y - e.y - state.E.at(i, j + 1, k).z + e.z;
			state.H.at(i, j, k).x += cdt_dx * cex;
		}
	}
	for (int k = 1; k < state.dim.z - 1; k++) {
		for (int i = 1; i < state.dim.x - 1; i++) {
			int j = state.dim.y - 1;
			vec3 e = state.E.at(i, j, k);
			float cey = state.E.at(i + 1, j, k).z - e.z - state.E.at(i, j, k + 1).x + e.x;
			state.H.at(i, j, k).y += cdt_dx * cey;
		}
	}
	for (int j = 1; j < state.dim.y - 1; j++) {
		for (int i = 1; i < state.dim.x - 1; i++) {
			int k = state.dim.z - 1;
			vec3 e = state.E.at(i, j, k);
			float cez = state.E.at(i, j + 1, k).x - e.x - state.E.at(i + 1, j, k).y + e.y;
			state.H.at(i, j, k).z += cdt_dx * cez;
		}
	}

	if (state.t - state.lastSample >= 1.0f) {
		state.lastSample = state.t;
		ivec3 p = state.resolution * ivec3{ 10, 10, 10 };
		vec3 sample = state.E.at(ic + p.x, jc + p.y, kc + p.z);
		state.samples.push_back(sample);
	}

	state.t += state.dt;
	state.step++;
}
