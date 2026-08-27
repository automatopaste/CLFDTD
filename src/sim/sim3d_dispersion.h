#pragma once

#include <vector>
#include <cstdint>

#include "glm/glm.hpp"

typedef glm::vec3 vec3;
typedef glm::ivec3 ivec3;

namespace sim3ddisp
{

	class field_vec {
	public:

		void resize(ivec3 dimensions) {
			clear();
			size = (size_t)dimensions.x * (size_t)dimensions.y * (size_t)dimensions.z * sizeof(vec3);
			data = (vec3*)_aligned_malloc(size, alignof(vec3));
			this->dimensions = dimensions;
		}

		void clear() {
			if (data) {
				_aligned_free(data);
			}
			size = 0;
			dimensions = ivec3{ 0, 0, 0 };
		}

		inline vec3& at(size_t i, size_t j, size_t k) {
			return data[k * dimensions.x * dimensions.y + j * dimensions.x + i];
		}

	private:
		vec3* data{ nullptr };
		size_t size{ 0 };
		ivec3 dimensions{ 0, 0, 0 };
	};

	struct State {
		field_vec ga;
		field_vec e;
		field_vec e0;
		field_vec p;
		field_vec p0;
		field_vec h;

		float lastSample;
		std::vector<vec3> samples;

		float dx;
		float c;
		float dt;
		float t;
		uint32_t step;
		ivec3 dim;
		int resolution;
		int npml;
	};

	void state_init(State& state);
	void state_update(State& state);

}