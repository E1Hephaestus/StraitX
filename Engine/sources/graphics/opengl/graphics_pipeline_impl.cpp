#include <new>
#include <cstdio>
#include "platform/memory.hpp"
#include "core/log.hpp"
#include "core/string.hpp"
#include "core/pair.hpp"
#include "graphics/opengl/shader_impl.hpp"
#include "graphics/opengl/debug.hpp"
#include "graphics/opengl/graphics_pipeline_impl.hpp"
#include "graphics/opengl/graphics_context_impl.hpp"

namespace StraitX{
namespace GL{

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
    SX_CORE_ASSERT(false, "GL: GraphicsPipeline: Unknown Elements Count");
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
    SX_CORE_ASSERT(false, "GL: GraphicsPipeline: Unknown Element Type");
    return 0;
}

struct ShaderBindingInfo{
    ShaderBindingType Type;
    u32 BaseGLBinding;
    u32 ArraySize; 
};

Span<const char> FindFirstUniformBindingStatement(const char *sources){
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

Span<const char> FindUniformBindingStatement(u32 binding_index, const char *sources){
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

            //SX_CORE_ASSERT(binding, "GL: GraphicsPipeline: uniform should have an explicit binding");

            auto index = String::Ignore(binding + 7, ' ');
            //SX_CORE_ASSERT(*index == '=', "GL: GraphicsPipeline: binding doesn't have index assigned");
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

u32 ReadArraySize(Span<const char> statement){
    SX_CORE_ASSERT(*statement.Pointer() == '[',"ReadArraySize: statement should point on array's opening bracket");

    auto size = String::Ignore(statement.Pointer() + 1, ' ');

    u32 array_size = 0;
    std::sscanf(size, "%u", &array_size);

    SX_CORE_ASSERT(String::Find(statement.Pointer(), statement.Size(), "]"), "ReadArraySize: array does not have a closing bracket");

    return array_size;
}

u32 GetUniformBufferStatementArraySize(Span<const char> statement){
    //SX_CORE_ASSERT(String::Find(statement.Pointer(), statement.Size(), "{"),"GL: GraphicsPipeline: Shader Uniform Buffer block is incomplete");
    //SX_CORE_ASSERT(String::Find(statement.Pointer(), statement.Size(), "}"),"GL: GraphicsPipeline: Shader Uniform Buffer block is incomplete");

    auto buffer_end = String::Find(statement.Pointer(), statement.Size(), "}");

    auto bracket = String::Find(buffer_end, statement.Size() - (buffer_end - statement.Pointer()), "[");

    if(!bracket) return 1;

    return ReadArraySize({bracket, statement.Size() - (bracket - statement.Pointer())});
}

u32 GetUniformSamplerStatementArraySize(Span<const char> statement){
    auto name = String::Ignore(String::Find(statement.Pointer(),statement.Size(), "sampler2D") + 9, ' ');

    auto bracket = String::Find(name, statement.Size() - (name - statement.Pointer()), "[");

    if(!bracket) return 1;

    return ReadArraySize({bracket, statement.Size() - (bracket - statement.Pointer())});
}

ShaderBindingType GetUniformStatementType(Span<const char> statement){
    if(String::Contains(statement.Pointer(), statement.Size(), "sampler2D"))
        return ShaderBindingType::Sampler;
    return ShaderBindingType::UniformBuffer;
}

void ReplaceVersionTo(const char *&in_sources, char *&out_sources, s32 version_number){
    const char *version = String::Ignore(String::Find(in_sources, "#version") + 8, ' ');
    Memory::Copy(in_sources, out_sources, version - in_sources);
    out_sources += version - in_sources;
    in_sources = String::IgnoreUntil(version, ' ');
    out_sources = out_sources + BufferPrinter<s32>::Print(version_number, out_sources);
}

void Copy(const char *end, const char *&in_sources, char *&out_sources){
    size_t offset = end - in_sources;
    Memory::Copy(in_sources, out_sources, offset);
    in_sources += offset;
    out_sources += offset;
}

void TranslateStatementWithoutLayoutQualifiers(const Span<const char> &statement,const char *&in_sources, char *&out_sources){
    const char *uniform = String::Find(statement.Pointer(), statement.Size(), "uniform");
    in_sources = uniform;

    Copy(statement.Pointer() + statement.Size(), in_sources, out_sources);
}

void TranslateStatementWithoutBindingQualifier(const Span<const char> &statement,const char *&in_sources, char *&out_sources){
    const char *binding = String::Find(statement.Pointer(), statement.Size(), "binding");
    do{
        *out_sources++ = *in_sources++;
    }while(in_sources != binding);

    while(*out_sources!='(' && *out_sources != ',')--out_sources;

    if(*out_sources == '(')++out_sources;

    while(*in_sources != ',' && *in_sources != ')')++in_sources;

    if(*in_sources == ',')++in_sources;

    Copy(statement.Pointer() + statement.Size(), in_sources, out_sources);
}

u32 GetStatementBindingIndex(const Span<const char> &statement){
    const char *binding = String::Ignore(String::IgnoreUntil(String::Find(statement.Pointer(), statement.Size(), "binding") + 7, '=') + 1, ' ');
    u32 binding_index = 0;
    std::sscanf(binding, "%u",&binding_index);
    return binding_index;
}

void TranslateStatementToBindingIndex(const Span<const char> &statement,const char *&in_sources, char *&out_sources, u32 binding_index){

    const char *binding = String::Ignore(String::IgnoreUntil(String::Find(statement.Pointer(), statement.Size(), "binding") + 7, '=') + 1, ' ');

    Copy(binding, in_sources, out_sources);

    out_sources = out_sources + BufferPrinter<u32>::Print(binding_index, out_sources);

    while(*in_sources >= '0' && *in_sources <= '9')++in_sources;

    Copy(statement.Pointer() + statement.Size(), in_sources, out_sources);
}

GraphicsPipelineImpl::GraphicsPipelineImpl(const GraphicsPipelineProperties &props):
    GraphicsPipeline(props),
    AttributesStride(GraphicsPipeline::CalculateStride(props.VertexAttributes)),
    Topology(s_TopologyTable[(size_t)props.Topology]),
    Rasterization(s_RasterizationModeTable[(size_t)props.Rasterization]),
    FramebufferViewport(props.FramebufferViewport),
    BlendFunc(s_BlendFunctionTable[(size_t)props.BlendFunc]),
    SrcBlendFactor(s_BlendFactorTable[(size_t)props.SrcBlendFactor]),
    DstBlendFactor(s_BlendFactorTable[(size_t)props.DstBlendFactor])
{
    for(auto &shader: props.Shaders){
        if(!(Valid = shader->IsValid()))
            return;
    }

    glGenVertexArrays(1, &VertexArray);
    glBindVertexArray(VertexArray);

    LogInfo("GL: VertexAttributes: Count: %", props.VertexAttributes.Size());

    for(size_t i = 0; i<props.VertexAttributes.Size(); ++i){
        Attributes.Push(props.VertexAttributes[i]);

        glEnableVertexAttribArray(i);
        
        if(SupportsVertexAttribFormat()){
            glVertexAttribFormat(i, ElementsCount(props.VertexAttributes[i]),ElementType(props.VertexAttributes[i]),false, 0 /*offset from the begining of the buffer*/);
            glVertexAttribBinding(i,i);
        }else{
            LogWarn("OpenGL: fallback to a compatible OpenGL profile");
        }
    }

    u32 current_uniform = 0;
    u32 current_sampler = 0;

    for(u32 i = 0; i<lengthof(VirtualBindings); ++i){
        for(auto shader: props.Shaders){
            auto impl = static_cast<const GL::ShaderImpl*>(shader);

            Span<const char> statement = FindUniformBindingStatement(i, impl->Sources);

            if(statement.Pointer() && statement.Size()){
                ShaderBindingType type = GetUniformStatementType(statement);

                if(!VirtualBindings[i].ArraySize){
                    VirtualBindings[i].Type = type;
                    switch (type) {
                    case ShaderBindingType::UniformBuffer:
                        VirtualBindings[i].BaseGLBinding = current_uniform;
                        VirtualBindings[i].ArraySize = GetUniformBufferStatementArraySize(statement);
                        current_uniform += VirtualBindings[i].ArraySize;
                        break;
                    case ShaderBindingType::Sampler:
                        VirtualBindings[i].BaseGLBinding = current_sampler;
                        VirtualBindings[i].ArraySize = GetUniformSamplerStatementArraySize(statement);
                        current_sampler += VirtualBindings[i].ArraySize;
                        break;
                    }
                }
            }
        }
    }

    Program = glCreateProgram();

    Span<u32> shaders(SX_STACK_ARRAY_ALLOC(u32, props.Shaders.Size()),props.Shaders.Size());

    for(size_t i = 0; i<props.Shaders.Size(); ++i){
        auto impl = static_cast<const GL::ShaderImpl*>(props.Shaders[i]);
        shaders[i] = glCreateShader(ShaderImpl::GetStage(impl->GetType()));

        auto src_size = impl->Length + lengthof(VirtualBindings)*2;
        char *shader_sources_buffer = (char*)alloca(src_size);

        char *out_sources = shader_sources_buffer;
        const char *in_sources = impl->Sources;

        if(!SupportsUniformBindings())ReplaceVersionTo(in_sources, out_sources, 410);

        Span<const char> statement;

        while((statement = FindFirstUniformBindingStatement(in_sources)).Pointer()){
            Copy(statement.Pointer(), in_sources, out_sources);

            if(SupportsUniformBindings()){
                u32 binding_index = GetStatementBindingIndex(statement);

                TranslateStatementToBindingIndex(statement, in_sources, out_sources, VirtualBindings[binding_index].BaseGLBinding);
            }else{
                // XXX !!!Every uniform should have binding!!! then i can understand if there is any other layout qualifiers by comma search
                if(String::Find(statement.Pointer(), statement.Size(), ","))
                    TranslateStatementWithoutBindingQualifier(statement, in_sources, out_sources);
                else
                    TranslateStatementWithoutLayoutQualifiers(statement, in_sources, out_sources);
            }

        }

        Copy(impl->Sources + impl->Length, in_sources, out_sources);
        *out_sources = 0;

        //LogInfo("Sources: %",shader_sources_buffer);
        /*Compile Shader, validate and attach*/{
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

    glUseProgram(Program);
    
    for(auto shader: props.Shaders){
        auto impl = static_cast<const GL::ShaderImpl*>(shader);

        Span<const char> statement;
        const char *search = impl->Sources;
        while((statement = FindFirstUniformBindingStatement(search + statement.Size())).Pointer()){
            search = statement.Pointer();

            u32 binding_index = GetStatementBindingIndex(statement);

            ShaderBindingType type = GetUniformStatementType(statement);

            switch (type) {
            case ShaderBindingType::UniformBuffer:
            {
                const char *name = String::Ignore(String::Find(statement.Pointer(), "uniform") + 7, ' ');

                size_t name_length = 0;
                for(const char *i = name; *i != ' ' && *i != '{' && *i != ';'; ++i,++name_length);

                char *name_null = (char*)alloca(name_length + 1);
                Memory::Copy(name, name_null, name_length);
                name_null[name_length] = 0;

                u32 index = glGetUniformBlockIndex(Program, name_null);
                glUniformBlockBinding(Program, index, VirtualBindings[binding_index].BaseGLBinding);
            }break;
            case ShaderBindingType::Sampler:
            {
                const char *name = String::Ignore(String::Find(statement.Pointer(), "sampler2D") + 9, ' ');

                size_t name_length = 0;
                for(const char *i = name; *i != ' ' && *i != '[' && *i != ';'; ++i,++name_length);

                char *name_null = (char*)alloca(name_length + 1);
                Memory::Copy(name, name_null, name_length);
                name_null[name_length] = 0;

                s32 *units = (s32*)alloca(VirtualBindings[binding_index].ArraySize * sizeof(s32));
                for(u32 i = 0; i<VirtualBindings[binding_index].ArraySize; ++i){
                    units[i] = VirtualBindings[binding_index].BaseGLBinding + i;
                }
                glUniform1iv(glGetUniformLocation(Program, name_null), VirtualBindings[binding_index].ArraySize, units);
            }break;
            }
        }
    } 


    for(auto shader: shaders)
        glDeleteShader(shader);
}

GraphicsPipelineImpl::~GraphicsPipelineImpl(){
    glDeleteVertexArrays(1, &VertexArray);
    glDeleteProgram(Program);
}

bool GraphicsPipelineImpl::IsValid()const{
    return Valid;
}

void GraphicsPipelineImpl::Bind()const{
    glPolygonMode(GL_FRONT_AND_BACK, Rasterization);
    glViewport(FramebufferViewport.x, FramebufferViewport.y, FramebufferViewport.Width, FramebufferViewport.Height);
    glBlendFunc(SrcBlendFactor, DstBlendFactor);
    glBlendEquation(BlendFunc);
    glBindVertexArray(VertexArray);
    glUseProgram(Program);
}

void GraphicsPipelineImpl::BindVertexBuffer(u32 id)const{
    if(SupportsVertexAttribFormat()){
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

bool GraphicsPipelineImpl::SupportsUniformBindings()const{
    auto &context = GraphicsContextImpl::s_Instance;

    return context.LoadedVersion().Major >= 4 && context.LoadedVersion().Minor >= 2;
}

bool GraphicsPipelineImpl::SupportsVertexAttribFormat()const{
    auto &context = GraphicsContextImpl::s_Instance;

	return context.LoadedVersion().Major == 4 && context.LoadedVersion().Minor >= 3;
}

GraphicsPipeline * GraphicsPipelineImpl::NewImpl(const GraphicsPipelineProperties &props){
    return new(Memory::Alloc(sizeof(GL::GraphicsPipelineImpl))) GL::GraphicsPipelineImpl(props);
}

void GraphicsPipelineImpl::DeleteImpl(GraphicsPipeline *pipeline){
    pipeline->~GraphicsPipeline();
    Memory::Free(pipeline);
}


}//namespace GL::
}//namespace StraitX::