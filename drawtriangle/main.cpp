//
//  main.cpp
//  DrawTriangle
//
//  Created by nami on 2018/06/22.
//  Copyright © 2018年 Kousaku Namikawa. All rights reserved.
//

#define GLFW_INCLUDE_VULKAN
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE// openglのデプスは-1.0~0.0だが、Vulkanは0.0~1.0
#define STB_IMAGE_IMPLEMENTATION

using namespace std;

// 3DCG ライブラリ
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <GLFW/glfw3.h>
#include <stb/stb_image.h> // texture用のライブラリ

// errorやlog用
#include <iostream>
#include <stdexcept>
// lambda function用　主にresorce management
#include <functional>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <fstream>
#include <array>
#include <chrono>
#include <random>

// EXIT_SUCCESSとEXIT_FAILUREを提供する
#include <cstdlib>


const int WIDTH = 800;
const int HEIGHT = 600;

const int MAX_FRAMES_IN_FLIGHT = 2;

const vector<const char*> validationLayers = {
    "VK_LAYER_LUNARG_standard_validation"
};

const vector<const char*> deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

const bool enableValidationLayers = false;

VkResult CreateDebugReportCallbackEXT(VkInstance instance, const VkDebugReportCallbackCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugReportCallbackEXT* pCallback) {
    auto func = (PFN_vkCreateDebugReportCallbackEXT) vkGetInstanceProcAddr(instance, "vkCreateDebugReportCallbackEXT");
    if (func != nullptr) {
        return func(instance, pCreateInfo, pAllocator, pCallback);
    } else {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

void DestroyDebugReportCallbackEXT(VkInstance instance, VkDebugReportCallbackEXT callback, const VkAllocationCallbacks* pAllocator) {
    auto func = (PFN_vkDestroyDebugReportCallbackEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugReportCallbackEXT");
    if (func != nullptr) {
        func(instance, callback, pAllocator);
    }
}

struct QueueFamilyIndices {
    int graphicsFamily = -1;
    int presentFamily = -1;
    
    bool isComplete() {
        return graphicsFamily >= 0 && presentFamily >= 0;
    }
};

// スワップチェーンについて調べる必要があること
struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities; // 何枚貯めて置けるかと画像のサイズ、それぞれのmin/max
    vector<VkSurfaceFormatKHR> formats; // ピクセルフォーマット、色関連
    vector<VkPresentModeKHR> presentModes; // どんな風に変えるか
};

struct Vertex {
    glm::vec3 pos;
    glm::vec3 color;
    glm::vec2 texCoord;
    
    static VkVertexInputBindingDescription getBindingDescription() {
        VkVertexInputBindingDescription bindingDescription = {};
        bindingDescription.binding = 0; // bindingの中のインデックス
        bindingDescription.stride = sizeof(Vertex); // 何バイトが1頂点/インスタンス分か
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX; // 頂点ごとかインスタンスごとに読み込むデータを次にするかを決める
        
        return bindingDescription;
    }
    
    static array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions() {
        array<VkVertexInputAttributeDescription, 3> attributeDescriptions = {};
        
        attributeDescriptions[0].binding = 0; // bindingのインデックス
        attributeDescriptions[0].location = 0; // shaderのロケーション
        attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT; // format
        attributeDescriptions[0].offset = offsetof(Vertex, pos); // o
        
        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[1].offset = offsetof(Vertex, color);
        
        attributeDescriptions[2].binding = 0;
        attributeDescriptions[2].location = 2;
        attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
        attributeDescriptions[2].offset = offsetof(Vertex, texCoord);
        
        return attributeDescriptions;
    }
};

vector<Vertex> vertices = {
    // first plane
    {{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
    {{0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
    {{0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
    {{-0.5f, 0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}},
    
    // second plane
    {{-0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
    {{0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
    {{0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
    {{-0.5f, 0.5f, -0.5f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}}
};

vector<uint16_t> indices = {
    0, 1, 2, 2, 3, 0,
    4, 5, 6, 6, 7, 4
};

struct UniformBufferObject {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
};

void error_callback(int error, const char* description) {
    puts(description);
}

static vector<char> readFile(const string& filename) {
    ifstream file(filename, ios::ate | ios::binary);
    
    if (!file.is_open()) {
        throw runtime_error("failed to open file!");
    }
    
    size_t fileSize = (size_t) file.tellg();
    vector<char> buffer(fileSize);
    
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    
    file.close();
    
    return buffer;
}

class HelloTriangleApplication {
public:
    void run() {
//        mt19937 rnd;。
        for (int i = 0; i < 500; ++i) {
            cout << (float)rand()/RAND_MAX << endl;
            
            glm::vec3 t = glm::vec3((float)rand()/RAND_MAX *2.f - 1.f, (float)rand()/RAND_MAX *2.f - 1.f, (float)rand()/RAND_MAX *2.f - 1.f);
            
            vertices.push_back(Vertex({t, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}}));
            vertices.push_back(Vertex({t+glm::vec3{(i%2)*.1, ((i+1)%2)*.1, 0}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}}));
            vertices.push_back(Vertex({t+glm::vec3{(i%2)*.1, (i%2)*.1, 0}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}}));
            vertices.push_back(Vertex({t+glm::vec3{((i+1)%2)*.1, (i%2)*.1, 0}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}}));
            for (auto k : {i*4, i*4+1, i*4+2, i*4+2, i*4+3, i*4})indices.push_back(k);
        }
        
        for (auto i : vertices) cout << i.pos.x << endl;
        cout << endl;
        
//        vertices.push_back(Vertex({{-0.5f, -0.5f, -1.f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}}));
//        vertices.push_back(Vertex({{0.5f, -0.5f, -1.f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}}));
//        vertices.push_back(Vertex({{0.5f, 0.5f, -1.f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}}));
//        vertices.push_back(Vertex({{-0.5f, 0.5f, -1.f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}}));

        
        
        initWindow();
        initVulkan();
        mainLoop();
        cleanup();
    }
    
private:
    GLFWwindow* window;
    
    VkInstance instance;
    VkDebugReportCallbackEXT callback;
    VkSurfaceKHR surface;
    
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device;
    
    VkQueue graphicsQueue;
    VkQueue presentQueue;
    
    VkSwapchainKHR swapChain;
    vector<VkImage> swapChainImages;
    VkFormat swapChainImageFormat;
    VkExtent2D swapChainExtent;
    
    vector<VkImageView> swapChainImageViews;
    
    VkRenderPass renderPass;
    VkDescriptorSetLayout descriptorSetLayout;
    VkPipelineLayout pipelineLayout;
    
    VkPipeline graphicsPipeline;
    
    vector<VkFramebuffer> swapChainFramebuffers;
    
    VkCommandPool commandPool;
    vector<VkCommandBuffer> commandBuffers;
    
    vector<VkSemaphore> imageAvailableSemaphores;
    vector<VkSemaphore> renderFinishedSemaphores;
    vector<VkFence> inFlightFences;
    size_t currentFrame = 0;
    
    VkBuffer vertexBuffer;
    VkDeviceMemory vertexBufferMemory;
    VkBuffer indexBuffer;
    VkDeviceMemory indexBufferMemory;
    
    
    vector<VkBuffer> uniformBuffers;
    vector<VkDeviceMemory> uniformBuffersMemory;
    
    VkDescriptorPool descriptorPool;
    vector<VkDescriptorSet> descriptorSets;
    
    VkImage textureImage;
    VkDeviceMemory textureImageMemory;
    
    VkImageView textureImageView;
    VkSampler textureSampler;
    
    vector<VkImage> depthImages;
    vector<VkDeviceMemory> depthImagesMemory;
    vector<VkImageView> depthImagesView;
    
    void initWindow() {
        if (!glfwVulkanSupported()) {
            cout << "vulkan loader not found!" << endl;
        }
        
        glfwInit();
        glfwGetTime();

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        
        window = glfwCreateWindow(800, 600, "Vulkan Test!!", nullptr, nullptr);
    }
    void initVulkan() {
        
        glfwSetErrorCallback(error_callback);
        createInstance();
        setupDebugCallback();
        createSurface();
        pickPhysicalDevice();
        createLogicalDevice();
        createSwapChain();
        createImageViews();
        createRenderPass();
        createDescriptorSetLayout();
        createGraphicsPipeline();
        createCommandPool();
        createDepthResources();
        createFramebuffers();
        createTextureImage();
        createTextureImageView();
        createTextureSampler();
        createVertexBuffer();
        createIndexBuffer();
        createUniformBuffer();
        createDescriptorPool();
        createDescriptorSets();
        createCommandBuffers();
        createSyncObjects();
    }
    
    void mainLoop() {
        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();
            drawFrame();
        }
        
        // ウィンドウを閉じたときに、非同期処理なので、同期して終了するために必要
        vkDeviceWaitIdle(device);
    }
    
    void cleanup() {
        cleanupSwapChain();
        
        vkDestroySampler(device, textureSampler, nullptr);
        vkDestroyImageView(device, textureImageView, nullptr);
        
        vkDestroyImage(device, textureImage, nullptr);
        vkFreeMemory(device, textureImageMemory, nullptr);
        
        vkDestroyDescriptorPool(device, descriptorPool, nullptr);

        vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
        
        for (size_t i = 0; i < swapChainImages.size(); i++) {
            vkDestroyBuffer(device, uniformBuffers[i], nullptr);
            vkFreeMemory(device, uniformBuffersMemory[i], nullptr);
        }
        
        vkDestroyBuffer(device, indexBuffer, nullptr);
        vkFreeMemory(device, indexBufferMemory, nullptr);
        
        vkDestroyBuffer(device, vertexBuffer, nullptr);
        vkFreeMemory(device, vertexBufferMemory, nullptr);
        
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            vkDestroySemaphore(device, renderFinishedSemaphores[i], nullptr);
            vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
            vkDestroyFence(device, inFlightFences[i], nullptr);
        }
        
        vkDestroyCommandPool(device, commandPool, nullptr);
        
        vkDestroyDevice(device, nullptr);
        
        if (enableValidationLayers) {
            DestroyDebugReportCallbackEXT(instance, callback, nullptr);
        }
        
        vkDestroySurfaceKHR(instance, surface, nullptr);
        vkDestroyInstance(instance, nullptr);
        
        glfwDestroyWindow(window);
        
        glfwTerminate();
    }
    
    void createInstance() {
        if (enableValidationLayers && !checkValidationLayerSupport()) {
            throw runtime_error("validation layers requested, but not avaliable!");
        }
        
        // application情報
        VkApplicationInfo appInfo = {};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "Hello Triangle";
        appInfo.applicationVersion = VK_MAKE_VERSION(1,0,0);
        appInfo.pEngineName = "No Engine";
        appInfo.engineVersion = VK_MAKE_VERSION(1,0,0);
        appInfo.apiVersion = VK_API_VERSION_1_0;
        
        // global extentions, validationlayerに関する情報
        VkInstanceCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;
        
        // 検証レイヤーを通しつつ、extension関連の初期化
        auto extensions = getRequiredExtensions();
        createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        createInfo.ppEnabledExtensionNames = extensions.data();
        
        if (enableValidationLayers) {
            createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();
        } else {
            createInfo.enabledLayerCount = 0;
        }

        // ここでようやくインスタンスを作る
        if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
            throw runtime_error("failed to create instance!");
        }
    }
    
    void setupDebugCallback() {
        if (!enableValidationLayers) return;
        
        VkDebugReportCallbackCreateInfoEXT createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
        createInfo.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;
        createInfo.pfnCallback = debugCallback;
        
        if (CreateDebugReportCallbackEXT(instance, &createInfo, nullptr, &callback) != VK_SUCCESS) {
            throw runtime_error("failed to set up debug callback!");
        }
    }
    
    void pickPhysicalDevice() {
        // 使用する物理デバイスを選択する
        
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
        
        if (deviceCount == 0) {
            throw runtime_error("failed to find GPUs with Vulkan support!");
        }
        
        vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());
        
        
        // 標準的なデバイスの選定
//        for (const auto& device : devices) {
//            if (isDeviceSuitable(device)) {
//                physicalDevice = device;
//                break;
//            }
//        }
        
        // 例えば一番高い性能のGPUを使いたい場合
        multimap<int, VkPhysicalDevice> candidates;
        
        for (const auto& device : devices) {
            int score = rateDeviceSuitability(device);
            candidates.insert(make_pair(score, device));
        }
        
        // 一番高いscoreを持つGPUセットする
        if (candidates.rbegin()->first > 0) {
            physicalDevice = candidates.rbegin()->second;
        } else {
            throw runtime_error("failed to find a suitable GPU!");
        }
        
        
        if (physicalDevice == VK_NULL_HANDLE) {
            throw runtime_error("failed to find a suitable GPU!");
        }
        // 例えば一番高い性能のGPUを使いたい場合 終わり
    }
    
    void createLogicalDevice() {
        QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
        
        vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        set<int> uniqueQueueFamilies = {indices.graphicsFamily, indices.presentFamily};
        
        float queuePriority = 1.0f;
        for (int queueFamily : uniqueQueueFamilies) {
            VkDeviceQueueCreateInfo queueCreateInfo = {};
            queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfo.queueFamilyIndex = queueFamily;
            queueCreateInfo.queueCount = 1;
            queueCreateInfo.pQueuePriorities = &queuePriority;
            queueCreateInfos.push_back(queueCreateInfo);
        }
        
        VkPhysicalDeviceFeatures deviceFeatures = {};
        deviceFeatures.samplerAnisotropy = VK_TRUE;
        
        VkDeviceCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        
        createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
        createInfo.pQueueCreateInfos = queueCreateInfos.data();
        
        createInfo.pEnabledFeatures = &deviceFeatures;
        
        createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
        createInfo.ppEnabledExtensionNames = deviceExtensions.data();
        
        if (enableValidationLayers) {
            createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();
        } else {
            createInfo.enabledLayerCount = 0;
        }
        
        if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS) {
            throw runtime_error("failed to create logical device!");
        }
        
        vkGetDeviceQueue(device, indices.graphicsFamily, 0, &graphicsQueue);
        vkGetDeviceQueue(device, indices.presentFamily, 0, &presentQueue);
    }
    
    void createSurface() {
        // glfwの簡単にウィンドウを作ってくれるやつ
        cout << glfwCreateWindowSurface(instance, window, nullptr, &surface) << endl;
        
        if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS) {
            throw runtime_error("failed to create window surface!");
        }
    }
    
    void createSwapChain() {
        SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice);
        
        VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
        
        VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
        
        VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);
        
        uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
        if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
            imageCount = swapChainSupport.capabilities.maxImageCount;
        }
        
        // スワップチェーン作るための細かい設定たち
        VkSwapchainCreateInfoKHR createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = surface;
        
        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = surfaceFormat.format;
        createInfo.imageColorSpace = surfaceFormat.colorSpace;
        createInfo.imageExtent = extent;
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        
        // どのように複数のキューでスワップチェーンを使用するかの設定
        QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
        uint32_t queueFamilyIndices[] = {(uint32_t) indices.graphicsFamily, (uint32_t) indices.presentFamily};
        
        if (indices.graphicsFamily != indices.presentFamily) {
            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            createInfo.queueFamilyIndexCount = 2;
            createInfo.pQueueFamilyIndices = queueFamilyIndices;
        } else {
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
            createInfo.queueFamilyIndexCount = 0;
            createInfo.pQueueFamilyIndices = nullptr;
        }
        
        createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
        
        // ウィンドウを透過して他のウィンドウをアルファブレンディングで表示するかどうか
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        
        createInfo.presentMode = presentMode;
        // 他のウィンドウが前にきたときに、そのウィンドウのピクセルの色を知りたいときなどにVK_FALSEにして使う(TRUEの場合は前面のウィンドウを無視する)
        createInfo.clipped = VK_TRUE;
        // スクリーンのリサイズ時などにスワップチェーンで作られるべきバッファが作成されなかったときに、古いものへの参照を指定することができる（後のチャプターで解説）
        createInfo.oldSwapchain = VK_NULL_HANDLE;
        
        if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapChain) != VK_SUCCESS) {
            throw std::runtime_error("failed to create swap chain!");
        }
        
        vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr);
        swapChainImages.resize(imageCount);
        vkGetSwapchainImagesKHR(device, swapChain, &imageCount, swapChainImages.data());
        
        swapChainImageFormat = surfaceFormat.format;
        swapChainExtent = extent;
    }
    
    void createImageViews() {
        swapChainImageViews.resize(swapChainImages.size());
        
        for (size_t i = 0; i < swapChainImages.size(); i++) {
            swapChainImageViews[i] = createImageView(swapChainImages[i], swapChainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT);
        }
        
    }
    
    void createRenderPass() {
        // レンダリング時に使用されるフレームバッファの数、そしてそれらのフォーマットやデプスバッファの有無、さらにそれらがどのようにレンダリング中に操作されるかを作成する
        // attachmentは一つ一つのフレームバッファに関する設定
        VkAttachmentDescription colorAttachment = {};
        colorAttachment.format = swapChainImageFormat;
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        
        // レンダリング前と後でデータをどのように処理するか(colorとdepth)
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        
        // 上のステンシル版
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; // レンダーパスが始まる前
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR; // レンダーパスが終了したときに自動的に遷移するレイアウト
    
        // subpass : レンダーパスの中で、ポストプロセッシングなどの前のフレームの情報が残されたままだと都合がいい場合に、それらの処理を一つにまとめることができる
        // subpassは後でパフォーマンスがいいように順番を並べ替えることができる
        VkAttachmentReference colorAttachmentRef = {};
        colorAttachmentRef.attachment = 0; // subpassのインデックス（順番）
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        
        
        VkAttachmentDescription depthAttachment = {};
        depthAttachment.format = findDepthFormat();
        depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE; // 描画が終わった時にはデプスの情報は必要ない
        depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; // 前の画像はどうでもいいよね
        depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        
        VkAttachmentReference depthAttachmentRef = {};
        depthAttachmentRef.attachment = 1;
        depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        
        // Vulkanは将来的に計算用のsubpassも追加される予定なので、このサブパスがグラフィック用であることを明示する必要がある
        VkSubpassDescription subpass = {};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorAttachmentRef; // shaderからlayout(location = 0) out vec4 outColorのように, シェーダー側のlocationとsubpassのAttachmentのインデックスが一致している
        subpass.pDepthStencilAttachment = &depthAttachmentRef; // デプスとステンシルはカラーと違い一つしかアタッチメントできない
        
        // Subpass同士の依存関係を示す（例：　このSubpassの次にこのSubpassを実行したいなど？）, 常にsrc<dstでないといけない
        VkSubpassDependency dependency = {};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL; // この定数は指定したSubpassたちの前に実行される暗黙のサブパス(内部システムのもの？)
        dependency.dstSubpass = 0;
        
        // その操作が発生する段階
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT; // スワップチェーンがイメージを読み込むのを待つ
                                                                                 // 待機する操作
        dependency.srcAccessMask = 0; // この場合は特に操作を指定していなくても、カラーアタッチメントのステージを待てば大丈夫
        
        // このSubpass内のどの操作を次の操作が待てばいいかを指定する
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT; //この場合、カラーアタッチメントのステージで
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT; // 読み書きが終われば次の処理は走っても大丈夫
        
        
        array<VkAttachmentDescription, 2> attachments = {colorAttachment, depthAttachment};
        VkRenderPassCreateInfo renderPassCreateInfo = {};
        renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassCreateInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        renderPassCreateInfo.pAttachments = attachments.data();
        renderPassCreateInfo.subpassCount = 1;
        renderPassCreateInfo.pSubpasses = &subpass;
        
        renderPassCreateInfo.dependencyCount = 1;
        renderPassCreateInfo.pDependencies = &dependency;
        
        
        if (vkCreateRenderPass(device, &renderPassCreateInfo, nullptr, &renderPass) != VK_SUCCESS) {
            throw runtime_error("failed to create render pass!");
        }
        
    }
    
    void createGraphicsPipeline() {
        auto vertShaderCode = readFile("shaders/vert.spv");
        auto fragShaderCode = readFile("shaders/frag.spv");
        
        cout << "vertShaderFile size : " << vertShaderCode.size() << "bytes" << endl;
        cout << "fragShaderFile size : " << fragShaderCode.size() << "bytes" << endl;
        
        VkShaderModule vertShaderModule;
        VkShaderModule fragShaderModule;
        
        // shaderModuleの作成（この時点ではただのバッファのラッパで、お互いのリンクや何のシェーダかは決められていない）
        vertShaderModule = createShaderModule(vertShaderCode);
        fragShaderModule = createShaderModule(fragShaderCode);
        
        // shader stageの作成
        VkPipelineShaderStageCreateInfo vertShaderStageInfo = {};
        vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
        
        vertShaderStageInfo.module = vertShaderModule;
        vertShaderStageInfo.pName = "main"; // 最初に呼び出す関数の名前を指定
    //        vertShaderStageInfo.pSpecializationInfo = // シェーダに定数を渡すことができる、参考:https://blogs.igalia.com/itoral/2018/03/20/improving-shader-performance-with-vulkans-specialization-constants/
        
        // レンダリングパイプラインに関する設定
        VkPipelineShaderStageCreateInfo fragShaderStageInfo = {};
        fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragShaderStageInfo.module = fragShaderModule;
        fragShaderStageInfo.pName = "main";
        // (プログラマブル部分の）レンダリングパイプラインの作成
        VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};
        
        // 頂点情報を取得
        auto bindingDescription = Vertex::getBindingDescription();
        auto attributeDescriptions = Vertex::getAttributeDescriptions();
        
        // 頂点シェーダーに渡す情報の設定
        VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = 1; // 頂点シェーダへの頂点データの数
        vertexInputInfo.pVertexBindingDescriptions = &bindingDescription; // 頂点データのポインタ
        vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size()); // attributeの数
        vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data(); // attributeを表すVkVertexInputAttributeDescriptionのポインタ
        
        VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST; // openglでいうGL_PRIMITIVE_MODE
        inputAssembly.primitiveRestartEnable = VK_FALSE; // triangleとLineに分割できる???
        
        // viewport設定
        VkViewport viewport = {};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = (float) swapChainExtent.width;
        viewport.height = (float) swapChainExtent.height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        
        // フレームバッファのトリミング範囲の設定
        VkRect2D scissor = {};
        scissor.offset = {0,0};
        scissor.extent = swapChainExtent;
        
        // viewportとトリミング範囲の設定（それぞれ複数ずつ使用することも可能だが、その場合論理デバイスで設定する必要がある）
        VkPipelineViewportStateCreateInfo viewportStateCreateInfo = {};
        viewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportStateCreateInfo.viewportCount = 1;
        viewportStateCreateInfo.pViewports = &viewport;
        viewportStateCreateInfo.scissorCount = 1;
        viewportStateCreateInfo.pScissors = &scissor;
        
        VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo = {};
        rasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizationStateCreateInfo.depthClampEnable = VK_FALSE; // シャドウマップなどを作る際にクリッピング空間外のオブジェクトを破棄するのではなく、クランプしたい場合にVK_TRUEで使うことができる。（論理デバイスでGPU機能を有効化する必要がある）
        rasterizationStateCreateInfo.rasterizerDiscardEnable = VK_FALSE; // ラスタライザをスキップするかどうか、基本的にスキップするとフレームバッファに書き込まれない
        rasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_FILL; // ポリゴンの塗りつぶし設定(FILLとLINEとPOINTがある）
        rasterizationStateCreateInfo.lineWidth = 1.0f; // 1.0より太い太さを設定する場合、論理デバイスでGPU機能を有効化する必要がある
        
        // カリング周り
        rasterizationStateCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
        rasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        
        // 深度値のバイアスを設定する。シャドウマップなどで、奥に行く時の補間方法などを細かく指定したい場合に使える
        rasterizationStateCreateInfo.depthBiasEnable = VK_FALSE;
