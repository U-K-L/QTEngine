#include "QTDoughApplication.h"
#include "ImGuizmo.cpp"

#include "../Engine/Renderer/UnigmaRenderingManager.h"
#include "../Engine/Camera/UnigmaCamera.h"
#include "../Engine/RenderPasses/RenderPassObject.h"
#include "../Engine/RenderPasses/BackgroundPass.h"
#include "../Engine/RenderPasses/CompositionPass.h"
#include "../Engine/RenderPasses/AlbedoPass.h"
#include "../Engine/RenderPasses/NormalPass.h"
#include "../Engine/RenderPasses/PositionPass.h"
#include "../Engine/RenderPasses/OutlinePass.h"
#include "../Engine/RenderPasses/ComputePass.h"
#include "../Engine/RenderPasses/SDFPass.h"
#include "../Engine/RenderPasses/VoxelizerPass.h"
#include "../Engine/RenderPasses/RayTracerPass.h"
#include "../Engine/RenderPasses/CombineSDFRasterPass.h"
#include "../Engine/RenderPasses/QuantaSpherePass.h"
#include "../Engine/RenderPasses/ImguiOverlayPass.h"
#include "../Engine/Physics/MaterialSimulationPass.h"
#include "../Engine/Physics/Emitter.h"
#include "../UnigmaNative/UnigmaNative.h"
#include "json.hpp"
#include "stb_image.h"
#include "stb_image_write.h"
#include <random>
#include <variant>
extern std::map<uint32_t, nlohmann::json> objectComponents;
extern nlohmann::json componentDefs;
UnigmaRenderingObject unigmaRenderingObjects[NUM_OBJECTS];
GameObjectShaderData gameObjectShaderDataArray[NUM_OBJECTS];
UnigmaCameraStruct CameraMain;
std::vector<RenderPassObject*> renderPassStack;
std::vector<ComputePass*> computePassStack;
std::vector<RayTracerPass*> rayTracePassStack;
QTDoughApplication* QTDoughApplication::instance = nullptr;
std::unordered_map<std::string, UnigmaTexture> textures;
std::unordered_map<std::string, Unigma3DTexture> textures3D;

MaterialSimulation* materialSimulationPass;
EmitterSystem* emitterSystem;

uint32_t currentFrame = 0;

// Undo/redo state (kept out of the header to avoid layout changes).
struct GizmoAction {
    int objectIndex;
    glm::vec3 beforePos, beforeRot, beforeScale;
    glm::vec3 afterPos, afterRot, afterScale;
};
struct MaterialAction {
    int objectIndex;
    std::string propertyKey;
    glm::vec4 before, after;
};
struct CameraAction {
    UnigmaCameraStruct before, after;
};
using UndoAction = std::variant<GizmoAction, MaterialAction, CameraAction>;
static std::vector<UndoAction> undoStack;
static std::vector<UndoAction> redoStack;
static bool gizmoWasUsing = false;
static glm::vec3 grabPos(0.0f), grabRot(0.0f), grabScale(1.0f);
static bool simulationPaused = false;
static bool simulationWarmupDone = false;

// Console log buffer.
static std::vector<std::string> consoleLog;
static const int CONSOLE_MAX_LINES = 256;
void ConsoleLog(const std::string& msg)
{
    consoleLog.push_back(msg);
    if ((int)consoleLog.size() > CONSOLE_MAX_LINES)
        consoleLog.erase(consoleLog.begin());
}

void SetupImGuiStyle()
{
    // Moonlight style by deathsu/madam-herta
    // https://github.com/Madam-Herta/Moonlight/
    ImGuiStyle& style = ImGui::GetStyle();

    style.Alpha = 1.0f;
    style.DisabledAlpha = 1.0f;
    style.WindowPadding = ImVec2(12.0f, 12.0f);
    style.WindowRounding = 11.5f;
    style.WindowBorderSize = 0.0f;
    style.WindowMinSize = ImVec2(20.0f, 20.0f);
    style.WindowTitleAlign = ImVec2(0.5f, 0.5f);
    style.WindowMenuButtonPosition = ImGuiDir_Right;
    style.ChildRounding = 0.0f;
    style.ChildBorderSize = 1.0f;
    style.PopupRounding = 0.0f;
    style.PopupBorderSize = 1.0f;
    style.FramePadding = ImVec2(20.0f, 3.400000095367432f);
    style.FrameRounding = 11.89999961853027f;
    style.FrameBorderSize = 0.0f;
    style.ItemSpacing = ImVec2(4.300000190734863f, 5.5f);
    style.ItemInnerSpacing = ImVec2(7.099999904632568f, 1.799999952316284f);
    style.CellPadding = ImVec2(12.10000038146973f, 9.199999809265137f);
    style.IndentSpacing = 0.0f;
    style.ColumnsMinSpacing = 4.900000095367432f;
    style.ScrollbarSize = 11.60000038146973f;
    style.ScrollbarRounding = 15.89999961853027f;
    style.GrabMinSize = 3.700000047683716f;
    style.GrabRounding = 20.0f;
    style.TabRounding = 0.0f;
    style.TabBorderSize = 0.0f;
    style.TabMinWidthForCloseButton = 0.0f;
    style.ColorButtonPosition = ImGuiDir_Right;
    style.ButtonTextAlign = ImVec2(0.5f, 0.5f);
    style.SelectableTextAlign = ImVec2(0.0f, 0.0f);

    style.Colors[ImGuiCol_Text] = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    style.Colors[ImGuiCol_TextDisabled] = ImVec4(0.2745098173618317f, 0.3176470696926117f, 0.4509803950786591f, 1.0f);
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.0784313753247261f, 0.08627451211214066f, 0.1019607856869698f, 1.0f);
    style.Colors[ImGuiCol_ChildBg] = ImVec4(0.09250493347644806f, 0.100297249853611f, 0.1158798336982727f, 1.0f);
    style.Colors[ImGuiCol_PopupBg] = ImVec4(0.0784313753247261f, 0.08627451211214066f, 0.1019607856869698f, 1.0f);
    style.Colors[ImGuiCol_Border] = ImVec4(0.1568627506494522f, 0.168627455830574f, 0.1921568661928177f, 1.0f);
    style.Colors[ImGuiCol_BorderShadow] = ImVec4(0.0784313753247261f, 0.08627451211214066f, 0.1019607856869698f, 1.0f);
    style.Colors[ImGuiCol_FrameBg] = ImVec4(0.1120669096708298f, 0.1262156516313553f, 0.1545064449310303f, 1.0f);
    style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.1568627506494522f, 0.168627455830574f, 0.1921568661928177f, 1.0f);
    style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.1568627506494522f, 0.168627455830574f, 0.1921568661928177f, 1.0f);
    style.Colors[ImGuiCol_TitleBg] = ImVec4(0.0470588244497776f, 0.05490196123719215f, 0.07058823853731155f, 1.0f);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.0470588244497776f, 0.05490196123719215f, 0.07058823853731155f, 1.0f);
    style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.0784313753247261f, 0.08627451211214066f, 0.1019607856869698f, 1.0f);
    style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.09803921729326248f, 0.105882354080677f, 0.1215686276555061f, 1.0f);
    style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.0470588244497776f, 0.05490196123719215f, 0.07058823853731155f, 1.0f);
    style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.1176470592617989f, 0.1333333402872086f, 0.1490196138620377f, 1.0f);
    style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.1568627506494522f, 0.168627455830574f, 0.1921568661928177f, 1.0f);
    style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.1176470592617989f, 0.1333333402872086f, 0.1490196138620377f, 1.0f);
    style.Colors[ImGuiCol_CheckMark] = ImVec4(0.9725490212440491f, 1.0f, 0.4980392158031464f, 1.0f);
    style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.971993625164032f, 1.0f, 0.4980392456054688f, 1.0f);
    style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(1.0f, 0.7953379154205322f, 0.4980392456054688f, 1.0f);
    style.Colors[ImGuiCol_Button] = ImVec4(0.1176470592617989f, 0.1333333402872086f, 0.1490196138620377f, 1.0f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.1821731775999069f, 0.1897992044687271f, 0.1974248886108398f, 1.0f);
    style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.1545050293207169f, 0.1545048952102661f, 0.1545064449310303f, 1.0f);
    style.Colors[ImGuiCol_Header] = ImVec4(0.1414651423692703f, 0.1629818230867386f, 0.2060086131095886f, 1.0f);
    style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.1072951927781105f, 0.107295036315918f, 0.1072961091995239f, 1.0f);
    style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.0784313753247261f, 0.08627451211214066f, 0.1019607856869698f, 1.0f);
    style.Colors[ImGuiCol_Separator] = ImVec4(0.1293079704046249f, 0.1479243338108063f, 0.1931330561637878f, 1.0f);
    style.Colors[ImGuiCol_SeparatorHovered] = ImVec4(0.1568627506494522f, 0.1843137294054031f, 0.250980406999588f, 1.0f);
    style.Colors[ImGuiCol_SeparatorActive] = ImVec4(0.1568627506494522f, 0.1843137294054031f, 0.250980406999588f, 1.0f);
    style.Colors[ImGuiCol_ResizeGrip] = ImVec4(0.1459212601184845f, 0.1459220051765442f, 0.1459227204322815f, 1.0f);
    style.Colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.9725490212440491f, 1.0f, 0.4980392158031464f, 1.0f);
    style.Colors[ImGuiCol_ResizeGripActive] = ImVec4(0.999999463558197f, 1.0f, 0.9999899864196777f, 1.0f);
    style.Colors[ImGuiCol_Tab] = ImVec4(0.0784313753247261f, 0.08627451211214066f, 0.1019607856869698f, 1.0f);
    style.Colors[ImGuiCol_TabHovered] = ImVec4(0.1176470592617989f, 0.1333333402872086f, 0.1490196138620377f, 1.0f);
    style.Colors[ImGuiCol_TabActive] = ImVec4(0.1176470592617989f, 0.1333333402872086f, 0.1490196138620377f, 1.0f);
    style.Colors[ImGuiCol_TabUnfocused] = ImVec4(0.0784313753247261f, 0.08627451211214066f, 0.1019607856869698f, 1.0f);
    style.Colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.1249424293637276f, 0.2735691666603088f, 0.5708154439926147f, 1.0f);
    style.Colors[ImGuiCol_PlotLines] = ImVec4(0.5215686559677124f, 0.6000000238418579f, 0.7019608020782471f, 1.0f);
    style.Colors[ImGuiCol_PlotLinesHovered] = ImVec4(0.03921568766236305f, 0.9803921580314636f, 0.9803921580314636f, 1.0f);
    style.Colors[ImGuiCol_PlotHistogram] = ImVec4(0.8841201663017273f, 0.7941429018974304f, 0.5615870356559753f, 1.0f);
    style.Colors[ImGuiCol_PlotHistogramHovered] = ImVec4(0.9570815563201904f, 0.9570719599723816f, 0.9570761322975159f, 1.0f);
    style.Colors[ImGuiCol_TableHeaderBg] = ImVec4(0.0470588244497776f, 0.05490196123719215f, 0.07058823853731155f, 1.0f);
    style.Colors[ImGuiCol_TableBorderStrong] = ImVec4(0.0470588244497776f, 0.05490196123719215f, 0.07058823853731155f, 1.0f);
    style.Colors[ImGuiCol_TableBorderLight] = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
    style.Colors[ImGuiCol_TableRowBg] = ImVec4(0.1176470592617989f, 0.1333333402872086f, 0.1490196138620377f, 1.0f);
    style.Colors[ImGuiCol_TableRowBgAlt] = ImVec4(0.09803921729326248f, 0.105882354080677f, 0.1215686276555061f, 1.0f);
    style.Colors[ImGuiCol_TextSelectedBg] = ImVec4(0.9356134533882141f, 0.9356129765510559f, 0.9356223344802856f, 1.0f);
    style.Colors[ImGuiCol_DragDropTarget] = ImVec4(0.4980392158031464f, 0.5137255191802979f, 1.0f, 1.0f);
    style.Colors[ImGuiCol_NavHighlight] = ImVec4(0.266094446182251f, 0.2890366911888123f, 1.0f, 1.0f);
    style.Colors[ImGuiCol_NavWindowingHighlight] = ImVec4(0.4980392158031464f, 0.5137255191802979f, 1.0f, 1.0f);
    style.Colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.196078434586525f, 0.1764705926179886f, 0.5450980663299561f, 0.501960813999176f);
    style.Colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.196078434586525f, 0.1764705926179886f, 0.5450980663299561f, 0.501960813999176f);
}

bool initialStart = false;
//extern SDL_Window *SDLWindow;

void QTDoughApplication::AddRenderObject(UnigmaRenderingStructCopyableAttributes* renderObject, UnigmaGameObject* gObj, uint32_t index)
{
    unigmaRenderingObjects[gObj->RenderID] = *renderObject;
    unigmaRenderingObjects[gObj->RenderID].isRendering = true;
    unigmaRenderingObjects[gObj->RenderID]._material = renderObject->_material;
    //Set ID of renderer
    unigmaRenderingObjects[gObj->RenderID]._renderer.GID = gObj->ID;
}

void QTDoughApplication::ClearObjectData()
{
    lights.clear();
}

void SaveScene()
{
    const std::string& sceneName = GetCurrentSceneName();
    if (sceneName.empty())
    {
        std::cerr << "SaveScene: No scene loaded." << std::endl;
        return;
    }

    std::string jsonPath = AssetsPath + "Scenes/" + sceneName + "/" + sceneName + ".json";

    // Read existing JSON.
    std::ifstream inFile(jsonPath);
    if (!inFile.is_open())
    {
        std::cerr << "SaveScene: Unable to open " << jsonPath << std::endl;
        return;
    }
    nlohmann::json sceneJson;
    inFile >> sceneJson;
    inFile.close();

    // Update each active rendering object's data back into the JSON.
    for (int i = 0; i < NUM_OBJECTS; i++)
    {
        if (!unigmaRenderingObjects[i].isRendering)
            continue;

        UnigmaGameObject* gObj = UNGetGameObject(unigmaRenderingObjects[i]._renderer.GID);
        if (!gObj)
            continue;

        uint32_t jid = gObj->JID;
        if (jid >= sceneJson["GameObjects"].size())
            continue;

        auto& goJson = sceneJson["GameObjects"][jid];
        const UnigmaTransform& t = unigmaRenderingObjects[i]._transform;

        // Position.
        goJson["position"]["local"]["x"] = t.position.x;
        goJson["position"]["local"]["y"] = t.position.y;
        goJson["position"]["local"]["z"] = t.position.z;
        goJson["position"]["world"]["x"] = t.position.x;
        goJson["position"]["world"]["y"] = t.position.y;
        goJson["position"]["world"]["z"] = t.position.z;

        // Rotation.
        goJson["rotation"]["local"]["x"] = t.rotation.x;
        goJson["rotation"]["local"]["y"] = t.rotation.y;
        goJson["rotation"]["local"]["z"] = t.rotation.z;
        goJson["rotation"]["world"]["x"] = t.rotation.x;
        goJson["rotation"]["world"]["y"] = t.rotation.y;
        goJson["rotation"]["world"]["z"] = t.rotation.z;

        // Scale.
        goJson["scale"]["local"]["x"] = t.scale.x;
        goJson["scale"]["local"]["y"] = t.scale.y;
        goJson["scale"]["local"]["z"] = t.scale.z;
        goJson["scale"]["world"]["x"] = t.scale.x;
        goJson["scale"]["world"]["y"] = t.scale.y;
        goJson["scale"]["world"]["z"] = t.scale.z;

        // Custom properties (material vector properties).
        if (goJson.contains("CustomProperties"))
        {
            for (auto& [key, val] : unigmaRenderingObjects[i]._material.vectorProperties)
            {
                goJson["CustomProperties"][key] = { val.x, val.y, val.z, val.w };
            }
        }

        // Components.
        if (objectComponents.count(jid))
            goJson["Components"] = objectComponents[jid];
    }

    // Lights: write back position, direction, emission, light type.
    for (uint32_t i = 0; i < UNGetLightsSize(); i++)
    {
        UnigmaLight* L = UNGetLight(i);
        if (!L) continue;
        UnigmaGameObject* gObj = UNGetGameObject(L->GID);
        if (!gObj) continue;
        uint32_t jid = gObj->JID;
        if (jid >= sceneJson["GameObjects"].size()) continue;

        auto& goJson = sceneJson["GameObjects"][jid];

        goJson["position"]["local"]["x"] = L->position.x;
        goJson["position"]["local"]["y"] = L->position.y;
        goJson["position"]["local"]["z"] = L->position.z;
        goJson["position"]["world"]["x"] = L->position.x;
        goJson["position"]["world"]["y"] = L->position.y;
        goJson["position"]["world"]["z"] = L->position.z;

        goJson["Direction"] = { L->direction.x, L->direction.y, L->direction.z };

        goJson["Emission"]["r"] = L->emission.x;
        goJson["Emission"]["g"] = L->emission.y;
        goJson["Emission"]["b"] = L->emission.z;
        goJson["Emission"]["a"] = L->emission.w;

        goJson["LightType"] = (uint32_t)L->type;
    }

    // Write back.
    std::ofstream outFile(jsonPath);
    if (!outFile.is_open())
    {
        std::cerr << "SaveScene: Unable to write " << jsonPath << std::endl;
        return;
    }
    outFile << sceneJson.dump(4);
    outFile.close();

    std::cout << "Scene saved to " << jsonPath << std::endl;
    ConsoleLog("Scene saved to " + jsonPath);
}

static void ApplyTransform(int objectIndex, const glm::vec3& pos, const glm::vec3& rot, const glm::vec3& scale)
{
    if (!VoxelizerPass::instance || objectIndex < 0
        || objectIndex >= (int)VoxelizerPass::instance->renderingObjects.size())
        return;
    UnigmaRenderingObject* obj = VoxelizerPass::instance->renderingObjects[objectIndex];
    obj->_transform.position = pos;
    obj->_transform.rotation = rot;
    obj->_transform.scale = scale;
    obj->_transform.UpdateTransform();
    obj->gizmoControlled = true;
}

static void ApplyMaterialColor(int objectIndex, const std::string& key, const glm::vec4& value)
{
    if (!VoxelizerPass::instance || objectIndex < 0
        || objectIndex >= (int)VoxelizerPass::instance->renderingObjects.size())
        return;
    UnigmaRenderingObject* obj = VoxelizerPass::instance->renderingObjects[objectIndex];
    obj->_material.vectorProperties[key] = value;
}

static void ApplyCameraState(const UnigmaCameraStruct& s)
{
    UnigmaCameraStruct* cam = UNGetCamera(0);
    if (cam) *cam = s;
}

static void ApplyUndo(const UndoAction& a)
{
    std::visit([](auto const& x) {
        using T = std::decay_t<decltype(x)>;
        if constexpr (std::is_same_v<T, GizmoAction>)
            ApplyTransform(x.objectIndex, x.beforePos, x.beforeRot, x.beforeScale);
        else if constexpr (std::is_same_v<T, MaterialAction>)
            ApplyMaterialColor(x.objectIndex, x.propertyKey, x.before);
        else if constexpr (std::is_same_v<T, CameraAction>)
            ApplyCameraState(x.before);
    }, a);
}

static void ApplyRedo(const UndoAction& a)
{
    std::visit([](auto const& x) {
        using T = std::decay_t<decltype(x)>;
        if constexpr (std::is_same_v<T, GizmoAction>)
            ApplyTransform(x.objectIndex, x.afterPos, x.afterRot, x.afterScale);
        else if constexpr (std::is_same_v<T, MaterialAction>)
            ApplyMaterialColor(x.objectIndex, x.propertyKey, x.after);
        else if constexpr (std::is_same_v<T, CameraAction>)
            ApplyCameraState(x.after);
    }, a);
}

void QTDoughApplication::UpdateObjects(UnigmaRenderingStruct* renderObject, UnigmaGameObject* gObj, uint32_t index)
{

    CameraMain = *UNGetCamera(0);
    if (!unigmaRenderingObjects[gObj->RenderID].gizmoControlled)
    {
        unigmaRenderingObjects[gObj->RenderID]._transform.position = gObj->transform.position;
        unigmaRenderingObjects[gObj->RenderID]._transform.rotation = gObj->transform.rotation;
        unigmaRenderingObjects[gObj->RenderID]._transform.UpdateTransform();
    }

    //Update the shader game objects.
    gameObjectShaderDataArray[gObj->RenderID].Midtone = unigmaRenderingObjects[gObj->RenderID]._material.vectorProperties["Midtone"];
    gameObjectShaderDataArray[gObj->RenderID].Highlight = unigmaRenderingObjects[gObj->RenderID]._material.vectorProperties["Highlight"];
    gameObjectShaderDataArray[gObj->RenderID].Shadow = unigmaRenderingObjects[gObj->RenderID]._material.vectorProperties["Shadow"];


    int lightSize = UNGetLightsSize();

    for (int i = 0; i < lightSize; i++)
    {
        UnigmaLight* light = UNGetLight(i);
        if(light != nullptr)
		{
			lights.push_back(light);
		}
    }
}

int QTDoughApplication::Run() {


    CameraMain = UnigmaCameraStruct();
    //InitSDLWindow();
	InitVulkan();

    while (PROGRAMEND == false) {


        // Advance the clock before anything reads it.
        previousTime = currentTime;
        currentTime = std::chrono::high_resolution_clock::now();
        // Main Game Loop.
        SetupEngineGUI();
        RunMainGameLoop();
    }



    // Final loop to release resources.
    RunMainGameLoop();
    vkDeviceWaitIdle(_logicalDevice);
	return 0;
}


