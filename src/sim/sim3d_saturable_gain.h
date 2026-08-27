#pragma once

#include <vector>
#include <cstdint>

#include "glm/glm.hpp"

typedef glm::vec3 vec3;
typedef glm::ivec3 ivec3;

namespace sim3dsat
{

	template <typename T>
	class field {
	public:

		void resize(ivec3 dimensions) {
			clear();
			size = (size_t)dimensions.x * (size_t)dimensions.y * (size_t)dimensions.z * sizeof(T);
			data = (T*)_aligned_malloc(size, alignof(T));
			this->dimensions = dimensions;
		}

		void clear() {
			if (data) {
				_aligned_free(data);
			}
			size = 0;
			dimensions = ivec3{ 0, 0, 0 };
		}

		inline T& at(size_t i, size_t j, size_t k) {
			return data[k * dimensions.x * dimensions.y + j * dimensions.x + i];
		}

	private:
		T* data{ nullptr };
		size_t size{ 0 };
		ivec3 dimensions{ 0, 0, 0 };
	};

	struct State {
		field<vec3> E;
		field<float> EPeak;
		field<vec3> F;
		field<vec3> J;
		field<vec3> H;
		field<float> s;

		field<float> psi_dHz_dx;
		field<float> psi_dHy_dx;
		field<float> psi_dHz_dy;
		field<float> psi_dHx_dy;
		field<float> psi_dHy_dz;
		field<float> psi_dHx_dz;
		field<float> psi_dEz_dx;
		field<float> psi_dEy_dx;
		field<float> psi_dEz_dy;
		field<float> psi_dEx_dy;
		field<float> psi_dEy_dz;
		field<float> psi_dEx_dz;
		std::vector<float> a;
		std::vector<float> b;

		float lastSample;
		std::vector<vec3> samples;
		std::vector<float> maxE;
		std::vector<float> sMin;

		float dx;
		float c;
		float dt;
		float t;
		uint32_t step;
		ivec3 dim;
		int resolution;
		int npml;
		float sigma0;
	};

	void state_init(State& state);
	void state_update(State& state);

}