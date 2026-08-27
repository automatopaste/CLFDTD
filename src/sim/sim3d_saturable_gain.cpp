#include "sim3d_saturable_gain.h"

#include <fmt/core.h>

#include <glm/glm.hpp>

void sim3dsat::state_init(State& state)
{
	state.resolution = 1;
	state.dim = state.resolution * ivec3{ 200, 200, 25 };
	state.npml = 10;
	state.step = 0;

	//state.epsR.resize(state.dim);
	state.E.resize(state.dim);
	state.EPeak.resize(state.dim);
	state.F.resize(state.dim);
	state.J.resize(state.dim);
	state.H.resize(state.dim);
	state.s.resize(state.dim);

	state.psi_dHz_dx.resize(state.dim);
	state.psi_dHy_dx.resize(state.dim);
	state.psi_dHz_dy.resize(state.dim);
	state.psi_dHx_dy.resize(state.dim);
	state.psi_dHy_dz.resize(state.dim);
	state.psi_dHx_dz.resize(state.dim);

	state.psi_dEz_dx.resize(state.dim);
	state.psi_dEy_dx.resize(state.dim);
	state.psi_dEz_dy.resize(state.dim);
	state.psi_dEx_dy.resize(state.dim);
	state.psi_dEy_dz.resize(state.dim);
	state.psi_dEx_dz.resize(state.dim);

	state.lastSample = -1.1f;
	state.samples.clear();

	state.dx = 10e-10f / (float)state.resolution;
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

				state.psi_dHz_dx.at(i, j, k) = 0.0f;
				state.psi_dHy_dx.at(i, j, k) = 0.0f;
				state.psi_dHz_dy.at(i, j, k) = 0.0f;
				state.psi_dHx_dy.at(i, j, k) = 0.0f;
				state.psi_dHy_dz.at(i, j, k) = 0.0f;
				state.psi_dHx_dz.at(i, j, k) = 0.0f;
				state.psi_dEz_dx.at(i, j, k) = 0.0f;
				state.psi_dEy_dx.at(i, j, k) = 0.0f;
				state.psi_dEz_dy.at(i, j, k) = 0.0f;
				state.psi_dEx_dy.at(i, j, k) = 0.0f;
				state.psi_dEy_dz.at(i, j, k) = 0.0f;
				state.psi_dEx_dz.at(i, j, k) = 0.0f;
			}
		}
	}

	// cpml

	state.a.resize(state.npml);
	state.b.resize(state.npml);

	float sigma_max = 3.0f;
	float kappa_max = 3.0f;
	float d = state.npml;
	float m = 2.0f;

	for (int i = 0; i < state.npml; i++) {
		float xnum = (float)i;
		float x = (state.npml - xnum) / state.npml;

		float sigma = sigma_max * powf(x, m);
		float kappa = 1.0f + (kappa_max - 1.0f) * powf(x, m);

		float sigma_eff = sigma / (1.0f + (kappa - 1.0f));
		float b = expf(-sigma_eff * 0.5f);
		float a = sigma_eff * (state.b[i] - 1.0f);

		state.b[i] = b;
		state.a[i] = a;
	}
}

