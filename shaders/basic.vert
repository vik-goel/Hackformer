#version 110 

uniform vec2 twoOverScreenSize;

attribute vec4 gl_Vertex;
attribute vec4 gl_MultiTexCoord0;

varying vec2 texCoord;
varying vec2 worldP;

void main() {
    worldP = gl_Vertex.xy;
    texCoord = gl_MultiTexCoord0.xy;

	vec2 pos = worldP * twoOverScreenSize - vec2(1.0, 1.0);
	gl_Position = vec4(pos, 0.0, 1.0);

}