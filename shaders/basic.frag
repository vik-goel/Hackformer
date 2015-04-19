#version 110

#define NUM_POINT_LIGHTS 8
#define NUM_SPOT_LIGHTS 8

struct PointLight {
	vec3 p;
	vec3 color;
	vec3 atten; // (constant, linear, exponent)
};

struct SpotLight {
	PointLight base;
	vec2 dir;
	float cutoff;
};

uniform PointLight pointLights[NUM_POINT_LIGHTS];
uniform SpotLight spotLights[NUM_SPOT_LIGHTS];
uniform vec3 ambient;

uniform sampler2D diffuseTexture;
uniform sampler2D normalTexture;

uniform vec4 tint;

uniform float normalXFlip;

varying vec2 texCoord;
varying vec2 worldP;

void main() {
	vec4 texColor = texture2D(diffuseTexture, texCoord);
	// vec3 normal = texture2D(normalTexture, texCoord).xyz * 2.0 - vec3(1.0);

	// normal.x *= normalXFlip;
	// normal.y *= -1.0;

	//vec3 result = ambient * texColor.xyz;
	vec3 result = texColor.xyz;
/*
	for(int lightIndex = 0; lightIndex < NUM_POINT_LIGHTS; lightIndex++) {
		vec3 lightDir = vec3(worldP, 0.0) - pointLights[lightIndex].p;

		float lightDistanceSquared = (lightDir.x * lightDir.x) + (lightDir.y * lightDir.y) + (lightDir.z * lightDir.z);
		float lightDistance = sqrt(lightDistanceSquared);

		float inner = max(0.0, dot(normal, lightDir * (1.0 / lightDistance)));
		float intensity = pointLights[lightIndex].atten.x + 
						  pointLights[lightIndex].atten.y * lightDistance + 
						  pointLights[lightIndex].atten.z * lightDistanceSquared;


		result += pointLights[lightIndex].color * (inner / intensity);
	}

	for(int lightIndex = 0; lightIndex < NUM_SPOT_LIGHTS; lightIndex++) {
		vec3 lightDir = vec3(worldP, 0.0) - spotLights[lightIndex].base.p;
		//vec2 lightDir = worldP - spotLights[lightIndex].base.p.xy;

		float lightDistanceSquared = (lightDir.x * lightDir.x) + (lightDir.y * lightDir.y) + (lightDir.z * lightDir.z);
		float lightDistance = sqrt(lightDistanceSquared);

		lightDir *= (1.0 / lightDistance);

		// vec2 v1 = normalize(spotLights[lightIndex].dir);
		// vec2 v2 = normalize(lightDir.xy);

		// float conePercent = spotLights[lightIndex].dir.x * lightDir.x + 
		// 					spotLights[lightIndex].dir.y * lightDir.y;

		float brightness = 0.02;

		float conePercent = (dot(spotLights[lightIndex].dir, lightDir.xy) - (1.0 - brightness))* (1.0 / brightness);

		if(conePercent >= 0.0) {
			float inner = 1.0;//max(0.0, dot(normal, lightDir));
			float intensity = spotLights[lightIndex].base.atten.x + 
							  spotLights[lightIndex].base.atten.y * lightDistance + 
							  spotLights[lightIndex].base.atten.z * lightDistanceSquared;

			
			// float brightness = conePercent / spotLights[lightIndex].cutoff;

			result +=  spotLights[lightIndex].base.color * ((inner * conePercent) / intensity);
		}
	}


	result = clamp(result, vec3(0, 0, 0), vec3(1, 1, 1));
*/
	//result *= texColor.xyz;

	vec3 gamma = vec3(1.0 / 2.2);
	result = pow(result, gamma) * tint.xyz; 

	gl_FragColor = vec4(result, texColor.a * tint.a);
    //gl_FragColor = vec4(normal * 0.5 + vec3(0.5), texColor.a);
}