void sim3dsat::state_update(State& state)
{
	const float c = state.c;
	const float dt = state.dt;
	const float dx = state.dx;
	const float t = state.t;
	const float eps0 = 8.8541878128e-12f;
	const float mu0 = 1.25663706212e-6f;

	// material properties
	const float lambda0 = 0.89e-6f;
	const float T2 = 0.07e-12f;
	const float n = 3.59f;

	const float sigma0 = -5000.0f; // S/m
	const float Is = 1e-5f;

	const float omega0 = (2.0f * 3.14159265359f * state.c) / lambda0;
	const float epsR = n * n;
	const float epsilon = eps0 * epsR;

	// E update

	for (int k = 1; k < state.dim.z; k++) {
		for (int j = 1; j < state.dim.y; j++) {
			for (int i = 1; i < state.dim.x; i++) {
				vec3 h = state.H.at(i, j, k);
				float dHz_dx = h.z - state.H.at(i - 1, j, k).z;
				float dHy_dx = h.y - state.H.at(i - 1, j, k).y;
				float dHz_dy = h.z - state.H.at(i, j - 1, k).z;
				float dHx_dy = h.x - state.H.at(i, j - 1, k).x;
				float dHy_dz = h.y - state.H.at(i, j, k - 1).y;
				float dHx_dz = h.x - state.H.at(i, j, k - 1).x;

				int ni = std::min(i, state.dim.x - 1 - i);
				int nj = std::min(j, state.dim.y - 1 - j);
				int nk = std::min(k, state.dim.z - 1 - k);

				float psi_dHz_dx_new = 0.0f;
				float psi_dHy_dx_new = 0.0f;
				float psi_dHz_dy_new = 0.0f;
				float psi_dHx_dy_new = 0.0f;
				float psi_dHy_dz_new = 0.0f;
				float psi_dHx_dz_new = 0.0f;

				if (ni < state.npml) {
					psi_dHz_dx_new = state.b[ni] * state.psi_dHz_dx.at(i, j, k) + state.a[ni] * dHz_dx;
					psi_dHy_dx_new = state.b[ni] * state.psi_dHy_dx.at(i, j, k) + state.a[ni] * dHy_dx;
					state.psi_dHz_dx.at(i, j, k) = psi_dHz_dx_new;
					state.psi_dHy_dx.at(i, j, k) = psi_dHy_dx_new;
				}
				if (nj < state.npml) {
					psi_dHz_dy_new = state.b[nj] * state.psi_dHz_dy.at(i, j, k) + state.a[nj] * dHz_dy;
					psi_dHx_dy_new = state.b[nj] * state.psi_dHx_dy.at(i, j, k) + state.a[nj] * dHx_dy;
					state.psi_dHz_dy.at(i, j, k) = psi_dHz_dy_new;
					state.psi_dHx_dy.at(i, j, k) = psi_dHx_dy_new;
				}
				if (nk < state.npml) {
					psi_dHy_dz_new = state.b[nk] * state.psi_dHy_dz.at(i, j, k) + state.a[nk] * dHy_dz;
					psi_dHx_dz_new = state.b[nk] * state.psi_dHx_dz.at(i, j, k) + state.a[nk] * dHx_dz;
					state.psi_dHy_dz.at(i, j, k) = psi_dHy_dz_new;
					state.psi_dHx_dz.at(i, j, k) = psi_dHx_dz_new;
				}

				vec3 curlH = {
					(dHz_dy + psi_dHz_dy_new) -
					(dHy_dz + psi_dHy_dz_new),

					(dHx_dz + psi_dHx_dz_new) -
					(dHz_dx + psi_dHz_dx_new),

					(dHy_dx + psi_dHy_dx_new) -
					(dHx_dy + psi_dHx_dy_new)
				};

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
				vec3 eNew = E - (dt / (2.0f * epsilon)) * (jNew + J) + (dt / (epsilon * dx)) * curlH;

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

	const float amplitude = 1e-2f;
	const float tau = 100e-15f;
	const float t0 = 4.0f * tau;
	const float source_omega0 = 2.1e15f;
	float envelope = expf(-0.5f * powf((t - t0) / tau, 2.0f));
	float source = amplitude * envelope * cosf(source_omega0 * (t - t0));

	int ic = state.dim.x / 2;
	int jc = state.dim.y / 2;
	int kc = state.dim.z / 2;
	for (int k = 0; k < state.dim.z; ++k) {
		for (int j = 0; j < state.dim.y; ++j) {
			//state.E.at(ic, j, k).z += source;
		}
	}
	state.E.at(ic, jc, kc).z += 1.0f;

	float cdt_dx = state.dt / (state.dx * mu0);

	// H update

	for (int k = 1; k < state.dim.z - 1; k++) {
		for (int j = 1; j < state.dim.y - 1; j++) {
			for (int i = 1; i < state.dim.x - 1; i++) {
				/*vec3 e = state.E.at(i, j, k);
				float cex = state.E.at(i, j, k + 1).y - e.y - state.E.at(i, j + 1, k).z + e.z;
				float cey = state.E.at(i + 1, j, k).z - e.z - state.E.at(i, j, k + 1).x + e.x;
				float cez = state.E.at(i, j + 1, k).x - e.x - state.E.at(i + 1, j, k).y + e.y;
				vec3 curlE = vec3{ cex, cey, cez };*/

				vec3 e = state.E.at(i, j, k);
				float dEz_dx = state.E.at(i + 1, j, k).z - e.z;
				float dEy_dx = state.E.at(i + 1, j, k).y - e.y;
				float dEz_dy = state.E.at(i, j + 1, k).z - e.z;
				float dEx_dy = state.E.at(i, j + 1, k).x - e.x;
				float dEy_dz = state.E.at(i, j, k + 1).y - e.y;
				float dEx_dz = state.E.at(i, j, k + 1).x - e.x;

				int ni = std::min(i, state.dim.x - 1 - i);
				int nj = std::min(j, state.dim.y - 1 - j);
				int nk = std::min(k, state.dim.z - 1 - k);

				float psi_dEz_dx_new = 0.0f;
				float psi_dEy_dx_new = 0.0f;
				float psi_dEz_dy_new = 0.0f;
				float psi_dEx_dy_new = 0.0f;
				float psi_dEy_dz_new = 0.0f;
				float psi_dEx_dz_new = 0.0f;

				if (ni < state.npml) {
					psi_dEz_dx_new = state.b[ni] * state.psi_dEz_dx.at(i, j, k) + state.a[ni] * dEz_dx;
					psi_dEy_dx_new = state.b[ni] * state.psi_dEy_dx.at(i, j, k) + state.a[ni] * dEy_dx;
					state.psi_dEz_dx.at(i, j, k) = psi_dEz_dx_new;
					state.psi_dEy_dx.at(i, j, k) = psi_dEy_dx_new;
				}
				if (nj < state.npml) {
					psi_dEz_dy_new = state.b[nj] * state.psi_dEz_dy.at(i, j, k) + state.a[nj] * dEz_dy;
					psi_dEx_dy_new = state.b[nj] * state.psi_dEx_dy.at(i, j, k) + state.a[nj] * dEx_dy;
					state.psi_dEz_dy.at(i, j, k) = psi_dEz_dy_new;
					state.psi_dEx_dy.at(i, j, k) = psi_dEx_dy_new;
				}
				if (nk < state.npml) {
					psi_dEy_dz_new = state.b[nk] * state.psi_dEy_dz.at(i, j, k) + state.a[nk] * dEy_dz;
					psi_dEx_dz_new = state.b[nk] * state.psi_dEx_dz.at(i, j, k) + state.a[nk] * dEx_dz;
					state.psi_dEy_dz.at(i, j, k) = psi_dEy_dz_new;
					state.psi_dEx_dz.at(i, j, k) = psi_dEx_dz_new;
				}

				vec3 curlE = {
					(dEy_dz + psi_dEy_dz_new) -
					(dEz_dy + psi_dEz_dy_new),

					(dEz_dx + psi_dEz_dx_new) -
					(dEx_dz + psi_dEx_dz_new),

					(dEx_dy + psi_dEx_dy_new) -
					(dEy_dx + psi_dEy_dx_new)				
				};

				state.H.at(i, j, k) += cdt_dx * curlE;
			}
		}
	}

	// special case H field edges do not use CPML : fix later

	for (int j = 1; j < state.dim.y - 1; j++) {
		for (int k = 1; k < state.dim.z - 1; k++) {
			int i = state.dim.x - 1;
			vec3 e = state.E.at(i, j, k);
			float dEy_dz = state.E.at(i, j, k + 1).y - e.y;
			float dEz_dy = state.E.at(i, j + 1, k).z - e.z;

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
