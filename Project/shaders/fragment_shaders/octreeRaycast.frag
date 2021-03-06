#version 430

// In / out
layout(location = 0) out vec4 fragColor;

// Uniforms
layout(binding = 0, r32ui) uniform readonly uimageBuffer octree;
layout(binding = 1, rgba32f) uniform readonly image2D worldPos;
uniform sampler3D brickPool;
uniform vec3 camPos;
uniform float stepSize;
uniform float volumeExtent;
uniform float directionBeginScale;
uniform float volumeRes;
uniform int maxSteps;
uniform int maxLevel;

// Defines
const uint pow2[] = {1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024};
const uvec3 insertPositions[] = {
    uvec3(0, 0, 0),
    uvec3(1, 0, 0),
    uvec3(0, 1, 0),
    uvec3(1, 1, 0),
    uvec3(0, 0, 1),
    uvec3(1, 0, 1),
    uvec3(0, 1, 1),
    uvec3(1, 1, 1)};

// Helper
uint getBit(uint value, uint position)
{
    return (value >> (position-1)) & 1u;
}

uvec3 decodeBrickCoords(uint coded)
{
    uvec3 coords;
    coords.z =  coded & 0x000003FF;
    coords.y = (coded & 0x000FFC00) >> 10U;
    coords.x = (coded & 0x3FF00000) >> 20U;
    return coords;
}

vec3 getVolumePos(vec3 worldPos)
{
    return (worldPos / volumeExtent) + 0.5;
}

// Main
void main()
{
    // Raycasting preparation
    vec3 fragWorldPosition = imageLoad(worldPos, ivec2(gl_FragCoord.xy)).xyz;

    vec3 direction = normalize(fragWorldPosition - camPos);
    vec3 rayPosition = fragWorldPosition - directionBeginScale * direction;
    vec4 outputColor = vec4(0,0,0,0);

    // Octree reading preparation
    uint nodeOffset = 0;
    uint childPointer = 0;
    uint nodeTile;

    // Get first child pointer
    nodeTile = imageLoad(octree, int(0)).x;
    uint firstChildPointer = nodeTile & uint(0x3fffffff);

    // Determine, when opacity is good enough
    bool finished = false;

    // Go over ray
    for(int i = 0; i < maxSteps; i++)
    {

        // Propagate ray along ray direction
        rayPosition += stepSize * direction;

		// Dont Raycast when outside of the volume
		if(abs(rayPosition).x > volumeExtent/2.0f ||
		abs(rayPosition).y > volumeExtent/2.0f||
		abs(rayPosition).z > volumeExtent/2.0f
		)
		break;

	vec3 innerOctreePosition = getVolumePos(rayPosition);

        // Reset child pointer
        childPointer = firstChildPointer;

        // Go through octree
        for(int j = 1; j < maxLevel; j++)
        {
            // Determine, in which octant the searched position is
            uvec3 nextOctant = uvec3(0, 0, 0);
            nextOctant.x = uint(2 * innerOctreePosition.x);
            nextOctant.y = uint(2 * innerOctreePosition.y);
            nextOctant.z = uint(2 * innerOctreePosition.z);

            // Make the octant position 1D for the linear memory
            nodeOffset = 2 * (nextOctant.x + 2 * nextOctant.y + 4 * nextOctant.z);
            nodeTile = imageLoad(octree, int(childPointer * 16U + nodeOffset)).x;

            // Update position in volume
            innerOctreePosition.x = 2 * innerOctreePosition.x - nextOctant.x;
            innerOctreePosition.y = 2 * innerOctreePosition.y - nextOctant.y;
            innerOctreePosition.z = 2 * innerOctreePosition.z - nextOctant.z;

            // The 32nd bit indicates whether the node has children:
            // 1 means has children
            // 0 means does not have children
            // Only read from brick, if we are at aimed level in octree
            if(/*getBit(nodeTile, 32) == 0 &&*/ j == maxLevel-1)
            {
                // Output the reached level as color
                //float level = float(j) / maxLevel;
                //outputColor.x = level;
                //outputColor.y = level;
                //outputColor.z = level;
                //finished = true;

                // Brick coordinates
                uint brickTile = imageLoad(octree, int(nodeOffset + childPointer *16U)+1).x;
                uvec3 brickCoords = decodeBrickCoords(brickTile);

                // Just a check, whether brick is there
                if(getBit(brickTile, 31) == 1)
                {
                    // Here we should intersect our brick seperately
                    // Go one octant deeper in this inner loop cicle to determine exact brick coordinate
                    nextOctant.x = uint(2 * innerOctreePosition.x);
                    nextOctant.y = uint(2 * innerOctreePosition.y);
                    nextOctant.z = uint(2 * innerOctreePosition.z);
                    uint offset = nextOctant.x + 2 * nextOctant.y + 4 * nextOctant.z;
                    brickCoords += insertPositions[offset]*2;

                    // Accumulate color
                    outputColor = texture(brickPool, brickCoords/(volumeRes) + (1.0/volumeRes)/2.0);
                    //outputColor.rgb += (1.0 - outputColor.a) * src.rgb * src.a;
                    //outputColor.a += (1.0 - outputColor.a) * src.a;

                    // More or less: if you hit something, exit
                    
                        //outputColor = src;
					if(outputColor.r>0.01 || outputColor.g>0.01 || outputColor.b>0.01 )
                    {
						finished = true;
					}
                }

                // Break inner loop
                break;
            }
            else
            {
                // If the node has children we read the pointer to the next nodetile
                childPointer = nodeTile & uint(0x3fffffff);
            }
        }

        // Break outer loop
        if(finished)
        {
            break;
        }
    }

    fragColor = outputColor;
}
