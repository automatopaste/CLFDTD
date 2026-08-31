#include "r2_commander.h"

#include "passes/2d_basic_commander.h"
#include "passes/3d_linear_commander.h"
#include "passes/3d_sat_commander.h"

#include "vk/vk_initialisers.h"

enum class SimulationType {
	BASIC_2D,
	DISPERSION_2D,
	LINEAR_3D,
	SAT_3D
};
constexpr SimulationType SIMULATION_TYPE = SimulationType::SAT_3D;

void R2::init(R2::RenderData& renderData, VulkanEngine* engine, VkExtent2D swapchainExtent, VkExtent2D drawExtent)
{
	switch (SIMULATION_TYPE) {
	case SimulationType::BASIC_2D:
		D2::init(renderData, engine, swapchainExtent, drawExtent);
		break;
	case SimulationType::LINEAR_3D:
		L3D::init(renderData, engine, swapchainExtent, drawExtent);
		break;
	case SimulationType::SAT_3D:
		L3Dsat::init(renderData, engine, swapchainExtent, drawExtent);
		break;
	default:
		break;
	}
}

void R2::cleanup(R2::RenderData& renderData, VulkanEngine* engine)
{
	switch (SIMULATION_TYPE) {
	case SimulationType::BASIC_2D:
		D2::cleanup(renderData, engine);
		break;
	case SimulationType::LINEAR_3D:
		L3D::cleanup(renderData, engine);
		break;
	case SimulationType::SAT_3D:
		L3Dsat::cleanup(renderData, engine);
		break;
	default:
		break;
	}
}

void R2::render(VkCommandBuffer cmd, R2::RenderData& renderData, VulkanEngine* engine, uint32_t frameIndex, VkImage swapchainImage, VkImageView swapchainImageView)
{
	switch (SIMULATION_TYPE) {
	case SimulationType::BASIC_2D:
		D2::render(cmd, renderData, engine, frameIndex, swapchainImage, swapchainImageView);
		break;
	case SimulationType::LINEAR_3D:
		L3D::render(cmd, renderData, engine, frameIndex, swapchainImage, swapchainImageView);
		break;
	case SimulationType::SAT_3D:
		L3Dsat::render(cmd, renderData, engine, frameIndex, swapchainImage, swapchainImageView);
		break;
	default:
		break;
	}
}
