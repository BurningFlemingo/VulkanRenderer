#version 460

layout (location = 0) in vec3 color;

layout (location = 0) out vec4 pxColor;

void main() {
	pxColor = vec4(color.xyz, 1.0);
}
