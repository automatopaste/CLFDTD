#include "renderer.h"

#include <format>

#include "sim/sim.h"
#include "sim/sim2d.h"
#include "sim/sim2d_dispersion.h"
#include "sim/sim3d.h"
#include "sim/sim3d_dispersion.h"
#include "sim/sim3d_saturable_gain.h"

#include "glm/glm.hpp"

void render::init_data(Data& data, u32 width, u32 height)
{
	data.width = width;
	data.height = height;

	data.window = SDL_CreateWindow("FDTD Simulation", width, height, 0);
	data.renderer = SDL_CreateRenderer(data.window, nullptr);
	data.texture = SDL_CreateTexture(data.renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, width, height);
	data.pixels.resize(width * height);

	for (u32 x = 0; x < width; x++)
	{
		for (u32 y = 0; y < height; y++)
		{
			u32 index = y * width + x;
			data.pixels[index] = (y ^ x) % 2 == 0 ? 0xFF0000FF : 0xFFFF0000; // checkerboard pattern
		}
	}
}

void render::cleanup_data(Data& data)
{
	SDL_DestroyTexture(data.texture);
	SDL_DestroyRenderer(data.renderer);
	SDL_DestroyWindow(data.window);
	SDL_Quit();
}

void render::render_state(Data& data, sim::State* state)
{
	sim::State& s = *state;

	data.numSteps = state->step;

	u32 width = data.width;
	u32 height = data.height;

	for (u32 x = 0; x < width; x++)
	{
		for (u32 y = 0; y < height; y++)
		{
			u32 index = y * width + x;

			if (((x / 2) + 1) < s.dimension)
			{
				const double E_SCALE = 500.0;
				const double H_SCALE = 100000.0;

				double e = s.eField[x / 2] * E_SCALE;
				double h = s.hField[x / 2] * H_SCALE;

				const double MAX_VALUE = 1000.0;
				e = (glm::clamp(e, -MAX_VALUE, MAX_VALUE) / MAX_VALUE) * 0.5 + 0.5;
				h = (glm::clamp(h, -MAX_VALUE, MAX_VALUE) / MAX_VALUE) * 0.5 + 0.5;

				float eYD = glm::abs(((float)height * e) - float(y));
				float eYM = glm::clamp(5.0f - eYD, 0.0f, 1.0f);
				u8 eI = (u8)(std::min(std::max(eYM * 255.0, 0.0), 255.0));

				float mYD = glm::abs(((float)height * h) - float(y));
				float mYM = glm::clamp(5.0f - mYD, 0.0f, 1.0f);
				u8 hI = (u8)(std::min(std::max(mYM * 255.0, 0.0), 255.0));

				data.pixels[index] = (u32)((0xFF << 24) | (eI << 16) | (0x00 << 8) | hI); // electric field in blue, magnetic field in red
			}
			else {
				data.pixels[index] = 0xFF000000; // black for areas outside the simulation dimension
			}
		}
	}
}

void render::render_frame(Data& data)
{
	SDL_UpdateTexture(data.texture, nullptr, data.pixels.data(), data.width * sizeof(u32));

	SDL_RenderClear(data.renderer);
	SDL_RenderTexture(data.renderer, data.texture, nullptr, nullptr);
	SDL_RenderPresent(data.renderer);

	std::string title = std::format("FDTD Simulation - {} steps", data.numSteps);
	SDL_SetWindowTitle(data.window, title.c_str());
}

