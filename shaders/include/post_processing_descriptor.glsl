layout (set = 1, binding = 0) uniform sampler2D drawImage;
layout (rgba8, set = 1, binding = 1) uniform restrict writeonly image2D swapchainImage;
layout (r32f, set = 1, binding = 2) uniform restrict readonly image2D ezImage;
layout (r32f, set = 1, binding = 3) uniform restrict readonly image2D gaImage;