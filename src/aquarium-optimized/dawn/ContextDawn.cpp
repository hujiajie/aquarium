//
// Copyright (c) 2019 The Aquarium Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// DeviceDawn.cpp: Implements accessing functions to the graphics API of Dawn.

#include <dawn/dawn_proc.h>
#include <dawn/dawn_wsi.h>
#include <dawn/webgpu.h>
#include <dawn/webgpu_cpp.h>
#include <dawn_native/DawnNative.h>
#include <array>
#include <iostream>
#include <shaderc/shaderc.hpp>
#include <string>
#include <vector>

#include "../FishModel.h"
#include "BufferDawn.h"
#include "ContextDawn.h"
#include "FishModelDawn.h"
#include "FishModelInstancedDrawDawn.h"
#include "GenericModelDawn.h"
#include "InnerModelDawn.h"
#include "OutsideModelDawn.h"
#include "ProgramDawn.h"
#include "SeaweedModelDawn.h"
#include "TextureDawn.h"

#include "common/AQUARIUM_ASSERT.h"
#include "common/Constants.h"
#include "imgui_impl_dawn.h"
#include "imgui_impl_glfw.h"
#include "utils/BackendBinding.h"
#include "utils/ComboRenderPipelineDescriptor.h"
#include "utils/SystemUtils.h"

#include "../Aquarium.h"

ContextDawn::ContextDawn(BACKENDTYPE backendType)
    : queue(nullptr),
      groupLayoutGeneral(nullptr),
      bindGroupGeneral(nullptr),
      groupLayoutWorld(nullptr),
      bindGroupWorld(nullptr),
      groupLayoutFishPer(nullptr),
      fishPersBuffer(nullptr),
      bindGroupFishPers(nullptr),
      stagingBuffer(nullptr),
      fishPers(nullptr),
      mDevice(nullptr),
      mWindow(nullptr),
      mInstance(),
      mSwapchain(nullptr),
      mCommandEncoder(nullptr),
      mRenderPass(nullptr),
      mRenderPassDescriptor({}),
      mSceneRenderTargetView(nullptr),
      mSceneDepthStencilView(nullptr),
      mPipeline(nullptr),
      mBindGroup(nullptr),
      mPreferredSwapChainFormat(wgpu::TextureFormat::RGBA8Unorm),
      mEnableMSAA(false),
      mappedData(nullptr)
{
    mResourceHelper = new ResourceHelper("dawn", "", backendType);
    initAvailableToggleBitset(backendType);
}

ContextDawn::~ContextDawn()
{
    delete mResourceHelper;
    if (mWindow != nullptr)
    {
        destoryImgUI();
    }

    mSceneRenderTargetView   = nullptr;
    mSceneDepthStencilView   = nullptr;
    mBackbufferView           = nullptr;
    mPipeline                = nullptr;
    mBindGroup               = nullptr;
    mLightWorldPositionBuffer = nullptr;
    mLightBuffer              = nullptr;
    mFogBuffer                = nullptr;
    mCommandEncoder          = nullptr;
    mCommandBuffers.clear();
    mRenderPass              = nullptr;
    mRenderPassDescriptor    = {};
    groupLayoutGeneral       = nullptr;
    bindGroupGeneral         = nullptr;
    groupLayoutWorld         = nullptr;
    bindGroupWorld           = nullptr;

    groupLayoutFishPer = nullptr;
    destoryFishResource();

    stagingBuffer = nullptr;

    mSwapchain               = nullptr;
    queue                    = nullptr;
    mDevice                  = nullptr;
}

