#version 450

layout(binding = 0) uniform UniformBufferObject {
    mat4 projectionMatrix;
    mat4 viewMatrix;
    mat4 modelMatrix;
    mat4 normalMatrix;

    vec3 viewingPosition;
    vec4 ambientLightColor;
    vec3 pointLightPosition;
    vec4 pointLightColor;
} uniformBufferObject;

layout(binding = 1) uniform sampler2D textureSampler;

layout(location = 0) in vec3 fragmentPositionWorldSpace;
layout(location = 1) in vec3 fragmentNormalWorldSpace;
layout(location = 2) in vec2 fragmentUVCoordinates;

layout(location = 0) out vec4 outputColor;

void main()
{
    vec3 ambientLighting = (uniformBufferObject.ambientLightColor.xyz * uniformBufferObject.ambientLightColor.w);

    vec3 pointLightRayDirection = (uniformBufferObject.pointLightPosition - fragmentPositionWorldSpace);
    float attenuation = (1.0 / dot(pointLightRayDirection, pointLightRayDirection));
    pointLightRayDirection = normalize(pointLightRayDirection);
    
    vec3 unpackedPointLightColor = (uniformBufferObject.pointLightColor.xyz * uniformBufferObject.pointLightColor.w * attenuation);
    float diffuseLightValue = max(dot(normalize(fragmentNormalWorldSpace), pointLightRayDirection), 0.0);  // we want to avoid negative values.
    vec3 diffuseLighting = (unpackedPointLightColor * diffuseLightValue);

    float shininessValue = 16;
    float specularExponent = 1;
    
    vec3 viewingDirection = normalize(uniformBufferObject.viewingPosition - fragmentPositionWorldSpace);
    vec3 reflectionDirection = reflect(pointLightRayDirection, fragmentNormalWorldSpace);
    vec3 halfwayDirection = normalize(pointLightRayDirection + viewingDirection);
    float specularComponent = pow(max(dot(fragmentNormalWorldSpace, halfwayDirection), 0.0), shininessValue);
    
    vec3 specularLighting = specularExponent * specularComponent * unpackedPointLightColor;
    
    vec4 completeLighting = vec4((ambientLighting + diffuseLighting + specularLighting), 1.0);
    

    outputColor = (completeLighting * texture(textureSampler, fragmentUVCoordinates));
}