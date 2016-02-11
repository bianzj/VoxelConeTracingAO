#include "App.h"

#include "externals/ImGui/imgui_impl_glfw_gl3.h"

#include <iostream>

using namespace std;

#ifdef __unix__
template<typename T, typename... Args>
std::unique_ptr<T> make_unique(Args&&... args) {
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}
#endif

// Ugly static variables
int mouseX, mouseY = 0;
int deltaCameraYaw = 0;
int deltaCameraPitch = 0;
float cameraMovement = 0;
bool rotateCamera = false;
bool rotateLight = false;

// GLFW callback for errors
static void errorCallback(int error, const char* description)
{
    std::cout << error << " " << description << std::endl;
}

// GLFW callback for keys
static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    // Check whether ImGui is handling this
    ImGuiIO& io = ImGui::GetIO();
    if(io.WantCaptureKeyboard)
    {
        return;
    }

    if(key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
    {
        glfwSetWindowShouldClose(window, true);
    }
    if(key == GLFW_KEY_UP && action == GLFW_PRESS)
    {
        cameraMovement += 2;
    }
    if(key == GLFW_KEY_DOWN && action == GLFW_PRESS)
    {
        cameraMovement -= 2;
    }
    if(key == GLFW_KEY_END && action == GLFW_PRESS)
    {
        cameraMovement = 0;
    }
}

// GLFW callback for cursor position
static void cursorPositionCallback(GLFWwindow* window, double xpos, double ypos)
{
    deltaCameraYaw = 10 * (mouseX - xpos);
    deltaCameraPitch = 10 * (mouseY - ypos);
    mouseX = xpos;
    mouseY = ypos;

    // Check whether ImGui is handling this
    ImGuiIO& io = ImGui::GetIO();
    if(io.WantCaptureMouse)
    {
        deltaCameraYaw = 0;
        deltaCameraPitch = 0;
        return;
    }
}

// GLFW callback for mouse buttons
void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
    {
        rotateCamera = true;
    }
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE)
    {
        rotateCamera = false;
    }
    if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS)
    {
        rotateLight = true;
    }
    if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_RELEASE)
    {
        rotateLight = false;
    }
}

App::App() : Controllable("App")
{
    int width = 1024;
    int height = 1024;
    mVoxeliseEachFrame = false;

    // Initialize GLFW and OpenGL
    glfwSetErrorCallback(errorCallback);
    if (!glfwInit())
        exit(EXIT_FAILURE);

    mpWindow = glfwCreateWindow(width, height, "VoxelConeTracing", NULL, NULL);
    if (!mpWindow)
    {
        glfwTerminate();
        exit(EXIT_FAILURE);
    }

    // Initialize
    glfwMakeContextCurrent(mpWindow);
    gl3wInit();

    // OpenGL initialization
    glClearColor(0.0f, 0.0f, 0.0f, 1);
    glEnable(GL_TEXTURE_1D);
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_TEXTURE_3D);

    // Init ImGui
    ImGui_ImplGlfwGL3_Init(mpWindow, true);

    // Set GLFW callbacks after ImGui
    glfwSetKeyCallback(mpWindow, keyCallback);
    glfwSetCursorPosCallback(mpWindow, cursorPositionCallback);
    glfwSetMouseButtonCallback(mpWindow, mouseButtonCallback);

    // Load fonts
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->AddFontDefault();

    // Variables for the loop
    mPrevTime = (GLfloat)glfwGetTime();

    // Register app as controllable
    this->registerControllable(this);

    // Scene (load polygon scene)
    m_scene = std::unique_ptr<Scene>(new Scene(this, std::string(MESHES_PATH) + "/sponza.obj"));

    // Voxelization
    m_voxelization = std::unique_ptr<Voxelization>(
        new Voxelization(this));

    mFragmentList = std::unique_ptr<FragmentList>(
            new FragmentList());

    // Sparse voxel octree (use fragment voxels and create octree for later use)

    m_svo = std::unique_ptr<SparseVoxelOctree>(new SparseVoxelOctree(this));

    m_svo->init();

    mupOctreeRaycast = std::unique_ptr<OctreeRaycast>(new OctreeRaycast(this));

	m_LightViewMap = make_unique<LightViewMap>(this);
	m_LightViewMap->init();
    m_VoxelConeTracing = make_unique<VoxelConeTracing>(this);

    m_VoxelConeTracing->init(width, height);

    m_FullScreenQuad = make_unique<FullScreenQuad>();

    m_PointCloud = make_unique<PointCloud>(mFragmentList.get(), &(m_scene->getCamera()));

    // create octree from static geometrie
    // Voxelization (create fragment voxels)
    m_voxelization->voxelize(VOLUME_EXTENT, m_scene.get(), mFragmentList.get());

    // Testing fragment list
    //
    m_svo->clearOctree();
    mFragmentList->mapToCUDA();


    //m_svo->updateOctree(mFragmentList->getColorBufferDevPointer());
    m_svo->buildOctree(mFragmentList->getPositionDevPointer(),
                       mFragmentList->getColorBufferDevPointer(),
                       mFragmentList->getNormalDevPointer(),
                       mFragmentList->getVoxelCount());

    mFragmentList->unmapFromCUDA();

}

App::~App()
{
    glfwDestroyWindow(mpWindow);
    glfwTerminate();
    exit(EXIT_SUCCESS);
}

