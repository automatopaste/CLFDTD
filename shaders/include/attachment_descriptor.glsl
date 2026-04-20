layout (set = 1, binding = 0) uniform sampler2D depthImage;
layout (set = 1, binding = 1) uniform sampler2D normalImage;
layout (set = 1, binding = 2) uniform sampler2D specularImage;
layout (rgba8, set = 1, binding = 3) uniform restrict writeonly image2D debugImage;
layout (rgba8, set = 1, binding = 4) uniform restrict writeonly image2D giProbeGenDebugImage;
layout (rgba8, set = 1, binding = 5) uniform restrict image2D giProbeRTImage;