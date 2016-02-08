#version 430

/*
* Voxelization fragment shader.
*/

//!< in-variables
in RenderVertex
{
    vec3 posDevice;
    vec3 normal;
    vec2 uv;
} In;

flat in vec4 AABB;

//!< uniforms
layout(binding = 0) uniform atomic_uint index;
uniform sampler2D tex;
layout(r32ui, location = 1) restrict writeonly uniform uimageBuffer positionOutputImage;
layout(rgba8, location = 2) restrict writeonly uniform imageBuffer normalOutputImage;
layout(rgba8, location = 3) restrict writeonly uniform imageBuffer colorOutputImage;

//!< out-variables
layout(location = 0) out vec4 fragColor;

// Convert vec3 to uint with 10 bit per component (wants values from 0..1023)
uint vec3ToUintXYZ10(uvec3 val)
{
    return (uint(val.z) & 0x000003FF)   << 20U
            |(uint(val.y) & 0x000003FF) << 10U
            |(uint(val.x) & 0x000003FF);
}

void main()
{
    // Clipping with bounding box
    if( gl_FragCoord.x < AABB.x
        || gl_FragCoord.x >= AABB.z
        || gl_FragCoord.y < AABB.y
        || gl_FragCoord.y >= AABB.w)
    {
        // discard; // Does not work as expected :(
    }

    // Index in output textures
    uint idx = atomicCounterIncrement(index);

    // Position from 0 to 1023 in volume
    In.posDevice.z = -In.posDevice.z; // Left hand to right hand system
    uvec3 pos = uvec3(((In.posDevice + 1) / 2.0) * 1023);

    // Save position of voxel fragment
    imageStore(positionOutputImage, int(idx), uvec4(vec3ToUintXYZ10(pos)));

    // Save normal of voxel fragment
    imageStore(normalOutputImage, int(idx), vec4(In.normal, 0));

    // Save color of voxel fragment
    imageStore(colorOutputImage, int(idx), texture(tex, In.uv).rgba);
}
