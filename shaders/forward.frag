#version 110

#define NUM_POINT_LIGHTS 8
#define NUM_SPOT_LIGHTS 8

struct PointLight {
	vec3 p;
	vec3 color;
	float range;
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
	vec3 normal = vec3(0.0);
	float innerOffs = 1.0;

	if(normalXFlip != 0.0) {
		normal = texture2D(normalTexture, texCoord).xyz * 2.0 - vec3(1.0);
		normal.x *= normalXFlip;
		innerOffs = 0.0;
	}

	vec3 result = ambient;

	for(int lightIndex = 0; lightIndex < NUM_POINT_LIGHTS; lightIndex++) {
		vec3 lightDir = vec3(worldP, 0.0) - pointLights[lightIndex].p;

		float lightDistanceSquared = (lightDir.x * lightDir.x) + (lightDir.y * lightDir.y) + (lightDir.z * lightDir.z);
		float lightDistance = sqrt(lightDistanceSquared);
		lightDir *= (1.0 / lightDistance);

		float inner = max(0.0, dot(normal, lightDir)) + innerOffs;
		float intensity = max(0.0, (pointLights[lightIndex].range - lightDistance) / pointLights[lightIndex].range);

		result += pointLights[lightIndex].color * (intensity * inner);
	}

	for(int lightIndex = 0; lightIndex < NUM_SPOT_LIGHTS; lightIndex++) {
		vec3 spotLightP = spotLights[lightIndex].base.p;
		vec3 lightDir = vec3(worldP, 0.0) - spotLightP;

		float lightDistanceSquared = (lightDir.x * lightDir.x) + (lightDir.y * lightDir.y) + (lightDir.z * lightDir.z);
		float lightDistance = sqrt(lightDistanceSquared);
		lightDir *= (1.0 / lightDistance);

		float inner = max(0.0, dot(normal, lightDir)) + innerOffs;
		float intensity = max(0.0, (spotLights[lightIndex].base.range - lightDistance) / spotLights[lightIndex].base.range);

		float conePercent = spotLights[lightIndex].dir.x * lightDir.x + spotLights[lightIndex].dir.y * lightDir.y;
		float brightness = max(0.0, (conePercent - spotLights[lightIndex].cutoff) / (1.0 - spotLights[lightIndex].cutoff));
		
		result += spotLights[lightIndex].base.color * (brightness * intensity * inner);
	}

	result = clamp(result, vec3(0.0), vec3(1.0));

	vec3 gamma = vec3(1.0 / 2.2);
	vec3 gammaCorrectedDiffuse = pow(texColor.xyz, gamma) * tint.xyz;
	result *= gammaCorrectedDiffuse; 

	gl_FragColor = vec4(result, texColor.a * tint.a);
    //gl_FragColor = vec4(normal * 0.5 + vec3(0.5), texColor.a);
}