// Queries device-local VRAM usage and budget (in bytes) via VK_EXT_memory_budget.
// Sums all heaps with VK_MEMORY_HEAP_DEVICE_LOCAL_BIT.
static void QueryVRAMUsage(VkPhysicalDevice physicalDevice, VkDeviceSize& outUsed, VkDeviceSize& outBudget, VkDeviceSize& totalBudget)
{
    outUsed = 0;
    outBudget = 0;

    VkPhysicalDeviceMemoryBudgetPropertiesEXT budgetProps{};
    budgetProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT;

    VkPhysicalDeviceMemoryProperties2 memProps2{};
    memProps2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2;
    memProps2.pNext = &budgetProps;

    vkGetPhysicalDeviceMemoryProperties2(physicalDevice, &memProps2);

    const VkPhysicalDeviceMemoryProperties& mp = memProps2.memoryProperties;
    for (uint32_t i = 0; i < mp.memoryHeapCount; i++)
    {
        if (mp.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
        {
            outUsed += budgetProps.heapUsage[i];
            outBudget += budgetProps.heapBudget[i];
        }
    }

    for (uint32_t i = 0; i < mp.memoryHeapCount; i++) {
        if (mp.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
            totalBudget += mp.memoryHeaps[i].size;
        }
    }
}


void QTDoughApplication::SetupEngineGUI()
{
    // Always advance ImGui's frame so RenderDrawData has valid data regardless of mode.
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    if (editorState.IsEditor())
    {
        ImGuizmo::BeginFrame();
        SetupImGuiStyle();

        // --- Main menu bar ---
        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("File"))
            {
                if (ImGui::MenuItem("Save", "Ctrl+S"))
                    SaveScene();
                if (ImGui::MenuItem("Export Pass Outputs"))
                    ExportPassOutputs();
                ImGui::Separator();
                if (ImGui::MenuItem("Undo", "Ctrl+Z", false, !undoStack.empty()))
                {
                    auto action = undoStack.back();
                    ApplyUndo(action);
                    redoStack.push_back(action);
                    undoStack.pop_back();
                }
                if (ImGui::MenuItem("Redo", "Ctrl+Y", false, !redoStack.empty()))
                {
                    auto action = redoStack.back();
                    ApplyRedo(action);
                    undoStack.push_back(action);
                    redoStack.pop_back();
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Run"))
            {
                bool isEditor = editorState.IsEditor();
                if (ImGui::MenuItem(isEditor ? "Play" : "Editor"))
                {
                    editorState.mode = isEditor ? EngineMode::Play : EngineMode::Editor;
                    if (!isEditor)
                        simulationPaused = true; // returning to editor pauses
                    else
                        simulationPaused = false; // entering play always runs
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Start Simulation", nullptr, false, simulationPaused))
                    simulationPaused = false;
                if (ImGui::MenuItem("Pause Simulation", nullptr, false, !simulationPaused))
                    simulationPaused = true;
                ImGui::Separator();
                bool isRecording = (recorder != nullptr);
                if (ImGui::MenuItem("Start Recording", nullptr, false, !isRecording))
                    StartRecording("", 30);
                if (ImGui::MenuItem("Stop Recording", nullptr, false, isRecording))
                    StopRecording();
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }

        // --- Ctrl+Z / Ctrl+Y hotkeys ---
        {
            static bool zWasPressed = false;
            static bool yWasPressed = false;
            bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
            bool zPressed = (GetKeyState('Z') & 0x8000) != 0;
            bool yPressed = (GetKeyState('Y') & 0x8000) != 0;

            if (ctrl && zPressed && !zWasPressed && !undoStack.empty())
            {
                auto action = undoStack.back();
                ApplyUndo(action);
                redoStack.push_back(action);
                undoStack.pop_back();
            }
            if (ctrl && yPressed && !yWasPressed && !redoStack.empty())
            {
                auto action = redoStack.back();
                ApplyRedo(action);
                undoStack.push_back(action);
                redoStack.pop_back();
            }
            zWasPressed = zPressed;
            yWasPressed = yPressed;
        }

        // --- Side panel (Settings) on the LEFT ---
        float menuH = ImGui::GetFrameHeight();
        float sideH = (float)SCREEN_HEIGHT - menuH;
        ImGui::SetNextWindowPos(ImVec2(0, menuH), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(180.0f, sideH), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSizeConstraints(ImVec2(120.0f, sideH), ImVec2(400.0f, sideH));
        ImGui::Begin("Settings", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
        float sidePanelWidth = ImGui::GetWindowWidth();

        // Performance stats
        if (std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - timeSecondPassed).count() > 999)
        {
            ImGuiIO& io = ImGui::GetIO();
            FPS = io.Framerate;
        }
        if (recorder == nullptr)
            ImGui::Text("%.3f ms/frame (%.1f FPS)", 1000.0f / FPS, FPS);
        else
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "● Recording");

        // VRAM usage (device-local heaps, via VK_EXT_memory_budget)
        {
            VkDeviceSize vramUsed = 0, vramBudget = 0, vramTotal = 0;
            QueryVRAMUsage(_physicalDevice, vramUsed, vramBudget, vramTotal);
            const double toMB = 1.0 / (1024.0 * 1024.0);
            double usedMB = (double)vramUsed * toMB;
            double budgetMB = (double)vramBudget * toMB;
            double totalMB = (double)vramTotal * toMB;
            ImGui::Text("VRAM: %.0f / %.0f / %.0f MB", usedMB, budgetMB, totalMB);

            float budgetFrac = (vramTotal > 0) ? (float)((double)vramBudget / (double)vramTotal) : 0.0f;
            float usedFrac = (vramTotal > 0) ? (float)((double)vramUsed / (double)vramTotal) : 0.0f;

            ImVec2 pos = ImGui::GetCursorScreenPos();
            ImVec2 size = ImVec2(ImGui::GetContentRegionAvail().x, ImGui::GetFrameHeight());
            ImDrawList* dl = ImGui::GetWindowDrawList();

            dl->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y),
                ImGui::GetColorU32(ImGuiCol_FrameBg), 3.0f);

            dl->AddRectFilled(pos, ImVec2(pos.x + size.x * budgetFrac, pos.y + size.y),
                IM_COL32(90, 160, 230, 255), 3.0f);

            ImU32 redColor = IM_COL32(230, 60, 60, 255);
            ImU32 yellowColor = IM_COL32(230, 200, 60, 255);
            ImU32 usedColor;

            if (usedMB > budgetMB)
                usedColor = redColor;
            else
                usedColor = yellowColor;

            dl->AddRectFilled(pos, ImVec2(pos.x + size.x * usedFrac, pos.y + size.y),
                usedColor, 3.0f);
            ImGui::Dummy(size); // advance layout cursor past the bar
        }
        ImGui::Separator();

        // Temperature display.
        ImGui::Text("Temp: %.1f C", materialSimulationPass->currentTemperature);
        int tempOffset = materialSimulationPass->temperatureHistoryHead % 120;
        ImGui::PlotLines("##TempWave", materialSimulationPass->temperatureHistory, 120, tempOffset, nullptr, FLT_MAX, FLT_MAX, ImVec2(ImGui::GetContentRegionAvail().x, 60));
        ImGui::Separator();

        if (VoxelizerPass::instance)
        {
            ImGui::SliderFloat("Support", &VoxelizerPass::instance->supportMultiplier, 0.0f, 5.0f, "%.3f");
            ImGui::Separator();
        }

        // --- Inspector (inline, scrollable) ---
        if (ImGui::CollapsingHeader("Inspector", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::BeginChild("InspectorScroll", ImVec2(0, 200), true);

            if (VoxelizerPass::instance && editorState.selectedBrushIndex >= 0
                && editorState.selectedBrushIndex < (int)VoxelizerPass::instance->brushes.size())
            {
                auto& b = VoxelizerPass::instance->brushes[editorState.selectedBrushIndex];
                auto& objects = VoxelizerPass::instance->renderingObjects;

                const char* name = "Brush";
                if (editorState.selectedBrushIndex < (int)objects.size())
                {
                    UnigmaGameObject* gObj = objects[editorState.selectedBrushIndex]->GetGameObject();
                    if (gObj && gObj->name[0] != '\0')
                        name = gObj->name;
                }

                ImGui::Text("%s", name);
                ImGui::Separator();
                ImGui::Text("ID: %u", b.id);

                int iType = (int)b.type;
                int iOpcode = (int)b.opcode;
                int iRes = (int)b.resolution;
                int iDensity = b.density;
                int iMaterialId = b.materialId;

                bool changed = false;
                changed |= ImGui::InputInt("Type", &iType);
                changed |= ImGui::InputInt("Opcode", &iOpcode);
                changed |= ImGui::InputInt("Resolution", &iRes);
                changed |= ImGui::DragFloat("Blend", &b.blend, 0.01f);
                changed |= ImGui::DragFloat("Stiffness", &b.stiffness, 0.01f);
                changed |= ImGui::DragFloat("Smoothness", &b.smoothness, 0.01f);
                changed |= ImGui::InputInt("Material", &iMaterialId);
                changed |= ImGui::InputInt("Density", &iDensity);
                changed |= ImGui::DragFloat("Mass", &b.mass, 0.01f);
                changed |= ImGui::DragFloat("Particle Radius", &b.particleRadius, 0.01f);

                if (changed)
                {
                    b.type = (uint32_t)iType;
                    b.opcode = (uint32_t)iOpcode;
                    b.resolution = (uint32_t)iRes;
                    b.density = iDensity;
                    b.materialId = iMaterialId;
                    b.isDirty = 1;
                }

                ImGui::Separator();
                ImGui::Text("Verts: %u", b.vertexCount);
                ImGui::Text("Dirty: %u", b.isDirty);
                glm::vec3 pos = glm::vec3(b.model[3]);
                ImGui::Text("Pos: %.1f, %.1f, %.1f", pos.x, pos.y, pos.z);
                if (materialSimulationPass->quantaCountReady && b.id < materialSimulationPass->brushQuantaCounts.size())
                {
                    uint32_t count = materialSimulationPass->brushQuantaCounts[b.id];
                    ImGui::Text("Quanta: %u", count);
                }
            }
            else
            {
                ImGui::TextDisabled("No brush selected");
            }

            ImGui::EndChild();
        }

        // --- Material properties ---
        if (ImGui::CollapsingHeader("Material", ImGuiTreeNodeFlags_DefaultOpen))
        {
            if (VoxelizerPass::instance && editorState.selectedBrushIndex >= 0
                && editorState.selectedBrushIndex < (int)VoxelizerPass::instance->renderingObjects.size())
            {
                static std::string matGrabKey;
                static int matGrabObj = -1;
                static glm::vec4 matGrabBefore(0.0f);

                UnigmaRenderingObject* obj = VoxelizerPass::instance->renderingObjects[editorState.selectedBrushIndex];
                auto& props = obj->_material.vectorProperties;

                for (auto& [key, val] : props)
                {
                    glm::vec4 valBeforeWidget = val;
                    float col[4] = { val.x, val.y, val.z, val.w };
                    if (ImGui::ColorEdit4(key.c_str(), col))
                    {
                        val = glm::vec4(col[0], col[1], col[2], col[3]);
                    }
                    if (ImGui::IsItemActivated())
                    {
                        matGrabKey = key;
                        matGrabObj = editorState.selectedBrushIndex;
                        matGrabBefore = valBeforeWidget;
                    }
                    if (ImGui::IsItemDeactivatedAfterEdit()
                        && matGrabObj == editorState.selectedBrushIndex
                        && matGrabKey == key
                        && matGrabBefore != val)
                    {
                        MaterialAction action;
                        action.objectIndex = matGrabObj;
                        action.propertyKey = matGrabKey;
                        action.before = matGrabBefore;
                        action.after = val;
                        undoStack.push_back(action);
                        redoStack.clear();
                        matGrabObj = -1;
                    }
                }
            }
            else
            {
                ImGui::TextDisabled("No object selected");
            }
        }

        // --- Components ---
        if (ImGui::CollapsingHeader("Components", ImGuiTreeNodeFlags_DefaultOpen))
        {
            // Get JID for selected object.
            uint32_t jid = UINT32_MAX;
            if (VoxelizerPass::instance && editorState.selectedBrushIndex >= 0
                && editorState.selectedBrushIndex < (int)VoxelizerPass::instance->renderingObjects.size())
            {
                UnigmaGameObject* gObj = VoxelizerPass::instance->renderingObjects[editorState.selectedBrushIndex]->GetGameObject();
                if (gObj)
                    jid = gObj->JID;
            }

            if (jid != UINT32_MAX && objectComponents.count(jid))
            {
                auto& comps = objectComponents[jid];

                // Display each component.
                std::string toRemove;
                for (auto it = comps.begin(); it != comps.end(); ++it)
                {
                    std::string compName = it.key();
                    bool open = ImGui::TreeNode(compName.c_str());
                    ImGui::SameLine();
                    std::string removeLabel = "X##" + compName;
                    if (ImGui::SmallButton(removeLabel.c_str()))
                        toRemove = compName;

                    if (open)
                    {
                        auto& fields = it.value();
                        for (auto fit = fields.begin(); fit != fields.end(); ++fit)
                        {
                            std::string fieldName = fit.key();
                            if (fieldName == "name")
                                continue;

                            std::string label = fieldName + "##" + compName;

                            if (fit.value().is_boolean())
                            {
                                bool val = fit.value().get<bool>();
                                if (ImGui::Checkbox(label.c_str(), &val))
                                    fit.value() = val;
                            }
                            else if (fit.value().is_number_float())
                            {
                                float val = fit.value().get<float>();
                                if (ImGui::DragFloat(label.c_str(), &val, 0.01f))
                                    fit.value() = val;
                            }
                            else if (fit.value().is_number_integer())
                            {
                                int val = fit.value().get<int>();
                                if (ImGui::InputInt(label.c_str(), &val))
                                    fit.value() = val;
                            }
                            else if (fit.value().is_string())
                            {
                                std::string val = fit.value().get<std::string>();
                                // Check if it's an enum in componentDefs.
                                bool isEnum = false;
                                if (componentDefs.contains(compName) && componentDefs[compName].contains(fieldName)
                                    && componentDefs[compName][fieldName].is_array())
                                {
                                    isEnum = true;
                                    auto& opts = componentDefs[compName][fieldName];
                                    if (ImGui::BeginCombo(label.c_str(), val.c_str()))
                                    {
                                        for (auto& opt : opts)
                                        {
                                            std::string s = opt.get<std::string>();
                                            if (ImGui::Selectable(s.c_str(), s == val))
                                                fit.value() = s;
                                        }
                                        ImGui::EndCombo();
                                    }
                                }
                                if (!isEnum)
                                {
                                    char buf[128] = {};
                                    strncpy(buf, val.c_str(), sizeof(buf) - 1);
                                    if (ImGui::InputText(label.c_str(), buf, sizeof(buf)))
                                        fit.value() = std::string(buf);
                                }
                            }
                            else if (fit.value().is_array() && fit.value().size() == 3)
                            {
                                float v[3] = { fit.value()[0], fit.value()[1], fit.value()[2] };
                                if (ImGui::DragFloat3(label.c_str(), v, 0.01f))
                                    fit.value() = { v[0], v[1], v[2] };
                            }
                        }
                        ImGui::TreePop();
                    }
                }

                if (!toRemove.empty())
                    comps.erase(toRemove);

                ImGui::Separator();

                // Add component button.
                if (ImGui::Button("Add Component"))
                    ImGui::OpenPopup("AddCompPopup");

                if (ImGui::BeginPopup("AddCompPopup"))
                {
                    for (auto it = componentDefs.begin(); it != componentDefs.end(); ++it)
                    {
                        std::string defName = it.key();
                        if (comps.contains(defName))
                            continue; // already has it
                        if (ImGui::Selectable(defName.c_str()))
                        {
                            nlohmann::json newComp;
                            newComp["name"] = defName;
                            auto& fields = it.value();
                            for (auto fit = fields.begin(); fit != fields.end(); ++fit)
                            {
                                auto& fval = fit.value();
                                if (fval.is_array())
                                    newComp[fit.key()] = fval[0]; // default to first enum option
                                else if (fval.is_object())
                                {
                                    if (fval.contains("default"))
                                        newComp[fit.key()] = fval["default"];
                                    else if (fval["type"] == "float")
                                        newComp[fit.key()] = 0.0f;
                                    else if (fval["type"] == "int")
                                        newComp[fit.key()] = 0;
                                    else if (fval["type"] == "bool")
                                        newComp[fit.key()] = false;
                                    else
                                        newComp[fit.key()] = "";
                                }
                                else if (fval == "float")
                                    newComp[fit.key()] = 0.0f;
                                else if (fval == "int")
                                    newComp[fit.key()] = 0;
                                else if (fval == "bool")
                                    newComp[fit.key()] = false;
                                else if (fval == "vector3")
                                    newComp[fit.key()] = { 1.0f, 1.0f, 1.0f };
                                else
                                    newComp[fit.key()] = "";
                            }
                            comps[defName] = newComp;
                        }
                    }
                    ImGui::EndPopup();
                }
            }
            else
            {
                ImGui::TextDisabled("No object selected");
            }
        }

        ImGui::End();

        // --- Viewport panel (game render) to the RIGHT of settings ---
        float bottomPanelHeight = 180.0f;
        float rightPanelWidth = 260.0f;
        float menuBarHeight = ImGui::GetFrameHeight();
        float viewportWidth = (float)SCREEN_WIDTH - sidePanelWidth - rightPanelWidth;
        float viewportHeight = (float)SCREEN_HEIGHT - bottomPanelHeight - menuBarHeight;
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::SetNextWindowPos(ImVec2(sidePanelWidth, menuBarHeight), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(viewportWidth, viewportHeight), ImGuiCond_Always);
        ImGui::Begin("Viewport", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar);

        // --- View mode tabs (Blender-style) ---
        {
            const struct { const char* name; ViewModes mode; } viewTabs[] = {
                { "Albedo",   ViewModes::Albedo },
                { "Render",   ViewModes::Render },
                { "SDF",      ViewModes::SDF },
                { "Quanta",   ViewModes::Quanta },
                { "Normals",  ViewModes::Normals },
                { "Material", ViewModes::Material },
                { "MBrush",   ViewModes::MaterialBrush },
            };
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2, 0));
            for (int i = 0; i < 7; i++)
            {
                if (i > 0) ImGui::SameLine();
                bool selected = (editorState.viewMode == viewTabs[i].mode);
                if (selected)
                {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.35f, 0.35f, 0.35f, 1.0f));
                }
                else
                {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.15f, 0.15f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.25f, 0.25f, 1.0f));
                }
                if (ImGui::Button(viewTabs[i].name))
                {
                    editorState.viewMode = viewTabs[i].mode;
                }
                ImGui::PopStyleColor(2);
            }
            ImGui::PopStyleVar(2);
        }

        ImVec2 vpContentPos = ImGui::GetCursorScreenPos();
        ImVec2 viewportSize = ImGui::GetContentRegionAvail();
        editorState.viewportX = vpContentPos.x;
        editorState.viewportY = vpContentPos.y;
        editorState.viewportW = viewportSize.x;
        editorState.viewportH = viewportSize.y;
        if (editorState.viewportDescriptorSet != VK_NULL_HANDLE)
        {
            ImGui::Image((ImTextureID)editorState.viewportDescriptorSet, viewportSize);
        }

        // --- ImGuizmo: manipulate all rendering objects (inside Viewport window) ---
        ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
        ImGuizmo::SetRect(vpContentPos.x, vpContentPos.y, viewportSize.x, viewportSize.y);

        glm::mat4 view = CameraMain.getViewMatrix();
        glm::mat4 proj = CameraMain.getProjectionMatrix();

        // Gizmo mode hotkeys: G=Translate, R=Rotate, S=Scale
        if (GetKeyState('G') & 0x8000) editorState.gizmoOperation = ImGuizmo::TRANSLATE;
        if (GetKeyState('R') & 0x8000) editorState.gizmoOperation = ImGuizmo::ROTATE;
        if (GetKeyState('S') & 0x8000) editorState.gizmoOperation = ImGuizmo::SCALE;

        // Order beat: F = collapse + brush assign selected brush.
        {
            static bool fWasPressed = false;
            bool fIsPressed = (GetKeyState('F') & 0x8000) != 0;
            if (fIsPressed && !fWasPressed && editorState.selectedBrushIndex >= 0)
            {
                MaterialSimulation::instance->pendingCollapseBrushIndex = editorState.selectedBrushIndex;
            }
            fWasPressed = fIsPressed;
        }

        if (VoxelizerPass::instance && editorState.selectedBrushIndex >= 0
            && editorState.selectedBrushIndex < (int)VoxelizerPass::instance->renderingObjects.size())
        {
            size_t gi = editorState.selectedBrushIndex;
            ImGuizmo::SetID(static_cast<int>(gi));
            UnigmaRenderingObject* obj = VoxelizerPass::instance->renderingObjects[gi];
            UnigmaTransform& t = obj->_transform;
            glm::mat4 model = t.GetModelMatrix();

            ImGuizmo::Manipulate(
                glm::value_ptr(view),
                glm::value_ptr(proj),
                editorState.gizmoOperation,
                ImGuizmo::WORLD,
                glm::value_ptr(model)
            );

            bool isUsing = ImGuizmo::IsUsing();

            // Snapshot on grab.
            if (isUsing && !gizmoWasUsing)
            {
                grabPos = t.position;
                grabRot = t.rotation;
                grabScale = t.scale;
            }

            if (isUsing)
            {
                obj->gizmoControlled = true;
                float trans[3], rot[3], scl[3];
                ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(model), trans, rot, scl);
                t.position = glm::vec3(trans[0], trans[1], trans[2]);
                t.rotation = glm::vec3(glm::radians(rot[0]), glm::radians(rot[1]), glm::radians(rot[2]));
                t.scale = glm::vec3(scl[0], scl[1], scl[2]);
                t.UpdateTransform();
            }

            // Push action on release.
            if (!isUsing && gizmoWasUsing)
            {
                GizmoAction action;
                action.objectIndex = (int)gi;
                action.beforePos = grabPos;
                action.beforeRot = grabRot;
                action.beforeScale = grabScale;
                action.afterPos = t.position;
                action.afterRot = t.rotation;
                action.afterScale = t.scale;
                undoStack.push_back(action);
                redoStack.clear();
            }

            gizmoWasUsing = isUsing;
        }

        // Light gizmo: translate moves position, rotate aligns direction.
        if (editorState.selectedLightIndex >= 0
            && editorState.selectedLightIndex < (int)UNGetLightsSize())
        {
            UnigmaLight* L = UNGetLight(editorState.selectedLightIndex);
            if (L)
            {
                ImGuizmo::SetID(10000 + editorState.selectedLightIndex);

                glm::vec3 dir = glm::normalize(L->direction);
                glm::vec3 upRef = (fabs(dir.z) > 0.99f) ? glm::vec3(0, 1, 0) : glm::vec3(0, 0, 1);
                glm::mat4 rot = glm::inverse(glm::lookAt(glm::vec3(0), dir, upRef));
                glm::mat4 model = glm::translate(glm::mat4(1.0f), L->position) * rot;

                ImGuizmo::OPERATION op = (editorState.gizmoOperation == ImGuizmo::SCALE)
                    ? ImGuizmo::ROTATE : editorState.gizmoOperation;

                ImGuizmo::Manipulate(
                    glm::value_ptr(view), glm::value_ptr(proj),
                    op, ImGuizmo::WORLD,
                    glm::value_ptr(model));

                if (ImGuizmo::IsUsing())
                {
                    L->position = glm::vec3(model[3]);
                    L->direction = glm::normalize(-glm::vec3(model[2]));
                }
            }
        }

        // Camera gizmo: translate moves position, rotate aligns forward.
        if (editorState.cameraSelected)
        {
            UnigmaCameraStruct* cam = UNGetCamera(0);
            if (cam)
            {
                ImGuizmo::SetID(20000);

                glm::vec3 camPos = cam->position();
                glm::vec3 fwd = glm::normalize(cam->forward());
                glm::vec3 upRef = (fabs(fwd.z) > 0.99f) ? glm::vec3(0, 1, 0) : glm::vec3(0, 0, 1);
                glm::mat4 rot = glm::inverse(glm::lookAt(glm::vec3(0), fwd, upRef));
                glm::mat4 model = glm::translate(glm::mat4(1.0f), camPos) * rot;

                ImGuizmo::OPERATION op = (editorState.gizmoOperation == ImGuizmo::SCALE)
                    ? ImGuizmo::ROTATE : editorState.gizmoOperation;

                ImGuizmo::Manipulate(
                    glm::value_ptr(view), glm::value_ptr(proj),
                    op, ImGuizmo::WORLD,
                    glm::value_ptr(model));

                static bool camGizmoWasUsing = false;
                static UnigmaCameraStruct camGizmoBefore;
                bool isUsing = ImGuizmo::IsUsing();

                if (isUsing && !camGizmoWasUsing)
                    camGizmoBefore = *cam;

                if (isUsing)
                {
                    cam->setPosition(glm::vec3(model[3]));
                    cam->setForward(glm::normalize(-glm::vec3(model[2])));
                }

                if (!isUsing && camGizmoWasUsing)
                {
                    CameraAction a;
                    a.before = camGizmoBefore;
                    a.after = *cam;
                    undoStack.push_back(a);
                    redoStack.clear();
                }

                camGizmoWasUsing = isUsing;
            }
        }

        ImGui::End();
        ImGui::PopStyleVar();

        // --- Bottom panel (Console + File Explorer) ---
        ImGui::SetNextWindowPos(ImVec2(sidePanelWidth, menuBarHeight + viewportHeight), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(viewportWidth, bottomPanelHeight), ImGuiCond_Always);
        ImGui::Begin("##BottomPanel", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);

        if (ImGui::BeginTabBar("BottomTabs"))
        {
            if (ImGui::BeginTabItem("Console"))
            {
                ImGui::BeginChild("ConsoleScroll", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
                for (auto& line : consoleLog)
                    ImGui::TextUnformatted(line.c_str());
                if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
                    ImGui::SetScrollHereY(1.0f);
                ImGui::EndChild();
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("File Explorer"))
            {
                const std::string& sceneName = GetCurrentSceneName();
                if (!sceneName.empty())
                {
                    ImGui::Text("Scene: %s", sceneName.c_str());
                    ImGui::Text("Path: Assets/Scenes/%s/", sceneName.c_str());
                }
                else
                {
                    ImGui::TextDisabled("No scene loaded");
                }
                ImGui::Separator();
                // TODO: asset browser / file listing
                ImGui::TextDisabled("Asset browser coming soon");
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }

        ImGui::End();

        // --- Right panel (Camera / Lights tabs) ---
        {
            float rightH = (float)SCREEN_HEIGHT - menuBarHeight;
            ImGui::SetNextWindowPos(ImVec2((float)SCREEN_WIDTH - rightPanelWidth, menuBarHeight), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(rightPanelWidth, rightH), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowSizeConstraints(ImVec2(rightPanelWidth, rightH), ImVec2(rightPanelWidth, rightH));
            ImGui::Begin("Scene", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);

            if (ImGui::BeginTabBar("SceneTabs"))
            {
                if (ImGui::BeginTabItem("Camera"))
                {
                    UnigmaCameraStruct* cam = UNGetCamera(0);
                    if (!cam)
                    {
                        ImGui::TextDisabled("No camera");
                    }
                    else
                    {
                        // Snapshot before any widget mutates the camera this frame.
                        UnigmaCameraStruct beforeCam = *cam;
                        static UnigmaCameraStruct camGrabBefore;
                        static bool camGrabValid = false;

                        auto checkCamUndo = [&]() {
                            if (ImGui::IsItemActivated())
                            {
                                camGrabBefore = beforeCam;
                                camGrabValid = true;
                            }
                            if (ImGui::IsItemDeactivatedAfterEdit() && camGrabValid)
                            {
                                CameraAction a;
                                a.before = camGrabBefore;
                                a.after = *cam;
                                undoStack.push_back(a);
                                redoStack.clear();
                                camGrabValid = false;
                            }
                            };

                        ImGui::Checkbox("Selected", &editorState.cameraSelected);

                        glm::vec3 camPos = cam->position();
                        if (ImGui::DragFloat3("Position", glm::value_ptr(camPos), 0.05f))
                        {
                            cam->setPosition(camPos);
                        }
                        checkCamUndo();

                        glm::vec3 camFwd = cam->forward();
                        if (ImGui::DragFloat3("Forward", glm::value_ptr(camFwd), 0.01f, -1.0f, 1.0f))
                        {
                            if (glm::length(camFwd) > 1e-5f)
                                cam->setForward(glm::normalize(camFwd));
                        }
                        checkCamUndo();

                        ImGui::DragFloat("FOV", &cam->fov, 0.25f, 1.0f, 179.0f);
                        checkCamUndo();
                        ImGui::DragFloat("Near", &cam->nearClip, 0.01f, 0.001f, 1000.0f);
                        checkCamUndo();
                        ImGui::DragFloat("Far", &cam->farClip, 1.0f, 0.1f, 100000.0f);
                        checkCamUndo();
                        ImGui::DragFloat("Ortho Width", &cam->orthoWidth, 0.1f, 0.01f, 1000.0f);
                        checkCamUndo();

                        bool ortho = cam->isOrthogonal >= 0.5f;
                        if (ImGui::Checkbox("Orthographic", &ortho))
                        {
                            cam->isOrthogonal = ortho ? 1.0f : 0.0f;
                            CameraAction a;
                            a.before = beforeCam;
                            a.after = *cam;
                            undoStack.push_back(a);
                            redoStack.clear();
                        }
                    }

                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Brushes"))
                {
                    if (!VoxelizerPass::instance)
                    {
                        ImGui::TextDisabled("Voxelizer not initialized");
                    }
                    else
                    {
                        auto& brushes = VoxelizerPass::instance->brushes;
                        auto& objects = VoxelizerPass::instance->renderingObjects;
                        for (size_t i = 0; i < brushes.size(); i++)
                        {
                            const char* label = "Brush";
                            if (i < objects.size())
                            {
                                UnigmaGameObject* gObj = objects[i]->GetGameObject();
                                if (gObj && gObj->name[0] != '\0')
                                    label = gObj->name;
                            }

                            bool selected = (editorState.selectedBrushIndex == (int)i);
                            if (ImGui::Selectable(label, selected))
                            {
                                editorState.selectedBrushIndex = (int)i;
                            }
                        }
                        if (materialSimulationPass->quantaCountReady)
                        {
                            ImGui::Separator();
                            uint32_t used = 0;
                            for (uint32_t c : materialSimulationPass->brushQuantaCounts)
                                used += c;
                            uint32_t free = QUANTA_COUNT - used;
                            float pct = 100.0f * (float)used / (float)QUANTA_COUNT;
                            ImGui::Text("Total: %u / %u (%.1f%%)", used, QUANTA_COUNT, pct);
                            ImGui::Text("Free: %u", free);
                        }
                    }
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Lights"))
                {
                    uint32_t lightCount = UNGetLightsSize();
                    if (lightCount == 0)
                    {
                        ImGui::TextDisabled("No lights in scene");
                    }
                    for (uint32_t i = 0; i < lightCount; i++)
                    {
                        UnigmaLight* L = UNGetLight(i);
                        if (!L) continue;
                        ImGui::PushID((int)i);

                        char header[32];
                        snprintf(header, sizeof(header), "Light %u", i);
                        bool isSelected = (editorState.selectedLightIndex == (int)i);
                        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen
                            | (isSelected ? ImGuiTreeNodeFlags_Selected : 0);
                        bool open = ImGui::TreeNodeEx(header, flags);
                        if (ImGui::IsItemClicked())
                            editorState.selectedLightIndex = (int)i;
                        if (open)
                        {
                            ImGui::DragFloat3("Position", glm::value_ptr(L->position), 0.05f);
                            ImGui::DragFloat3("Direction", glm::value_ptr(L->direction), 0.01f, -1.0f, 1.0f);

                            glm::vec3 color(L->emission.x, L->emission.y, L->emission.z);
                            if (ImGui::ColorEdit3("Color", glm::value_ptr(color)))
                            {
                                L->emission.x = color.x;
                                L->emission.y = color.y;
                                L->emission.z = color.z;
                            }
                            ImGui::DragFloat("Intensity", &L->emission.w, 0.05f, 0.0f, 10000.0f);

                            const char* types[] = { "Directional", "Point", "Spot", "Area" };
                            int typeIdx = (int)L->type;
                            if (typeIdx < 0 || typeIdx >= IM_ARRAYSIZE(types)) typeIdx = 0;
                            if (ImGui::Combo("Type", &typeIdx, types, IM_ARRAYSIZE(types)))
                            {
                                L->type = (uint32_t)typeIdx;
                            }

                            ImGui::TreePop();
                        }

                        ImGui::PopID();
                    }
                    ImGui::EndTabItem();
                }

                ImGui::EndTabBar();
            }

            ImGui::End();
        }
    }

    // Always end the ImGui frame so the overlay pass has valid draw data, even in play mode.
    ImGui::Render();
}

void QTDoughApplication::RunMainGameLoop()
{

    // Global F9 — toggle recording (works in both editor and play mode).
    {
        static bool f9WasPressed = false;
        bool f9Pressed = (GetKeyState(VK_F9) & 0x8000) != 0;
        if (f9Pressed && !f9WasPressed)
        {
            if (recorder == nullptr) StartRecording("", 30);
            else                     StopRecording();
        }
        f9WasPressed = f9Pressed;
    }


    //If a request is sent pause rendering.
    {
        std::unique_lock<std::mutex> lock(QTDoughEngineThread->shared.mtx);

        if (QTDoughEngineThread->shared.requestSignal)
        {
            ClearObjectData();

            //Signal main that we've finished the current frame.
            QTDoughEngineThread->shared.workFinished = true;
            lock.unlock();
            QTDoughEngineThread->shared.cvMain.notify_one();

            //Wait for main to finish updating and signal us to continue.
            lock.lock();
            QTDoughEngineThread->shared.cvWorker.wait(lock, [this] {
                return QTDoughEngineThread->shared.continueSignal;
            });

            QTDoughEngineThread->shared.continueSignal = false;
            QTDoughEngineThread->shared.requestSignal = false;
        }
    }
    //std::cout << "Updating frame..." << std::endl;




    // Calculate the elapsed time in milliseconds
    auto elapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - timeSecondPassed);

    if (elapsedTime.count() >= 1000) {

        timeSecondPassed = currentTime;
    }

    if (elapsedTime.count() >= 33)
    {
        // Auto-pause after 2s warmup in editor mode.
        if (!simulationWarmupDone)
        {
            auto sinceStart = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - timeSinceApplication);
            if (sinceStart.count() >= 2000)
            {
                simulationWarmupDone = true;
                if (editorState.IsEditor())
                    simulationPaused = true;
            }
        }

        if (!simulationPaused || !simulationWarmupDone)
            ComputePhysics();
    }

    elapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - timeMinutePassed);

    if (elapsedTime.count() >= 1000 * 60) {

        timeMinutePassed = currentTime;
    }

    DrawFrame();
    if (GatherBlenderInfo() == 0)
    {
        //CameraToBlender();
        //GetMeshDataAllObjects();
    }

    //RecreateResources();
}

