#include"PlanetNew.hpp"

#include"SV/OpenGL/Texture.hpp"
#include"SV/Atmosphere/Atmosphere.hpp"
#include"SV/Utilities/Geometry.hpp"

#include"SV/Utilities/Array.hpp"

//
//
namespace SV
{

#pragma region Mercury Class
	//
	constexpr static const char MERCURY_DIFFUSE_MAP[] = "Solar System\\Mercury\\Mercury Diffuse.png";
	//
	Ref<SV::Texture2D> Mercury::mercuryDiffuseMap = nullptr;
	Ref<SV::Atmosphere::IAtmosphere> Mercury::mercuryAtmosphere = nullptr;
	//
	Mercury::Mercury(const Ref<SV::VAO>& quadVAO) : PlanetNew(3.285E23)
	{
		if (mercuryDiffuseMap == nullptr)
		{
			mercuryDiffuseMap = make <SV::Texture2D>(TextureData(TEXTURE_FILE_PATH + MERCURY_DIFFUSE_MAP, SV::LoadColourChannels::RGB));
			mercuryDiffuseMap->SetTextureWrapping(SV::TextureWrap::Repeat);

			mercuryAtmosphere = make <SV::Atmosphere::JupiterAtmosphere>(quadVAO);
		}

		this->diffuseMap = mercuryDiffuseMap;
		this->planetAtmo = mercuryAtmosphere;
	}
	//
#pragma endregion

#pragma region Venus Class
	//
	constexpr static const char VENUS_DIFFUSE_MAP[] = "Solar System\\Venus\\Venus Surface 1080p.jpg";
	constexpr static const char VENUS_CLOUDS_MAP[] = "Solar System\\Venus\\Venus Clouds 1080p.png";
	//
	Ref<SV::AsyncTexture2D> Venus::venusDiffuseMap = nullptr;
	Ref<SV::AsyncTexture2D> Venus::venusCloudsMap = nullptr;
	Ref<SV::Atmosphere::IAtmosphere> Venus::venusAtmosphere = nullptr;
	//
	Venus::Venus(const Ref<SV::VAO>& quadVAO) : PlanetNew(4.867E24)
	{
		if (venusDiffuseMap == nullptr)
		{
			venusDiffuseMap = make <SV::AsyncTexture2D>(make <SV::AsyncTextureData>(TEXTURE_FILE_PATH + VENUS_DIFFUSE_MAP, SV::LoadColourChannels::RGB));
			venusDiffuseMap->SetTextureWrapping(SV::TextureWrap::Repeat);

			venusCloudsMap = make <SV::AsyncTexture2D>(make <SV::AsyncTextureData>(TEXTURE_FILE_PATH + VENUS_CLOUDS_MAP, SV::LoadColourChannels::RGBA));
			venusCloudsMap->SetTextureWrapping(SV::TextureWrap::Repeat);

			venusAtmosphere = make <SV::Atmosphere::JupiterAtmosphere>(quadVAO);
		}

		this->planetAtmo = venusAtmosphere;
		this->diffuseMap = venusDiffuseMap;
		this->cloudsMap = venusCloudsMap;
	}
	//
#pragma endregion

#pragma region Earth Class
	//
	constexpr static const char EARTH_DIFFUSE_MAP[] = "Solar System\\Earth\\NASA\\Earth.jpg";
	constexpr static const char EARTH_SPECULAR_MAP[] = "Solar System\\Earth\\NASA\\Earth Specular Map.jpg";
	constexpr static const char EARTH_CLOUDS_MAP[] = "Solar System\\Earth\\NASA\\Earth Clouds.jpg";
	constexpr static const char EARTH_NIGHT_MAP[] = "Solar System\\Earth\\NASA\\Earth Night.jpg";
	//
	Ref<SV::AsyncTexture2D> Earth::earthDiffuseMap = nullptr;
	Ref<SV::AsyncTexture2D> Earth::earthSpecularMap = nullptr;
	Ref<SV::AsyncTexture2D> Earth::earthCloudsMap = nullptr;
	Ref<SV::AsyncTexture2D> Earth::earthNightMap = nullptr;

