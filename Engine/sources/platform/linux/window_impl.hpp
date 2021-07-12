#ifndef STRAITX_LINUX_WINDOW_IMPL_HPP
#define STRAITX_LINUX_WINDOW_IMPL_HPP

#include "platform/events.hpp"
#include "platform/result.hpp"
#include "platform/linux/screen_impl.hpp"

struct _XIM;
struct _XIC;
struct __GLXFBConfigRec;

namespace Linux{

struct WindowImpl{
    unsigned long Handle = 0;
    struct __GLXFBConfigRec *FBConfig = nullptr;
	int Width = 0;
	int Height = 0;
	struct _XIM *InputMethod = nullptr;
	struct _XIC *InputContext = 0;

    WindowImpl() = default;

    Result Open(const ScreenImpl &screen, int width, int height);

    Result Close();

    bool IsOpen() const;

    void SetTitle(const char *title);

    void SetSize(int width, int height);

    Size2u Size()const;

	static Size2u GetSizeFromHandle(unsigned long handle);

    static __GLXFBConfigRec *PickBestFBConfig(int screen_index);
};

}//namespace Linux::

#endif // STRAITX_LINUX_WINDOW_IMPL_HPP