void QTDoughApplication::ComputePhysics()
{
    vkWaitForFences(_logicalDevice, 1, &_physicsFence, VK_TRUE, UINT64_MAX);
    vkResetFences(_logicalDevice, 1, &_physicsFence);

    vkResetCommandBuffer(_physicsCommandBuffer, 0);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(_physicsCommandBuffer, &beginInfo);

    // Emit 100 leptons in a sphere when L is pressed.
    {
        const Uint8* keystate = SDL_GetKeyboardState(NULL);
        static bool lWasPressed = false;
        if (keystate[SDL_SCANCODE_L] && !lWasPressed)
        {
            VoxelizerPass* voxelizer = VoxelizerPass::instance;
            glm::vec3 pos = voxelizer->brushes[editorState.selectedBrushIndex].model[3];
            Emitter ev{};
            ev.information = glm::ivec4(0, 1, 0, 0);            // y=1 = LEPTON.
            ev.position = glm::vec4(pos.x, pos.y, pos.z, 100.0f); // center=origin, count=100.
            ev.shape = glm::vec4(0.5f, 0.0f, 0.0f, 1.0f);      // radius=2, shape=sphere.
            ev.direction = glm::vec4(0.0f, 0.0f, 1.0f, 0.725f);
            ev.velocity = glm::vec4(2.0f, 0.0f, 0.0f, 5.0f);   // w=lifespan.
            ev.mana = glm::vec4(200.25f, 0.0f, 0.0f, 0.0f);
            emitterSystem->AddEvent(ev);
            std::cout << "Emitted event" << std::endl;
        }
        lWasPressed = keystate[SDL_SCANCODE_L];
    }

    // Laser beam: emit leptons along a line while left click + spacebar held.
    if ((GetKeyState(VK_LBUTTON) & 0x8000) && (GetKeyState(VK_SPACE) & 0x8000))
    {
        int mx, my;
        SDL_GetMouseState(&mx, &my);
        glm::vec3 origin, direction;
        MaterialSimulation::instance->ScreenToWorldRay((float)mx, (float)my, origin, direction);
        Photon photon;
        photon.position = glm::vec4(origin, 1.0f);
        photon.direction = glm::vec4(direction, 1.0f);
        photon.information = glm::ivec4(0);

        int wasHit = MaterialSimulation::instance->RayCast(photon, true);
        if (wasHit > 0)
        {
            extern UnigmaCameraStruct CameraMain;
            glm::vec3 camPos = CameraMain.position();
            glm::vec3 camFwd = glm::normalize(CameraMain.forward());
            glm::vec3 camRight = CameraMain.right;
            glm::vec3 camUp = CameraMain.up;
            glm::vec3 rayOrigin = glm::vec3(2, 2, 4);//origin + camRight * 1.5f - camUp * 0.3f + camFwd * 5.5f;

            glm::vec3 directionToPoint = glm::normalize(glm::vec3(photon.position) - rayOrigin);

            Emitter ev{};
            ev.information = glm::ivec4(0, 1, 0, 0);
            ev.position = glm::vec4(rayOrigin, 10);
            ev.shape = glm::vec4(1.25f, 0.0f, 0.0f, 2.0f);
            ev.direction = glm::vec4(directionToPoint, 0.35f);
            ev.velocity = glm::vec4(10.0f, 1.0f, 0.0f, 5.0f);
            ev.mana = glm::vec4(20000.25f, 0.0f, 0.0f, 0.0f);
            EmitterSystem::instance->AddEvent(ev);
        }


    }


    emitterSystem->FlushEvents();
    emitterSystem->Dispatch(_physicsCommandBuffer);
    materialSimulationPass->Simulate(_physicsCommandBuffer);
    materialSimulationPass->SurveyTemperature();

    vkEndCommandBuffer(_physicsCommandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &_physicsCommandBuffer;

    if (vkQueueSubmit(_vkPhysicsQueue, 1, &submitInfo, _physicsFence) != VK_SUCCESS) {
        throw std::runtime_error("failed to submit physics command buffer!");
    }
}

void QTDoughApplication::DrawFrame()
{


    //Aquire the rendered image.
    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(_logicalDevice, _swapChain, UINT64_MAX, _imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

    //Resize screen if something had changed.
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        RecreateSwapChain();
        return;
    }
    else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("failed to acquire swap chain image!");
    }

    UpdateGlobalDescriptorSet();

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    // Compute submission        
    vkWaitForFences(_logicalDevice, 1, &computeInFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

    vkResetFences(_logicalDevice, 1, &computeInFlightFences[currentFrame]);

    vkResetCommandBuffer(computeCommandBuffers[currentFrame], /*VkCommandBufferResetFlagBits*/ 0);
    RecordComputeCommandBuffer(computeCommandBuffers[currentFrame]); //Begin command buffers.

    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &computeCommandBuffers[currentFrame];
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &computeFinishedSemaphores[currentFrame];

    if (vkQueueSubmit(_vkComputeQueue, 1, &submitInfo, computeInFlightFences[currentFrame]) != VK_SUCCESS) {
        throw std::runtime_error("failed to submit compute command buffer!");
    };

    //Waits for this fence to finish. 
    vkWaitForFences(_logicalDevice, 1, &_inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

    VkResult computeWaitResult = vkWaitForFences(
        _logicalDevice,
        1,
        &computeInFlightFences[currentFrame],
        VK_TRUE,
        UINT64_MAX
    );

    if (computeWaitResult == VK_SUCCESS)
    {
        //Get data from previous frame.
        ReadBackGPUData();
    }
    else
    {
        std::cout << "DrawFrame: compute fence wait failed (VkResult=" << computeWaitResult
                  << "), skipping readback." << std::endl;
    }

    // Write previous frame's bytes to ffmpeg
    if (recorder && recorder->IsRecording()) {
        recorder->DrainFrameToEncoder(currentFrame);
    }

    //Set fence back to unsignal for next time.
    vkResetFences(_logicalDevice, 1, &_inFlightFences[currentFrame]);



    //vkResetCommandBuffer(computeCommandBuffers[currentFrame], /*VkCommandBufferResetFlagBits*/ 0);
    //RecordComputeCommandBuffer(computeCommandBuffers[currentFrame]);

    //Return command buffers back to original state.
    vkResetCommandBuffer(_commandBuffers[currentFrame], /*VkCommandBufferResetFlagBits*/ 0);
    RecordCommandBuffer(_commandBuffers[currentFrame], imageIndex);;

    VkSemaphore waitSemaphores[] = { _imageAvailableSemaphores[currentFrame] };
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;

    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &_commandBuffers[currentFrame];

    VkSemaphore signalSemaphores[] = { _renderFinishedSemaphores[currentFrame] };
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    /*
    if (vkQueueSubmit(_vkComputeQueue, 1, &submitInfo, nullptr) != VK_SUCCESS) {
        throw std::runtime_error("failed to submit compute command buffer!");
    };
    */
    //Sends the recorded command buffer to be rendered.
    if (vkQueueSubmit(_vkGraphicsQueue, 1, &submitInfo, _inFlightFences[currentFrame]) != VK_SUCCESS) {
        throw std::runtime_error("failed to submit draw command buffer!");
    }

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapChains[] = { _swapChain };
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;

    presentInfo.pImageIndices = &imageIndex;

    //Actually draws to the screen.
    result = vkQueuePresentKHR(_presentQueue, &presentInfo);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized) {
        framebufferResized = false;
        RecreateSwapChain();
        UpdateGlobalDescriptorSet();
    }
    else if (result != VK_SUCCESS) {
        throw std::runtime_error("failed to present swap chain image!");
    }


    //DebugCompute(currentFrame);

    currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}


VkVertexInputBindingDescription QTDoughApplication::getBindingDescription() {
    VkVertexInputBindingDescription bindingDescription{};
    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(Vertex);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    return bindingDescription;
}

std::array<VkVertexInputAttributeDescription, 4> QTDoughApplication::getAttributeDescriptions() {

    std::array<VkVertexInputAttributeDescription, 4> attributeDescriptions{};

    attributeDescriptions[0].binding = 0;
    attributeDescriptions[0].location = 0;
    attributeDescriptions[0].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    attributeDescriptions[0].offset = offsetof(Vertex, pos);

    attributeDescriptions[1].binding = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    attributeDescriptions[1].offset = offsetof(Vertex, color);

    attributeDescriptions[2].binding = 0;
    attributeDescriptions[2].location = 2;
    attributeDescriptions[2].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    attributeDescriptions[2].offset = offsetof(Vertex, texCoord);

    attributeDescriptions[3].binding = 0;
    attributeDescriptions[3].location = 3;
    attributeDescriptions[3].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    attributeDescriptions[3].offset = offsetof(Vertex, normal);

    return attributeDescriptions;
}

bool QTDoughApplication::HasStencilComponent(VkFormat format) {
    return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
}

VkFormat QTDoughApplication::FindDepthFormat() {
    return FindSupportedFormat(
        { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
    );
}

VkFormat QTDoughApplication::FindSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) {
    for (VkFormat format : candidates) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(_physicalDevice, format, &props);

        if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
            return format;
        }
        else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
            return format;
        }
    }

    throw std::runtime_error("failed to find supported format!");

}

VkRenderingAttachmentInfo QTDoughApplication::AttachmentInfo(VkImageView view, VkClearValue* clear, VkImageLayout layout /*= VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL*/)
{
    VkRenderingAttachmentInfo colorAttachment{};
    colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    colorAttachment.pNext = nullptr;

    colorAttachment.imageView = view;
    colorAttachment.imageLayout = layout;
    colorAttachment.loadOp = clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    if (clear) {
        colorAttachment.clearValue = *clear;
    }

    return colorAttachment;
}

