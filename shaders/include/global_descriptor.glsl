layout (set = 0, binding = 0) uniform SceneData {
	mat4 view;
	mat4 proj;
	mat4 viewproj;
	mat4 invviewproj;
	mat4 invproj;
	mat4 invview;
	vec4 ambientColor;
	vec4 sunlightDirection; //w for sun power
	vec4 sunlightColor;
	vec4 viewPos;
	uvec4 viewport;
} sceneData;

layout (set = 0, binding = 1) uniform sampler2D[] materialTextures;

// light culling definitions
#define TILE_SIZE 8
#define LIGHTS_PER_TILE 16
#define SHADOWCASTING_LIGHTS_PER_TILE 16
#define SHADOWMAP_MAGIC_NUMBER 0xFFFFFFFA