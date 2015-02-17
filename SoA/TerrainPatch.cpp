#include "stdafx.h"
#include "TerrainPatch.h"
#include "TerrainPatchMesher.h"
#include "TerrainPatchMesh.h"

#include "Chunk.h"
#include "RPC.h"
#include "RenderUtils.h"
#include "SpaceSystemComponents.h"
#include "SphericalTerrainComponentUpdater.h"
#include "SphericalTerrainGpuGenerator.h"
#include "VoxelCoordinateSpaces.h"
#include "VoxelSpaceConversions.h"

// TODO(Ben): These constants are fairly arbitrary
const float DIST_MIN = 3.0f;
const float DIST_MAX = 3.1f;
const float MIN_SIZE = 0.4096f;

TerrainPatch::~TerrainPatch() {
    destroy();
}

void TerrainPatch::init(const f64v2& gridPosition,
                                 WorldCubeFace cubeFace,
                                 int lod,
                                 const TerrainPatchData* sphericalTerrainData,
                                 f64 width,
                                 TerrainRpcDispatcher* dispatcher) {
    m_gridPos = gridPosition;
    m_cubeFace = cubeFace;
    m_lod = lod;
    m_sphericalTerrainData = sphericalTerrainData;
    m_width = width;
    m_dispatcher = dispatcher;

    // Construct an approximate AABB
    const i32v3& coordMapping = VoxelSpaceConversions::VOXEL_TO_WORLD[(int)m_cubeFace];
    const i32v2& coordMults = VoxelSpaceConversions::FACE_TO_WORLD_MULTS[(int)m_cubeFace];
    f64v3 corners[4];
    corners[0][coordMapping.x] = gridPosition.x * coordMults.x;
    corners[0][coordMapping.y] = sphericalTerrainData->getRadius() * VoxelSpaceConversions::FACE_Y_MULTS[(int)m_cubeFace];
    corners[0][coordMapping.z] = gridPosition.y * coordMults.y;
    corners[0] = glm::normalize(corners[0]) * m_sphericalTerrainData->getRadius();
    corners[1][coordMapping.x] = gridPosition.x * coordMults.x;
    corners[1][coordMapping.y] = sphericalTerrainData->getRadius() * VoxelSpaceConversions::FACE_Y_MULTS[(int)m_cubeFace];
    corners[1][coordMapping.z] = (gridPosition.y + m_width) * coordMults.y;
    corners[1] = glm::normalize(corners[1]) * m_sphericalTerrainData->getRadius();
    corners[2][coordMapping.x] = (gridPosition.x + m_width) * coordMults.x;
    corners[2][coordMapping.y] = sphericalTerrainData->getRadius() * VoxelSpaceConversions::FACE_Y_MULTS[(int)m_cubeFace];
    corners[2][coordMapping.z] = (gridPosition.y + m_width) * coordMults.y;
    corners[2] = glm::normalize(corners[2]) * m_sphericalTerrainData->getRadius();
    corners[3][coordMapping.x] = (gridPosition.x + m_width) * coordMults.x;
    corners[3][coordMapping.y] = sphericalTerrainData->getRadius() * VoxelSpaceConversions::FACE_Y_MULTS[(int)m_cubeFace];
    corners[3][coordMapping.z] = gridPosition.y * coordMults.y;
    corners[3] = glm::normalize(corners[3]) * m_sphericalTerrainData->getRadius();

    f64 minX = INT_MAX, maxX = INT_MIN;
    f64 minY = INT_MAX, maxY = INT_MIN;
    f64 minZ = INT_MAX, maxZ = INT_MIN;
    for (int i = 0; i < 4; i++) {
        auto& c = corners[i];
        if (c.x < minX) {
            minX = c.x;
        } else if (c.x > maxX) {
            maxX = c.x;
        }
        if (c.y < minY) {
            minY = c.y;
        } else if (c.y > maxY) {
            maxY = c.y;
        }
        if (c.z < minZ) {
            minZ = c.z;
        } else if (c.z > maxZ) {
            maxZ = c.z;
        }
    }
    // Get world position and bounding box
    m_aabbPos = f32v3(minX, minY, minZ);
    m_aabbDims = f32v3(maxX - minX, maxY - minY, maxZ - minZ);
}