VkRenderingInfo QTDoughApplication::RenderingInfo(VkExtent2D extent, VkRenderingAttachmentInfo* colorAttachment, VkRenderingAttachmentInfo* depthAttachment)
{
    VkRenderingInfo renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.pNext = nullptr;
    renderingInfo.flags = 0;
    renderingInfo.renderArea.offset = { 0, 0 };
    renderingInfo.renderArea.extent = extent;
    renderingInfo.layerCount = 1;
    renderingInfo.viewMask = 0; // For multiview rendering, if needed

    if (colorAttachment != nullptr)
    {
        renderingInfo.colorAttachmentCount = 1;
        renderingInfo.pColorAttachments = colorAttachment;
    }
    else
    {
        renderingInfo.colorAttachmentCount = 0;
        renderingInfo.pColorAttachments = nullptr;
    }

    renderingInfo.pDepthAttachment = depthAttachment;
    renderingInfo.pStencilAttachment = nullptr; // Assuming no separate stencil attachment

    return renderingInfo;
}


void QTDoughApplication::DrawImgui(VkCommandBuffer cmd, VkImageView targetImageView)
{
    VkRenderingAttachmentInfo colorAttachment = AttachmentInfo(targetImageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkRenderingInfo renderInfo = RenderingInfo(swapChainExtent, &colorAttachment, nullptr);

    vkCmdBeginRendering(cmd, &renderInfo);

    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

    vkCmdEndRendering(cmd);
}

VkSubmitInfo QTDoughApplication::SubmitInfo(VkCommandBuffer* cmd)
{
    VkSubmitInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    info.pNext = nullptr;

    info.waitSemaphoreCount = 0;
    info.pWaitSemaphores = nullptr;
    info.pWaitDstStageMask = nullptr;
    info.commandBufferCount = 1;
    info.pCommandBuffers = cmd;
    info.signalSemaphoreCount = 0;
    info.pSignalSemaphores = nullptr;

    return info;
}

VkSubmitInfo2 QTDoughApplication::SubmitInfo(VkCommandBufferSubmitInfo* cmd, VkSemaphoreSubmitInfo* signalSemaphoreInfo,
    VkSemaphoreSubmitInfo* waitSemaphoreInfo)
{
    VkSubmitInfo2 info = {};
    info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    info.pNext = nullptr;

    info.waitSemaphoreInfoCount = waitSemaphoreInfo == nullptr ? 0 : 1;
    info.pWaitSemaphoreInfos = waitSemaphoreInfo;

    info.signalSemaphoreInfoCount = signalSemaphoreInfo == nullptr ? 0 : 1;
    info.pSignalSemaphoreInfos = signalSemaphoreInfo;

    info.commandBufferInfoCount = 1;
    info.pCommandBufferInfos = cmd;

    return info;
}

VkCommandBufferSubmitInfo QTDoughApplication::CommandBufferSubmitInfo(VkCommandBuffer cmd)
{
    VkCommandBufferSubmitInfo info{};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
    info.pNext = nullptr;
    info.commandBuffer = cmd;
    info.deviceMask = 0;

    return info;
}

VkCommandBufferBeginInfo QTDoughApplication::CommandBufferBeginInfo(VkCommandBufferUsageFlags flags /*= 0*/)
{
    VkCommandBufferBeginInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    info.pNext = nullptr;

    info.pInheritanceInfo = nullptr;
    info.flags = flags;
    return info;
}

VkCommandBufferAllocateInfo QTDoughApplication::CommandBufferAllocateInfo(VkCommandPool pool, uint32_t count /*= 1*/)
{
    VkCommandBufferAllocateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    info.pNext = nullptr;

    info.commandPool = pool;
    info.commandBufferCount = count;
    info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    return info;
}

void QTDoughApplication::InitCommands()
{
    VK_CHECK(vkCreateCommandPool(_logicalDevice, &_commandPoolInfo, nullptr, &_immCommandPool));

    // allocate the command buffer for immediate submits
    VkCommandBufferAllocateInfo cmdAllocInfo = CommandBufferAllocateInfo(_immCommandPool, 1);

    VK_CHECK(vkAllocateCommandBuffers(_logicalDevice, &cmdAllocInfo, &_immCommandBuffer));


    /*
    _mainDeletionQueue.push_function([=]() {
        vkDestroyCommandPool(_logicalDevice, _immCommandPool, nullptr);
        });
    */

}

void QTDoughApplication::InitSyncStructures()
{
    VK_CHECK(vkCreateFence(_logicalDevice, &fenceInfo, nullptr, &_immFence));

}

void QTDoughApplication::ImmediateSubmit(std::function<void(VkCommandBuffer cmd)>&& function)
{
    VK_CHECK(vkResetFences(_logicalDevice, 1, &_immFence));
    VK_CHECK(vkResetCommandBuffer(_immCommandBuffer, 0));

    VkCommandBuffer cmd = _immCommandBuffer;

    VkCommandBufferBeginInfo cmdBeginInfo = CommandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

    function(cmd);

    VK_CHECK(vkEndCommandBuffer(cmd));

    VkCommandBufferSubmitInfo cmdinfo = CommandBufferSubmitInfo(cmd);
    VkSubmitInfo2 submit = SubmitInfo(&cmdinfo, nullptr, nullptr);

    // submit command buffer to the queue and execute it.
    //  _renderFence will now block until the graphic commands finish execution
    VK_CHECK(vkQueueSubmit2(_vkGraphicsQueue, 1, &submit, _immFence));

    VK_CHECK(vkWaitForFences(_logicalDevice, 1, &_immFence, true, 9999999999));
}

void QTDoughApplication::InitImGui()
{
    //1: create descriptor pool for IMGUI
    // the size of the pool is very oversize, but it's copied from imgui demo itself.
    VkDescriptorPoolSize pool_sizes[] =
    {
        { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
    };

    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 1000;
    pool_info.poolSizeCount = std::size(pool_sizes);
    pool_info.pPoolSizes = pool_sizes;

    VkDescriptorPool imguiPool;
    VK_CHECK(vkCreateDescriptorPool(_logicalDevice, &pool_info, nullptr, &imguiPool));

    std::cout << "Started pool info imgui" << std::endl;
    // 2: initialize imgui library

    // this initializes the core structures of imgui
    ImGui::CreateContext();
    ImGui_ImplSDL2_InitForVulkan(QTSDLWindow);
    std::cout << "Started pool info imgui" << std::endl;
    // this initializes imgui for Vulkan
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = _vkInstance;
    init_info.PhysicalDevice = _physicalDevice;
    init_info.Device = _logicalDevice;
    init_info.Queue = _vkGraphicsQueue;
    init_info.DescriptorPool = imguiPool;
    init_info.MinImageCount = 3;
    init_info.ImageCount = 3;
    init_info.UseDynamicRendering = true;

    //dynamic rendering parameters for imgui to use
    init_info.PipelineRenderingCreateInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
    init_info.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
    init_info.PipelineRenderingCreateInfo.pColorAttachmentFormats = &_swapChainImageFormat;


    init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

    ImGui_ImplVulkan_Init(&init_info);

    ImGui_ImplVulkan_CreateFontsTexture();

    renderPassStack.push_back(new ImguiOverlayPass());

    /*
    // add the destroy the imgui created structures
    _mainDeletionQueue.push_function([=]() {
        ImGui_ImplVulkan_Shutdown();
        vkDestroyDescriptorPool(_device, imguiPool, nullptr);
        });
        */
}

void QTDoughApplication::CreateFrameOutput()
{
    CreateImage(
        swapChainExtent.width,
        swapChainExtent.height,
        _swapChainImageFormat,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        frameOutput,
        frameOutputMemory
    );
    frameOutputView = CreateImageView(frameOutput, _swapChainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT);

    CreateImage(
        swapChainExtent.width,
        swapChainExtent.height,
        _swapChainImageFormat,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        frameOutputSnapshot,
        frameOutputSnapshotMemory
    );
    frameOutputSnapshotView = CreateImageView(frameOutputSnapshot, _swapChainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT);
    // No initial layout transition — the per-frame barriers use srcLayout=UNDEFINED, which
    // discards prior contents and works regardless of the image's current state.
}

void QTDoughApplication::CleanupFrameOutput()
{
    if (frameOutputView != VK_NULL_HANDLE) {
        vkDestroyImageView(_logicalDevice, frameOutputView, nullptr);
        frameOutputView = VK_NULL_HANDLE;
    }
    if (frameOutput != VK_NULL_HANDLE) {
        vkDestroyImage(_logicalDevice, frameOutput, nullptr);
        frameOutput = VK_NULL_HANDLE;
    }
    if (frameOutputMemory != VK_NULL_HANDLE) {
        vkFreeMemory(_logicalDevice, frameOutputMemory, nullptr);
        frameOutputMemory = VK_NULL_HANDLE;
    }
    if (frameOutputSnapshotView != VK_NULL_HANDLE) {
        vkDestroyImageView(_logicalDevice, frameOutputSnapshotView, nullptr);
        frameOutputSnapshotView = VK_NULL_HANDLE;
    }
    if (frameOutputSnapshot != VK_NULL_HANDLE) {
        vkDestroyImage(_logicalDevice, frameOutputSnapshot, nullptr);
        frameOutputSnapshot = VK_NULL_HANDLE;
    }
    if (frameOutputSnapshotMemory != VK_NULL_HANDLE) {
        vkFreeMemory(_logicalDevice, frameOutputSnapshotMemory, nullptr);
        frameOutputSnapshotMemory = VK_NULL_HANDLE;
    }
}

void QTDoughApplication::CreateEditorViewportImage()
{
    // ImGui-side: sampler + descriptor binding so ImGui::Image can display frameOutput.
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    VK_CHECK(vkCreateSampler(_logicalDevice, &samplerInfo, nullptr, &editorState.viewportSampler));

    editorState.viewportDescriptorSet = ImGui_ImplVulkan_AddTexture(
        editorState.viewportSampler,
        frameOutputSnapshotView,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    );
}

void QTDoughApplication::CleanupEditorViewportImage()
{
    if (editorState.viewportDescriptorSet != VK_NULL_HANDLE) {
        ImGui_ImplVulkan_RemoveTexture(editorState.viewportDescriptorSet);
        editorState.viewportDescriptorSet = VK_NULL_HANDLE;
    }
    if (editorState.viewportSampler != VK_NULL_HANDLE) {
        vkDestroySampler(_logicalDevice, editorState.viewportSampler, nullptr);
        editorState.viewportSampler = VK_NULL_HANDLE;
    }
}

uint32_t QTDoughApplication::FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(_physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }


    throw std::runtime_error("failed to find suitable memory type!");
}

void QTDoughApplication::CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(_logicalDevice, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to create buffer!");
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(_logicalDevice, buffer, &memRequirements);

    VkMemoryAllocateFlagsInfo allocFlags{
    VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO
    };
    allocFlags.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = FindMemoryType(memRequirements.memoryTypeBits, properties);

    allocInfo.pNext =
        (usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)
        ? &allocFlags
        : nullptr;

    if (vkAllocateMemory(_logicalDevice, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate buffer memory!");
    }

    vkBindBufferMemory(_logicalDevice, buffer, bufferMemory, 0);
}

/*
void QTDoughApplication::UpdateUniformBuffer(uint32_t currentImage) {
    static auto startTime = std::chrono::high_resolution_clock::now();

    auto currentTime = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

    UniformBufferObject ubo{};
    ubo.model = glm::mat4(1.0f);
    ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    ubo.proj = glm::perspective(glm::radians(45.0f), swapChainExtent.width / (float)swapChainExtent.height, 0.1f, 10.0f);
    ubo.proj[1][1] *= -1;

    memcpy(_uniformBuffersMapped[currentImage], &ubo, sizeof(ubo));
}
*/
void QTDoughApplication::CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
    VkCommandBuffer commandBuffer = BeginSingleTimeCommands();

    VkBufferCopy copyRegion{};
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

    EndSingleTimeCommands(commandBuffer);
}

void QTDoughApplication::CopyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) {
    VkCommandBuffer commandBuffer = BeginSingleTimeCommands();



    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;

    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;

    region.imageOffset = { 0, 0, 0 };
    region.imageExtent = {
        width,
        height,
        1
    };

    vkCmdCopyBufferToImage(
        commandBuffer,
        buffer,
        image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &region
    );

    EndSingleTimeCommands(commandBuffer);
}

void QTDoughApplication::AddPasses()
{

    CompositionPass* compPass = new CompositionPass();
    OutlinePass* outlinePass = new OutlinePass();
    CombineSDFRasterPass* combineSDFRasterPass = new CombineSDFRasterPass();
    PositionPass* positionPass = new PositionPass();
    NormalPass* normalPass = new NormalPass();
    AlbedoPass* albedoPass = new AlbedoPass();
    BackgroundPass* bgPass = new BackgroundPass();
    QuantaSpherePass* quantaSpherePass = new QuantaSpherePass();

    //Add objects.
    albedoPass->AddObjects(unigmaRenderingObjects);
    normalPass->AddObjects(unigmaRenderingObjects);
    positionPass->AddObjects(unigmaRenderingObjects);

    renderPassStack.push_back(bgPass);
    renderPassStack.push_back(albedoPass);
    renderPassStack.push_back(normalPass);
    renderPassStack.push_back(positionPass);
    renderPassStack.push_back(combineSDFRasterPass);
    renderPassStack.push_back(outlinePass);
    renderPassStack.push_back(quantaSpherePass);
    renderPassStack.push_back(compPass);


    //Compute Passes.
    SDFPass* sdfPass = new SDFPass();
    VoxelizerPass* voxelizerPass = new VoxelizerPass();
    VoxelizerPass::SetInstance(voxelizerPass);

    //Add objects to compute pass.
    voxelizerPass->AddObjects(unigmaRenderingObjects);
    sdfPass->AddObjects(unigmaRenderingObjects);


    //Add the compute pass to the stack.
    computePassStack.push_back(voxelizerPass);
    computePassStack.push_back(sdfPass);

    //Ray tracer passes.
    RayTracerPass* rayTracerPass = new RayTracerPass();
    rayTracerPass->AddObjects(unigmaRenderingObjects);

    //Add ray tracer pass to the stack.
    rayTracePassStack.push_back(rayTracerPass);



    std::cout << "Passes count: " << renderPassStack.size() << std::endl;
}

void QTDoughApplication::InitVulkan()
{

    //Create the intial instances, windows, get the GPU and create the swap chain.
	CreateInstance();
    SetupDebugMessenger();
    CreateWindowSurface();
    PickPhysicalDevice();
    CreateLogicalDevice();
    CreateSwapChain();

    //Create command pool early so GPU benchmark can use it.
    CreateCommandPool();
    RunGPUBenchmark();

    //Create Material Sim.
    materialSimulationPass = new MaterialSimulation();
    materialSimulationPass->InitMaterialSim();
    materialSimulationPass->SetInstance(materialSimulationPass);

    //Create Emitter System (phase 1: buffers only).
    emitterSystem = new EmitterSystem();
    emitterSystem->instance = emitterSystem;
    emitterSystem->InitEmitter();

    AddPasses();

    //Create all the image views.
    CreateImageViews();

    //The descriptor layouts.
    CreateDescriptorSetLayout();
    CreateGlobalDescriptorSetLayout();
    CreateComputeDescriptorSetLayout();
    //CreateOffscreenDescriptorSetLayout();

    //Create graphics pipelines.
    CreateMaterials();
    CreateGraphicsPipeline();
    CreateComputePipeline();
    //CreateBackgroundGraphicsPipeline();

    //Create Depths
    CreateDepthResources();

    //Create the images and their samplers.
    //CreateTextureImage();
    //CreateTextureImageView();
    CreateTextureSampler();
    CreateImages();

    //Load models and create the buffers for vertices and indicies and uniforms.
    //LoadModel();
    CreateVertexBuffer();
    CreateQuadBuffers();
    CreateIndexBuffer();
    CreateUniformBuffers();
    CreateShaderStorageBuffers();

    //Create descriptor pools and sets
    CreateDescriptorPool();
    CreateGlobalDescriptorPool();
    CreateDescriptorSets();
    CreateGlobalUniformBuffers();
    CreateGlobalDescriptorSet();
    CreateComputeDescriptorSets();

    //Initialize material simulation compute workload (after global descriptors are ready).
    materialSimulationPass->InitComputeWorkload();

    //Initialize emitter compute workload (after materialSim and global descriptors are ready).
    emitterSystem->InitComputeWorkload();

    //Create command buffers.
    CreateCommandBuffers();
    CreateComputeCommandBuffers();
    CreatePhysicsCommandBuffer();

    //Imgui.
    CreateFrameOutput();
    InitImGui();
    CreateEditorViewportImage();

    //Sync the buffers.
    CreateSyncObjects();

}

