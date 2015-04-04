#version 110

uniform sampler2D diffuse;
uniform vec4 tint;

varying vec2 texCoord;

void main() {
    gl_FragColor = texture2D(diffuse, texCoord) * tint;
}