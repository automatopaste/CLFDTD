#include <cstdint>
#include <iostream>

#include "glm/glm.hpp"

#include "fmt/core.h"

#include "sim/sim.h"

int main() {
	fmt::print("initialising CLFDTD simulation\n");

	sim::State state{};
	sim::init_sim(state);

	sim::run_sim(state, 20);

	return 0;
}