void QTDoughApplication::RunGPUBenchmark()
{
    std::cout << "========================" << std::endl;
    std::cout << "Running GPU Benchmark..." << std::endl;
    std::cout << "========================" << std::endl;

    // Get timestamp period from device properties.
    VkPhysicalDeviceProperties devProps{};
    vkGetPhysicalDeviceProperties(_physicalDevice, &devProps);
    float timestampPeriod = devProps.limits.timestampPeriod; // nanoseconds per tick

    if (timestampPeriod == 0.0f) {
        std::cout << "GPU timestamps not supported, skipping benchmark." << std::endl;
        return;
    }

    // Create timestamp query pool (4 queries: before/after ALU, before/after BW).
    VkQueryPoolCreateInfo queryPoolInfo{};
    queryPoolInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
    queryPoolInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
    queryPoolInfo.queryCount = 4;

    VkQueryPool queryPool;
    VK_CHECK(vkCreateQueryPool(_logicalDevice, &queryPoolInfo, nullptr, &queryPool));

    // --- ALU throughput test resources ---
    const uint32_t aluElementCount = 65536; // 65536 float4 = 1 MB
    VkDeviceSize aluBufferSize = aluElementCount * sizeof(float) * 4;

    VkBuffer aluBuffer;
    VkDeviceMemory aluBufferMemory;
    CreateBuffer(aluBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, aluBuffer, aluBufferMemory);

    // ALU descriptor set layout: binding 0 = storage buffer.
    VkDescriptorSetLayoutBinding aluBinding{};
    aluBinding.binding = 0;
    aluBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    aluBinding.descriptorCount = 1;
    aluBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutCreateInfo aluDslCI{};
    aluDslCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    aluDslCI.bindingCount = 1;
    aluDslCI.pBindings = &aluBinding;

    VkDescriptorSetLayout aluDSL;
    VK_CHECK(vkCreateDescriptorSetLayout(_logicalDevice, &aluDslCI, nullptr, &aluDSL));

    // ALU pipeline.
    auto aluShaderCode = readFile("src/shaders/benchmark_alu.spv");
    VkShaderModule aluShaderModule = CreateShaderModule(aluShaderCode);

    VkPipelineLayoutCreateInfo aluPlCI{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    aluPlCI.setLayoutCount = 1;
    aluPlCI.pSetLayouts = &aluDSL;

    VkPipelineLayout aluPipelineLayout;
    VK_CHECK(vkCreatePipelineLayout(_logicalDevice, &aluPlCI, nullptr, &aluPipelineLayout));

    VkPipelineShaderStageCreateInfo aluStage{};
    aluStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    aluStage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    aluStage.module = aluShaderModule;
    aluStage.pName = "main";

    VkComputePipelineCreateInfo aluPipelineCI{};
    aluPipelineCI.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    aluPipelineCI.stage = aluStage;
    aluPipelineCI.layout = aluPipelineLayout;

    VkPipeline aluPipeline;
    VK_CHECK(vkCreateComputePipelines(_logicalDevice, VK_NULL_HANDLE, 1, &aluPipelineCI, nullptr, &aluPipeline));

    // --- Bandwidth test resources ---
    const uint32_t bwElementCount = 2 * 1024 * 1024; // 2M float4 = 32 MB each
    VkDeviceSize bwBufferSize = bwElementCount * sizeof(float) * 4;

    VkBuffer bwSrcBuffer, bwDstBuffer;
    VkDeviceMemory bwSrcMemory, bwDstMemory;
    CreateBuffer(bwBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, bwSrcBuffer, bwSrcMemory);
    CreateBuffer(bwBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, bwDstBuffer, bwDstMemory);

    // BW descriptor set layout: binding 0 + binding 1 = storage buffers.
    VkDescriptorSetLayoutBinding bwBindings[2]{};
    bwBindings[0].binding = 0;
    bwBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bwBindings[0].descriptorCount = 1;
    bwBindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bwBindings[1].binding = 1;
    bwBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bwBindings[1].descriptorCount = 1;
    bwBindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutCreateInfo bwDslCI{};
    bwDslCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    bwDslCI.bindingCount = 2;
    bwDslCI.pBindings = bwBindings;

    VkDescriptorSetLayout bwDSL;
    VK_CHECK(vkCreateDescriptorSetLayout(_logicalDevice, &bwDslCI, nullptr, &bwDSL));

    // BW pipeline.
    auto bwShaderCode = readFile("src/shaders/benchmark_bw.spv");
    VkShaderModule bwShaderModule = CreateShaderModule(bwShaderCode);

    VkPipelineLayoutCreateInfo bwPlCI{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    bwPlCI.setLayoutCount = 1;
    bwPlCI.pSetLayouts = &bwDSL;

    VkPipelineLayout bwPipelineLayout;
    VK_CHECK(vkCreatePipelineLayout(_logicalDevice, &bwPlCI, nullptr, &bwPipelineLayout));

    VkPipelineShaderStageCreateInfo bwStage{};
    bwStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    bwStage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    bwStage.module = bwShaderModule;
    bwStage.pName = "main";

    VkComputePipelineCreateInfo bwPipelineCI{};
    bwPipelineCI.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    bwPipelineCI.stage = bwStage;
    bwPipelineCI.layout = bwPipelineLayout;

    VkPipeline bwPipeline;
    VK_CHECK(vkCreateComputePipelines(_logicalDevice, VK_NULL_HANDLE, 1, &bwPipelineCI, nullptr, &bwPipeline));

    // --- Descriptor pool and sets ---
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSize.descriptorCount = 3; // ALU(1) + BW(2)

    VkDescriptorPoolCreateInfo dpCI{};
    dpCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    dpCI.maxSets = 2;
    dpCI.poolSizeCount = 1;
    dpCI.pPoolSizes = &poolSize;

    VkDescriptorPool benchPool;
    VK_CHECK(vkCreateDescriptorPool(_logicalDevice, &dpCI, nullptr, &benchPool));

    // ALU descriptor set.
    VkDescriptorSetAllocateInfo aluDsAI{};
    aluDsAI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    aluDsAI.descriptorPool = benchPool;
    aluDsAI.descriptorSetCount = 1;
    aluDsAI.pSetLayouts = &aluDSL;

    VkDescriptorSet aluDS;
    VK_CHECK(vkAllocateDescriptorSets(_logicalDevice, &aluDsAI, &aluDS));

    VkDescriptorBufferInfo aluBufInfo{};
    aluBufInfo.buffer = aluBuffer;
    aluBufInfo.offset = 0;
    aluBufInfo.range = aluBufferSize;

    VkWriteDescriptorSet aluWrite{};
    aluWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    aluWrite.dstSet = aluDS;
    aluWrite.dstBinding = 0;
    aluWrite.descriptorCount = 1;
    aluWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    aluWrite.pBufferInfo = &aluBufInfo;

    vkUpdateDescriptorSets(_logicalDevice, 1, &aluWrite, 0, nullptr);

    // BW descriptor set.
    VkDescriptorSetAllocateInfo bwDsAI{};
    bwDsAI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    bwDsAI.descriptorPool = benchPool;
    bwDsAI.descriptorSetCount = 1;
    bwDsAI.pSetLayouts = &bwDSL;

    VkDescriptorSet bwDS;
    VK_CHECK(vkAllocateDescriptorSets(_logicalDevice, &bwDsAI, &bwDS));

    VkDescriptorBufferInfo bwSrcInfo{ bwSrcBuffer, 0, bwBufferSize };
    VkDescriptorBufferInfo bwDstInfo{ bwDstBuffer, 0, bwBufferSize };

    VkWriteDescriptorSet bwWrites[2]{};
    bwWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    bwWrites[0].dstSet = bwDS;
    bwWrites[0].dstBinding = 0;
    bwWrites[0].descriptorCount = 1;
    bwWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bwWrites[0].pBufferInfo = &bwSrcInfo;
    bwWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    bwWrites[1].dstSet = bwDS;
    bwWrites[1].dstBinding = 1;
    bwWrites[1].descriptorCount = 1;
    bwWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bwWrites[1].pBufferInfo = &bwDstInfo;

    vkUpdateDescriptorSets(_logicalDevice, 2, bwWrites, 0, nullptr);

    // --- Record and submit command buffer ---
    VkCommandBufferAllocateInfo cmdAI{};
    cmdAI.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdAI.commandPool = _commandPool;
    cmdAI.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdAI.commandBufferCount = 1;

    VkCommandBuffer cmd;
    VK_CHECK(vkAllocateCommandBuffers(_logicalDevice, &cmdAI, &cmd));

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo));

    vkCmdResetQueryPool(cmd, queryPool, 0, 4);

    VkMemoryBarrier memBarrier{};
    memBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    memBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    memBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;

    // ALU benchmark: timestamp -> dispatch -> barrier -> timestamp.
    vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, queryPool, 0);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, aluPipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                            aluPipelineLayout, 0, 1, &aluDS, 0, nullptr);
    vkCmdDispatch(cmd, 256, 1, 1); // 256 groups * 256 threads = 65536 threads

    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         0, 1, &memBarrier, 0, nullptr, 0, nullptr);

    vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, queryPool, 1);

    // BW benchmark: timestamp -> 8x dispatch -> timestamp.
    vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, queryPool, 2);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, bwPipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                            bwPipelineLayout, 0, 1, &bwDS, 0, nullptr);

    uint32_t bwGroupCount = bwElementCount / 256;
    for (int rep = 0; rep < 8; rep++) {
        vkCmdDispatch(cmd, bwGroupCount, 1, 1);
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             0, 1, &memBarrier, 0, nullptr, 0, nullptr);
    }

    vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, queryPool, 3);

    VK_CHECK(vkEndCommandBuffer(cmd));

    // Submit and wait.
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd;

    vkQueueSubmit(_vkComputeQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(_vkComputeQueue);

    // --- Read results ---
    uint64_t timestamps[4];
    VK_CHECK(vkGetQueryPoolResults(_logicalDevice, queryPool, 0, 4,
        sizeof(timestamps), timestamps, sizeof(uint64_t),
        VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT));

    // ALU: 65536 threads * 1024 FMA iterations * 8 FLOPs per FMA on float4 = 536,870,912 FLOPs.
    double aluTicks = static_cast<double>(timestamps[1] - timestamps[0]);
    double aluTimeSec = (aluTicks * static_cast<double>(timestampPeriod)) * 1e-9;
    double totalFLOPs = 65536.0 * 1024.0 * 8.0;
    double gflops = (totalFLOPs / aluTimeSec) / 1e9;

    // BW: 8 reps * (32MB read + 32MB write) = 512 MB transferred.
    double bwTicks = static_cast<double>(timestamps[3] - timestamps[2]);
    double bwTimeSec = (bwTicks * static_cast<double>(timestampPeriod)) * 1e-9;
    double totalBytes = 8.0 * 2.0 * static_cast<double>(bwBufferSize);
    double gbps = (totalBytes / bwTimeSec) / 1e9;

    std::cout << "GPU Benchmark Results:" << std::endl;
    std::cout << "  Compute throughput: " << gflops << " GFLOPS" << std::endl;
    std::cout << "  Memory bandwidth:   " << gbps << " GB/s" << std::endl;

    // --- Set quality level ---
    if (gflops >= 12000.0 && gbps >= 400.0) {
        GameQualityLevel = 0; // Ultra
    } else if (gflops >= 8000.0 && gbps >= 300.0) {
        GameQualityLevel = 1; // High
    } else if (gflops >= 5000.0 && gbps >= 200.0) {
        GameQualityLevel = 2; // Medium, Target.
    } else {
        GameQualityLevel = 3; // Low, likely below 30 FPS, TODO: Show a popup in-game that GPU is below target.
    }

    const char* qualityNames[] = { "Ultra", "High", "Medium", "Low" };
    std::cout << "  Selected quality level: " << qualityNames[GameQualityLevel]
              << " (" << GameQualityLevel << ")" << std::endl;
    std::cout << "========================" << std::endl;

    // --- Cleanup ---
    vkFreeCommandBuffers(_logicalDevice, _commandPool, 1, &cmd);
    vkDestroyQueryPool(_logicalDevice, queryPool, nullptr);

    vkDestroyPipeline(_logicalDevice, aluPipeline, nullptr);
    vkDestroyPipelineLayout(_logicalDevice, aluPipelineLayout, nullptr);
    vkDestroyPipeline(_logicalDevice, bwPipeline, nullptr);
    vkDestroyPipelineLayout(_logicalDevice, bwPipelineLayout, nullptr);

    vkDestroyDescriptorPool(_logicalDevice, benchPool, nullptr);
    vkDestroyDescriptorSetLayout(_logicalDevice, aluDSL, nullptr);
    vkDestroyDescriptorSetLayout(_logicalDevice, bwDSL, nullptr);

    vkDestroyShaderModule(_logicalDevice, aluShaderModule, nullptr);
    vkDestroyShaderModule(_logicalDevice, bwShaderModule, nullptr);

    vkDestroyBuffer(_logicalDevice, aluBuffer, nullptr);
    vkFreeMemory(_logicalDevice, aluBufferMemory, nullptr);
    vkDestroyBuffer(_logicalDevice, bwSrcBuffer, nullptr);
    vkFreeMemory(_logicalDevice, bwSrcMemory, nullptr);
    vkDestroyBuffer(_logicalDevice, bwDstBuffer, nullptr);
    vkFreeMemory(_logicalDevice, bwDstMemory, nullptr);
}

VkResult QTDoughApplication::CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) {
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr) {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    }
    else {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}