bool ContextDawn::initialize(
    BACKENDTYPE backend,
    const std::bitset<static_cast<size_t>(TOGGLE::TOGGLEMAX)> &toggleBitset,
    int windowWidth,
    int windowHeight)
{
    dawn_native::BackendType backendType = dawn_native::BackendType::Null;

    switch (backend)
    {
        case BACKENDTYPE::BACKENDTYPEDAWND3D12:
        {
            backendType = dawn_native::BackendType::D3D12;
            break;
        }
        case BACKENDTYPE::BACKENDTYPEDAWNVULKAN:
        {
            backendType = dawn_native::BackendType::Vulkan;
            break;
        }
        case BACKENDTYPE::BACKENDTYPEDAWNMETAL:
        {
            backendType = dawn_native::BackendType::Metal;
            break;
        }
        case BACKENDTYPE::BACKENDTYPEOPENGL:
        {
            backendType = dawn_native::BackendType::OpenGL;
            break;
        }
        default:
        {
            std::cerr << "Backend type can not reached." << std::endl;
            return false;
        }
    }

    mEnableMSAA = toggleBitset.test(static_cast<size_t>(TOGGLE::ENABLEMSAAx4));

    // initialise GLFW
    if (!glfwInit())
    {
        std::cout << "Failed to initialise GLFW" << std::endl;
        return false;
    }

    utils::SetupGLFWWindowHintsForBackend(backendType);
    // set full screen
    glfwWindowHint(GLFW_VISIBLE, GL_FALSE);

    GLFWmonitor *pMonitor   = glfwGetPrimaryMonitor();
    const GLFWvidmode *mode = glfwGetVideoMode(pMonitor);
    mClientWidth            = mode->width;
    mClientHeight           = mode->height;

    setWindowSize(windowWidth, windowHeight);

    if (toggleBitset.test(static_cast<size_t>(TOGGLE::ENABLEFULLSCREENMODE)))
    {
        mWindow = glfwCreateWindow(mClientWidth, mClientHeight, "Aquarium", pMonitor, nullptr);
    }
    else
    {
        mWindow = glfwCreateWindow(mClientWidth, mClientHeight, "Aquarium", nullptr, nullptr);
    }

    if (mWindow == nullptr)
    {
        std::cout << "Failed to open GLFW window." << std::endl;
        glfwTerminate();
        return false;
    }

    // Get the resolution of screen
    glfwGetFramebufferSize(mWindow, &mClientWidth, &mClientHeight);

    mInstance = std::make_unique<dawn_native::Instance>();

	// Enable debug layer in Debug mode
	#if defined(_DEBUG)
	mInstance->EnableBackendValidation(true);
	#endif

    utils::DiscoverAdapter(mInstance.get(), mWindow, backendType);

    dawn_native::Adapter backendAdapter;
    if (!GetHardwareAdapter(mInstance, &backendAdapter, backendType, toggleBitset))
    {
        return false;
    }

    WGPUDevice backendDevice;
    dawn_native::DeviceDescriptor descriptor;
    if (toggleBitset.test(static_cast<size_t>(TOGGLE::TURNOFFVSYNC)))
    {
        const char *turnOffVsync = "turn_off_vsync";
        descriptor.forceEnabledToggles.push_back(turnOffVsync);
    }
    if (toggleBitset.test(static_cast<size_t>(TOGGLE::DISABLED3D12RENDERPASS)))
    {
        const char *useD3D12RenderPass = "use_d3d12_render_pass";
        descriptor.forceDisabledToggles.push_back(useD3D12RenderPass);
    }
    if (toggleBitset.test(static_cast<size_t>(TOGGLE::DISABLEDAWNVALIDATION)))
    {
        const char *skipValidation = "skip_validation";
        descriptor.forceEnabledToggles.push_back(skipValidation);
    }
    backendDevice = backendAdapter.CreateDevice(&descriptor);

    DawnProcTable backendProcs = dawn_native::GetProcs();

    utils::BackendBinding *binding = utils::CreateBinding(backendType, mWindow, backendDevice);
    if (binding == nullptr)
    {
        return false;
    }

    dawnProcSetProcs(&backendProcs);
    mDevice = wgpu::Device::Acquire(backendDevice);

    queue = mDevice.CreateQueue();
    wgpu::SwapChainDescriptor swapChainDesc;
    swapChainDesc.implementation = binding->GetSwapChainImplementation();
    mSwapchain                   = mDevice.CreateSwapChain(&swapChainDesc);

    mPreferredSwapChainFormat =
        static_cast<wgpu::TextureFormat>(binding->GetPreferredSwapChainTextureFormat());
    mSwapchain.Configure(mPreferredSwapChainFormat, kSwapchainBackBufferUsage, mClientWidth,
                         mClientHeight);

    dawn_native::PCIInfo info = backendAdapter.GetPCIInfo();
    std::string renderer      = info.name;
    std::cout << renderer << std::endl;
    mResourceHelper->setRenderer(renderer);

    // When MSAA is enabled, we create an intermediate multisampled texture to render the scene to.
    if (mEnableMSAA)
    {
        mSceneRenderTargetView = createMultisampledRenderTargetView();
    }

    mSceneDepthStencilView = createDepthStencilView();

    // TODO(jiawei.shao@intel.com): support recreating swapchain when window is resized on all
    // backends
    if (backend == BACKENDTYPE::BACKENDTYPEDAWNVULKAN)
    {
        glfwSetFramebufferSizeCallback(mWindow, framebufferResizeCallback);
        glfwSetWindowUserPointer(mWindow, this);
    }

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer bindings
    // We use glfw to create window for dawn backend as well.
    // Because imgui doesn't have dawn backend, we rewrite the functions by dawn API in
    // imgui_impl_dawn.cpp and imgui_impl_dawn.h
    ImGui_ImplGlfw_InitForOpenGL(mWindow, true);
    ImGui_ImplDawn_Init(this, mPreferredSwapChainFormat);

    return true;
}

