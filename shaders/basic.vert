#version 110 

uniform vec2 twoOverScreenSize;

attribute vec4 gl_Vertex;
attribute vec4 gl_MultiTexCoord0;

varying vec2 texCoord;

void main() {
	vec2 pos = gl_Vertex.xy * twoOverScreenSize - vec2(1.0, 1.0);
	gl_Position = vec4(pos, 0.0, 1.0);

    texCoord = gl_MultiTexCoord0.xy;
}