void QTDoughApplication::SetupDebugMessenger() {
    if (!enableValidationLayers) return;


    VkDebugUtilsMessengerCreateInfoEXT createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = debugCallback;
    createInfo.pUserData = nullptr; // Optional

    if (CreateDebugUtilsMessengerEXT(_vkInstance, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS) {
        throw std::runtime_error("failed to set up debug messenger!");
    }
}

void QTDoughApplication::CreateCompute()
{

    /*
    std::vector<VkBuffer> shaderStorageBuffers;
    std::vector<VkDeviceMemory> shaderStorageBuffersMemory;

    auto computeShaderCode = readFile("shaders/compute.spv");

    VkShaderModule computeShaderModule = CreateShaderModule(computeShaderCode);

    VkPipelineShaderStageCreateInfo computeShaderStageInfo{};
    computeShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    computeShaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    computeShaderStageInfo.module = computeShaderModule;
    computeShaderStageInfo.pName = "main";

    // Initialize particles
    std::default_random_engine rndEngine((unsigned)time(nullptr));
    std::uniform_real_distribution<float> rndDist(0.0f, 1.0f);

    // Initial particle positions on a circle
    std::vector<Particle> particles(PARTICLE_COUNT);
    for (auto& particle : particles) {
        float r = 0.25f * sqrt(rndDist(rndEngine));
        float theta = rndDist(rndEngine) * 2 * 3.14159265358979323846;
        float x = r * cos(theta) * swapChainExtent.height / swapChainExtent.width;
        float y = r * sin(theta);
        particle.position = glm::vec2(x, y);
        particle.velocity = glm::normalize(glm::vec2(x, y)) * 0.00025f;
        particle.color = glm::vec4(rndDist(rndEngine), rndDist(rndEngine), rndDist(rndEngine), 1.0f);
    }

    VkDeviceSize bufferSize = sizeof(Particle) * PARTICLE_COUNT;

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

    void* data;
    vkMapMemory(_logicalDevice, stagingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, particles.data(), (size_t)bufferSize);
    vkUnmapMemory(_logicalDevice, stagingBufferMemory);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        CreateBuffer(bufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, shaderStorageBuffers[i], shaderStorageBuffersMemory[i]);
        // Copy data from the staging buffer (host) to the shader storage buffer (GPU)
        CopyBuffer(stagingBuffer, shaderStorageBuffers[i], bufferSize);
    }

    std::array<VkDescriptorSetLayoutBinding, 3> layoutBindings{};
    layoutBindings[0].binding = 0;
    layoutBindings[0].descriptorCount = 1;
    layoutBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    layoutBindings[0].pImmutableSamplers = nullptr;
    layoutBindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    layoutBindings[1].binding = 1;
    layoutBindings[1].descriptorCount = 1;
    layoutBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    layoutBindings[1].pImmutableSamplers = nullptr;
    layoutBindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    layoutBindings[2].binding = 2;
    layoutBindings[2].descriptorCount = 1;
    layoutBindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    layoutBindings[2].pImmutableSamplers = nullptr;
    layoutBindings[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 3;
    layoutInfo.pBindings = layoutBindings.data();

    if (vkCreateDescriptorSetLayout(_logicalDevice, &layoutInfo, nullptr, &computeDescriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create compute descriptor set layout!");
    }

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        //VkDescriptorBufferInfo uniformBufferInfo{};
        //uniformBufferInfo.buffer = uniformBuffers[i];
        //uniformBufferInfo.offset = 0;
        //uniformBufferInfo.range = sizeof(UniformBufferObject);

        std::array<VkWriteDescriptorSet, 2> descriptorWrites{};
        std::array<VkDescriptorPoolSize, 2> poolSizes{};


        VkDescriptorBufferInfo storageBufferInfoLastFrame{};
        storageBufferInfoLastFrame.buffer = shaderStorageBuffers[(i - 1) % MAX_FRAMES_IN_FLIGHT];
        storageBufferInfoLastFrame.offset = 0;
        storageBufferInfoLastFrame.range = sizeof(Particle) * PARTICLE_COUNT;

        descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[1].dstSet = computeDescriptorSets[i];
        descriptorWrites[1].dstBinding = 1;
        descriptorWrites[1].dstArrayElement = 0;
        descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrites[1].descriptorCount = 1;
        descriptorWrites[1].pBufferInfo = &storageBufferInfoLastFrame;

        VkDescriptorBufferInfo storageBufferInfoCurrentFrame{};
        storageBufferInfoCurrentFrame.buffer = shaderStorageBuffers[i];
        storageBufferInfoCurrentFrame.offset = 0;
        storageBufferInfoCurrentFrame.range = sizeof(Particle) * PARTICLE_COUNT;

        descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[2].dstSet = computeDescriptorSets[i];
        descriptorWrites[2].dstBinding = 2;
        descriptorWrites[2].dstArrayElement = 0;
        descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrites[2].descriptorCount = 1;
        descriptorWrites[2].pBufferInfo = &storageBufferInfoCurrentFrame;

        poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        poolSizes[1].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT) * 2;

        vkUpdateDescriptorSets(_logicalDevice, 3, descriptorWrites.data(), 0, nullptr);
    }

    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.layout = _computePipelineLayout;
    pipelineInfo.stage = computeShaderStageInfo;

    if (vkCreateComputePipelines(_logicalDevice, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &_computePipeline) != VK_SUCCESS) {
        throw std::runtime_error("failed to create compute pipeline!");
    }

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &computeDescriptorSetLayout;

    if (vkCreatePipelineLayout(_logicalDevice, &pipelineLayoutInfo, nullptr, &_computePipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create compute pipeline layout!");
    }
    */
}

void QTDoughApplication::CreateComputeDescriptorSets() {
    for(int i = 0; i < computePassStack.size(); i++)
	{
		computePassStack[i]->CreateComputeDescriptorSets();
	}

    for (int i = 0; i < rayTracePassStack.size(); i++)
    {
        rayTracePassStack[i]->CreateComputeDescriptorSets();
    }
}

void QTDoughApplication::CreateShaderStorageBuffers() {

    for (int i = 0; i < computePassStack.size(); i++)
    {
        computePassStack[i]->CreateShaderStorageBuffers();
    }

    for (int i = 0; i < rayTracePassStack.size(); i++)
    {
        rayTracePassStack[i]->CreateShaderStorageBuffers();
    }
}

void QTDoughApplication::CreateComputePipeline() {

    for(int i = 0; i < computePassStack.size(); i++)
	{
		computePassStack[i]->CreateComputePipeline();
	}

    for (int i = 0; i < rayTracePassStack.size(); i++)
    {
        rayTracePassStack[i]->CreateComputePipeline();
    }
	
}

void QTDoughApplication::CreateComputeDescriptorSetLayout() {

    for(int i = 0; i < computePassStack.size(); i++)
    {
        computePassStack[i]->CreateComputeDescriptorSetLayout();
	}
    
	for (int i = 0; i < rayTracePassStack.size(); i++)
	{
		rayTracePassStack[i]->CreateComputeDescriptorSetLayout();
	}
}


void QTDoughApplication::CreateComputeCommandBuffers() {
    computeCommandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = _commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = (uint32_t)computeCommandBuffers.size();

    if (vkAllocateCommandBuffers(_logicalDevice, &allocInfo, computeCommandBuffers.data()) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate compute command buffers!");
    }

    std::cout << "Allocated compute command buffers" << std::endl;
}

void QTDoughApplication::CreatePhysicsCommandBuffer() {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = _commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    if (vkAllocateCommandBuffers(_logicalDevice, &allocInfo, &_physicsCommandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate physics command buffer!");
    }
}

void QTDoughApplication::CreateImages()
{

    for (int i = 0; i < renderPassStack.size(); i++)
    {
        renderPassStack[i]->CreateImages();
    }

    for (int i = 0; i < computePassStack.size(); i++)
	{
		computePassStack[i]->CreateImages();
	}

    for (int i = 0; i < rayTracePassStack.size(); i++)
	{
		rayTracePassStack[i]->CreateImages();
	}

}

void QTDoughApplication::GetMeshDataAllObjects()
{
    //Loop over all objects
    for (int i = 0; i < NUM_OBJECTS; i++)
    {
        //unigmaRenderingObjects[i].RenderObjectToUnigma(*this, renderObjects[i], unigmaRenderingObjects[i], CameraMain);

        unigmaRenderingObjects[i].isRendering = false;
        if (strstr(renderObjects[i].name, "Camera") != nullptr)
            continue;
        if (strstr(renderObjects[i].name, "Light") != nullptr)
            continue;
        if (strstr(renderObjects[i].name, "Cube") != nullptr)
        {
            unigmaRenderingObjects[i].isRendering = true;
            unigmaRenderingObjects[i].RenderObjectToUnigma(*this, renderObjects[i], unigmaRenderingObjects[i], CameraMain);
        }

    }
}

void QTDoughApplication::CreateVertexBuffer()
{
    for (int i = 0; i < NUM_OBJECTS; i++)
    {
        if (unigmaRenderingObjects[i].isRendering)
            unigmaRenderingObjects[i].CreateVertexBuffer(*this);
    }
}

void QTDoughApplication::CreateIndexBuffer()
{
    for (int i = 0; i < NUM_OBJECTS; i++)
    {
        if (unigmaRenderingObjects[i].isRendering)
            unigmaRenderingObjects[i].CreateIndexBuffer(*this);
    }
}


void QTDoughApplication::LoadModel()
{
    unigmaRenderingObjects[0].LoadModel(unigmaRenderingObjects[0]._mesh);
}

void QTDoughApplication::CreateDepthResources()
{
    VkFormat depthFormat = FindDepthFormat();

    CreateImage(swapChainExtent.width, swapChainExtent.height, depthFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, depthImage, depthImageMemory);
    depthImageView = CreateImageView(depthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);


}

void QTDoughApplication::CreateTextureSampler() {

    vkGetPhysicalDeviceProperties(_physicalDevice, &properties);

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;

    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

    samplerInfo.anisotropyEnable = VK_TRUE;
    samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;

    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;

    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 5.0f;

    if (vkCreateSampler(_logicalDevice, &samplerInfo, nullptr, &textureSampler) != VK_SUCCESS) {
        throw std::runtime_error("failed to create texture sampler!");
    }

}



void QTDoughApplication::TransitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout) {
    VkCommandBuffer commandBuffer = BeginSingleTimeCommands();

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;

    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else {
        throw std::invalid_argument("unsupported layout transition!");
    }

    vkCmdPipelineBarrier(
        commandBuffer,
        sourceStage, destinationStage,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier
    );

    EndSingleTimeCommands(commandBuffer);
}

VkCommandBuffer QTDoughApplication::BeginSingleTimeCommands() {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = _commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(_logicalDevice, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    return commandBuffer;
}

void QTDoughApplication::CreateImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory, VkSampleCountFlagBits numSamples) {
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = tiling;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage;
    imageInfo.samples = numSamples;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(_logicalDevice, &imageInfo, nullptr, &image) != VK_SUCCESS) {
        throw std::runtime_error("failed to create image!");
    }

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(_logicalDevice, image, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = FindMemoryType(memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(_logicalDevice, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate image memory!");
    }

    vkBindImageMemory(_logicalDevice, image, imageMemory, 0);
}

void QTDoughApplication::CreateImages3D(uint32_t width, uint32_t height, uint32_t depth, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory)
{
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_3D;                         
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = depth;                                 
    imageInfo.mipLevels = 5;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = tiling;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(_logicalDevice, &imageInfo, nullptr, &image) != VK_SUCCESS) {
        throw std::runtime_error("failed to create 3D image!");
    }

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(_logicalDevice, image, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = FindMemoryType(memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(_logicalDevice, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate 3D image memory!");
    }

    vkBindImageMemory(_logicalDevice, image, imageMemory, 0);
}


void QTDoughApplication::EndSingleTimeCommands(VkCommandBuffer commandBuffer)
{
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(_vkGraphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(_vkGraphicsQueue);

    vkFreeCommandBuffers(_logicalDevice, _commandPool, 1, &commandBuffer);
}

void QTDoughApplication::EndSingleTimeCommands(VkCommandBuffer commandBuffer, VkFence fence)
{
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(_vkGraphicsQueue, 1, &submitInfo, fence);
    vkQueueWaitIdle(_vkGraphicsQueue);

    vkFreeCommandBuffers(_logicalDevice, _commandPool, 1, &commandBuffer);
}

void QTDoughApplication::EndSingleTimeCommandsAsync(uint32_t currentFrame, VkCommandBuffer commandBuffer, std::function<void()> callback)
{
    VkFence& fence = computeFences[currentFrame];

    vkResetFences(_logicalDevice, 1, &fence);
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    VkResult submitResult = vkQueueSubmit(_vkComputeQueue, 1, &submitInfo, fence);
    if (submitResult != VK_SUCCESS) {
        std::cerr << "Failed to submit command buffer!" << std::endl;
        return;
    }

    // Move command buffer to heap to ensure it survives the thread
    VkCommandBuffer* commandBufferCopy = new VkCommandBuffer(commandBuffer);

    std::thread([this, &fence, commandBufferCopy, callback]() {
        vkWaitForFences(_logicalDevice, 1, &fence, VK_TRUE, UINT64_MAX);

        vkFreeCommandBuffers(_logicalDevice, _commandPool, 1, commandBufferCopy);
        delete commandBufferCopy;

        callback();
        }).detach();
}

void QTDoughApplication::ReadbackBufferData(VkBuffer srcBuffer, VkDeviceSize size, void* pDstData, VkDeviceSize srcOffset) {
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    CreateBuffer(
        size,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingBuffer,
        stagingBufferMemory
    );

    VkCommandBuffer commandBuffer = BeginSingleTimeCommands();

    VkBufferMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.buffer = srcBuffer;
    barrier.offset = srcOffset;
    barrier.size = size;

    vkCmdPipelineBarrier(
        commandBuffer,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0, 0, nullptr, 1, &barrier, 0, nullptr
    );

    VkBufferCopy copyRegion{};
    copyRegion.srcOffset = srcOffset;
    copyRegion.dstOffset = 0;
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, srcBuffer, stagingBuffer, 1, &copyRegion);

    EndSingleTimeCommands(commandBuffer);

    void* mappedData;
    vkMapMemory(_logicalDevice, stagingBufferMemory, 0, size, 0, &mappedData);
    memcpy(pDstData, mappedData, static_cast<size_t>(size));
    vkUnmapMemory(_logicalDevice, stagingBufferMemory);

    vkDestroyBuffer(_logicalDevice, stagingBuffer, nullptr);
    vkFreeMemory(_logicalDevice, stagingBufferMemory, nullptr);
}


void QTDoughApplication::CreateTextureImage()
{
    /*
    //std::string path = AssetsPath + unigmaRenderingObjects[0]._material.textures[0].TEXTURE_PATH;
    std::string path = AssetsPath + "Textures/viking_room.png";
    int texWidth, texHeight, texChannels;
    stbi_uc* pixels = stbi_load(path.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
    VkDeviceSize imageSize = texWidth * texHeight * 4;

    if (!pixels) {
        throw std::runtime_error("failed to load texture image!");
    }

    CreateBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

    void* data;
    vkMapMemory(_logicalDevice, stagingBufferMemory, 0, imageSize, 0, &data);
    memcpy(data, pixels, static_cast<size_t>(imageSize));
    vkUnmapMemory(_logicalDevice, stagingBufferMemory);

    stbi_image_free(pixels);

    CreateImage(texWidth, texHeight, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, textureImage, textureImageMemory);

    TransitionImageLayout(textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    CopyBufferToImage(stagingBuffer, textureImage, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));

    TransitionImageLayout(textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    vkDestroyBuffer(_logicalDevice, stagingBuffer, nullptr);
    vkFreeMemory(_logicalDevice, stagingBufferMemory, nullptr);
    */
}

void QTDoughApplication::CreateDescriptorSets()
{
    for (int i = 0; i < renderPassStack.size(); i++)
    {
        renderPassStack[i]->CreateDescriptorSets();
    }

    for (int i = 0; i < NUM_OBJECTS; i++)
    {
        if (unigmaRenderingObjects[i].isRendering)
            unigmaRenderingObjects[i].CreateDescriptorSets(*this);
    }
}


void QTDoughApplication::CreateUniformBuffers()
{
    std::cout << "Creating Uniform Buffers" << std::endl;
    for (int i = 0; i < renderPassStack.size(); i++)
    {
        renderPassStack[i]->CreateUniformBuffers();
    }

    for (int i = 0; i < computePassStack.size(); i++)
	{
		computePassStack[i]->CreateUniformBuffers();
	}

    for (int i = 0; i < rayTracePassStack.size(); i++)
    {
        rayTracePassStack[i]->CreateUniformBuffers();
    }

    for (int i = 0; i < NUM_OBJECTS; i++)
    {
        if (unigmaRenderingObjects[i].isRendering)
            unigmaRenderingObjects[i].CreateUniformBuffers(*this);
    }
    std::cout << "Created Uniform Buffers" << std::endl;
}

void QTDoughApplication::CreateDescriptorSetLayout()
{

    for (int i = 0; i < renderPassStack.size(); i++)
    {
        renderPassStack[i]->CreateDescriptorSetLayout();
    }

    for (int i = 0; i < NUM_OBJECTS; i++)
    {
        if (unigmaRenderingObjects[i].isRendering)
            unigmaRenderingObjects[i].CreateDescriptorSetLayout(*this);
    }
}

void QTDoughApplication::CreateDescriptorPool()
{
    std::array<VkDescriptorPoolSize, 2> poolSizes{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

    poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSizes[1].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT) * 2;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 2;
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

    if (vkCreateDescriptorPool(_logicalDevice, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor pool!");
    }

    for (int i = 0; i < renderPassStack.size(); i++)
    {
        renderPassStack[i]->CreateDescriptorPool();
    }
    //Create descriptor pool for compute pass.
    for (int i = 0; i < computePassStack.size(); i++)
    {
        computePassStack[i]->CreateDescriptorPool();
    }

    for (int i = 0; i < rayTracePassStack.size(); i++)
	{
		rayTracePassStack[i]->CreateDescriptorPool();
	}

    for (int i = 0; i < NUM_OBJECTS; i++)
    {
        if (unigmaRenderingObjects[i].isRendering)
            unigmaRenderingObjects[i].CreateDescriptorPool(*this);
    }
}


void QTDoughApplication::CreateGlobalDescriptorSetLayout()
{

    std::cout << "Creating global descriptor set layout" << std::endl;
    CreateGlobalSamplers(MAX_BINDLESS_IMAGES);

    // Binding for sampled images
    VkDescriptorSetLayoutBinding sampledImageBinding{};
    sampledImageBinding.binding = 0;
    sampledImageBinding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    sampledImageBinding.descriptorCount = MAX_BINDLESS_IMAGES;
    sampledImageBinding.stageFlags = VK_SHADER_STAGE_ALL;
    sampledImageBinding.pImmutableSamplers = nullptr;

    // Binding for samplers
    VkDescriptorSetLayoutBinding samplerBinding{};
    samplerBinding.binding = 1;
    samplerBinding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
    samplerBinding.descriptorCount = MAX_BINDLESS_IMAGES;
    samplerBinding.stageFlags = VK_SHADER_STAGE_ALL;
    samplerBinding.pImmutableSamplers = globalSamplers.data();

    //Uniform buffer.
    VkDescriptorSetLayoutBinding uboLayoutBinding{};
    uboLayoutBinding.binding = 2;
    uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboLayoutBinding.descriptorCount = 1;
    uboLayoutBinding.stageFlags = VK_SHADER_STAGE_ALL;
    uboLayoutBinding.pImmutableSamplers = nullptr;

    // Storage images the compute shader writes to (u1[])
    VkDescriptorSetLayoutBinding imgStorage{};
    imgStorage.binding = 3;
    imgStorage.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    imgStorage.descriptorCount = MAX_BINDLESS_IMAGES;
    imgStorage.stageFlags = VK_SHADER_STAGE_ALL;

    //3D texture sampler.
    VkDescriptorSetLayoutBinding sampled3DImageBinding{};
    sampled3DImageBinding.binding = 4;
    sampled3DImageBinding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    sampled3DImageBinding.descriptorCount = MAX_BINDLESS_IMAGES;
    sampled3DImageBinding.stageFlags = VK_SHADER_STAGE_ALL;
    sampled3DImageBinding.pImmutableSamplers = nullptr;

    // 3D Textures for reading and writing.
    VkDescriptorSetLayoutBinding img3DStorage{};
    img3DStorage.binding = 5;
    img3DStorage.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    img3DStorage.descriptorCount = MAX_BINDLESS_IMAGES;
    img3DStorage.stageFlags = VK_SHADER_STAGE_ALL;

    VkDescriptorSetLayoutBinding globalObjsLayoutBinding{};
    globalObjsLayoutBinding.binding = 6;
    globalObjsLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    globalObjsLayoutBinding.descriptorCount = 1;
    globalObjsLayoutBinding.stageFlags = VK_SHADER_STAGE_ALL;
    globalObjsLayoutBinding.pImmutableSamplers = nullptr;

    // We now have two bindings
    std::array<VkDescriptorSetLayoutBinding, 7> bindings = { sampledImageBinding, samplerBinding, uboLayoutBinding, imgStorage, sampled3DImageBinding, img3DStorage, globalObjsLayoutBinding };

    // For descriptor indexing flags
    VkDescriptorBindingFlags bindingFlags[7] = {
        VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
        VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
        0,
        VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
        VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
        VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
        0
    };


    VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsInfo{};
    bindingFlagsInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
    bindingFlagsInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    bindingFlagsInfo.pBindingFlags = bindingFlags;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.pBindings = bindings.data();
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pNext = &bindingFlagsInfo;

    // Enable update-after-bind for descriptor indexing
    layoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;

    if (vkCreateDescriptorSetLayout(_logicalDevice, &layoutInfo, nullptr, &globalDescriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create global descriptor set layout!");
    }

    std::cout << "Created global descriptor set layout" << std::endl;
}

void QTDoughApplication::CreateGlobalDescriptorPool()
{
    std::cout << "Creating Descriptor Pool" << std::endl;
    std::array<VkDescriptorPoolSize, 7> poolSizes{};

    // Sampled 2D textures
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    poolSizes[0].descriptorCount = MAX_BINDLESS_IMAGES;

    // Samplers
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_SAMPLER;
    poolSizes[1].descriptorCount = MAX_BINDLESS_IMAGES;

    // Uniform buffers
    poolSizes[2].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[2].descriptorCount = MAX_FRAMES_IN_FLIGHT;

    // Storage 2D images
    poolSizes[3].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    poolSizes[3].descriptorCount = MAX_BINDLESS_IMAGES;

    // Sampled 3D textures
    poolSizes[4].type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    poolSizes[4].descriptorCount = MAX_BINDLESS_IMAGES;

    // Storage 3D images
    poolSizes[5].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    poolSizes[5].descriptorCount = MAX_BINDLESS_IMAGES;

    // Game global objects buffer
    poolSizes[6].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSizes[6].descriptorCount = MAX_FRAMES_IN_FLIGHT;


    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = MAX_FRAMES_IN_FLIGHT;

    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;

    if (vkCreateDescriptorPool(_logicalDevice, &poolInfo, nullptr, &globalDescriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create global descriptor pool!");
    }

    std::cout << "Created Descriptor Pool" << std::endl;

}

void QTDoughApplication::CreateGlobalUniformBuffers() {
    globalUniformBufferObject.resize(MAX_FRAMES_IN_FLIGHT);
    globalUniformBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);

    VkDeviceSize bufferSize = sizeof(GlobalUniformBufferObject);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        CreateBuffer(bufferSize,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            globalUniformBufferObject[i],
            globalUniformBuffersMemory[i]);
    }

    globalShaderGameObjs.resize(MAX_FRAMES_IN_FLIGHT);
    globalShaderGameObjsMemory.resize(MAX_FRAMES_IN_FLIGHT);

    bufferSizeGamObjs = NUM_OBJECTS * sizeof(GameObjectShaderData);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        CreateBuffer(bufferSizeGamObjs,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            globalShaderGameObjs[i],
            globalShaderGameObjsMemory[i]);
    }

}

void QTDoughApplication::CreateGlobalDescriptorSet()
{
    std::cout << "Creating Global Descriptor Set" << std::endl;
    globalDescriptorSets.resize(MAX_FRAMES_IN_FLIGHT);

    std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, globalDescriptorSetLayout);

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = globalDescriptorPool;
    allocInfo.descriptorSetCount = MAX_FRAMES_IN_FLIGHT;
    allocInfo.pSetLayouts = layouts.data();  // 2 layouts for 2 descriptor sets

    if (vkAllocateDescriptorSets(_logicalDevice, &allocInfo, globalDescriptorSets.data()) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate global descriptor set!");
    }


    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {

        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = globalUniformBufferObject[i];
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(GlobalUniformBufferObject);

        VkWriteDescriptorSet uboWrite{};
        uboWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        uboWrite.dstSet = globalDescriptorSets[i];
        uboWrite.dstBinding = 2;
        uboWrite.dstArrayElement = 0;
        uboWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        uboWrite.descriptorCount = 1;
        uboWrite.pBufferInfo = &bufferInfo;

        VkDescriptorBufferInfo ssboBufferInfo{};
        ssboBufferInfo.buffer = globalShaderGameObjs[i];
        ssboBufferInfo.offset = 0;
        ssboBufferInfo.range = VK_WHOLE_SIZE;

        VkWriteDescriptorSet ssboWrite{};
        ssboWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        ssboWrite.dstSet = globalDescriptorSets[i];
        ssboWrite.dstBinding = 6; 
        ssboWrite.dstArrayElement = 0;
        ssboWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        ssboWrite.descriptorCount = 1;
        ssboWrite.pBufferInfo = &ssboBufferInfo;

        std::vector<VkWriteDescriptorSet> writes = { uboWrite, ssboWrite };
        vkUpdateDescriptorSets(_logicalDevice,
            static_cast<uint32_t>(writes.size()),
            writes.data(),
            0,
            nullptr);
    }

    std::cout << "Created Global Descriptor Set" << std::endl;
}


void QTDoughApplication::UpdateGlobalDescriptorSet()
{
    std::vector<UnigmaTexture> keys;
    int index = 0;
    for (auto& pair : textures) {
        pair.second.ID = index;
        keys.push_back(pair.second);
        index++;
    }

    auto sizeTextures = keys.size();
    std::vector<VkDescriptorImageInfo> imageInfos(sizeTextures);

    for (size_t i = 0; i < sizeTextures; ++i) {
        imageInfos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfos[i].imageView = keys[i].u_imageView;
        imageInfos[i].sampler = VK_NULL_HANDLE; // Samplers handled separately
    }

    //Writable textures
    std::vector<VkDescriptorImageInfo> storageInfos(sizeTextures);
    for (size_t i = 0; i < sizeTextures; ++i) {
        storageInfos[i].imageLayout = VK_IMAGE_LAYOUT_GENERAL;   // UAV layout
        storageInfos[i].imageView = keys[i].u_imageView;
        storageInfos[i].sampler = VK_NULL_HANDLE;
    }

    //3D images
    std::vector<Unigma3DTexture> keys3D;
    for (auto& pair : textures3D) {
		keys3D.push_back(pair.second);
	}

    std::sort(keys3D.begin(), keys3D.end(), [](const Unigma3DTexture& a, const Unigma3DTexture& b) {
        return a.ID < b.ID;
        });

    auto sizeTextures3D = keys3D.size();
    std::vector<VkDescriptorImageInfo> imageInfos3D(sizeTextures3D);
    for (size_t i = 0; i < sizeTextures3D; ++i) {
		imageInfos3D[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageInfos3D[i].imageView = keys3D[i].u_imageView;
		imageInfos3D[i].sampler = VK_NULL_HANDLE; // Samplers handled separately
	}

    //Writable 3D textures
    std::vector<VkDescriptorImageInfo> storageInfos3D(sizeTextures3D);
    for (size_t i = 0; i < sizeTextures3D; ++i) {
		storageInfos3D[i].imageLayout = VK_IMAGE_LAYOUT_GENERAL;   // UAV layout
		storageInfos3D[i].imageView = keys3D[i].u_imageView;
		storageInfos3D[i].sampler = VK_NULL_HANDLE;
	}


    VkWriteDescriptorSet imageWrite{};
    imageWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    imageWrite.dstSet = globalDescriptorSets[currentFrame];
    imageWrite.dstBinding = 0; // Binding for sampled images
    imageWrite.dstArrayElement = 0;
    imageWrite.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    imageWrite.descriptorCount = static_cast<uint32_t>(imageInfos.size());
    imageWrite.pImageInfo = imageInfos.data();

    VkWriteDescriptorSet writeStorage{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
    writeStorage.dstSet = globalDescriptorSets[currentFrame];
    writeStorage.dstBinding = 3;
    writeStorage.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    writeStorage.descriptorCount = static_cast<uint32_t>(storageInfos.size());
    writeStorage.pImageInfo = storageInfos.data();

    VkWriteDescriptorSet imageWrite3D{};
    imageWrite3D.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    imageWrite3D.dstSet = globalDescriptorSets[currentFrame];
    imageWrite3D.dstBinding = 4; // Binding for sampled 3D images
    imageWrite3D.dstArrayElement = 0;
    imageWrite3D.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    imageWrite3D.descriptorCount = static_cast<uint32_t>(imageInfos3D.size());
    imageWrite3D.pImageInfo = imageInfos3D.data();


    VkWriteDescriptorSet writeStorage3D{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
    writeStorage3D.dstSet = globalDescriptorSets[currentFrame];
    writeStorage3D.dstBinding = 5;
    writeStorage3D.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    writeStorage3D.descriptorCount = static_cast<uint32_t>(storageInfos3D.size());
    writeStorage3D.pImageInfo = storageInfos3D.data();

    std::array<VkWriteDescriptorSet, 4> writes = { imageWrite, writeStorage, imageWrite3D, writeStorage3D };

    vkUpdateDescriptorSets(_logicalDevice,static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);

    auto timeNow = std::chrono::high_resolution_clock::now();
    auto duration = timeSinceApplication;
    float timeInSeconds = std::chrono::duration<float>(timeNow - duration).count();


    float deltaTime = std::chrono::duration<float>(currentTime - previousTime).count(); //time between frames.

    GlobalUniformBufferObject globalUBO{};
    globalUBO.deltaTime = deltaTime;
    globalUBO.time = timeInSeconds;

    for (int i = 0; i < lights.size(); i++)
    {
        globalUBO.light[i].direction = lights[i]->direction;
        globalUBO.light[i].emission = lights[i]->emission;
    }


    
    void* data;
    vkMapMemory(_logicalDevice, globalUniformBuffersMemory[currentFrame], 0,
        sizeof(GlobalUniformBufferObject), 0, &data);
    memcpy(data, &globalUBO, sizeof(GlobalUniformBufferObject));
    vkUnmapMemory(_logicalDevice, globalUniformBuffersMemory[currentFrame]);

    // currentTime is now advanced in RunMainGameLoop.

    void* dataObjs;
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkMapMemory(_logicalDevice, globalShaderGameObjsMemory[i], 0, bufferSizeGamObjs, 0, &dataObjs);
        memcpy(dataObjs, gameObjectShaderDataArray, bufferSizeGamObjs);
        vkUnmapMemory(_logicalDevice, globalShaderGameObjsMemory[i]);
    }
    
}


void QTDoughApplication::LoadTexture(const std::string& filename) {
    if (textures.count(filename) > 0) {
        return;
    }

    UnigmaTexture texture;

    int texWidth, texHeight, texChannels;
    stbi_uc* pixels = stbi_load(filename.c_str(), &texWidth, &texHeight, &texChannels, 4);
    if (!pixels) {
        throw std::runtime_error("failed to load texture image!");
    }

    VkDeviceSize imageSize = texWidth * texHeight * 4;

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    CreateBuffer(
        imageSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingBuffer,
        stagingBufferMemory
    );

    void* data;
    vkMapMemory(_logicalDevice, stagingBufferMemory, 0, imageSize, 0, &data);
    memcpy(data, pixels, static_cast<size_t>(imageSize));
    vkUnmapMemory(_logicalDevice, stagingBufferMemory);

    stbi_image_free(pixels);

    CreateImage(
        texWidth, texHeight,
        VK_FORMAT_R8G8B8A8_SRGB,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        texture.u_image,
        texture.u_imageMemory
    );


    // Initial layout transition
    TransitionImageLayout(
        texture.u_image,
        VK_FORMAT_R8G8B8A8_SRGB,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
    );
    std::cout << "Texture Loaded Name: " << filename << std::endl;
    CopyBufferToImage(stagingBuffer, texture.u_image, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));

    // Final layout transition
    TransitionImageLayout(
        texture.u_image,
        VK_FORMAT_R8G8B8A8_SRGB,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    );

    vkDestroyBuffer(_logicalDevice, stagingBuffer, nullptr);
    vkFreeMemory(_logicalDevice, stagingBufferMemory, nullptr);

    texture.u_imageView = CreateImageView(texture.u_image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT);

    //Remove the path and .png from the filename.
    std::vector<std::string> tokens;
    std::istringstream tokenStream(filename);
    std::string token;
    while (std::getline(tokenStream, token, '/')) {
        tokens.push_back(token);
    }

    // Safely remove .png if present
    if (!tokens.empty()) {
        std::string& lastToken = tokens.back();
        if (lastToken.size() >= 4 && lastToken.compare(lastToken.size() - 4, 4, ".png") == 0) {
            lastToken.erase(lastToken.end() - 4, lastToken.end());
        }
        std::cout << "Texture Loaded Name Final Token: " << tokens.back() << std::endl;
    }

    //Pick the removed .png and path to get name, else use the full path.
    std::string textureName = tokens.empty() ? filename : tokens.back();
    textures.insert({ textureName, texture });
}


void QTDoughApplication::CreateSyncObjects()
{
    computeFences.resize(MAX_FRAMES_IN_FLIGHT);
    //Create fences for compute.
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkFenceCreateInfo fenceInfo{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        vkCreateFence(_logicalDevice, &fenceInfo, nullptr, &computeFences[i]);
    }

    //Resize to fit the amount of possible frames in flight.
    _imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    _renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    _inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

    computeInFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
    computeFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);


    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT; //First frame doesn't wait.

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (vkCreateSemaphore(_logicalDevice, &semaphoreInfo, nullptr, &_imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(_logicalDevice, &semaphoreInfo, nullptr, &_renderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(_logicalDevice, &fenceInfo, nullptr, &_inFlightFences[i]) != VK_SUCCESS) {

            throw std::runtime_error("failed to create synchronization objects for a frame!");
        }
        if (vkCreateSemaphore(_logicalDevice, &semaphoreInfo, nullptr, &computeFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(_logicalDevice, &fenceInfo, nullptr, &computeInFlightFences[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create compute synchronization objects for a frame!");
        }
    }

    if (vkCreateFence(_logicalDevice, &fenceInfo, nullptr, &_physicsFence) != VK_SUCCESS) {
        throw std::runtime_error("failed to create physics fence!");
    }
}


void QTDoughApplication::RecordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex)
{

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("failed to begin recording command buffer!");
    }

    {
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = frameOutput;
        barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        vkCmdPipelineBarrier(commandBuffer,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            0, 0, nullptr, 0, nullptr, 1, &barrier);
    }

    // Render all your passes
    RenderPasses(commandBuffer, imageIndex);

    // Now transition the depth image from DEPTH_STENCIL_ATTACHMENT_OPTIMAL to SHADER_READ_ONLY_OPTIMAL
    {
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = depthImage;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            0,
            0, nullptr,
            0, nullptr,
            1, &barrier
        );
    }

    // Blit frameOutput into the swapchain image. Always runs, regardless of which UI passes
    // (or none) are registered — the engine owns presentation.
    {
        VkImageMemoryBarrier preBlit[2] = {};
        preBlit[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        preBlit[0].oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        preBlit[0].newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        preBlit[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        preBlit[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        preBlit[0].image = frameOutput;
        preBlit[0].subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        preBlit[0].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        preBlit[0].dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

        preBlit[1].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        preBlit[1].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        preBlit[1].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        preBlit[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        preBlit[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        preBlit[1].image = swapChainImages[imageIndex];
        preBlit[1].subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        preBlit[1].srcAccessMask = 0;
        preBlit[1].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        vkCmdPipelineBarrier(commandBuffer,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            0, 0, nullptr, 0, nullptr, 2, preBlit);

        VkImageBlit region{};
        region.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
        region.srcOffsets[0] = { 0, 0, 0 };
        region.srcOffsets[1] = { (int32_t)swapChainExtent.width, (int32_t)swapChainExtent.height, 1 };
        region.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
        region.dstOffsets[0] = { 0, 0, 0 };
        region.dstOffsets[1] = { (int32_t)swapChainExtent.width, (int32_t)swapChainExtent.height, 1 };

        vkCmdBlitImage(commandBuffer,
            frameOutput, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            swapChainImages[imageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1, &region, VK_FILTER_LINEAR);

        // Put swap into COLOR_ATTACHMENT so the recorder's internal barrier (which expects that
        // oldLayout) keeps working.
        VkImageMemoryBarrier postBlit{};
        postBlit.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        postBlit.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        postBlit.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        postBlit.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        postBlit.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        postBlit.image = swapChainImages[imageIndex];
        postBlit.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        postBlit.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        postBlit.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        vkCmdPipelineBarrier(commandBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            0, 0, nullptr, 0, nullptr, 1, &postBlit);
    }

    if (recorder && recorder->IsRecording()) {
        recorder->CmdCopySwapImageToStaging(commandBuffer,
            swapChainImages[imageIndex],
            currentFrame,
            swapChainExtent.width,
            swapChainExtent.height);
    }

    // Final swap transition for present.
    {
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = swapChainImages[imageIndex];
        barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        barrier.dstAccessMask = 0;

        vkCmdPipelineBarrier(commandBuffer,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            0, 0, nullptr, 0, nullptr, 1, &barrier);
    }

    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to record command buffer!");
    }
}


void QTDoughApplication::RecordComputeCommandBuffer(VkCommandBuffer commandBuffer) {
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("failed to begin recording compute command buffer!");
    }

    DispatchPasses(commandBuffer, currentFrame);

    /**
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _computePipeline);

    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _computePipelineLayout, 0, 1, &computeDescriptorSets[currentFrame], 0, nullptr);

    vkCmdDispatch(commandBuffer, 1000 / 256, 1, 1);
    */

    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to record compute command buffer!");
    }

}


void QTDoughApplication::CameraToBlender()
{
    if (renderObjectsMap.count("OBCamera") > 0)
    {
        CameraMain._transform = renderObjectsMap["OBCamera"]->transformMatrix;
    }
}

void QTDoughApplication::RenderPasses(VkCommandBuffer commandBuffer, uint32_t imageIndex)
{
    for (int i = 0; i < renderPassStack.size(); i++)
    {
        renderPassStack[i]->UpdateUniformBuffer(imageIndex, currentFrame);
        //renderPassStack[i]->UpdateUniformBufferObjects(commandBuffer, imageIndex, currentFrame, nullptr, &CameraMain);
    }

    for (int i = 0; i < renderPassStack.size(); i++)
    {
        if (renderPassStack[i]->PassName == "QuantaSpherePass")
        {
            if (editorState.viewMode == ViewModes::Quanta)
                renderPassStack[i]->Render(commandBuffer, imageIndex, currentFrame, nullptr, &CameraMain);
            continue;
        }
        renderPassStack[i]->Render(commandBuffer, imageIndex, currentFrame, nullptr, &CameraMain);
    }

    //Compute
    for (int i = 0; i < computePassStack.size(); i++)
	{
		computePassStack[i]->UpdateUniformBuffer(commandBuffer, imageIndex, currentFrame, CameraMain);
	}

    for (int i = 0; i < rayTracePassStack.size(); i++)
    {
        rayTracePassStack[i]->UpdateUniformBuffer(commandBuffer, imageIndex, currentFrame, CameraMain);
    }

}

void QTDoughApplication::RenderObjects(VkCommandBuffer commandBuffer, uint32_t imageIndex)
{
    for (int i = 0; i < NUM_OBJECTS; i++)
    {
        if (unigmaRenderingObjects[i].isRendering)
        {
            //print out is rendering.
            std::cout << "Rendering Object: " << unigmaRenderingObjects[i].isRendering << std::endl;
            //unigmaRenderingObjects[i].UpdateUniformBuffer(*this, currentFrame, unigmaRenderingObjects[i], CameraMain);
            //unigmaRenderingObjects[i].Render(*this, commandBuffer, imageIndex, currentFrame);
        }
    }
}

void QTDoughApplication::DispatchPasses(VkCommandBuffer commandBuffer, uint32_t imageIndex)
{
    for (int i = 0; i < computePassStack.size(); i++)
    {
        //Check for sdfpass.
        if (computePassStack[i]->PassName == "SDFPass")
        {
            if (editorState.viewMode == ViewModes::SDF || editorState.viewMode == ViewModes::Material || editorState.viewMode == ViewModes::MaterialBrush)
                computePassStack[i]->Dispatch(commandBuffer, imageIndex);
            continue;
        }
        computePassStack[i]->Dispatch(commandBuffer, imageIndex);
    }

    for (int i = 0; i < rayTracePassStack.size(); i++)
    {
        rayTracePassStack[i]->Dispatch(commandBuffer, imageIndex);
    }
}

void QTDoughApplication::CreateCommandBuffers()
{
    _commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = _commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = (uint32_t)_commandBuffers.size();


    if (vkAllocateCommandBuffers(_logicalDevice, &allocInfo, _commandBuffers.data()) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate command buffers!");
    }

    std::cout << "Created Command Buffer" << std::endl;
}

void QTDoughApplication::CreateCommandPool()
{
    std::cout << "Creating Command Pool" << std::endl;
    QueueFamilyIndices queueFamilyIndices = FindQueueFamilies(_physicalDevice);
    _commandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    _commandPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    _commandPoolInfo.queueFamilyIndex = queueFamilyIndices.graphicsAndComputeFamily.value();

    if (vkCreateCommandPool(_logicalDevice , &_commandPoolInfo, nullptr, &_commandPool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create command pool!");
    }

    std::cout << "Created Command Pool" << std::endl;
}

void QTDoughApplication::CreateRenderPass()
{
    std::cout << "Creating Render Passes" << std::endl;

    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = _swapChainImageFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;

    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = FindDepthFormat();
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthAttachmentRef{};
    depthAttachmentRef.attachment = 1;
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;


    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;


    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    std::array<VkAttachmentDescription, 2> attachments = { colorAttachment, depthAttachment };
    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;


    std::cout << "Subpasses created." << std::endl;

    if (vkCreateRenderPass(_logicalDevice, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
        throw std::runtime_error("failed to create render pass!");
    }

    std::cout << "Renderpasses created." << std::endl;
}


//Graphics pipeline has to be made per shader, and therefore per material using a different shader.
void QTDoughApplication::CreateGraphicsPipeline()
{
    for (int i = 0; i < renderPassStack.size(); i++)
    {
        renderPassStack[i]->CreateGraphicsPipeline();
    }

    for (int i = 0; i < NUM_OBJECTS; i++)
    {
        if(unigmaRenderingObjects[i].isRendering)
            unigmaRenderingObjects[i].CreateGraphicsPipeline(*this);
    }
}

VkShaderModule QTDoughApplication::CreateShaderModule(const std::vector<char>& code)
{
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(_logicalDevice, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        throw std::runtime_error("failed to create shader module!");
    }

    return shaderModule;
}

void QTDoughApplication::CreateTextureImageView() {
    textureImageView = CreateImageView(textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT);
}

void QTDoughApplication::CreateGlobalSamplers(uint32_t samplerCount)
{
    // Define common sampler settings
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.anisotropyEnable = VK_FALSE;      
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;   
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.minLod = 0.0f;       
    samplerInfo.maxLod = 5.0f;
    samplerInfo.mipLodBias = 0.0f;

    globalSamplers.resize(samplerCount);

    for (uint32_t i = 0; i < samplerCount; ++i) {
        if (vkCreateSampler(_logicalDevice, &samplerInfo, nullptr, &globalSamplers[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create sampler!");
        }
    }
}

VkImageView QTDoughApplication::CreateImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags) {
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = aspectFlags;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    VkImageView imageView;
    if (vkCreateImageView(_logicalDevice , &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
        throw std::runtime_error("failed to create texture image view!");
    }

    return imageView;
}

VkImageView QTDoughApplication::Create3DImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags) {
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_3D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = aspectFlags;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 5;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    VkImageView imageView;
    if (vkCreateImageView(_logicalDevice, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
        throw std::runtime_error("failed to create 3D image view!");
    }

    return imageView;
}


void QTDoughApplication::CreateImageViews() {
    swapChainImageViews.resize(swapChainImages.size());

    for (uint32_t i = 0; i < swapChainImages.size(); i++) {
        swapChainImageViews[i] = CreateImageView(swapChainImages[i], _swapChainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT);
    }
}

void QTDoughApplication::CreateMaterials() {
    for (int i = 0; i < renderPassStack.size(); i++)
    {
        renderPassStack[i]->CreateMaterials();
    }

    for (int i = 0; i < computePassStack.size(); i++)
    {
		computePassStack[i]->CreateMaterials();
	}

    for (int i = 0; i < rayTracePassStack.size(); i++)
    {
        rayTracePassStack[i]->CreateMaterials();
    }

    for (int i = 0; i < NUM_OBJECTS; i++)
    {
        if(unigmaRenderingObjects[i].isRendering)
            unigmaRenderingObjects[i].CreateGraphicsPipeline(*this);
    }
}

void QTDoughApplication::CreateSwapChain() {

    std::cout << "Creating swap chain" << std::endl;
    SwapChainSupportDetails swapChainSupport = QuerySwapChainSupport(_physicalDevice);

    VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
    VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
    VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;

    if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
        imageCount = swapChainSupport.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = _vkSurface;

    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    QueueFamilyIndices indices = FindQueueFamilies(_physicalDevice);
    uint32_t queueFamilyIndices[] = { indices.graphicsAndComputeFamily.value(), indices.presentFamily.value() };

    if (indices.graphicsAndComputeFamily != indices.presentFamily) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    }
    else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.queueFamilyIndexCount = 0; // Optional
        createInfo.pQueueFamilyIndices = nullptr; // Optional
    }

    createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;

    createInfo.oldSwapchain = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(_logicalDevice, &createInfo, nullptr, &_swapChain) != VK_SUCCESS) {
        throw std::runtime_error("failed to create swap chain!");
    }

    vkGetSwapchainImagesKHR(_logicalDevice , _swapChain, &imageCount, nullptr);
    std::cout << "Created swap chain" << std::endl;
    swapChainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(_logicalDevice, _swapChain, &imageCount, swapChainImages.data());

    _swapChainImageFormat = surfaceFormat.format;
    swapChainExtent = extent;
}

static bool DumpVkImageToPng(QTDoughApplication* app,
                             VkImage src, VkFormat fmt,
                             uint32_t w, uint32_t h,
                             VkImageLayout currentLayout,
                             const std::string& outPath)
{
    if (src == VK_NULL_HANDLE || w == 0 || h == 0)
        return false;

    uint32_t bpp = 0;
    bool isFloat = false;
    bool isBGRA = false;
    switch (fmt)
    {
    case VK_FORMAT_R32G32B32A32_SFLOAT: bpp = 16; isFloat = true; break;
    case VK_FORMAT_R8G8B8A8_UNORM:
    case VK_FORMAT_R8G8B8A8_SRGB:       bpp = 4; break;
    case VK_FORMAT_B8G8R8A8_UNORM:
    case VK_FORMAT_B8G8R8A8_SRGB:       bpp = 4; isBGRA = true; break;
    default: return false;
    }

    VkDeviceSize bufSize = (VkDeviceSize)w * h * bpp;
    VkBuffer staging;
    VkDeviceMemory stagingMem;
    app->CreateBuffer(bufSize,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        staging, stagingMem);

    VkCommandBuffer cmd = app->BeginSingleTimeCommands();

    VkImageMemoryBarrier toSrc{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
    toSrc.oldLayout = currentLayout;
    toSrc.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    toSrc.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toSrc.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toSrc.image = src;
    toSrc.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    toSrc.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    toSrc.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &toSrc);

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
    region.imageOffset = { 0, 0, 0 };
    region.imageExtent = { w, h, 1 };
    vkCmdCopyImageToBuffer(cmd, src,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        staging, 1, &region);

    VkImageMemoryBarrier toShader = toSrc;
    toShader.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    toShader.newLayout = currentLayout;
    toShader.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    toShader.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        0, 0, nullptr, 0, nullptr, 1, &toShader);

    app->EndSingleTimeCommands(cmd);

    void* mapped = nullptr;
    vkMapMemory(app->_logicalDevice, stagingMem, 0, bufSize, 0, &mapped);

    std::vector<uint8_t> pixels((size_t)w * h * 4);
    if (isFloat)
    {
        const float* src32 = static_cast<const float*>(mapped);
        for (size_t i = 0; i < (size_t)w * h; ++i)
        {
            for (int c = 0; c < 4; ++c)
            {
                float v = src32[i * 4 + c];
                if (v < 0.0f) v = 0.0f;
                if (v > 1.0f) v = 1.0f;
                pixels[i * 4 + c] = (uint8_t)(v * 255.0f + 0.5f);
            }
        }
    }
    else
    {
        const uint8_t* src8 = static_cast<const uint8_t*>(mapped);
        if (isBGRA)
        {
            for (size_t i = 0; i < (size_t)w * h; ++i)
            {
                pixels[i * 4 + 0] = src8[i * 4 + 2];
                pixels[i * 4 + 1] = src8[i * 4 + 1];
                pixels[i * 4 + 2] = src8[i * 4 + 0];
                pixels[i * 4 + 3] = src8[i * 4 + 3];
            }
        }
        else
        {
            std::memcpy(pixels.data(), src8, pixels.size());
        }
    }

    vkUnmapMemory(app->_logicalDevice, stagingMem);
    vkDestroyBuffer(app->_logicalDevice, staging, nullptr);
    vkFreeMemory(app->_logicalDevice, stagingMem, nullptr);

    int ok = stbi_write_png(outPath.c_str(), (int)w, (int)h, 4, pixels.data(), (int)(w * 4));
    return ok != 0;
}

static std::string SanitizeFilenameKey(const std::string& s)
{
    std::string out = s;
    for (char& c : out)
    {
        if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|')
            c = '_';
    }
    return out;
}

static std::string MakeCaptureFilename() {
    std::string dir = "C:/ProjectsSpeed/QTDEngine/Media/Captures";
    std::filesystem::create_directories(dir);

    char ts[32];
    std::time_t t = std::time(nullptr);
    std::strftime(ts, sizeof(ts), "%Y%m%d_%H%M%S", std::localtime(&t));

    return dir + "/capture_" + ts + ".mp4";
}


void QTDoughApplication::StartRecording(const char* path, uint32_t fps) {
    if (!recorder)
        recorder = new VideoRecorder(_logicalDevice, _physicalDevice, _vkGraphicsQueue, _commandPool);

    std::string outPath = (path && *path) ? std::string(path) : MakeCaptureFilename();
    std::cout << "[Recorder] Output path: " << outPath << std::endl;

    try {
        std::filesystem::create_directories(std::filesystem::path(outPath).parent_path());
    }
    catch (...) {
        std::cout << "Failed to create directories for recording path: " << outPath << std::endl;
    }

    recorder->Begin(outPath.c_str(),
        swapChainExtent.width,
        swapChainExtent.height,
        fps,
        _swapChainImageFormat,
        MAX_FRAMES_IN_FLIGHT);
}

void QTDoughApplication::StopRecording() {
    if (recorder) { recorder->End(); delete recorder; recorder = nullptr; }
    std::cout << "[Recorder] Recording stopped." << std::endl;
}

void QTDoughApplication::ExportPassOutputs()
{
    char ts[32];
    std::time_t t = std::time(nullptr);
    std::strftime(ts, sizeof(ts), "%Y%m%d_%H%M%S", std::localtime(&t));

    std::string folder = std::string("C:/ProjectsSpeed/QTDEngine/Media/Captures/passes_") + ts;
    std::filesystem::create_directories(folder);

    vkDeviceWaitIdle(_logicalDevice);

    int dumped = 0;
    int skipped = 0;

    auto dumpKey = [&](const std::string& key, VkFormat fmt)
    {
        auto it = textures.find(key);
        if (it == textures.end()) { ++skipped; return; }
        UnigmaTexture& tex = it->second;
        uint32_t w = tex.WIDTH > 0 ? tex.WIDTH : swapChainExtent.width;
        uint32_t h = tex.HEIGHT > 0 ? tex.HEIGHT : swapChainExtent.height;
        std::string path = folder + "/" + SanitizeFilenameKey(key) + ".png";
        if (DumpVkImageToPng(this, tex.u_image, fmt, w, h, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, path))
        {
            ++dumped;
            ConsoleLog("Dumped " + key + " -> " + path);
        }
        else
        {
            ++skipped;
            ConsoleLog("Failed to dump " + key);
        }
    };

    for (RayTracerPass* p : rayTracePassStack)
    {
        if (!p) continue;
        dumpKey(p->PassName, VK_FORMAT_R32G32B32A32_SFLOAT);
        for (const auto& n : p->PassNames)
            dumpKey(n, VK_FORMAT_R32G32B32A32_SFLOAT);
    }

    for (ComputePass* p : computePassStack)
    {
        if (!p) continue;
        dumpKey(p->PassName, VK_FORMAT_R32G32B32A32_SFLOAT);
        for (const auto& n : p->PassNames)
            dumpKey(n, VK_FORMAT_R32G32B32A32_SFLOAT);
    }

    for (RenderPassObject* p : renderPassStack)
    {
        if (!p) continue;
        VkFormat fmt = (dynamic_cast<AlbedoPass*>(p) || dynamic_cast<PositionPass*>(p))
            ? _swapChainImageFormat
            : VK_FORMAT_R32G32B32A32_SFLOAT;
        dumpKey(p->PassName, fmt);
        for (const auto& n : p->PassNames)
            dumpKey(n, fmt);
    }

    ConsoleLog("Exported " + std::to_string(dumped) + " pass outputs to " + folder
        + " (" + std::to_string(skipped) + " skipped)");
}

void QTDoughApplication::CreateInstance()
{

    //Create time.
    timeSinceApplication = std::chrono::steady_clock::now();

    unsigned int extensionCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
    VkApplicationInfo appInfo{};
    VkInstanceCreateInfo createInfo{};
    
    SDL_Vulkan_GetInstanceExtensions(QTSDLWindow, &extensionCount, NULL); //Get the count.
    const char** extensions = (const char**)malloc(sizeof(char*) * extensionCount); //Well justified ;p
    SDL_Vulkan_GetInstanceExtensions(QTSDLWindow, &extensionCount, extensions); //Get the extensions.

    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "QTDough";
    appInfo.applicationVersion = VK_MAKE_VERSION(0, 0, 1);
    appInfo.pEngineName = "QTDoughEngine";
    appInfo.engineVersion = VK_MAKE_VERSION(0, 0, 1);
    appInfo.apiVersion = VK_API_VERSION_1_3;

    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = extensionCount;
    createInfo.ppEnabledExtensionNames = extensions;
    createInfo.enabledLayerCount = 0;

    auto sdlExtensions = GetRequiredExtensions();

    createInfo.enabledExtensionCount = static_cast<uint32_t>(sdlExtensions.size());
    createInfo.ppEnabledExtensionNames = sdlExtensions.data();

    if (enableValidationLayers && !CheckValidationLayerSupport()) {
        throw std::runtime_error("validation layers requested, but not available!");
    }


    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
    if (enableValidationLayers) {
        debugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        debugCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        debugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        debugCreateInfo.pfnUserCallback = debugCallback;
        debugCreateInfo.pUserData = nullptr; // Optional

        createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
    }
    else {
        createInfo.enabledLayerCount = 0;

        createInfo.pNext = nullptr;
    }

    //Finally create the instance.
    VkResult result = vkCreateInstance(&createInfo, nullptr, &_vkInstance);
    if (vkCreateInstance(&createInfo, nullptr, &_vkInstance) != VK_SUCCESS) {
        throw std::runtime_error("failed to create instance!");
    }

}

bool QTDoughApplication::CheckValidationLayerSupport() {
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    for (const char* layerName : validationLayers) {
        bool layerFound = false;

        for (const auto& layerProperties : availableLayers) {
            if (strcmp(layerName, layerProperties.layerName) == 0) {
                layerFound = true;
                break;
            }
        }

        if (!layerFound) {
            return false;
        }
    }

    return true;
}

void QTDoughApplication::PickPhysicalDevice()
{
    
    uint32_t deviceCount = 0;

    vkEnumeratePhysicalDevices(_vkInstance, &deviceCount, nullptr);

    std::cout << "Device count: " << deviceCount << std::endl;

    if (deviceCount == 0) {
        throw std::runtime_error("failed to find GPUs with Vulkan support!");
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(_vkInstance, &deviceCount, devices.data());

    for (const auto& device : devices) {
        if (IsDeviceSuitable(device)) {
            _physicalDevice = device;
            break;
        }
    }

    if (_physicalDevice == VK_NULL_HANDLE) {
        throw std::runtime_error("failed to find a suitable GPU! Ensure you have GPU that supports Ray Tracing Acceleration");
    }

    msaaSamples = GetMaxUsableSampleCount();

    std::cout << "Max MSAA Samples: " << msaaSamples << std::endl;
    std::cout << "Suitable device found" << std::endl;
}

bool QTDoughApplication::IsDeviceSuitable(VkPhysicalDevice device) {

    //Set up device features / properties struct.
    VkPhysicalDeviceProperties deviceProperties;
    VkPhysicalDeviceFeatures deviceFeatures;
    vkGetPhysicalDeviceProperties(device, &deviceProperties);
    vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

    //After getting GPU features create list of features we want GPU to have to run program.

    //Must support raytracing.

    VkPhysicalDeviceAccelerationStructureFeaturesKHR accelFeatures{};
    accelFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;

    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rayTracingFeatures = {};
    rayTracingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
    rayTracingFeatures.pNext = &accelFeatures;


    VkPhysicalDeviceFeatures2 deviceFeatures2{};
    deviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    deviceFeatures2.pNext = &rayTracingFeatures;


    vkGetPhysicalDeviceFeatures2(device, &deviceFeatures2);

    
    //Get the features of this device. Does it support ray tracing?
    vkGetPhysicalDeviceFeatures2(device, &deviceFeatures2);
    QueueFamilyIndices indices = FindQueueFamilies(device);

    bool extensionsSupported = CheckDeviceExtensionSupport(device);

    bool swapChainAdequate = false;
    if (extensionsSupported) {
        SwapChainSupportDetails swapChainSupport = QuerySwapChainSupport(device);
        swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
    }


    uint32_t maxBoundSets = deviceProperties.limits.maxBoundDescriptorSets;
    uint32_t maxPerStageImages = deviceProperties.limits.maxPerStageDescriptorSampledImages;
    uint32_t maxTotalImages = deviceProperties.limits.maxDescriptorSetSampledImages;
    uint32_t max3DTextureSize = deviceProperties.limits.maxImageDimension3D;

    //Get VRAM
    VkPhysicalDeviceMemoryProperties memoryProperties;
    vkGetPhysicalDeviceMemoryProperties(device, &memoryProperties);
    VkDeviceSize totalVRAM = 0;
    for (uint32_t i = 0; i < memoryProperties.memoryHeapCount; i++) {
		if (memoryProperties.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
			totalVRAM += memoryProperties.memoryHeaps[i].size;
		}
	}
    TotalGPURam = totalVRAM / (1024 * 1024);
    std::cout << "========================: " << std::endl;
    std::cout << "=========TOTAL VRAM============: " << std::endl;
    std::cout << "Device VRAM: " << TotalGPURam << " MB" << std::endl;
    std::cout << "Device Name: " << deviceProperties.deviceName << std::endl;
    std::cout << "Max Bound Descriptor Sets: " << maxBoundSets << std::endl;
    std::cout << "Max Per Stage Sampled Images: " << maxPerStageImages << std::endl;
    std::cout << "Max Total Sampled Images: " << maxTotalImages << std::endl;
    std::cout << "Max 3D Texture Size: " << max3DTextureSize << std::endl;
    std::cout << "========================: " << std::endl;

    if(TotalGPURam < 7000)
	{
		std::cout << "Warning: System RAM is less than 8GB, less than minimum requirements." << std::endl;
	}

    if(!rayTracingFeatures.rayTracingPipeline || !accelFeatures.accelerationStructure)
    {
        std::cout << "Device does not support required ray tracing features." << std::endl;
	}


    //return indices.isComplete() && extensionsSupported && swapChainAdequate;

    //return true;

    return indices.isComplete()
        && extensionsSupported
        && swapChainAdequate
        && rayTracingFeatures.rayTracingPipeline
        && accelFeatures.accelerationStructure
        && (TotalGPURam >= 7000); //At least 7GB VRAM, really 8GB.
}

std::vector<const char*> QTDoughApplication::GetRequiredExtensions() {
    uint32_t extensionCount = 0;

    SDL_Vulkan_GetInstanceExtensions(QTSDLWindow, &extensionCount, NULL);
    const char** sdlExtensions = (const char**)malloc(sizeof(char*) * extensionCount);
    SDL_Vulkan_GetInstanceExtensions(QTSDLWindow, &extensionCount, sdlExtensions);

    std::vector<const char*> extensions(sdlExtensions, sdlExtensions + extensionCount);

    if (enableValidationLayers) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    return extensions;
}


QueueFamilyIndices QTDoughApplication::FindQueueFamilies(VkPhysicalDevice device) {

    QueueFamilyIndices indices;

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    int i = 0;
    for (const auto& queueFamily : queueFamilies) {
        if ((queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) && (queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT)) {
            indices.graphicsAndComputeFamily = i;
        }


        if(queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT)
        {
            indices.physicsFamily = i;
		}

        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, _vkSurface, &presentSupport);

        if (presentSupport) {
            indices.presentFamily = i;
        }

        if (indices.isComplete()) {
            break;
        }

        i++;
    }

    return indices;
}

void QTDoughApplication::CreateLogicalDevice()
{
    QueueFamilyIndices indices = FindQueueFamilies(_physicalDevice);

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;

    std::map<uint32_t, uint32_t> familyQueueCountMap;

    familyQueueCountMap[indices.graphicsAndComputeFamily.value()] = std::max(familyQueueCountMap[indices.graphicsAndComputeFamily.value()], (uint32_t)3);
    familyQueueCountMap[indices.presentFamily.value()] = std::max(familyQueueCountMap[indices.presentFamily.value()], (uint32_t)1);
    familyQueueCountMap[indices.physicsFamily.value()] = std::max(familyQueueCountMap[indices.physicsFamily.value()], (uint32_t)1);



    //Want compute, graphics, and physics queues.
    float queuePriorities[4] = { 1.0f, 1.0f, 0.5f, 0.5f };

    for (auto [queue, count] : familyQueueCountMap)
    {
        VkDeviceQueueCreateInfo queueCreateInfo{};
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = queue;
		queueCreateInfo.queueCount = count;
		queueCreateInfo.pQueuePriorities = queuePriorities;
		queueCreateInfos.push_back(queueCreateInfo);
    }
    /*
    for(int i = 0; i < QueueFamilies.size(); i++)
	{
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = QueueFamilies[i];
        queueCreateInfo.queueCount = i;
        queueCreateInfo.pQueuePriorities = queuePriorities;
        queueCreateInfos.push_back(queueCreateInfo);
	}
    */
    /*
    for (uint32_t queueFamily : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = (queueFamily == indices.graphicsAndComputeFamily.value()) ? 2 : 1;
        queueCreateInfo.pQueuePriorities = queuePriorities;
        queueCreateInfos.push_back(queueCreateInfo);
    }
    */
    _createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    _createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    _createInfo.pQueueCreateInfos = queueCreateInfos.data();

    VkPhysicalDeviceFeatures deviceFeatures;
    vkGetPhysicalDeviceFeatures(_physicalDevice, &deviceFeatures);
    deviceFeatures.samplerAnisotropy = VK_TRUE;;

    _createInfo.pEnabledFeatures = nullptr;

    _createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    _createInfo.ppEnabledExtensionNames = deviceExtensions.data();

    if (enableValidationLayers) {
        _createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        _createInfo.ppEnabledLayerNames = validationLayers.data();
    }
    else {
        _createInfo.enabledLayerCount = 0;
    }


    // Setup feature structs
    VkPhysicalDeviceScalarBlockLayoutFeatures scalarBlockLayoutFeatures{};
    scalarBlockLayoutFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SCALAR_BLOCK_LAYOUT_FEATURES;
    scalarBlockLayoutFeatures.scalarBlockLayout = VK_TRUE;

    VkPhysicalDeviceDescriptorIndexingFeatures descriptorIndexingFeatures{};
    descriptorIndexingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
    descriptorIndexingFeatures.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
    descriptorIndexingFeatures.runtimeDescriptorArray = VK_TRUE;


    VkPhysicalDeviceBufferDeviceAddressFeatures bufferDeviceAddress{};
    bufferDeviceAddress.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
    bufferDeviceAddress.bufferDeviceAddress = VK_TRUE;

    VkPhysicalDeviceAccelerationStructureFeaturesKHR accelerationStructure{};
    accelerationStructure.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
    accelerationStructure.accelerationStructure = VK_TRUE;

    VkPhysicalDeviceRayTracingPipelineFeaturesKHR raytracingPipeline{};
    raytracingPipeline.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
    raytracingPipeline.rayTracingPipeline = VK_TRUE;

    // Chain them
    descriptorIndexingFeatures.pNext = &scalarBlockLayoutFeatures;
    scalarBlockLayoutFeatures.pNext = &bufferDeviceAddress;
    bufferDeviceAddress.pNext = &accelerationStructure;
    accelerationStructure.pNext = &raytracingPipeline;
    raytracingPipeline.pNext = nullptr;

    VkPhysicalDeviceVulkan13Features vulkan13Features{};
    vulkan13Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    vulkan13Features.dynamicRendering = VK_TRUE;
    vulkan13Features.synchronization2 = VK_TRUE;

    VkPhysicalDeviceFeatures2 deviceFeatures2{};
    deviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    deviceFeatures2.features = deviceFeatures;
    deviceFeatures2.pNext = &vulkan13Features;
    vulkan13Features.pNext = &descriptorIndexingFeatures;


    // Plug into VkDeviceCreateInfo
    _createInfo.pNext = &deviceFeatures2;
    _createInfo.pEnabledFeatures = nullptr;

    uint32_t qCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(_physicalDevice, &qCount, nullptr);
    std::vector<VkQueueFamilyProperties> qProps(qCount);
    vkGetPhysicalDeviceQueueFamilyProperties(_physicalDevice, &qCount, qProps.data());

    uint32_t fam = indices.graphicsAndComputeFamily.value();
    if (qProps[fam].queueCount < 3) {
        throw std::runtime_error("Selected queue family does not support 3 queues.");
    }

    //Finally instantiate this device.
    if (vkCreateDevice(_physicalDevice, &_createInfo, nullptr, &_logicalDevice) != VK_SUCCESS) {
        throw std::runtime_error("failed to create logical device!");
    }

    vkGetDeviceQueue(_logicalDevice, indices.graphicsAndComputeFamily.value(), 0, &_vkGraphicsQueue);
    vkGetDeviceQueue(_logicalDevice, indices.graphicsAndComputeFamily.value(), 1, &_vkComputeQueue);
    vkGetDeviceQueue(_logicalDevice, indices.graphicsAndComputeFamily.value(), 2, &_vkPhysicsQueue);

    vkGetDeviceQueue(_logicalDevice, indices.presentFamily.value(), 0, &_presentQueue);
}

void QTDoughApplication::CleanupSwapChain()
{
    for (size_t i = 0; i < swapChainFramebuffers.size(); i++) {
        vkDestroyFramebuffer(_logicalDevice, swapChainFramebuffers[i], nullptr);
    }

    for (size_t i = 0; i < swapChainImageViews.size(); i++) {
        vkDestroyImageView(_logicalDevice, swapChainImageViews[i], nullptr);
    }

    vkDestroySwapchainKHR(_logicalDevice, _swapChain, nullptr);
}

void QTDoughApplication::RecreateSwapChain()
{
    int width = 0, height = 0;
    SDL_GetWindowSize(QTSDLWindow, &width, &height);
    while (width == 0 || height == 0) {
        SDL_GetWindowSize(QTSDLWindow, &width, &height);
        SDL_WaitEvent(NULL);
    }

    vkDeviceWaitIdle(_logicalDevice);

    // Destroy old depth resources before recreating.
    if (depthImageView != VK_NULL_HANDLE) {
        vkDestroyImageView(_logicalDevice, depthImageView, nullptr);
        depthImageView = VK_NULL_HANDLE;
    }
    if (depthImage != VK_NULL_HANDLE) {
        vkDestroyImage(_logicalDevice, depthImage, nullptr);
        depthImage = VK_NULL_HANDLE;
    }
    if (depthImageMemory != VK_NULL_HANDLE) {
        vkFreeMemory(_logicalDevice, depthImageMemory, nullptr);
        depthImageMemory = VK_NULL_HANDLE;
    }

    CleanupEditorViewportImage();
    CleanupFrameOutput();
    CleanupSwapChain();

    // Update screen dimensions.
    SCREEN_WIDTH = width;
    SCREEN_HEIGHT = height;

    // Recreate swapchain and dependent resources
    CreateSwapChain();
    CreateImageViews();
    CreateDepthResources();

    CreateFrameOutput();
    CreateEditorViewportImage();
    RecreateResources();

    //Print size of new screen.
    std::cout << "New screen size: " << width << "x" << height << std::endl;
}

void QTDoughApplication::RecreateResources()
{

    /*
    vkDestroyDescriptorPool(_logicalDevice, globalDescriptorPool, nullptr);

    // Recreate pool
    CreateGlobalDescriptorPool();

    // Allocate sets again
    CreateGlobalDescriptorSet(); // Or call vkAllocateDescriptorSets again
    */
    /*
    // Destroy all textures and associated memory
    for (auto& pair : textures) {
        UnigmaTexture& tex = pair.second;
        vkDestroyImageView(_logicalDevice, tex.u_imageView, nullptr);
        vkDestroyImage(_logicalDevice, tex.u_image, nullptr);
        vkFreeMemory(_logicalDevice, tex.u_imageMemory, nullptr);
    }
    */
    QTDoughApplication::instance->textures.clear();

    //CreateSwapChain();
    //CreateImageViews();

    //UpdateGlobalDescriptorSet(); // Update descriptors with the newly loaded textures

    // Recreate graphics pipelines for each render pass
    for (auto& renderPass : renderPassStack) {
        renderPass->CleanupPipeline();
        renderPass->CreateMaterials();
        renderPass->CreateGraphicsPipeline();
    }

    for (auto& computePass : computePassStack) {
        computePass->CleanupImages();
    }

    for (auto& rtPass : rayTracePassStack) {
        rtPass->CleanupImages();
    }

    CreateImages();

    UpdateGlobalDescriptorSet();
}


void QTDoughApplication::CreateWindowSurface()
{

    if (!SDL_Vulkan_CreateSurface(QTSDLWindow, _vkInstance, &_vkSurface)) {
        throw std::runtime_error("Failed to create window surface!");
    }
    std::cout << "Created Window Surface" << std::endl;


    /*
    QueueFamilyIndices indices = FindQueueFamilies(_physicalDevice);

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = { indices.graphicsFamily.value(), indices.presentFamily.value() };

    float queuePriority = 1.0f;
    for (uint32_t queueFamily : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    _createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    _createInfo.pQueueCreateInfos = queueCreateInfos.data();

    vkGetDeviceQueue(_logicalDevice, indices.presentFamily.value(), 0, &_presentQueue);
    */
}

VkSampleCountFlagBits QTDoughApplication::GetMaxUsableSampleCount() {
    VkPhysicalDeviceProperties physicalDeviceProperties;
    vkGetPhysicalDeviceProperties(_physicalDevice, &physicalDeviceProperties);

    VkSampleCountFlags counts = physicalDeviceProperties.limits.framebufferColorSampleCounts & physicalDeviceProperties.limits.framebufferDepthSampleCounts;
    if (counts & VK_SAMPLE_COUNT_64_BIT) { return VK_SAMPLE_COUNT_64_BIT; }
    if (counts & VK_SAMPLE_COUNT_32_BIT) { return VK_SAMPLE_COUNT_32_BIT; }
    if (counts & VK_SAMPLE_COUNT_16_BIT) { return VK_SAMPLE_COUNT_16_BIT; }
    if (counts & VK_SAMPLE_COUNT_8_BIT) { return VK_SAMPLE_COUNT_8_BIT; }
    if (counts & VK_SAMPLE_COUNT_4_BIT) { return VK_SAMPLE_COUNT_4_BIT; }
    if (counts & VK_SAMPLE_COUNT_2_BIT) { return VK_SAMPLE_COUNT_2_BIT; }

    return VK_SAMPLE_COUNT_1_BIT;
}

bool QTDoughApplication::CheckDeviceExtensionSupport(VkPhysicalDevice device) {
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

    std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

    for (const auto& extension : availableExtensions) {
        requiredExtensions.erase(extension.extensionName);
    }

    return requiredExtensions.empty();
}

SwapChainSupportDetails QTDoughApplication::QuerySwapChainSupport(VkPhysicalDevice device) {
    std::cout << "Swap chain being queried" << std::endl;
    SwapChainSupportDetails details;

    VkResult result;

    // Get surface capabilities
    result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, _vkSurface, &details.capabilities);
    if (result != VK_SUCCESS) {
        std::cerr << "Failed to get physical device surface capabilities: " << result << std::endl;
        // Handle the error appropriately (e.g., return, throw exception)
        return details;
    }

    // Get surface formats
    uint32_t formatCount = 0;
    result = vkGetPhysicalDeviceSurfaceFormatsKHR(device, _vkSurface, &formatCount, nullptr);
    if (result != VK_SUCCESS || formatCount == 0) {
        std::cerr << "Failed to get physical device surface formats: " << result << std::endl;
        // Handle the error appropriately
        return details;
    }

    details.formats.resize(formatCount);
    result = vkGetPhysicalDeviceSurfaceFormatsKHR(device, _vkSurface, &formatCount, details.formats.data());
    if (result != VK_SUCCESS) {
        std::cerr << "Failed to get physical device surface formats data: " << result << std::endl;
        // Handle the error appropriately
        return details;
    }

    // Get present modes
    uint32_t presentModeCount = 0;
    result = vkGetPhysicalDeviceSurfacePresentModesKHR(device, _vkSurface, &presentModeCount, nullptr);
    if (result != VK_SUCCESS || presentModeCount == 0) {
        std::cerr << "Failed to get physical device surface present modes: " << result << std::endl;
        // Handle the error appropriately
        return details;
    }

    details.presentModes.resize(presentModeCount);
    result = vkGetPhysicalDeviceSurfacePresentModesKHR(device, _vkSurface, &presentModeCount, details.presentModes.data());
    if (result != VK_SUCCESS) {
        std::cerr << "Failed to get physical device surface present modes data: " << result << std::endl;
        // Handle the error appropriately
        return details;
    }

    std::cout << "Swap chain support details acquired successfully." << std::endl;
    return details;
}

VkSurfaceFormatKHR QTDoughApplication::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
    for (const auto& availableFormat : availableFormats) {
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return availableFormat;
        }
    }

    return availableFormats[0];
}

VkPresentModeKHR QTDoughApplication::chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {
    for (const auto& availablePresentMode : availablePresentModes) {
        if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return availablePresentMode;
        }
    }

    return VK_PRESENT_MODE_FIFO_KHR;
}


VkExtent2D QTDoughApplication::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) {

    if (capabilities.currentExtent.width != UINT32_MAX) {
        return capabilities.currentExtent;
    }
    else {
        int width, height;
        SDL_GetWindowSize(QTSDLWindow, &width, &height);

        VkExtent2D actualExtent = {
            static_cast<uint32_t>(width),
            static_cast<uint32_t>(height)
        };

        actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

        return actualExtent;
    }

}

void QTDoughApplication::ReadBackGPUData() {
    for (int i = 0; i < computePassStack.size(); i++)
    {
        computePassStack[i]->ReadBackGPUData();
    }
}

void QTDoughApplication::CreateQuadBuffers() {

    std::vector<Vertex> quadVertices = {
        {.pos = { -1.0f, -1.0f, 0.0f,0.0f }, .texCoord = { 0.0f, 1.0f,0.0f,0.0f } },
        {.pos = { 1.0f, -1.0f, 0.0f,0.0f }, .texCoord = { 1.0f, 1.0f,0.0f,0.0f } },
        {.pos = { 1.0f, 1.0f, 0.0f,0.0f }, .texCoord = { 1.0f, 0.0f,0.0f,0.0f } },
        {.pos = { -1.0f, 1.0f, 0.0f,0.0f }, .texCoord = { 0.0f, 0.0f,0.0f,0.0f } }
    };

    std::vector<uint16_t> quadIndices = {
        0, 1, 2, // First triangle
        2, 3, 0  // Second triangle
    };



    VkDeviceSize bufferSize = sizeof(quadVertices[0]) * quadVertices.size();

    // Create vertex buffer
    CreateBuffer(
        bufferSize,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        quadVertexBuffer,
        quadVertexBufferMemory
    );

    // Copy vertex data
    void* data;
    vkMapMemory(_logicalDevice, quadVertexBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, quadVertices.data(), (size_t)bufferSize);
    vkUnmapMemory(_logicalDevice, quadVertexBufferMemory);

    // Create index buffer
    bufferSize = sizeof(quadIndices[0]) * quadIndices.size();

    CreateBuffer(
        bufferSize,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        quadIndexBuffer,
        quadIndexBufferMemory
    );

    // Copy index data
    vkMapMemory(_logicalDevice, quadIndexBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, quadIndices.data(), (size_t)bufferSize);
    vkUnmapMemory(_logicalDevice, quadIndexBufferMemory);
}

void QTDoughApplication::DebugCompute(uint32_t currentFrame) {
    for(int i = 0; i < computePassStack.size(); i++)
	{
		computePassStack[i]->DebugCompute(currentFrame);
	}
}



void QTDoughApplication::Cleanup()
{
    CleanupEditorViewportImage();
    CleanupFrameOutput();
    CleanupSwapChain();

    for (int i = 0; i < NUM_OBJECTS; i++)
    {
        unigmaRenderingObjects[i].Cleanup(*this);
    }

    vkDestroyImageView(_logicalDevice, depthImageView, nullptr);
    vkDestroyImage(_logicalDevice, depthImage, nullptr);
    vkFreeMemory(_logicalDevice, depthImageMemory, nullptr);

    vkDestroySampler(_logicalDevice, textureSampler, nullptr);
    vkDestroyImageView(_logicalDevice, textureImageView, nullptr);

    vkDestroyImage(_logicalDevice, textureImage, nullptr);
    vkFreeMemory(_logicalDevice, textureImageMemory, nullptr);


    vkDestroyImageView(_logicalDevice, textureImageView, nullptr);

    vkDestroyImage(_logicalDevice, textureImage, nullptr);
    vkFreeMemory(_logicalDevice, textureImageMemory, nullptr);

    for (auto framebuffer : swapChainFramebuffers) {
        vkDestroyFramebuffer(_logicalDevice, framebuffer, nullptr);
    }
    for (auto imageView : swapChainImageViews) {
        vkDestroyImageView(_logicalDevice, imageView, nullptr);
    }
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroySemaphore(_logicalDevice, _renderFinishedSemaphores[i], nullptr);
        vkDestroySemaphore(_logicalDevice, _imageAvailableSemaphores[i], nullptr);
        vkDestroyFence(_logicalDevice, _inFlightFences[i], nullptr);
    }

    vkDestroyFence(_logicalDevice, _physicsFence, nullptr);
    vkDestroyCommandPool(_logicalDevice, _commandPool, nullptr);
    vkDestroyPipeline(_logicalDevice, graphicsPipeline, nullptr);
    vkDestroyPipelineLayout(_logicalDevice, _pipelineLayout, nullptr);
    vkDestroyRenderPass(_logicalDevice, renderPass, nullptr);
    vkDestroyInstance(_vkInstance, nullptr);
    vkDestroySwapchainKHR(_logicalDevice, _swapChain, nullptr);
    vkDestroyDevice(_logicalDevice, nullptr);
    vkDestroySurfaceKHR(_vkInstance, _vkSurface, nullptr);
    SDL_DestroyWindow(QTSDLWindow);
    SDL_Quit();
}