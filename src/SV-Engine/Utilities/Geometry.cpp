#include"SV-Engine/Precompiled Global.hpp"
#include"Geometry.hpp"
#include"SV-Engine/Utilities/Array.hpp"
#include"SV-Engine/Utilities/SVUtilities.hpp"
#include<cmath>
#include"SV-Engine/Utilities/Maths.hpp"
#include"SV-Engine/OpenGL/Texture.hpp"
#include"SV-Engine/Utilities/Errors.hpp"

#include<vector>
#include<map>
#include<set>

namespace SV
{
	//
#pragma region Geometry
	//
	glm::vec3 Geometry::CalculateNormal(const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2)
	{
		//	glm::vec3 A = v1 - v0;
		//	glm::vec3 B = v2 - v0;
		//	glm::vec3 N = glm::cross(A, B);
		//	float sinAlpha = glm::length(N) / (glm::length(A) * glm::length(B));
		//	return glm::normalize(N) * sinAlpha;
		return glm::normalize(glm::cross(v1 - v0, v2 - v0));
	}
	//
	Ref<Mesh> Geometry::CreateUVSphere(float radius, unsigned int lattitudes, unsigned int longitudes)
	{
		SV_ASSERT_T(lattitudes < 3, SV::Geometry::CreateSphere, "The minimum number of slices is 3");
		SV_ASSERT_T(longitudes < 3, SV::Geometry::CreateSphere, "The minimum number of stacks is 3");

		SV::Array<Vertex> verticies(((size_t)lattitudes) * (longitudes - 1) + 2, false);
		SV::Array<glm::vec2> texCoord(verticies.GetCapacity(), false);
		SV::Array<TriangleI> triangles(((size_t)longitudes - 2) * lattitudes * 2 + 2 * lattitudes, false);

		unsigned int i = 0;

		{
			unsigned int j = 0;
			glm::vec3 currentVertex;

			//Write data for top most vertex
			verticies.Emplace(0.0f, 1.0f, 0.0f);
			texCoord.Emplace(0.5f, 0.0f);

			for (i = 1; i < longitudes; i++)
			{
				//Compute the V texture coordinate
				float V = float(i) / float(longitudes);
				//Compute the stack's angle
				float Phi = V * SV::Maths::PI;

				for (j = 0; j < lattitudes; j++)
				{
					//Compute the U texture coordinate
					float U = float(j) / float(lattitudes);
					//Compute the slice's angle
					float Theta = U * SV::Maths::Tau;

					currentVertex.x = cos(Theta) * sin(Phi);
					currentVertex.y = cos(Phi);
					currentVertex.z = sin(Theta) * sin(Phi);

					verticies.Add(currentVertex);

					texCoord.Emplace(
						((atan2(currentVertex.x, currentVertex.z) + Maths::PI) / Maths::PI) * 0.5f,
						((acos(currentVertex.y) + Maths::PI) / Maths::PI) - 1.0f);
				}
			}

			//Write data for bottom most vertex
			verticies.Emplace(0.0f, -1.0f, 0.0f);
			texCoord.Emplace(0.5f, 1.0f);
		}

		{
			const unsigned int topTriangleCount = lattitudes;
			const unsigned int mainFaceCount = (longitudes - 2) * lattitudes;

			for (i = 0; i < topTriangleCount; i++)
			{
				triangles.Emplace(
					i == topTriangleCount - 1 ? 1 : i + 2,
					i + 1,
					0);
			}

			//Face count instead of triangle count as two triangles are written per iteration
			for (i = topTriangleCount; i < mainFaceCount + topTriangleCount; i++)
			{
				if ((i + 1) % (lattitudes) == 0)
				{
					//First triangle's indices
					triangles.Emplace(
						i - lattitudes + 2,
						i + 1,
						i - lattitudes + 1);
					//Second triangle's indices
					triangles.Emplace(
						i - lattitudes + 2,
						i - lattitudes + 1,
						i - lattitudes - (lattitudes - 1) + 1);

					continue;
				}

				//First triangle's indices
				triangles.Emplace(
					i + 2,
					i + 1,
					i - lattitudes + 1);
				//Second triangle's indices
				triangles.Emplace(
					i - lattitudes + 1,
					i - lattitudes + 2,
					i + 2);
			}

			for (i = 0; i < topTriangleCount; i++)
			{
				unsigned int triIndex = i + verticies.GetCount() - 1 - lattitudes;
				triangles.Emplace(
					i == topTriangleCount - 1 ? triIndex - (lattitudes - 1) : triIndex + 1,
					verticies.GetCount() - 1,
					triIndex);
			}

			SV::Array<unsigned int> wrappedCoords = move(DetectSphereWrappedUVCoords(triangles, texCoord, longitudes));
			FixSphereWrappedUV(wrappedCoords, verticies, triangles, texCoord);
		}

		Ref<Mesh> sphereMesh = make <SV::Mesh>(
			verticies.GetCount() * sizeof(glm::vec3) * 2 +
			texCoord.GetCount() * sizeof(glm::vec2) +
			triangles.GetCount() * sizeof(TriangleI)
			);

		ArraySpan<float> verticiesSpan = sphereMesh->GetVertexArray(verticies.GetCount() * 3);
		ArraySpan<float> normalsSpan = sphereMesh->GetNormalsArray(verticies.GetCount() * 3);
		ArraySpan<float> texCoordsSpan = sphereMesh->GetTexCoordsArray(texCoord.GetCount() * 2);
		ArraySpan<unsigned int> indiciesSpan = sphereMesh->GetIndiciesArray(triangles.GetCount() * 3);

		memcpy(normalsSpan.GetInternalArray(), verticies.GetInternalArray(), verticies.GetCount() * sizeof(glm::vec3));

		for (glm::vec3& vertex : verticies)
		{
			vertex *= radius;
		}

		memcpy(verticiesSpan.GetInternalArray(), verticies.GetInternalArray(), verticies.GetCount() * sizeof(glm::vec3));
		memcpy(texCoordsSpan.GetInternalArray(), texCoord.GetInternalArray(), texCoord.GetCount() * sizeof(glm::vec2));
		memcpy(indiciesSpan.GetInternalArray(), triangles.GetInternalArray(), triangles.GetCount() * sizeof(TriangleI));

		return sphereMesh;
	}
	//
	Ref<Mesh> Geometry::CreateQuadsphere(float radius, unsigned int subdivisions)
	{
		SV::Array<glm::vec3> verticiesArray(subdivisions * subdivisions * 6, true);
		SV::Array<TriangleI> trianglesArray((subdivisions - 1) * (subdivisions - 1) * 2 * 6, true);
		SV::Array<glm::vec2> texCoordsArray(verticiesArray.GetCount(), true);

		constexpr glm::vec3 localUps[] = {
			glm::vec3{0.0f, 1.0f, 0.0f},
			glm::vec3{0.0f, -1.0f, 0.0f},

			glm::vec3{1.0f, 0.0f, 0.0f},
			glm::vec3{-1.0f, 0.0f, 0.0f},

			glm::vec3{0.0f, 0.0f, 1.0f},
			glm::vec3{0.0f, 0.0f, -1.0f}
		};

		glm::vec3 axisA;
		glm::vec3 axisB;

		unsigned int triIndex = 0;

		for (unsigned int faces = 0; faces < 6; faces++)
		{
			axisA.x = localUps[faces].y;
			axisA.y = localUps[faces].z;
			axisA.z = localUps[faces].x;

			axisB = glm::cross(localUps[faces], axisA);

			for (unsigned int y = 0; y < subdivisions; y++)
			{
				for (unsigned int x = 0; x < subdivisions; x++)
				{
					glm::vec2 percent(x, y);
					percent /= (float)(subdivisions - 1);

					unsigned int index = x + y * subdivisions + faces * subdivisions * subdivisions;
					verticiesArray[index] = glm::normalize(
						localUps[faces] +
						(percent.x - 0.5f) * 2.0f * axisA +
						(percent.y - 0.5f) * 2.0f * axisB);

					if (x != subdivisions - 1 && y != subdivisions - 1)
					{
						//trianglesArray[triIndex] = index;
						//trianglesArray[triIndex + 1] = index + subdivisions + 1;
						//trianglesArray[triIndex + 2] = index + subdivisions;

						//trianglesArray[triIndex + 3] = index;
						//trianglesArray[triIndex + 4] = index + 1;
						//trianglesArray[triIndex + 5] = index + subdivisions + 1;

						trianglesArray[triIndex].x = index;
						trianglesArray[triIndex].y = index + subdivisions + 1;
						trianglesArray[triIndex].z = index + subdivisions;

						trianglesArray[triIndex + 1].x = index;
						trianglesArray[triIndex + 1].y = index + 1;
						trianglesArray[triIndex + 1].z = index + subdivisions + 1;

						triIndex += 2;
					}
				}
			}
		}

		for (unsigned int i = 0; i < verticiesArray.GetCount(); i++)
		{
			const glm::vec3& normal = verticiesArray[i];

			texCoordsArray[i].x = ((atan2(normal.x, normal.z) + Maths::PI) / Maths::PI) * 0.5f;
			texCoordsArray[i].y = ((acos(normal.y) + Maths::PI) / Maths::PI) - 1.0;
		}

		SV::Array<unsigned int> wrappedCoords = move(DetectSphereWrappedUVCoords(trianglesArray, texCoordsArray, subdivisions * 4));
		FixSphereWrappedUV(wrappedCoords, verticiesArray, trianglesArray, texCoordsArray, (unsigned int)(subdivisions * 4.5f));

		Ref<SV::Mesh> sphereMesh = make <SV::Mesh>(
			verticiesArray.GetCount() * sizeof(glm::vec3) * 2 /*For normals array */ +
			trianglesArray.GetCount() * sizeof(TriangleI) +
			texCoordsArray.GetCount() * sizeof(glm::vec2)
			);

		ArraySpan<float>& vertexArrayS = sphereMesh->GetVertexArray(verticiesArray.GetCount() * 3);
		ArraySpan<unsigned int>& indiciesArrayS = sphereMesh->GetIndiciesArray(trianglesArray.GetCount() * 3);
		ArraySpan<float>& normalsArrayS = sphereMesh->GetNormalsArray(verticiesArray.GetCount() * 3);
		ArraySpan<float>& texCoordArrayS = sphereMesh->GetTexCoordsArray(texCoordsArray.GetCount() * 2);

		memcpy(normalsArrayS.GetInternalArray(), verticiesArray.GetInternalArray(), verticiesArray.GetCount() * sizeof(glm::vec3));

		for (glm::vec3& vertex : verticiesArray)
		{
			vertex *= radius;
		}

		memcpy(vertexArrayS.GetInternalArray(), verticiesArray.GetInternalArray(), verticiesArray.GetCount() * sizeof(glm::vec3));
		memcpy(indiciesArrayS.GetInternalArray(), trianglesArray.GetInternalArray(), trianglesArray.GetCount() * sizeof(TriangleI));
		memcpy(texCoordArrayS.GetInternalArray(), texCoordsArray.GetInternalArray(), texCoordsArray.GetCount() * sizeof(glm::vec2));

		return sphereMesh;
	}
	//
	Ref<Mesh> Geometry::CreateIcosahedron(float size)
	{
		Ref<Mesh> icosahedronMesh = make <SV::Mesh>(
			ICOSAHEDRON_VERTS.size() * sizeof(float) * 2 +
			ICOSAHEDRON_TRIS.size() * sizeof(unsigned int)
			);

		ArraySpan<float> Verticies = icosahedronMesh->GetVertexArray(ICOSAHEDRON_VERTS.size());
		ArraySpan<float> Normals = icosahedronMesh->GetNormalsArray(ICOSAHEDRON_VERTS.size());
		ArraySpan<unsigned int> Indicies = icosahedronMesh->GetIndiciesArray(ICOSAHEDRON_TRIS.size());

		unsigned int i = 0;

		for (; i < ICOSAHEDRON_VERTS.size(); i++)
		{
			Normals[i] = (float)ICOSAHEDRON_VERTS[i];
			Verticies[i] = (float)ICOSAHEDRON_VERTS[i] * size;
		}
		for (i = 0; i < ICOSAHEDRON_TRIS.size(); i++)
		{
			Indicies[i] = (unsigned int)ICOSAHEDRON_TRIS[i];
		}

		return icosahedronMesh;
	}
	//
	Ref<Mesh> Geometry::CreateIcosphere(float radius, unsigned int subdivisions)
	{
		auto PrecomputeNoTriangles = [](unsigned int subdivisions) -> unsigned int
		{
			return 20 * Maths::IntegerPow((unsigned int)4, subdivisions); };

		auto PrecomputeVertexAmount = [](unsigned int subdivisions)->unsigned int
		{
			unsigned int result = 12;

			for (unsigned int i = 1; i <= subdivisions; i++)
			{
				result += (30 * Maths::IntegerPow((unsigned int)4, i - 1));
			}
			return result;
		};

		SV::Array<Vertex> icoVerticies(PrecomputeVertexAmount(subdivisions), false);
		SV::Array<TriangleI> icoTriangles(PrecomputeNoTriangles(subdivisions), false);
		SV::Array<TriangleI> icoTrianglesBuffer(subdivisions > 0 ? PrecomputeNoTriangles(subdivisions - 1) : 0, false);

		unsigned int i = 0;

		//Write Icosahedron Data
		for (i = 0; i < ICOSAHEDRON_VERTS.size() / 3; i++)
		{
			icoVerticies.Emplace(
				ICOSAHEDRON_VERTS[i * 3 + 0],
				ICOSAHEDRON_VERTS[i * 3 + 1],
				ICOSAHEDRON_VERTS[i * 3 + 2]);
		}
		for (i = 0; i < ICOSAHEDRON_TRIS.size() / 3; i++)
		{
			icoTriangles.Emplace(
				ICOSAHEDRON_TRIS[i * 3 + 0],
				ICOSAHEDRON_TRIS[i * 3 + 1],
				ICOSAHEDRON_TRIS[i * 3 + 2]);
		}

		std::unordered_map<std::pair<unsigned int, unsigned int>, unsigned int> lookupTable;

		//Lamda functions for subdividing icosahedron
		auto CalculateVertex = [&lookupTable, &icoVerticies](unsigned int first, unsigned int second) -> unsigned int
		{
			std::pair<unsigned int, unsigned int> key(first, second);
			if (key.first > key.second)
			{
				std::swap(key.first, key.second);
			}

			auto inserted = lookupTable.insert({ key, icoVerticies.GetCount() });
			if (inserted.second)
			{
				const glm::vec3& edge0 = icoVerticies[first];
				const glm::vec3& edge1 = icoVerticies[second];
				icoVerticies.Add(glm::normalize(edge0 + edge1));
			}

			return inserted.first->second;
		};
		auto Subdivide = [&CalculateVertex, &icoTriangles, &icoTrianglesBuffer, &icoVerticies, &lookupTable]() ->void
		{
			TriangleI midPointTri;
			lookupTable.clear();
			icoTriangles.Empty();

			for (TriangleI& currentTri : icoTrianglesBuffer)
			{
				for (int edge = 0; edge < 3; ++edge)
				{
					midPointTri[edge] = CalculateVertex(currentTri[edge], currentTri[(edge + 1) % 3]);
				}

				icoTriangles.Emplace(currentTri[0], midPointTri[0], midPointTri[2]);
				icoTriangles.Emplace(currentTri[1], midPointTri[1], midPointTri[0]);
				icoTriangles.Emplace(currentTri[2], midPointTri[2], midPointTri[1]);
				icoTriangles.Emplace(midPointTri[0], midPointTri[1], midPointTri[2]);
			}
		};

		if (subdivisions > 0)
		{
			icoTrianglesBuffer = icoTriangles;
		}

		//Perform subdivisions
		for (i = 0; i < subdivisions; ++i)
		{
			Subdivide();

			if (i != subdivisions - 1)
			{
				icoTrianglesBuffer = icoTriangles;
			}
		}

		icoTrianglesBuffer.Clear();

		//Create an array to hold texture coordinates
		SV::Array<glm::vec2> texCoords(icoVerticies.GetCount(), true);
		//Populate texture coordinate array
		for (i = 0; i < icoVerticies.GetCount(); i++)
		{
			const glm::vec3& normal = icoVerticies[i];

			texCoords[i].x = ((atan2(normal.x, normal.z) + Maths::PI) / Maths::PI) * 0.5f;
			texCoords[i].y = ((acos(normal.y) + Maths::PI) / Maths::PI) - 1.0;
		}

		//Fix texture seam issue by duplicating verticies and correcting texture coordinates
		Array<unsigned int> wrappedCoords = move(DetectSphereWrappedUVCoords(icoTriangles, texCoords));
		FixSphereWrappedUV(wrappedCoords, icoVerticies, icoTriangles, texCoords);

		//Create a mesh object to store data
		Ref<SV::Mesh> icoSphereMesh = make <SV::Mesh>(
			icoVerticies.GetCount() * sizeof(glm::vec3) * 2 +
			icoTriangles.GetCount() * sizeof(glm::uivec3) +
			texCoords.GetCount() * 2 * sizeof(float)
			);

		ArraySpan<float> icosphereVerts = icoSphereMesh->GetVertexArray(icoVerticies.GetCount() * 3);
		ArraySpan<float> icosphereNorms = icoSphereMesh->GetNormalsArray(icoVerticies.GetCount() * 3);
		ArraySpan<unsigned int> icosphereIndicies = icoSphereMesh->GetIndiciesArray(icoTriangles.GetCount() * 3);
		ArraySpan<float> icosphereUV = icoSphereMesh->GetTexCoordsArray(icoVerticies.GetCount() * 2);

		//Copy normalised verticies first
		memcpy(icosphereNorms.GetInternalArray(), icoVerticies.GetInternalArray(), icoVerticies.GetCount() * sizeof(glm::vec3));

		//Scale all verticies by radius
		for (glm::vec3& vert : icoVerticies)
		{
			vert *= radius;
		}

		//Copy verticies, UV coordinates and triangles (indicies) into data buffer
		memcpy(icosphereVerts.GetInternalArray(), icoVerticies.GetInternalArray(), icoVerticies.GetCount() * sizeof(glm::vec3));
		memcpy(icosphereIndicies.GetInternalArray(), icoTriangles.GetInternalArray(), icoTriangles.GetCount() * sizeof(TriangleI));
		memcpy(icosphereUV.GetInternalArray(), texCoords.GetInternalArray(), texCoords.GetCount() * sizeof(glm::vec2));

		//Return icosphere mesh
		return icoSphereMesh;
	}
	//
	Ref<Mesh> Geometry::CreateQuad()
	{
		constexpr float quadVerticies[] = { -1.0f, -1.0f, 1.0f, -1.0f, 1.0f, 1.0f, -1.0f, 1.0f };
		constexpr unsigned int quadIndicies[] = { 0, 1, 3, 3, 1, 2 };

		constexpr unsigned int vertexCount = sizeof(quadVerticies) / sizeof(float);
		constexpr unsigned int  indiciesCount = sizeof(quadIndicies) / sizeof(unsigned int);

		Ref<Mesh> quadMesh = create <Mesh>(vertexCount * sizeof(float) + indiciesCount * sizeof(unsigned int));
		memcpy(quadMesh->GetVertexArray(vertexCount).GetInternalArray(), &quadVerticies, vertexCount * sizeof(float));
		memcpy(quadMesh->GetIndiciesArray(indiciesCount).GetInternalArray(), &quadIndicies, indiciesCount * sizeof(unsigned int));
		return quadMesh;
	}
	//
	Ref<Mesh> Geometry::CreateGrid(float scale, unsigned int slices, unsigned int stacks)
	{
		//unsigned int vertexCount = (slices + 1) * (stacks + 1) * 3;

		SV::Array<float> vertices;
		SV::Array<unsigned int> indices;

		unsigned int i = 0, j = 0;

		for (j = 0; j <= slices; ++j)
		{
			for (i = 0; i <= slices; ++i)
			{
				float x = ((float)i / (float)slices) * scale;
				float y = 0;
				float z = ((float)j / (float)slices) * scale;
				vertices.AddRange({ x,y,z });
			}
		}

		for (j = 0; j < slices; ++j)
		{
			for (i = 0; i < slices; ++i)
			{
				unsigned int  row1 = j * (slices + 1);
				unsigned int row2 = (j + 1) * (slices + 1);

				indices.AddRange({ row1 + i, row1 + i + 1, row1 + i + 1, row2 + i + 1 });
				indices.AddRange({ row2 + i + 1, row2 + i, row2 + i, row1 + i });
			}
		}

		Ref<Mesh> gridMesh = make <Mesh>(vertices.GetCount() * sizeof(float) + indices.GetCount() * sizeof(unsigned int));
		auto& vArraySpan = gridMesh->GetVertexArray(vertices.GetCount());
		auto& iArraySpan = gridMesh->GetIndiciesArray(indices.GetCount());

		memcpy(vArraySpan.GetInternalArray(), vertices.GetInternalArray(), vertices.GetCount() * sizeof(float));
		memcpy(iArraySpan.GetInternalArray(), indices.GetInternalArray(), indices.GetCount() * sizeof(unsigned int));

		return gridMesh;
	}
	//
	Ref<Mesh> Geometry::CreateCube(float size)
	{
		constexpr glm::vec3 cubeVerticies[] = {
			//Front
{-1,  -1, 1},
{1,  -1, 1},
{1,  1, 1},
{1,  1, 1},
{-1,  1, 1},
{-1,  -1, 1},

//Right
{1,  -1, 1},
{1,  -1, -1},
{1,  1, -1},
{1,  1, -1},
{1,  1, 1},
{1,  -1, 1},

//Back
{-1,  1, -1},
{1,  1, -1},
{1,  -1, -1},
{1,  -1, -1},
{-1,  -1, -1},
{-1,  1, -1},

//Left
{-1,  -1, -1},
{-1,  -1, 1},
{-1,  1, 1},
{-1,  1, 1},
{-1,  1, -1},
{-1,  -1, -1},

//Bottom
{-1,  -1, -1},
{1,  -1, -1},
{1,  -1, 1},
{1,  -1, 1},
{-1,  -1, 1},
{-1,  -1, -1},

//Top
{-1,  1, 1},
{1,  1, 1},
{1,  1, -1},
{1,  1, -1},
{-1,  1, -1},
{-1,  1, 1}
		};
		constexpr glm::vec3 cubeNormals[] = {
	{0, 0, 1},
{0, 0, 1},
{0, 0, 1},
{0, 0, 1},
{0, 0, 1},
{0, 0, 1},

{1, 0, 0},
{1, 0, -0},
{1, 0, 0},
{1, 0, -0},
{1, 0, 0},
{1, 0, -0},

{0, 0, -1},
{0, 0, -1},
{0, 0, -1},
{0, 0, -1},
{0, 0, -1},
{0, 0, -1},

{-1, 0, 0},
{-1, 0, -0},
{-1, 0, 0},
{-1, 0, -0},
{-1, 0, 0},
{-1, 0, -0},

{0, -1, 0},
{-0, -1, 0},
{0, -1, 0},
{-0, -1, 0},
{0, -1, 0},
{-0, -1, 0},

{-0, 1, 0},
{0, 1, 0},
{-0, 1, 0},
{0, 1, 0},
{-0, 1, 0},
{0, 1, 0}
		};
		constexpr glm::vec2 cubeUV[] = {
			//Front
			{0.0f, 0.0f},
			{1.0f, 0.0f},
			{1.0f, -1.0f},
			{1.0f, -1.0f},
			{0.0f, -1.0f},
			{0.0f, 0.0f},

			//Right
			{0.0f, 0.0f},
			{1.0f, 0.0f},
			{1.0f, -1.0f},
			{1.0f, -1.0f},
			{0.0f, -1.0f},
			{0.0f, 0.0f},

			//Back
			{1.0f, -1.0f},
			{0.0f, -1.0f},
			{0.0f, 0.0f},
			{0.0f, 0.0f},
			{1.0f, 0.0f},
			{1.0f, -1.0f},

			//Left
			{0.0f, 0.0f},
			{1.0f, 0.0f},
			{1.0f, -1.0f},
			{1.0f, -1.0f},
			{0.0f, -1.0f},
			{0.0f, 0.0f},

			//Bottom
			{0.0f, 0.0f},
			{-1.0f, 0.0f},
			{-1.0f, 1.0f},
			{-1.0f, 1.0f},
			{0.0f, 1.0f},
			{0.0f, 0.0f},

			//Top
			{0.0f, 0.0f},
			{-1.0f, 0.0f},
			{-1.0f, 1.0f},
			{-1.0f, 1.0f},
			{0.0f, 1.0f},
			{0.0f, 0.0f}
		};

		//SV::Array<glm::vec3> normals;
		//for (unsigned int i = 0; i < sizeof(cubeVerticies) / sizeof(glm::vec3) / 3; i++)
		//{
		//	glm::vec3 a = cubeVerticies[i * 3 + 0];
		//	glm::vec3 b = cubeVerticies[i * 3 + 1];
		//	glm::vec3 c = cubeVerticies[i * 3 + 2];

		//	normals.Add(CalculateNormal(a, b, c));
		//}

		//std::stringstream ss;
		//ss << "{";
		//for (glm::vec3& normal : normals)
		//{
		//	ss << "{" << normal.x << ", " << normal.y << ", " << normal.z << "},\n";
		//}
		//ss << "}";
		//std::string result = ss.str();

		Ref<Mesh> cubeMesh = create <Mesh>(
			sizeof(cubeVerticies) +
			sizeof(cubeNormals) +
			sizeof(cubeUV)
			);

		ArraySpan<float> vertSpan = cubeMesh->GetVertexArray(sizeof(cubeVerticies) / sizeof(float));
		ArraySpan<float> normalSpan = cubeMesh->GetNormalsArray(sizeof(cubeNormals) / sizeof(float));
		ArraySpan<float> texCoordSpan = cubeMesh->GetTexCoordsArray(sizeof(cubeUV) / sizeof(float));

		memcpy(vertSpan.GetInternalArray(), &cubeVerticies, sizeof(cubeVerticies));
		memcpy(normalSpan.GetInternalArray(), &cubeNormals, sizeof(cubeNormals));
		memcpy(texCoordSpan.GetInternalArray(), &cubeUV, sizeof(cubeUV));

		for (float& v : vertSpan)
		{
			v *= size;
		}

		return cubeMesh;
	}
	//
	Ref<Mesh> Geometry::CreateCircle(float radius, unsigned int segments)
	{
		Ref<Mesh> circleMesh = create<Mesh>((size_t)segments * 2 * sizeof(float));
		ArraySpan<float>& Verticies = circleMesh->GetVertexArray((size_t)segments * 2);

		float theta = 0.0f;
		for (unsigned int i = 0; i < segments; i++)
		{
			theta = 2.0f * SV::Maths::PI * float(i) / float(segments);

			Verticies[(size_t)i * 2 + 0] = radius * cos(theta);
			Verticies[(size_t)i * 2 + 1] = radius * sin(theta);
		}

		return circleMesh;
	}
	//
	Ref<Mesh> Geometry::CreateRings(float innerRadius, float outerRadius, unsigned int segments)
	{
		unsigned int& faceCount = segments;
		size_t vertexCount = (size_t)faceCount * 4;
		size_t indexCount = (size_t)segments * 6;

		Ref<Mesh> ringMesh = create <Mesh>(vertexCount * sizeof(float) + indexCount * sizeof(unsigned int));

		ArraySpan<float>& Verticies = ringMesh->GetVertexArray(vertexCount);
		ArraySpan<unsigned int>& Indicies = ringMesh->GetIndiciesArray(indexCount);

		float theta;
		unsigned int i = 0;
		for (i = 0; i < faceCount; i++)
		{
			theta = 2.0f * SV::Maths::PI * float(i) / float(segments);

			Verticies[(size_t)i * 4 + 0] = innerRadius * cos(theta);
			Verticies[(size_t)i * 4 + 1] = innerRadius * sin(theta);

			Verticies[(size_t)i * 4 + 2] = outerRadius * cos(theta);
			Verticies[(size_t)i * 4 + 3] = outerRadius * sin(theta);
		}

		for (i = 0; i < segments; i++)
		{
			if (i + 1 == segments)
			{
				Indicies[(size_t)i * 6 + 0] = i * 2 + 0;
				Indicies[(size_t)i * 6 + 1] = 1;
				Indicies[(size_t)i * 6 + 2] = i * 2 + 1;

				Indicies[(size_t)i * 6 + 3] = i * 2 + 0;
				Indicies[(size_t)i * 6 + 4] = 0;
				Indicies[(size_t)i * 6 + 5] = 1;
			}
			else
			{
				Indicies[(size_t)i * 6 + 0] = i * 2 + 0;
				Indicies[(size_t)i * 6 + 1] = i * 2 + 3;
				Indicies[(size_t)i * 6 + 2] = i * 2 + 1;

				Indicies[(size_t)i * 6 + 3] = i * 2 + 0;
				Indicies[(size_t)i * 6 + 4] = i * 2 + 2;
				Indicies[(size_t)i * 6 + 5] = i * 2 + 3;
			}
		}

		return ringMesh;
	}
	//
	Ref<Mesh> Geometry::CreateHalfRings(float innerRadius, float outerRadius, unsigned int segments)
	{
		unsigned int faceCount = segments / 2 + 1;
		size_t vertexCount = (size_t)faceCount * 4;
		size_t indexCount = (size_t)(faceCount - 1) * 6;

		Ref<Mesh> ringMesh = create <Mesh>(vertexCount * sizeof(float) + indexCount * sizeof(unsigned int));

		ArraySpan<float> Verticies = ringMesh->GetVertexArray(vertexCount);
		ArraySpan<unsigned int> Indicies = ringMesh->GetIndiciesArray(indexCount);

		float theta;
		unsigned int i = 0;
		for (i = 0; i < faceCount; i++)
		{
			theta = 2.0f * SV::Maths::PI * float(i) / float(segments);

			Verticies[(size_t)i * 4 + 0] = innerRadius * cos(theta);
			Verticies[(size_t)i * 4 + 1] = innerRadius * sin(theta);

			Verticies[(size_t)i * 4 + 2] = outerRadius * cos(theta);
			Verticies[(size_t)i * 4 + 3] = outerRadius * sin(theta);
		}

		for (i = 0; i < faceCount - 1; i++)
		{
			Indicies[(size_t)i * 6 + 0] = i * 2 + 0;
			Indicies[(size_t)i * 6 + 1] = i * 2 + 3;
			Indicies[(size_t)i * 6 + 2] = i * 2 + 1;

			Indicies[(size_t)i * 6 + 3] = i * 2 + 0;
			Indicies[(size_t)i * 6 + 4] = i * 2 + 2;
			Indicies[(size_t)i * 6 + 5] = i * 2 + 3;
		}

		return ringMesh;
	}
	//
	Ref<Mesh> Geometry::CreateTorus(float innerRadius, float outerRadius, float depth, unsigned int sides, unsigned int rings, unsigned short twistyness)
	{
		//Array for final verticies and normal vectors
		unsigned int elementCount = (sides + 1) * ((rings + 1) * (twistyness + 1));
		size_t vertexCount = ((size_t)elementCount * 3);
		size_t texCoordsCount = ((size_t)elementCount * 2);
		size_t indiciesCount = ((size_t)elementCount * 3) - 6;

		Ref<Mesh> torusMesh = create <Mesh>(
			vertexCount * sizeof(float) * 2 + //For verticies and normals
			texCoordsCount * sizeof(float) +
			indiciesCount * sizeof(unsigned int)
			);

		ArraySpan<float>& Vertices = torusMesh->GetVertexArray(vertexCount);
		ArraySpan<float>& Normals = torusMesh->GetNormalsArray(vertexCount);
		ArraySpan<float>& TextureCoordinates = torusMesh->GetTexCoordsArray(texCoordsCount);
		ArraySpan<unsigned int>& Indicies = torusMesh->GetIndiciesArray(indiciesCount);

		//Incremental varibles
		unsigned int i = 0, j = 0;
		unsigned int a = 0;

		//Tesselation "twistyness". Varible used for correct quad tesselation
		short k = twistyness;
		//Angles for torus calculation
		float theta = 0, phi = 0;

		for (i = 0; i <= sides; i++)
		{
			for (j = 0; j <= rings; j++)
			{
				for (k = twistyness; k >= 0; k--)
				{
					//	theta = (i + k) % rings+0.5f;
					//phi = j % rings;
					theta = (float)fmod(i + k, float(rings) + 0.5f);
					phi = (float)fmod(j, rings);

					Vertices[(size_t)a * 3 + 0] = (outerRadius + innerRadius * cos(theta * SV::Maths::Tau / rings)) * cos(phi * SV::Maths::Tau / rings);
					Vertices[(size_t)a * 3 + 1] = (outerRadius + innerRadius * cos(theta * SV::Maths::Tau / rings)) * sin(phi * SV::Maths::Tau / rings);
					Vertices[(size_t)a * 3 + 2] = depth * sin(theta * SV::Maths::Tau / rings);

					Normals[(size_t)a * 3 + 0] = cos(theta * SV::Maths::Tau / rings) * cos(phi * SV::Maths::Tau / rings);
					Normals[(size_t)a * 3 + 1] = cos(theta * SV::Maths::Tau / rings) * sin(phi * SV::Maths::Tau / rings);
					Normals[(size_t)a * 3 + 2] = sin(theta * SV::Maths::Tau / rings);

					//	TextureCoordinates.SetObjectValue(acos(currentVertex.y / innerRadius) / (2*phi), a * 2);
					//	TextureCoordinates.SetObjectValue(acos(currentVertex.x/(innerRadius + outerRadius*cos(theta*TwoPI / rings)))*(2*phi), a * 2 + 1);
					TextureCoordinates[(size_t)a * 2 + 0] = (i + k) / (float)sides;
					TextureCoordinates[(size_t)a * 2 + 1] = phi / (float)rings;

					a++;
				}
			}
		}

		j = (unsigned int)(Indicies.GetCount() / 6);
		for (i = 0; i < j; i++)
		{
			Indicies[(size_t)i * 6 + 0] = i * 2;
			Indicies[(size_t)i * 6 + 1] = i * 2 + 1;
			Indicies[(size_t)i * 6 + 2] = i * 2 + 2;

			Indicies[(size_t)i * 6 + 3] = i * 2 + 2;
			Indicies[(size_t)i * 6 + 4] = i * 2 + 1;
			Indicies[(size_t)i * 6 + 5] = i * 2 + 3;
		}

		return torusMesh;
	}
	//
	SV::Array<unsigned int> Geometry::DetectSphereWrappedUVCoords(const ArrayView<TriangleI> triangles, const ArrayView<glm::vec2> texCoords, unsigned int hintCount)
	{
		SV::Array<unsigned int> returnIndicies(hintCount > 0 ? hintCount : DEFAULT_ARRAY_CAPACITY, false);

		for (unsigned int i = 0; i < triangles.GetCount(); ++i)
		{
			const TriangleI& currentTri = triangles[i];
			glm::vec3 texA(texCoords[currentTri[0]], 0.0f);
			glm::vec3 texB(texCoords[currentTri[1]], 0.0f);
			glm::vec3 texC(texCoords[currentTri[2]], 0.0f);

			glm::vec3 normal = glm::cross(texB - texA, texC - texA);
			if (normal.z > 0.0f)
			{
				returnIndicies.Add(i);
			}
		}

		returnIndicies.Trim();

		return move(returnIndicies);
	}
	//
	void Geometry::FixSphereWrappedUV(const SV::Array<unsigned int>& wrapped, SV::Array<glm::vec3>& verticies, SV::Array<TriangleI>& triangles, SV::Array<glm::vec2>& texCoords, unsigned int hintCount)
	{
		unsigned int verticieIndex = verticies.GetCount() - 1;
		std::unordered_map<unsigned int, unsigned int> visited;

		if (hintCount > 0)
		{
			verticies.Reserve(hintCount);
			texCoords.Reserve(hintCount);
		}

		for (const unsigned int i : wrapped)
		{
			unsigned int a = triangles[i][0];
			unsigned int b = triangles[i][1];
			unsigned int c = triangles[i][2];

			if (texCoords[a].x < 0.25f)
			{
				unsigned int tempA = a;
				if (visited.find(a) == visited.end())
				{
					glm::vec3 dVertex = verticies[a];
					verticies.Add(dVertex);
					glm::vec2 dTexCoord = texCoords[a];
					texCoords.Emplace(dTexCoord.x + 1.0f, dTexCoord.y);
					verticieIndex++;
					visited.insert({ a, verticieIndex });
					tempA = verticieIndex;
				}
				else
				{
					tempA = visited[a];
				}
				a = tempA;
			}

			if (texCoords[b].x < 0.25f)
			{
				unsigned int tempB = b;
				if (visited.find(b) == visited.end())
				{
					glm::vec3 dVertex = verticies[b];
					verticies.Add(dVertex);
					glm::vec2 dTexCoord = texCoords[b];
					texCoords.Emplace(dTexCoord.x + 1.0f, dTexCoord.y);
					verticieIndex++;
					visited.insert({ b, verticieIndex });
					tempB = verticieIndex;
				}
				else
				{
					tempB = visited[b];
				}
				b = tempB;
			}

			if (texCoords[c].x < 0.25f)
			{
				unsigned int tempC = c;
				if (visited.find(b) == visited.end())
				{
					glm::vec3 dVertex = verticies[c];
					verticies.Add(dVertex);
					glm::vec2 dTexCoord = texCoords[c];
					texCoords.Emplace(dTexCoord.x + 1.0f, dTexCoord.y);
					verticieIndex++;
					visited.insert({ c, verticieIndex });
					tempC = verticieIndex;
				}
				else
				{
					tempC = visited[c];
				}
				c = tempC;
			}

			triangles[i].x = a;
			triangles[i].y = b;
			triangles[i].z = c;
		}

		verticies.Trim();
		triangles.Trim();
		texCoords.Trim();
	}
	//
	void Geometry::FixPoleVerticies(SV::Array<glm::vec3>& verticies, SV::Array<glm::vec2>& texCoords, SV::Array<TriangleI>& triangles)
	{
		unsigned int northPole = 0, southPole = 0;
		unsigned int i = 0;

		for (; i < verticies.GetCount(); i++)
		{
			if (verticies[i].y >= 0.9995f) northPole = i;
			else if (verticies[i].y <= -0.9995f) southPole = i;
		}

		unsigned int verticeIndex = (unsigned int)verticies.GetCount() - 1;

		auto FixVertex = [&triangles, &texCoords, &verticies, &verticeIndex](unsigned int i, unsigned int poleIndex, unsigned int b, unsigned int c) -> void
		{
			const TriangleI& currentTri = triangles[i];
			const glm::vec2& B = texCoords[currentTri[b]];
			const glm::vec2& C = texCoords[currentTri[c]];
			verticies.Add(verticies[currentTri[poleIndex]]);
			//texCoords.Emplace((B.x + C.x) * 0.5f, texCoords[currentTri[poleIndex]].y);
			texCoords.Emplace(1, 1);
			verticeIndex++;
			triangles[i][poleIndex] = verticeIndex;
		};

		for (i = 0; i < triangles.GetCount(); i++)
		{
			if (triangles[i].x == northPole)
			{
				FixVertex(i, 0, 1, 2);
			}
			else if (triangles[i].y == northPole)
			{
				FixVertex(i, 1, 0, 2);
			}
			else if (triangles[i].z == northPole)
			{
				FixVertex(i, 2, 0, 1);
			}

			if (triangles[i].x == southPole)
			{
				FixVertex(i, 0, 1, 2);
			}
			else if (triangles[i].y == southPole)
			{
				FixVertex(i, 1, 0, 2);
			}
			else if (triangles[i].z == southPole)
			{
				FixVertex(i, 2, 0, 1);
			}
		}

	}
	/*
	private void FixSharedPoleVertices(Mesh sphereMesh)
{
	Vertex north = sphereMesh.Vertices.First((v) => v.Position.Y == 1);
	Vertex south = sphereMesh.Vertices.First((v) => v.Position.Y == -1);
	uint northIndex = (uint)Array.IndexOf(sphereMesh.Vertices, north);
	uint southIndex = (uint)Array.IndexOf(sphereMesh.Vertices, south);
	List<Vertex> vertices = new List<Vertex>(sphereMesh.Vertices);
	uint verticeIndex = (uint)sphereMesh.Vertices.Length - 1;
	for (int i = 0; i < sphereMesh.Triangles.Length; ++i)
	{
		if (sphereMesh.Triangles[i].A == northIndex)
		{
			Vertex A = sphereMesh.Vertices[sphereMesh.Triangles[i].A];
			Vertex B = sphereMesh.Vertices[sphereMesh.Triangles[i].B];
			Vertex C = sphereMesh.Vertices[sphereMesh.Triangles[i].C];
			Vertex newNorth = north;
			newNorth.TexCoord.X = (B.TexCoord.X + C.TexCoord.X) / 2;
			verticeIndex++;
			vertices.Add(newNorth);
			sphereMesh.Triangles[i].A = verticeIndex;
		}
		else if (sphereMesh.Triangles[i].A == southIndex)
		{
			Vertex A = sphereMesh.Vertices[sphereMesh.Triangles[i].A];
			Vertex B = sphereMesh.Vertices[sphereMesh.Triangles[i].B];
			Vertex C = sphereMesh.Vertices[sphereMesh.Triangles[i].C];
			Vertex newSouth = south;
			newSouth.TexCoord.X = (B.TexCoord.X + C.TexCoord.X) / 2;
			verticeIndex++;
			vertices.Add(newSouth);
			sphereMesh.Triangles[i].A = verticeIndex;
		}
	}
	sphereMesh.Vertices = vertices.ToArray();
}
	*/
	//
#pragma endregion
//
}