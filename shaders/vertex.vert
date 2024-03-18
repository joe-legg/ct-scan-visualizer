#version 400
layout(location = 0) in vec3 vertex_positions;

uniform mat4 mvp;

float rand(vec2 co){
    return fract(sin(dot(co, vec2(12.9898, 78.233))) * 43758.5453);
}

void main() {
    
    gl_Position = mvp * vec4(vertex_positions + rand(vertex_positions.xy), 1.0);
}
