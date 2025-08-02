#version 400 core

layout(location = 0) in vec2 position;
layout(location = 1) in vec2 texCoord;
layout(location = 2) in vec4 aColor;
layout(location = 3) in float aTexIndex;

out vec2 vTexCoord;
out vec4 vColor;
out float vTexIndex;

uniform mat4 uMvp;

void main() {
  gl_Position = uMvp * vec4(position, 0.0, 1.0);
  vTexCoord = texCoord;
  vColor = aColor;
  vTexIndex = aTexIndex;
}
