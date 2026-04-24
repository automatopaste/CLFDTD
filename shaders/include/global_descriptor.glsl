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
