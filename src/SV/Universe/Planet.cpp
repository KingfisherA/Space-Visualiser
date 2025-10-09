#include"SV-Engine/Precompiled Global.hpp"
#include"SV/Universe/Planet.hpp"
#include"SV-Engine/OpenGL/VAO.hpp"
#include"SV-Engine/OpenGL/Texture.hpp"
#include"SV-Engine/Utilities/Geometry.hpp"
#include"SV-Engine/Scene/Camera.hpp"
#include"SV-Engine/Scene/Shadows.hpp"
#include"SV-Engine/Utilities/Array.hpp"
#include<algorithm>

namespace SV
{
	//
	//
#pragma region Planet Shader Interface
//
//const float ringDensity = 20.0f; 
//const float ringsWidth = 0.7f; = ringsMinorRadius * 0.7f;
//pRingsProgram->BindRingParameters(vec4(1.0f / ringsWidth, ringsWidth, 1.0f / ringDensity, 0.0f)); 
	const glm::vec4 IPlanetShader::DefaultRingParams(1.4289f, 0.7f, 1.0f / 0.8f, 0.0f);
	//
	IPlanetShader::IPlanetShader(const VertexShader& vShader, const FragmentShader& fShader) : ShaderProgram(vShader, fShader, AttributeList{ {0, "position"}, {1, "normal"}, {2, "textureCoords"} })
	{
		viewMatUni = GetUniformLocation("viewMatrix");
		projMatUni = GetUniformLocation("projectionMatrix");
		transformMatUni = GetUniformLocation("transformMatrix");
		normalMatUni = GetUniformLocation("normalMatrix");
		//
		lightPosUni = GetUniformLocation("lightPosition");
		lightColUni = GetUniformLocation("lightColour");
		//
		surfaceMapUni = GetUniformLocation("surfaceMap");
		//
		ringsParamUni = GetUniformLocation("ringsParams");
		ringsMapUni = GetUniformLocation("ringsMap");
		//
		this->Bind();
		this->BindRingParameters(glm::vec4(0.0f));
		this->Unbind();
	}
	//
#pragma endregion
//
#pragma region Terrestrial Planet Shader
//
	const StringView PLANET_VERTEX_SHADER = "./Shaders/Universe/Planet/PlanetShader.v.glsl";
	const StringView PLANET_FRAGMENT_SHADER = "./Shaders/Universe/Planet/PlanetShader.f.glsl";
	TerrestrialShader::TerrestrialShader() : IPlanetShader(VertexShader(PLANET_VERTEX_SHADER), FragmentShader(PLANET_FRAGMENT_SHADER))
	{
		invViewMatUni = GetUniformLocation("inverseViewMatrix");
		reflectivityUni = GetUniformLocation("reflectivity");
		damperUni = GetUniformLocation("shineDamper");
		//
		timeUni = GetUniformLocation("time");
		//
		cloudsMapUni = GetUniformLocation("cloudsMap");
		specularMapUni = GetUniformLocation("specularMap");
		nightMapUni = GetUniformLocation("nightMap");
	}
	//
#pragma endregion
//
#pragma region Planet Rings Shader
//
//vec4(inverse ring width, ring inner radius, inverse ring density, ring face)
//
	const StringView PLANET_RINGS_VETEX_SHADER = "./Shaders/Universe/Planet/Planet Rings.v.glsl";
	const StringView PLANET_RINGS_FRAGMENT_SHADER = "./Shaders/Universe/Planet/Planet Rings.f.glsl";
	PlanetRingsShader::PlanetRingsShader() : IPlanetShader(VertexShader(PLANET_RINGS_VETEX_SHADER), FragmentShader(PLANET_RINGS_FRAGMENT_SHADER))
	{
	}
	//
#pragma endregion
//
#pragma region GasPlanetShader
//
	const StringView GAS_SHADER_VERTEX = "";
	const StringView GAS_SHADER_FRAGMENT= "";
	//
	GasPlanetShader::GasPlanetShader() : IPlanetShader(VertexShader(GAS_SHADER_VERTEX), FragmentShader(GAS_SHADER_FRAGMENT))
	{
	}
	//
#pragma endregion
//
#pragma region Atmosphere Shader
//
	const AtmosphericData AtmosphericData::DefaultValues(0.0030f, 0.0015f, \
		16.0f, -0.75f, 1.0f / powf(0.650f, 4.0f), 1.0f / powf(0.570f, 4.0f), \
		1.0f / powf(0.475f, 4.0f), 3);
	const AtmosphericData AtmosphericData::DefaultValuesCalculated(0.0030f, 0.0015f, \
		16.0f, -0.75f, 5.6020447463324113f, 9.4732844379230354f, \
		19.6438026104772063f, 3);
	const float AtmosphericData::DefaultAdditionFactor = 0.065f;
	//
#define ATMO_PER_FRAG
#ifdef ATMO_PER_FRAG
	const StringView ATMO_VERTEX_SHADER = "./Shaders/Universe/Planet/Atmosphere Shaders/AtmosphereShaderPerFragment.v.glsl";
	const StringView ATMO_FRAGMENT_SHADER = "./Shaders/Universe/Planet/Atmosphere Shaders/AtmosphereShaderPerFragment.f.glsl";
#else
	const SV::StringView ATMO_VERTEX_SHADER = "./Shaders/Universe/Planet/Atmosphere Shaders/AtmosphereShaderPerVertex.v.glsl";
	const SV::StringView ATMO_FRAGMENT_SHADER = "./Shaders/Universe/Planet/Atmosphere Shaders/AtmosphereShaderPerVertex.f.glsl";
#endif // ATMO_PER_FRAG
	AtmosphereShader::AtmosphereShader() : ShaderProgram(VertexShader(ATMO_VERTEX_SHADER), FragmentShader(ATMO_FRAGMENT_SHADER), AttributeList{ {0, "vertex"} })
	{
		transformMatUni = GetUniformLocation("transformationMatrix");
		viewMatUni = GetUniformLocation("viewMatrix");
		projMatUni = GetUniformLocation("projectionMatrix");
		//
		camPosUni = GetUniformLocation("v3CameraPos");
		lightPosUni = GetUniformLocation("v3LightPos");
		translationUni = GetUniformLocation("translation");
		//
		invWavelengthUni = GetUniformLocation("v3InvWavelength");
		krESunUni = GetUniformLocation("fKrESun");
		kmESunUni = GetUniformLocation("fKmESun");
		kr4PiUni = GetUniformLocation("fKr4PI");
		km4PiUni = GetUniformLocation("fKm4PI");
		gUni = GetUniformLocation("g");
		g2Uni = GetUniformLocation("g2");
		samplesUni = GetUniformLocation("Samples");
		//
		outerRadUni = GetUniformLocation("fOuterRadius");
		outerRadUni2 = GetUniformLocation("fOuterRadius2");
		innerRadUni = GetUniformLocation("fInnerRadius");
		scaleUni = GetUniformLocation("fScale");
		scaleDepthUni = GetUniformLocation("fScaleDepth");
		scaleOverSDUni = GetUniformLocation("fScaleOverScaleDepth");
	}
	//
	void AtmosphereShader::BindAtmosphericData(const AtmosphericData& data)
	{
		UploadFloat(invWavelengthUni, glm::vec3(data.WavelengthR, data.WavelengthG, data.WavelengthB));
		UploadFloat(krESunUni, data.Kr * data.ESun);
		UploadFloat(kmESunUni, data.Km * data.ESun);
		UploadFloat(kr4PiUni, data.Kr * 4.0f * SV::Maths::PI);
		UploadFloat(km4PiUni, data.Km * 4.0f * SV::Maths::PI);
		UploadFloat(gUni, data.g);
		UploadFloat(g2Uni, data.g * data.g);
		UploadInt(samplesUni, (int)data.Samples);
	}
	//
	void AtmosphereShader::BindRadii(float innerRadius, float outerRadius, float scaleDepth)
	{
		UploadFloat(innerRadUni, innerRadius);
		UploadFloat(outerRadUni, outerRadius);
		UploadFloat(outerRadUni2, outerRadius * outerRadius);

		float scaleV = 1.0f / (outerRadius - innerRadius);
		UploadFloat(scaleUni, scaleV);
		UploadFloat(scaleOverSDUni, scaleV / scaleDepth);
		UploadFloat(scaleDepthUni, scaleDepth);
	}
	//
#pragma endregion
//
#pragma region Planet Renderer
//
	const glm::mat4 PlanetRenderer::RINGS_ROTATION = glm::rotate(glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
	const float additionFactor = 0.015f;
	//
	PlanetRenderer::PlanetRenderer()
	{
		auto buffers = ArrayBuffer::GenerateBuffers(6);
		auto indexBuffers = IndexBuffer::GenerateBuffers(2);

		Ref<Mesh> sMesh = Geometry::CreateUVSphere(1.0f, 50, 50);
		sphereVAO = create <VAO>();
		sphereVAO->Bind();

		buffers[0]->StoreData(sMesh->GetVertexArray());
		buffers[1]->StoreData(sMesh->GetNormalsArray());
		buffers[2]->StoreData(sMesh->GetTexCoordsArray());
		indexBuffers[0]->StoreData(sMesh->GetIndiciesArray());

		sphereVAO->AddAttributeArray(0, buffers[0]);
		sphereVAO->AddAttributeArray(1, buffers[1]);
		sphereVAO->AddAttributeArray(2, buffers[2], 2);
		sphereVAO->AddIndexBuffer(indexBuffers[0]);

		torusVAO = create <VAO>();
		Ref<Mesh> tMesh = Geometry::CreateTorus(0.4f, 2.0f, 0.01f, 50, 50);
		torusVAO->Bind();

		buffers[3]->StoreData(tMesh->GetVertexArray());
		buffers[4]->StoreData(tMesh->GetNormalsArray());
		buffers[5]->StoreData(tMesh->GetTexCoordsArray());
		indexBuffers[1]->StoreData(tMesh->GetIndiciesArray());

		torusVAO->AddAttributeArray(0, buffers[3]);
		torusVAO->AddAttributeArray(1, buffers[4]);
		torusVAO->AddAttributeArray(2, buffers[5], 2);
		torusVAO->AddIndexBuffer(indexBuffers[1]);
		torusVAO->Unbind();

		pRingsShader = create <PlanetRingsShader>();
		pRingsShader->Bind();
		pRingsShader->BindSurfaceMapUnit(0);
		pRingsShader->BindRingsMap(1);
		pRingsShader->BindLightCol(glm::vec3(1.0f));
		pRingsShader->Unbind();

		terrestrialShader = create <TerrestrialShader>();
		terrestrialShader->Bind();
		terrestrialShader->BindReflectivity(0.5f);
		terrestrialShader->BindShineDamper(5.0f);
		//
		terrestrialShader->BindSurfaceMapUnit(0);
		terrestrialShader->BindCloudsMapUnit(1);
		terrestrialShader->BindSpecularMapUnit(2);
		terrestrialShader->BindNightMapUnit(3);
		terrestrialShader->BindRingsMap(4);
		//
		terrestrialShader->BindLightCol(glm::vec3(1.0f));
		terrestrialShader->Unbind();

		atmoShader = create <AtmosphereShader>();
		//	atmosphereShader->BindAtmosphericData(data);
		//atmosphereShader->BindCamPos(vec3(0.0, 0.0f, 0.0f));
		//atmosphereShader->BindLightPos(vec3(0.0f, 0.0f, 0.0f));
		//atmosphereShader->BindRadii(1.0f, earthAtmoRad);
		//atmosphereShader->BindTranslation(vec3(0.0f, 0.0f, 0.0f));

		ringsShader = create <SphereShadowShader>();
		ringsShader->Bind();
		ringsShader->BindSpherePos(glm::vec3(0.0f));
		ringsShader->BindSphereRad(1.0f);
		ringsShader->BindShadowSofteness(3.5f);
		ringsShader->BindDiffuseMapUnit(1);
		ringsShader->Unbind();
	}
	//
	//PlanetRenderer::~PlanetRenderer()
	//{
	//	_deletegl pRingsShader;
	//	_deletegl terrestrialShader;
	//	_deletegl atmoShader;
	//
	//	_deletegl ringsShader;
	//	delete sphereVAO;
	//	delete torusVAO;
	//
	//	for (TerrestrialPlanet*& p : terrestrialPlanets)
	//	{
	//		delete p;
	//	}
	//
	//	for (GasPlanet*& p : gasPlanets)
	//	{
	//		delete p;
	//	}
	//}
	//
	void PlanetRenderer::AddPlanet(Ref<IPlanet> p)
	{
		if (p == nullptr) return;

		if (dynamic_cast<TerrestrialPlanet*>(p.operator->()) != nullptr)
		{
			terrestrialPlanets.push_back(std::static_pointer_cast<TerrestrialPlanet>(p));
		}
		else if (dynamic_cast<GasPlanet*>(p.operator->()) != nullptr)
		{
			gasPlanets.push_back(std::static_pointer_cast<GasPlanet>(p));
		}
	}
	//
	void PlanetRenderer::RemovePlanet(Ref<IPlanet> p)
	{
		if (dynamic_cast<TerrestrialPlanet*>(p.operator->()) != nullptr)
		{
			terrestrialPlanets.erase(std::remove(terrestrialPlanets.begin(), terrestrialPlanets.end(), std::static_pointer_cast<TerrestrialPlanet>(p)));
		}
		else if (dynamic_cast<GasPlanet*>(p.operator->()) != nullptr)
		{
			gasPlanets.erase(std::remove(gasPlanets.begin(), gasPlanets.end(), std::static_pointer_cast<GasPlanet>(p)));
		}
	}
	//
	void PlanetRenderer::RenderPlanets(const glm::vec3& lightPosition, const Camera& cam, const glm::mat4& projectionMatrix, float currentTime) const
	{
		torusVAO->Bind();
		torusVAO->EnableAttributeArray(0);
		torusVAO->EnableAttributeArray(1);
		torusVAO->EnableAttributeArray(2);

		RenderPlanetRings(lightPosition, cam, projectionMatrix, currentTime);

		torusVAO->DisableAttributeArray(0);
		torusVAO->DisableAttributeArray(1);
		torusVAO->DisableAttributeArray(2);
		torusVAO->Unbind();

		sphereVAO->Bind();
		sphereVAO->EnableAttributeArray(0);
		sphereVAO->EnableAttributeArray(1);
		sphereVAO->EnableAttributeArray(2);

		RenderGasPlanets(lightPosition, cam, projectionMatrix, currentTime);
		RenderTerrestrialPlanets(lightPosition, cam, projectionMatrix, currentTime);
		RenderAtmospheres(lightPosition, cam, projectionMatrix);

		sphereVAO->DisableAttributeArray(0);
		sphereVAO->DisableAttributeArray(1);
		sphereVAO->DisableAttributeArray(2);
		sphereVAO->Unbind();
	}
	//
	void PlanetRenderer::RenderTerrestrialPlanets(const glm::vec3& lightPosition, const Camera& cam, const glm::mat4& projectionMatrix, float currentTime) const
	{
		if (terrestrialPlanets.size() == 0) return;

		Ref<SV::TerrestrialPlanet> currentPlanet;

		terrestrialShader->Bind();
		terrestrialShader->BindViewMat(cam.GetViewMatrix());
		terrestrialShader->BindProjectionMat(projectionMatrix);
		terrestrialShader->BindLightPos(lightPosition);
		terrestrialShader->BindTime(currentTime);

		for (unsigned int i = 0; i < terrestrialPlanets.size(); i++)
		{
			currentPlanet = terrestrialPlanets[i];

			if (currentPlanet->diffuseMapTexture != nullptr) currentPlanet->diffuseMapTexture->Bind(0);
			if (currentPlanet->cloudsMap != nullptr) currentPlanet->cloudsMap->Bind(1);
			if (currentPlanet->specularMap != nullptr) currentPlanet->specularMap->Bind(2);
			if (currentPlanet->nightMap != nullptr) currentPlanet->nightMap->Bind(3);
			if (currentPlanet->ringTexture != nullptr) currentPlanet->ringTexture->Bind(4); terrestrialShader->BindRingParameters(currentPlanet->ringParameters);

			currentPlanet->RecalculateTransformation();

			terrestrialShader->BindTransformationMat(currentPlanet->transformationMatrix);
			terrestrialShader->BindNormalMat(Maths::CalculateNormalMatrix(currentPlanet->transformationMatrix));

			glDrawElements(GL_TRIANGLES, sphereVAO->GetEleCount(), GL_UNSIGNED_INT, nullptr);
		}

		terrestrialShader->Unbind();

	}
	//
	void PlanetRenderer::RenderGasPlanets(const glm::vec3& lightPosition, const Camera& cam, const glm::mat4& projectionMatrix, float currentTime) const
	{
		if (gasPlanets.size() == 0) return;

		Ref<GasPlanet> currentPlanet;

		pRingsShader->Bind();
		pRingsShader->BindViewMat(cam.GetViewMatrix());
		pRingsShader->BindProjectionMat(projectionMatrix);
		pRingsShader->BindLightPos(lightPosition);

		for (unsigned int i = 0; i < gasPlanets.size(); i++)
		{
			currentPlanet = gasPlanets[i];

			currentPlanet->diffuseMapTexture->Bind(0);
			if (currentPlanet->ringTexture != nullptr) currentPlanet->ringTexture->Bind(1); pRingsShader->BindRingParameters(currentPlanet->ringParameters);

			currentPlanet->RecalculateTransformation();

			pRingsShader->BindTransformationMat(currentPlanet->transformationMatrix);
			pRingsShader->BindNormalMat(Maths::CalculateNormalMatrix(currentPlanet->transformationMatrix));

			glDrawElements(GL_TRIANGLES, sphereVAO->GetEleCount(), GL_UNSIGNED_INT, nullptr);
		}

		pRingsShader->Unbind();
	}
	//
	void PlanetRenderer::RenderAtmospheres(const glm::vec3& lightPosition, const Camera& cam, const glm::mat4& projectionMatrix) const
	{
		unsigned int i = 0;

		glCullFace(GL_FRONT);
		OpenGLContext::EnableAdditiveBlending();

		atmoShader->Bind();
		atmoShader->BindLightPos(glm::normalize(lightPosition));
		atmoShader->BindCamPos(cam.GetActualPosition());

		auto RenderAtmoFunc = [&](Ref<IPlanet> currentPlanet) -> void {
			if (!currentPlanet->hasAtmosphere) return;

			atmoShader->BindAtmosphericData(currentPlanet->atmosphereData);
			float radius = currentPlanet->GetScale().x;
			atmoShader->BindRadii(radius, radius + additionFactor);
			atmoShader->BindTranslation(currentPlanet->GetTranslation());

			atmoShader->BindMatricies(cam.GetViewMatrix(), projectionMatrix, currentPlanet->transformationMatrix * glm::scale(glm::vec3(radius + additionFactor)));

			glDrawElements(GL_TRIANGLES, sphereVAO->GetEleCount(), GL_UNSIGNED_INT, nullptr);
		};

		for (i = 0; i < gasPlanets.size(); i++)
		{
			RenderAtmoFunc(gasPlanets[i]);
		}

		for (i = 0; i < terrestrialPlanets.size(); i++)
		{
			RenderAtmoFunc(terrestrialPlanets[i]);
		}

		atmoShader->Unbind();

		glCullFace(GL_BACK);
		OpenGLContext::EnableAlphaBlending();
	}
	//
	void PlanetRenderer::RenderPlanetRings(const glm::vec3& lightPosition, const Camera& cam, const glm::mat4& projectionMatrix, float currentTime) const
	{
		unsigned int i = 0;

		OpenGLContext::EnableAdditiveBlending();

		ringsShader->Bind();
		ringsShader->BindViewMatrix(cam.GetViewMatrix());
		ringsShader->BindProjectionMatrix(projectionMatrix);
		ringsShader->BindLightPosition(lightPosition);

		auto RenderRingsFunc = [&](Ref<IPlanet> currentPlanet) -> void {
			if (currentPlanet->ringTexture == nullptr) return;

			currentPlanet->ringTexture->Bind(1);

			ringsShader->BindTransformMatrix(currentPlanet->transformationMatrix * RINGS_ROTATION);
			ringsShader->BindSphereRad(currentPlanet->GetScale().x);
			ringsShader->BindSpherePos(currentPlanet->GetTranslation());

			glDrawElements(GL_TRIANGLES, torusVAO->GetEleCount(), GL_UNSIGNED_INT, nullptr);
		};

		for (i = 0; i < gasPlanets.size(); i++)
		{
			RenderRingsFunc(gasPlanets[i]);
		}

		for (i = 0; i < terrestrialPlanets.size(); i++)
		{
			RenderRingsFunc(terrestrialPlanets[i]);
		}

		ringsShader->Unbind();

		OpenGLContext::EnableAlphaBlending();
	}
	//
#pragma endregion
	//
}