//        rasterizationStateCreateInfo.depthBiasConstantFactor = 0.0f;
//        rasterizationStateCreateInfo.depthBiasClamp = 0.0f;
//        rasterizationStateCreateInfo.depthViasSlopeFactor = 0.0f;
        
        // マルチサンプリング（単純に大きい解像度でレンダリングしてから縮小をかけるより大幅にコストを下げることができる）、論理デバイスで要有効化
        VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo = {};
        multisampleStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampleStateCreateInfo.sampleShadingEnable = VK_FALSE;
        multisampleStateCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
//        multisampleStateCreateInfo.minSampleShading = 1.0f;
//        multisampleStateCreateInfo.pSampleMask = nullptr;
//        multisampleStateCreateInfo.alphaToCoverageEnable = VK_FALSE;
//        multisampleStateCreateInfo.alphaToOneEnable = VK_FALSE;
        
        // depth test & stencil test
        VkPipelineDepthStencilStateCreateInfo depthStencil = {};
        depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencil.depthTestEnable = VK_TRUE; // デプステストの有効化
        depthStencil.depthWriteEnable = VK_TRUE;
        
        depthStencil.depthCompareOp = VK_COMPARE_OP_LESS; // 深度値の比較方法
        
        depthStencil.depthBoundsTestEnable = VK_FALSE;
        depthStencil.minDepthBounds = 0.0f;
        depthStencil.maxDepthBounds = 1.0f;
        
        depthStencil.stencilTestEnable = VK_FALSE;
        depthStencil.front = {};
        depthStencil.back = {};
        
        // 各フレームバッファにブレンディングの設定を付加
        VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_FALSE;
        
        // 例えばこんな感じ
