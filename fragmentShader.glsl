#version 400 core

layout(location = 0) out vec4 color;

in vec2 vTexCoord;
in vec4 vColor;
in float vTexIndex;

// uniform vec4 uColor;
uniform sampler2D uTextures[2];

void main() {
    int texIndex = int(vTexIndex);
    vec4 texColor = texture(uTextures[texIndex], vTexCoord);
    color = texColor + vColor;
}