void ContextDawn::framebufferResizeCallback(GLFWwindow *window, int width, int height) {
    ContextDawn *contextDawn = reinterpret_cast<ContextDawn *>(glfwGetWindowUserPointer(window));
    contextDawn->mIsSwapchainOutOfDate = true;
}

bool ContextDawn::GetHardwareAdapter(
    std::unique_ptr<dawn_native::Instance> &instance,
    dawn_native::Adapter *backendAdapter,
    dawn_native::BackendType backendType,
    const std::bitset<static_cast<size_t>(TOGGLE::TOGGLEMAX)> &toggleBitset)
{
    bool enableIntegratedGpu = toggleBitset.test(static_cast<size_t>(TOGGLE::INTEGRATEDGPU));
    bool enableDiscreteGpu   = toggleBitset.test(static_cast<size_t>(TOGGLE::DISCRETEGPU));
    bool useDefaultGpu       = (enableDiscreteGpu | enableIntegratedGpu) == false ? true : false;
    bool result             = false;

    // Get an adapter for the backend to use, and create the Device.
    for (auto &adapter : instance->GetAdapters())
    {
        if (adapter.GetBackendType() == backendType)
        {
            if (useDefaultGpu ||
                (enableDiscreteGpu &&
                 adapter.GetDeviceType() == dawn_native::DeviceType::DiscreteGPU) ||
                (enableIntegratedGpu &&
                 adapter.GetDeviceType() == dawn_native::DeviceType::IntegratedGPU))
            {
                *backendAdapter = adapter;
                result          = true;
                break;
            }
        }
    }

    if (!result)
    {
        std::cerr << "Failed to create adapter." << std::endl;
        return false;
    }

    return true;
}

void ContextDawn::initAvailableToggleBitset(BACKENDTYPE backendType)
{
    mAvailableToggleBitset.set(static_cast<size_t>(TOGGLE::ENABLEMSAAx4));
    mAvailableToggleBitset.set(static_cast<size_t>(TOGGLE::ENABLEINSTANCEDDRAWS));
    mAvailableToggleBitset.set(static_cast<size_t>(TOGGLE::ENABLEDYNAMICBUFFEROFFSET));
    mAvailableToggleBitset.set(static_cast<size_t>(TOGGLE::DISCRETEGPU));
    mAvailableToggleBitset.set(static_cast<size_t>(TOGGLE::INTEGRATEDGPU));
    mAvailableToggleBitset.set(static_cast<size_t>(TOGGLE::ENABLEFULLSCREENMODE));
    mAvailableToggleBitset.set(static_cast<size_t>(TOGGLE::BUFFERMAPPINGASYNC));
    mAvailableToggleBitset.set(static_cast<size_t>(TOGGLE::TURNOFFVSYNC));
    mAvailableToggleBitset.set(static_cast<size_t>(TOGGLE::DISABLED3D12RENDERPASS));
    mAvailableToggleBitset.set(static_cast<size_t>(TOGGLE::DISABLEDAWNVALIDATION));
}

Texture *ContextDawn::createTexture(const std::string &name, const std::string &url)
{
    Texture *texture = new TextureDawn(this, name, url);
    texture->loadTexture();
    return texture;
}

Texture *ContextDawn::createTexture(const std::string &name, const std::vector<std::string> &urls)
{
    Texture *texture = new TextureDawn(this, name, urls);
    texture->loadTexture();
    return texture;
}

wgpu::Texture ContextDawn::createTexture(const wgpu::TextureDescriptor &descriptor) const
{
    return mDevice.CreateTexture(&descriptor);
}