//        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
//        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
//        colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
//        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
//        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
//        colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
        
        // アルファブレンディング
//        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
//        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
//        colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
//        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
//        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
//        colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

        VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo = {};
        colorBlendStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlendStateCreateInfo.logicOpEnable = VK_FALSE;
        colorBlendStateCreateInfo.logicOp = VK_LOGIC_OP_COPY;
        colorBlendStateCreateInfo.attachmentCount = 1;
        colorBlendStateCreateInfo.pAttachments = &colorBlendAttachment;
        colorBlendStateCreateInfo.blendConstants[0] = 0.0f;
        colorBlendStateCreateInfo.blendConstants[1] = 0.0f;
        colorBlendStateCreateInfo.blendConstants[2] = 0.0f;
        colorBlendStateCreateInfo.blendConstants[3] = 0.0f;
        
        // 一部のパイプラインの設定は、動的に変更することができる、その場合Dynamic stateとしてあらかじめ登録しておく必要がある
//        VkDynamicState dynamicStates[] = {
//            VK_DYNAMIC_STATE_VIEWPORT,
//            VK_DYNAMIC_STATE_LINE_WIDTH
//        }
//        VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = {};
//        dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
//        dynamicStateCreateInfo.dynamicStateCount = 2;
//        dynamicStateCreateInfo.pDynamicStates = dynamicStates;
        
        // uniform変数
        VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
        pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutCreateInfo.setLayoutCount = 1;
        pipelineLayoutCreateInfo.pSetLayouts = &descriptorSetLayout; // descriptorのセット
        pipelineLayoutCreateInfo.pushConstantRangeCount = 0;
