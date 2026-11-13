// Native X11/Vulkan 1.0 platform. No Win32 calls or original executable code.
#include "wpc_platform.h"
#include "vulkan_shaders.h"
#include <X11/Xlib.h>
#include <X11/XKBlib.h>
#include <X11/keysym.h>
#include <X11/Xutil.h>
#include "video/h264/h264_std_codec.h"
#include "video/h265/h265_std_codec.h"
#include "video/vp9/vp9_std_codec.h"
#include "video/av1/av1_std_codec.h"
#include "vulkan/vulkan_core.h"
#include "vulkan/vulkan_xlib.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

using Vertex = WpcTexturedVertex3D;
struct WpcTexture {
    VkImage image{};
    VkDeviceMemory memory{};
    VkImageView view{};
    // Index: the clamp mask (bits 0-1), + 4 for the bilinear 3D samplers.
    std::array<VkDescriptorSet,8> descriptors{};
    uint32_t width{}, height{};
};
struct WpcWindow {
    Display *display{};
    Window xwindow{};
    Atom wm_delete{};
    int width{}, height{};
    bool close{}, frame{}, resize{};
    std::array<bool, WPC_KEY_COUNT> keys{};
    std::string typed; // KeyPress text since the last wpc_input_text
    std::array<bool, 256> codes{}; // Linux input codes (X keycode - 8) held
    int last_code = 0;             // the last pressed code, until read
    VkInstance instance{};
    VkSurfaceKHR surface{};
    VkPhysicalDevice physical{};
    VkDevice device{};
    uint32_t family{}, image_index{};
    VkQueue queue{};
    VkSwapchainKHR swapchain{};
    VkFormat format{};
    VkExtent2D extent{};
    VkRenderPass render_pass{};
    VkDescriptorSetLayout descriptor_layout{};
    VkDescriptorPool descriptor_pool{};
    std::array<VkSampler,8> samplers{};
    VkPipelineLayout pipeline_layout{};
    VkPipeline sprites{}, lines{};
    // Material triangles by culling (bit 0), WPC_DRAW_ADDITIVE (bit 1),
    // WPC_DRAW_NO_DEPTH_WRITE (bit 2) and WPC_DRAW_INVERSE (bit 3).
    std::array<VkPipeline,16> materials{};
    uint32_t draw_mode = 0;
    int brightness = 50; // MenuPrefs +0x20 (LUMINOSITE), 0x6f4588
    VkFormat depth_format{};
    VkImage depth_image{};
    VkImageView depth_view{};
    VkDeviceMemory depth_memory{};
    VkCommandPool command_pool{};
    VkCommandBuffer command{};
    VkFence fence{};
    VkSemaphore acquired{};
    std::vector<VkImageView> views;
    std::vector<VkFramebuffer> framebuffers;
    // A present semaphore is tied to each swapchain image, so it cannot be
    // signalled again until the presentation engine has consumed its last use.
    std::vector<VkSemaphore> rendered;
    std::vector<WpcTexture *> textures;
    WpcTexture *white{};
    VkBuffer vertices{};
    VkDeviceMemory vertex_memory{};
    void *mapped{};
    size_t vertex_offset{};
    size_t vertex_capacity=4*1024*1024;
    float mvp[16]{};
    float fog[8]{}; // RGBA color, then start/end view depth
};