void render::render_state_2d(Data& data, sim::State2D* state)
{
	sim::State2D& s = *state;

	data.numSteps = state->step;

	u32 width = data.width;
	u32 height = data.height;

	for (u32 x = 0; x < width; x++)
	{
		for (u32 y = 0; y < height; y++)
		{
			u32 index = y * width + x;

			if ((x + 4) < s.dimensions.x && (y + 4) < s.dimensions.y)
			{
				const double E_SCALE = 50000.0;
				const double H_SCALE = 10000000.0;

				double ez = s.Ez[y + s.dimensions.y * x] * E_SCALE;
				double hx = s.Hx[y + s.dimensions.y * x] * H_SCALE;
				double hy = s.Hy[y + (s.dimensions.y - 1) * x] * H_SCALE;

				const double MAX_VALUE = 1000.0;
				double electricR = abs(glm::clamp(ez, 0.0, MAX_VALUE) / MAX_VALUE);
				double electricB = abs(glm::clamp(ez, -MAX_VALUE, 0.0) / MAX_VALUE);
				hx = abs(glm::clamp(hx, -MAX_VALUE, MAX_VALUE) / MAX_VALUE);
				hy = abs(glm::clamp(hy, -MAX_VALUE, MAX_VALUE) / MAX_VALUE);
				double magneticG = (hx + hy) / 2.0;

				u8 eR = (u8)(std::min(std::max(electricR * 255.0, 0.0), 255.0));
				u8 eG = (u8)(std::min(std::max(electricB * 255.0, 0.0), 255.0));
				u8 mB = (u8)(std::min(std::max(magneticG * 255.0, 0.0), 255.0));

				data.pixels[index] = (u32)((0xFF << 24) | (eG << 16) | (0 << 8) | eR);
			}
			else {
				data.pixels[index] = 0xFF000000; // black for areas outside the simulation dimension
			}
		}
	}
}

void render::render_sim_2d(Data& data, sim2d::State* state)
{
	sim2d::State& s = *state;

	data.numSteps = state->step;

	u32 width = data.width;
	u32 height = data.height;

	for (u32 x = 0; x < width; x++)
	{
		for (u32 y = 0; y < height; y++)
		{
			u32 index = y * width + x;

			if ((x + 4) < s.x && (y + 4) < s.y)
			{
				float ez = s.ez[y + s.y * x] * 100.0f;
				float ga = s.ga[y + s.y * x] * 0.0f;
				//float hx = s.hx[y + s.y * x] * H_SCALE;
				//float hy = s.hy[y + (s.y - 1) * x] * H_SCALE;

				const float MAX_VALUE = 1.0f;
				float electricR = abs(glm::clamp(ez, 0.0f, MAX_VALUE) / MAX_VALUE);
				float electricB = abs(glm::clamp(ez, -MAX_VALUE, 0.0f) / MAX_VALUE);
				//hx = abs(glm::clamp(hx, -MAX_VALUE, MAX_VALUE) / MAX_VALUE);
				//hy = abs(glm::clamp(hy, -MAX_VALUE, MAX_VALUE) / MAX_VALUE);
				//float magneticG = (hx + hy) / 2.0f;

				u8 R = (u8)(std::min(std::max(electricR * 255.0f, 0.0f), 255.0f));
				u8 G = (u8)(std::min(std::max(ga * 255.0f, 0.0f), 255.0f));
				u8 B = (u8)(std::min(std::max(electricB * 255.0f, 0.0f), 255.0f));

				data.pixels[index] = (u32)((0xFF << 24) | (B << 16) | (G << 8) | R);
			}
			else {
				data.pixels[index] = 0xFF000000; // black for areas outside the simulation dimension
			}
		}
	}
}

void render::render_sim_2d_dispersion(Data& data, sim2d_dispersion::State* state)
{
	sim2d_dispersion::State& s = *state;

	data.numSteps = state->step;

	u32 width = data.width;
	u32 height = data.height;

	for (u32 x = 0; x < width; x++)
	{
		for (u32 y = 0; y < height; y++)
		{
			u32 index = y * width + x;

			if ((x + 4) < s.x && (y + 4) < s.y)
			{
				float ez = s.ez[y + s.y * x] * 10.0f;
				float ga = 0.0f;
				//float hx = s.hx[y + s.y * x] * H_SCALE;
				//float hy = s.hy[y + (s.y - 1) * x] * H_SCALE;

				const float MAX_VALUE = 1.0f;
				float electricR = abs(glm::clamp(ez, 0.0f, MAX_VALUE) / MAX_VALUE);
				float electricB = abs(glm::clamp(ez, -MAX_VALUE, 0.0f) / MAX_VALUE);
				//hx = abs(glm::clamp(hx, -MAX_VALUE, MAX_VALUE) / MAX_VALUE);
				//hy = abs(glm::clamp(hy, -MAX_VALUE, MAX_VALUE) / MAX_VALUE);
				//float magneticG = (hx + hy) / 2.0f;

				u8 R = (u8)(std::min(std::max(electricR * 255.0f, 0.0f), 255.0f));
				u8 G = (u8)(std::min(std::max(ga * 255.0f, 0.0f), 255.0f));
				u8 B = (u8)(std::min(std::max(electricB * 255.0f, 0.0f), 255.0f));

				data.pixels[index] = (u32)((0xFF << 24) | (B << 16) | (G << 8) | R);
			}
			else {
				data.pixels[index] = 0xFF000000; // black for areas outside the simulation dimension
			}
		}
	}
}

