#version 400
layout(location = 0) in vec3 vertex_positions;

uniform mat4 mvp;

void main() {
    gl_Position = mvp * vec4(vertex_positions, 1.0);
}
