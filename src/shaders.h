#pragma once

const char* vertexShaderSource = R"(
#version 120
attribute vec2 aPos;
attribute vec3 aColor;

uniform mat4 projection;
uniform mat4 view;

varying vec3 fragmentColor;

void main()
{
    gl_Position = projection * view * vec4(aPos, 0.0, 1.0);
    fragmentColor = aColor;
}
)";

const char* fragmentShaderSource = R"(
#version 120
varying vec3 fragmentColor;

void main()
{
    gl_FragColor = vec4(fragmentColor, 1.0);
}
)";

const char* gridVertexShaderSource = R"(
#version 120
attribute vec2 aPos;

uniform mat4 projection;
uniform mat4 view;
uniform float gridSize;
uniform vec2 viewCenter;
uniform float zoomLevel;

varying float opacity;

void main()
{
    gl_Position = projection * view * vec4(aPos, 0.0, 1.0);
    
    // Fade grid lines based on zoom level
    float fadeStart = 0.5;
    float fadeEnd = 0.1;
    opacity = smoothstep(fadeEnd, fadeStart, 1.0 / zoomLevel);
    opacity *= 0.3; // Base opacity
}
)";

const char* gridFragmentShaderSource = R"(
#version 120
varying float opacity;

void main()
{
    gl_FragColor = vec4(0.4, 0.4, 0.4, opacity);
}
)";
