#version 450

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inNormals;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) out flat vec3 fragNormal;

void main() {
    mat4 identityMatrix = mat4(
        1.0, 0.0, 0.0, 0.0,  // First column
        0.0, 1.0, 0.0, 0.0,  // Second column
        0.0, 0.0, 1.0, 0.0,  // Third column
        0.0, 0.0, 0.0, 1.0   // Fourth column
    );
    //gl_Position = identityMatrix * identityMatrix * identityMatrix * vec4(inPosition, 0.0, 1.0);
    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPosition, 1.0);
    fragColor = inColor;
    fragTexCoord = inTexCoord;
    fragNormal = inNormals;
}