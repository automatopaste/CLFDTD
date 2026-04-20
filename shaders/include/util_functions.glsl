vec3 unpack_normal(vec2 packedNormal) {
    vec3 result;
    result.z = (packedNormal.x * packedNormal.x + packedNormal.y * packedNormal.y) * 2.0 - 1.0;
    result.xy = normalize (packedNormal.xy) * sqrt (1.0 - result.z * result.z);
    return result;
}

vec3 get_world_pos(vec2 uv, float depth) {
	vec4 clipPos = vec4(uv.x * 2.0 - 1.0, uv.y * 2.0 - 1.0, depth, 1.0);
	vec4 worldPos = sceneData.invviewproj * clipPos;
	worldPos.xyz /= worldPos.w;
	return worldPos.xyz;
}

vec3 get_uv_depth(vec3 worldPos) {
	vec4 ndc = sceneData.viewproj * vec4(worldPos, 1.0);
	ndc.xyz /= ndc.w;
	ndc.xy = ndc.xy * 0.5 + 0.5;
	return ndc.xyz;
}