//        pipelineLayoutCreateInfo.pPushConstantRanges = nullptr;
        
        if (vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
            throw runtime_error("failed to create pipeline layout!");
        }
        
        VkGraphicsPipelineCreateInfo pipelineCreateInfo = {};
        pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineCreateInfo.stageCount = 2;
        pipelineCreateInfo.pStages = shaderStages;
        
        // 今まで作ってきたやつをVkGraphicsPipelineCreateInfoにぶち込む
        pipelineCreateInfo.pVertexInputState = &vertexInputInfo;
        pipelineCreateInfo.pInputAssemblyState = &inputAssembly;
        pipelineCreateInfo.pViewportState = &viewportStateCreateInfo;
        pipelineCreateInfo.pRasterizationState = &rasterizationStateCreateInfo;
        pipelineCreateInfo.pMultisampleState = &multisampleStateCreateInfo;
        pipelineCreateInfo.pDepthStencilState = &depthStencil;
        pipelineCreateInfo.pColorBlendState = &colorBlendStateCreateInfo;
//        pipelineCreateInfo.pDynamicState = nullptr;
        
        pipelineCreateInfo.layout = pipelineLayout;
        
        pipelineCreateInfo.renderPass = renderPass; // このパイプラインを他のレンダーパスで使用することもできる
        pipelineCreateInfo.subpass = 0; // 使用するsubpassのインデックス
        
        // Vulkanでは既存のパイプラインから新しいパイプラインを作ることができ、そのときに別のパイプラインとの関連性を指定できる
        pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
