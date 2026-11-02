#include"SV-Engine/Precompiled Global.hpp"
#include"Application.hpp"
// #include<Windows.h>
// #include"..\Resource Files\resource.h"

//Vital utilities
#include"SV-Engine/Utilities/GlobalItems.hpp"
#include"SV-Engine/Utilities/Array.hpp"
#include"SV-Engine/Utilities/Geometry.hpp"
#include"SV-Engine/Utilities/SVUtilities.hpp"
#include<memory>
#include"SV-Engine/Utilities/SVMemory.hpp"

//OpenGL objects
#include"SV-Engine/OpenGL/OpenGL Context.hpp"
#include"SV-Engine/GUI/Display.hpp"
#include"SV-Engine/OpenGL/VAO.hpp"
#include"SV-Engine/OpenGL/ShaderProgram.hpp"
#include"SV-Engine/OpenGL/FBO.hpp"
#include"SV-Engine/OpenGL/Texture.hpp"
#include"SV-Engine/Scene/Shadows.hpp"

//Space Visuliser Objects
#include"SV-Engine/OpenGL/Post Processing.hpp"
#include"SV-Engine/Scene/Camera.hpp"
#include"SV/Universe/Star.hpp"
#include"SV/Universe/Planet.hpp"
#include"SV/Atmosphere/Atmosphere.hpp"
#include"SV/Universe/PlanetNew.hpp"
#include"SV/Universe/Planetary System.h"
#include<glm/glm.hpp>

using SV::Console;
using SV::ConsoleColour;

//Function Declarations
static void OnWindowResized(SV::Display* const sender, int newWidth, int newHeight);
static void OnKeyboard(SV::Display* const sender, int key, int scancode, int action, int mods);
static void OnMouseMotion(SV::Display* const sender, double xPos, double deltaX, double yPos, double deltaY);
static void OnMouseClick(SV::Display* const sender, int button, int action, int mods);
static void OnScroll(SV::Display* const sender, double xOffset, double detaXOffset);
static void DrawGrid();
static void DrawAxis();
//Grid Mesh
static Ref<SV::VAO> gridVAO = nullptr;
static Ref<SV::GridShader> gridShader = nullptr;
//
static bool InitiliseResources();
//
static void ExecuteRenderLoop();
static void RenderCentreCube();
static void MoveToObject();

//
static glm::mat4 projectionMatrix;
static glm::mat4 orthagonalMatrix;

//Stars
static Ref<SV::StarRenderer> sRenderer = nullptr;

//Planets
static Ref<SV::PlanetRenderer> pRenderer = nullptr;

//Planet New Test
static Ref<SV::PlanetNewRenderer> pNewRenderer = nullptr;

//Solar System
static Ref<SV::Universe::PlanetarySystem> solarSystem = nullptr;

//Shadow test
static glm::vec3 lightPosition(-10.0f, 0.0f, 0.0f);

//Camera variables
static SV::Camera mainCamera;
static Ref<SV::Display> mainWindow = nullptr;

//Current frame time 
static double aniCounter = 0.0;

static Ref<SV::DiffuseShader> diffuseColShader = nullptr;
static Ref<SV::TexturedDiffuseShader> diffuseTexShader = nullptr;
static Ref<SV::VAO> testSphereVAO = nullptr;
static Ref<SV::AsyncTexture2D> testTexture = nullptr;

static Ref<SV::VAO> centreCube = nullptr;
static SV::Transformation cubeTransformation;

//Atmosphere Test
static bool isCtrlPressed = false;
static float gExposure = 10.0f;
static int currentKey = -1;

static bool gWireframe = false;
static float gPointSize = 4.0f;

//Things to do
//Corona and star brightness/intensity calculations
//Complete planet renderer
//Fix corona depth issue
//Allow multiple shader attachments
//Shine damper/reflectivity settings

static std::unordered_map<SV::StringView, SV::CelestrialBody*> nameIndexMap; /* = {
					{ "sun"_view, {0.0f, 0.0f, 0.0f} },
					{"mercury"_view, { -10.0f, 0.0f, 0.0f }},
					{ "venus"_view, {5.0f, 0.0f, 0.0f} },
					{ "earth"_view, {147.11, 0.0f, 0.0f} },
					{ "mars"_view, {12.5f, 0.0f, 0.0f} },
					{ "jupiter"_view, {15.0f, 0.0f, 0.0f} },
					{ "saturn"_view,{22.0f, 0.0f, 0.0f} },
					{ "titan"_view, {30.0f, 0.0f, 0.0f} }
};*/

