#version 110 

uniform vec2 twoOverScreenSize;

varying vec2 texCoord;

void main() {
    texCoord = gl_MultiTexCoord0.xy;

	vec2 pos = gl_Vertex.xy * twoOverScreenSize - vec2(1.0, 1.0);
	gl_Position = vec4(pos, 0.0, 1.0);
}
//end