//        pipelineCreateInfo.basePipelineIndex = -1;
        
        // 本当は一回のコールで複数のパイプラインを同時に作れるように設計されている
        if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &graphicsPipeline) != VK_SUCCESS) {
            throw runtime_error("failed to create graphics pipeline!");
        }
        
        
        
        
        vkDestroyShaderModule(device, fragShaderModule, nullptr);
        vkDestroyShaderModule(device, vertShaderModule, nullptr);
    }
    
    void createFramebuffers() {
        swapChainFramebuffers.resize(swapChainImageViews.size());
        
        for (size_t i = 0; i < swapChainImageViews.size(); i++) {
            array<VkImageView, 2> attachments = {
                swapChainImageViews[i],
                depthImagesView[i]
            };
            
            VkFramebufferCreateInfo framebufferCreateInfo = {};
            framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferCreateInfo.renderPass = renderPass;
            framebufferCreateInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
            framebufferCreateInfo.pAttachments = attachments.data();
            framebufferCreateInfo.width = swapChainExtent.width;
            framebufferCreateInfo.height = swapChainExtent.height;
            framebufferCreateInfo.layers = 1;
            
            if (vkCreateFramebuffer(device, &framebufferCreateInfo, nullptr, &swapChainFramebuffers[i]) != VK_SUCCESS) {
                throw runtime_error("failed to create framebuffer!");
            }
        }
    }
    
    void createCommandPool() {
        QueueFamilyIndices queueFamilyIndices = findQueueFamilies(physicalDevice);
        
        VkCommandPoolCreateInfo poolCreateInfo = {};
        poolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolCreateInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily;
        poolCreateInfo.flags = 0;
        
        if (vkCreateCommandPool(device, &poolCreateInfo, nullptr, &commandPool) != VK_SUCCESS) {
            throw runtime_error("failed to create command pool!");
        }
    }
    
    void createCommandBuffers() {
        commandBuffers.resize(swapChainFramebuffers.size());
        
        VkCommandBufferAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = commandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; // コマンドの強さ（Primaryなら直接実行キューとして渡せるが、他のキューから参照できなく、Secondaryなら直接実行キューには渡せないが、他のキューから実行できる）
        allocInfo.commandBufferCount = (uint32_t) commandBuffers.size();
        
        if (vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data()) != VK_SUCCESS) {
            throw runtime_error("failed to allocate command buffers!");
        }
        
        for (size_t i = 0; i < commandBuffers.size(); i++) {
            VkCommandBufferBeginInfo beginInfo = {};
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT; // コマンドバッファの使用方法
            beginInfo.pInheritanceInfo = nullptr;
            
            if (vkBeginCommandBuffer(commandBuffers[i], &beginInfo) != VK_SUCCESS) {
                throw runtime_error("failed to begin recording command buffer!");
            }
            
            VkRenderPassBeginInfo renderPassInfo = {};
            renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            renderPassInfo.renderPass = renderPass;
            renderPassInfo.framebuffer = swapChainFramebuffers[i];
            
            renderPassInfo.renderArea.offset = {0,0};
            renderPassInfo.renderArea.extent = swapChainExtent;
            
            array<VkClearValue, 2> clearValues = {};
            clearValues[0].color = {0.0f,0.0f,0.0f,1.0f};
            clearValues[1].depthStencil = {1.0f,0};
            
            renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
            renderPassInfo.pClearValues = clearValues.data();
            
            vkCmdBeginRenderPass(commandBuffers[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
            
            vkCmdBindPipeline(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);
            
            VkBuffer vertexBuffers[] = {vertexBuffer};
            VkDeviceSize offsets[] = {0};
            // 二番目はオフセット、三番目はバインディングの数
            vkCmdBindVertexBuffers(commandBuffers[i], 0, 1, vertexBuffers, offsets);
            vkCmdBindIndexBuffer(commandBuffers[i], indexBuffer, 0, VK_INDEX_TYPE_UINT16);
            
            vkCmdBindDescriptorSets(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets[i], 0, nullptr); // descriptorをセット
            
            vkCmdDrawIndexed(commandBuffers[i], static_cast<uint32_t>(indices.size()), 1, 0, 0, 0); //一番目から, commandBuffer, vertexCount, instanceCount, indexの読み込み開始位置, indexに一律の数字を追加できる, instance offset
            
//            vkCmdDraw(commandBuffers[i], static_cast<uint32_t>(vertices.size()), 1, 0, 0); //一番目から, commandBuffer, vertexCount, instanceCount, firstVertex, firstInstance
            
            vkCmdEndRenderPass(commandBuffers[i]);
            
            if (vkEndCommandBuffer(commandBuffers[i]) !=  VK_SUCCESS) {
                throw runtime_error("failed to record command buffer!");
            }
        }
        
        
    }
    
    void drawFrame() {
        vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE, numeric_limits<uint64_t>::max());
        vkResetFences(device, 1, &inFlightFences[currentFrame]);
        
        uint32_t imageIndex;
        
        // 一番目から device, swapchain, イメージが使用可能になるまでのタイムアウト（ナノ秒、最大値なら無効）, semaphore | or & fence, アクセスすべきスワップチェーンのイメージのインデックス
        VkResult result = vkAcquireNextImageKHR(device, swapChain, numeric_limits<uint64_t>::max(), imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);
        
        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            recreateSwapChain();
            return;
        } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            throw runtime_error("failed to acquire swap chain image!");
        }
        
        updateUniformBuffer(imageIndex);
        
        VkSubmitInfo submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        
        VkSemaphore waitSemaphores[] = {imageAvailableSemaphores[currentFrame]}; // 実行が開始される前にどのSemaphoresを待つか
        VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT}; // 実行が開始される前にどのStageを待つか
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;
        
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffers[imageIndex];
        
        VkSemaphore signalSemaphores[] = {renderFinishedSemaphores[currentFrame]};
        // コマンドバッファの実行が終了したときに通知したいSemaphoreを指定する
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;
        
        if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS) {
            throw runtime_error("failed to submit draw command buffer!");
        }
        
        VkPresentInfoKHR presentInfo = {};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = signalSemaphores;
        
        VkSwapchainKHR swapChains[] = {swapChain};
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapChains;
        presentInfo.pImageIndices = &imageIndex;
        
        // 複数のスワップチェーンがあった場合に、プレゼンテーションが成功した場合にそれぞれのスワップチェーンを確認することができる
        presentInfo.pResults = nullptr;
        
        // イメージをスワップチェーンに格納する
        result = vkQueuePresentKHR(presentQueue, &presentInfo);
        
        vkQueueWaitIdle(presentQueue);
        
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
            recreateSwapChain();
        } else if (result != VK_SUCCESS) {
            throw runtime_error("failed to present swap chain image!");
        }
        
        currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    }
    
    void createSyncObjects() {
        // semaphoreでGPUとGPUの同期を行い、fenceでGPUとCPUの同期をとっている
        
        imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
        
        VkSemaphoreCreateInfo semaphoreInfo = {};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        
        VkFenceCreateInfo fenceInfo = {};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        
        
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS || vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS || vkCreateFence(device, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS) {
                throw runtime_error("failed to create semaphores!");
            }
        }

    }
    
    void cleanupSwapChain() {
        for (size_t i = 0; i < swapChainImages.size(); i++) {
            vkDestroyImageView(device, depthImagesView[i], nullptr);
            vkDestroyImage(device, depthImages[i], nullptr);
            vkFreeMemory(device, depthImagesMemory[i], nullptr);
        }
        
        
        for (auto framebuffer : swapChainFramebuffers) {
            vkDestroyFramebuffer(device, framebuffer, nullptr);
        }
        
        vkFreeCommandBuffers(device, commandPool, static_cast<uint32_t>(commandBuffers.size()), commandBuffers.data());
        
        vkDestroyPipeline(device, graphicsPipeline, nullptr);
        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
        vkDestroyRenderPass(device, renderPass, nullptr);
        
        for (auto imageView : swapChainImageViews) {
            vkDestroyImageView(device, imageView, nullptr);
        }
        
        vkDestroySwapchainKHR(device, swapChain, nullptr);
    }
    
    // ウィンドウのリサイズ時にスワップチェーンを作り直す
    void recreateSwapChain() {
        vkDeviceWaitIdle(device);
        
        createSwapChain();
        createImageViews();
        createRenderPass();
        createGraphicsPipeline();
        createDepthResources();
        createFramebuffers();
        createCommandBuffers();
    }
    
    void createVertexBuffer() {
        VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();
        
        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferMemory;
        createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);
        
        void* data;
        // オフセットとサイズを指定して作ったメモリをCPU側からアクセス可能なメモリにマッピングして使用可能にする
        // サイズをVK_WHOLE_SIZEにすれば全てのメモリをマップすることもできる
        // 最後から二番目の変数はflagsを指定できる（現在のAPIではまだ使用不可）
        vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
        memcpy(data, vertices.data(), (size_t) bufferSize); // 頂点データのメモリをコピーしていれる
        vkUnmapMemory(device, stagingBufferMemory);
        
        createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertexBuffer, vertexBufferMemory);
        copyBuffer(stagingBuffer, vertexBuffer, bufferSize); // ステージングバッファを頂点バッファにコピー
        
        vkDestroyBuffer(device, stagingBuffer, nullptr);
        vkFreeMemory(device, stagingBufferMemory, nullptr);
    }
    
    void createIndexBuffer() {
        VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();
        
        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferMemory;
        createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);
        
        void* data;
        vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
        memcpy(data, indices.data(), (size_t) bufferSize);
        vkUnmapMemory(device, stagingBufferMemory);
        
        createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, indexBuffer, indexBufferMemory);
        copyBuffer(stagingBuffer, indexBuffer, bufferSize);
        
        vkDestroyBuffer(device, stagingBuffer, nullptr);
        vkFreeMemory(device, stagingBufferMemory, nullptr);
    }
    
    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory) {
        VkBufferCreateInfo bufferInfo = {};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = usage; //bufferのusage ^で複数指定可能
        
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        
        if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
            throw runtime_error("failed to create vertex buffer!");
        }
        
        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(device, buffer, &memRequirements);
        
        VkMemoryAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties); // CPU側から書き込めるメモリを指定
        
        if (vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
            throw runtime_error("failed to allocate vertex buffer memory!");
        }
        
        vkBindBufferMemory(device, buffer, bufferMemory, 0); // 実際にメモリを有効化 四番目のパラメーターはメモリ領域内のオフセット
    }
    
    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
        VkCommandBuffer commandBuffer = beginSingleTimeCommands();
        
        VkBufferCopy copyRegion = {};
        copyRegion.srcOffset = 0;
        copyRegion.dstOffset = 0;
        copyRegion.size = size;
        vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);
        
        endSingleTimeCommands(commandBuffer);
    }
    
    void createDescriptorSetLayout() {
        VkDescriptorSetLayoutBinding uboLayoutBinding = {};
        uboLayoutBinding.binding = 0; // shaderのuniform変数のbindingの番号に相当
        uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        uboLayoutBinding.descriptorCount = 1; // C++側での配列内での要素数
        
        uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT; // シェーダーのどのステージで使うか
        
        uboLayoutBinding.pImmutableSamplers = nullptr; // image sampling に関する設定（後述）
        
        VkDescriptorSetLayoutBinding samplerLayoutBinding = {};
        samplerLayoutBinding.binding = 1;
        samplerLayoutBinding.descriptorCount = 1;
        samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        samplerLayoutBinding.pImmutableSamplers = nullptr;
        samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        
        array<VkDescriptorSetLayoutBinding,2> bindings = {uboLayoutBinding, samplerLayoutBinding};
        
        VkDescriptorSetLayoutCreateInfo layoutInfo = {};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
        layoutInfo.pBindings = bindings.data();
        
        if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
            throw runtime_error("failed to create descriptor set layout!!");
        }
        
        
    }
    
    void createUniformBuffer() {
        VkDeviceSize bufferSize = sizeof(UniformBufferObject);
        
        uniformBuffers.resize(swapChainImages.size());
        uniformBuffersMemory.resize(swapChainImages.size());
        
        for (size_t i = 0; i < swapChainImages.size(); i++) {
            createBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, uniformBuffers[i], uniformBuffersMemory[i]);
        }
    }
    
    void updateUniformBuffer(uint32_t currentImage) {
        static auto startTime = chrono::high_resolution_clock::now(); // 開始時の時間
        
        auto currentTime = chrono::high_resolution_clock::now(); // 関数実行時の時間
        float time = chrono::duration<float, chrono::seconds::period>(currentTime - startTime).count(); // スタートしてからの時間

        
        UniformBufferObject ubo = {};
        ubo.model = glm::rotate(glm::mat4(1.0f), float(exp(1.0 - fmod(time, 1.0)) - 1.0) * glm::radians(180.0f), glm::vec3(0.f, 0.f, 1.0f * int((int(time) % 2) * 2 - 1)));
        
        ubo.view = glm::lookAt(glm::vec3(2.0f), glm::vec3(0.0f), glm::vec3(sin(time), cos(time), -sin(time)));
        
        ubo.proj = glm::perspective(glm::radians(45.0f), swapChainExtent.width / (float) swapChainExtent.height, 0.1f, 10.0f);
        
        ubo.proj[1][1] *= -1; // vulkanはopenGLとY軸反転
        
        void*data;
        vkMapMemory(device, uniformBuffersMemory[currentImage], 0, sizeof(ubo), 0, &data);
        memcpy(data, &ubo, sizeof(ubo));
        vkUnmapMemory(device, uniformBuffersMemory[currentImage]);
    }
    
    void createDescriptorPool() {
        array<VkDescriptorPoolSize, 2> poolSizes = {};
        poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSizes[0].descriptorCount = static_cast<uint32_t>(swapChainImages.size());
        poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSizes[1].descriptorCount = static_cast<uint32_t>(swapChainImages.size());
        
        VkDescriptorPoolCreateInfo poolInfo = {};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size()); // descriptorの数
        poolInfo.pPoolSizes = poolSizes.data();
        
        poolInfo.maxSets = static_cast<uint32_t>(swapChainImages.size());
        // あとでdescriptorを変更する場合、poolInfo.flagsで指定できる
        
        if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
            throw runtime_error("failed to create descriptor pool!");
        }
    }
    
    void createDescriptorSets() {
        vector<VkDescriptorSetLayout> layouts(swapChainImages.size(), descriptorSetLayout);
        VkDescriptorSetAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = descriptorPool;
        allocInfo.descriptorSetCount = static_cast<uint32_t>(swapChainImages.size());
        allocInfo.pSetLayouts = layouts.data();
        
        descriptorSets.resize(swapChainImages.size());
        if (vkAllocateDescriptorSets(device, &allocInfo, &descriptorSets[0]) != VK_SUCCESS) {
            throw runtime_error("failed to allocate descriptor sets!");
        }
        
        for (size_t i = 0; i < swapChainImages.size(); i++) {
            VkDescriptorBufferInfo bufferInfo = {};
            bufferInfo.buffer = uniformBuffers[i];
            bufferInfo.offset = 0;
            bufferInfo.range = sizeof(UniformBufferObject);
            
            VkDescriptorImageInfo imageInfo = {};
            imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageInfo.imageView = textureImageView;
            imageInfo.sampler = textureSampler;
            
            array<VkWriteDescriptorSet, 2> descriptorWrites = {};
            descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[0].dstSet = descriptorSets[i];
            descriptorWrites[0].dstBinding = 0;
            descriptorWrites[0].dstArrayElement = 0; // descriptorは配列になる可能性があるのでその場合は変更する必要がある、アップデートしたいdescriptorの最初のインデックス
            
            descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            descriptorWrites[0].descriptorCount = 1; // 変更したいdescriptorの数
            
            descriptorWrites[0].pBufferInfo = &bufferInfo;
            descriptorWrites[0].pTexelBufferView = nullptr;
            
            descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[1].dstSet = descriptorSets[i];
            descriptorWrites[1].dstBinding = 1;
            descriptorWrites[1].dstArrayElement = 0;
            descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptorWrites[1].descriptorCount = 1;
            descriptorWrites[1].pImageInfo = &imageInfo;
            
            vkUpdateDescriptorSets(device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
        }
    }
    
    void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout) {
        // barrierはパイプライン内でシェーダーが実行される前にバリアを設けることで、テクスチャの情報などを確実にシェーダーが読み込む前に書き込まれてるいるようにすることができる
        VkCommandBuffer commandBuffer = beginSingleTimeCommands();
        
        VkImageMemoryBarrier barrier = {};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = oldLayout;
        barrier.newLayout = newLayout;
        
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        
        barrier.image = image;
        
        if (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
            
            if (hasStencilComponent(format)) {
                barrier.subresourceRange.aspectMask != VK_IMAGE_ASPECT_STENCIL_BIT;
            }
        } else {
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        }
        
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        
        VkPipelineStageFlags sourceStage;
        VkPipelineStageFlags destinationStage;
        
        if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            
            sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            
            sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        } else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            
            sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            destinationStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        } else {
            throw invalid_argument("unsupported layout transition!");
        }
        
        vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
        
        endSingleTimeCommands(commandBuffer);
    }
    
    void createTextureImage() {
        int texWidth, texHeight, texChannels;
        stbi_uc* pixels = stbi_load("/Users/Namikawa/Documents/Vulkan/DrawTriangle/DrawTriangle/texture/texture.png", &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
        
        VkDeviceSize imageSize = texWidth * texHeight * 4;
        if (!pixels) {
            throw runtime_error("failed to load texture image!");
        }
        
        // texture用のstagingbufferの作成
        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferMemory;
        createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);
        
        // ステージングバッファにstbで読んだピクセルをコピー
        void* data;
        vkMapMemory(device, stagingBufferMemory, 0, imageSize, 0, &data);
        memcpy(data, pixels, static_cast<size_t>(imageSize));
        vkUnmapMemory(device, stagingBufferMemory);
        
        // stbのピクセルはもういらないので解放
        stbi_image_free(pixels);
        
        createImage(texWidth, texHeight, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, textureImage, textureImageMemory);
        
        transitionImageLayout(textureImage, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        copyBufferToImage(stagingBuffer, textureImage, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));
        
        transitionImageLayout(textureImage, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        
        vkDestroyBuffer(device, stagingBuffer, nullptr);
        vkFreeMemory(device, stagingBufferMemory, nullptr);
    }
    
    void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) {
        VkCommandBuffer commandBuffer = beginSingleTimeCommands();
        
        // どこの部分をコピーするかの領域の設定
        VkBufferImageCopy region = {};
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;
        
        region.imageOffset = {0,0,0};
        region.imageExtent = {
            width,
            height,
            1
        };
        
        vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
        
        endSingleTimeCommands(commandBuffer);
    }
    
    VkCommandBuffer beginSingleTimeCommands() {
        VkCommandBufferAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = commandPool;
        allocInfo.commandBufferCount = 1;
        
        VkCommandBuffer commandBuffer;
        vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);
        
        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT; // 今回は最初の一回しか行わないので、一回だけの設定
        
        vkBeginCommandBuffer(commandBuffer, &beginInfo);
        
        return commandBuffer;
    }
    
    void endSingleTimeCommands(VkCommandBuffer commandBuffer) {
        vkEndCommandBuffer(commandBuffer);
        
        VkSubmitInfo submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;
        
        vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(graphicsQueue); // 転送キューがIdleになるのをまつ
                                        // ここで複数の転送キューがある場合、フェンスを利用してvkWaitForFenceで行っても良い（最適化を行える）
        
        vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
    }
    
    void createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory) {
        // VkImage用のcreateInfo
        VkImageCreateInfo imageInfo = {};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = width;
        imageInfo.extent.height = height;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        
        imageInfo.format = format;
        // タイリングを指定、直接メモリの中のテクセルにアクセスするとき(staging image)はLINEARを指定する、ステージングバッファを使うときはシェーダから効率的にアクセスするためにOPTIMALを使用する
        imageInfo.tiling = tiling;
        
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; // タイリングをLINEARにした場合変更する必要がある
        
        imageInfo.usage = usage;
        
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE; // 異なるグラフィックキューで共有する場合に変更する
        
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT; // マルチサンプリングに関する設定
        imageInfo.flags = 0; // 例えば3Dテクスチャを使っていた時に空気に大量のメモリを割り当てたくない時に使用することができる
        
        if (vkCreateImage(device, &imageInfo, nullptr, &image) != VK_SUCCESS) throw runtime_error("failed to create image!");
        
        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(device, image, &memRequirements);
        
        VkMemoryAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);
        
        if (vkAllocateMemory(device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) throw runtime_error("failed to allocate image memory!");
        
        vkBindImageMemory(device, image, imageMemory, 0);
    }
    
    void createTextureImageView() {
        textureImageView = createImageView(textureImage, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT);
    }
    
    void createDepthResources() {
        VkFormat depthFormat = findDepthFormat();
        
        depthImages.resize(swapChainImages.size());
        depthImagesMemory.resize(swapChainImages.size());
        depthImagesView.resize(swapChainImages.size());
        
        for (size_t i = 0; i < swapChainImageViews.size(); i++) {
            createImage(swapChainExtent.width, swapChainExtent.height, depthFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, depthImages[i], depthImagesMemory[i]);
            depthImagesView[i] = createImageView(depthImages[i], depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);
            
            transitionImageLayout(depthImages[i], depthFormat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
            
            
        }
    }
    
    bool hasStencilComponent(VkFormat format) {
        return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
    }
    
    VkFormat findDepthFormat() {
        return findSupportedFormat({VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT}, VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
    }
    
    VkFormat findSupportedFormat(const vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) {
        for (VkFormat format : candidates) {
            VkFormatProperties props;
            vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);
            
            if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
                return format;
            } else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
                return format;
            }
        }
        
        throw runtime_error("failed to find supported format!");
    }
    
    VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags) {
        VkImageViewCreateInfo viewInfo = {};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = format;
        viewInfo.subresourceRange.aspectMask = aspectFlags;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;
        // ここでviewInfo.componentsはVK_COMPONENT_SWIZZLE_IDENTITYが常に0と定義されているので省略している
        // スウィズル演算子の設定
//        viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
//        viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
//        viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
//        viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        
        VkImageView imageView;
        
        if (vkCreateImageView(device, &viewInfo, nullptr, &imageView) != VK_SUCCESS) throw runtime_error("failed to create texture image view!");
        
        return imageView;
    }
    
    void createTextureSampler() {
        VkSamplerCreateInfo samplerInfo = {};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_LINEAR; // oversampling
        samplerInfo.minFilter = VK_FILTER_LINEAR; // undersampling
        
        // テクスチャの座標ごとにモードを指定できる
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        
        samplerInfo.anisotropyEnable = VK_TRUE;
        samplerInfo.maxAnisotropy = 16;
        
        samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK; // ボーダーカラー
        
        samplerInfo.unnormalizedCoordinates = VK_FALSE; // 座標をノーマライズするか（true なら [(0,0):(texWidth,texHeight))になる）
        
        samplerInfo.compareEnable = VK_FALSE; // 主にシャドウマップのPCFに使われる（多分、シャドウマップを作成する時に作成したいところに処理が入らないなどをなくすためのやつ）
        samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
        
        // ミップマップに関する設定
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.mipLodBias = 0.0f;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = 0.0f;
        
        if (vkCreateSampler(device, &samplerInfo, nullptr, &textureSampler) != VK_SUCCESS) throw runtime_error("failed to create texture sampler!");
    }
    
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
        // propertiesでメモリに対して要求する機能を指定
        VkPhysicalDeviceMemoryProperties memProperties;
        vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);
        
        for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
            if (typeFilter & (1 << i) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
                return i;
            }
        }
        
        throw runtime_error("failed to find suitable memory type!");
    }
    
    VkShaderModule createShaderModule(const vector<char>& code) {
        VkShaderModuleCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = code.size();
        createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());
        
        VkShaderModule shaderModule;
        if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
            throw runtime_error("failed to create shader module!");
        }
        
        return shaderModule;
    }
    
    // 例えば使用できるデバイスの中で一番高いものを使用したい場合
    int rateDeviceSuitability(VkPhysicalDevice device) {
        // 基本的な名前・タイプ・サポートされているVulkanのバージョンなどを取得することができる
        VkPhysicalDeviceProperties deviceProperties;
        vkGetPhysicalDeviceProperties(device, &deviceProperties);
        // VR向けのマルチビューポートや圧縮テクスチャ、64bit浮動小数点などのオプション機能
        VkPhysicalDeviceFeatures deviceFeatures;
        vkGetPhysicalDeviceFeatures(device, &deviceFeatures);
        
        // 前略
        
        int score = 0;
        
        // Discrete GPUs have a significant performance advantage
        if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            score += 1000;
        }
        
        // Maximum possible GPUs have a significant performance advantage
        score += deviceProperties.limits.maxImageDimension2D;
        
        // Application can't function without geometry shaders
