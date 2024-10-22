#version 450

layout(binding = 1) uniform sampler2D texSampler;

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec3 fragNormal;

layout(location = 0) out vec4 outColor;

void main() {
    vec3 lightDir = normalize(vec3(0.5, 0.5, 0.0f));
    float nDotL = dot(fragNormal, lightDir)*0.5 + 0.5;
    //outColor = texture(texSampler, fragTexCoord);
    outColor = vec4(nDotL, nDotL, nDotL, 1.0);
}