	Ref<SV::Atmosphere::EarthAtmosphere> Earth::earthAtmosphere = nullptr;
	//
	Earth::Earth(const Ref<SV::VAO>& quadVAO) : PlanetNew(5.972E24)
	{
		if (earthDiffuseMap == nullptr)
		{
			earthDiffuseMap = make <SV::AsyncTexture2D>(make <AsyncTextureData>(SV::TEXTURE_FILE_PATH + EARTH_DIFFUSE_MAP, SV::LoadColourChannels::RGB));
			earthDiffuseMap->SetTextureWrapping(SV::TextureWrap::Repeat);
			earthDiffuseMap->AddMipMapping();

			earthSpecularMap = make <SV::AsyncTexture2D>(make <AsyncTextureData>(SV::TEXTURE_FILE_PATH + EARTH_SPECULAR_MAP, SV::LoadColourChannels::RGB));
			earthSpecularMap->SetTextureWrapping(SV::TextureWrap::Repeat);
			earthSpecularMap->AddMipMapping();

			earthCloudsMap = make <SV::AsyncTexture2D>(make <AsyncTextureData>(SV::TEXTURE_FILE_PATH + EARTH_CLOUDS_MAP, SV::LoadColourChannels::RGBA));
			earthCloudsMap->SetTextureWrapping(SV::TextureWrap::Repeat);
			earthCloudsMap->AddMipMapping();

			earthNightMap = make <SV::AsyncTexture2D>(make <AsyncTextureData>(SV::TEXTURE_FILE_PATH + EARTH_NIGHT_MAP, SV::LoadColourChannels::RGB));
			earthNightMap->SetTextureWrapping(SV::TextureWrap::Repeat);
			earthNightMap->AddMipMapping();

			earthAtmosphere = make <SV::Atmosphere::EarthAtmosphere>(quadVAO, SV::Atmosphere::Luminance::None);
		}

		this->planetAtmo = earthAtmosphere;
		this->diffuseMap = earthDiffuseMap;
		this->specularMap = earthSpecularMap;
		this->cloudsMap = earthCloudsMap;
		this->nightMap = earthNightMap;
		this->ringShadowMap = nullptr;
	}
	//
#pragma endregion

#pragma region Mars Class
	//
	constexpr static const char MARS_DIFFUSE_TEXTURE[] = "Solar System\\Mars\\Mars Surface.png";
	//
	Ref<SV::Texture2D> Mars::marsDiffuseMap = nullptr;
	Ref<SV::Atmosphere::IAtmosphere> Mars::marsAtmosphere = nullptr;
	//
	Mars::Mars(const Ref<SV::VAO>& quadVAO) : PlanetNew(6.39E23)
	{
		if (marsDiffuseMap == nullptr)
		{
			marsDiffuseMap = make <SV::Texture2D>(TextureData(TEXTURE_FILE_PATH + MARS_DIFFUSE_TEXTURE, SV::LoadColourChannels::RGB));
			marsDiffuseMap->SetTextureWrapping(SV::TextureWrap::Repeat);

			marsAtmosphere = make <SV::Atmosphere::JupiterAtmosphere>(quadVAO);
		}

		this->diffuseMap = marsDiffuseMap;
		this->planetAtmo = marsAtmosphere;
	}
	//
#pragma endregion

#pragma region Jupiter Class
	//
	constexpr static const char JUPITER_DIFFUSE_MAP[] = "Solar System\\Jupiter\\Jupiter Cassini Med Resolution Map.jpg";
	constexpr static const char JUPITER_RING_MAP[] = "Solar System\\Jupiter\\Jupiter Rings.png";
	//
	Ref<SV::AsyncTexture2D> Jupiter::jupiterDiffuseMap = nullptr;
	Ref<SV::AsyncTexture2D> Jupiter::jupiterRingsMap = nullptr;
	Ref<SV::Atmosphere::JupiterAtmosphere> Jupiter::jupiterAtmosphere = nullptr;
	//
	Jupiter::Jupiter(const Ref<SV::VAO>& quadVAO) : PlanetNew(1.898E27)
	{
		if (jupiterDiffuseMap == nullptr)
		{
			jupiterDiffuseMap = make <SV::AsyncTexture2D>(make <SV::AsyncTextureData>(SV::TEXTURE_FILE_PATH + JUPITER_DIFFUSE_MAP, SV::LoadColourChannels::RGB));
			jupiterDiffuseMap->SetTextureWrapping(TextureWrap::Repeat);
			jupiterDiffuseMap->AddMipMapping();

			jupiterRingsMap = make <SV::AsyncTexture2D>(make <SV::AsyncTextureData>(SV::TEXTURE_FILE_PATH + JUPITER_RING_MAP, SV::LoadColourChannels::RGBA));
			jupiterRingsMap->SetTextureWrapping(TextureWrap::ClampToBorder);
			jupiterRingsMap->AddMipMapping();

			jupiterAtmosphere = make <SV::Atmosphere::JupiterAtmosphere>(quadVAO, SV::Atmosphere::Luminance::None);
		}

		this->diffuseMap = jupiterDiffuseMap;
		this->planetAtmo = jupiterAtmosphere;
		this->ringShadowMap = jupiterRingsMap;
	}
	//
#pragma endregion

#pragma region Saturn Class
//
	constexpr static const char SATURN_DIFFUSE_MAP[] = "Solar System\\Saturn\\8K Saturn Map.jpg";
	constexpr static const char SATURN_RING_MAP[] = "Solar System\\Saturn\\8K Saturn Ring Map.png";
	Ref<SV::AsyncTexture2D> Saturn::saturnRingMap = nullptr;
	Ref<SV::AsyncTexture2D> Saturn::saturnDiffuseMap = nullptr;
	Ref<SV::Atmosphere::SaturnAtmosphere> Saturn::saturnAtmosphere = nullptr;
	//
	Saturn::Saturn(const Ref<SV::VAO>& quadVAO) : PlanetNew(5.683E26)
	{
		if (saturnDiffuseMap == nullptr)
		{
			saturnDiffuseMap = make <SV::AsyncTexture2D>(make <AsyncTextureData>(SV::TEXTURE_FILE_PATH + SATURN_DIFFUSE_MAP, SV::LoadColourChannels::RGB));
			saturnDiffuseMap->SetTextureWrapping(SV::TextureWrap::Repeat);
			saturnDiffuseMap->AddMipMapping();

			saturnAtmosphere = make <SV::Atmosphere::SaturnAtmosphere>(quadVAO);

			saturnRingMap = make <SV::AsyncTexture2D>(make <AsyncTextureData>(SV::TEXTURE_FILE_PATH + SATURN_RING_MAP, SV::LoadColourChannels::RGBA));
			saturnRingMap->AddMipMapping();
			saturnRingMap->SetBorderColour(glm::vec4(0.0f));
			saturnRingMap->SetTextureWrapping(SV::TextureWrap::ClampToBorder);
		}

		this->planetAtmo = saturnAtmosphere;
		this->diffuseMap = saturnDiffuseMap;
		this->ringShadowMap = saturnRingMap;
	}
	//
#pragma endregion

#pragma region Titan Class
	//
	Ref<SV::Atmosphere::TitanAtmosphere> Titan::titanAtmosphere = nullptr;
	//
	Titan::Titan(const Ref<SV::VAO>& quadVAO) : PlanetNew(1.3452E23)
	{
		if (titanAtmosphere == nullptr)
		{
			titanAtmosphere = make <SV::Atmosphere::TitanAtmosphere>(quadVAO);
		}

		this->diffuseMap = WeakRef((ITexture*)SV::Texture2D::GetBlackTex());
		this->planetAtmo = titanAtmosphere;
	}
	//
#pragma endregion

#pragma region Planet Ring Shader Class
	//
	constexpr const char PLANET_RING_VERTEX[] = "Shaders\\Atmosphere\\Rings\\Rings.v.glsl";
	constexpr const char PLANET_RING_FRAGMENT[] = "Shaders\\Atmosphere\\Rings\\Rings.f.glsl";
	//
	PlanetRingShader::PlanetRingShader() : ShaderProgram(SV::VertexShader(PLANET_RING_VERTEX), SV::FragmentShader(PLANET_RING_FRAGMENT))
	{

	}
	//
#pragma endregion

#pragma region Planet New Renderer Class
	//
	const  glm::mat4 PlanetNewRenderer::RING_ROTATION_CORRECTION = glm::rotate(glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));;
	//
	PlanetNewRenderer::PlanetNewRenderer() :
		surfaceProgram(SV::Atmosphere::AtmoShaderType::SurfaceShader, Atmosphere::AtmospherePrecomputer::GetTextureHeaderString(), SV::Atmosphere::Luminance::None),
		skyProgram(SV::Atmosphere::AtmoShaderType::SkyShader, SV::Atmosphere::AtmospherePrecomputer::GetTextureHeaderString(), SV::Atmosphere::Luminance::None),
		planetVAO(),
		ringsVAO(),
		ringsProgram(),
		skyCamera()
	{
		constexpr unsigned int SPHERE_SUBDIVISIONS = 50;

		Ref<Mesh> sphereMesh = Geometry::CreateQuadsphere(1.0f, SPHERE_SUBDIVISIONS);
		planetVAO.Bind();
		planetVAO.AddAttributeArray(0, make <SV::ArrayBuffer>(sphereMesh->GetVertexArray()), SV::AttributeLayout(3));
		planetVAO.AddAttributeArray(1, make <SV::ArrayBuffer>(sphereMesh->GetNormalsArray()), SV::AttributeLayout(3));
		planetVAO.AddAttributeArray(2, make <SV::ArrayBuffer>(sphereMesh->GetTexCoordsArray()), SV::AttributeLayout(2));
		planetVAO.AddIndexBuffer(make <SV::IndexBuffer>(sphereMesh->GetIndiciesArray()));

		planetVAO.EnableAttributeArray(0, 2);
		planetVAO.Unbind();
		resetref(sphereMesh);

		constexpr float RINGS_INNER_RAD = 1.5f;
		constexpr float RINGS_OUTER_RAD = 3.2f;
		constexpr unsigned int RINGS_SEGMENTS = 60;
		Ref<Mesh> ringsMesh = Geometry::CreateHalfRings(RINGS_INNER_RAD, RINGS_OUTER_RAD, RINGS_SEGMENTS);
		ringsVAO.Bind();
		ringsVAO.AddAttributeArray(0, make <SV::ArrayBuffer>(ringsMesh->GetVertexArray()), SV::AttributeLayout(2));
		ringsVAO.AddIndexBuffer(make <SV::IndexBuffer>(ringsMesh->GetIndiciesArray()));
		ringsVAO.EnableAttributeArray(0);
		ringsVAO.Unbind();

		ringsProgram.Bind();
		ringsProgram.UploadRingWidth(RINGS_OUTER_RAD - RINGS_INNER_RAD);
		ringsProgram.Unbind();

		surfaceProgram.Bind();
		surfaceProgram.UploadPlanetCentre(glm::vec3(0.0f));
		surfaceProgram.UploadWhitePoint(glm::vec3(1.0f));

		skyProgram.Bind();
		skyProgram.UploadPlanetCentre(glm::vec3(0.0f));
		skyProgram.UploadWhitePoint(glm::vec3(1.0f));
		skyProgram.Unbind();

		PopulateDistanceCache(glm::vec3(0.0f));
	}
	//
	void PlanetNewRenderer::Render(const glm::mat4& projectionMatrix, const SV::Camera& cam, const glm::vec3& sunPosition, const float exposure, const float frameTime)
	{
		if (planets.IsEmpty()) return;

		const glm::mat4 viewProjMatrix = projectionMatrix * cam.GetViewMatrix();

		auto RenderRings = [&viewProjMatrix, &cam, &sunPosition, exposure, frameTime, this](const bool isBack) -> void
		{
			ringsProgram.Bind();
			ringsVAO.Bind();

			float yaw = isBack ? -cam.GetYaw() - 180.0f : -cam.GetYaw();
			const glm::mat4& rotMatrix = glm::rotate(glm::radians(yaw), glm::vec3(0.0f, 1.0f, 0.0f));

			glDisable(GL_CULL_FACE);
			for (const auto& keyPair : distanceCache)
			{
				const Ref<PlanetNew>& currentPlanet = planets[keyPair.second];
				if (currentPlanet->GetRingsShadowMap() == nullptr) continue;

				ringsProgram.UploadRingMap(0, *currentPlanet->GetRingsShadowMap());
				ringsProgram.UploadTransformationMat(currentPlanet->transformationMatrix * rotMatrix * RING_ROTATION_CORRECTION);
				ringsProgram.UploadViewProjectionMat(viewProjMatrix);
				ringsProgram.UploadSunPos(sunPosition);
				ringsProgram.UploadExposure(exposure);
				ringsProgram.UploadFloat("uPlanetPosition", currentPlanet->GetTranslation());

				glDrawElements(GL_TRIANGLES, ringsVAO.GetEleCount(), GL_UNSIGNED_INT, nullptr);
			}
			glEnable(GL_CULL_FACE);

		};

		RenderRings(true);

		planetVAO.Bind();

		for (const auto& keyPair : distanceCache)
		{
			Ref<PlanetNew>& currentPlanet = planets[keyPair.second];
			const SV::Atmosphere::ProcessedAtmoData& currentAtmoData = currentPlanet->GetAtmosphericData();
			currentPlanet->RecalculateTransformation();

			const Ref<ITexture>& diffuseMap = currentPlanet->GetDiffuseMap();
			const Ref<ITexture>& specularMap = currentPlanet->GetSpecularMap();
			const Ref<ITexture>& cloudsMap = currentPlanet->GetCloudsMap();
			const Ref<ITexture>& nightMap = currentPlanet->GetNightMap();
			const Ref<ITexture>& ringShadowMap = currentPlanet->GetRingsShadowMap();

			if (diffuseMap != nullptr && diffuseMap->GetClass() == ITexture::TextureClass::AsyncTexture2D) ((AsyncTexture2D*)diffuseMap refptr)->TryLoadData();
			if (specularMap != nullptr && specularMap->GetClass() == ITexture::TextureClass::AsyncTexture2D) ((AsyncTexture2D*)specularMap refptr)->TryLoadData();
			if (cloudsMap != nullptr && cloudsMap->GetClass() == ITexture::TextureClass::AsyncTexture2D) ((AsyncTexture2D*)cloudsMap refptr)->TryLoadData();
			if (nightMap != nullptr && nightMap->GetClass() == ITexture::TextureClass::AsyncTexture2D) ((AsyncTexture2D*)nightMap refptr)->TryLoadData();
			if (ringShadowMap != nullptr && ringShadowMap->GetClass() == ITexture::TextureClass::AsyncTexture2D) ((AsyncTexture2D*)ringShadowMap refptr)->TryLoadData();

#pragma region Surface Shader Rendering

			surfaceProgram.Bind();
			//Upload Matricies
			const glm::mat4 skyTransfomation = currentPlanet->transformationMatrix * glm::scale(glm::vec3((currentAtmoData.topRadius / currentAtmoData.bottomRadius)));
			surfaceProgram.UploadTransformationMat(skyTransfomation);
			surfaceProgram.UploadViewProjMat(viewProjMatrix);
			surfaceProgram.UploadNormalMat(Maths::CalculateNormalMatrix(currentPlanet->transformationMatrix));
			surfaceProgram.UploadInverseViewMat(cam.GetInverseViewMatrix());

			//Upload Scene Properties
			constexpr float planetRadius = 6360.0f;
			surfaceProgram.UploadCamPos(cam.GetActualPosition());
			surfaceProgram.UploadAtmoExposure(/*currentAtmo->GetLuminance() != Luminance::NONE ? (float)exposure * (float)1e-5 :*/ (float)exposure);
			surfaceProgram.UploadSunPos(sunPosition);
			surfaceProgram.UploadFrameTime(frameTime);
			surfaceProgram.UploadPlanetCentre(currentPlanet->GetTranslation());

			//Upload Atmosphere paramaters
			surfaceProgram.UploadAtmosphereParameters(currentAtmoData);
			//surfaceProgram.UploadSunSize(currentPlanet->GetAtmosphericData().sunAngularRadius);
			const Atmosphere::AtmospherePrecomputer& precomp = *currentPlanet->GetPrecomputer();
			surfaceProgram.UploadAtmoTextures(0, *precomp.GetTransmittanceTex(),
				1, *precomp.GetScatteringTex(),
				2, *precomp.GetIrradianceTex(),
				3, precomp.GetMieScatteringTex() refptr);

			surfaceProgram.UploadFloat("uExternalAtmoIntensity", currentAtmoData.surfaceAtmoIntensity);

			//Upload surface textures
			surfaceProgram.UploadDiffuseMap(4, *diffuseMap);
			surfaceProgram.UploadSpecularMap(5, specularMap refptr);
			surfaceProgram.UploadCloudsMap(6, cloudsMap refptr);
			surfaceProgram.UploadNightMap(7, nightMap refptr);
			surfaceProgram.UploadRingShadowMap(8, ringShadowMap refptr);

			glDrawElements(GL_TRIANGLES, planetVAO.GetEleCount(), GL_UNSIGNED_INT, nullptr);

#pragma endregion

#pragma region Sky Shader Rendering
			//skyProgram.Bind();

			////Set sky camera properties
			//skyCamera.Imitate(cam);
			//constexpr float defaultZoomAmount = 8200.0f;
			//skyCamera.SetZoom(defaultZoomAmount * cam.GetZoom());
			//skyCamera.SetCentrePosition(glm::vec3(0.0f));
			//skyCamera.RecalculateViewMatrix();


			////Upload Matricies
			//skyProgram.UploadTransformationMat(skyTransfomation);
			//skyProgram.UploadViewProjMat(viewProjMatrix);
			//skyProgram.UploadNormalMat(Maths::CalculateNormalMatrix(skyTransfomation));

			////Upload scene uniforms
			//skyProgram.UploadCamPos(cam.GetActualPosition() - currentPlanet->GetTranslation());
			//skyProgram.UploadAtmoExposure((float)exposure);
			//skyProgram.UploadSunPos(sunPosition);
			//skyProgram.UploadPlanetCentre(currentPlanet->GetTranslation());

			//float k = 1.0f / Atmosphere::kLengthUnitInMeters;
			//skyProgram.UploadFloat("uAlphaRad", 2.0f);
			//skyProgram.UploadFloat("uTime", frameTime);

			////Upload Atmospheric Parameters
			//skyProgram.UploadAtmosphereParameters(currentAtmoData);
			////skyProgram.UploadSunSize(currentPlanet->GetAtmosphericData().sunAngularRadius);
			//precomp.GetScatteringTex()->Bind(1);
			//skyProgram.UploadScatteringTexture(1);

			//glDrawElements(GL_TRIANGLES, planetVAO.GetEleCount(), GL_UNSIGNED_INT, nullptr);
#pragma endregion

		}

		RenderRings(false);

		ITexture::Unbind(TextureType::Texture2D, 0);
		ShaderProgram::Unbind();
		VAO::Unbind();
	}
	//
#pragma endregion

}
//