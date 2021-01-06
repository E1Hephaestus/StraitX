#ifndef STRAITX_VULKAN_INSTANCE_HPP
#define STRAITX_VULKAN_INSTANCE_HPP

#include "platform/vulkan.hpp"
#include "platform/result.hpp"
#include "platform/types.hpp"
#include "core/array_ptr.hpp"

namespace StraitX{
namespace Vk{

struct Instance{
    VkInstance Handle = VK_NULL_HANDLE;

    Result Create(const Version &version, const ArrayPtr<const char *> &extensions, const ArrayPtr<const char *> &layers);

    void Destroy();

    static bool CheckLayers(const ArrayPtr<const char *> &layers);

    static bool CheckExtensions(const ArrayPtr<const char *> &extensions);
};

};//namespace Vk::
};//namespace StraitX::
#endif // STRAITX_VULKAN_INSTANCE_HPP