#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <xcb/xcb.h>
#define VK_USE_PLATFORM_XCB_KHR
#define VK_NO_PROTOTYPES
#include "vulkan/vulkan.h"

#define CHECK(x, msg) do { VkResult r = (x); if (r != VK_SUCCESS) { printf("FAILED %s: VkResult=%d\n", msg, r); return 1; } printf("%s ok\n", msg); } while (0)

static PFN_vkGetInstanceProcAddr icd_gpa;
static VkInstance instance;
static PFN_vkGetDeviceProcAddr GetDeviceProcAddr;
#define IPROC(name) (PFN_##name)icd_gpa(instance, #name)
#define DPROC(dev, name) (PFN_##name)GetDeviceProcAddr(dev, #name)

static int load_file(const char *path, uint8_t *buf, size_t buf_cap, size_t *size_out) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) return -1;
    ssize_t n = read(fd, buf, buf_cap);
    close(fd);
    if (n < 0) return -1;
    *size_out = (size_t)n;
    return 0;
}

int main(void) {
    xcb_connection_t *conn = xcb_connect(NULL, NULL);
    if (xcb_connection_has_error(conn)) { printf("xcb_connect failed\n"); return 1; }
    const xcb_setup_t *setup = xcb_get_setup(conn);
    xcb_screen_t *xcb_screen = xcb_setup_roots_iterator(setup).data;
    xcb_window_t win = xcb_generate_id(conn);
    xcb_create_window(conn, XCB_COPY_FROM_PARENT, win, xcb_screen->root,
                       0, 0, 320, 240, 1,
                       XCB_WINDOW_CLASS_INPUT_OUTPUT, xcb_screen->root_visual, 0, NULL);
    xcb_map_window(conn, win);
    xcb_flush(conn);
    printf("XCB window ready\n");

    void *lib = dlopen("/data/data/com.termux/files/home/funnymdzz-mesa/build/src/panfrost/vulkan/libvulkan_panfrost.so", RTLD_NOW);
    icd_gpa = (PFN_vkGetInstanceProcAddr)dlsym(lib, "vk_icdGetInstanceProcAddr");

    PFN_vkCreateInstance CreateInstance = (PFN_vkCreateInstance)icd_gpa(NULL, "vkCreateInstance");
    const char *ext_names[] = { "VK_KHR_surface", "VK_KHR_xcb_surface" };
    VkInstanceCreateInfo inst_info = { .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, .enabledExtensionCount = 2, .ppEnabledExtensionNames = ext_names };
    CHECK(CreateInstance(&inst_info, NULL, &instance), "vkCreateInstance");

    PFN_vkCreateXcbSurfaceKHR CreateXcbSurfaceKHR = IPROC(vkCreateXcbSurfaceKHR);
    VkXcbSurfaceCreateInfoKHR surf_info = { .sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR, .connection = conn, .window = win };
    VkSurfaceKHR surface;
    CHECK(CreateXcbSurfaceKHR(instance, &surf_info, NULL, &surface), "vkCreateXcbSurfaceKHR");

    GetDeviceProcAddr = IPROC(vkGetDeviceProcAddr);
    PFN_vkEnumeratePhysicalDevices EnumeratePhysicalDevices = IPROC(vkEnumeratePhysicalDevices);
    PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR GetSurfaceCapabilitiesKHR = IPROC(vkGetPhysicalDeviceSurfaceCapabilitiesKHR);
    PFN_vkGetPhysicalDeviceSurfaceSupportKHR GetSurfaceSupportKHR = IPROC(vkGetPhysicalDeviceSurfaceSupportKHR);
    PFN_vkGetPhysicalDeviceSurfaceFormatsKHR GetSurfaceFormatsKHR = IPROC(vkGetPhysicalDeviceSurfaceFormatsKHR);
    PFN_vkGetPhysicalDeviceMemoryProperties GetMemoryProperties = IPROC(vkGetPhysicalDeviceMemoryProperties);
    PFN_vkCreateDevice CreateDevice = IPROC(vkCreateDevice);

    uint32_t pcount = 1;
    VkPhysicalDevice pdev;
    printf("before EnumeratePhysicalDevices\n"); fflush(stdout);
    EnumeratePhysicalDevices(instance, &pcount, &pdev);
    printf("after EnumeratePhysicalDevices\n"); fflush(stdout);

    VkSurfaceCapabilitiesKHR caps;
    VkBool32 supported = VK_FALSE;
    CHECK(GetSurfaceSupportKHR(pdev, 0, surface, &supported), "vkGetPhysicalDeviceSurfaceSupportKHR");
    printf("presentation supported: %s\n", supported ? "YES" : "NO"); fflush(stdout);
    printf("before GetSurfaceCapabilitiesKHR\n"); fflush(stdout);
    CHECK(GetSurfaceCapabilitiesKHR(pdev, surface, &caps), "vkGetPhysicalDeviceSurfaceCapabilitiesKHR");

    uint32_t fmt_count = 0;
    GetSurfaceFormatsKHR(pdev, surface, &fmt_count, NULL);
    VkSurfaceFormatKHR formats[16];
    if (fmt_count > 16) fmt_count = 16;
    GetSurfaceFormatsKHR(pdev, surface, &fmt_count, formats);
    VkSurfaceFormatKHR chosen = formats[0];
    printf("using surface format=%d colorspace=%d, extent=%ux%u\n",
           chosen.format, chosen.colorSpace, caps.currentExtent.width, caps.currentExtent.height);

    const char *dev_ext_names[] = { "VK_KHR_swapchain" };
    float priority = 1.0f;
    VkDeviceQueueCreateInfo qinfo = { .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, .queueFamilyIndex = 0, .queueCount = 1, .pQueuePriorities = &priority };
    VkDeviceCreateInfo dinfo = { .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, .queueCreateInfoCount = 1, .pQueueCreateInfos = &qinfo, .enabledExtensionCount = 1, .ppEnabledExtensionNames = dev_ext_names };
    VkDevice device;
    CHECK(CreateDevice(pdev, &dinfo, NULL, &device), "vkCreateDevice");

    PFN_vkCreateSwapchainKHR CreateSwapchainKHR = DPROC(device, vkCreateSwapchainKHR);
    PFN_vkGetSwapchainImagesKHR GetSwapchainImagesKHR = DPROC(device, vkGetSwapchainImagesKHR);

    uint32_t img_count = caps.minImageCount;
    VkSwapchainCreateInfoKHR sc_info = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = surface,
        .minImageCount = img_count,
        .imageFormat = chosen.format,
        .imageColorSpace = chosen.colorSpace,
        .imageExtent = caps.currentExtent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .preTransform = caps.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = VK_PRESENT_MODE_FIFO_KHR,
        .clipped = VK_TRUE,
    };
    VkSwapchainKHR swapchain;
    CHECK(CreateSwapchainKHR(device, &sc_info, NULL, &swapchain), "vkCreateSwapchainKHR");

    uint32_t sc_img_count = 0;
    GetSwapchainImagesKHR(device, swapchain, &sc_img_count, NULL);
    VkImage sc_images[8];
    if (sc_img_count > 8) sc_img_count = 8;
    GetSwapchainImagesKHR(device, swapchain, &sc_img_count, sc_images);
    printf("swapchain created, %u images\n", sc_img_count);

    PFN_vkCreateImageView CreateImageView = DPROC(device, vkCreateImageView);
    PFN_vkCreateRenderPass CreateRenderPass = DPROC(device, vkCreateRenderPass);
    PFN_vkCreateFramebuffer CreateFramebuffer = DPROC(device, vkCreateFramebuffer);
    PFN_vkCreateShaderModule CreateShaderModule = DPROC(device, vkCreateShaderModule);
    PFN_vkCreatePipelineLayout CreatePipelineLayout = DPROC(device, vkCreatePipelineLayout);
    PFN_vkCreateGraphicsPipelines CreateGraphicsPipelines = DPROC(device, vkCreateGraphicsPipelines);

    VkAttachmentDescription attach = {
        .format = chosen.format,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
    };
    VkAttachmentReference color_ref = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
    VkSubpassDescription subpass = { .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS, .colorAttachmentCount = 1, .pColorAttachments = &color_ref };
    VkRenderPassCreateInfo rp_info = { .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, .attachmentCount = 1, .pAttachments = &attach, .subpassCount = 1, .pSubpasses = &subpass };
    VkRenderPass render_pass;
    CHECK(CreateRenderPass(device, &rp_info, NULL, &render_pass), "vkCreateRenderPass");

    VkImageView views[8];
    VkFramebuffer framebuffers[8];
    for (uint32_t i = 0; i < sc_img_count; i++) {
        VkImageViewCreateInfo vi = { .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, .image = sc_images[i], .viewType = VK_IMAGE_VIEW_TYPE_2D, .format = chosen.format, .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 } };
        CHECK(CreateImageView(device, &vi, NULL, &views[i]), "vkCreateImageView(swapchain)");
        VkFramebufferCreateInfo fb = { .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, .renderPass = render_pass, .attachmentCount = 1, .pAttachments = &views[i], .width = caps.currentExtent.width, .height = caps.currentExtent.height, .layers = 1 };
        CHECK(CreateFramebuffer(device, &fb, NULL, &framebuffers[i]), "vkCreateFramebuffer(swapchain)");
    }
    printf("views+framebuffers ready for %u images\n", sc_img_count);

    size_t vert_size, frag_size;
    uint8_t vert_code[8192], frag_code[8192];
    load_file("/data/data/com.termux/files/home/triangle_vert.spv", vert_code, sizeof(vert_code), &vert_size);
    load_file("/data/data/com.termux/files/home/triangle_frag.spv", frag_code, sizeof(frag_code), &frag_size);

    VkShaderModuleCreateInfo vsmi = { .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, .codeSize = vert_size, .pCode = (uint32_t*)vert_code };
    VkShaderModule vert_module;
    CHECK(CreateShaderModule(device, &vsmi, NULL, &vert_module), "vkCreateShaderModule(vert)");
    VkShaderModuleCreateInfo fsmi = { .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, .codeSize = frag_size, .pCode = (uint32_t*)frag_code };
    VkShaderModule frag_module;
    CHECK(CreateShaderModule(device, &fsmi, NULL, &frag_module), "vkCreateShaderModule(frag)");

    VkPipelineLayoutCreateInfo pl_info = { .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    VkPipelineLayout playout;
    CHECK(CreatePipelineLayout(device, &pl_info, NULL, &playout), "vkCreatePipelineLayout");

    VkPipelineShaderStageCreateInfo stages[2] = {
        { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_VERTEX_BIT, .module = vert_module, .pName = "main" },
        { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_FRAGMENT_BIT, .module = frag_module, .pName = "main" },
    };
    VkPipelineVertexInputStateCreateInfo vi_state = { .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
    VkPipelineInputAssemblyStateCreateInfo ia_state = { .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST };
    VkViewport viewport = { 0, 0, (float)caps.currentExtent.width, (float)caps.currentExtent.height, 0, 1 };
    VkRect2D scissor = { {0,0}, caps.currentExtent };
    VkPipelineViewportStateCreateInfo vp_state = { .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, .viewportCount = 1, .pViewports = &viewport, .scissorCount = 1, .pScissors = &scissor };
    VkPipelineRasterizationStateCreateInfo rs_state = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, .polygonMode = VK_POLYGON_MODE_FILL, .cullMode = VK_CULL_MODE_NONE, .lineWidth = 1.0f };
    VkPipelineMultisampleStateCreateInfo ms_state = { .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT };
    VkPipelineColorBlendAttachmentState cb_attach = { .colorWriteMask = 0xF };
    VkPipelineColorBlendStateCreateInfo cb_state = { .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, .attachmentCount = 1, .pAttachments = &cb_attach };
    VkGraphicsPipelineCreateInfo gp_info = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = 2, .pStages = stages,
        .pVertexInputState = &vi_state, .pInputAssemblyState = &ia_state,
        .pViewportState = &vp_state, .pRasterizationState = &rs_state,
        .pMultisampleState = &ms_state, .pColorBlendState = &cb_state,
        .layout = playout, .renderPass = render_pass, .subpass = 0,
    };
    VkPipeline pipeline;
    CHECK(CreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &gp_info, NULL, &pipeline), "vkCreateGraphicsPipelines");

    PFN_vkCreateCommandPool CreateCommandPool = DPROC(device, vkCreateCommandPool);
    PFN_vkAllocateCommandBuffers AllocateCommandBuffers = DPROC(device, vkAllocateCommandBuffers);
    PFN_vkBeginCommandBuffer BeginCommandBuffer = DPROC(device, vkBeginCommandBuffer);
    PFN_vkCmdBeginRenderPass CmdBeginRenderPass = DPROC(device, vkCmdBeginRenderPass);
    PFN_vkCmdBindPipeline CmdBindPipeline = DPROC(device, vkCmdBindPipeline);
    PFN_vkCmdDraw CmdDraw = DPROC(device, vkCmdDraw);
    PFN_vkCmdEndRenderPass CmdEndRenderPass = DPROC(device, vkCmdEndRenderPass);
    PFN_vkEndCommandBuffer EndCommandBuffer = DPROC(device, vkEndCommandBuffer);
    PFN_vkGetDeviceQueue GetDeviceQueue = DPROC(device, vkGetDeviceQueue);
    PFN_vkQueueSubmit QueueSubmit = DPROC(device, vkQueueSubmit);
    PFN_vkQueueWaitIdle QueueWaitIdle = DPROC(device, vkQueueWaitIdle);
    PFN_vkAcquireNextImageKHR AcquireNextImageKHR = DPROC(device, vkAcquireNextImageKHR);
    PFN_vkQueuePresentKHR QueuePresentKHR = DPROC(device, vkQueuePresentKHR);
    PFN_vkCreateSemaphore CreateSemaphore = DPROC(device, vkCreateSemaphore);

    VkCommandPoolCreateInfo cp_info = { .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, .queueFamilyIndex = 0 };
    VkCommandPool cpool;
    CHECK(CreateCommandPool(device, &cp_info, NULL, &cpool), "vkCreateCommandPool");

    VkCommandBuffer cmdbufs[8];
    VkCommandBufferAllocateInfo cb_alloc = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, .commandPool = cpool, .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY, .commandBufferCount = sc_img_count };
    CHECK(AllocateCommandBuffers(device, &cb_alloc, cmdbufs), "vkAllocateCommandBuffers");

    for (uint32_t i = 0; i < sc_img_count; i++) {
        VkCommandBufferBeginInfo begin_info = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
        BeginCommandBuffer(cmdbufs[i], &begin_info);
        VkClearValue clear = { .color = { .float32 = {0.0f, 0.0f, 0.2f, 1.0f} } };
        VkRenderPassBeginInfo rp_begin = { .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, .renderPass = render_pass, .framebuffer = framebuffers[i], .renderArea = { {0,0}, caps.currentExtent }, .clearValueCount = 1, .pClearValues = &clear };
        CmdBeginRenderPass(cmdbufs[i], &rp_begin, VK_SUBPASS_CONTENTS_INLINE);
        CmdBindPipeline(cmdbufs[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        CmdDraw(cmdbufs[i], 3, 1, 0, 0);
        CmdEndRenderPass(cmdbufs[i]);
        EndCommandBuffer(cmdbufs[i]);
    }
    printf("command buffers recorded for all swapchain images\n");

    VkSemaphoreCreateInfo sem_info = { .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    VkSemaphore acquire_sem, present_sem;
    CreateSemaphore(device, &sem_info, NULL, &acquire_sem);
    CreateSemaphore(device, &sem_info, NULL, &present_sem);

    VkQueue queue;
    GetDeviceQueue(device, 0, 0, &queue);

    for (int frame = 0; frame < 3; frame++) {
        uint32_t img_idx;
        VkResult ar = AcquireNextImageKHR(device, swapchain, UINT64_MAX, acquire_sem, VK_NULL_HANDLE, &img_idx);
        printf("frame %d: vkAcquireNextImageKHR -> VkResult=%d, image index=%u\n", frame, ar, img_idx);
        if (ar != VK_SUCCESS && ar != VK_SUBOPTIMAL_KHR) { printf("acquire failed\n"); return 1; }

        VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        VkSubmitInfo submit_info = {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .waitSemaphoreCount = 1, .pWaitSemaphores = &acquire_sem, .pWaitDstStageMask = &wait_stage,
            .commandBufferCount = 1, .pCommandBuffers = &cmdbufs[img_idx],
            .signalSemaphoreCount = 1, .pSignalSemaphores = &present_sem,
        };
        VkResult sr = QueueSubmit(queue, 1, &submit_info, VK_NULL_HANDLE);
        printf("frame %d: vkQueueSubmit -> VkResult=%d\n", frame, sr);
        if (sr != VK_SUCCESS) return 1;

        VkPresentInfoKHR present_info = {
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .waitSemaphoreCount = 1, .pWaitSemaphores = &present_sem,
            .swapchainCount = 1, .pSwapchains = &swapchain, .pImageIndices = &img_idx,
        };
        VkResult pr = QueuePresentKHR(queue, &present_info);
        printf("frame %d: vkQueuePresentKHR -> VkResult=%d\n", frame, pr);
        QueueWaitIdle(queue);
        xcb_flush(conn);
        usleep(500000);
    }

    printf("PRESENTATION LOOP COMPLETE - %d frames presented\n", 3);
    return 0;
}
