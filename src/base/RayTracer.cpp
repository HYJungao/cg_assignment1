#define _CRT_SECURE_NO_WARNINGS

#include "base/Defs.hpp"
#include "base/Math.hpp"
#include "RayTracer.hpp"
#include <stdio.h>
#include "rtIntersect.inl"
#include <fstream>
#include <algorithm>
#include <array>
#include <vector>
#include <numeric>
#include "rtlib.hpp"


// Helper function for hashing scene data for caching BVHs
extern "C" void MD5Buffer( void* buffer, size_t bufLen, unsigned int* pDigest );


namespace FW
{


Vec2f getTexelCoords(Vec2f uv, const Vec2i size)
{

	// YOUR CODE HERE (R3):
	// Get texel indices of texel nearest to the uv vector. Used in texturing.
	// UV coordinates range from negative to positive infinity. First map them
	// to a range between 0 and 1 in order to support tiling textures, then
	// scale the coordinates by image resolution and find the nearest pixel.

    float u = uv.x - floor(uv.x);
    float v = uv.y - floor(uv.y);

    float x = u * size.x;
    float y = v * size.y;
    
    return Vec2f(x, y);
}

Mat3f formBasis(const Vec3f& n) {
    // YOUR CODE HERE (R4):

    Vec3f q(n);

    float absX = std::abs(q.x);
    float absY = std::abs(q.y);
    float absZ = std::abs(q.z);

    if (absX <= absY && absX <= absZ) {
        q.x = 1;
    }
    else if (absY <= absX && absY <= absZ) {
        q.y = 1;
    }
    else {
        q.z = 1;
    }

    Vec3f t = cross(q, n).normalized();
    q = cross(n, t);

    Mat3f r;
    r.col(0) = std::move(t);
    r.col(1) = std::move(q);
    r.col(2) = n;
    
    return r;
}


String RayTracer::computeMD5( const std::vector<Vec3f>& vertices )
{
    unsigned char digest[16];
    MD5Buffer( (void*)&vertices[0], sizeof(Vec3f)*vertices.size(), (unsigned int*)digest );

    // turn into string
    char ad[33];
    for ( int i = 0; i < 16; ++i )
        ::sprintf( ad+i*2, "%02x", digest[i] );
    ad[32] = 0;

    return FW::String( ad );
}


// --------------------------------------------------------------------------


RayTracer::RayTracer()
{
}

RayTracer::~RayTracer()
{
}


void RayTracer::loadHierarchy(const char* filename, std::vector<RTTriangle>& triangles)
{
	std::ifstream ifs(filename, std::ios::binary);
    m_bvh = Bvh(ifs);

    m_triangles = &triangles;
    m_indices = &(m_bvh.getIndices());
}

void RayTracer::saveHierarchy(const char* filename, const std::vector<RTTriangle>& triangles) {
	(void)triangles; // Not used.

	std::ofstream ofs(filename, std::ios::binary);
	m_bvh.save(ofs);
}

// R1 code
std::unique_ptr<BvhNode> RayTracer::constructBvh(size_t start, size_t end) {
    
    std::unique_ptr<BvhNode> node = std::make_unique<BvhNode>();

    if (end - start <= 1) {
        size_t index = m_indices->at(start);
        AABB box(m_triangles->at(index).min(), m_triangles->at(index).max());
        for (size_t i = start + 1; i < end; ++i) {
            index = m_indices->at(i);
            box.min = FW::min(box.min, m_triangles->at(index).min());
            box.max = FW::max(box.max, m_triangles->at(index).max());
        }

        node->bb = std::move(box);
        // node->bb = AABB(triangles[start].min(), triangles[start].max());
        node->startPrim = start;
        node->endPrim = end;
        node->left = nullptr;
        node->right = nullptr;
        return node;
    }
    else 
    {
        size_t index = m_indices->at(start);
        AABB box(m_triangles->at(index).centroid(), m_triangles->at(index).centroid());
        for (size_t i = start + 1; i < end; ++i) {
            index = m_indices->at(i);
            box.min = FW::min(box.min, m_triangles->at(index).centroid());
            box.max = FW::max(box.max, m_triangles->at(index).centroid());
        }

        Vec3f diagonal = box.max - box.min;

        if (diagonal.x > diagonal.y && diagonal.x > diagonal.z) {
            std::sort(m_indices->begin() + start, m_indices->begin() + end, [this](auto num1, auto num2) {
                Vec3f tmp1 = (m_triangles->at(num1).min() + m_triangles->at(num1).max()) * 0.5;
                Vec3f tmp2 = (m_triangles->at(num2).min() + m_triangles->at(num2).max()) * 0.5;
                return tmp1.x < tmp2.x;
            });
        }
        else if (diagonal.y > diagonal.z) {
            std::sort(m_indices->begin() + start, m_indices->begin() + end, [this](auto num1, auto num2) {
                Vec3f tmp1 = (m_triangles->at(num1).min() + m_triangles->at(num1).max()) * 0.5;
                Vec3f tmp2 = (m_triangles->at(num2).min() + m_triangles->at(num2).max()) * 0.5;
                return tmp1.y < tmp2.y;
            });
        } 
        else {
            std::sort(m_indices->begin() + start, m_indices->begin() + end, [this](auto num1, auto num2) {
                Vec3f tmp1 = (m_triangles->at(num1).min() + m_triangles->at(num1).max()) * 0.5;
                Vec3f tmp2 = (m_triangles->at(num2).min() + m_triangles->at(num2).max()) * 0.5;
                return tmp1.z < tmp2.z;
            });
        }

        size_t mid = start + ((end - start) / 2);

        node->left = constructBvh(start, mid);
        node->right = constructBvh(mid, end);
        node->startPrim = start;
        node->endPrim = end;
        node->bb = AABB(FW::min(node->left->bb.min, node->right->bb.min), FW::max(node->left->bb.max, node->right->bb.max));
        return node;
    }
}

std::unique_ptr<BvhNode> RayTracer::constructBvhSah(size_t start, size_t end) {

    std::unique_ptr<BvhNode> node = std::make_unique<BvhNode>();

    if (end - start <= 2) {
        size_t index = m_indices->at(start);
        AABB box(m_triangles->at(index).min(), m_triangles->at(index).max());
        for (size_t i = start + 1; i < end; ++i) {
            index = m_indices->at(i);
            box.min = FW::min(box.min, m_triangles->at(index).min());
            box.max = FW::max(box.max, m_triangles->at(index).max());
        }

        node->bb = std::move(box);
        // node->bb = AABB(triangles[start].min(), triangles[start].max());
        node->startPrim = start;
        node->endPrim = end;
        node->left = nullptr;
        node->right = nullptr;
        return node;
    }
    else
    {
        size_t index = m_indices->at(start);
        AABB box(m_triangles->at(index).centroid(), m_triangles->at(index).centroid());
        for (size_t i = start + 1; i < end; ++i) {
            index = m_indices->at(i);
            box.min = FW::min(box.min, m_triangles->at(index).centroid());
            box.max = FW::max(box.max, m_triangles->at(index).centroid());
        }

        Vec3f diagonal = box.max - box.min;

        if (diagonal.x > diagonal.y && diagonal.x > diagonal.z) {
            std::sort(m_indices->begin() + start, m_indices->begin() + end, [this](auto num1, auto num2) {
                Vec3f tmp1 = (m_triangles->at(num1).min() + m_triangles->at(num1).max()) * 0.5;
                Vec3f tmp2 = (m_triangles->at(num2).min() + m_triangles->at(num2).max()) * 0.5;
                return tmp1.x < tmp2.x;
                });
        }
        else if (diagonal.y > diagonal.z) {
            std::sort(m_indices->begin() + start, m_indices->begin() + end, [this](auto num1, auto num2) {
                Vec3f tmp1 = (m_triangles->at(num1).min() + m_triangles->at(num1).max()) * 0.5;
                Vec3f tmp2 = (m_triangles->at(num2).min() + m_triangles->at(num2).max()) * 0.5;
                return tmp1.y < tmp2.y;
                });
        }
        else {
            std::sort(m_indices->begin() + start, m_indices->begin() + end, [this](auto num1, auto num2) {
                Vec3f tmp1 = (m_triangles->at(num1).min() + m_triangles->at(num1).max()) * 0.5;
                Vec3f tmp2 = (m_triangles->at(num2).min() + m_triangles->at(num2).max()) * 0.5;
                return tmp1.z < tmp2.z;
                });
        }

        size_t optMid = start + ((end - start) / 2);
        float minCost = std::numeric_limits<float>::max();

        // ****** unoptimized version of SAH, O(n^2) ******
        //for (size_t i = start; i < end; ++i) {
        //    size_t leftIndex= m_indices->at(start);
        //    size_t rightIndex = m_indices->at(i);
        //    AABB leftBox(m_triangles->at(leftIndex).centroid(), m_triangles->at(leftIndex).centroid());
        //    AABB rightBox(m_triangles->at(rightIndex).centroid(), m_triangles->at(rightIndex).centroid());

        //    for (size_t ii = start + 1; ii < i; ++ii) {
        //        leftIndex = m_indices->at(ii);
        //        leftBox.min = FW::min(leftBox.min, m_triangles->at(leftIndex).centroid());
        //        leftBox.max = FW::max(leftBox.max, m_triangles->at(leftIndex).centroid());
        //    }

        //    for (size_t ii = i + 1; ii < end; ++ii) {
        //        rightIndex = m_indices->at(ii);
        //        rightBox.min = FW::min(rightBox.min, m_triangles->at(rightIndex).centroid());
        //        rightBox.max = FW::max(rightBox.max, m_triangles->at(rightIndex).centroid());
        //    }

        //    float leftArea = leftBox.area();
        //    float rightArea = rightBox.area();
        //    float totalArea = box.area();
        //    float cost = (leftArea * (i - start) + rightArea * (end - i)) / totalArea;
        //    if (cost < minCost) {
        //        minCost = cost;
        //        optMid = i;
        //    }
        //}


        // ****** optimized version of SAH, O(n) ******
        // ****** first compute all AABB area of [start,i] and [i,end], reduce redundant computation ******
        size_t leftIndex = m_indices->at(start);
        size_t rightIndex = m_indices->at(end - 1);
        AABB leftBox(m_triangles->at(leftIndex).centroid(), m_triangles->at(leftIndex).centroid());
        AABB rightBox(m_triangles->at(rightIndex).centroid(), m_triangles->at(rightIndex).centroid());

        std::vector<float> leftChildrenArea(end - start);
        std::vector<float> rightChildrenArea(end - start);

        for (size_t i = start; i < end; ++i) {
            leftIndex = m_indices->at(i);
            leftBox.min = FW::min(leftBox.min, m_triangles->at(leftIndex).centroid());
            leftBox.max = FW::max(leftBox.max, m_triangles->at(leftIndex).centroid());
            leftChildrenArea[i - start] = leftBox.area();

            rightIndex = m_indices->at(start + end - i - 1);
            rightBox.min = FW::min(rightBox.min, m_triangles->at(rightIndex).centroid());
            rightBox.max = FW::max(rightBox.max, m_triangles->at(rightIndex).centroid());
            rightChildrenArea[end - 1 - i] = rightBox.area();
        }

        float totalArea = box.area();
        for (size_t i = start; i < end; ++i) {
            float leftArea = leftChildrenArea[i - start];
            float rightArea = rightChildrenArea[i - start];
            float cost = (leftArea * (i - start) + rightArea * (end - i)) / totalArea;
            if (cost < minCost) {
                minCost = cost;
                optMid = i;
            }
        }

        node->left = constructBvhSah(start, optMid);
        node->right = constructBvhSah(optMid, end);
        node->startPrim = start;
        node->endPrim = end;
        node->bb = AABB(FW::min(node->left->bb.min, node->right->bb.min), FW::max(node->left->bb.max, node->right->bb.max));
        return node;
    }
}

void RayTracer::constructHierarchy(std::vector<RTTriangle>& triangles, SplitMode splitMode) {
    // YOUR CODE HERE (R1):
    // This is where you should construct your BVH.

    m_triangles = &triangles;
    m_indices = &(m_bvh.getIndices());
    m_indices->resize(triangles.size());
    std::iota(m_indices->begin(), m_indices->end(), 0);
    std::unique_ptr<BvhNode> root = constructBvhSah(0, triangles.size());
    m_bvh.setRoot(std::move(root));
}

RaycastResult RayTracer::intersect(const BvhNode& node, const Vec3f& orig, const Vec3f& dir, const Vec3f& normDir, const Vec3f& invDir) const {

    std::array<bool, 3> dirIsNeg{ normDir.x > 0, normDir.y > 0, normDir.z > 0 };
    if (node.bb.intersect(orig, invDir, dirIsNeg) == false) {
        return RaycastResult();
    }

    if (node.left == nullptr && node.right == nullptr) {
        float closest_t = 1.0f, closest_u = 0.0f, closest_v = 0.0f;
        int closest_i = -1;

        RaycastResult castresult;
        size_t index;
        for ( int i = node.startPrim; i < node.endPrim; ++i )
        {
            float t, u, v;
            index = m_indices->at(i);
            if ( (*m_triangles)[index].intersect_woop( orig, dir, t, u, v ) )
            {
                if ( t > 0.0f && t < closest_t)
                {
                    closest_i = index;
                    closest_t = t;
                    closest_u = u;
                    closest_v = v;
                }
            }
        }

        if (closest_i != -1)
            castresult = RaycastResult(&(*m_triangles)[closest_i], closest_t, closest_u, closest_v, orig + closest_t *dir, orig, dir);
        
        return castresult;
    }

    RaycastResult leftHit = intersect(*(node.left), orig, dir, normDir, invDir);
    RaycastResult rightHit = intersect(*(node.right), orig, dir, normDir, invDir);

    return leftHit.t < rightHit.t ? std::move(leftHit) : std::move(rightHit);
}

RaycastResult RayTracer::raycast(const Vec3f& orig, const Vec3f& dir) const {
	++m_rayCount;

    Vec3f normDir = dir.normalized();
    Vec3f invDir = Vec3f(1. / normDir.x, 1. / normDir.y, 1. / normDir.z);
    return intersect(m_bvh.root(), orig, dir, normDir, invDir);

    // YOUR CODE HERE (R1):
    // This is where you traverse the tree you built! It's probably easiest
    // to introduce another function above that does the actual traversal, and
    // use this function only to begin the recursion by calling the traversal
    // function with the given ray and your root node. You can also use this
    // function to do one-off things per ray like finding the elementwise
    // reciprocal of the ray direction.

    // You can use this existing code for leaf nodes of the BVH (you do want to
    // change the range of the loop to match the elements the leaf covers.)
    //float closest_t = 1.0f, closest_u = 0.0f, closest_v = 0.0f;
    //int closest_i = -1;

    //RaycastResult castresult;

    //// Naive loop over all triangles; this will give you the correct results,
    //// but is terribly slow when ran for all triangles for each ray. Try it.
    //for ( int i = 0; i < m_triangles->size(); ++i )
    //{
    //    float t, u, v;
    //    if ( (*m_triangles)[i].intersect_woop( orig, dir, t, u, v ) )
    //    {
    //        if ( t > 0.0f && t < closest_t)
    //        {
    //            closest_i = i;
    //            closest_t = t;
    //            closest_u = u;
    //            closest_v = v;
    //        }
    //    }
    //}

    //if (closest_i != -1)
    //    castresult = RaycastResult(&(*m_triangles)[closest_i], closest_t, closest_u, closest_v, orig + closest_t *dir, orig, dir);
    //
    //return castresult;
}


} // namespace FW