wgpu::Sampler ContextDawn::createSampler(const wgpu::SamplerDescriptor &descriptor) const
{
    return mDevice.CreateSampler(&descriptor);
}

wgpu::Buffer ContextDawn::createBufferFromData(const void *pixels,
                                               int size,
                                               wgpu::BufferUsage usage) const
{
    return utils::CreateBufferFromData(mDevice, pixels, size, usage);
}

wgpu::BufferCopyView ContextDawn::createBufferCopyView(const wgpu::Buffer &buffer,
                                                       uint32_t offset,
                                                       uint32_t rowPitch,
                                                       uint32_t imageHeight) const
{

    return utils::CreateBufferCopyView(buffer, offset, rowPitch, imageHeight);
}

wgpu::TextureCopyView ContextDawn::createTextureCopyView(wgpu::Texture texture,
                                                         uint32_t level,
                                                         uint32_t slice,
                                                         wgpu::Origin3D origin)
{

    return utils::CreateTextureCopyView(texture, level, slice, origin);
}

wgpu::CommandBuffer ContextDawn::copyBufferToTexture(const wgpu::BufferCopyView &bufferCopyView,
                                                     const wgpu::TextureCopyView &textureCopyView,
                                                     const wgpu::Extent3D &ext3D) const
{
    wgpu::CommandEncoder encoder = mDevice.CreateCommandEncoder();
    encoder.CopyBufferToTexture(&bufferCopyView, &textureCopyView, &ext3D);
    wgpu::CommandBuffer copy = encoder.Finish();
    return copy;
}

wgpu::CommandBuffer ContextDawn::copyBufferToBuffer(wgpu::Buffer const &srcBuffer,
                                                    uint64_t srcOffset,
                                                    wgpu::Buffer const &destBuffer,
                                                    uint64_t destOffset,
                                                    uint64_t size)
{
    wgpu::CommandEncoder encoder = mDevice.CreateCommandEncoder();
    encoder.CopyBufferToBuffer(srcBuffer, srcOffset, destBuffer, destOffset, size);
    wgpu::CommandBuffer copy = encoder.Finish();

    return copy;
}

wgpu::ShaderModule ContextDawn::createShaderModule(utils::SingleShaderStage stage,
                                                   const std::string &str) const
{
    return utils::CreateShaderModule(mDevice, stage, str.c_str());
}

wgpu::BindGroupLayout ContextDawn::MakeBindGroupLayout(
    std::initializer_list<wgpu::BindGroupLayoutBinding> bindingsInitializer) const
{

    return utils::MakeBindGroupLayout(mDevice, bindingsInitializer);
}

wgpu::PipelineLayout ContextDawn::MakeBasicPipelineLayout(
    std::vector<wgpu::BindGroupLayout> bindingsInitializer) const
{
    wgpu::PipelineLayoutDescriptor descriptor;

    descriptor.bindGroupLayoutCount = static_cast<uint32_t>(bindingsInitializer.size());
    descriptor.bindGroupLayouts = bindingsInitializer.data();

    return mDevice.CreatePipelineLayout(&descriptor);
}

