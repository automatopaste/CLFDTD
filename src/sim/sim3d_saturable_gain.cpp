#include "sim3d_saturable_gain.h"

#include <fmt/core.h>

#include <glm/glm.hpp>

void sim3dsat::state_init(State& state)
{
	constexpr float c0 = 299792458.0f;
	constexpr float dx0 = 12.4e-6f / 400.0f; // 400 cell simulation of 12.4um GaAs crystal
	state.lengthUnit = dx0; // Normalised simulation length unit
	state.timeUnit = dx0 / c0; // Derived simulation time unit (L / c)

	constexpr float courant = 0.5f;
	state.dx = 1.0f;
	state.c = 1.0f;
	state.dt = courant * state.dx / state.c;
	state.t = 0.0f;

	state.resolution = 1;
	//state.dim = state.resolution * ivec3{ 6000, 2, 2 }; // gain
	//state.dim = state.resolution * ivec3{ 500, 2, 2 }; // cavity 1d
	state.dim = state.resolution * ivec3{ 500, 90, 90 }; // cavity 3d
	state.npml = 12;
	state.step = 0;

	//state.epsR.resize(state.dim);
	state.E.resize(state.dim);
	state.EPeak.resize(state.dim);
	state.F.resize(state.dim);
	state.J.resize(state.dim);
	state.H.resize(state.dim);

	state.psi_dHz_dx.resize(ivec3(state.npml * 2, state.dim.y, state.dim.z));
	state.psi_dHy_dx.resize(ivec3(state.npml * 2, state.dim.y, state.dim.z));
	state.psi_dHz_dy.resize(ivec3(state.dim.x, state.npml * 2, state.dim.z));
	state.psi_dHx_dy.resize(ivec3(state.dim.x, state.npml * 2, state.dim.z));
	state.psi_dHy_dz.resize(ivec3(state.dim.x, state.dim.y, state.npml * 2));
	state.psi_dHx_dz.resize(ivec3(state.dim.x, state.dim.y, state.npml * 2));

	state.psi_dEz_dx.resize(ivec3(state.npml * 2, state.dim.y, state.dim.z));
	state.psi_dEy_dx.resize(ivec3(state.npml * 2, state.dim.y, state.dim.z));
	state.psi_dEz_dy.resize(ivec3(state.dim.x, state.npml * 2, state.dim.z));
	state.psi_dEx_dy.resize(ivec3(state.dim.x, state.npml * 2, state.dim.z));
	state.psi_dEy_dz.resize(ivec3(state.dim.x, state.dim.y, state.npml * 2));
	state.psi_dEx_dz.resize(ivec3(state.dim.x, state.dim.y, state.npml * 2));

	state.lastSample = -1.1f;
	state.samples.clear();

	ivec3 c = ivec3(state.dim.x / 2, state.dim.y / 2, state.dim.z / 2);
	for (int k = 0; k < state.dim.z; k++) {
		for (int j = 0; j < state.dim.y; j++) {
			for (int i = 0; i < state.dim.x; i++) {
				state.E.at(i, j, k) = vec3{ 0.0f, 0.0f, 0.0f };
				state.EPeak.at(i, j, k) = 0.0f;
				state.F.at(i, j, k) = vec3{ 0.0f, 0.0f, 0.0f };
				state.J.at(i, j, k) = vec3{ 0.0f, 0.0f, 0.0f };
				state.H.at(i, j, k) = vec3{ 0.0f, 0.0f, 0.0f };

				if (i < state.npml * 2) {
					int ii = i < c.x ? i : state.dim.x - i;
					state.psi_dHz_dx.at(ii, j, k) = 0.0f;
					state.psi_dHy_dx.at(ii, j, k) = 0.0f;
					state.psi_dEz_dx.at(ii, j, k) = 0.0f;
					state.psi_dEy_dx.at(ii, j, k) = 0.0f;
				}
				if (j < state.npml * 2) {
					int jj = j < c.y ? j : state.dim.y - j;
					state.psi_dHz_dy.at(i, jj, k) = 0.0f;
					state.psi_dHx_dy.at(i, jj, k) = 0.0f;
					state.psi_dEz_dy.at(i, jj, k) = 0.0f;
					state.psi_dEx_dy.at(i, jj, k) = 0.0f;
				}
				if (k < state.npml * 2) {
					int kk = k < c.z ? k : state.dim.z - k;
					state.psi_dHy_dz.at(i, j, kk) = 0.0f;
					state.psi_dHx_dz.at(i, j, kk) = 0.0f;
					state.psi_dEy_dz.at(i, j, kk) = 0.0f;
					state.psi_dEx_dz.at(i, j, kk) = 0.0f;
				}
			}
		}
	}

	for (int i = 0; i < state.npml; i++) {
		
	}

	// cpml

	state.a.resize(state.npml);
	state.b.resize(state.npml);

	float sigma_max = 6.0f;
	float kappa_max = 6.0f;
	float d = state.npml;
	float m = 3.0f;

	for (int i = 0; i < state.npml; i++) {
		float xnum = (float)i;
		float x = (state.npml - xnum) / state.npml;

		float sigma = sigma_max * powf(x, m);
		float kappa = 1.0f + (kappa_max - 1.0f) * powf(x, m);

		float sigma_eff = sigma / (1.0f + (kappa - 1.0f));
		float b = expf(-sigma_eff * 0.5f);
		float a = sigma_eff * (b - 1.0f);

		state.b[i] = b;
		state.a[i] = a;
	}
}

