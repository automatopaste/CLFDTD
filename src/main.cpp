#include <cstdint>
#include <iostream>
#include <thread>
#include <chrono>

#include "glm/glm.hpp"

#include "fmt/core.h"

#include "sim/sim.h"
#include "sim/sim2d.h"
#include "sim/renderer.h"

#include "vk/vk_engine.h"

void run_1d() {
	sim::State state{};
	sim::init_sim(state);

	render::Data renderData{};
	render::init_data(renderData, 1600, 800);

	bool quit = false;

	while (!quit)
	{
		SDL_Event event;;
		while (SDL_PollEvent(&event)) {
			switch (event.type)
			{
			case SDL_EVENT_QUIT:
				quit = true;
				break;
			default:
				break;
			}
		}

		sim::step_sim(state);

		state.step++;

		render::render_state(renderData, &state);
		render::render_frame(renderData);

		std::this_thread::sleep_for(std::chrono::milliseconds(5));
	}
}

void run_2d() {
	sim::State2D state{};
	sim::init_sim_2d(state);

	render::Data renderData{};
	render::init_data(renderData, 1600, 800);

	bool quit = false;
	while (!quit)
	{
		SDL_Event event;;
		while (SDL_PollEvent(&event)) {
			switch (event.type)
			{
			case SDL_EVENT_QUIT:
				quit = true;
				break;
			default:
				break;
			}
		}

		if (state.step < 12000) {
			sim::step_sim_2d(state);
		}

		if (state.step % 10 == 0) {
			render::render_state_2d(renderData, &state);
			render::render_frame(renderData);
		}

		//std::this_thread::sleep_for(std::chrono::milliseconds(16));
	}
}

void run_sim_2d() {
	sim2d::State state{};
	sim2d::state_init(state);

	render::Data renderData{};
	render::init_data(renderData, 800, 800);

	bool quit = false;
	bool fileWritten = false;

	while (!quit)
	{
		SDL_Event event;;
		while (SDL_PollEvent(&event)) {
			switch (event.type)
			{
			case SDL_EVENT_QUIT:
				quit = true;
				break;
			default:
				break;
			}
		}

		if (state.step < 12000) {
			sim2d::state_update(state);
		}
		
		if (state.step >= 12000 && !fileWritten) {
			fileWritten = true;

			FILE* f = fopen("data.bin", "wb");
			if (!f) {
				return;
			}

			fwrite(state.samples.data(), sizeof(float), state.samples.size(), f);
			fclose(f);
		}

		if (state.step % 1 == 0) {
			render::render_sim_2d(renderData, &state);
			render::render_frame(renderData);
		}

		//std::this_thread::sleep_for(std::chrono::milliseconds(16));
	}
}

int main() {
	fmt::print("initialising CLFDTD simulation\n");

	//run_sim_2d();

	// run shader src copy batch file
	system("call ../../../batch_shader_copy.bat");

	VulkanEngine engine{};

	engine.init();

	engine.run();

	return 0;
}