wgpu::RenderPipeline ContextDawn::createRenderPipeline(
    wgpu::PipelineLayout mPipelineLayout,
    ProgramDawn *mProgramDawn,
    const wgpu::VertexStateDescriptor &mVertexStateDescriptor,
    bool enableBlend) const
{
    const wgpu::ShaderModule &mVsModule = mProgramDawn->getVSModule();
    const wgpu::ShaderModule &mFsModule = mProgramDawn->getFSModule();

    wgpu::BlendDescriptor blendDescriptor;
    blendDescriptor.operation = wgpu::BlendOperation::Add;
    if (enableBlend)
    {
        blendDescriptor.srcFactor = wgpu::BlendFactor::SrcAlpha;
        blendDescriptor.dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha;
    }
    else
    {
        blendDescriptor.srcFactor = wgpu::BlendFactor::One;
        blendDescriptor.dstFactor = wgpu::BlendFactor::Zero;
    }

    wgpu::ColorStateDescriptor ColorStateDescriptor;
    ColorStateDescriptor.colorBlend     = blendDescriptor;
    ColorStateDescriptor.alphaBlend     = blendDescriptor;
    ColorStateDescriptor.writeMask      = wgpu::ColorWriteMask::All;

    wgpu::RasterizationStateDescriptor rasterizationState;
    rasterizationState.nextInChain         = nullptr;
    rasterizationState.frontFace           = wgpu::FrontFace::CCW;
    rasterizationState.cullMode            = wgpu::CullMode::Back;
    rasterizationState.depthBias           = 0;
    rasterizationState.depthBiasSlopeScale = 0.0;
    rasterizationState.depthBiasClamp      = 0.0;

    // test
    utils::ComboRenderPipelineDescriptor descriptor(mDevice);
    descriptor.layout                               = mPipelineLayout;
    descriptor.vertexStage.module                   = mVsModule;
    descriptor.cFragmentStage.module                = mFsModule;
    descriptor.vertexState                          = &mVertexStateDescriptor;
    descriptor.depthStencilState                    = &descriptor.cDepthStencilState;
    descriptor.cDepthStencilState.format            = wgpu::TextureFormat::Depth24PlusStencil8;
    descriptor.colorStateCount                      = 1;
    descriptor.cColorStates[0]                      = ColorStateDescriptor;
    descriptor.cColorStates[0].format               = mPreferredSwapChainFormat;
    descriptor.cDepthStencilState.depthWriteEnabled = true;
    descriptor.cDepthStencilState.depthCompare      = wgpu::CompareFunction::Less;
    descriptor.primitiveTopology                    = wgpu::PrimitiveTopology::TriangleList;
    descriptor.sampleCount                          = mEnableMSAA ? 4 : 1;
    descriptor.rasterizationState                   = &rasterizationState;

    wgpu::RenderPipeline mPipeline = mDevice.CreateRenderPipeline(&descriptor);

    return mPipeline;
}

wgpu::TextureView ContextDawn::createMultisampledRenderTargetView() const
{
    wgpu::TextureDescriptor descriptor;
    descriptor.dimension       = wgpu::TextureDimension::e2D;
    descriptor.size.width      = mClientWidth;
    descriptor.size.height     = mClientHeight;
    descriptor.size.depth      = 1;
    descriptor.arrayLayerCount = 1;
    descriptor.sampleCount     = 4;
    descriptor.format          = mPreferredSwapChainFormat;
    descriptor.mipLevelCount   = 1;
    descriptor.usage           = wgpu::TextureUsage::OutputAttachment;

    return mDevice.CreateTexture(&descriptor).CreateView();
}

wgpu::TextureView ContextDawn::createDepthStencilView() const
{
    wgpu::TextureDescriptor descriptor;
    descriptor.dimension       = wgpu::TextureDimension::e2D;
    descriptor.size.width      = mClientWidth;
    descriptor.size.height     = mClientHeight;
    descriptor.size.depth      = 1;
    descriptor.arrayLayerCount = 1;
    descriptor.sampleCount     = mEnableMSAA ? 4 : 1;
    descriptor.format          = wgpu::TextureFormat::Depth24PlusStencil8;
    descriptor.mipLevelCount   = 1;
    descriptor.usage           = wgpu::TextureUsage::OutputAttachment;
    auto depthStencilTexture   = mDevice.CreateTexture(&descriptor);
    return depthStencilTexture.CreateView();
}

wgpu::Buffer ContextDawn::createBuffer(uint32_t size, wgpu::BufferUsage bit) const
{
    wgpu::BufferDescriptor descriptor;
    descriptor.size = size;
    descriptor.usage = bit;

    wgpu::Buffer buffer = mDevice.CreateBuffer(&descriptor);
    return buffer;
}

void ContextDawn::setBufferData(const wgpu::Buffer &buffer,
                                uint32_t start,
                                uint32_t size,
                                const void *pixels) const
{
    buffer.SetSubData(start, size, reinterpret_cast<const uint8_t*>(pixels));
}

wgpu::BindGroup ContextDawn::makeBindGroup(
    const wgpu::BindGroupLayout &layout,
    std::initializer_list<utils::BindingInitializationHelper> bindingsInitializer) const
{
    return utils::MakeBindGroup(mDevice, layout, bindingsInitializer);
}

