#ifndef STRAITX_FBCONFIG_HPP
#define STRAITX_FBCONFIG_HPP

#include "platform/types.hpp"
#include "platform/error.hpp"
#include "platform/platform_detection.hpp"
#include "platform/display.hpp"
#include "platform/screen.hpp"

#ifdef SX_PLATFORM_LINUX
    #include "platform/linux/fbconfig_x11.hpp"
    typedef StraitX::Linux::FBConfigX11 FBConfigImpl;
#else
    #error "Your platfrom does not support FrameBufferConfig yet"
#endif

namespace StraitX{

class FBConfig{
private:
    FBConfigImpl m_Impl;
public:
    FBConfig(Display &display);

    Error PickDefault(const Screen &screen);

    Error PickDesired(const PixelFormat &desired, const Screen &screen);

    FBConfigImpl &Impl();

    const FBConfigImpl &Impl()const;

    const PixelFormat &Pixel()const;
};

}; // namespace StraitX::

#endif // STRAITX_FBCONFIG_HPP