#define MAX_RADIUS 16

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;
layout(binding = 0, rgba8) coherent uniform image2D theImage;
layout(std140, binding = 1) uniform theParameters {
	int radius;
	float weights[MAX_RADIUS];
} parameters;

void main() {
	ivec2 uv = ivec2(gl_GlobalInvocationID.xy);
	ivec2 size = imageSize(theImage);

	float weight = parameters.weights[0];
	vec4 sum = vec4(imageLoad(theImage, uv)) * weight;

	int radius = min(parameters.radius, MAX_RADIUS);
	for (int i = 1; i < radius; i++) {
		float w = parameters.weights[i];

		#if GAUSSIAN_VERTICAL
			if (uv.y + i < size.y) {
				sum += imageLoad(theImage, uv + ivec2(0, i)) * w;
				weight += w;
			}

			if (uv.y - i >= 0) {
				sum += imageLoad(theImage, uv - ivec2(0, i)) * w;
				weight += w;
			}
		#else
			if (uv.x + i < size.x) {
				sum += imageLoad(theImage, uv + ivec2(i, 0)) * w;
				weight += w;
			}

			if (uv.x - i >= 0) {
				sum += imageLoad(theImage, uv - ivec2(i, 0)) * w;
				weight += w;
			}
		#endif
	}

	sum /= weight;

	memoryBarrierImage();
	imageStore(theImage, uv, clamp(sum, vec4(0, 0, 0, 0), vec4(255, 255, 255, 255)));
}
