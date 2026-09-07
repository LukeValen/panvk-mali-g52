#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <unistd.h>
#define VK_NO_PROTOTYPES
#include "vulkan/vulkan.h"

#define CHECK(x, msg) do { VkResult r = (x); if (r != VK_SUCCESS) { printf("FAILED %s: VkResult=%d\n", msg, r); return 1; } printf("%s ok\n", msg); } while (0)

static PFN_vkGetInstanceProcAddr icd_gpa;
static VkInstance instance;
static PFN_vkGetDeviceProcAddr GetDeviceProcAddr;

#define IPROC(name) (PFN_##name)icd_gpa(instance, #name)
#define DPROC(dev, name) (PFN_##name)GetDeviceProcAddr(dev, #name)

int main(void) {
    void *lib = dlopen("/data/data/com.termux/files/home/funnymdzz-mesa/build/src/panfrost/vulkan/libvulkan_panfrost.so", RTLD_NOW);
    if (!lib) { printf("dlopen failed\n"); return 1; }
    icd_gpa = (PFN_vkGetInstanceProcAddr)dlsym(lib, "vk_icdGetInstanceProcAddr");

    PFN_vkCreateInstance CreateInstance = (PFN_vkCreateInstance)icd_gpa(NULL, "vkCreateInstance");
    VkApplicationInfo app_info = { .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO, .apiVersion = VK_API_VERSION_1_0 };
    VkInstanceCreateInfo inst_info = { .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, .pApplicationInfo = &app_info };
    CHECK(CreateInstance(&inst_info, NULL, &instance), "vkCreateInstance");

    GetDeviceProcAddr = IPROC(vkGetDeviceProcAddr);
    PFN_vkEnumeratePhysicalDevices EnumeratePhysicalDevices = IPROC(vkEnumeratePhysicalDevices);
    PFN_vkGetPhysicalDeviceMemoryProperties GetMemoryProperties = IPROC(vkGetPhysicalDeviceMemoryProperties);
    PFN_vkCreateDevice CreateDevice = IPROC(vkCreateDevice);

    uint32_t count = 1;
    VkPhysicalDevice pdev;
    EnumeratePhysicalDevices(instance, &count, &pdev);

    float priority = 1.0f;
    VkDeviceQueueCreateInfo qinfo = { .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, .queueFamilyIndex = 0, .queueCount = 1, .pQueuePriorities = &priority };
    VkDeviceCreateInfo dinfo = { .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, .queueCreateInfoCount = 1, .pQueueCreateInfos = &qinfo };
    VkDevice device;
    CHECK(CreateDevice(pdev, &dinfo, NULL, &device), "vkCreateDevice");

    PFN_vkCreateBuffer CreateBuffer = DPROC(device, vkCreateBuffer);
    PFN_vkGetBufferMemoryRequirements GetBufferMemoryRequirements = DPROC(device, vkGetBufferMemoryRequirements);
    PFN_vkAllocateMemory AllocateMemory = DPROC(device, vkAllocateMemory);
    PFN_vkBindBufferMemory BindBufferMemory = DPROC(device, vkBindBufferMemory);
    PFN_vkMapMemory MapMemory = DPROC(device, vkMapMemory);

    VkBufferCreateInfo buf_info = { .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .size = 256, .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, .sharingMode = VK_SHARING_MODE_EXCLUSIVE };
    VkBuffer buffer;
    CHECK(CreateBuffer(device, &buf_info, NULL, &buffer), "vkCreateBuffer");

    VkMemoryRequirements mem_req;
    GetBufferMemoryRequirements(device, buffer, &mem_req);

    VkPhysicalDeviceMemoryProperties mem_props;
    GetMemoryProperties(pdev, &mem_props);
    uint32_t mem_type = 0;
    for (uint32_t i = 0; i < mem_props.memoryTypeCount; i++) {
        if ((mem_req.memoryTypeBits & (1 << i)) &&
            (mem_props.memoryTypes[i].propertyFlags & (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))) {
            mem_type = i;
            break;
        }
    }
    printf("using memory type %u\n", mem_type);

    VkMemoryAllocateInfo alloc_info = { .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, .allocationSize = mem_req.size, .memoryTypeIndex = mem_type };
    VkDeviceMemory memory;
    CHECK(AllocateMemory(device, &alloc_info, NULL, &memory), "vkAllocateMemory");
    CHECK(BindBufferMemory(device, buffer, memory, 0), "vkBindBufferMemory");

    void *mapped;
    CHECK(MapMemory(device, memory, 0, 256, 0, &mapped), "vkMapMemory");
    memset(mapped, 0xAA, 256);
    printf("buffer initialized with 0xAA, first u32=0x%08x\n", *(uint32_t*)mapped);

    PFN_vkCreateShaderModule CreateShaderModule = DPROC(device, vkCreateShaderModule);
    PFN_vkCreateDescriptorSetLayout CreateDescriptorSetLayout = DPROC(device, vkCreateDescriptorSetLayout);
    PFN_vkCreatePipelineLayout CreatePipelineLayout = DPROC(device, vkCreatePipelineLayout);
    PFN_vkCreateComputePipelines CreateComputePipelines = DPROC(device, vkCreateComputePipelines);
    PFN_vkCreateDescriptorPool CreateDescriptorPool = DPROC(device, vkCreateDescriptorPool);
    PFN_vkAllocateDescriptorSets AllocateDescriptorSets = DPROC(device, vkAllocateDescriptorSets);
    PFN_vkUpdateDescriptorSets UpdateDescriptorSets = DPROC(device, vkUpdateDescriptorSets);

    int fd = open("/data/data/com.termux/files/home/write_value.spv", O_RDONLY);
    if (fd < 0) { printf("open spv failed\n"); return 1; }
    uint8_t spv_buf[4096];
    ssize_t spv_size = read(fd, spv_buf, sizeof(spv_buf));
    close(fd);
    printf("shader loaded, %zd bytes\n", spv_size);

    VkShaderModuleCreateInfo shader_info = { .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, .codeSize = (size_t)spv_size, .pCode = (uint32_t*)spv_buf };
    VkShaderModule shader;
    CHECK(CreateShaderModule(device, &shader_info, NULL, &shader), "vkCreateShaderModule");

    VkDescriptorSetLayoutBinding binding = { .binding = 0, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT };
    VkDescriptorSetLayoutCreateInfo dsl_info = { .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, .bindingCount = 1, .pBindings = &binding };
    VkDescriptorSetLayout dsl;
    CHECK(CreateDescriptorSetLayout(device, &dsl_info, NULL, &dsl), "vkCreateDescriptorSetLayout");

    VkPipelineLayoutCreateInfo pl_info = { .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, .setLayoutCount = 1, .pSetLayouts = &dsl };
    VkPipelineLayout playout;
    CHECK(CreatePipelineLayout(device, &pl_info, NULL, &playout), "vkCreatePipelineLayout");

    VkPipelineShaderStageCreateInfo stage_info = { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_COMPUTE_BIT, .module = shader, .pName = "main" };
    VkComputePipelineCreateInfo pipe_info = { .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO, .stage = stage_info, .layout = playout };
    VkPipeline pipeline;
    CHECK(CreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipe_info, NULL, &pipeline), "vkCreateComputePipelines");

    VkDescriptorPoolSize pool_size = { .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = 1 };
    VkDescriptorPoolCreateInfo dp_info = { .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO, .maxSets = 1, .poolSizeCount = 1, .pPoolSizes = &pool_size };
    VkDescriptorPool dpool;
    CHECK(CreateDescriptorPool(device, &dp_info, NULL, &dpool), "vkCreateDescriptorPool");

    VkDescriptorSetAllocateInfo ds_alloc = { .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, .descriptorPool = dpool, .descriptorSetCount = 1, .pSetLayouts = &dsl };
    VkDescriptorSet dset;
    CHECK(AllocateDescriptorSets(device, &ds_alloc, &dset), "vkAllocateDescriptorSets");

    VkDescriptorBufferInfo buf_desc = { .buffer = buffer, .offset = 0, .range = 256 };
    VkWriteDescriptorSet write = { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = dset, .dstBinding = 0, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .pBufferInfo = &buf_desc };
    UpdateDescriptorSets(device, 1, &write, 0, NULL);
    printf("pipeline+descriptors ready\n");

    PFN_vkCreateCommandPool CreateCommandPool = DPROC(device, vkCreateCommandPool);
    PFN_vkAllocateCommandBuffers AllocateCommandBuffers = DPROC(device, vkAllocateCommandBuffers);
    PFN_vkBeginCommandBuffer BeginCommandBuffer = DPROC(device, vkBeginCommandBuffer);
    PFN_vkCmdBindPipeline CmdBindPipeline = DPROC(device, vkCmdBindPipeline);
    PFN_vkCmdBindDescriptorSets CmdBindDescriptorSets = DPROC(device, vkCmdBindDescriptorSets);
    PFN_vkCmdDispatch CmdDispatch = DPROC(device, vkCmdDispatch);
    PFN_vkEndCommandBuffer EndCommandBuffer = DPROC(device, vkEndCommandBuffer);
    PFN_vkGetDeviceQueue GetDeviceQueue = DPROC(device, vkGetDeviceQueue);
    PFN_vkQueueSubmit QueueSubmit = DPROC(device, vkQueueSubmit);
    PFN_vkQueueWaitIdle QueueWaitIdle = DPROC(device, vkQueueWaitIdle);

    VkCommandPoolCreateInfo cp_info = { .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, .queueFamilyIndex = 0 };
    VkCommandPool cpool;
    CHECK(CreateCommandPool(device, &cp_info, NULL, &cpool), "vkCreateCommandPool");

    VkCommandBufferAllocateInfo cb_alloc = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, .commandPool = cpool, .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY, .commandBufferCount = 1 };
    VkCommandBuffer cmdbuf;
    CHECK(AllocateCommandBuffers(device, &cb_alloc, &cmdbuf), "vkAllocateCommandBuffers");

    VkCommandBufferBeginInfo begin_info = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    CHECK(BeginCommandBuffer(cmdbuf, &begin_info), "vkBeginCommandBuffer");
    CmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    CmdBindDescriptorSets(cmdbuf, VK_PIPELINE_BIND_POINT_COMPUTE, playout, 0, 1, &dset, 0, NULL);
    CmdDispatch(cmdbuf, 1, 1, 1);
    CHECK(EndCommandBuffer(cmdbuf), "vkEndCommandBuffer");
    printf("command buffer recorded\n");

    VkQueue queue;
    GetDeviceQueue(device, 0, 0, &queue);

    VkSubmitInfo submit_info = { .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO, .commandBufferCount = 1, .pCommandBuffers = &cmdbuf };
    printf("calling vkQueueSubmit (this exercises our kbase patch)...\n");
    CHECK(QueueSubmit(queue, 1, &submit_info, VK_NULL_HANDLE), "vkQueueSubmit");

    printf("calling vkQueueWaitIdle...\n");
    CHECK(QueueWaitIdle(queue), "vkQueueWaitIdle");

    printf("buffer after dispatch, first u32=0x%08x (expected 0x00000309 = 777)\n", *(uint32_t*)mapped);

    return 0;
}
