#include <new>
#include <cstdio>
#include "platform/memory.hpp"
#include "core/log.hpp"
#include "core/string.hpp"
#include "core/pair.hpp"
#include "graphics/opengl/shader_impl.hpp"
#include "graphics/opengl/debug.hpp"
#include "graphics/opengl/graphics_pipeline_impl.hpp"
#include "graphics/opengl/graphics_api_impl.hpp"

namespace StraitX{
namespace GL{


void DescriptorSet::Bind() const{
    for(auto i = 0; i<lengthof(UniformBufferBindings); ++i){
        if(UniformBufferBindings[i].UniformBuffer)
            glBindBufferBase(GL_UNIFORM_BUFFER, i, UniformBufferBindings[i].UniformBuffer);
    }
    for(auto i = 0; i<lengthof(SamplerBindings); ++i){
        if(SamplerBindings[i].Texture && SamplerBindings[i].Sampler){
            glActiveTexture(GL_TEXTURE0 + i);
            glBindTexture(GL_TEXTURE_2D, SamplerBindings[i].Texture);
            glBindSampler(i, SamplerBindings[i].Sampler);
        }
    }
}

GLenum GraphicsPipelineImpl::s_BlendFunctionTable[]={
    GL_FUNC_ADD
};
GLenum GraphicsPipelineImpl::s_BlendFactorTable[]={
    GL_ZERO,
    GL_ONE,
    GL_ONE_MINUS_SRC_ALPHA,
    GL_ONE_MINUS_DST_ALPHA,
    GL_SRC_ALPHA,
    GL_DST_ALPHA
};

GLenum GraphicsPipelineImpl::s_TopologyTable[] = {
    GL_POINTS,
    GL_LINES,
    GL_LINE_STRIP,
    GL_TRIANGLES,
    GL_TRIANGLE_STRIP
};
GLenum GraphicsPipelineImpl::s_RasterizationModeTable[] = {
    GL_FILL,
    GL_POINT,
    GL_LINE
};

static u32 ElementsCount(VertexAttribute attribute){
    switch (attribute){
    case VertexAttribute::Int:
    case VertexAttribute::Uint:
    case VertexAttribute::Float:
        return 1;
    case VertexAttribute::Int2:
    case VertexAttribute::Uint2:
    case VertexAttribute::Float2:
        return 2;
    case VertexAttribute::Int3:
    case VertexAttribute::Uint3:
    case VertexAttribute::Float3:
        return 3;
    case VertexAttribute::Int4:
    case VertexAttribute::Uint4:
    case VertexAttribute::Float4:
        return 4;
    }
    CoreAssert(false, "GL: GraphicsPipeline: Unknown Elements Count");
    return 0;
}

static GLenum ElementType(VertexAttribute attribute){
    switch (attribute){
    case VertexAttribute::Int:
    case VertexAttribute::Int2:
    case VertexAttribute::Int3:
    case VertexAttribute::Int4:
        return GL_INT;
    case VertexAttribute::Uint:
    case VertexAttribute::Uint2:
    case VertexAttribute::Uint3:
    case VertexAttribute::Uint4:
        return GL_UNSIGNED_INT;
    case VertexAttribute::Float:
    case VertexAttribute::Float2:
    case VertexAttribute::Float3:
    case VertexAttribute::Float4:
        return GL_FLOAT;
    }
    CoreAssert(false, "GL: GraphicsPipeline: Unknown Element Type");
    return 0;
}

struct ShaderBindingInfo{
    ShaderBindingType Type;
    u32 BaseGLBinding;
    u32 ArraySize; 
};

ArrayPtr<const char> FindFirstUniformBindingStatement(const char *sources){
    while((sources = String::Find(sources, "layout"))){
        auto test_brack_open = String::Find(sources, "{");
        auto test_brack_close = String::Find(sources, "}");
        auto semicolon = sources;

        do{
            semicolon = String::Find(semicolon+1, ";");
        }while(semicolon > test_brack_open && semicolon < test_brack_close);

        size_t statement_size = semicolon - sources; // +1 ?

        if(String::Find(sources, statement_size, "uniform"))
            return {sources, statement_size};
        sources += 6;
    }
    return {nullptr, 0};
}

ArrayPtr<const char> FindUniformBindingStatement(u32 binding_index, const char *sources){
    while((sources = String::Find(sources, "layout"))){
        auto test_brack_open = String::Find(sources, "{");
        auto test_brack_close = String::Find(sources, "}");
        auto semicolon = sources;

        do{
            semicolon = String::Find(semicolon+1, ";");
        }while(semicolon > test_brack_open && semicolon < test_brack_close);

        size_t statement_size = semicolon - sources; // +1 ?

        auto uniform = String::Find(sources, statement_size, "uniform");

        if(uniform){
            auto binding = String::Find(sources, statement_size, "binding");

            //CoreAssert(binding, "GL: GraphicsPipeline: uniform should have an explicit binding");

            auto index = String::Ignore(binding + 7, ' ');
            //CoreAssert(*index == '=', "GL: GraphicsPipeline: binding doesn't have index assigned");
            index = String::Ignore(index + 1, ' ');

            u32 found_index = -1;
            std::sscanf(index, "%u", &found_index);

            if(found_index == binding_index)
                return {sources, statement_size};
        }
        sources += 6;
    }
    return {nullptr, 0};
}

u32 ReadArraySize(ArrayPtr<const char> statement){
    CoreAssert(*statement.Pointer() == '[',"ReadArraySize: statement should point on array's opening bracket");

    auto size = String::Ignore(statement.Pointer() + 1, ' ');

    u32 array_size = 0;
    std::sscanf(size, "%u", &array_size);

    CoreAssert(String::Find(statement.Pointer(), statement.Size(), "]"), "ReadArraySize: array does not have a closing bracket");

    return array_size;
}

u32 GetUniformBufferStatementArraySize(ArrayPtr<const char> statement){
    //CoreAssert(String::Find(statement.Pointer(), statement.Size(), "{"),"GL: GraphicsPipeline: Shader Uniform Buffer block is incomplete");
    //CoreAssert(String::Find(statement.Pointer(), statement.Size(), "}"),"GL: GraphicsPipeline: Shader Uniform Buffer block is incomplete");

    auto buffer_end = String::Find(statement.Pointer(), statement.Size(), "}");

    auto bracket = String::Find(buffer_end, statement.Size() - (buffer_end - statement.Pointer()), "[");

    if(!bracket) return 1;

    return ReadArraySize({bracket, statement.Size() - (bracket - statement.Pointer())});
}

u32 GetUniformSamplerStatementArraySize(ArrayPtr<const char> statement){
    auto name = String::Ignore(String::Find(statement.Pointer(),statement.Size(), "sampler2D") + 9, ' ');

    auto bracket = String::Find(name, statement.Size() - (name - statement.Pointer()), "[");

    if(!bracket) return 1;

    return ReadArraySize({bracket, statement.Size() - (bracket - statement.Pointer())});
}

ShaderBindingType GetUniformStatementType(ArrayPtr<const char> statement){
    if(String::Contains(statement.Pointer(), statement.Size(), "sampler2D"))
        return ShaderBindingType::Sampler;
    return ShaderBindingType::UniformBuffer;
}


GraphicsPipelineImpl::GraphicsPipelineImpl(LogicalGPU &owner, const GraphicsPipelineProperties &props):
    GraphicsPipeline(props),
    AttributesStride(GraphicsPipeline::CalculateStride(props.VertexAttributes)),
    Topology(s_TopologyTable[(size_t)props.Topology]),
    Rasterization(s_RasterizationModeTable[(size_t)props.Rasterization]),
    FramebufferViewport(props.FramebufferViewport),
    BlendFunc(s_BlendFunctionTable[(size_t)props.BlendFunc]),
    SrcBlendFactor(s_BlendFactorTable[(size_t)props.SrcBlendFactor]),
    DstBlendFactor(s_BlendFactorTable[(size_t)props.DstBlendFactor])
{
    //OpenGL...
    (void)owner;

    glGenVertexArrays(1, &VertexArray);
    glBindVertexArray(VertexArray);

    LogInfo("GL: VertexAttributes: Count: %", props.VertexAttributes.Size());

    for(size_t i = 0; i<props.VertexAttributes.Size(); ++i){
        Attributes.Push(props.VertexAttributes[i]);

        glEnableVertexAttribArray(i);
        
        if(GraphicsAPIImpl::Instance.LoadedOpenGLVersion.Major == 4 && GraphicsAPIImpl::Instance.LoadedOpenGLVersion.Minor >= 3){
            glVertexAttribFormat(i, ElementsCount(props.VertexAttributes[i]),ElementType(props.VertexAttributes[i]),false, 0 /*offset from the begining of the buffer*/);
            glVertexAttribBinding(i,i);
        }else{
            LogWarn("OpenGL: fallback to a compatible OpenGL profile")
        }
    }

    size_t current_uniform = 0;
    size_t current_sampler = 1;

    for(u32 i = 0; i<lengthof(Set.VirtualBindings); ++i){
        for(auto shader: props.Shaders){
            auto impl = static_cast<const GL::ShaderImpl*>(shader);

            auto statement = FindUniformBindingStatement(i, impl->Sources);

            if(statement.Pointer() && statement.Size()){
                auto type = GetUniformStatementType(statement);

                if(!Set.VirtualBindings[i].ArraySize){
                    Set.VirtualBindings[i].Type = type;
                    switch (type) {
                    case ShaderBindingType::UniformBuffer:
                        Set.VirtualBindings[i].BaseGLBinding = current_uniform;
                        Set.VirtualBindings[i].ArraySize = GetUniformBufferStatementArraySize(statement);
                        current_uniform += Set.VirtualBindings[i].ArraySize;
                        break;
                    case ShaderBindingType::Sampler:
                        Set.VirtualBindings[i].BaseGLBinding = current_sampler;
                        Set.VirtualBindings[i].ArraySize = GetUniformSamplerStatementArraySize(statement);
                        current_sampler += Set.VirtualBindings[i].ArraySize;
                        break;
                    }
                }
            }
        }
    }

    Program = glCreateProgram();

    ArrayPtr<u32> shaders((u32*)alloca(sizeof(u32)*props.Shaders.Size()),props.Shaders.Size());

    for(size_t i = 0; i<props.Shaders.Size(); ++i){
        auto impl = static_cast<const GL::ShaderImpl*>(props.Shaders[i]);
        shaders[i] = glCreateShader(ShaderImpl::GetStage(impl->GetType()));

        auto src_size = impl->Length + lengthof(Set.VirtualBindings)*2;
        char *shader_sources_buffer = (char*)alloca(src_size);

        char *out_sources = shader_sources_buffer;
        const char *in_sources = impl->Sources;

        ArrayPtr<const char> statement;

        while((statement = FindFirstUniformBindingStatement(in_sources)).Pointer()){
            {
                size_t offset = statement.Pointer() - in_sources;
                Memory::Copy(in_sources, out_sources, offset);
                in_sources += offset;
                out_sources += offset;
            }

            auto binding_index = String::IgnoreUntil(String::Find(statement.Pointer(), statement.Size(), "binding") + 7, '=');

            while(*binding_index < '0' || *binding_index > '9')++binding_index; 

            {
                size_t offset = binding_index - in_sources;
                Memory::Copy(in_sources, out_sources, offset);
                in_sources += offset;
                out_sources += offset;
            }
            u32 index = -1;
            std::sscanf(in_sources, "%u", &index);

            while(*in_sources >= '0' && *in_sources <= '9')++in_sources;


            auto type = GetUniformStatementType(statement);

            out_sources = BufferPrint(out_sources, Set.VirtualBindings[index].BaseGLBinding);

            {
                size_t offset = statement.Size() - (in_sources - statement.Pointer());
                Memory::Copy(in_sources, out_sources, offset);
                in_sources += offset;
                out_sources += offset;
            }

        }

        {
            size_t offset = impl->Length - (in_sources - impl->Sources);
            Memory::Copy(in_sources, out_sources, offset);
            out_sources += offset;
        }

        *out_sources = 0;

        LogInfo("Sources: %", shader_sources_buffer);
        {
            int length = out_sources - shader_sources_buffer;
            glShaderSource(shaders[i], 1, &shader_sources_buffer, &length);
            glCompileShader(shaders[i]);

            int status;
            glGetShaderiv(shaders[i], GL_COMPILE_STATUS, &status);
            if(status != GL_TRUE){
                int log_length;
                glGetShaderiv(shaders[i], GL_INFO_LOG_LENGTH, &log_length);

                char *log = (char*)alloca(log_length + 1);
                glGetShaderInfoLog(shaders[i],log_length, &log_length, log);
                log[log_length] = 0;
                LogError("OpenGL: Shader compilation Failure: %", log);

                Valid = false;
            }
            glAttachShader(Program, shaders[i]);
        }
    }

    {
        glLinkProgram(Program);
        int status;
        glGetProgramiv(Program, GL_LINK_STATUS, &status);

        if(status != GL_TRUE){
            int log_length;
            glGetProgramiv(Program, GL_INFO_LOG_LENGTH, &log_length);

            char *log = (char*)alloca(log_length + 1);
            glGetProgramInfoLog(Program,log_length, &log_length, log);
            log[log_length] = 0;
            LogError("OpenGL: Shader linkage Failure: %", log);

            Valid = false;
        }
    }

    for(auto shader: shaders)
        glDeleteShader(shader);
}

GraphicsPipelineImpl::~GraphicsPipelineImpl(){
    glDeleteVertexArrays(1, &VertexArray);
    glDeleteProgram(Program);
}

bool GraphicsPipelineImpl::IsValid(){
    return Valid;
}

void GraphicsPipelineImpl::Bind(size_t binding, size_t index, const GPUBuffer &uniform_buffer){
    Set.UniformBufferBindings[Set.VirtualBindings[binding].BaseGLBinding + index].UniformBuffer = uniform_buffer.Handle().U32;

    glBindBufferBase(GL_UNIFORM_BUFFER, Set.VirtualBindings[binding].BaseGLBinding + index, uniform_buffer.Handle().U32);
}

void GraphicsPipelineImpl::Bind(size_t binding, size_t index, const GPUTexture &texture, const Sampler &sampler){
    Set.SamplerBindings[Set.VirtualBindings[binding].BaseGLBinding + index].Texture = texture.Handle().U32;
    Set.SamplerBindings[Set.VirtualBindings[binding].BaseGLBinding + index].Sampler = sampler.Handle().U32;

    glActiveTexture(Set.VirtualBindings[binding].BaseGLBinding + index);
    glBindTexture(GL_TEXTURE_2D, texture.Handle().U32);
    glBindSampler(Set.VirtualBindings[binding].BaseGLBinding + index, sampler.Handle().U32);
}

void GraphicsPipelineImpl::Bind()const{
    glPolygonMode(GL_FRONT_AND_BACK, Rasterization);
    glViewport(FramebufferViewport.x, FramebufferViewport.y, FramebufferViewport.Width, FramebufferViewport.Height);
    glBlendFunc(SrcBlendFactor, DstBlendFactor);
    glBlendEquation(BlendFunc);
    glBindVertexArray(VertexArray);
    glUseProgram(Program);

    Set.Bind();
}

void GraphicsPipelineImpl::BindVertexBuffer(u32 id)const{
    if(GraphicsAPIImpl::Instance.LoadedOpenGLVersion.Major == 4 && GraphicsAPIImpl::Instance.LoadedOpenGLVersion.Minor >= 3){
        size_t offset = 0;
        for(size_t i = 0; i<Attributes.Size(); ++i){
            glBindVertexBuffer(i, id, offset, AttributesStride);
            offset+=GraphicsPipeline::s_VertexAttributeSizeTable[(size_t)Attributes[i]];
        }
    }else{
        glBindBuffer(GL_ARRAY_BUFFER, id);
        size_t offset = 0;
        for(size_t i = 0; i<Attributes.Size(); ++i){
            glVertexAttribPointer(i, ElementsCount(Attributes[i]),ElementType(Attributes[i]), false, AttributesStride, (void*)offset);
            offset+=GraphicsPipeline::s_VertexAttributeSizeTable[(size_t)Attributes[i]];
        }
    }
}

void GraphicsPipelineImpl::BindIndexBuffer(u32 id)const{
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, id);
}

GraphicsPipeline * GraphicsPipelineImpl::NewImpl(LogicalGPU &owner, const GraphicsPipelineProperties &props){
    return new(Memory::Alloc(sizeof(GL::GraphicsPipelineImpl))) GL::GraphicsPipelineImpl(owner, props);
}

void GraphicsPipelineImpl::DeleteImpl(GraphicsPipeline *pipeline){
    pipeline->~GraphicsPipeline();
    Memory::Free(pipeline);
}


}//namespace GL::
}//namespace StraitX::