void TerrainPatch::update(const f64v3& cameraPos) {
    
    // Calculate distance from camera
    f64v3 closestPoint = calculateClosestPointAndDist(cameraPos);
    
    if (m_children) {
        if (m_distance > m_width * DIST_MAX) {
            if (!m_mesh) {
                requestMesh();
            }
            if (hasMesh()) {
                // Out of range, kill children
                delete[] m_children;
                m_children = nullptr;
            }
        } else if (m_mesh) {
            // In range, but we need to remove our mesh.
            // Check to see if all children are renderable
            bool deleteMesh = true;
            for (int i = 0; i < 4; i++) {
                if (!m_children[i].isRenderable()) {
                    deleteMesh = false;
                    break;
                }
            }
            
            if (deleteMesh) {
                // Children are renderable, free mesh.
                // Render thread will deallocate.
                m_mesh->m_shouldDelete = true;
                m_mesh = nullptr;
            }
        }
    } else if (canSubdivide()) {
        // Check if we are over horizon. If we are, don't divide.
        if (!isOverHorizon(cameraPos, closestPoint, m_sphericalTerrainData->getRadius())) {
            m_children = new TerrainPatch[4];
            // Segment into 4 children
            for (int z = 0; z < 2; z++) {
                for (int x = 0; x < 2; x++) {
                    m_children[(z << 1) + x].init(m_gridPos + f64v2((m_width / 2.0) * x, (m_width / 2.0) * z),
                                                  m_cubeFace, m_lod + 1, m_sphericalTerrainData, m_width / 2.0,
                                                  m_dispatcher);
                }
            }
        }
    } else if (!m_mesh) {
        requestMesh();
    }
    
    // Recursively update children if they exist
    if (m_children) {
        for (int i = 0; i < 4; i++) {
            m_children[i].update(cameraPos);
        }
    }
}

void TerrainPatch::destroy() {
    if (m_mesh) {
        m_mesh->m_shouldDelete = true;
        m_mesh = nullptr;
    }
    delete[] m_children;
    m_children = nullptr;
}

bool TerrainPatch::hasMesh() const {
    return (m_mesh && m_mesh->m_isRenderable);
}

bool TerrainPatch::isRenderable() const {
    if (hasMesh()) return true;
    if (m_children) {
        for (int i = 0; i < 4; i++) {
            if (!m_children[i].isRenderable()) return false;
        }
        return true;
    }
    return false;
}

bool TerrainPatch::isOverHorizon(const f32v3 &relCamPos, const f32v3 &point, f32 planetRadius) {
    const f32 DELTA = 0.1f;
    f32 camHeight = glm::length(relCamPos);
    f32v3 normalizedCamPos = relCamPos / camHeight;

    // Limit the camera depth
    if (camHeight < planetRadius + 1.0f) camHeight = planetRadius + 1.0f;

    f32 horizonAngle = acos(planetRadius / camHeight);
    f32 lodAngle = acos(glm::dot(normalizedCamPos, glm::normalize(point)));
    if (lodAngle >= horizonAngle + DELTA) return true;
    return false;
}
bool TerrainPatch::isOverHorizon(const f64v3 &relCamPos, const f64v3 &point, f64 planetRadius) {
    const f64 DELTA = 0.1;
    f64 camHeight = glm::length(relCamPos);
    f64v3 normalizedCamPos = relCamPos / camHeight;

    // Limit the camera depth
    if (camHeight < planetRadius + 1.0) camHeight = planetRadius + 1.0;

    f64 horizonAngle = acos(planetRadius / camHeight);
    f64 lodAngle = acos(glm::dot(normalizedCamPos, glm::normalize(point)));
    if (lodAngle >= horizonAngle + DELTA) {
        return true;
    }
    return false;
}

bool TerrainPatch::canSubdivide() const {
    return (m_lod < PATCH_MAX_LOD && m_distance < m_width * DIST_MIN && m_width > MIN_SIZE);
}

void TerrainPatch::requestMesh() {
    // Try to generate a mesh
    const i32v2& coordMults = VoxelSpaceConversions::FACE_TO_WORLD_MULTS[(int)m_cubeFace];

    f32v3 startPos(m_gridPos.x * coordMults.x,
                   m_sphericalTerrainData->getRadius() * VoxelSpaceConversions::FACE_Y_MULTS[(int)m_cubeFace],
                   m_gridPos.y* coordMults.y);
    m_mesh = m_dispatcher->dispatchTerrainGen(startPos,
                                              m_width,
                                              m_lod,
                                              m_cubeFace, true);
}

f64v3 TerrainPatch::calculateClosestPointAndDist(const f64v3& cameraPos) {
    f64v3 closestPoint;
    if (hasMesh()) {
        // If we have a mesh, we can use it's accurate bounding box    
        closestPoint = m_mesh->getClosestPoint(cameraPos);
    } else {
        closestPoint = getClosestPointOnAABB(cameraPos, m_aabbPos, m_aabbDims);
    }
    m_distance = glm::length(closestPoint - cameraPos);
    return closestPoint;
}
