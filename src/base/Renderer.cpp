#include "Renderer.hpp"
#include "RayTracer.hpp"

#include <atomic>
#include <chrono>


namespace FW {


Renderer::Renderer()
{
    m_aoRayLength = 0.5f;
	m_aoNumRays = 16;
	m_aaNumRays = 1;
    m_raysPerSecond = 0.0f;
}

Renderer::~Renderer()
{
}

void Renderer::gatherLightTriangles(RayTracer* rt) {
	// gather light triangles into vector for possible later use in the area light extra
	m_lightTriangles.clear();
	m_combinedLightArea = .0f;
	for (auto& tri : *rt->m_triangles)
	{
		MeshBase::Material* mat = tri.m_material;
		if (mat->emission.length() > .0f)
		{
			m_lightTriangles.push_back(&tri);
			m_combinedLightArea += tri.area();
		}
	}
}

timingResult Renderer::rayTracePicture( RayTracer* rt, Image* image, const CameraControls& cameraCtrl, ShadingMode mode )
{

    // measure time to render
	LARGE_INTEGER start, stop, frequency;
	QueryPerformanceFrequency(&frequency);
	QueryPerformanceCounter(&start); // Start time stamp	
	rt->resetRayCounter();

    // this has a side effect of forcing Image to reserve its memory immediately
    // otherwise we get a rendering bug & memory leak in OpenMP parallel code
    image->getMutablePtr();

    // get camera orientation and projection
    Mat4f worldToCamera = cameraCtrl.getWorldToCamera();
    Mat4f projection = Mat4f::fitToView(Vec2f(-1,-1), Vec2f(2,2), image->getSize())*cameraCtrl.getCameraToClip();

    // inverse projection from clip space to world space
    Mat4f invP = (projection * worldToCamera).inverted();

    // progress counter
    std::atomic<int> lines_done = 0;

    int height = image->getSize().y;
    int width = image->getSize().x;

	for (int j = 0; j < height; j++)
	for (int i = 0; i < width; i++)
		image->setVec4f(Vec2i(i, j), Vec4f(.0f)); //initialize image to 0

	// YOUR CODE HERE(R5):
	// remove this to enable multithreading (you also need to enable it in the project properties: C++/Language/Open MP support)
	#pragma omp parallel for
    for ( int j = 0; j < height; ++j )
    {
        // Each thread must have its own random generator
        Random rnd;

        for ( int i = 0; i < width; ++i )
        {
            // generate ray through pixel
				float x = (i + 0.5f) / image->getSize().x *  2.0f - 1.0f;
				float y = (j + 0.5f) / image->getSize().y * -2.0f + 1.0f;
				// point on front plane in homogeneous coordinates
				Vec4f P0( x, y, 0.0f, 1.0f );
				// point on back plane in homogeneous coordinates
				Vec4f P1( x, y, 1.0f, 1.0f );

				// apply inverse projection, divide by w to get object-space points
				Vec4f Roh = (invP * P0);
				Vec3f Ro = (Roh * (1.0f / Roh.w)).getXYZ();
				Vec4f Rdh = (invP * P1);
				Vec3f Rd = (Rdh * (1.0f / Rdh.w)).getXYZ();

				// Subtract front plane point from back plane point,
				// yields ray direction.
				// NOTE that it's not normalized; the direction Rd is defined
				// so that the segment to be traced is [Ro, Ro+Rd], i.e.,
				// intersections that come _after_ the point Ro+Rd are to be discarded.
				Rd = Rd - Ro;

				// trace!
				RaycastResult hit = rt->raycast( Ro, Rd );

				// if we hit something, fetch a color and insert into image
				Vec4f color(0,0,0,1);
				if ( hit.tri != nullptr )
				{
					switch( mode )
					{
					case ShadingMode_Headlight:
						color = computeShadingHeadlight( hit, cameraCtrl);
						break;
					case ShadingMode_AmbientOcclusion:
						color = computeShadingAmbientOcclusion( rt, hit, cameraCtrl, rnd );
						break;
					case ShadingMode_Whitted:
						color = computeShadingWhitted( rt, hit, cameraCtrl, rnd, 0 );
						break;
					}
				}
				// put pixel.
				image->setVec4f(Vec2i(i, j), color);
        }

        // Print progress info
		#pragma omp critical
		{
			++lines_done;
			::printf("%.2f%% \r", lines_done * 100.0f / height);
		}
    }

	// how fast did we go?
	timingResult result;

	QueryPerformanceCounter(&stop); // Stop time stamp

	result.duration = (int)((stop.QuadPart - start.QuadPart) * 1000.0 / frequency.QuadPart); // Get timer result in milliseconds

	// calculate average rays per second
	result.rayCount = rt->getRayCount();
    m_raysPerSecond = 1000.0f * result.rayCount / result.duration;

    printf("\n");

	return result;
}


void Renderer::getTextureParameters(const RaycastResult& hit, Vec3f& diffuse, Vec3f& n, Vec3f& specular)
{
	MeshBase::Material* mat = hit.tri->m_material;
	// YOUR CODE HERE (R3):
	// using the barycentric coordinates of the intersection (hit.u, hit.v) and the
	// vertex texture coordinates hit.tri->m_vertices[i].t of the intersected triangle,
	// compute the uv coordinate of the intersection point.

	Vec2f uv0 = hit.tri->m_vertices[0].t;
	Vec2f uv1 = hit.tri->m_vertices[1].t;
	Vec2f uv2 = hit.tri->m_vertices[2].t;

	Vec2f uv = (1 - hit.u - hit.v) * uv0 + hit.u * uv1 + hit.v * uv2;

	Texture& diffuseTex = mat->textures[MeshBase::TextureType_Diffuse]; //note: you can fetch other kinds of textures like this too. 
																		//By default specular maps, displacement maps and alpha stencils
																		//are loaded too if the .mtl file specifies them.
	if (diffuseTex.exists()) //check whether material uses a diffuse texture
	{
		const Image& img = *diffuseTex.getImage();

		if (m_bilinearFiltering) {
			Vec2f texelCoords = getTexelCoordsBilinear(uv, img.getSize());
			Vec2f texelCoordsPositive(texelCoords.x < 0 ? texelCoords.x + img.getSize().x : texelCoords.x, texelCoords.y < 0 ? texelCoords.y + img.getSize().y : texelCoords.y);
			int leftX = static_cast<int>(std::floor(texelCoordsPositive.x)) % img.getSize().x;
			int leftY = static_cast<int>(std::floor(texelCoordsPositive.y)) % img.getSize().y;
			int rightX = static_cast<int>(std::ceil(texelCoordsPositive.x)) % img.getSize().x;
			int rightY = static_cast<int>(std::ceil(texelCoordsPositive.y)) % img.getSize().y;

			Vec3f diffuse1 = img.getVec4f(Vec2i(leftX, leftY)).getXYZ();
			Vec3f diffuse2 = img.getVec4f(Vec2i(leftX, rightY)).getXYZ();
			Vec3f diffuse3 = img.getVec4f(Vec2i(rightX, leftY)).getXYZ();
			Vec3f diffuse4 = img.getVec4f(Vec2i(rightX, rightY)).getXYZ();

			Vec3f tmpX1 = diffuse1 + (texelCoordsPositive.x - static_cast<float>(leftX)) * (diffuse3 - diffuse1);
			Vec3f tmpX2 = diffuse2 + (texelCoordsPositive.x - static_cast<float>(leftX)) * (diffuse4 - diffuse2);

			diffuse = tmpX1 + (texelCoordsPositive.y - static_cast<float>(leftY)) * (tmpX2 - tmpX1);
		}
		else {
			//fetch diffuse color from texture
			Vec2i texelCoords = getTexelCoords(uv, img.getSize());

			// YOUR CODE HERE (R3): uncomment the line below once you have implemented getTexelCoords.
			diffuse = img.getVec4f(texelCoords).getXYZ();
		}
	}
	Texture& normalTex = mat->textures[MeshBase::TextureType_Normal];
	if (normalTex.exists() && m_normalMapped) //check whether material uses a normal map
	{
		const Image& img = *normalTex.getImage();
		//EXTRA: do tangent space normal mapping
		//first, get texel coordinates as above, for the rest, see handout
		Vec2i texelCoords = getTexelCoords(uv, img.getSize());
		Vec3f norm = img.getVec4f(texelCoords).getXYZ() * 2.0f - 1.0f;

		Vec3f deltaPos1 = hit.tri->m_vertices[1].p - hit.tri->m_vertices[0].p;
		Vec3f deltaPos2 = hit.tri->m_vertices[2].p - hit.tri->m_vertices[0].p;

		Vec2f deltaUV1 = uv1 - uv0;
		Vec2f deltaUV2 = uv2 - uv0;

		float r = 1.0f / (deltaUV1.x * deltaUV2.y - deltaUV1.y * deltaUV2.x);
		Vec3f t = ((deltaPos1 * deltaUV2.y - deltaPos2 * deltaUV1.y) * r).normalized();
		Vec3f b = ((deltaPos2 * deltaUV1.x - deltaPos1 * deltaUV2.x) * r).normalized();

		Mat3f tbn;

		tbn.col(0) = t;
		tbn.col(1) = b;
		tbn.col(2) = n;

		n = (tbn * norm).normalized();
	}

	// EXTRA: read a value from the specular texture into specular_mult.
	Texture& specularTex = mat->textures[MeshBase::TextureType_Specular];
	if (specularTex.exists() && m_specularMapped) //check whether material uses a normal map
	{
		const Image& img = *specularTex.getImage();
		Vec2i texelCoords = getTexelCoords(uv, img.getSize());
		specular = img.getVec4f(texelCoords).getXYZ();
	}
}

Vec4f Renderer::computeShadingHeadlight(const RaycastResult& hit, const CameraControls& cameraCtrl)
{
	// get diffuse color
	MeshBase::Material* mat = hit.tri->m_material;
	Vec3f diffuse = mat->diffuse.getXYZ();
	Vec3f n(hit.tri->normal());
	Vec3f specular = mat->specular; // specular color. Not used in requirements, but you can use this in extras if you wish.

	if (m_useTextures)
		getTextureParameters(hit, diffuse, n, specular);

    // dot with view ray direction <=> "headlight shading"
	float d = fabs(dot(n, (hit.point - cameraCtrl.getPosition()).normalized()));

    // assign gray value (d,d,d)
	Vec3f shade = d;

	if (m_specularMapped) {
		Vec3f hit2Cam((cameraCtrl.getPosition() - hit.point).normalized());
		Vec3f specularColor = Vec3f(pow(d, 100)) * specular * 0.69;

		return Vec4f(shade * (diffuse + specularColor), 1.0f);
	}

    return Vec4f( shade*diffuse, 1.0f );
}


Vec4f Renderer::computeShadingAmbientOcclusion(RayTracer* rt, const RaycastResult& hit, const CameraControls& cameraCtrl, Random& rnd)
{
    // YOUR CODE HERE (R4)
	Vec3f hit2Cam((cameraCtrl.getPosition() - hit.point).normalized());
	Vec3f hitPoint((hit2Cam * 0.001) + hit.point);
	Vec3f n(hit.tri->normal());

	if (dot(hit2Cam, n) < 0) {
		n = -n;
	}

	Mat3f rotationMat = formBasis(n);

	int totalNoHit = m_aoNumRays;
	for (int i = 0; i < m_aoNumRays; ++i) {

		float x, y, z;
		do {
			x = rnd.getF32(-1, 1);
			y = rnd.getF32(-1, 1);
		} while (x * x + y * y > 1.0);

		Vec3f rayDirection(x, y, sqrtf(1 - x * x - y * y));

		RaycastResult newHit = rt->raycast(hitPoint, (rotationMat * rayDirection) * m_aoRayLength);

		if (newHit) {
			totalNoHit--;
		}
	}

	return Vec4f(static_cast<float>(totalNoHit) / static_cast<float>(m_aoNumRays));
}

Vec4f Renderer::computeShadingWhitted(RayTracer* rt, const RaycastResult& hit, const CameraControls& cameraCtrl, Random& rnd, int num_bounces)
{
	//EXTRA: implement a whitted integrator
	return Vec4f(.0f);
}

} // namespace FW
