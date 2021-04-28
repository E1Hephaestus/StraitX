#ifndef STRAITX_GRAPHICS_API_VTABLE_HPP
#define STRAITX_GRAPHICS_API_VTABLE_HPP

#include "graphics/api/graphics_api.hpp"
#include "graphics/api/gpu_context.hpp"
#include "graphics/api/swapchain.hpp"
#include "graphics/api/gpu_buffer.hpp"
#include "graphics/api/cpu_buffer.hpp"
#include "graphics/api/gpu_texture.hpp"
#include "graphics/api/sampler.hpp"
#include "graphics/api/shader.hpp"
#include "graphics/api/render_pass.hpp"
#include "graphics/api/graphics_pipeline.hpp"
#include "graphics/api/framebuffer.hpp"
#include "graphics/api/dma.hpp"

namespace StraitX{

struct GraphicsAPIVtable{
    GraphicsAPI *GraphicsAPIPtr = {};

    GPUContext::VTable GPUContextVTable = {};

    Swapchain::VTable SwapchainVTable = {};

    GPUBuffer::VTable GPUBufferVTable = {};

    CPUBuffer::VTable CPUBufferVTable = {};

    GPUTexture::VTable GPUTextureVTable = {};

    Sampler::VTable SamplerVTable = {};

    Shader::VTable ShaderVTable = {};

    RenderPass::VTable RenderPassVTable = {};

    GraphicsPipeline::VTable GraphicsPipelineVTable = {};

    Framebuffer::VTable FramebufferVTable = {};

    DMA::VTable DMAVTable = {};
};

}//namespace StraitX::

#endif//STRAITX_GRAPHICS_API_VTABLE_HPP