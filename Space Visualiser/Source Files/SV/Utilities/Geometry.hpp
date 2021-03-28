#pragma once
#include"SV/Utilities/GlobalItems.hpp"
#include"Array.hpp"
#include"SVMemory.hpp"
#include"SV/OpenGL/Texture.hpp"
#include<array>

namespace SV
{

#pragma region Circle Constants
	//
	constexpr float DefaultCircleRadius = 1.0f;
	constexpr unsigned int DefaultCircleSegments = 32;
	//
#pragma endregion

	//
	class Mesh : public SV::StaticArrayGroup<4>
	{
	public:
		Mesh() : Mesh(0) {}
		Mesh(const size_t bufferByteSize) : SV::StaticArrayGroup<4>(bufferByteSize) {}
		//
		inline SV::ArraySpan<float>& GetVertexArray(const size_t floatCount = 0)
		{
			if (this->GetArray<float>(0) != nullptr) return this->GetArray<float>(0);
			return this->AllocateArray<float>(floatCount, 0);
		}
		inline SV::ArraySpan<float>& GetNormalsArray(const size_t floatCount = 0)
		{
			if (this->GetArray<float>(1) != nullptr) return this->GetArray<float>(1);
			return this->AllocateArray<float>(floatCount, 1);
		}
		inline SV::ArraySpan<float>& GetTexCoordsArray(const size_t floatCount = 0)
		{
			if (this->GetArray<float>(2) != nullptr) return this->GetArray<float>(2);
			return this->AllocateArray<float>(floatCount, 2);
		}
		inline SV::ArraySpan<unsigned int>& GetIndiciesArray(const size_t intCount = 0)
		{
			if (this->GetArray<unsigned int>(3) != nullptr) return this->GetArray<unsigned int>(3);
			return this->AllocateArray<unsigned int>(intCount, 3);
		}
		//
	};

	constexpr double ICO_X = 0.525731112119133606;
	constexpr double ICO_Z = 0.850650808352039932;
	constexpr double ICO_N = 0.0;

	constexpr std::array<double, 36> ICOSAHEDRON_VERTS = {
				ICO_X, -ICO_N, -ICO_Z,
				-ICO_X, -ICO_N, -ICO_Z,
				ICO_X, -ICO_N, ICO_Z,
				-ICO_X, -ICO_N, ICO_Z,
				-ICO_N, -ICO_Z, -ICO_X,
				-ICO_N, -ICO_Z, ICO_X,
				-ICO_N, ICO_Z, -ICO_X,
				-ICO_N, ICO_Z, ICO_X,
				-ICO_Z, -ICO_X, -ICO_N,
				ICO_Z, -ICO_X, -ICO_N,
				-ICO_Z, ICO_X, -ICO_N,
				ICO_Z, ICO_X, -ICO_N
	};
	constexpr std::array<unsigned int, 60> ICOSAHEDRON_TRIS = {
0,4,1 , 0,9,4 , 9,5,4 , 4,5,8 , 4,8,1 ,
 8,10,1 , 8,3,10 , 5,3,8 , 5,2,3 , 2,7,3 ,
 7,10,3 , 7,6,10 , 7,11,6 , 11,0,6 , 0,1,6 ,
 6,1,10 , 9,0,11 , 9,11,2 , 9,2,5 , 7,2,11
	};


	//
	class Geometry final abstract
	{
	public:
		using TriangleI = glm::uivec3;
		using Vertex = glm::vec3;
		using TexCoord = glm::vec2;
		//
		static glm::vec3 CalculateNormal(const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2);
		//
		static Ref<Mesh> CreateUVSphere(float radius, unsigned int lattitudes, unsigned int longitudes);
		static Ref<Mesh> CreateQuadsphere(float radius, unsigned int subdivisions);
		//
		static Ref<Mesh> CreateIcosahedron(float size = 1.0f);
		static Ref<Mesh> CreateIcosphere(float radius, unsigned int subdivisions);
	private:
		//
		static SV::Array<unsigned int> DetectSphereWrappedUVCoords(const ArrayView<TriangleI> triangles, const ArrayView<glm::vec2> texCoords, unsigned int hintCount = 0);
		static void FixSphereWrappedUV(const SV::Array<unsigned int>& wrapped, SV::Array<glm::vec3>& verticies, SV::Array<TriangleI>& triangles, SV::Array<glm::vec2>& texCoords, unsigned int hintCount = 0);
		static void FixPoleVerticies(SV::Array<glm::vec3>& verticies, SV::Array<glm::vec2>& texCoords, SV::Array<TriangleI>& triangles);
		//
	public:
		//
		static  Ref<Mesh> CreateQuad();
		//
		static Ref<Mesh> CreateGrid(float scale, unsigned int slices, unsigned int stacks);
		//
		static Ref<Mesh> CreateCube(float size = 1.0f);
		//
		static Ref<Mesh> CreateCircle(float radius, unsigned int segments);
		static Ref<Mesh> CreateRings(float innerRadius, float outerRadius, unsigned int segments);
		static Ref<Mesh> CreateHalfRings(float innerRadius, float outerRadius, unsigned int segments);
		//
		static Ref<Mesh> CreateTorus(float innerRadius, float outerRadius, float depth, unsigned int sides, unsigned int rings, unsigned short twistyness = 1);
		//
	};
}