//        if (!deviceFeatures.geometryShader) {
//            return 0;
//        }
        
        QueueFamilyIndices indices = findQueueFamilies(device);
        
        
        if (!indices.isComplete()) {
            score = 0;
        }
        
        if (!checkDeviceExtensionSupport(device)) {
//            score = 0;
        }
        
        SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
        if (swapChainSupport.formats.empty() || swapChainSupport.presentModes.empty()) {
            score = 0;
        }
        
//        if (!deviceFeatures.samplerAnisotropy) {
//            score = 0;
//        }
        
        cout << "device name : " << deviceProperties.deviceName << endl;
        cout << "\t" << "device type : " << deviceProperties.deviceType << endl;
        cout << "\t" << "API support : " << indices.isComplete() << endl;
        cout << "\t" << "geometry shader support : " << deviceFeatures.geometryShader << endl;
        cout << "\t" << "extension support : " << checkDeviceExtensionSupport(device) << endl;
        cout << "\t" << "swapchain support : " << (!swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty()) << endl;
        cout << "\t" << "anisotropy filter support : " << !deviceFeatures.samplerAnisotropy << endl;
        cout << "\t" << "device score : " << score << endl;
        
        return score;
        
    }
    
    
    // デバイスの拡張機能(スワップチェーンなど)をサポートしているかを確認
    bool checkDeviceExtensionSupport(VkPhysicalDevice device) {
        uint32_t extensionCount;
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
        
        vector<VkExtensionProperties> avaliableExtensions(extensionCount);
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, avaliableExtensions.data());
        
        set<string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());
        
        for (const auto& extension : avaliableExtensions) {
            requiredExtensions.erase(extension.extensionName);
        }
        
        return requiredExtensions.empty();
    }
    
    // バッファの画像サイズの選択
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) {
        if (capabilities.currentExtent.width != numeric_limits<uint32_t>::max()) {
            return capabilities.currentExtent;
        } else {
            int width, height;
            glfwGetFramebufferSize(window, &width, &height);
            
            VkExtent2D actualExtent = {
                static_cast<uint32_t>(width),
                static_cast<uint32_t>(height)
            };
            
//            VkExtent2D actualExtent = {WIDTH, HEIGHT};
            
            actualExtent.width = max(capabilities.minImageExtent.width, min(capabilities.maxImageExtent.width, actualExtent.width));
            actualExtent.height = max(capabilities.minImageExtent.height, min(capabilities.maxImageExtent.height, actualExtent.height));
            
            return actualExtent;
        }
    }
    
    // どのように画像差し替えるか
    VkPresentModeKHR chooseSwapPresentMode(const vector<VkPresentModeKHR> avaliablePresentModes) {
        // 基本的にはVertidcal Sync（正確には違う？）で
        VkPresentModeKHR bestMode = VK_PRESENT_MODE_FIFO_KHR;
        
        // できればトリプルバッファリングでやりたい
        for (const auto& avaliablePresentMode : avaliablePresentModes) {
            if (avaliablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
                return avaliablePresentMode;
            } else if (avaliablePresentMode == VK_PRESENT_MODE_IMMEDIATE_KHR) {
                // 最悪これ使うしかない
                bestMode = avaliablePresentMode;
            }
        }
        return bestMode;
    }
    
    // バッファのフォーマット
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const vector<VkSurfaceFormatKHR>& avaliableFormats) {
        // もし、使えるフォーマットが使いたいフォーマットだけならそれ使う
        if (avaliableFormats.size() == 1 && avaliableFormats[0].format == VK_FORMAT_UNDEFINED) {
            return {VK_FORMAT_B8G8R8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
        }
        
        // 使いたいフォーマットがあるか探す
        for (const auto& avaliableFormat : avaliableFormats) {
            if (avaliableFormat.format == VK_FORMAT_B8G8R8_UNORM && avaliableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                return avaliableFormat;
            }
        }
        
        // いいのがなかったら一番目使っとけ
        return avaliableFormats[0];
    }
    
    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device) {
        SwapChainSupportDetails details;
        
        // キャパビリティ
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);
        
        // フォーマット・カラー
        uint32_t formatCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(device ,surface, &formatCount, nullptr);
        
        if (formatCount != 0) {
            details.formats.resize(formatCount);
            vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
        }
        
        // プレゼンテーションモード
        uint32_t presentModeCount;
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);
        
        if (presentModeCount != 0) {
            details.presentModes.resize(presentModeCount);
            vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
        }
        
        return details;
    }
    
    
    bool isDeviceSuitable(VkPhysicalDevice device) {
        // この関数で、GPUなどが実際に使いたい機能を使えるかを判断する
        
        // 基本的な名前・タイプ・サポートされているVulkanのバージョンなどを取得することができる
        VkPhysicalDeviceProperties deviceProperties;
        vkGetPhysicalDeviceProperties(device, &deviceProperties);
        
        // VR向けのマルチビューポートや圧縮テクスチャ、64bit浮動小数点などのオプション機能
        VkPhysicalDeviceFeatures deviceFeatures;
        vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

        // 例えば、geometry shaderを使いたい場合は以下のようにする
//        return deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU && deviceFeatures.geometryShader;
        
//        return true;
        
        QueueFamilyIndices indices = findQueueFamilies(device);
        
        return indices.isComplete();
    }
    
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device) {
        QueueFamilyIndices indices;
        
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
        
        vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());
        
        int i = 0;
        for (const auto& queueFamily : queueFamilies) {
            if (queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                indices.graphicsFamily = i;
            }
            
            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
            
            if (queueFamily.queueCount  > 0 && presentSupport) {
                indices.presentFamily = i;
            }
            
            if (indices.isComplete()) {
                break;
            }
            
            ++i;
        }
        
        return indices;
    }
    
    vector<const char*> getRequiredExtensions() {
        uint32_t glfwExtensionCount = 0;
        const char** glfwExtensions;
        glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
        
        vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

        if (enableValidationLayers) {
            extensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
        }
        
        return extensions;
    }
    
    bool checkValidationLayerSupport() {
        uint32_t layerCount;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
        
        vector<VkLayerProperties> avaliableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, avaliableLayers.data());
        
        for (const char* layerName : validationLayers) {
            bool layerFound = false;
            cout << "layerName : " << endl;
            for (const auto& layerProperties : avaliableLayers) {
                if (strcmp(layerName, layerProperties.layerName) == 0) {
                    layerFound = true;
                    break;
                }
            }
            
            if(!layerFound) return false;
        }
        
        return true;
    }
    
    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objType, uint64_t obj, size_t location, int32_t code, const char* layerPrefix, const char* msg, void* userDate) {
        cerr << "validation layer: " << msg << endl;
        
        return VK_FALSE;
    }
};

int main(int argc, const char * argv[]) {
    HelloTriangleApplication app;

    try {
        app.run();
    } catch (const runtime_error& e) {
        cerr << e.what() << endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