void ContextDawn::initGeneralResources(Aquarium* aquarium)
{
    // initilize general uniform buffers
    groupLayoutGeneral = MakeBindGroupLayout({
        {0, wgpu::ShaderStage::Fragment, wgpu::BindingType::UniformBuffer},
        {1, wgpu::ShaderStage::Fragment, wgpu::BindingType::UniformBuffer},
    });

    mLightBuffer = createBufferFromData(&aquarium->lightUniforms, sizeof(aquarium->lightUniforms),
                                        wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Uniform);
    mFogBuffer   = createBufferFromData(&aquarium->fogUniforms, sizeof(aquarium->fogUniforms),
                                      wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Uniform);

    bindGroupGeneral =
        makeBindGroup(groupLayoutGeneral, {{0, mLightBuffer, 0, sizeof(aquarium->lightUniforms)},
                                           {1, mFogBuffer, 0, sizeof(aquarium->fogUniforms)}});

    setBufferData(mLightBuffer, 0, sizeof(LightUniforms), &aquarium->lightUniforms);
    setBufferData(mFogBuffer, 0, sizeof(FogUniforms), &aquarium->fogUniforms);

    // initilize world uniform buffers
    groupLayoutWorld = MakeBindGroupLayout({
        {0, wgpu::ShaderStage::Vertex, wgpu::BindingType::UniformBuffer},
    });

    mLightWorldPositionBuffer = createBufferFromData(
        &aquarium->lightWorldPositionUniform, sizeof(aquarium->lightWorldPositionUniform),
        wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Uniform);

    bindGroupWorld = makeBindGroup(
        groupLayoutWorld,
        {
            {0, mLightWorldPositionBuffer, 0, sizeof(aquarium->lightWorldPositionUniform)},
        });

    setBufferData(mLightWorldPositionBuffer, 0, sizeof(LightWorldPositionUniform),
                  &aquarium->lightWorldPositionUniform);

    bool enableDynamicBufferOffset =
        aquarium->toggleBitset.test(static_cast<size_t>(TOGGLE::ENABLEDYNAMICBUFFEROFFSET));
    if (enableDynamicBufferOffset)
    {
        groupLayoutFishPer = MakeBindGroupLayout({
            {0, wgpu::ShaderStage::Vertex, wgpu::BindingType::UniformBuffer, true},
        });
    }
    else
    {
        groupLayoutFishPer = MakeBindGroupLayout({
            {0, wgpu::ShaderStage::Vertex, wgpu::BindingType::UniformBuffer},
        });
    }
    reallocResource(aquarium->getPreFishCount(), aquarium->getCurFishCount(),
                    enableDynamicBufferOffset);

    // Create a big buffer for buffer mapping async
    // TODO(yizhou): Use the staging buffer to upload more data, such as vertex buffers and world
    // uniforms
    stagingBuffer = createBuffer(sizeof(FishPer) * aquarium->getCurFishCount(),
                                 wgpu::BufferUsage::MapWrite | wgpu::BufferUsage::CopySrc);
}

void ContextDawn::updateWorldlUniforms(Aquarium* aquarium)
{
    setBufferData(mLightWorldPositionBuffer, 0, sizeof(LightWorldPositionUniform),
                  &aquarium->lightWorldPositionUniform);
}

Buffer *ContextDawn::createBuffer(int numComponents, std::vector<float> *buf, bool isIndex)
{
    Buffer *buffer = new BufferDawn(this, static_cast<int>(buf->size()), numComponents, buf, isIndex);
    return buffer;
}

Buffer *ContextDawn::createBuffer(int numComponents, std::vector<unsigned short> *buf, bool isIndex)
{
    Buffer *buffer =
        new BufferDawn(this, static_cast<int>(buf->size()), numComponents, buf, isIndex);
    return buffer;
}

Program *ContextDawn::createProgram(const std::string &mVId, const std::string &mFId)
{
    ProgramDawn *program = new ProgramDawn(this, mVId, mFId);

    return program;
}

void ContextDawn::setWindowTitle(const std::string &text)
{
    glfwSetWindowTitle(mWindow, text.c_str());
}

bool ContextDawn::ShouldQuit()
{
    return glfwWindowShouldClose(mWindow);
}

