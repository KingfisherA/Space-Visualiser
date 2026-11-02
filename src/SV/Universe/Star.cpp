#include"SV-Engine/Precompiled Global.hpp"
#include"Star.hpp"
#include"SV-Engine/Utilities/GlobalItems.hpp"
#include"SV-Engine/Utilities/Geometry.hpp"
#include"SV-Engine/OpenGL/VAO.hpp"
#include"SV-Engine/Utilities/Array.hpp"
#include"SV-Engine/Scene/Camera.hpp"
#include"SV-Engine/Utilities/Array.hpp"
#include"SV-Engine/Utilities/SVMemory.hpp"
#include<algorithm>

namespace SV
{
	//
	//
#pragma region Star Shader Program
//
	const StringView STAR_VERTEX_SHADER = "./Shaders/Universe/Star/Star.v.glsl";
	const StringView STAR_FRAGMENT_SHADER = "./Shaders/Universe/Star/Star.f.glsl";
	//
	StarShader::StarShader() : ShaderProgram(VertexShader(STAR_VERTEX_SHADER), FragmentShader(STAR_FRAGMENT_SHADER))
	{
		viewMatLocation = GetUniformLocation("u_viewMatrix");
		projMatLocation = GetUniformLocation("u_projectionMatrix");
		//
		timeLocation = GetUniformLocation("u_time");
		cameraPositionLocation = GetUniformLocation("u_cameraPosition");
	}
	//
#pragma endregion
//
//
#pragma region Corona Shader Program
//
	const StringView CORONA_VERTEX_SHADER = "./Shaders/Universe/Star/Corona.v.glsl";
	const StringView CORONA_FRAGMENT_SHADER = "./Shaders/Universe/Star/Corona.f.glsl";
	//
	CoronaShader::CoronaShader() : ShaderProgram(VertexShader(CORONA_VERTEX_SHADER), FragmentShader(CORONA_FRAGMENT_SHADER), AttributeList{ {0, "vertex"} })
	{
		viewMatLocation = GetUniformLocation("u_viewMatrix");
		projMatLocation = GetUniformLocation("u_projectionMatrix");
		//
		timeLocation = GetUniformLocation("time");
		noiseOctavesLocation = GetUniformLocation("noiseOctaves");
	}
	//
#pragma endregion
//
//
#pragma region Star Renderer
	//
	StarRenderer::StarRenderer()
	{
		//
#pragma region Sphere and quad initilisation
		SV::Array<Ref<SV::VAO>> vaos = SV::VAO::GenerateArrays(2);
		starVAO = vaos[0];
		coronaVAO = vaos[1];
		vaos.Clear();

		SV::Array<Ref<SV::ArrayBuffer>> arrayBuffers = move(SV::ArrayBuffer::GenerateBuffers(3));
		SV::Array<Ref<SV::IndexBuffer>> indexBuffers = move(SV::IndexBuffer::GenerateBuffers(2));

		constexpr unsigned int SPHERE_SUBDIVISIONS = 20;
		Ref<Mesh> sphereMesh = Geometry::CreateQuadsphere(1.0f, SPHERE_SUBDIVISIONS);
		starVAO->Bind();

		arrayBuffers[0]->StoreData(sphereMesh->GetVertexArray());
		arrayBuffers[1]->StoreData(sphereMesh->GetNormalsArray());
		indexBuffers[0]->StoreData(sphereMesh->GetIndiciesArray());

		starVAO->AddAttributeArray(0, arrayBuffers[0]);
		starVAO->AddAttributeArray(1, arrayBuffers[1]);
		starVAO->AddIndexBuffer(indexBuffers[0]);

		resetref(sphereMesh);

		starAttributeBuffer = create <ArrayBuffer>(starAttributeSize * (unsigned int)sizeof(float) * 3U, GLType::Float, BufferUsage::DynamicDraw);

		//Tranformation Matrix
		starVAO->AddAttributeArray(2, starAttributeBuffer, AttributeLayout(4, starAttributeSize, 0, 1));
		starVAO->AddAttributeArray(3, starAttributeBuffer, AttributeLayout(4, starAttributeSize, 4, 1));
		starVAO->AddAttributeArray(4, starAttributeBuffer, AttributeLayout(4, starAttributeSize, 8, 1));
		starVAO->AddAttributeArray(5, starAttributeBuffer, AttributeLayout(4, starAttributeSize, 12, 1));

		//Normal matrix
		starVAO->AddAttributeArray(6, starAttributeBuffer, AttributeLayout(3, starAttributeSize, 16, 1));
		starVAO->AddAttributeArray(7, starAttributeBuffer, AttributeLayout(3, starAttributeSize, 19, 1));
		starVAO->AddAttributeArray(8, starAttributeBuffer, AttributeLayout(3, starAttributeSize, 22, 1));

		//Other Attributes
		starVAO->AddAttributeArray(9, starAttributeBuffer, AttributeLayout(3, starAttributeSize, 25, 1)); //Radius, Star Temperature, Noise Octaves
		starVAO->AddAttributeArray(10, starAttributeBuffer, AttributeLayout(3, starAttributeSize, 28, 1)); //Star Colour

		Ref<Mesh> quadMesh = Geometry::CreateQuad();
		coronaVAO->Bind();

		arrayBuffers[2]->StoreData(quadMesh->GetVertexArray());
		indexBuffers[1]->StoreData(quadMesh->GetIndiciesArray());

		coronaVAO->AddAttributeArray(0, arrayBuffers[2], 2);
		coronaVAO->AddIndexBuffer(indexBuffers[1]);

		coronaAttributeBuffer = create <ArrayBuffer>(coronaAttributeSize * (unsigned int)sizeof(float) * 3U, GLType::Float, BufferUsage::DynamicDraw);

		//Tranformation Matrix
		coronaVAO->AddAttributeArray(1, coronaAttributeBuffer, AttributeLayout(4, coronaAttributeSize, 0, 1));
		coronaVAO->AddAttributeArray(2, coronaAttributeBuffer, AttributeLayout(4, coronaAttributeSize, 4, 1));
		coronaVAO->AddAttributeArray(3, coronaAttributeBuffer, AttributeLayout(4, coronaAttributeSize, 8, 1));
		coronaVAO->AddAttributeArray(4, coronaAttributeBuffer, AttributeLayout(4, coronaAttributeSize, 12, 1));

		//Star Colours
		coronaVAO->AddAttributeArray(5, coronaAttributeBuffer, AttributeLayout(3, coronaAttributeSize, 16, 1));

		VAO::Unbind();
#pragma endregion
		//
#pragma region Shader program initilisation
		starProgram = create <StarShader>();
		coronaProgram = create <CoronaShader>();
#pragma endregion
		//
		PopulateDistanceCache(glm::vec3(0.0f));
	}
	//
	void StarRenderer::PackStarAttributes(SV::Array<float>& data)
	{
		size_t count = data.GetCount() / starAttributeSize;
		count = count > stars.GetCount() ? stars.GetCount() : count;

		size_t i = 0;

		for (const auto& keyPair : distanceCache)
		{
			Star& currentStar = *stars[keyPair.second];

			currentStar.RecalculateTransformation();
			currentStar.transformationMatrix = currentStar.transformationMatrix * glm::scale(glm::vec3(0.75f));

			memcpy(&data[i * starAttributeSize], (void*)glm::value_ptr(currentStar.transformationMatrix), sizeof(glm::mat4));

			memcpy(&data[(i * starAttributeSize) + 16], (void*)glm::value_ptr(Maths::CalculateNormalMatrix((glm::mat4&)currentStar.transformationMatrix)), sizeof(glm::mat3));

			data[(i * starAttributeSize) + 25] = currentStar.radius;
			data[(i * starAttributeSize) + 26] = currentStar.starTemperature;
			data[(i * starAttributeSize) + 27] = 3.0f; //Noise octaves

			const glm::vec3& col = currentStar.starColour;
			data[(i * starAttributeSize) + 28] = col.r;
			data[(i * starAttributeSize) + 29] = col.g;
			data[(i * starAttributeSize) + 30] = col.b;

			i++;
		}
	}
	void StarRenderer::PackCoronaAttributes(SV::Array<float>& data, const Camera& currentCamera)
	{
		unsigned int count = (unsigned int)(data.GetCount()) / (unsigned int)coronaAttributeSize;
		count = count > (unsigned int)stars.GetCount() ? (unsigned int)stars.GetCount() : count;

		size_t i = 0;

		for (const auto& keyPair : distanceCache)
		{
			Star& currentStar = *stars[keyPair.second];
			//Corona is sized relative to the star's scale so it isn't hidden inside larger stars
			glm::mat4 transformationMatrix = glm::translate(currentStar.GetTranslation()) * glm::scale(1.8f * currentStar.GetScale());// *Maths::SphericalBillboard(currentCamera.GetActualPosition(), currentStar.GetTranslation());

			memcpy(&data[((size_t)i * coronaAttributeSize) + 0], glm::value_ptr(transformationMatrix), sizeof(glm::mat4));

			glm::vec3& col = currentStar.starColour;
			data[((size_t)i * coronaAttributeSize) + 16] = col.r;
			data[((size_t)i * coronaAttributeSize) + 17] = col.g;
			data[((size_t)i * coronaAttributeSize) + 18] = col.b;

			i++;
		}
	}
	//
	void StarRenderer::Render(float time, const Camera& currentCamera, const glm::mat4& projMatrix)
	{
		if (stars.IsEmpty()) return;

		//
#pragma region Corona Rendering
	//Render star coronas
		OpenGLContext::EnableAdditiveBlending();

		coronaProgram->Bind();
		coronaProgram->SetViewMatrix(currentCamera.GetViewMatrix());
		coronaProgram->SetProjectionMatrix(projMatrix);
		coronaProgram->SetTime(time);

		coronaVAO->Bind();
		coronaVAO->EnableAttributeArray(0, 5);

		unsigned int coronaEleCount = ((unsigned int)stars.GetCount() * coronaAttributeSize);

		if (starAttributeArray.GetCount() < coronaEleCount) starAttributeArray.Resize(stars.GetCount() * coronaAttributeSize);

		StarRenderer::PackCoronaAttributes(starAttributeArray, currentCamera);

		if (coronaAttributeBuffer->GetCount() < coronaEleCount)
		{
			coronaAttributeBuffer->StoreData(nullptr, coronaEleCount, BufferUsage::DynamicDraw);
		}
		coronaAttributeBuffer->UpdateData(ArrayView(starAttributeArray, 0, coronaEleCount));

		glDepthMask(false);
		glDrawElementsInstanced(GL_TRIANGLES, coronaVAO->GetEleCount(), GL_UNSIGNED_INT, nullptr, (GLsizei)stars.GetCount());
		glDepthMask(true);

		coronaVAO->DisableAttributeArray(0, 5);
		coronaVAO->Unbind();

		coronaProgram->Unbind();

		OpenGLContext::EnableAlphaBlending();
#pragma endregion
		//
#pragma region Star Rendering

		unsigned int starEleCount = (unsigned int)stars.GetCount() * starAttributeSize;
		starAttributeArray.Resize((size_t)starEleCount);

		//Render star objects
		starProgram->Bind();
		starProgram->SetViewMatrix(currentCamera.GetViewMatrix());
		starProgram->SetProjectionMatrix(projMatrix);
		starProgram->SetTime(time);
		starProgram->SetCamPosition(currentCamera.GetActualPosition());

		StarRenderer::PackStarAttributes(starAttributeArray);

		if (starAttributeBuffer->GetCount() < starEleCount)
		{
			starAttributeBuffer->StoreData(nullptr, (unsigned int)starEleCount, BufferUsage::DynamicDraw);
		}
		starAttributeBuffer->UpdateData(ArrayView(starAttributeArray, 0, starEleCount));

		starVAO->Bind();
		starVAO->EnableAttributeArray(0, 10);

		//glDrawElements(GL_TRIANGLES, starVAO->GetEleCount(), GL_UNSIGNED_INT, nullptr);

		glDrawElementsInstanced(GL_TRIANGLES, starVAO->GetEleCount(), GL_UNSIGNED_INT, nullptr, (GLsizei)stars.GetCount());

		starVAO->DisableAttributeArray(0, 10);

#pragma endregion
		//
	}
	//
#pragma endregion
//
//
}