//
bool Application::Initilise(const char* assemblyName)
{
	SV::Display::Initilise(SV::OpenGLContext::OPENGL_MAJOR_VERSION, SV::OpenGLContext::OPENGL_MINOR_VERSION);

	//HANDLE smallIcon = LoadImage(GetModuleHandleA((LPCSTR)assemblyName), MAKEINTRESOURCE(IDI_ICON1), IMAGE_ICON, 16, 16, LR_COLOR);
	//HANDLE largeIcon = LoadImage(GetModuleHandleA((LPCSTR)assemblyName), MAKEINTRESOURCE(IDI_ICON2), IMAGE_ICON, 32, 32, LR_COLOR);
	mainWindow = make <SV::Display>(800, 600, "Space Visualiser");//, smallIcon, largeIcon);

	if (!SV::OpenGLContext::InitiliseOpenGL()) return false;
	glEnable(GL_MULTISAMPLE);
	return true;
}
int Application::Run()
{
	//Ref<SV::VAO> quadTest = make <SV::VAO>();
	//Ref<SV::Mesh> quadMesh = SV::Geometry::CreateQuad();
	//quadTest->Bind();
	//quadTest->AddAttributeArray(0, make <SV::ArrayBuffer>(quadMesh->GetVertexArray()));
	//quadTest->AddIndexBuffer(make <SV::IndexBuffer>(quadMesh->GetIndiciesArray()));
	//quadTest->Unbind();

	//std::cout << 'a';

	//make <SV::Atmosphere::EarthAtmosphere>(quadTest);

	//std::cout << 'a';

	//return EXIT_SUCCESS;
	if (!InitiliseResources()) return EXIT_FAILURE;

	//Window Bindings
	mainWindow->AddWindowResizeEvent(&OnWindowResized);

	//Keyboard Bindings
	mainWindow->AddKeyboardEvent(&OnKeyboard);

	//Mouse Bindings
	mainWindow->AddMouseMotionEvent(&OnMouseMotion);
	mainWindow->AddScrollEvent(&OnScroll);
	mainWindow->AddMouseClickEvent(&OnMouseClick);

	ExecuteRenderLoop();

	mainWindow.reset();

	return EXIT_SUCCESS;
}
//
void OnKeyboard(SV::Display* const sender, int key, int scancode, int action, int mods)
{
	currentKey = key;
	
	if (action == GLFW_RELEASE)
	{
		isCtrlPressed = false;
		currentKey = -1;
		return;
	}

	isCtrlPressed = (mods & GLFW_MOD_CONTROL) != 0;

	switch (key)
	{
	case GLFW_KEY_MINUS:
		gExposure -= 0.5f;
		break;
	case GLFW_KEY_EQUAL:
		gExposure += 0.5f;
		break;

	case GLFW_KEY_1:
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		glEnable(GL_CULL_FACE);
		break;
	case GLFW_KEY_2:
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		glDisable(GL_CULL_FACE);
		break;
	case GLFW_KEY_3:
		glPolygonMode(GL_FRONT_AND_BACK, GL_POINT);
		glDisable(GL_CULL_FACE);
		break;

	case GLFW_KEY_X:
		gPointSize += 0.5f;
		glPointSize(gPointSize);
		glLineWidth(gPointSize);
		break;
	case GLFW_KEY_Z:
		gPointSize -= 0.5f;
		glPointSize(gPointSize);
		glLineWidth(gPointSize);
		break;

	case GLFW_KEY_P:
		if (isCtrlPressed) MoveToObject();
		break;
	}

}
void OnMouseClick(SV::Display* const sender, int button, int action, int mods)
{

}
void OnMouseMotion(SV::Display* const sender, double xPos, double deltaX, double yPos, double deltaY)
{
	if (mainWindow->GetMouseAction() != GLFW_PRESS) return;

	mainCamera.UpdateMouse(deltaX, deltaY, mainWindow->GetMouseButton());
}
void OnScroll(SV::Display* const sender, double xOffset, double detaXOffset)
{
	mainCamera.UpdateZoom(detaXOffset);
}
void OnWindowResized(SV::Display* const sender, int newWidth, int newHeight)
{
	constexpr float FOV = 45.0f;
	constexpr float nearDist = 0.1f;
	constexpr float farDist = 1500.0f;

	if (newWidth <= 0 || newHeight <= 0) return;

	float aspect = (float)newWidth / (float)newHeight;

	float ratio_size_per_depth = atan(glm::radians(FOV / 2.0f)) * 2.0f;
	float size_y = ratio_size_per_depth;
	float size_x = ratio_size_per_depth * aspect;

	projectionMatrix = glm::perspective(glm::radians(FOV), aspect, nearDist, farDist);
	orthagonalMatrix = glm::ortho(-size_x, size_x, -size_y, size_y, nearDist, farDist);
	glViewport(0, 0, newWidth, newHeight);
}
//
bool InitiliseResources()
{
	SV::Texture2D::InitiliseStaticTextures();

	{
		Ref<SV::Mesh> gridMesh = SV::Geometry::CreateGrid(1000.0f, 1000, 1000);

		gridVAO = make <SV::VAO>();

		auto vertexBuf = make<SV::ArrayBuffer>(gridMesh->GetVertexArray());
		gridVAO->AddAttributeArray(0, vertexBuf, SV::AttributeLayout(3));
		gridVAO->AddAttributeArray(1, vertexBuf, SV::AttributeLayout(3));
		gridVAO->AddIndexBuffer(make <SV::IndexBuffer>(gridMesh->GetIndiciesArray()));
	}

	gridShader = make<SV::GridShader>();
	gridShader->Bind();
	gridShader->BindTransMat(glm::translate(glm::vec3(-500.0f, 0.0f, -500.0f)));
	gridShader->Unbind();

	//for (unsigned int i = 0; i < 5; i++)
	//{
	//	glm::vec3 colour(SV::Utilities::GetRandomFloat(), SV::Utilities::GetRandomFloat(), SV::Utilities::GetRandomFloat());
	//	glm::vec3 transformA(SV::Utilities::GetRandomFloat() * 10.0f, SV::Utilities::GetRandomFloat() * 10.0f, SV::Utilities::GetRandomFloat() * 10.0f);
	//	sRenderer->Add(make <SV::Star>(1.0f, 5800.0f, colour, transformA));
	//}

	//glActiveTexture(GL_TEXTURE3);
	//glBindTexture(GL_TEXTURE_2D, 0);

	Ref<SV::Mesh> cubeMesh = SV::Geometry::CreateQuadsphere(0.1f, 20);//SV::Geometry::CreateCube(0.05f);
	Ref<SV::ArrayBuffer> cubeVertsVBO = create <SV::ArrayBuffer>(cubeMesh->GetVertexArray());
	Ref<SV::ArrayBuffer> cubeNormalsVBO = create<SV::ArrayBuffer>(cubeMesh->GetNormalsArray());
	centreCube = create <SV::VAO>();
	centreCube->Bind();
	centreCube->AddIndexBuffer(create <SV::IndexBuffer>(cubeMesh->GetIndiciesArray()));
	centreCube->AddAttributeArray(0, cubeVertsVBO);
	centreCube->AddAttributeArray(1, cubeNormalsVBO);
	centreCube->Unbind();

	cubeMesh.reset();

	diffuseColShader = create <SV::DiffuseShader>();
	diffuseColShader->Bind();
	diffuseColShader->BindDiffuseColour(vec4(1.0f, 1.0f, 1.0f, 0.85f));
	diffuseColShader->BindLightCol(vec3(1.0f));
	diffuseColShader->Unbind();

	//earthSurfaceTexture = create <SV::Texture2D>(SV::TextureData(SV::TEXTURE_FILE_PATH + "Solar System/Earth/NASA/Earth.jpg", SV::LoadColourChannels::RGB));
	//earthSurfaceTexture->SetTextureWrapping(SV::TextureWrap::Repeat);

	//Ref<SV::TerrestrialPlanet> earth = create <SV::TerrestrialPlanet>(earthSurfaceTexture, earthCloudsTexture, earthSpecularMap, earthNightMap);
	//SV::AtmosphericData defaultData = SV::AtmosphericData::DefaultValuesCalculated;
	//defaultData.ESun = 18.0f;
	//earth->AddAtmosphere(defaultData)->AddRings(ringTexture);
	//pRenderer->AddPlanet(earth);

	//Ref<SV::Texture2D> venusDiffuse = create<SV::Texture2D>(SV::TextureData(SV::TEXTURE_FILE_PATH + "Solar System/Venus/Venus_Map.png", SV::LoadColourChannels::RGB));
	//Ref<SV::Texture2D> venusAtmo = create<SV::Texture2D>(SV::TextureData(SV::TEXTURE_FILE_PATH + "Solar System/Venus/Venus_Atmosphere.png", SV::LoadColourChannels::RGBA));

	//Ref<SV::TerrestrialPlanet> venus = create<SV::TerrestrialPlanet>(venusDiffuse, venusAtmo, nullptr, nullptr);
	//venus->AddAtmosphere();
	//venus->IncreaseTranslation(glm::vec3(10.0f, 0.0f, 0.0f));
	//pRenderer->AddPlanet(venus);

	projectionMatrix = glm::perspective(glm::radians(45.0f), (float)mainWindow->GetWidth() / (float)mainWindow->GetHeight(), 0.01f, 1500.0f);

	Ref<SV::VAO> quadVAO = create <SV::VAO>();
	Ref<SV::Mesh> quadMesh = SV::Geometry::CreateQuad();
	quadVAO->Bind();
	quadVAO->AddAttributeArray(0, create<SV::ArrayBuffer>(quadMesh->GetVertexArray()), SV::AttributeLayout(2));
	quadVAO->AddIndexBuffer(create <SV::IndexBuffer>(quadMesh->GetIndiciesArray()));
	quadVAO->EnableAttributeArray(0);
	quadVAO->Unbind();

	testSphereVAO = make <SV::VAO>();
#define SphereMode 0
#if SphereMode == 0
	const Ref<SV::Mesh>& sphereMesh = SV::Geometry::CreateQuadsphere(1.0f, 50);
#elif SphereMode == 1
	const Ref<SV::Mesh>& sphereMesh = SV::Geometry::CreateIcosphere(1.0f, 6);
#elif SphereMode == 2
	const Ref<SV::Mesh>& sphereMesh = SV::Geometry::CreateUVSphere(1.0f, 50, 50);
#endif // SphereMode

	testSphereVAO->Bind();
	testSphereVAO->AddAttributeArray(0, make <SV::ArrayBuffer>(sphereMesh->GetVertexArray()), SV::AttributeLayout(3));
	testSphereVAO->AddAttributeArray(1, make <SV::ArrayBuffer>(sphereMesh->GetNormalsArray()), SV::AttributeLayout(3));
	testSphereVAO->AddAttributeArray(2, make <SV::ArrayBuffer>(sphereMesh->GetTexCoordsArray()), SV::AttributeLayout(2));
	testSphereVAO->AddIndexBuffer(make <SV::IndexBuffer>(sphereMesh->GetIndiciesArray()));
	testSphereVAO->EnableAttributeArray(0, 2);
	testSphereVAO->Unbind();

	diffuseTexShader = make <SV::TexturedDiffuseShader>();
	diffuseTexShader->Bind();
	diffuseTexShader->BindDiffuseMapUnit(1);
	diffuseTexShader->Unbind();

	testTexture = make <SV::AsyncTexture2D>(make <SV::AsyncTextureData>(SV::TEXTURE_FILE_PATH + "Test/Test.png"));
	testTexture->SetTextureWrapping(SV::TextureWrap::Repeat);

	sRenderer = make <SV::StarRenderer>();
	//sRenderer->Add(make <SV::Star>(1.0f, 1.0f, 5800.0f, glm::vec3(26.0f, 128.0f, 128.0f) / 255.0f * 2.0f, glm::vec3(5.0f, 0.0f, 0.0f)));
	//sRenderer->Add(make <SV::Star>(1.0f, 1.0f, 100.0f, glm::vec3(1.0f, 0.2f, 0.2f), glm::vec3(0.0f, 0.0f, -10.0f)));

	pNewRenderer = make <SV::PlanetNewRenderer>();
	// pNewRenderer->Add(make <SV::Mercury>(quadVAO));
	// pNewRenderer->Add(make <SV::Venus>(quadVAO));
	// pNewRenderer->Add(make <SV::Earth>(quadVAO));
	// pNewRenderer->Add(make <SV::Mars>(quadVAO));
	// pNewRenderer->Add(make <SV::Jupiter>(quadVAO));
	// pNewRenderer->Add(make <SV::Saturn>(quadVAO));
	// pNewRenderer->Add(make <SV::Titan>(quadVAO));

	solarSystem = make <SV::Universe::PlanetarySystem>(sRenderer, pNewRenderer);
	Ref<SV::PresetStars::Sun> sunObj = make <SV::PresetStars::Sun>(glm::vec3(0.0f, 0.0f, 0.0f));
	sunObj->Scale(glm::vec3{ 15.0f });
	solarSystem->Add(sunObj);
	//solarSystem->Add(make <SV::Mercury>(quadVAO));
	//solarSystem->Add(make <SV::Venus>(quadVAO));
	solarSystem->Add(make <SV::Earth>(quadVAO));
	//solarSystem->Add(make <SV::Mars>(quadVAO));
	//solarSystem->Add(make <SV::Jupiter>(quadVAO));
	//solarSystem->Add(make <SV::Saturn>(quadVAO));
	//solarSystem->Add(make <SV::Titan>(quadVAO));

	//const Ref<SV::Mercury>& mercuryObj = pNewRenderer->GetInstance<SV::Mercury>();
	//mercuryObj->Translate({ -10.0f, 0.0f, 0.0f });
	//mercuryObj->Scale(glm::vec3{ 0.5f });

	//const Ref<SV::Venus>& venusObj = pNewRenderer->GetInstance<SV::Venus>();
	//venusObj->Translate({ 5.0f, 0.0f, 0.0f });

	const Ref<SV::Earth>& earthObj = pNewRenderer->GetInstance<SV::Earth>();
	//earthObj->Translate(glm::vec3(147.11, 0.0f, 0.0f));
	earthObj->Translate(glm::vec3(20.0, 0.0f, 0.0f));
	earthObj->SetVelocity({ 0.0, 24000, 0.0 });

	//const Ref<SV::Mars>& marsObj = pNewRenderer->GetInstance<SV::Mars>();
	//marsObj->Translate({ 12.5f, 0.0f, 0.0f });
	//marsObj->Scale(glm::vec3{ 0.75f });

	//const Ref<SV::Jupiter>& jupiterObj = pNewRenderer->GetInstance<SV::Jupiter>();
	//jupiterObj->Translate(glm::vec3(15.0f, 0.0f, 0.0f));

	//const Ref<SV::Saturn>& saturnObj = pNewRenderer->GetInstance<SV::Saturn>();
	//saturnObj->Translate(glm::vec3(22.0f, 0.0f, 0.0f));

	//const Ref<SV::Titan>& titanObj = pNewRenderer->GetInstance<SV::Titan>();
	//titanObj->Translate(glm::vec3(30.0f, 0.0f, 0.0f));

	nameIndexMap.insert({ "sun"_view, (SV::CelestrialBody*)sunObj refptr });
	nameIndexMap.insert({ "earth"_view, (SV::CelestrialBody*)earthObj refptr });

	return true;
}
//
void ExecuteRenderLoop()
{
	OnWindowResized(mainWindow.get(), (int)mainWindow->GetWidth(), (int)mainWindow->GetHeight());

	while (!mainWindow->IsCloseRequested())
	{
		constexpr double ANIMATION_SPEED = 0.2;

		mainCamera.UpdateCamera(mainWindow->GetDeltaFrameTime(), currentKey);
		aniCounter += ANIMATION_SPEED * mainWindow->GetDeltaFrameTime();
		//lightPosition.y = sin((float)aniCounter) * 5.0f;

		//
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		//
		//pRenderer->RenderPlanets(lightPosition, mainCamera, projectionMatrix, aniCounter * 0.002f);
		//
		/*atmoRenderer->Render(projectionMatrix, mainCamera, 10.0f, lightPosition, *earthSurfaceTexture, *earthSpecularMap, earthCloudsTexture.operator->(), *ringTexture);*/

		DrawGrid();

		solarSystem->Update(mainWindow->GetDeltaFrameTime() * 500.0);

		auto RenderPlanets = []() { pNewRenderer->Render(projectionMatrix,
			mainCamera,
			lightPosition,
			gExposure,
			aniCounter); };
		auto RenderStars = []() {sRenderer->Render(aniCounter * 0.05, mainCamera, projectionMatrix); };

		pNewRenderer->PopulateDistanceCache(mainCamera.GetActualPosition());
		sRenderer->PopulateDistanceCache(mainCamera.GetActualPosition());

		float closestPlanetDis = pNewRenderer->GetClosestDistance();
		float closestStarDis = sRenderer->GetClosestDistance();

		if (closestPlanetDis <= closestStarDis)
		{
			RenderStars();
			RenderPlanets();
		}
		else
		{
			RenderPlanets();
			RenderStars();
		}

		/*glm::mat4 transformMat = glm::rotate(aniCounter, glm::vec3(0.0f, 1.0f, 0.0f));
		diffuseTexShader->Bind();
		diffuseTexShader->BindProjMat(projectionMatrix);
		diffuseTexShader->BindViewMat(mainCamera.GetViewMatrix());
		diffuseTexShader->BindTransMat(transformMat);
		diffuseTexShader->BindNormalMat(SV::Maths::CalculateNormalMatrix(transformMat));
		diffuseTexShader->BindLightPos(lightPosition);

		testTexture->Bind(1);

		testSphereVAO->Bind();

		glDrawElements(GL_TRIANGLES, testSphereVAO->GetEleCount(), GL_UNSIGNED_INT, nullptr);

		testSphereVAO->Unbind();
		testTexture->Unbind();
		diffuseTexShader->Unbind();*/


		//atmoRenderer->Render(projectionMatrix, mainCamera, aniCounter * 0.1f, exposure, lightPosition, *earthSurfaceTexture, *earthSpecularMap, earthCloudsTexture.operator->(), ringTexture.operator->(), earthNightMap.operator->());
		
		RenderCentreCube();
		

		mainWindow->PollEvents();
		mainWindow->FlushScreen();
	}
}
//
void RenderCentreCube()
{
	SV::OpenGLContext::EnableAlphaBlending();

	diffuseColShader->Bind();
	diffuseColShader->BindProjMat(projectionMatrix);
	diffuseColShader->BindViewMat(mainCamera.GetViewMatrix());

	cubeTransformation.Translate(mainCamera.GetCentrePosition());
	cubeTransformation.RecalculateTransformation();
	diffuseColShader->BindTransMat(cubeTransformation);
	diffuseColShader->BindNormalMat(SV::Maths::CalculateNormalMatrix(cubeTransformation));

	diffuseColShader->BindLightPos(lightPosition);

	centreCube->Bind();
	centreCube->EnableAttributeArray(0, 1);

	glDrawElements(GL_TRIANGLES, centreCube->GetEleCount(), GL_UNSIGNED_INT, nullptr);

	centreCube->DisableAttributeArray(0, 1);
	centreCube->Unbind();

	diffuseColShader->Unbind();
}
//
void DrawGrid()
{
	glDepthMask(GL_FALSE);
	gridShader->Bind();

	gridShader->BindVPMatrix(projectionMatrix * mainCamera.GetViewMatrix());
	const glm::vec3& camPos = mainCamera.GetCentrePosition();
	gridShader->BindCamPos(camPos);

	gridVAO->Bind();
	gridVAO->EnableAttributeArray({ 0,1 });

	glDrawElements(GL_LINES, gridVAO->GetEleCount(), GL_UNSIGNED_INT, nullptr);

	gridVAO->DisableAttributeArray({ 0,1 });

	gridVAO->Unbind();

	gridShader->Unbind();
	glDepthMask(GL_TRUE);
 }
//
void MoveToObject()
{
	Console::Focus();

	SV::String name;
	std::cout << "Please input the name of the planet you wish to teleport to: ";
	std::cin >> name;
	name.ToLowerAssign();

	auto nameIt = nameIndexMap.find(name);

	if (nameIt == nameIndexMap.end())
	{
		std::cout << "Name inputted invalid\n";
		return;
	}

	mainWindow->Focus();
	mainCamera.MoveTo(nameIt->second->GetTranslation(), 6.0f);
}
//