static void check(VkResult result, const char *operation) {
    if (result != VK_SUCCESS)
        throw std::runtime_error(std::string(operation) + " failed (VkResult " + std::to_string(result) + ")");
}
void wpc_log(const char *format, ...) {
    va_list args; va_start(args, format); vfprintf(stdout, format, args); va_end(args);
    fputc('\n', stdout); fflush(stdout);
}
void wpc_log_error(const char *format, ...) {
    va_list args; va_start(args, format); vfprintf(stderr, format, args); va_end(args);
    fputc('\n', stderr); fflush(stderr);
}
static uint32_t memory_type(WpcWindow *w, uint32_t bits, VkMemoryPropertyFlags flags) {
    VkPhysicalDeviceMemoryProperties properties;
    vkGetPhysicalDeviceMemoryProperties(w->physical, &properties);
    for (uint32_t i = 0; i < properties.memoryTypeCount; ++i)
        if ((bits & (1u << i)) && (properties.memoryTypes[i].propertyFlags & flags) == flags) return i;
    throw std::runtime_error("No suitable Vulkan memory type");
}
static void buffer(WpcWindow *w, VkDeviceSize bytes, VkBufferUsageFlags usage,
                   VkBuffer &out, VkDeviceMemory &memory) {
    VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    info.size = bytes; info.usage = usage; info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    check(vkCreateBuffer(w->device, &info, nullptr, &out), "vkCreateBuffer");
    VkMemoryRequirements requirements;
    vkGetBufferMemoryRequirements(w->device, out, &requirements);
    VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocation.allocationSize = requirements.size;
    allocation.memoryTypeIndex = memory_type(w, requirements.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    check(vkAllocateMemory(w->device, &allocation, nullptr, &memory), "vkAllocateMemory(buffer)");
    check(vkBindBufferMemory(w->device, out, memory, 0), "vkBindBufferMemory");
}
static VkImageView image_view(WpcWindow *w, VkImage image, VkFormat format, VkImageAspectFlags aspect=VK_IMAGE_ASPECT_COLOR_BIT) {
    VkImageViewCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    info.image = image; info.viewType = VK_IMAGE_VIEW_TYPE_2D; info.format = format;
    info.subresourceRange = {aspect, 0, 1, 0, 1};
    VkImageView view{};
    check(vkCreateImageView(w->device, &info, nullptr, &view), "vkCreateImageView");
    return view;
}
static VkPipeline pipeline(WpcWindow *w, VkPrimitiveTopology topology, bool depth_test=false, bool cull_back=false,
                           bool additive=false, bool depth_write=true, bool inverse=false) {
    VkShaderModule vert{}, frag{};
    try {
        VkShaderModuleCreateInfo module{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        module.codeSize = sizeof(wpc_sprite_vert); module.pCode = wpc_sprite_vert;
        check(vkCreateShaderModule(w->device, &module, nullptr, &vert), "vertex shader");
        module.codeSize = sizeof(wpc_sprite_frag); module.pCode = wpc_sprite_frag;
        check(vkCreateShaderModule(w->device, &module, nullptr, &frag), "fragment shader");
        VkPipelineShaderStageCreateInfo stages[2]{};
        stages[0].sType = stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT; stages[0].module = vert; stages[0].pName = "main";
        stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT; stages[1].module = frag; stages[1].pName = "main";
        VkVertexInputBindingDescription binding{0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX};
        VkVertexInputAttributeDescription attributes[] = {
            {0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0},
            {1, 0, VK_FORMAT_R32G32_SFLOAT, 3 * sizeof(float)},
            {2, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 5 * sizeof(float)}};
        VkPipelineVertexInputStateCreateInfo input{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
        input.vertexBindingDescriptionCount = 1; input.pVertexBindingDescriptions = &binding;
        input.vertexAttributeDescriptionCount = 3; input.pVertexAttributeDescriptions = attributes;
        VkPipelineInputAssemblyStateCreateInfo assembly{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
        assembly.topology = topology;
        VkPipelineViewportStateCreateInfo viewport{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
        viewport.viewportCount = 1; viewport.scissorCount = 1;
        VkPipelineRasterizationStateCreateInfo raster{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
        raster.polygonMode = VK_POLYGON_MODE_FILL;
        raster.cullMode = cull_back ? VK_CULL_MODE_BACK_BIT : VK_CULL_MODE_NONE;
        raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        raster.lineWidth = 1;
        VkPipelineMultisampleStateCreateInfo samples{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
        samples.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        VkPipelineDepthStencilStateCreateInfo depth{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
        depth.depthTestEnable = depth_test; depth.depthWriteEnable = depth_test && depth_write;
        depth.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
        VkPipelineColorBlendAttachmentState attachment{};
        attachment.colorWriteMask = 15; attachment.blendEnable = VK_TRUE;
        // WPC's SRCALPHA/INVSRCALPHA default; draw flag 0x8000 sets ONE/ONE (0x4b3630).
        // 0x4b5550's mode bit 0x2 sets INVSRCALPHA/INVSRCCOLOR for the draw.
        attachment.srcColorBlendFactor = additive ? VK_BLEND_FACTOR_ONE : inverse ? VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA : VK_BLEND_FACTOR_SRC_ALPHA;
        attachment.dstColorBlendFactor = additive ? VK_BLEND_FACTOR_ONE : inverse ? VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR : VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        attachment.colorBlendOp = attachment.alphaBlendOp = VK_BLEND_OP_ADD;
        attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        VkPipelineColorBlendStateCreateInfo blend{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
        blend.attachmentCount = 1; blend.pAttachments = &attachment;
        VkDynamicState dynamic_states[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamic{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
        dynamic.dynamicStateCount = 2; dynamic.pDynamicStates = dynamic_states;
        VkGraphicsPipelineCreateInfo info{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
        info.stageCount = 2; info.pStages = stages; info.pVertexInputState = &input;
        info.pInputAssemblyState = &assembly; info.pViewportState = &viewport;
        info.pRasterizationState = &raster; info.pMultisampleState = &samples;
        info.pColorBlendState = &blend; info.pDynamicState = &dynamic;
        info.pDepthStencilState = &depth;
        info.layout = w->pipeline_layout; info.renderPass = w->render_pass;
        VkPipeline result{};
        check(vkCreateGraphicsPipelines(w->device, VK_NULL_HANDLE, 1, &info, nullptr, &result), "graphics pipeline");
        vkDestroyShaderModule(w->device, frag, nullptr); vkDestroyShaderModule(w->device, vert, nullptr);
        return result;
    } catch (...) {
        if (frag) vkDestroyShaderModule(w->device, frag, nullptr);
        if (vert) vkDestroyShaderModule(w->device, vert, nullptr);
        throw;
    }
}
static void destroy_swapchain(WpcWindow *w) {
    for (auto fb : w->framebuffers) vkDestroyFramebuffer(w->device, fb, nullptr);
    for (auto view : w->views) vkDestroyImageView(w->device, view, nullptr);
    for (auto semaphore : w->rendered) vkDestroySemaphore(w->device, semaphore, nullptr);
    w->framebuffers.clear(); w->views.clear(); w->rendered.clear();
    if (w->sprites) vkDestroyPipeline(w->device, w->sprites, nullptr);
    if (w->lines) vkDestroyPipeline(w->device, w->lines, nullptr);
    for (auto &p : w->materials) { if (p) vkDestroyPipeline(w->device, p, nullptr); p = VK_NULL_HANDLE; }
    if (w->render_pass) vkDestroyRenderPass(w->device, w->render_pass, nullptr);
    if (w->swapchain) vkDestroySwapchainKHR(w->device, w->swapchain, nullptr);
    if (w->depth_view) vkDestroyImageView(w->device, w->depth_view, nullptr);
    if (w->depth_image) vkDestroyImage(w->device, w->depth_image, nullptr);
    if (w->depth_memory) vkFreeMemory(w->device, w->depth_memory, nullptr);
    w->depth_view = VK_NULL_HANDLE; w->depth_image = VK_NULL_HANDLE; w->depth_memory = VK_NULL_HANDLE;
    w->sprites = w->lines = VK_NULL_HANDLE;
    w->render_pass = VK_NULL_HANDLE; w->swapchain = VK_NULL_HANDLE;
}
static void swapchain(WpcWindow *w) {
    check(vkDeviceWaitIdle(w->device), "wait before swapchain recreation");
    destroy_swapchain(w);
    VkSurfaceCapabilitiesKHR caps;
    check(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(w->physical, w->surface, &caps), "surface capabilities");
    uint32_t count{};
    check(vkGetPhysicalDeviceSurfaceFormatsKHR(w->physical, w->surface, &count, nullptr), "surface formats");
    if (!count) throw std::runtime_error("No Vulkan surface formats");
    std::vector<VkSurfaceFormatKHR> formats(count);
    check(vkGetPhysicalDeviceSurfaceFormatsKHR(w->physical, w->surface, &count, formats.data()), "surface formats");
    auto format = formats.front();
    for (auto candidate : formats)
        if (candidate.format == VK_FORMAT_B8G8R8A8_UNORM && candidate.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            format = candidate;
    if (format.format == VK_FORMAT_UNDEFINED) format.format = VK_FORMAT_B8G8R8A8_UNORM;
    w->format = format.format;
    w->extent = caps.currentExtent;
    if (w->extent.width == UINT32_MAX) {
        w->extent.width = std::clamp(uint32_t(w->width), caps.minImageExtent.width, caps.maxImageExtent.width);
        w->extent.height = std::clamp(uint32_t(w->height), caps.minImageExtent.height, caps.maxImageExtent.height);
    }
    uint32_t images = caps.minImageCount + 1;
    if (caps.maxImageCount) images = std::min(images, caps.maxImageCount);
    VkSwapchainCreateInfoKHR info{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    info.surface = w->surface; info.minImageCount = images;
    info.imageFormat = format.format; info.imageColorSpace = format.colorSpace;
    info.imageExtent = w->extent; info.imageArrayLayers = 1;
    info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    info.preTransform = caps.currentTransform;
    for (auto alpha : {VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR, VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
                       VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR, VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR})
        if (caps.supportedCompositeAlpha & alpha) { info.compositeAlpha = alpha; break; }
    info.presentMode = VK_PRESENT_MODE_FIFO_KHR; info.clipped = VK_TRUE;
    check(vkCreateSwapchainKHR(w->device, &info, nullptr, &w->swapchain), "vkCreateSwapchainKHR");
    check(vkGetSwapchainImagesKHR(w->device, w->swapchain, &images, nullptr), "swapchain images");
    std::vector<VkImage> image_handles(images);
    check(vkGetSwapchainImagesKHR(w->device, w->swapchain, &images, image_handles.data()), "swapchain images");
    w->depth_format = VK_FORMAT_UNDEFINED;
    for (auto candidate : {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D16_UNORM}) {
        VkFormatProperties properties; vkGetPhysicalDeviceFormatProperties(w->physical, candidate, &properties);
        if (properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) { w->depth_format=candidate; break; }
    }
    if (w->depth_format == VK_FORMAT_UNDEFINED) throw std::runtime_error("No supported depth attachment format");
    VkImageCreateInfo depth_image{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    depth_image.imageType = VK_IMAGE_TYPE_2D; depth_image.format = w->depth_format;
    depth_image.extent = {w->extent.width,w->extent.height,1};
    depth_image.mipLevels = depth_image.arrayLayers = 1; depth_image.samples = VK_SAMPLE_COUNT_1_BIT;
    depth_image.tiling = VK_IMAGE_TILING_OPTIMAL; depth_image.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    check(vkCreateImage(w->device,&depth_image,nullptr,&w->depth_image),"depth image");
    VkMemoryRequirements depth_requirements; vkGetImageMemoryRequirements(w->device,w->depth_image,&depth_requirements);
    VkMemoryAllocateInfo depth_allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    depth_allocation.allocationSize = depth_requirements.size;
    depth_allocation.memoryTypeIndex = memory_type(w,depth_requirements.memoryTypeBits,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    check(vkAllocateMemory(w->device,&depth_allocation,nullptr,&w->depth_memory),"depth memory");
    check(vkBindImageMemory(w->device,w->depth_image,w->depth_memory,0),"bind depth memory");
    w->depth_view = image_view(w,w->depth_image,w->depth_format,VK_IMAGE_ASPECT_DEPTH_BIT);
    VkAttachmentDescription attachment{};
    attachment.format = w->format; attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    VkAttachmentDescription attachments[2] = {attachment,{}};
    attachments[1].format = w->depth_format; attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE; attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    VkAttachmentReference ref{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkAttachmentReference depth_ref{1,VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1; subpass.pColorAttachments = &ref;
    subpass.pDepthStencilAttachment = &depth_ref;
    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL; dependency.dstSubpass = 0;
    dependency.srcStageMask = dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    VkRenderPassCreateInfo pass{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    pass.attachmentCount = 2; pass.pAttachments = attachments;
    pass.subpassCount = 1; pass.pSubpasses = &subpass;
    pass.dependencyCount = 1; pass.pDependencies = &dependency;
    check(vkCreateRenderPass(w->device, &pass, nullptr, &w->render_pass), "render pass");
    for (auto image : image_handles) {
        w->views.push_back(image_view(w, image, w->format));
        VkFramebufferCreateInfo fb{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        VkImageView framebuffer_views[] = {w->views.back(),w->depth_view};
        fb.renderPass = w->render_pass; fb.attachmentCount = 2; fb.pAttachments = framebuffer_views;
        fb.width = w->extent.width; fb.height = w->extent.height; fb.layers = 1;
        VkFramebuffer framebuffer{};
        check(vkCreateFramebuffer(w->device, &fb, nullptr, &framebuffer), "framebuffer");
        w->framebuffers.push_back(framebuffer);
        VkSemaphoreCreateInfo semaphore{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        VkSemaphore rendered{};
        check(vkCreateSemaphore(w->device, &semaphore, nullptr, &rendered), "present semaphore");
        w->rendered.push_back(rendered);
    }
    w->sprites = pipeline(w, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    w->lines = pipeline(w, VK_PRIMITIVE_TOPOLOGY_LINE_LIST);
    for (uint32_t i = 0; i < w->materials.size(); ++i)
        w->materials[i] = pipeline(w, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, true, i&1, (i&2)!=0, (i&4)==0, (i&8)!=0);
    w->resize = false;
}

WpcWindow *wpc_window_create(const char *title, int width, int height) {
    auto *w = new WpcWindow{};
    try {
        if (width <= 0 || height <= 0) throw std::runtime_error("Invalid window dimensions");
        w->display = XOpenDisplay(nullptr);
        if (!w->display) throw std::runtime_error("XOpenDisplay failed");
        w->width = width; w->height = height;
        w->xwindow = XCreateSimpleWindow(w->display, DefaultRootWindow(w->display), 0, 0, width, height, 0, 0, 0);
        if (!w->xwindow) throw std::runtime_error("XCreateSimpleWindow failed");
        XStoreName(w->display, w->xwindow, title ? title : "Wacky Races");
        XSelectInput(w->display, w->xwindow, KeyPressMask | KeyReleaseMask | StructureNotifyMask | FocusChangeMask);
        w->wm_delete = XInternAtom(w->display, "WM_DELETE_WINDOW", False);
        XSetWMProtocols(w->display, w->xwindow, &w->wm_delete, 1);
        Bool supported{};
        XkbSetDetectableAutoRepeat(w->display, True, &supported);
        // The original hides the pointer: SetCursor(NULL) at the renderer's
        // setup (0x4ae619, 0x4abece) and ShowCursor(FALSE) until hidden
        // (0x4ac0b0). Natively a blank 1x1 cursor over the window.
        {
            static const char blank[1] = {0};
            const Pixmap bits = XCreateBitmapFromData(w->display, w->xwindow, blank, 1, 1);
            if (bits) {
                XColor black{};
                const Cursor none = XCreatePixmapCursor(w->display, bits, bits, &black, &black, 0, 0);
                if (none) {XDefineCursor(w->display, w->xwindow, none); XFreeCursor(w->display, none);}
                XFreePixmap(w->display, bits);
            }
        }
        XMapWindow(w->display, w->xwindow); XFlush(w->display);
        VkApplicationInfo application{VK_STRUCTURE_TYPE_APPLICATION_INFO};
        application.pApplicationName = "Wacky Races"; application.apiVersion = VK_API_VERSION_1_0;
        const char *extensions[] = {VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_XLIB_SURFACE_EXTENSION_NAME};
        VkInstanceCreateInfo instance{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
        instance.pApplicationInfo = &application; instance.enabledExtensionCount = 2; instance.ppEnabledExtensionNames = extensions;
        check(vkCreateInstance(&instance, nullptr, &w->instance), "vkCreateInstance");
        VkXlibSurfaceCreateInfoKHR surface{VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR};
        surface.dpy = w->display; surface.window = w->xwindow;
        check(vkCreateXlibSurfaceKHR(w->instance, &surface, nullptr, &w->surface), "vkCreateXlibSurfaceKHR");
        uint32_t count{};
        check(vkEnumeratePhysicalDevices(w->instance, &count, nullptr), "physical devices");
        std::vector<VkPhysicalDevice> devices(count);
        check(vkEnumeratePhysicalDevices(w->instance, &count, devices.data()), "physical devices");
        int best = -1;
        for (auto device : devices) {
            VkPhysicalDeviceProperties properties;
            vkGetPhysicalDeviceProperties(device, &properties);
            uint32_t families{};
            vkGetPhysicalDeviceQueueFamilyProperties(device, &families, nullptr);
            std::vector<VkQueueFamilyProperties> queues(families);
            vkGetPhysicalDeviceQueueFamilyProperties(device, &families, queues.data());
            uint32_t extension_count{};
            check(vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, nullptr), "device extensions");
            std::vector<VkExtensionProperties> available(extension_count);
            check(vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, available.data()), "device extensions");
            bool has_swapchain = false;
            for (const auto &ext : available) if (!strcmp(ext.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME)) has_swapchain = true;
            if (!has_swapchain) continue;
            for (uint32_t i = 0; i < families; ++i) {
                VkBool32 present{};
                check(vkGetPhysicalDeviceSurfaceSupportKHR(device, i, w->surface, &present), "surface support");
                int score = properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ? 2 :
                            properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU ? 1 : 0;
                if (present && (queues[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && score > best) {
                    w->physical = device; w->family = i; best = score;
                }
            }
        }
        if (!w->physical) throw std::runtime_error("No Vulkan graphics/present device with swapchain support");
        VkPhysicalDeviceProperties properties; vkGetPhysicalDeviceProperties(w->physical, &properties);
        wpc_log("Vulkan: %s", properties.deviceName);
        float priority = 1;
        VkDeviceQueueCreateInfo queue{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
        queue.queueFamilyIndex = w->family; queue.queueCount = 1; queue.pQueuePriorities = &priority;
        const char *device_extension = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
        VkDeviceCreateInfo device{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
        device.queueCreateInfoCount = 1; device.pQueueCreateInfos = &queue;
        device.enabledExtensionCount = 1; device.ppEnabledExtensionNames = &device_extension;
        check(vkCreateDevice(w->physical, &device, nullptr, &w->device), "vkCreateDevice");
        vkGetDeviceQueue(w->device, w->family, 0, &w->queue);
        VkCommandPoolCreateInfo pool{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
        pool.queueFamilyIndex = w->family;
        check(vkCreateCommandPool(w->device, &pool, nullptr, &w->command_pool), "command pool");
        VkCommandBufferAllocateInfo command{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        command.commandPool = w->command_pool; command.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; command.commandBufferCount = 1;
        check(vkAllocateCommandBuffers(w->device, &command, &w->command), "command buffer");
        VkFenceCreateInfo fence{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO}; fence.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        check(vkCreateFence(w->device, &fence, nullptr, &w->fence), "frame fence");
        VkSemaphoreCreateInfo semaphore{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        check(vkCreateSemaphore(w->device, &semaphore, nullptr, &w->acquired), "acquire semaphore");
        VkDescriptorSetLayoutBinding binding{0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr};
        VkDescriptorSetLayoutCreateInfo descriptor{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        descriptor.bindingCount = 1; descriptor.pBindings = &binding;
        check(vkCreateDescriptorSetLayout(w->device, &descriptor, nullptr, &w->descriptor_layout), "descriptor layout");
        VkDescriptorPoolSize size{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4096*8};
        VkDescriptorPoolCreateInfo descriptors{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        descriptors.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT; descriptors.maxSets = 4096*8;
        descriptors.poolSizeCount = 1; descriptors.pPoolSizes = &size;
        check(vkCreateDescriptorPool(w->device, &descriptors, nullptr, &w->descriptor_pool), "descriptor pool");
        VkPushConstantRange transform{VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT, 0, 96};
        VkPipelineLayoutCreateInfo layout{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        layout.setLayoutCount = 1; layout.pSetLayouts = &w->descriptor_layout;
        layout.pushConstantRangeCount = 1; layout.pPushConstantRanges = &transform;
        check(vkCreatePipelineLayout(w->device, &layout, nullptr, &w->pipeline_layout), "pipeline layout");
        VkSamplerCreateInfo sampler{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        sampler.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        // 0x4b1ad0: Direct3D stage 0 MAG/MIN/MIPFILTER = LINEAR for the 3D
        // draws (samplers 4-7); 2D stays nearest (not traced).
        for (uint32_t mask=0;mask<8;++mask) {
            sampler.magFilter = sampler.minFilter = mask&4 ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
            sampler.addressModeU = mask&1 ? VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE : VK_SAMPLER_ADDRESS_MODE_REPEAT;
            sampler.addressModeV = mask&2 ? VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE : VK_SAMPLER_ADDRESS_MODE_REPEAT;
            check(vkCreateSampler(w->device, &sampler, nullptr, &w->samplers[mask]), "sampler");
        }
        buffer(w, w->vertex_capacity, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, w->vertices, w->vertex_memory);
        check(vkMapMemory(w->device, w->vertex_memory, 0, w->vertex_capacity, 0, &w->mapped), "map vertices");
        swapchain(w);
        w->mvp[0] = w->mvp[5] = w->mvp[10] = w->mvp[15] = 1;
        const uint8_t white[] = {255,255,255,255};
        w->white = wpc_vk_texture_create(w, 1, 1, white);
        if (!w->white) throw std::runtime_error("Could not upload white texture");
        return w;
    } catch (const std::exception &error) {
        wpc_log_error("Platform initialization: %s", error.what());
        wpc_window_destroy(w); return nullptr;
    }
}

static void free_texture(WpcWindow *w, WpcTexture *texture) {
    for (auto descriptor:texture->descriptors)
        if (descriptor) vkFreeDescriptorSets(w->device, w->descriptor_pool, 1, &descriptor);
    if (texture->view) vkDestroyImageView(w->device, texture->view, nullptr);
    if (texture->image) vkDestroyImage(w->device, texture->image, nullptr);
    if (texture->memory) vkFreeMemory(w->device, texture->memory, nullptr);
    delete texture;
}
void wpc_window_destroy(WpcWindow *w) {
    if (!w) return;
    if (w->device) {
        vkDeviceWaitIdle(w->device);
        for (auto *texture : w->textures) free_texture(w, texture);
        destroy_swapchain(w);
        if (w->mapped) vkUnmapMemory(w->device, w->vertex_memory);
        if (w->vertices) vkDestroyBuffer(w->device, w->vertices, nullptr);
        if (w->vertex_memory) vkFreeMemory(w->device, w->vertex_memory, nullptr);
        for (auto sampler:w->samplers) if (sampler) vkDestroySampler(w->device, sampler, nullptr);
        if (w->pipeline_layout) vkDestroyPipelineLayout(w->device, w->pipeline_layout, nullptr);
        if (w->descriptor_pool) vkDestroyDescriptorPool(w->device, w->descriptor_pool, nullptr);
        if (w->descriptor_layout) vkDestroyDescriptorSetLayout(w->device, w->descriptor_layout, nullptr);
        if (w->fence) vkDestroyFence(w->device, w->fence, nullptr);
        if (w->acquired) vkDestroySemaphore(w->device, w->acquired, nullptr);
        if (w->command_pool) vkDestroyCommandPool(w->device, w->command_pool, nullptr);
        vkDestroyDevice(w->device, nullptr);
    }
    if (w->surface) vkDestroySurfaceKHR(w->instance, w->surface, nullptr);
    if (w->instance) vkDestroyInstance(w->instance, nullptr);
    if (w->display) {
        if (w->xwindow) XDestroyWindow(w->display, w->xwindow);
        XCloseDisplay(w->display);
    }
    delete w;
}
WpcTexture *wpc_vk_texture_create(WpcWindow *w, uint32_t width, uint32_t height, const uint8_t *rgba) {
    if (!w || !w->device || w->frame || !width || !height || !rgba) return nullptr;
    auto *texture = new WpcTexture{};
    VkBuffer staging{}; VkDeviceMemory memory{};
    try {
        VkPhysicalDeviceProperties properties; vkGetPhysicalDeviceProperties(w->physical, &properties);
        if (width > properties.limits.maxImageDimension2D || height > properties.limits.maxImageDimension2D)
            throw std::runtime_error("Texture exceeds device dimensions");
        VkDeviceSize bytes = VkDeviceSize(width) * height * 4;
        buffer(w, bytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, staging, memory);
        void *mapped{}; check(vkMapMemory(w->device, memory, 0, bytes, 0, &mapped), "map upload");
        memcpy(mapped, rgba, size_t(bytes)); vkUnmapMemory(w->device, memory);
        VkImageCreateInfo image{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        image.imageType = VK_IMAGE_TYPE_2D; image.extent = {width, height, 1};
        image.mipLevels = image.arrayLayers = 1; image.format = VK_FORMAT_R8G8B8A8_UNORM;
        image.tiling = VK_IMAGE_TILING_OPTIMAL; image.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        image.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        image.samples = VK_SAMPLE_COUNT_1_BIT; image.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        check(vkCreateImage(w->device, &image, nullptr, &texture->image), "texture image");
        VkMemoryRequirements requirements; vkGetImageMemoryRequirements(w->device, texture->image, &requirements);
        VkMemoryAllocateInfo allocate{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        allocate.allocationSize = requirements.size;
        allocate.memoryTypeIndex = memory_type(w, requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        check(vkAllocateMemory(w->device, &allocate, nullptr, &texture->memory), "texture memory");
        check(vkBindImageMemory(w->device, texture->image, texture->memory, 0), "bind texture memory");
        check(vkDeviceWaitIdle(w->device), "wait before upload");
        check(vkResetCommandPool(w->device, w->command_pool, 0), "reset upload pool");
        VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO}; begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        check(vkBeginCommandBuffer(w->command, &begin), "begin upload");
        VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = texture->image; barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED; barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        vkCmdPipelineBarrier(w->command, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                             0, 0, nullptr, 0, nullptr, 1, &barrier);
        VkBufferImageCopy copy{}; copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT,0,0,1}; copy.imageExtent = {width,height,1};
        vkCmdCopyBufferToImage(w->command, staging, texture->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL; barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT; barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(w->command, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                             0, 0, nullptr, 0, nullptr, 1, &barrier);
        check(vkEndCommandBuffer(w->command), "end upload");
        VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO}; submit.commandBufferCount = 1; submit.pCommandBuffers = &w->command;
        check(vkQueueSubmit(w->queue, 1, &submit, VK_NULL_HANDLE), "submit upload");
        check(vkQueueWaitIdle(w->queue), "wait upload");
        vkDestroyBuffer(w->device, staging, nullptr); staging = VK_NULL_HANDLE;
        vkFreeMemory(w->device, memory, nullptr); memory = VK_NULL_HANDLE;
        texture->view = image_view(w, texture->image, VK_FORMAT_R8G8B8A8_UNORM);
        texture->width = width; texture->height = height;
        VkDescriptorSetAllocateInfo descriptor{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        std::array<VkDescriptorSetLayout,8> layouts; layouts.fill(w->descriptor_layout);
        descriptor.descriptorPool = w->descriptor_pool; descriptor.descriptorSetCount = 8; descriptor.pSetLayouts = layouts.data();
        check(vkAllocateDescriptorSets(w->device, &descriptor, texture->descriptors.data()), "texture descriptors");
        std::array<VkDescriptorImageInfo,8> samples;
        std::array<VkWriteDescriptorSet,8> writes{};
        for (uint32_t mask=0;mask<8;++mask) {
            samples[mask]={w->samplers[mask],texture->view,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
            writes[mask].sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[mask].dstSet=texture->descriptors[mask]; writes[mask].descriptorCount=1;
            writes[mask].descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; writes[mask].pImageInfo=&samples[mask];
        }
        vkUpdateDescriptorSets(w->device,8,writes.data(),0,nullptr);
        w->textures.push_back(texture); return texture;
    } catch (const std::exception &error) {
        wpc_log_error("Texture upload: %s", error.what());
        vkDeviceWaitIdle(w->device);
        if (staging) vkDestroyBuffer(w->device, staging, nullptr);
        if (memory) vkFreeMemory(w->device, memory, nullptr);
        free_texture(w, texture); return nullptr;
    }
}
int wpc_vk_texture_update(WpcWindow *w, WpcTexture *texture, const uint8_t *rgba) {
    if (!w || !texture || !rgba || w->frame || texture == w->white) return 0;
    if (std::find(w->textures.begin(), w->textures.end(), texture) == w->textures.end()) return 0;
    VkBuffer staging{}; VkDeviceMemory memory{};
    try {
        const VkDeviceSize bytes = VkDeviceSize(texture->width) * texture->height * 4;
        buffer(w, bytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, staging, memory);
        void *mapped{}; check(vkMapMemory(w->device, memory, 0, bytes, 0, &mapped), "map texture update");
        memcpy(mapped, rgba, size_t(bytes)); vkUnmapMemory(w->device, memory);
        // The previous frame may still sample this image.
        check(vkDeviceWaitIdle(w->device), "wait before texture update");
        check(vkResetCommandPool(w->device, w->command_pool, 0), "reset update pool");
        VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO}; begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        check(vkBeginCommandBuffer(w->command, &begin), "begin texture update");
        VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = texture->image; barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
        barrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT; barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        vkCmdPipelineBarrier(w->command, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                             0, 0, nullptr, 0, nullptr, 1, &barrier);
        VkBufferImageCopy copy{}; copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT,0,0,1}; copy.imageExtent = {texture->width,texture->height,1};
        vkCmdCopyBufferToImage(w->command, staging, texture->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL; barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT; barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(w->command, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                             0, 0, nullptr, 0, nullptr, 1, &barrier);
        check(vkEndCommandBuffer(w->command), "end texture update");
        VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO}; submit.commandBufferCount = 1; submit.pCommandBuffers = &w->command;
        check(vkQueueSubmit(w->queue, 1, &submit, VK_NULL_HANDLE), "submit texture update");
        check(vkQueueWaitIdle(w->queue), "wait texture update");
        vkDestroyBuffer(w->device, staging, nullptr); vkFreeMemory(w->device, memory, nullptr);
        return 1;
    } catch (const std::exception &error) {
        wpc_log_error("Texture update: %s", error.what());
        vkDeviceWaitIdle(w->device);
        if (staging) vkDestroyBuffer(w->device, staging, nullptr);
        if (memory) vkFreeMemory(w->device, memory, nullptr);
        return 0;
    }
}
void wpc_vk_texture_destroy(WpcWindow *w, WpcTexture *texture) {
    if (!w || !texture || w->frame || texture == w->white) return;
    auto found = std::find(w->textures.begin(), w->textures.end(), texture);
    if (found == w->textures.end()) return;
    vkDeviceWaitIdle(w->device); free_texture(w, texture); w->textures.erase(found);
}

static WpcKey key(KeySym symbol) {
    switch (symbol) {
        case XK_Escape: return WPC_KEY_ESCAPE; case XK_Return: return WPC_KEY_RETURN;
        case XK_Up: return WPC_KEY_UP; case XK_Down: return WPC_KEY_DOWN;
        case XK_Left: return WPC_KEY_LEFT; case XK_Right: return WPC_KEY_RIGHT;
        case XK_space: return WPC_KEY_SPACE;
        case XK_Control_L: case XK_Control_R: return WPC_KEY_CONTROL;
        default: return WPC_KEY_UNKNOWN;
    }
}
int wpc_window_poll_events(WpcWindow *w) {
    if (!w) return 1;
    while (XPending(w->display)) {
        XEvent event; XNextEvent(w->display, &event);
        switch (event.type) {
            case ClientMessage: if (Atom(event.xclient.data.l[0]) == w->wm_delete) w->close = true; break;
            case DestroyNotify: w->close = true; w->xwindow = 0; break;
            case ConfigureNotify:
                if (w->width != event.xconfigure.width || w->height != event.xconfigure.height) w->resize = true;
                w->width = event.xconfigure.width; w->height = event.xconfigure.height; break;
            case FocusOut: w->keys.fill(false); w->codes.fill(false); break;
            case KeyPress: case KeyRelease: {
                WpcKey mapped = key(XLookupKeysym(&event.xkey, 0));
                if (mapped != WPC_KEY_UNKNOWN) w->keys[mapped] = event.type == KeyPress;
                const int code = int(event.xkey.keycode) - 8;
                if (code > 0 && code < 256) {
                    w->codes[size_t(code)] = event.type == KeyPress;
                    if (event.type == KeyPress) w->last_code = code;
                }
                if (event.type == KeyPress) {
                    char text[8]; KeySym sym = 0;
                    const int n = XLookupString(&event.xkey, text, sizeof text, &sym, nullptr);
                    if (sym == XK_BackSpace) w->typed += '\b';
                    else for (int i = 0; i < n; ++i) if (static_cast<unsigned char>(text[i]) >= 0x20 && text[i] != 0x7f) w->typed += text[i];
                    if (w->typed.size() > 64) w->typed.erase(0, w->typed.size() - 64);
                }
                break;
            }
        }
    }
    return w->close;
}
void wpc_window_get_size(WpcWindow *w, int *width, int *height) {
    if (!w) return;
    if (width) *width = w->width;
    if (height) *height = w->height;
}
int wpc_input_key_down(WpcWindow *w, WpcKey k) { return w && k > WPC_KEY_UNKNOWN && k < WPC_KEY_COUNT && w->keys[k]; }
void wpc_window_set_fullscreen(WpcWindow *w, int on) {
    if (!w || !w->display || !w->xwindow) return;
    XEvent e{};
    e.xclient.type = ClientMessage; e.xclient.window = w->xwindow;
    e.xclient.message_type = XInternAtom(w->display, "_NET_WM_STATE", False);
    e.xclient.format = 32;
    e.xclient.data.l[0] = on ? 1 : 0; // _NET_WM_STATE_ADD / _REMOVE
    e.xclient.data.l[1] = long(XInternAtom(w->display, "_NET_WM_STATE_FULLSCREEN", False));
    e.xclient.data.l[3] = 1;
    XSendEvent(w->display, DefaultRootWindow(w->display), False, SubstructureRedirectMask | SubstructureNotifyMask, &e);
    XFlush(w->display);
}
int wpc_input_code_down(WpcWindow *w, int code) { return w && code > 0 && code < 256 && w->codes[size_t(code)]; }
int wpc_input_last_code(WpcWindow *w) { if (!w) return 0; const int c = w->last_code; w->last_code = 0; return c; }
size_t wpc_input_text(WpcWindow *w, char *out, size_t capacity) {
    if (!w || !out || !capacity) return 0;
    const size_t n = std::min(capacity, w->typed.size());
    std::memcpy(out, w->typed.data(), n);
    w->typed.erase(0, n);
    return n;
}
int wpc_vk_make_current(WpcWindow *w) { return w && w->device; }
void wpc_window_present(WpcWindow *) {} // end_frame submits and presents.
int wpc_vk_begin_frame(WpcWindow *w) {
    if (!w || w->frame) return -1;
    try {
        if (w->width <= 0 || w->height <= 0) return 0;
        check(vkWaitForFences(w->device, 1, &w->fence, VK_TRUE, UINT64_MAX), "wait frame");
        if (w->resize) swapchain(w);
        VkResult result = vkAcquireNextImageKHR(w->device, w->swapchain, UINT64_MAX, w->acquired, VK_NULL_HANDLE, &w->image_index);
        if (result == VK_ERROR_OUT_OF_DATE_KHR) { w->resize = true; return 0; }
        if (result == VK_SUBOPTIMAL_KHR) w->resize = true; else check(result, "acquire image");
        check(vkResetCommandPool(w->device, w->command_pool, 0), "reset frame pool");
        VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        check(vkBeginCommandBuffer(w->command, &begin), "begin frame");
        VkClearValue clear[2]{}; clear[0].color.float32[3] = 1; clear[1].depthStencil.depth = 1;
        VkRenderPassBeginInfo pass{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        pass.renderPass = w->render_pass; pass.framebuffer = w->framebuffers[w->image_index];
        pass.renderArea.extent = w->extent; pass.clearValueCount = 2; pass.pClearValues = clear;
        vkCmdBeginRenderPass(w->command, &pass, VK_SUBPASS_CONTENTS_INLINE);
        VkViewport viewport{0,0,float(w->extent.width),float(w->extent.height),0,1};
        VkRect2D scissor{{0,0},w->extent};
        vkCmdSetViewport(w->command, 0, 1, &viewport); vkCmdSetScissor(w->command, 0, 1, &scissor);
        w->vertex_offset = 0; w->draw_mode = 0; w->frame = true; return 1;
    } catch (const std::exception &error) {
        wpc_log_error("Begin frame: %s", error.what()); w->close = true; return -1;
    }
}
void wpc_vk_set_clip_rect(WpcWindow *w, int x, int y, int width, int height) {
    if (!w || !w->frame) return;
    int left=std::clamp(x,0,w->width), top=std::clamp(y,0,w->height);
    int right=int(std::clamp(int64_t(x)+width,int64_t(left),int64_t(w->width)));
    int bottom=int(std::clamp(int64_t(y)+height,int64_t(top),int64_t(w->height)));
    VkRect2D clip{{int32_t(int64_t(left)*w->extent.width/w->width),int32_t(int64_t(top)*w->extent.height/w->height)},
                 {uint32_t(int64_t(right-left)*w->extent.width/w->width),uint32_t(int64_t(bottom-top)*w->extent.height/w->height)}};
    vkCmdSetScissor(w->command,0,1,&clip);
}
void wpc_vk_clear(WpcWindow *w, float r, float g, float b) {
    if (!w || !w->frame) return;
    VkClearAttachment attachment{}; attachment.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    attachment.clearValue.color = {{r,g,b,1}};
    VkClearRect rect{}; rect.rect.extent = w->extent; rect.layerCount = 1;
    vkCmdClearAttachments(w->command, 1, &attachment, 1, &rect);
}
int wpc_vk_reserve_vertices(WpcWindow *w, uint32_t count) {
    constexpr size_t limit=256*1024*1024;
    if(!w || !w->device || w->frame || count>limit/sizeof(Vertex)) return 0;
    const size_t bytes=size_t(count)*sizeof(Vertex);
    if(bytes<=w->vertex_capacity) return 1;
    size_t capacity=w->vertex_capacity;
    while(capacity<bytes) capacity=std::min(limit,capacity*2);
    VkBuffer next{};VkDeviceMemory memory{};void* mapped=nullptr;
    try {
        buffer(w,capacity,VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,next,memory);
        check(vkMapMemory(w->device,memory,0,capacity,0,&mapped),"map grown vertices");
        // The previous frame may still reference the old buffer. Allocate the
        // replacement first, then retire the old allocation only after idle.
        check(vkDeviceWaitIdle(w->device),"wait before vertex storage growth");
        vkUnmapMemory(w->device,w->vertex_memory);
        vkDestroyBuffer(w->device,w->vertices,nullptr);
        vkFreeMemory(w->device,w->vertex_memory,nullptr);
        w->vertices=next;w->vertex_memory=memory;w->mapped=mapped;
        w->vertex_capacity=capacity;w->vertex_offset=0;
        wpc_log("Frame vertex storage: %zu bytes",capacity);
        return 1;
    } catch(const std::exception& e) {
        if(mapped) vkUnmapMemory(w->device,memory);
        if(next) vkDestroyBuffer(w->device,next,nullptr);
        if(memory) vkFreeMemory(w->device,memory,nullptr);
        wpc_log_error("Vertex storage growth: %s",e.what());return 0;
    }
}
static void draw(WpcWindow *w, const Vertex *vertices, size_t count, WpcTexture *texture, VkPipeline pipeline, const float *mvp, uint32_t clamp_axes=3, bool fogged=false) {
    if (!w || !w->frame || !texture || !count) return;
    size_t bytes = count * sizeof(Vertex);
    if (bytes > w->vertex_capacity - w->vertex_offset) {
        wpc_log_error("Frame vertex capacity exceeded"); w->close = true; return;
    }
    memcpy(static_cast<uint8_t *>(w->mapped) + w->vertex_offset, vertices, bytes);
    VkDeviceSize offset = w->vertex_offset; w->vertex_offset += bytes;
    vkCmdBindPipeline(w->command, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    vkCmdBindVertexBuffers(w->command, 0, 1, &w->vertices, &offset);
    vkCmdBindDescriptorSets(w->command, VK_PIPELINE_BIND_POINT_GRAPHICS, w->pipeline_layout, 0, 1, &texture->descriptors[clamp_axes&7], 0, nullptr);
    float constants[24]{};
    memcpy(constants,mvp,64);
    if (fogged) memcpy(constants+16,w->fog,32);
    vkCmdPushConstants(w->command, w->pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT, 0, 96, constants);
    vkCmdDraw(w->command, uint32_t(count), 1, 0, 0);
}
static void rect(WpcWindow *w, int x, int y, int width, int height, WpcTexture *texture, float r, float g, float b, float a) {
    if (!w || width <= 0 || height <= 0) return;
    // Coordinates are client pixels, with top-left origin, as used by WPC UI.
    float x0 = 2.f * x / w->width - 1.f, y0 = 2.f * y / w->height - 1.f;
    float x1 = 2.f * (float(x) + width) / w->width - 1.f, y1 = 2.f * (float(y) + height) / w->height - 1.f;
    Vertex v[] = {{x0,y0,0,0,0,r,g,b,a},{x1,y0,0,1,0,r,g,b,a},{x1,y1,0,1,1,r,g,b,a},
                  {x0,y0,0,0,0,r,g,b,a},{x1,y1,0,1,1,r,g,b,a},{x0,y1,0,0,1,r,g,b,a}};
    const float identity[] = {1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    draw(w, v, 6, texture, w->sprites, identity);
}
void wpc_vk_draw_rect(WpcWindow *w, int x, int y, int width, int height, float r,float g,float b,float a) {
    if (w) rect(w,x,y,width,height,w->white,r,g,b,a);
}
void wpc_vk_draw_texture(WpcWindow *w, int x, int y, int width, int height, void *texture) {
    rect(w,x,y,width,height,static_cast<WpcTexture *>(texture),1,1,1,1);
}
void wpc_vk_draw_texture_tinted(WpcWindow *w, int x, int y, int width, int height,
                               WpcTexture *texture, float r,float g,float b,float a) {
    rect(w,x,y,width,height,texture,r,g,b,a);
}
void wpc_vk_set_draw_mode(WpcWindow *w, uint32_t mode) { if (w) w->draw_mode = mode & (WPC_DRAW_ADDITIVE|WPC_DRAW_NO_DEPTH_WRITE|WPC_DRAW_INVERSE); }
void wpc_vk_set_mvp(WpcWindow *w, const float mvp[16]) { if (w && mvp) memcpy(w->mvp,mvp,64); }
void wpc_vk_set_fog(WpcWindow *w, float r, float g, float b, float start, float end) {
    if (!w) return;
    const float fog[8] = {r,g,b,1,start,end,0,0};
    memcpy(w->fog,fog,sizeof fog);
}
void wpc_vk_draw_3d(WpcWindow *w, const WpcVertex3D *vertices, uint32_t count) {
    if (!w || !vertices) return;
    std::vector<Vertex> converted; converted.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
        const auto &v = vertices[i]; converted.push_back({v.x,v.y,v.z,0,0,v.r,v.g,v.b,1});
    }
    draw(w, converted.data(), converted.size(), w->white, w->lines, w->mvp);
}
void wpc_vk_draw_textured_triangles(WpcWindow *w, WpcTexture *texture, const WpcTexturedVertex3D *vertices, uint32_t count) {
    if (!w || !vertices || !texture || count%3) return;
    draw(w,vertices,count,texture,w->materials[0],w->mvp);
}
void wpc_vk_draw_material_triangles(WpcWindow *w, WpcTexture *texture, const WpcTexturedVertex3D *vertices, uint32_t count, uint32_t clamp_axes, int two_sided) {
    if (!w || !vertices || !texture || count%3) return;
    VkPipeline pipeline = w->materials[(two_sided ? 0 : 1) | (w->draw_mode & 14)];
    draw(w,vertices,count,texture,pipeline,w->mvp,(clamp_axes&3)|4,true);
}
void wpc_vk_set_brightness(WpcWindow *w, int value) { if (w) w->brightness = std::clamp(value,0,100); }
int wpc_vk_end_frame(WpcWindow *w) {
    if (!w || !w->frame) return 0;
    // 0x4abf50's gamma ramp, x = (b - 50) x 800 (0x489bb8): ramp[i] =
    // clamp(i x (65535 - |x|) / 256 + max(x, 0)), the same for r, g and b,
    // i.e. the picture blended toward white (x > 0) or black by |x| / 65535.
    if (w->brightness!=50) {
        const int x=(w->brightness-50)*800;
        wpc_vk_set_clip_rect(w,0,0,w->width,w->height);
        const float c=x>0?1.f:0.f;
        rect(w,0,0,w->width,w->height,w->white,c,c,c,float(std::abs(x))/65535.f);
    }
    try {
        vkCmdEndRenderPass(w->command); check(vkEndCommandBuffer(w->command), "end frame");
        VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
        submit.waitSemaphoreCount = 1; submit.pWaitSemaphores = &w->acquired; submit.pWaitDstStageMask = &wait_stage;
        submit.commandBufferCount = 1; submit.pCommandBuffers = &w->command;
        submit.signalSemaphoreCount = 1; submit.pSignalSemaphores = &w->rendered[w->image_index];
        check(vkResetFences(w->device, 1, &w->fence), "reset frame fence");
        check(vkQueueSubmit(w->queue, 1, &submit, w->fence), "submit frame");
        VkPresentInfoKHR present{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
        present.waitSemaphoreCount = 1; present.pWaitSemaphores = &w->rendered[w->image_index];
        present.swapchainCount = 1; present.pSwapchains = &w->swapchain; present.pImageIndices = &w->image_index;
        VkResult result = vkQueuePresentKHR(w->queue, &present);
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) w->resize = true;
        else check(result, "present frame");
        w->frame = false; return 1;
    } catch (const std::exception &error) {
        wpc_log_error("End frame: %s", error.what()); w->frame = false; w->close = true; return 0;
    }
}
double wpc_time_now(void) {
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
void wpc_sleep_ms(uint32_t milliseconds) { std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds)); }
void *wpc_file_open(const char *path, const char *mode) { return fopen(path,mode); }
size_t wpc_file_read(void *file, void *out, size_t size) { return file ? fread(out,1,size,static_cast<FILE *>(file)) : 0; }
void wpc_file_close(void *file) { if (file) fclose(static_cast<FILE *>(file)); }