void App::run()
{
    // Loop
    while (!glfwWindowShouldClose(mpWindow))
    {
        // Calc time per frame
        GLfloat currentTime = (GLfloat)glfwGetTime();
        GLfloat deltaTime = currentTime - mPrevTime;
        mPrevTime = currentTime;

        // Clear buffers
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // ImGui new frame
        ImGui_ImplGlfwGL3_NewFrame();

        // Update camera
        if(rotateCamera)
        {
            m_scene->updateCamera(cameraMovement * deltaTime, 0.1f * deltaCameraYaw * deltaTime, 0.1f * deltaCameraPitch * deltaTime);
        }
        else
        {
            m_scene->updateCamera(cameraMovement * deltaTime, 0, 0);
        }

        // Update light
        if (rotateLight)
        {
            m_scene->updateLight(cameraMovement * deltaTime, 0.1f * deltaCameraYaw * deltaTime, 0.1f * deltaCameraPitch * deltaTime);
        }
        else
        {
            m_scene->updateLight(cameraMovement * deltaTime, 0, 0);
        }

        // Voxelization of scene
        if(mVoxeliseEachFrame)
        {
            // Voxelization (create fragment voxels)
            m_voxelization->voxelize(VOLUME_EXTENT, m_scene.get(), mFragmentList.get());


            // Testing fragment list
            //
            m_svo->clearOctree();
            mFragmentList->mapToCUDA();


            //m_svo->updateOctree(mFragmentList->getColorBufferDevPointer());
            m_svo->buildOctree(mFragmentList->getPositionDevPointer(),
                               mFragmentList->getColorBufferDevPointer(),
                               mFragmentList->getNormalDevPointer(),
                               mFragmentList->getVoxelCount());

            mFragmentList->unmapFromCUDA();
        }

        // Get window resolution and set viewport for scene rendering
        GLint width, height;
        glfwGetWindowSize(mpWindow, &width, &height);
        glViewport(0, 0, width, height);

        m_VoxelConeTracing->geometryPass(width,height,m_scene);

        // Choose visualization TODO: make this available to user interface
        switch(VISUALIZATION)
        {
        case Visualization::RAYCASTING:
            mupOctreeRaycast->draw(
                m_scene->getCamPos(),
                m_svo->getNodePool(),
                m_svo->getBrickPool(),
                m_VoxelConeTracing->getGBuffer(),
                VOLUME_EXTENT);
            break;
        case Visualization::POINT_CLOUD:
            m_PointCloud->draw(width,height, VOLUME_EXTENT);
            break;
        case Visualization::SHADOW_MAP:
			m_LightViewMap->shadowMapPass(m_scene);
			m_VoxelConeTracing->draw(width, height, m_LightViewMap->getCurrentShadowMapRes(), m_FullScreenQuad->getvaoID(), m_LightViewMap->getDepthTextureID(), m_scene, m_svo->getNodePool(), m_svo->getBrickPool(), 5, false, VOLUME_EXTENT);
			m_LightViewMap->shadowMapRender(150,150,width, height, m_FullScreenQuad->getvaoID());
            break;
        case Visualization::TRACINGGBUFFER:
            m_LightViewMap->shadowMapPass(m_scene);
			m_VoxelConeTracing->draw(width, height, m_LightViewMap->getCurrentShadowMapRes(), m_FullScreenQuad->getvaoID(), m_LightViewMap->getDepthTextureID(), m_scene, m_svo->getNodePool(), m_svo->getBrickPool(), 5, true, VOLUME_EXTENT);
			m_LightViewMap->shadowMapRender(150,150,width, height, m_FullScreenQuad->getvaoID());
            break;
        case Visualization::VOXEL_CONE_TRACING:
			m_LightViewMap->shadowMapPass(m_scene);
			m_VoxelConeTracing->draw(width, height, m_LightViewMap->getCurrentShadowMapRes(), m_FullScreenQuad->getvaoID(), m_LightViewMap->getDepthTextureID(), m_scene, m_svo->getNodePool(), m_svo->getBrickPool(), 5, false, VOLUME_EXTENT);
            break;
		case Visualization::GBUFFER:
			m_LightViewMap->shadowMapPass(m_scene);
			m_VoxelConeTracing->RenderGBuffer(width, height);
			m_LightViewMap->shadowMapRender(width/2,height/2,width, height, m_FullScreenQuad->getvaoID());
			break;
		case Visualization::AMBIENT_OCCLUSION:
			m_VoxelConeTracing->ambientOcclusion(width, height, m_FullScreenQuad->getvaoID(), m_scene, m_svo->getNodePool(), m_svo->getBrickPool(), VOLUME_EXTENT);
			break;
        }

        // FUTURE STUFF


        // Update all controllables
        bool opened = true;
        ImGui::Begin("Properties", &opened, ImVec2(100, 200));
        for(Controllable* pControllable : mControllables)
        {
            pControllable->updateGui();
        }
        ImGui::End();

        // Render ImGui (that what is defined by controllables)
        ImGui::Render();

        // Prepare next frame
        glfwSwapBuffers(mpWindow);
        glfwPollEvents();

    }
}

void App::registerControllable(Controllable* pControllable)
{
    mControllables.push_back(pControllable);
}

void App::fillGui()
{
    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
    ImGui::SliderFloat("VolumeExtent", &VOLUME_EXTENT, 300.f, 1024.f, "%0.5f");
    ImGui::Checkbox("Voxelize each frame",&mVoxeliseEachFrame);
	ImGui::Combo("Visualisation", &VISUALIZATION, "RayCasting\0PointCloud\0LightViewMap\0TracingAndGBuffer\0VoxelConeTracing\0GBuffer\0Ambient-Occlusion\0");
}

