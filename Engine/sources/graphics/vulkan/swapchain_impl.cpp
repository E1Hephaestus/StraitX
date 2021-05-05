#include <new>
#include "platform/memory.hpp"
#include "core/assert.hpp"
#include "core/log.hpp"
#include "graphics/vulkan/graphics_api_impl.hpp"
#include "graphics/vulkan/swapchain_impl.hpp"
#include "graphics/vulkan/gpu_texture_impl.hpp"
#include "graphics/vulkan/gpu_context_impl.hpp"
#include "graphics/vulkan/gpu.hpp"
#include "graphics/vulkan/dma_impl.hpp"

namespace StraitX{
namespace Vk{

constexpr VkColorSpaceKHR DesiredColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;


static bool IsSupported(VkPhysicalDevice dev, VkSurfaceKHR surface, VkFormat format, VkColorSpaceKHR color_space){
    u32 count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(dev, surface, &count, nullptr);

    auto *formats = (VkSurfaceFormatKHR*)alloca(count * sizeof(VkSurfaceFormatKHR));

    vkGetPhysicalDeviceSurfaceFormatsKHR(dev, surface, &count, formats);

    bool support = false;

    for(u32 i = 0; i<count; i++){
        if(formats[i].colorSpace == color_space && formats[i].format == format)
            support = true;
    }

    return support;
}

static AttachmentDescription SwapchainAttachments[] = {
    {
        GPUTexture::Layout::PresentSrcOptimal,
        GPUTexture::Layout::PresentSrcOptimal,
        GPUTexture::Layout::ColorAttachmentOptimal,
        TextureFormat::Unknown,
        SamplePoints::Samples_1,
    },
    {
        GPUTexture::Layout::DepthStencilAttachmentOptimal,
        GPUTexture::Layout::DepthStencilAttachmentOptimal,
        GPUTexture::Layout::DepthStencilAttachmentOptimal,
        TextureFormat::Unknown,
        SamplePoints::Samples_1,
    }
};

static RenderPassProperties ToFramebufferProperties(const SwapchainProperties &props){
    //XXX not thread-safe
    SwapchainAttachments[0].Format = TextureFormat::BGRA8;
    SwapchainAttachments[0].Samples = props.FramebufferSamples; 

    SwapchainAttachments[1].Format = props.DepthFormat;
    return {{SwapchainAttachments, size_t(1 + IsDepthFormat(props.DepthFormat))}};
}


SwapchainImpl::SwapchainImpl(const Window &window, const SwapchainProperties &props):
    Swapchain(props),
    m_Colorspace(DesiredColorSpace),
    m_ImagesCount(props.FramebuffersCount),
    m_FramebufferPass(ToFramebufferProperties(props)),
    m_TargetQueueFamily(QueueFamily::Graphics),
    m_TargetQueue(GPU::Get().Queue(m_TargetQueueFamily)),
    m_TargetQueueIndex(GPU::Get().QueueIndex(m_TargetQueueFamily))
{
    
    CoreFunctionAssert(m_Surface.Create(Vk::GraphicsAPIImpl::Instance.Handle(), window),Result::Success, "Vk: SwapchainImpl: Can't obtain surface");
    
    {
        VkBool32 supported;
        vkGetPhysicalDeviceSurfaceSupportKHR(GPU::Get().PhysicalHandle(), m_TargetQueueIndex, m_Surface.Handle, &supported);
        if(!supported){
            LogError("Vk: SwapchainImpl: Current Physical Device does not support swapchain");
            return;
        }
    }
    
    VkSurfaceCapabilitiesKHR capabilities;
    CoreFunctionAssert(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(GPU::Get().PhysicalHandle(), m_Surface.Handle, &capabilities), VK_SUCCESS, "Vk: SwapchainImpl: can't obtain surface sapabilites");

    if(capabilities.minImageCount > m_ImagesCount){
        LogWarn("Vk: Swapchain: System requires % framebuffers", capabilities.minImageCount);
        m_ImagesCount = capabilities.minImageCount;
    }

    CoreAssert((!capabilities.maxImageCount || m_ImagesCount <= capabilities.maxImageCount), "Vk: SwapchainImpl: current system does not support this amount of framebuffers");
    
    if(!IsSupported(GPU::Get().PhysicalHandle(), m_Surface.Handle, m_Format, m_Colorspace)){
        LogError("Vk: SwapchainImpl: ColorSpace and Format are not supported");
        return;
    }

    m_Size.x = capabilities.currentExtent.width;
    m_Size.y = capabilities.currentExtent.height;
    
    VkSwapchainCreateInfoKHR info;
    info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    info.pNext = nullptr;
    info.flags = 0;
    info.oldSwapchain = VK_NULL_HANDLE;
    info.queueFamilyIndexCount = 0;
    info.pQueueFamilyIndices = nullptr;
    info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    info.clipped = true;
    info.minImageCount = m_ImagesCount;
    info.preTransform = capabilities.currentTransform;
    info.surface = m_Surface.Handle;
    info.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR; // should be supported everywhere // TODO replace with VSync mode
    info.imageFormat = m_Format;
    info.imageColorSpace = m_Colorspace;
    info.imageExtent = capabilities.currentExtent;
    info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE; // XXX For now we support just a single queue
    info.imageArrayLayers = 1;
    info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    CoreFunctionAssert(vkCreateSwapchainKHR(GPU::Get().Handle(), &info, nullptr, &m_Handle), VK_SUCCESS, "Vk: SwapchainImpl: Can't create a swapchain");

    if(IsDepthFormat(props.DepthFormat)){
        m_DepthAttachment.New(props.DepthFormat, GPUTexture::DepthStencilOptimal | GPUTexture::TransferDst, m_Size.x, m_Size.y);

        DMAImpl::ChangeGPUTextureLayoutImpl(m_DepthAttachment, GPUTexture::Layout::DepthStencilAttachmentOptimal);
    }

    InitializeFramebuffers(m_Format);

    // First Acquire is synched
    vkAcquireNextImageKHR(GPU::Get().Handle(), m_Handle, 0, VK_NULL_HANDLE, m_AcquireFence.Handle, &m_CurrentImage);
    m_AcquireFence.WaitFor();
}

SwapchainImpl::~SwapchainImpl(){
    vkQueueWaitIdle(m_TargetQueue);

    FinalizeFramebuffers();
    
    vkDestroySwapchainKHR(GPU::Get().Handle(), m_Handle, nullptr);

    m_Surface.Destroy();
}

const Framebuffer *SwapchainImpl::CurrentFramebuffer()const{
    return &m_Framebuffers[m_CurrentImage];
}

const RenderPass *SwapchainImpl::FramebufferPass()const{
    return &m_FramebufferPass;
}

Swapchain *SwapchainImpl::NewImpl(const Window &window, const SwapchainProperties &props){
    return new (Memory::Alloc(sizeof(Vk::SwapchainImpl))) SwapchainImpl(window, props);
}

void SwapchainImpl::DeleteImpl(Swapchain *swapchain){
    swapchain->~Swapchain();
    Memory::Free(swapchain);
}

void SwapchainImpl::InitializeFramebuffers(VkFormat format){
    u32 images_count = 0;
    vkGetSwapchainImagesKHR(GPU::Get().Handle(), m_Handle, &images_count, nullptr);
    auto *images = (VkImage*)alloca(images_count * sizeof(VkImage));
    vkGetSwapchainImagesKHR(GPU::Get().Handle(), m_Handle, &images_count, images);

    CoreAssert(images_count <= MaxFramebuffers, "Vk: Swapchain: unsupported amount of Images");

    for(u32 i = 0; i<images_count; ++i){
        GPUTextureImpl(m_Images.Emplace()).CreateWithImage(images[i], GPUTexture::Layout::PresentSrcOptimal, TextureFormat::BGRA8, GPUTexture::UsageBits::Sampled, m_Size.x, m_Size.y);

        const GPUTexture *attachments[] = {
            &m_Images[i],
            &m_DepthAttachment
        };
        FramebufferProperties props;
        props.Size        = m_Size;
        props.Attachments = {attachments, size_t(1 + !m_DepthAttachment.IsEmpty())};

        m_Framebuffers.Emplace(&m_FramebufferPass, props);
    }
}

void SwapchainImpl::FinalizeFramebuffers(){
    m_Framebuffers.Clear();
    for(auto &image: m_Images){
        GPUTextureImpl(image).DestroyWithoutImage();
        GPUTextureImpl(image).Handle = 0;
    }
}

void SwapchainImpl::PresentCurrent(VkSemaphore wait_semaphore){
    VkResult result;

    VkPresentInfoKHR info;
    info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    info.pNext = nullptr;
    info.waitSemaphoreCount = 1;
    info.pWaitSemaphores = &wait_semaphore;
    info.swapchainCount = 1;
    info.pSwapchains = &m_Handle;
    info.pImageIndices = &m_CurrentImage;
    info.pResults = &result;

    vkQueuePresentKHR(m_TargetQueue, &info);
}

void SwapchainImpl::AcquireNext(VkSemaphore signal_semaphore){
    vkAcquireNextImageKHR(GPU::Get().Handle(), m_Handle, 0, signal_semaphore, VK_NULL_HANDLE, &m_CurrentImage);
}

}//namespace Vk::
}//namespace StraitX::
