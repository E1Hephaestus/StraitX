#ifndef STRAITX_ENGINE_HPP
#define STRAITX_ENGINE_HPP

#include "platform/error.hpp"
#include "platform/glcontext.hpp"
#include "servers/display_server.hpp"
#include "graphics/vulkan/renderer_api.hpp"

namespace StraitX{

class Application;

class Engine{
private:
    Application *m_Application;
    bool m_Running;
    DisplayServer m_DisplayServer;
    Error m_ErrorRendererHW, m_ErrorRendererAPI;
    Error m_ErrorDisplayServer, m_ErrorApplication, m_ErrorMX;
public:
    Engine();
    ~Engine();

    int Run();

    void Stop();
    
private:
    Error Initialize();
    Error Finalize();
    void MainLoop();
};

}; // namespace StraitX::


#endif // STRAITX_ENGINE_HPP