// PCG hash: maps a 32-bit state to a pseudorandom 32-bit value.
static uint32_t pcgHash(uint32_t& state) {
	state = state * 747796405u + 2891336453u;
	uint32_t word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
	return (word >> 22u) ^ word;
}

// Convert uint32_t to a uniform float strictly inside (0, 1).
static float randomUniform(uint32_t& state) {
	// Use 24 high-quality bits; offset avoids exactly 0 and 1.
	return (float(pcgHash(state) >> 8u) + 0.5f) / 16777216.0f;
}

// mean = 0 and variance = 1e-6.
static vec3 randomGaussianVec3(uint32_t& state)
{
	float u1 = randomUniform(state);
	float u2 = randomUniform(state);

	float r1 = sqrt(-2.0 * log(u1));
	float a1 = 6.283185307179586 * u2;

	float u3 = randomUniform(state);
	float u4 = randomUniform(state);

	float r2 = sqrt(-2.0 * log(u3));
	float a2 = 6.283185307179586 * u4;

	return 1e-3f * vec3(
		r1 * cos(a1),
		r1 * sin(a1),
		r2 * cos(a2)
	);
}

void sim3dsat::state_update(State& state)
{
	const float c = state.c;
	const float dt = state.dt;
	const float dx = state.dx;
	const float t = state.t;
	const float epsSI = 8.8541878128e-12f;
	const float mu0 = 1.25663706212e-6f;

	const float cSI = 299792458.0f;
	const float eps0 = 1.0f;

	// material properties
	const float lambdaSI = 0.89e-6f;
	const float T2SI = 0.07e-12f;
	const float n = 3.59f;

	const float sigmaSI = -5000.0f; // S/m
	const float IsSI = 1e-5f;

	const float lengthUnit = state.lengthUnit;
	const float timeUnit = state.timeUnit;
	float lambda0 = lambdaSI / lengthUnit;
	float T2 = T2SI / timeUnit;
	float sigma0 = sigmaSI * (timeUnit / epsSI);
	const float Is0 = IsSI / (epsSI * cSI);

	const float omega0 = (2.0f * 3.14159265359f * state.c) / lambda0;
	const float epsR = n * n;
	const float epsilon = eps0 * epsR;

	// gaussian carrier source
	const float amplitude = 1.0f;
	const float bandwidth = 5e12f * 5.0f;
	const float tau = (sqrt(2.0f * log(2.0f)) / (3.14159265359f * bandwidth)) / timeUnit;
	const float t0 = 4.0f * tau;
	float source_omega0 = 2.0 * 3.14159265359 * (cSI / lambdaSI) * timeUnit;
	float envelope = expf(-0.5 * powf((t - t0) / tau, 2.0f));
	float source = amplitude * envelope * cosf(source_omega0 * (t - t0));

	const ivec3 sourcePosition = ivec3(25, state.dim.y / 2, state.dim.z / 2);
	const float sourceRadiusSI = 1e-9;
	float sourceRadius = sourceRadiusSI / lengthUnit;

	constexpr bool PERIODIC_X = false;
	constexpr bool PERIODIC_Y = false;
	constexpr bool PERIODIC_Z = false;

	constexpr int iLim = PERIODIC_X ? 0 : 1;
	constexpr int jLim = PERIODIC_Y ? 0 : 1;
	constexpr int kLim = PERIODIC_Z ? 0 : 1;

	int ic = state.dim.x / 2;
	int jc = state.dim.y / 2;
	int kc = state.dim.z / 2;

	// E update

	for (int k = kLim; k < state.dim.z; k++) {
		for (int j = jLim; j < state.dim.y; j++) {
			for (int i = iLim; i < state.dim.x; i++) {
				ivec3 ijk = ivec3(i, j, k);
				ivec3 ijk_im1 = ijk + ivec3(-1, 0, 0);
				ivec3 ijk_jm1 = ijk + ivec3(0, -1, 0);
				ivec3 ijk_km1 = ijk + ivec3(0, 0, -1);

				if (PERIODIC_X && ijk_im1.x < 0) ijk_im1.x = state.dim.x - 1;
				if (PERIODIC_Y && ijk_jm1.y < 0) ijk_jm1.y = state.dim.y - 1;
				if (PERIODIC_Z && ijk_km1.z < 0) ijk_km1.z = state.dim.z - 1;

				vec3 h = state.H.at(i, j, k);
				float dHz_dx = h.z - state.H.at(ijk_im1).z;
				float dHy_dx = h.y - state.H.at(ijk_im1).y;
				float dHz_dy = h.z - state.H.at(ijk_jm1).z;
				float dHx_dy = h.x - state.H.at(ijk_jm1).x;
				float dHy_dz = h.y - state.H.at(ijk_km1).y;
				float dHx_dz = h.x - state.H.at(ijk_km1).x;

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
					float a = state.a[ni];
					float b = state.b[ni];
					int xLoc = ijk.x < ic ? ni : (state.npml * 2) - 1 - ni;
					ivec3 loc = ivec3(xLoc, j, k);
					psi_dHz_dx_new = b * state.psi_dHz_dx.at(loc) + a * dHz_dx;
					psi_dHy_dx_new = b * state.psi_dHy_dx.at(loc) + a * dHy_dx;
					state.psi_dHz_dx.at(loc) = psi_dHz_dx_new;
					state.psi_dHy_dx.at(loc) = psi_dHy_dx_new;
				}
				if (nj < state.npml) {
					float a = state.a[nj];
					float b = state.b[nj];
					int yLoc = ijk.y < ic ? nj : (state.npml * 2) - 1 - nj;
					ivec3 loc = ivec3(i, yLoc, k);
					psi_dHz_dy_new = b * state.psi_dHz_dy.at(loc) + a * dHz_dy;
					psi_dHx_dy_new = b * state.psi_dHx_dy.at(loc) + a * dHx_dy;
					state.psi_dHz_dy.at(loc) = psi_dHz_dy_new;
					state.psi_dHx_dy.at(loc) = psi_dHx_dy_new;
				}
				if (nk < state.npml) {
					float a = state.a[nk];
					float b = state.b[nk];
					int zLoc = ijk.z < ic ? nk : (state.npml * 2) - 1 - nk;
					ivec3 loc = ivec3(i, j, zLoc);
					psi_dHy_dz_new = b * state.psi_dHy_dz.at(loc) + a * dHy_dz;
					psi_dHx_dz_new = b * state.psi_dHx_dz.at(loc) + a * dHx_dz;
					state.psi_dHy_dz.at(loc) = psi_dHy_dz_new;
					state.psi_dHx_dz.at(loc) = psi_dHx_dz_new;
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
				
				float ePeak = state.EPeak.at(i, j, k);
				float I = 0.5f * c * n * eps0 * ePeak * ePeak;
				float s = 1.0f / (1.0f + I / Is0);

				float Z2 = (1.0f + omega0 * omega0 * T2 * T2);

				float B = 8.0f * epsilon * T2 * (dt + T2) + dt * dt * (2.0f * epsilon * Z2 + s * sigma0 * (dt + 2.0f * T2));

				float A1 = (4.0f * dt * s * sigma0 * (dt + 2.0f * T2)) / (B * dx);
				float A2 = (8.0f * epsilon * s * sigma0 * dt) / B;
				float A3 = -(4.0f * dt * (2.0f * epsilon * Z2 + s * sigma0 * (dt + 2.0f * T2))) / B;
				float A4 = (-(8.0f * epsilon * T2 * (dt - T2)) / B) - ((dt * dt * (2.0f * epsilon * Z2 + s * sigma0 * (dt + 2.0f * T2))) / B);

				vec3 E = state.E.at(i, j, k);
				vec3 J = state.J.at(i, j, k);
				vec3 F = state.F.at(i, j, k);

				if (abs(i - sourcePosition.x) < sourceRadius) {
					E.z -= source;
				}

				vec3 fNew = A1 * curlH + A2 * E + A3 * J + A4 * F;
				vec3 jNew = J + (dt / 2.0f) * (fNew + F);
				vec3 eNew = E - (dt / (2.0f * epsilon)) * (jNew + J) + (dt / (epsilon * dx)) * curlH;

				const uint32_t seed = 0x96767AE1u;
				uint32_t seedState = seed ^ (i * 0x9E3779B9u) ^ (state.step * 0x85EBCA6Bu);
				vec3 randomGaussian = randomGaussianVec3(seedState);
				eNew.z += randomGaussian.x; // one-dimensional only

				float eMag = sqrtf(eNew.x * eNew.x + eNew.y * eNew.y + eNew.z * eNew.z);
				float ePeakNew = fmaxf(eMag, state.EPeak.at(i, j, k));				

				state.F.at(i, j, k) = fNew;
				state.J.at(i, j, k) = jNew;
				state.E.at(i, j, k) = eNew;
				state.EPeak.at(i, j, k) = ePeakNew;
			}
		}
	}

	vec3 sample = state.E.at(ic, jc, kc);
	state.samples.push_back(sample);

	float cdt_dx = state.dt / (state.dx * mu0);

	// H update

	for (int k = kLim; k < state.dim.z - 1; k++) {
		for (int j = jLim; j < state.dim.y - 1; j++) {
			for (int i = iLim; i < state.dim.x - 1; i++) {
				ivec3 ijk = ivec3(i, j, k);
				ivec3 ijk_ip1 = ijk + ivec3(1, 0, 0);
				ivec3 ijk_jp1 = ijk + ivec3(0, 1, 0);
				ivec3 ijk_kp1 = ijk + ivec3(0, 0, 1);

				if (PERIODIC_X && ijk_ip1.x >= state.dim.x) ijk_ip1.x = 0;
				if (PERIODIC_Y && ijk_jp1.y >= state.dim.y) ijk_jp1.y = 0;
				if (PERIODIC_Z && ijk_kp1.z >= state.dim.z) ijk_kp1.z = 0;

				float dEz_dx = 0.0;
				float dEy_dx = 0.0;
				float dEz_dy = 0.0;
				float dEx_dy = 0.0;
				float dEy_dz = 0.0;
				float dEx_dz = 0.0;

				vec3 e = state.E.at(i, j, k);

				if (PERIODIC_X || ijk.x < state.dim.x - 1) {
					vec3 ex1 = state.E.at(ijk_ip1);
					dEz_dx = ex1.z - e.z;
					dEy_dx = ex1.y - e.y;
				}
				if (PERIODIC_Y || ijk.y < state.dim.y - 1) {
					vec3 ey1 = state.E.at(ijk_jp1);
					dEz_dy = ey1.z - e.z;
					dEx_dy = ey1.x - e.x;
				}
				if (PERIODIC_Z || ijk.z < state.dim.z - 1) {
					vec3 ez1 = state.E.at(ijk_kp1);
					dEy_dz = ez1.y - e.y;
					dEx_dz = ez1.x - e.x;
				}

				int ni = 0;
				int nj = 0;
				int nk = 0;

				if (!PERIODIC_X) ni = std::min(i, state.dim.x - 1 - i);
				if (!PERIODIC_Y) nj = std::min(j, state.dim.y - 1 - j);
				if (!PERIODIC_Z) nk = std::min(k, state.dim.z - 1 - k);

				float psi_dEz_dx_new = 0.0f;
				float psi_dEy_dx_new = 0.0f;
				float psi_dEz_dy_new = 0.0f;
				float psi_dEx_dy_new = 0.0f;
				float psi_dEy_dz_new = 0.0f;
				float psi_dEx_dz_new = 0.0f;

				if (!PERIODIC_X && ni < state.npml) {
					float a = state.a[ni];
					float b = state.b[ni];
					int xLoc = ijk.x < ic ? ni : (state.npml * 2) - 1 - ni;
					ivec3 loc = ivec3(xLoc, j, k);
					psi_dEz_dx_new = b * state.psi_dEz_dx.at(loc) + a * dEz_dx;
					psi_dEy_dx_new = b * state.psi_dEy_dx.at(loc) + a * dEy_dx;
					state.psi_dEz_dx.at(loc) = psi_dEz_dx_new;
					state.psi_dEy_dx.at(loc) = psi_dEy_dx_new;
				}
				if (!PERIODIC_Y && nj < state.npml) {
					float a = state.a[nj];
					float b = state.b[nj];
					int yLoc = ijk.y < ic ? nj : (state.npml * 2) - 1 - nj;
					ivec3 loc = ivec3(i, yLoc, k);
					psi_dEz_dy_new = state.b[nj] * state.psi_dEz_dy.at(loc) + a * dEz_dy;
					psi_dEx_dy_new = state.b[nj] * state.psi_dEx_dy.at(loc) + a * dEx_dy;
					state.psi_dEz_dy.at(loc) = psi_dEz_dy_new;
					state.psi_dEx_dy.at(loc) = psi_dEx_dy_new;
				}
				if (!PERIODIC_Z && nk < state.npml) {
					float a = state.a[nk];
					float b = state.b[nk];
					int zLoc = ijk.z < ic ? nk : (state.npml * 2) - 1 - nk;
					ivec3 loc = ivec3(i, j, zLoc);
					psi_dEy_dz_new = state.b[nk] * state.psi_dEy_dz.at(loc) + a * dEy_dz;
					psi_dEx_dz_new = state.b[nk] * state.psi_dEx_dz.at(loc) + a * dEx_dz;
					state.psi_dEy_dz.at(loc) = psi_dEy_dz_new;
					state.psi_dEx_dz.at(loc) = psi_dEx_dz_new;
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

	state.t += state.dt;
	state.step++;
}