void ContextDawn::KeyBoardQuit()
{
    if (glfwGetKey(mWindow, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(mWindow, GL_TRUE);
}

// Submit commands of the frame
void ContextDawn::DoFlush(const std::bitset<static_cast<size_t>(TOGGLE::TOGGLEMAX)> &toggleBitset)
{
    mRenderPass.EndPass();
    wgpu::CommandBuffer cmd = mCommandEncoder.Finish();
    mCommandBuffers.emplace_back(cmd);

    // Wait for staging buffer uploading
    if (toggleBitset.test(static_cast<size_t>(TOGGLE::BUFFERMAPPINGASYNC)))
    {
        while (mappedData == nullptr)
        {
            WaitABit();
        }
        mappedData = nullptr;

        stagingBuffer.Unmap();
    }

    Flush();

    mSwapchain.Present();

    glfwPollEvents();
}

void ContextDawn::Flush()
{
    queue.Submit(mCommandBuffers.size(), mCommandBuffers.data());
    mCommandBuffers.clear();
}

void ContextDawn::Terminate()
{
    glfwTerminate();
}

void ContextDawn::showWindow()
{
    glfwShowWindow(mWindow);
}

void ContextDawn::updateFPS(const FPSTimer &fpsTimer,
                            int *fishCount,
                            std::bitset<static_cast<size_t>(TOGGLE::TOGGLEMAX)> *toggleBitset)
{
    // Start the Dear ImGui frame
    ImGui_ImplDawn_NewFrame(mEnableMSAA);
    renderImgui(fpsTimer, fishCount, toggleBitset);
    ImGui_ImplDawn_RenderDrawData(ImGui::GetDrawData());
}

void ContextDawn::showFPS()
{
    ImGui_ImplDawn_Draw(ImGui::GetDrawData());
}

void ContextDawn::destoryImgUI()
{
    ImGui_ImplDawn_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void ContextDawn::preFrame()
{
    if (mIsSwapchainOutOfDate) {
        glfwGetFramebufferSize(mWindow, &mClientWidth, &mClientHeight);
        if (mEnableMSAA) {
            mSceneRenderTargetView = createMultisampledRenderTargetView();
        }
        mSceneDepthStencilView = createDepthStencilView();
        mSwapchain.Configure(mPreferredSwapChainFormat, kSwapchainBackBufferUsage, mClientWidth,
                             mClientHeight);

        mIsSwapchainOutOfDate = false;
    }

    mCommandEncoder = mDevice.CreateCommandEncoder();
    mBackbufferView = mSwapchain.GetCurrentTextureView();

    if (mEnableMSAA)
    {
        // If MSAA is enabled, we render to a multisampled texture and then resolve to the backbuffer
        mRenderPassDescriptor = utils::ComboRenderPassDescriptor({mSceneRenderTargetView},
                                                                 mSceneDepthStencilView);
        mRenderPassDescriptor.cColorAttachments[0].resolveTarget = mBackbufferView;
    }
    else
    {
        // When MSAA is off, we render directly to the backbuffer
        mRenderPassDescriptor =
            utils::ComboRenderPassDescriptor({mBackbufferView}, mSceneDepthStencilView);
    }

    mRenderPass = mCommandEncoder.BeginRenderPass(&mRenderPassDescriptor);
}

Model * ContextDawn::createModel(Aquarium* aquarium, MODELGROUP type, MODELNAME name, bool blend)
{
    Model *model;
    switch (type)
    {
    case MODELGROUP::FISH:
        model = new FishModelDawn(this, aquarium, type, name, blend);
        break;
    case MODELGROUP::FISHINSTANCEDDRAW:
        model = new FishModelInstancedDrawDawn(this, aquarium, type, name, blend);
        break;
    case MODELGROUP::GENERIC:
        model = new GenericModelDawn(this, aquarium, type, name, blend);
        break;
    case MODELGROUP::INNER:
        model = new InnerModelDawn(this, aquarium, type, name, blend);
        break;
    case MODELGROUP::SEAWEED:
        model = new SeaweedModelDawn(this, aquarium, type, name, blend);
        break;
    case MODELGROUP::OUTSIDE:
        model = new OutsideModelDawn(this, aquarium, type, name, blend);
        break;
    default:
        model = nullptr;
        std::cout << "can not create model type" << std::endl;
    }

    return model;
}

void ContextDawn::reallocResource(int preTotalInstance,
                                  int curTotalInstance,
                                  bool enableDynamicBufferOffset)
{
    mPreTotalInstance          = preTotalInstance;
    mCurTotalInstance          = curTotalInstance;
    mEnableDynamicBufferOffset = enableDynamicBufferOffset;

    if (curTotalInstance == 0)
        return;

    // If current fish number > pre fish number, allocate a new bigger buffer.
    // If current fish number <= prefish number, do not allocate a new one.
    // TODO(yizhou) : optimize the buffer allocation strategy.
    if (preTotalInstance >= curTotalInstance)
    {
        return;
    }

    destoryFishResource();

    fishPers = new FishPer[curTotalInstance];

    if (enableDynamicBufferOffset)
    {
        bindGroupFishPers = new wgpu::BindGroup[1];
    }
    else
    {
        bindGroupFishPers = new wgpu::BindGroup[curTotalInstance];
    }

    fishPersBuffer = createBufferFromData(fishPers, sizeof(FishPer) * curTotalInstance,
                                          wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Uniform);

    // TODO(yizhou): the staging buffer should be bigger to hold other uniform buffers in the
    // future. But now we only use the buffer to upload fish buffer, so the size is the same as fish
    // buffer.
    stagingBuffer = createBuffer(sizeof(FishPer) * curTotalInstance,
                                 wgpu::BufferUsage::MapWrite | wgpu::BufferUsage::CopySrc);

    if (enableDynamicBufferOffset)
    {
        bindGroupFishPers[0] =
            makeBindGroup(groupLayoutFishPer, {{0, fishPersBuffer, 0, sizeof(FishPer)}});
    }
    else
    {
        for (int i = 0; i < curTotalInstance; i++)
        {
            bindGroupFishPers[i] = makeBindGroup(
                groupLayoutFishPer, {{0, fishPersBuffer, sizeof(FishPer) * i, sizeof(FishPer)}});
        }
    }
}

wgpu::CreateBufferMappedResult ContextDawn::CreateBufferMapped(wgpu::BufferUsage usage,
                                                               uint64_t size)
{
    wgpu::BufferDescriptor descriptor;
    descriptor.nextInChain = nullptr;
    descriptor.size        = size;
    descriptor.usage       = usage;

    wgpu::CreateBufferMappedResult result = mDevice.CreateBufferMapped(&descriptor);
    ASSERT(result.dataLength == size);
    return result;
}

void ContextDawn::MapWriteCallback(WGPUBufferMapAsyncStatus status,
                                   void *data,
                                   uint64_t,
                                   void *userdata)
{
    ASSERT(status == WGPUBufferMapAsyncStatus_Success);
    ASSERT(data != nullptr);
    ContextDawn *contextDawn = static_cast<ContextDawn *>(userdata);

    int size                = contextDawn->mCurTotalInstance;
    contextDawn->mappedData = data;
    memcpy(contextDawn->mappedData, contextDawn->fishPers,
           sizeof(FishPer) * size);

    // Create a command buffer that copy from the staging buffer to dest buffer.
    wgpu::CommandBuffer command = contextDawn->copyBufferToBuffer(
        contextDawn->stagingBuffer, 0, contextDawn->fishPersBuffer, 0, sizeof(FishPer) * size);
    contextDawn->mCommandBuffers.emplace_back(command);
}

void ContextDawn::WaitABit()
{
    mDevice.Tick();

    utils::USleep(100);
}

void ContextDawn::updateAllFishData(
    const std::bitset<static_cast<size_t>(TOGGLE::TOGGLEMAX)> &toggleBitset)
{
    if (toggleBitset.test(static_cast<size_t>(TOGGLE::BUFFERMAPPINGASYNC)))
    {
        stagingBuffer.MapWriteAsync(MapWriteCallback, this);
    }
    else
    {
        setBufferData(fishPersBuffer, 0, sizeof(FishPer) * mCurTotalInstance, fishPers);
    }
}

void ContextDawn::destoryFishResource()
{
    fishPersBuffer = nullptr;
    stagingBuffer  = nullptr;

    if (fishPers != nullptr)
    {
        delete fishPers;
        fishPers = nullptr;
    }
    if (mEnableDynamicBufferOffset)
    {
        if (bindGroupFishPers != nullptr)
        {
            if (bindGroupFishPers[0].Get() != nullptr)
            {
                bindGroupFishPers[0] = nullptr;
            }
        }
    }
    else
    {
        if (bindGroupFishPers != nullptr)
        {
            for (int i = 0; i < mPreTotalInstance; i++)
            {
                if (bindGroupFishPers[i].Get() != nullptr)
                {
                    bindGroupFishPers[i] = nullptr;
                }
            }
        }
    }

    bindGroupFishPers = nullptr;
}