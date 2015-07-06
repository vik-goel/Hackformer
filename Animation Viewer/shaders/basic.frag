#version 110

uniform sampler2D diffuseTexture;
uniform vec4 tint;

varying vec2 texCoord;

void main() {
	vec4 texColor = texture2D(diffuseTexture, texCoord);
	
	vec3 gamma = vec3(1.0 / 2.2);
	vec3 result = pow(texColor.xyz, gamma); 

	gl_FragColor = vec4(result.xyz * tint.xyz, texColor.a * tint.a);
}