void render::render_sim_3d(Data& data, sim3d::State* state)
{
	sim3d::State& s = *state;

	data.numSteps = state->step;

	u32 width = data.width;
	u32 height = data.height;

	for (u32 x = 0; x < width; x++)
	{
		for (u32 y = 0; y < height; y++)
		{
			if ((x + 4) < s.dim.x && (y + 4) < s.dim.y)
			{
				int kc = s.dim.z / 2;
				vec3 e = s.e.at(x, y, kc) / 0.01f;
				float ga = 0.0f;

				vec3 eScaled = e * 0.5f + 0.5f;

				u8 R = (u8)glm::clamp(eScaled.x * 255.0f, 0.0f, 255.0f);
				u8 G = (u8)glm::clamp(eScaled.y * 255.0f, 0.0f, 255.0f);
				u8 B = (u8)glm::clamp(eScaled.z * 255.0f, 0.0f, 255.0f);

				data.pixels[y * width + x] = (u32)((0xFF << 24) | (B << 16) | (G << 8) | R);
			}
			else {
				data.pixels[y * width + x] = 0xFF000000; // black for areas outside the simulation dimension
			}
		}
	}
}

void render::render_sim_3d_dispersion(Data& data, sim3ddisp::State* state)
{
	sim3ddisp::State& s = *state;

	data.numSteps = state->step;

	u32 width = data.width;
	u32 height = data.height;

	for (u32 x = 0; x < width; x++)
	{
		for (u32 y = 0; y < height; y++)
		{
			if ((x + 4) < s.dim.x && (y + 4) < s.dim.y)
			{
				int kc = s.dim.z / 2;
				vec3 e = s.e.at(x, y, kc) / 1e-20f;
				float ga = 0.0f;

				vec3 eScaled = e * 0.5f + 0.5f;

				u8 R = (u8)glm::clamp(eScaled.x * 255.0f, 0.0f, 255.0f);
				u8 G = (u8)glm::clamp(eScaled.y * 255.0f, 0.0f, 255.0f);
				u8 B = (u8)glm::clamp(eScaled.z * 255.0f, 0.0f, 255.0f);

				data.pixels[y * width + x] = (u32)((0xFF << 24) | (B << 16) | (G << 8) | R);
			}
			else {
				data.pixels[y * width + x] = 0xFF000000; // black for areas outside the simulation dimension
			}
		}
	}
}

void render::render_sim_3d_saturable_gain(Data& data, sim3dsat::State* state)
{
	sim3dsat::State& s = *state;

	data.numSteps = state->step;

	u32 width = data.width;
	u32 height = data.height;

	for (u32 x = 0; x < width; x++)
	{
		for (u32 y = 0; y < height; y++)
		{
			if ((x + 4) < s.dim.x && (y + 4) < s.dim.y)
			{
				int kc = s.dim.z / 2;
				vec3 e = s.E.at(x, y, kc) * 1e4f;
				float ga = 0.0f;

				vec3 eScaled = e * 0.5f + 0.5f;

				u8 R = (u8)glm::clamp(eScaled.x * 255.0f, 0.0f, 255.0f);
				u8 G = (u8)glm::clamp(eScaled.y * 255.0f, 0.0f, 255.0f);
				u8 B = (u8)glm::clamp(eScaled.z * 255.0f, 0.0f, 255.0f);

				data.pixels[y * width + x] = (u32)((0xFF << 24) | (B << 16) | (G << 8) | R);
			}
			else {
				data.pixels[y * width + x] = 0xFF000000; // black for areas outside the simulation dimension
			}
		}
	}
}
