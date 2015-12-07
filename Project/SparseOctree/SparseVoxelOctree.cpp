//
// Created by nils1990 on 06.12.15.
//
#include "SparseVoxelOctree.h"

void SparseVoxelOctree::init()
{
    // testsing for node and brick-pool
    m_nodePool.init();
    m_brickPool.init();

    m_brickPool.registerTextureForCUDAWriting();
}
SparseVoxelOctree::~SparseVoxelOctree()
{
    m_brickPool.unregisterTextureForCUDA();
}

void SparseVoxelOctree::fillGui()
{

}

void SparseVoxelOctree::updateOctree()
{
    cudaArray_t voxelliste;
    m_nodePool.fillNodePool(voxelliste); // normalerweise voxelisierer.getVoxelList() oder so

    if(m_nodePool.getPoolSize() < 8192)
        m_nodePool.updateConstMemory();

    m_brickPool.fillBrickPool(m_nodePool);

    //
}

