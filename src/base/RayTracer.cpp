#define _CRT_SECURE_NO_WARNINGS

#include "base/Defs.hpp"
#include "base/Math.hpp"
#include "RayTracer.hpp"
#include <stdio.h>
#include "rtIntersect.inl"
#include <fstream>
#include <algorithm>
#include <array>
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
    
	return Vec2f();
}

Mat3f formBasis(const Vec3f& n) {
    // YOUR CODE HERE (R4):
    return Mat3f();
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
}

void RayTracer::saveHierarchy(const char* filename, const std::vector<RTTriangle>& triangles) {
	(void)triangles; // Not used.

	std::ofstream ofs(filename, std::ios::binary);
	m_bvh.save(ofs);
}

// R1 code
std::unique_ptr<BvhNode> RayTracer::constructBvh(std::vector<RTTriangle>& triangles, size_t start, size_t end) {
    
    std::unique_ptr<BvhNode> node = std::make_unique<BvhNode>();

    if (end - start <= 6) {
        AABB box(triangles[start].min(), triangles[start].max());
        for (size_t i = start + 1; i < end; ++i) {
            box.min = FW::min(box.min, triangles[i].min());
            box.max = FW::max(box.max, triangles[i].max());
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
        AABB box(triangles[start].min(), triangles[start].max());
        for (size_t i = start + 1; i < end; ++i) {
            Vec3f tmp = (triangles[i].min() + triangles[i].max()) * 0.5;
            box.min = FW::min(box.min, tmp);
            box.max = FW::max(box.max, tmp);
        }

        Vec3f diagonal = box.max - box.min;

        if (diagonal.x > diagonal.y && diagonal.x > diagonal.z) {
            std::sort(triangles.begin() + start, triangles.begin() + end, [](auto f1, auto f2) {
                Vec3f tmp1 = (f1.min() + f1.max()) * 0.5;
                Vec3f tmp2 = (f2.min() + f2.max()) * 0.5;
                return tmp1.x < tmp2.x;
            });
        }
        else if (diagonal.y > diagonal.z) {
            std::sort(triangles.begin() + start, triangles.begin() + end, [](auto f1, auto f2) {
                Vec3f tmp1 = (f1.min() + f1.max()) * 0.5;
                Vec3f tmp2 = (f2.min() + f2.max()) * 0.5;
                return tmp1.y < tmp2.y;
            });
        } 
        else {
            std::sort(triangles.begin() + start, triangles.begin() + end, [](auto f1, auto f2) {
                Vec3f tmp1 = (f1.min() + f1.max()) * 0.5;
                Vec3f tmp2 = (f2.min() + f2.max()) * 0.5;
                return tmp1.z < tmp2.z;
            });
        }

        size_t mid = start + ((end - start) / 2);

        node->left = constructBvh(triangles, start, mid);
        node->right = constructBvh(triangles, mid, end);
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
    std::unique_ptr<BvhNode> root = constructBvh(triangles, 0, triangles.size());
    m_bvh.setRoot(std::move(root));
}

RaycastResult RayTracer::intersect(const BvhNode& node, const Vec3f& orig, const Vec3f& dir, const Vec3f& normDir, const Vec3f& invDir, float length) const {

    std::array<bool, 3> dirIsNeg{ normDir.x > 0, normDir.y > 0, normDir.z > 0 };
    if (node.bb.intersect(orig, invDir, dirIsNeg) == false) {
        return RaycastResult();
    }

    if (node.left == nullptr && node.right == nullptr) {
        float closest_t = 1.0f, closest_u = 0.0f, closest_v = 0.0f;
        int closest_i = -1;

        RaycastResult castresult;
        for ( int i = node.startPrim; i < node.endPrim; ++i )
        {
            float t, u, v;
            if ( (*m_triangles)[i].intersect_woop( orig, dir, t, u, v ) )
            {
                if ( t > 0.0f && t < closest_t)
                {
                    closest_i = i;
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

    RaycastResult leftHit = intersect(*(node.left), orig, dir, normDir, invDir, length);
    RaycastResult rightHit = intersect(*(node.right), orig, dir, normDir, invDir, length);

    //return leftHit.t < rightHit.t ? std::move(leftHit) : std::move(rightHit);
    RaycastResult finalHit = leftHit.t < rightHit.t ? std::move(leftHit) : std::move(rightHit);

    return finalHit.t < length ? finalHit : RaycastResult();
}

RaycastResult RayTracer::raycast(const Vec3f& orig, const Vec3f& dir) const {
	++m_rayCount;

    Vec3f normDir = dir.normalized();
    Vec3f invDir = Vec3f(1. / normDir.x, 1. / normDir.y, 1. / normDir.z);
    return intersect(m_bvh.root(), orig, dir, normDir, invDir, dir.length());

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