#version 110

uniform sampler2D stencilTexture;
uniform vec4 tint;

varying vec2 texCoord;

void main() {
	vec4 stencilColor = texture2D(stencilTexture, texCoord);
	gl_FragColor = stencilColor * tint;
}