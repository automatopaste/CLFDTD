#pragma once

#include <vector>

#include "types.h"

#include "SDL3/SDL.h"

namespace sim {
	struct State;
	struct State2D;
}

namespace sim2d {
	struct State;
}
namespace sim2d_dispersion {
	struct State;
}

namespace sim3d {
	struct State;
}

namespace render {

	struct Data {
		SDL_Window* window;
		SDL_Renderer* renderer;
		SDL_Texture* texture;
		u32 width;
		u32 height;

		u32 numSteps;

		std::vector<u32> pixels;
	};

	void init_data(Data& data, u32 width, u32 height);
	void cleanup_data(Data& data);

	void render_state(Data& data, sim::State* state);
	void render_frame(Data& data);

	void render_state_2d(Data& data, sim::State2D* state);

	void render_sim_2d(Data& data, sim2d::State* state);
	void render_sim_2d_dispersion(Data& data, sim2d_dispersion::State* state);

	void render_sim_3d(Data& data, sim3d::State* state);

}