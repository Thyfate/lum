#define VMA_IMPLEMENTATION
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#include "render.hpp" 

// #include <vma/vk_mem_alloc.h>

// #include "defines.hpp"

using namespace std;
using namespace glm;

// #define copy_Buffer(a, ...) do {println; (copy_Buffer)(a, __VA_ARGS__);} while(0)

//they are global because its easier lol
static vector<const char*> instanceLayers = {
    #ifndef VKNDEBUG
    "VK_LAYER_KHRONOS_validation",
    #endif
    "VK_LAYER_LUNARG_monitor",
};
static vector<const char*> instanceExtensions = {
    VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
    #ifndef VKNDEBUG
    VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
    #endif

    VK_KHR_SURFACE_EXTENSION_NAME,
    "VK_KHR_win32_surface",
};
static vector<const char*>   deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME,
    // VK_NV_DEVICE_DIAGNOSTIC_CHECKPOINTS_EXTENSION_NAME
    // VK_NV_device_diagnostic_checkpoints
    // "VK_KHR_shader_non_semantic_info"
};

static tuple<int, int> get_block_xy(int N);

void Renderer::init(int x_size, int y_size, int z_size, int block_palette_size, int copy_queue_size, vec2 ratio, bool vsync, bool fullscreen){
    world_size = ivec3(x_size, y_size, z_size);
     origin_world.allocate(world_size);
    current_world.allocate(world_size);
    _ratio = ratio;
    is_scaled = (ratio != vec2(1));
    is_vsync = vsync;
    is_fullscreen = fullscreen;
// void init(VisualWorld &_world){
    // _world = world;
    create_Window();
    VK_CHECK(volkInitialize());
    get_Instance_Extensions();
    // get_PhysicalDevice_Extensions();
    create_Instance();
    volkLoadInstance(instance);   
    
#ifndef VKNDEBUG
    setup_Debug_Messenger();
#endif
    create_Surface();
    pick_Physical_Device();
    create_Logical_Device();

    //AMD VMA 
    create_Allocator();
    
    create_Swapchain();
    create_Swapchain_Image_Views();

    create_RenderPass_Graphical();
    create_RenderPass_RayGen();

    create_Command_Pool();
    create_Command_Buffers(&graphicalCommandBuffers, MAX_FRAMES_IN_FLIGHT);
    create_Command_Buffers(&   rayGenCommandBuffers, MAX_FRAMES_IN_FLIGHT);
    create_Command_Buffers(&  computeCommandBuffers, MAX_FRAMES_IN_FLIGHT);

    create_samplers();

#define RAYTRACED_IMAGE_FORMAT VK_FORMAT_R16G16B16A16_UNORM
    create_Image_Storages(&raytraced_frame,
        VK_IMAGE_TYPE_2D,
        RAYTRACED_IMAGE_FORMAT,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT,
        VK_IMAGE_LAYOUT_GENERAL,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_ACCESS_SHADER_WRITE_BIT,
        {raytraceExtent.width, raytraceExtent.height, 1});
    create_Image_Storages(&denoised_frame,
        VK_IMAGE_TYPE_2D,
        RAYTRACED_IMAGE_FORMAT,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT,
        VK_IMAGE_LAYOUT_GENERAL,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_ACCESS_SHADER_WRITE_BIT, 
        {raytraceExtent.width, raytraceExtent.height, 1});
    
    //gbuffer area
    // create_Buffer_Storages(depthStaging.buffers, depthStaging.allocs,
    //     VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
    //     sizeof(float32)*swapChainExtent.width*swapChainExtent.height);
    // create_Image_Storages(depth, 
    //     VK_IMAGE_TYPE_2D,
    //     VK_FORMAT_R32_SFLOAT,
    //     VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
    //     VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
    //     VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
    //     VK_IMAGE_ASPECT_COLOR_BIT,
    //     VK_IMAGE_LAYOUT_GENERAL,
    //     VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
    //     VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT, 
    //     {swapChainExtent.width, swapChainExtent.height, 1}); //copy to this 
    create_Image_Storages(&matNorm,
        VK_IMAGE_TYPE_2D,
        VK_FORMAT_R8G8B8A8_SNORM,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT,
        VK_IMAGE_LAYOUT_GENERAL,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT, 
        {swapChainExtent.width, swapChainExtent.height, 1});
    #define OLD_UV_FORMAT VK_FORMAT_R32G32_SFLOAT
    create_Image_Storages(&oldUv,
        VK_IMAGE_TYPE_2D,
        OLD_UV_FORMAT,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT,
        VK_IMAGE_LAYOUT_GENERAL,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT, 
        {swapChainExtent.width, swapChainExtent.height, 1});
    // create_Image_Storages(gBuffer,
    //     VK_IMAGE_TYPE_2D,
    //     VK_FORMAT_R32G32B32A32_UINT,
    //     VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
    //     VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
    //     VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
    //     VK_IMAGE_ASPECT_COLOR_BIT,
    //     VK_IMAGE_LAYOUT_GENERAL,
    //     VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
    //     VK_ACCESS_SHADER_WRITE_BIT,
    //     {swapChainExtent.width, swapChainExtent.height, 1});
    if(is_scaled){
        //gbuffer area
        // create_Image_Storages(depth_downscaled,
        //     VK_IMAGE_TYPE_2D,
        //     VK_FORMAT_R32_SFLOAT,
        //     VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        //     VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        //     VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        //     VK_IMAGE_ASPECT_COLOR_BIT,
        //     VK_IMAGE_LAYOUT_GENERAL,
        //     VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        //     VK_ACCESS_SHADER_WRITE_BIT, 
        //     {raytraceExtent.width, raytraceExtent.height, 1});
        create_Image_Storages(&oldUv_downscaled,
            VK_IMAGE_TYPE_2D,
            OLD_UV_FORMAT,
            VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
            VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
            VK_IMAGE_ASPECT_COLOR_BIT,
            VK_IMAGE_LAYOUT_GENERAL,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_ACCESS_SHADER_WRITE_BIT, 
            {raytraceExtent.width, raytraceExtent.height, 1});
        create_Image_Storages(&matNorm_downscaled,
            VK_IMAGE_TYPE_2D,
            VK_FORMAT_R8G8B8A8_SNORM,
            VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
            VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
            VK_IMAGE_ASPECT_COLOR_BIT,
            VK_IMAGE_LAYOUT_GENERAL,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_ACCESS_SHADER_WRITE_BIT, 
            {raytraceExtent.width, raytraceExtent.height, 1});

        create_Image_Storages(&upscaled_frame,
            VK_IMAGE_TYPE_2D,
            RAYTRACED_IMAGE_FORMAT,
            VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
            VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
            VK_IMAGE_ASPECT_COLOR_BIT,
            VK_IMAGE_LAYOUT_GENERAL,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_ACCESS_SHADER_WRITE_BIT, 
            {swapChainExtent.width, swapChainExtent.height, 1});
        // create_Image_Storages(gBuffer_downscaled,
        //     VK_IMAGE_TYPE_2D,
        //     VK_FORMAT_R32G32B32A32_UINT,
        //     VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        //     VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        //     VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        //     VK_IMAGE_ASPECT_COLOR_BIT,
        //     VK_IMAGE_LAYOUT_GENERAL,
        //     VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        //     VK_ACCESS_SHADER_WRITE_BIT,
        //     {raytraceExtent.width, raytraceExtent.height, 1});
    } //if is scaled
    #define DEPTH_LAYOUT VK_IMAGE_LAYOUT_GENERAL
    create_Image_Storages(&depthForChecks,
        VK_IMAGE_TYPE_2D,
        VK_FORMAT_D32_SFLOAT,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        VK_IMAGE_ASPECT_DEPTH_BIT,
        DEPTH_LAYOUT,
        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        // {swapChainExtent.height, swapChainExtent.width, 1});
        {swapChainExtent.width, swapChainExtent.height, 1});
    create_Image_Storages(&step_count,
        VK_IMAGE_TYPE_2D,
        VK_FORMAT_R8G8B8A8_UINT,
        VK_IMAGE_USAGE_STORAGE_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT,
        VK_IMAGE_LAYOUT_GENERAL,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT ,
        {(raytraceExtent.width+7) / 8, (raytraceExtent.height+7) / 8, 1}); // local_size / warp_size of raytracer

    create_Buffer_Storages(&staging_world,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        world_size.x*world_size.y*world_size.z*sizeof(BlockID_t), true);
    create_Image_Storages(&world,
        VK_IMAGE_TYPE_3D,
        VK_FORMAT_R16_SINT,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT,
        VK_IMAGE_LAYOUT_GENERAL,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT,
        world_size); //TODO: dynamic
    create_Image_Storages(&origin_block_palette,
        VK_IMAGE_TYPE_3D,
        VK_FORMAT_R8G8_UINT,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT,
        VK_IMAGE_LAYOUT_GENERAL,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
        {16*BLOCK_PALETTE_SIZE_X, 16*BLOCK_PALETTE_SIZE_Y, 16}); //TODO: dynamic
    create_Image_Storages(&block_palette,
        VK_IMAGE_TYPE_3D,
        VK_FORMAT_R8G8_UINT,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT,
        VK_IMAGE_LAYOUT_GENERAL,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
        {16*BLOCK_PALETTE_SIZE_X, 16*BLOCK_PALETTE_SIZE_Y, 16}); //TODO: dynamic
    create_Image_Storages(&material_palette,
        VK_IMAGE_TYPE_2D,
        VK_FORMAT_R32_SFLOAT, //try R32G32
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT,
        VK_IMAGE_LAYOUT_GENERAL,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_MEMORY_READ_BIT ,
        {6, 256, 1}); //TODO: dynamic, text formats, pack Material into smth other than 6 floats :)

    // create_Buffer_Storages(vector<VkBuffer>* buffers, vector<VmaAllocation>* allocs, VkBufferUsageFlags usage, u32 size)
    // create_Buffer_Storages(depthStaginBuffers, paletteCounterBufferAllocations,
    //     VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
    //     sizeof(int));
    // create_Buffer_Storages(copyCounterBuffers, copyCounterBufferAllocations,
    //     VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
    //     sizeof(int)*(copy_queue_size));
    create_Buffer_Storages(&RayGenUniformBuffers,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        sizeof(mat4)*2, true);
    RayGenUniformMapped.resize(MAX_FRAMES_IN_FLIGHT);
    staging_world_mapped.resize(MAX_FRAMES_IN_FLIGHT);
    for (int i=0; i<MAX_FRAMES_IN_FLIGHT; i++){
        vmaMapMemory(VMAllocator, RayGenUniformBuffers[i].alloc, &RayGenUniformMapped[i]);
        vmaMapMemory(VMAllocator,  staging_world[i].alloc, &staging_world_mapped[i]);
    }
    
    vector<vector<VkImageView>> swapViews = {};
    swapViews.resize(1);
    // for()
    for(int i=0; i<swapchain_images.size(); i++) {
        swapViews[0].push_back(swapchain_images[i].view);
    }
    create_N_Framebuffers(&swapChainFramebuffers, &swapViews, graphicalRenderPass, swapchain_images.size(), swapChainExtent.width, swapChainExtent.height);
// printl(swapchain_images.images.size());
    // vector<vector<VkImageView>> rayGenVeiws = {gBuffer.views, rayGenNormImageViews, rayGenPosDiffImageViews, depth.view};

    vector<vector<VkImageView>> rayGenVeiws = {};
    rayGenVeiws.resize(3);
    for(int i=0; i<MAX_FRAMES_IN_FLIGHT; i++) {
        rayGenVeiws[0].push_back(       matNorm[i].view);
        rayGenVeiws[1].push_back(         oldUv[i].view);
        rayGenVeiws[2].push_back(depthForChecks[i].view);
    }
    create_N_Framebuffers(&rayGenFramebuffers, &rayGenVeiws, rayGenRenderPass, MAX_FRAMES_IN_FLIGHT, swapChainExtent.width, swapChainExtent.height);
    
    create_Descriptor_Set_Layouts();
    create_Descriptor_Pool();
    allocate_Descriptors();

    // setup_Blockify_Descriptors();
    // setup_Copy_Descriptors();
    //setup for map?
    // setup_Df_Descriptors();
    setup_Raytrace_Descriptors();
    setup_Denoise_Descriptors();
    if(is_scaled) {
        setup_Upscale_Descriptors();
    }
    setup_Graphical_Descriptors();
    setup_RayGen_Descriptors();

    create_RayGen_Pipeline();
    create_Graphics_Pipeline();

    create_compute_pipelines();

    
    create_Sync_Objects();
}
void Renderer::delete_Images(vector<Image>* images){
    for (int i=0; i<MAX_FRAMES_IN_FLIGHT; i++){
         vkDestroyImageView(device,  (*images)[i].view, NULL);
        vmaDestroyImage(VMAllocator, (*images)[i].image, (*images)[i].alloc);
    }
}
void Renderer::cleanup(){
    delete_Images(&raytraced_frame);
    delete_Images(&denoised_frame);
    if(is_scaled) {
        delete_Images(&upscaled_frame);
        
        delete_Images(&matNorm_downscaled);
        delete_Images(&oldUv_downscaled);
        // delete_Images(&depth_downscaled);
        // delete_Images(&)
        // delete_Images(&gBuffer_downscaled);
    }
    delete_Images(&matNorm);
    delete_Images(&oldUv);
    // delete_Images(&depth);
    // delete_Images(&gBuffer);
    delete_Images(&depthForChecks);
    delete_Images(&world);
    delete_Images(&step_count);
    delete_Images(&origin_block_palette);
    delete_Images(&block_palette);
    delete_Images(&material_palette);

    for (int i=0; i<MAX_FRAMES_IN_FLIGHT; i++){

        vkDestroySampler(device, raytracedImageSamplers[i], NULL);
        vkDestroySampler(device,  denoisedImageSamplers[i], NULL);
        
        // vmaDestroyBuffer(VMAllocator, depthStaginBuffers[i], paletteCounterBufferAllocations[i]);
        vmaDestroyBuffer(VMAllocator, copyCounterBuffers[i], copyCounterBufferAllocations[i]);

            vmaUnmapMemory(VMAllocator,                           staging_world[i].alloc);
            vmaUnmapMemory(VMAllocator,                          RayGenUniformBuffers[i].alloc);
        vmaDestroyBuffer(VMAllocator,  staging_world[i].buffer, staging_world[i].alloc);
        vmaDestroyBuffer(VMAllocator, RayGenUniformBuffers[i].buffer, RayGenUniformBuffers[i].alloc);

        vkDestroyFramebuffer(device, rayGenFramebuffers[i], NULL);
    }

    vkDestroyDescriptorPool(device, descriptorPool, NULL);
    vkDestroyDescriptorSetLayout(device,     RayGenDescriptorSetLayout, NULL);
    vkDestroyDescriptorSetLayout(device,   blockifyDescriptorSetLayout, NULL);
    vkDestroyDescriptorSetLayout(device,       copyDescriptorSetLayout, NULL);
    vkDestroyDescriptorSetLayout(device,        mapDescriptorSetLayout, NULL);
    vkDestroyDescriptorSetLayout(device,         dfDescriptorSetLayout, NULL);
    // vkDestroyDescriptorSetLayout(device,         dfyDescriptorSetLayout, NULL);
    // vkDestroyDescriptorSetLayout(device,         dfzDescriptorSetLayout, NULL);
    vkDestroyDescriptorSetLayout(device,   raytraceDescriptorSetLayout, NULL);
    vkDestroyDescriptorSetLayout(device,    denoiseDescriptorSetLayout, NULL);
    vkDestroyDescriptorSetLayout(device,    upscaleDescriptorSetLayout, NULL);
    vkDestroyDescriptorSetLayout(device,  graphicalDescriptorSetLayout, NULL);
    for (int i=0; i < MAX_FRAMES_IN_FLIGHT; i++){
        vkDestroySemaphore(device,  imageAvailableSemaphores[i], NULL);
        vkDestroySemaphore(device,  renderFinishedSemaphores[i], NULL);
        // vkDestroySemaphore(device, blockifyFinishedSemaphores[i], NULL);
        // vkDestroySemaphore(device, copyFinishedSemaphores[i], NULL);
        // vkDestroySemaphore(device, mapFinishedSemaphores[i], NULL);
        vkDestroySemaphore(device, raytraceFinishedSemaphores[i], NULL);
        vkDestroySemaphore(device,  rayGenFinishedSemaphores[i], NULL);
        vkDestroyFence(device, graphicalInFlightFences[i], NULL);
        // vkDestroyFence(device,   blockifyInFlightFences[i], NULL);
        // vkDestroyFence(device,   copyInFlightFences[i], NULL);
        // vkDestroyFence(device,   mapInFlightFences[i], NULL);
        vkDestroyFence(device,   raytraceInFlightFences[i], NULL);
        vkDestroyFence(device,    rayGenInFlightFences[i], NULL);

    }
    vkDestroyCommandPool(device, commandPool, NULL);
    for (auto framebuffer : swapChainFramebuffers) {
        vkDestroyFramebuffer(device, framebuffer, NULL);
    }
    vkDestroyRenderPass(device, graphicalRenderPass, NULL);
    vkDestroyRenderPass(device, rayGenRenderPass, NULL);

    vkDestroyPipeline(device,   rayGenPipeline, NULL);
    vkDestroyPipeline(device, blockifyPipeline, NULL);
    vkDestroyPipeline(device,     copyPipeline, NULL);
    vkDestroyPipeline(device,      mapPipeline, NULL);
    vkDestroyPipeline(device,       dfxPipeline, NULL);
    vkDestroyPipeline(device,       dfyPipeline, NULL);
    vkDestroyPipeline(device,       dfzPipeline, NULL);
    vkDestroyPipeline(device, raytracePipeline, NULL);
    vkDestroyPipeline(device, denoisePipeline, NULL);
    vkDestroyPipeline(device, upscalePipeline, NULL);
    vkDestroyPipeline(device, graphicsPipeline, NULL);

    vkDestroyPipelineLayout(device,   rayGenPipelineLayout, NULL);
    vkDestroyPipelineLayout(device, blockifyLayout, NULL);
    vkDestroyPipelineLayout(device,     copyLayout, NULL);
    vkDestroyPipelineLayout(device,      mapLayout, NULL);
    vkDestroyPipelineLayout(device,       dfxLayout, NULL);
    vkDestroyPipelineLayout(device,       dfyLayout, NULL);
    vkDestroyPipelineLayout(device,       dfzLayout, NULL);
    vkDestroyPipelineLayout(device, raytraceLayout, NULL);
    vkDestroyPipelineLayout(device,  denoiseLayout, NULL);
    vkDestroyPipelineLayout(device,  upscaleLayout, NULL);
    vkDestroyPipelineLayout(device, graphicsLayout, NULL);

    vkDestroyShaderModule(device, rayGenVertShaderModule, NULL);
    vkDestroyShaderModule(device, rayGenFragShaderModule, NULL); 
    // vkDestroyShaderModule(device, blockifyShaderModule, NULL);
    // vkDestroyShaderModule(device,     copyShaderModule, NULL);
    // vkDestroyShaderModule(device,      mapShaderModule, NULL);
    // vkDestroyShaderModule(device,       dfShaderModule, NULL);
    // vkDestroyShaderModule(device, raytraceShaderModule, NULL); 
    vkDestroyShaderModule(device, graphicsVertShaderModule, NULL);
    vkDestroyShaderModule(device, graphicsFragShaderModule, NULL); 
    for (auto img : swapchain_images) {
        vkDestroyImageView(device, img.view, NULL);
    }
    vkDestroySwapchainKHR(device, swapchain, NULL);
    //do before destroyDevice
    vmaDestroyAllocator(VMAllocator);
    vkDestroyDevice(device, NULL);
    //"unpicking" physical device is unnessesary :)
    vkDestroySurfaceKHR(instance, surface, NULL);
#ifndef VKNDEBUG
    vkDestroyDebugUtilsMessengerEXT(instance, debugMessenger, NULL);
#endif
    vkDestroyInstance(instance, NULL);
    glfwDestroyWindow(window.pointer);
}

void Renderer::get_Instance_Extensions(){
    u32 glfwExtCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtCount);
    assert(glfwExtensions!=NULL);
    assert(glfwExtCount>0);
    for(u32 i=0; i < glfwExtCount; i++){
        instanceExtensions.push_back(glfwExtensions[i]);
        printl(glfwExtensions[i]);
    }

    //actual verify part
    u32 supportedExtensionCount = 0;
    VK_CHECK(vkEnumerateInstanceExtensionProperties(NULL, &supportedExtensionCount, NULL));
    vector<VkExtensionProperties> supportedInstanceExtensions(supportedExtensionCount);
    VK_CHECK(vkEnumerateInstanceExtensionProperties(NULL, &supportedExtensionCount, supportedInstanceExtensions.data()));    

    for(auto ext : instanceExtensions){
        bool supported = false;
        for(auto sup : supportedInstanceExtensions){
            if(strcmp(ext, sup.extensionName) == 0) supported = true;
        }
        if(not supported) {
            cout << KRED << ext << " not supported" << KEND;
            exit(1);
        }
    }
}

void Renderer::create_Allocator(){
    VmaAllocatorCreateInfo allocatorInfo = {};
    VmaVulkanFunctions vulkanFunctions;
        vulkanFunctions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
        vulkanFunctions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;
        vulkanFunctions.vkGetPhysicalDeviceProperties = vkGetPhysicalDeviceProperties;
        vulkanFunctions.vkGetPhysicalDeviceMemoryProperties = vkGetPhysicalDeviceMemoryProperties;
        vulkanFunctions.vkAllocateMemory = vkAllocateMemory;
        vulkanFunctions.vkFreeMemory = vkFreeMemory;
        vulkanFunctions.vkMapMemory = vkMapMemory;
        vulkanFunctions.vkUnmapMemory = vkUnmapMemory;
        vulkanFunctions.vkFlushMappedMemoryRanges = vkFlushMappedMemoryRanges;
        vulkanFunctions.vkInvalidateMappedMemoryRanges = vkInvalidateMappedMemoryRanges;
        vulkanFunctions.vkBindBufferMemory = vkBindBufferMemory;
        vulkanFunctions.vkBindImageMemory = vkBindImageMemory;
        vulkanFunctions.vkGetBufferMemoryRequirements = vkGetBufferMemoryRequirements;
        vulkanFunctions.vkGetImageMemoryRequirements = vkGetImageMemoryRequirements;
        vulkanFunctions.vkCreateBuffer = vkCreateBuffer;
        vulkanFunctions.vkDestroyBuffer = vkDestroyBuffer;
        vulkanFunctions.vkCreateImage = vkCreateImage;
        vulkanFunctions.vkDestroyImage = vkDestroyImage;
        vulkanFunctions.vkCmdCopyBuffer = vkCmdCopyBuffer;
        vulkanFunctions.vkGetBufferMemoryRequirements2KHR = vkGetBufferMemoryRequirements2KHR;
        vulkanFunctions.vkGetImageMemoryRequirements2KHR = vkGetImageMemoryRequirements2KHR;
        vulkanFunctions.vkBindBufferMemory2KHR = vkBindBufferMemory2KHR;
        vulkanFunctions.vkBindImageMemory2KHR = vkBindImageMemory2KHR;
        vulkanFunctions.vkGetPhysicalDeviceMemoryProperties2KHR = vkGetPhysicalDeviceMemoryProperties2KHR;
    allocatorInfo.pVulkanFunctions = (const VmaVulkanFunctions*) &vulkanFunctions;
    allocatorInfo.instance = instance;
    allocatorInfo.physicalDevice = physicalDevice;
    allocatorInfo.device = device;
    VK_CHECK(vmaCreateAllocator(&allocatorInfo, &VMAllocator));
}

void Renderer::create_Window(){
    int glfwRes = glfwInit();
    assert(glfwRes != 0);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    const GLFWvidmode* mode = glfwGetVideoMode(glfwGetPrimaryMonitor());
    assert(mode!=NULL);
    window.width  = (mode->width  / 2) * 2;
    window.height = (mode->height / 2) * 2;
    
    if(is_fullscreen){
        window.pointer = glfwCreateWindow(window.width, window.height, "renderer_vk", glfwGetPrimaryMonitor(), 0);
    } else {
        window.pointer = glfwCreateWindow(window.width, window.height, "renderer_vk", 0, 0);
    }
    assert(window.pointer != NULL);
}

void Renderer::create_Instance(){
    VkApplicationInfo app_info = {};
        app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        app_info.pApplicationName = "Lol vulkan raytracer";
        app_info.applicationVersion = VK_MAKE_VERSION(1,0,0);
        app_info.pEngineName = "my vk engine";
        app_info.engineVersion = VK_MAKE_VERSION(1,0,0);
        app_info.apiVersion = VK_API_VERSION_1_3;

    vector<VkValidationFeatureEnableEXT> enabledFeatures = {
        // VK_VALIDATION_FEATURE_ENABLE_BEST_PRACTICES_EXT, 
        VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT
    };

    VkValidationFeaturesEXT features = {};
        features.sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT;
        features.enabledValidationFeatureCount = enabledFeatures.size();
        features.pEnabledValidationFeatures    = enabledFeatures.data();

    VkInstanceCreateInfo createInfo = {};
    #ifndef VKNDEBUG 
        // createInfo.
        createInfo.pNext = &features; // according to spec

    #endif 
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &app_info;
        createInfo.enabledExtensionCount   = instanceExtensions.size();
        createInfo.ppEnabledExtensionNames = instanceExtensions.data();
        createInfo.enabledLayerCount   = instanceLayers.size();
        createInfo.ppEnabledLayerNames = instanceLayers.data();

        // createInfo.

    VK_CHECK(vkCreateInstance(&createInfo, NULL, &instance));
}

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData) {
    if(messageType == VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) printf(KCYN);
    if(messageType == VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) printf(KRED); 
    if(messageType == VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) printf(KBLU); 
    printf("validation layer: %s\n\n" KEND, pCallbackData->pMessage);
    return VK_FALSE;
}

void Renderer::setup_Debug_Messenger(){
    VkDebugUtilsMessengerEXT debugMessenger;
    
    VkDebugUtilsMessengerCreateInfoEXT debugUtilsCreateInfo = {};
        debugUtilsCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        debugUtilsCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        debugUtilsCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        debugUtilsCreateInfo.pfnUserCallback = debugCallback;

    VK_CHECK(vkCreateDebugUtilsMessengerEXT(instance, &debugUtilsCreateInfo, NULL, &debugMessenger));

    this->debugMessenger = debugMessenger;
}

void Renderer::create_Surface(){
    VK_CHECK(glfwCreateWindowSurface(instance, window.pointer, NULL, &surface));
}

QueueFamilyIndices Renderer::find_Queue_Families(VkPhysicalDevice device){
    QueueFamilyIndices indices;

    u32 queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, NULL);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    i32 i = 0;
    for (auto queueFamily : queueFamilies) {
        if ((queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) and (queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT)) {
            indices.graphicalAndCompute = i;
        }

        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);

        if (presentSupport) {
            indices.present = i;
        }

        if (indices.isComplete()) {
            break;
        }

        i++;
    }

    return indices;
}

bool Renderer::check_PhysicalDevice_Extension_Support(VkPhysicalDevice device){
    u32 extensionCount;
    vkEnumerateDeviceExtensionProperties(device, NULL, &extensionCount, NULL);
    vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, NULL, &extensionCount, availableExtensions.data());

    //for time complexity? FOR CODE COMPLEXITY HAHAHA
    //does not apply to current code

    vector<const char*> requiredExtensions = deviceExtensions;

    for (auto req : requiredExtensions) {
        bool supports = false;
        for (auto avail : availableExtensions) {
            if(strcmp(req, avail.extensionName) == 0) {
                supports = true;
            }
        }
        if (not supports) {
            // cout << KRED << req << "not supported\n" << KEND;
            return false;
        }
    }
    return true;
}

SwapChainSupportDetails Renderer::query_Swapchain_Support(VkPhysicalDevice device){
    SwapChainSupportDetails details;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

    u32 formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, NULL);
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());

    u32 presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, NULL);
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());

    return details;
}

bool Renderer::check_Format_Support(VkPhysicalDevice device, VkFormat format, VkFormatFeatureFlags features) {
    VkFormatProperties formatProps;
    vkGetPhysicalDeviceFormatProperties(device, format, &formatProps);
    // printf("%X\n", features);
    // printf("%X\n", formatProps.optimalTilingFeatures);
    // printf("%d\n", formatProps.optimalTilingFeatures & features);

    // if (formatProps.optimalTilingFeatures & features) {
    //     return true;
    // }
    // imageFormatProps.
    // return false;
    
    // // cout << res;
    // if (res == VK_SUCCESS) {
    //     return true;
    // }
    return formatProps.optimalTilingFeatures & features;
}

bool Renderer::is_PhysicalDevice_Suitable(VkPhysicalDevice device) {
    QueueFamilyIndices indices = find_Queue_Families(device);
    SwapChainSupportDetails swapChainSupport;
    bool extensionsSupported = check_PhysicalDevice_Extension_Support(device);
    bool imageFormatSupport = check_Format_Support(
        device,
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT
    );
    // bool srgbFormatSupport = check_Format_Support(
    //     device,
    //     VK_FORMAT_R8G8B8A8_SRGB,
    //     VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT
    // );

    if (extensionsSupported) {
        swapChainSupport  = query_Swapchain_Support(device);
    }

    return indices.isComplete() 
        && extensionsSupported 
        && swapChainSupport.is_Suitable() 
        && imageFormatSupport
    ;   
}

void Renderer::pick_Physical_Device(){
    u32 deviceCount;
    VK_CHECK(vkEnumeratePhysicalDevices(instance, &deviceCount, NULL));
    vector<VkPhysicalDevice> devices (deviceCount);
    VK_CHECK(vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data()));

    physicalDevice = VK_NULL_HANDLE;
    for (auto device : devices) {
        // cout << device << "\n";
        if (is_PhysicalDevice_Suitable(device)){
            physicalDevice = device;//could be moved out btw
            break;
        }
    }
    if(physicalDevice == VK_NULL_HANDLE){
        cout << KRED "no suitable physical device\n" KEND;
        exit(1);
    }
}

void Renderer::create_Logical_Device(){
    QueueFamilyIndices indices = find_Queue_Families(physicalDevice);

    vector<VkDeviceQueueCreateInfo> queueCreateInfos = {};
    set<u32> uniqueQueueFamilies = {indices.graphicalAndCompute.value(), indices.present.value()};

    float queuePriority = 1.0f; //random priority?
    for (u32 queueFamily : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    VkPhysicalDeviceFeatures deviceFeatures = {};
#ifndef VKNDEBUG
        // deviceFeatures.robustBufferAccess = true;
        // deviceFeatures.co
#endif
    VkDeviceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.queueCreateInfoCount = queueCreateInfos.size();
        createInfo.pQueueCreateInfos    = queueCreateInfos.data();
        createInfo.pEnabledFeatures = &deviceFeatures;
        createInfo.enabledExtensionCount   = deviceExtensions.size();
        createInfo.ppEnabledExtensionNames = deviceExtensions.data();
        createInfo.enabledLayerCount   = instanceLayers.size();
        createInfo.ppEnabledLayerNames = instanceLayers.data();

    VK_CHECK(vkCreateDevice(physicalDevice, &createInfo, NULL, &device));

    vkGetDeviceQueue(device, indices.graphicalAndCompute.value(), 0, &graphicsQueue);
    vkGetDeviceQueue(device, indices.graphicalAndCompute.value(), 0, &computeQueue);
    vkGetDeviceQueue(device, indices.present.value(), 0, &presentQueue);
}

VkSurfaceFormatKHR Renderer::choose_Swap_SurfaceFormat(vector<VkSurfaceFormatKHR> availableFormats) {
    for (auto format : availableFormats) {
        if (format.format == VK_FORMAT_R8G8B8A8_UNORM && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return format;
        }
    }
    cout << KYEL "Where is your RAYTRACED_IMAGE_FORMAT VK_COLOR_SPACE_SRGB_NONLINEAR_KHR?\n" KEND;
    return availableFormats[0];
}

VkPresentModeKHR Renderer::choose_Swap_PresentMode(vector<VkPresentModeKHR> availablePresentModes) {
    for (auto mode : availablePresentModes) {
        if (
            ((mode == VK_PRESENT_MODE_FIFO_KHR) && is_vsync) ||
            ((mode == VK_PRESENT_MODE_MAILBOX_KHR) && !is_vsync)
            ) {
            return mode;}
    }
    // cout << "fifo\n";
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D Renderer::choose_Swap_Extent(VkSurfaceCapabilitiesKHR capabilities) {
    if (capabilities.currentExtent.width != UINT_MAX) {
        return capabilities.currentExtent;
    } else {
        i32 width, height;
        glfwGetFramebufferSize(window.pointer, &width, &height);

        VkExtent2D actualExtent {(u32) width, (u32) height};

        actualExtent.width  = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

        return actualExtent;
    }
}

void Renderer::create_Swapchain(){
    SwapChainSupportDetails swapChainSupport = query_Swapchain_Support(physicalDevice);

    VkSurfaceFormatKHR surfaceFormat = choose_Swap_SurfaceFormat(swapChainSupport.formats);
    VkPresentModeKHR presentMode = choose_Swap_PresentMode(swapChainSupport.presentModes);
    VkExtent2D extent = choose_Swap_Extent(swapChainSupport.capabilities);

    u32 imageCount = swapChainSupport.capabilities.minImageCount + 1;
    if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
        imageCount = swapChainSupport.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = surface;
        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = surfaceFormat.format;
        createInfo.imageColorSpace = surfaceFormat.colorSpace;
        createInfo.imageExtent = extent;
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT;

    QueueFamilyIndices indices = find_Queue_Families(physicalDevice);
    u32 queueFamilyIndices[] = {indices.graphicalAndCompute.value(), indices.present.value()};

    if (indices.graphicalAndCompute != indices.present) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

        createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        createInfo.presentMode = presentMode;
        createInfo.clipped = VK_TRUE;
        createInfo.oldSwapchain = VK_NULL_HANDLE;

    VK_CHECK(vkCreateSwapchainKHR(device, &createInfo, NULL, &swapchain));

    vkGetSwapchainImagesKHR(device, swapchain, &imageCount, NULL);
    swapchain_images.resize(imageCount);
    vector<VkImage> imgs (imageCount);

    // cout << imageCount << "\n";
    vkGetSwapchainImagesKHR(device, swapchain, &imageCount, imgs.data());
    for(int i=0; i<imageCount; i++){
        swapchain_images[i].image = imgs[i];
    }


    swapChainImageFormat = surfaceFormat.format;
    swapChainExtent = extent;
    
    raytraceExtent.width  = swapChainExtent.width  / _ratio.x;
    raytraceExtent.height = swapChainExtent.height / _ratio.y;
}
void Renderer::recreate_Swapchain(){
    // vkDeviceWaitIdle(device);

    // for (auto framebuffer : swapChainFramebuffers) {
    //     vkDestroyFramebuffer(device, framebuffer, NULL);}
    // for (auto imageView : swapchain_images.views) {
    //     vkDestroyImageView(device, imageView, NULL);}
    // vkDestroySwapchainKHR(device, swapchain, NULL);

    // for (i32 i=0; i<MAX_FRAMES_IN_FLIGHT; i++){
    //     vkDestroyImageView(device,      raytraced_frame[i].view, NULL);
    //     vkDestroyImageView(device, gBuffer[i].view, NULL);
    //     vkDestroyImageView(device,   rayGenNormImageViews[i], NULL);
    //     vkFreeMemory(device,      raytracedvector<ImagesMemor>[i], NULL);
    //     vkFreeMemory(device, gBuffervector<ImagesMemor>[i], NULL);
    //     vkFreeMemory(device,   rayGenNormvector<ImagesMemor>[i], NULL);
    //     vkDestroyImage(device,      raytraced_frame[i].image, NULL);
    //     vkDestroyImage(device, gBuffer[i].image, NULL);
    //     vkDestroyImage(device,   rayGenNormvector<Image>[i], NULL);

    //     vkDestroyFramebuffer(device, rayGenFramebuffers[i], NULL);
    // }

    // create_Swapchain();
    // create_Image_Views();

    // create_Image_Storages(raytraced_frame.images, raytracedvector<ImagesMemor>, raytraced_frame.views,
    //     VK_IMAGE_TYPE_2D,
    //     VK_FORMAT_R8G8B8A8_UNORM,
    //     VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
    //     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    //     VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
    //     VK_ACCESS_SHADER_WRITE_BIT);
    // create_Image_Storages(gBuffer.images, gBuffervector<ImagesMemor>, gBuffer.views,
    //     VK_IMAGE_TYPE_2D,
    //     VK_FORMAT_R32G32B32A32_SFLOAT,
    //     VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
    //     VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    //     VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
    //     VK_ACCESS_SHADER_WRITE_BIT);
    // create_Image_Storages(rayGenNormvector<Image>, rayGenNormvector<ImagesMemor>, rayGenNormImageViews,
    //     VK_IMAGE_TYPE_2D,
    //     VK_FORMAT_R32G32B32A32_SFLOAT,
    //     VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
    //     VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    //     VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
    //     VK_ACCESS_SHADER_WRITE_BIT);

    // vector<vector<VkImageView>> swapViews = {swapchain_images.views};
    // create_N_Framebuffers(swapChainFramebuffers, swapViews, graphicalRenderPass, swapchain_images.images.size(), swapChainExtent.width, swapChainExtent.height);

    // vector<vector<VkImageView>> rayVeiws = {gBuffer.views, rayGenNormImageViews};
    // create_N_Framebuffers(rayGenFramebuffers, rayVeiws, rayGenRenderPass, MAX_FRAMES_IN_FLIGHT, swapChainExtent.width, swapChainExtent.height);

}

void Renderer::create_Swapchain_Image_Views(){
    // swapchain_images.views.resize(swapchain_images.images.size());

    for(i32 i=0; i<swapchain_images.size(); i++){
        VkImageViewCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image = swapchain_images[i].image;
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = swapChainImageFormat;
        createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;

        VK_CHECK(vkCreateImageView(device, &createInfo, NULL, &swapchain_images[i].view));
    }
}

vector<char> read_Shader(const char* filename) {
    ifstream file(filename, ios::ate | ios::binary);
    assert(file.is_open());

    u32 fileSize = file.tellg();
    vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);

    file.close();
    return buffer;
}

VkShaderModule Renderer::create_Shader_Module(vector<char>* code) {
    VkShaderModule shaderModule;
    VkShaderModuleCreateInfo createInfo = {};

    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = (*code).size();
    createInfo.pCode = reinterpret_cast<const u32*>((*code).data());

    VK_CHECK(vkCreateShaderModule(device, &createInfo, NULL, &shaderModule));

    return shaderModule;
}

void Renderer::create_RenderPass_Graphical(){
    VkAttachmentDescription colorAttachment = {};
        colorAttachment.format = swapChainImageFormat;
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachment.loadOp  = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE; 
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentRef = {};
        colorAttachmentRef.attachment = 0;
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass = {};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorAttachmentRef;

    VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    
    VkRenderPassCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        createInfo.attachmentCount = 1;
        createInfo.pAttachments = &colorAttachment;
        createInfo.subpassCount = 1;
        createInfo.pSubpasses = &subpass;
        createInfo.dependencyCount = 1;
        createInfo.pDependencies = &dependency;
    
    VK_CHECK(vkCreateRenderPass(device, &createInfo, NULL, &graphicalRenderPass));

}
void Renderer::create_RenderPass_RayGen(){
    VkAttachmentDescription caMatNorm = {};
        caMatNorm.format = VK_FORMAT_R8G8B8A8_SNORM;
        caMatNorm.samples = VK_SAMPLE_COUNT_1_BIT;
        caMatNorm.loadOp  = VK_ATTACHMENT_LOAD_OP_CLEAR;
        caMatNorm.storeOp = VK_ATTACHMENT_STORE_OP_STORE; 
        caMatNorm.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        caMatNorm.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        caMatNorm.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        caMatNorm.finalLayout = VK_IMAGE_LAYOUT_GENERAL;
    VkAttachmentDescription caOldUv = {};
        caOldUv.format = OLD_UV_FORMAT;
        caOldUv.samples = VK_SAMPLE_COUNT_1_BIT;
        caOldUv.loadOp  = VK_ATTACHMENT_LOAD_OP_CLEAR;
        caOldUv.storeOp = VK_ATTACHMENT_STORE_OP_STORE; 
        caOldUv.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        caOldUv.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        caOldUv.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        caOldUv.finalLayout = VK_IMAGE_LAYOUT_GENERAL; 
    // VkAttachmentDescription caDepth = {};
    //     caDepth.format = VK_FORMAT_R32_SFLOAT;
    //     caDepth.samples = VK_SAMPLE_COUNT_1_BIT;
    //     caDepth.loadOp  = VK_ATTACHMENT_LOAD_OP_CLEAR;
    //     caDepth.storeOp = VK_ATTACHMENT_STORE_OP_STORE; 
    //     caDepth.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    //     caDepth.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    //     caDepth.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    //     caDepth.finalLayout = VK_IMAGE_LAYOUT_GENERAL;
    VkAttachmentDescription depthAttachment = {}; 
        depthAttachment.format = VK_FORMAT_D32_SFLOAT;
        depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        depthAttachment.loadOp  = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE; 
        depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depthAttachment.finalLayout = DEPTH_LAYOUT;
        // depthAttachment.
    VkAttachmentReference MatNormRef = {};
        MatNormRef.attachment = 0;
        MatNormRef.layout = VK_IMAGE_LAYOUT_GENERAL;
    VkAttachmentReference oldUvRef = {};
        oldUvRef.attachment = 1;
        oldUvRef.layout = VK_IMAGE_LAYOUT_GENERAL;
    // VkAttachmentReference depthRef = {};
    //     depthRef.attachment = 2;
    //     depthRef.layout = VK_IMAGE_LAYOUT_GENERAL;
    VkAttachmentReference depthAttachmentRef = {};
        depthAttachmentRef.attachment = 2;
        depthAttachmentRef.layout = VK_IMAGE_LAYOUT_GENERAL;

    vector<VkAttachmentDescription> attachments = {caMatNorm, caOldUv, /*caDepth,*/ depthAttachment};

    vector<VkAttachmentReference>   colorAttachmentRefs = {MatNormRef, oldUvRef, /*depthRef*/};

    VkSubpassDescription subpass = {};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.pDepthStencilAttachment = &depthAttachmentRef;
        subpass.colorAttachmentCount = colorAttachmentRefs.size();
        subpass.pColorAttachments = colorAttachmentRefs.data();

    VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    
    VkRenderPassCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        createInfo.attachmentCount = attachments.size();
        createInfo.pAttachments    = attachments.data();
        createInfo.subpassCount = 1;
        createInfo.pSubpasses = &subpass;
        createInfo.dependencyCount = 1;
        createInfo.pDependencies = &dependency;
    
    VK_CHECK(vkCreateRenderPass(device, &createInfo, NULL, &rayGenRenderPass));

}

void Renderer::create_Graphics_Pipeline(){
    auto vertShaderCode = read_Shader("shaders/compiled/vert.spv");
    auto fragShaderCode = read_Shader("shaders/compiled/frag.spv");

    graphicsVertShaderModule = create_Shader_Module(&vertShaderCode);
    graphicsFragShaderModule = create_Shader_Module(&fragShaderCode);

    VkPipelineShaderStageCreateInfo vertShaderStageInfo = {};
        vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertShaderStageInfo.module = graphicsVertShaderModule;
        vertShaderStageInfo.pName = "main";
    VkPipelineShaderStageCreateInfo fragShaderStageInfo = {};
        fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragShaderStageInfo.module = graphicsFragShaderModule;
        fragShaderStageInfo.pName = "main";

    //common
    VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

    VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = 0;
        vertexInputInfo.pVertexBindingDescriptions = NULL; // Optional
        vertexInputInfo.vertexAttributeDescriptionCount = 0;
        vertexInputInfo.pVertexAttributeDescriptions = NULL; // Optional
    
    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width  = (float) swapChainExtent.width;
        viewport.height = (float) swapChainExtent.height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = swapChainExtent;  

    vector<VkDynamicState> dynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };
    VkPipelineDynamicStateCreateInfo dynamicState = {};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = dynamicStates.size();
        dynamicState.pDynamicStates = dynamicStates.data();

    VkPipelineViewportStateCreateInfo viewportState = {};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.scissorCount = 1;
    
    VkPipelineRasterizationStateCreateInfo rasterizer = {};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.depthClampEnable = VK_FALSE;
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth = 1.0f;
        rasterizer.depthBiasEnable = VK_FALSE;
        rasterizer.depthBiasConstantFactor = 0.0f; // Optional
        rasterizer.depthBiasClamp = 0.0f; // Optional
        rasterizer.depthBiasSlopeFactor = 0.0f; // Optional

    VkPipelineMultisampleStateCreateInfo multisampling = {};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable = VK_FALSE;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        multisampling.minSampleShading = 1.0f; // Optional
        multisampling.pSampleMask = NULL; // Optional
        multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
        multisampling.alphaToOneEnable = VK_FALSE; // Optional
    
    VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_FALSE;
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
        colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD; // Optional
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
        colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD; // Optional

    VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.logicOpEnable = VK_FALSE;
        colorBlending.logicOp = VK_LOGIC_OP_COPY; // Optional
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachment;
        colorBlending.blendConstants[0] = 0.0f; // Optional
        colorBlending.blendConstants[1] = 0.0f; // Optional
        colorBlending.blendConstants[2] = 0.0f; // Optional
        colorBlending.blendConstants[3] = 0.0f; // Optional

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &graphicalDescriptorSetLayout;
        pipelineLayoutInfo.pushConstantRangeCount = 0; // Optional
        pipelineLayoutInfo.pPushConstantRanges = NULL; // Optional

    VK_CHECK(vkCreatePipelineLayout(device, &pipelineLayoutInfo, NULL, &graphicsLayout));

    VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = shaderStages;
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pDepthStencilState = NULL; // Optional
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDynamicState = &dynamicState;
        
        pipelineInfo.layout = graphicsLayout;
        
        pipelineInfo.renderPass = graphicalRenderPass;
        pipelineInfo.subpass = 0;
        
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
        pipelineInfo.basePipelineIndex = -1; // Optional

        VK_CHECK(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, NULL, &graphicsPipeline));
}

void Renderer::create_RayGen_Pipeline(){
    auto vertShaderCode = read_Shader("shaders/compiled/rayGenVert.spv");
    auto fragShaderCode = read_Shader("shaders/compiled/rayGenFrag.spv");

    rayGenVertShaderModule = create_Shader_Module(&vertShaderCode);
    rayGenFragShaderModule = create_Shader_Module(&fragShaderCode);

    VkPipelineShaderStageCreateInfo vertShaderStageInfo = {};
        vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertShaderStageInfo.module = rayGenVertShaderModule;
        vertShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo fragShaderStageInfo = {};
        fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragShaderStageInfo.module = rayGenFragShaderModule;
        fragShaderStageInfo.pName = "main";

    VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(Vertex);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    vector<VkVertexInputAttributeDescription> attributeDescriptions(3);
        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[0].offset = offsetof(Vertex, pos);
        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[1].offset = offsetof(Vertex, norm);
        attributeDescriptions[2].binding = 0;
        attributeDescriptions[2].location = 2;
        attributeDescriptions[2].format = VK_FORMAT_R8_UINT;
        attributeDescriptions[2].offset = offsetof(Vertex, matID);

    VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = 1;
        vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
        vertexInputInfo.vertexAttributeDescriptionCount = attributeDescriptions.size();
        vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();
    
    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width  = (float) swapChainExtent.width;
        viewport.height = (float) swapChainExtent.height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = swapChainExtent;  

    vector<VkDynamicState> dynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };
    VkPipelineDynamicStateCreateInfo dynamicState = {};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = dynamicStates.size();
        dynamicState.pDynamicStates = dynamicStates.data();

    VkPipelineViewportStateCreateInfo viewportState = {};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.scissorCount = 1;
    
    VkPipelineRasterizationStateCreateInfo rasterizer = {};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.depthClampEnable = VK_FALSE;
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth = 1.0f;
        rasterizer.depthBiasEnable = VK_FALSE;
        rasterizer.depthBiasConstantFactor = 0.0f; // Optional
        rasterizer.depthBiasClamp = 0.0f; // Optional
        rasterizer.depthBiasSlopeFactor = 0.0f; // Optional

    VkPipelineMultisampleStateCreateInfo multisampling = {};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable = VK_FALSE;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        multisampling.minSampleShading = 1.0f; // Optional
        multisampling.pSampleMask = NULL; // Optional
        multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
        multisampling.alphaToOneEnable = VK_FALSE; // Optional
    
    VkPipelineColorBlendAttachmentState blendMatNorm = {};
            blendMatNorm.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
            blendMatNorm.blendEnable = VK_FALSE;
            blendMatNorm.srcColorBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
            blendMatNorm.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
            blendMatNorm.colorBlendOp = VK_BLEND_OP_ADD; // Optional
            blendMatNorm.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
            blendMatNorm.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
            blendMatNorm.alphaBlendOp = VK_BLEND_OP_ADD; // Optional
    VkPipelineColorBlendAttachmentState blendOldUv = {};
        blendOldUv.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        blendOldUv.blendEnable = VK_FALSE;
        blendOldUv.srcColorBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
        blendOldUv.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
        blendOldUv.colorBlendOp = VK_BLEND_OP_ADD; // Optional
        blendOldUv.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
        blendOldUv.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
        blendOldUv.alphaBlendOp = VK_BLEND_OP_ADD; // Optional
    // VkPipelineColorBlendAttachmentState blendDepth = {};
    //     blendDepth.colorWriteMask = VK_COLOR_COMPONENT_R_BIT;
    //     blendDepth.blendEnable = VK_FALSE;
    //     blendDepth.srcColorBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
    //     blendDepth.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
    //     blendDepth.colorBlendOp = VK_BLEND_OP_ADD; // Optional
    //     blendDepth.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
    //     blendDepth.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
    //     blendDepth.alphaBlendOp = VK_BLEND_OP_ADD; // Optional

    vector<VkPipelineColorBlendAttachmentState> colorBlendAttachments = {blendMatNorm, blendOldUv, /*blendDepth*/};
// printl(colorBlendAttachments.size());
    VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.logicOpEnable = VK_FALSE;
        colorBlending.logicOp = VK_LOGIC_OP_COPY; // Optional
        colorBlending.attachmentCount = colorBlendAttachments.size();
        colorBlending.pAttachments = colorBlendAttachments.data();
        colorBlending.blendConstants[0] = 0.0f; // Optional
        colorBlending.blendConstants[1] = 0.0f; // Optional
        colorBlending.blendConstants[2] = 0.0f; // Optional
        colorBlending.blendConstants[3] = 0.0f; // Optional

    VkPushConstantRange pushRange = {};
        // pushRange.size = 256;
        pushRange.size = sizeof(mat4)*2; //trans
        pushRange.offset = 0;
        pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        // pipelineLayoutInfo.setLayoutCount = 0;
        // pipelineLayoutInfo.pSetLayouts = NULL;
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &RayGenDescriptorSetLayout;
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges = &pushRange;

    VK_CHECK(vkCreatePipelineLayout(device, &pipelineLayoutInfo, NULL, &rayGenPipelineLayout));

    VkPipelineDepthStencilStateCreateInfo depthStencil = {VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};    
        depthStencil.depthTestEnable = VK_TRUE;
        depthStencil.depthWriteEnable = VK_TRUE;
        depthStencil.depthCompareOp = VK_COMPARE_OP_GREATER_OR_EQUAL;
        depthStencil.depthBoundsTestEnable = VK_FALSE;
        depthStencil.minDepthBounds = 0.0f; // Optional
        depthStencil.maxDepthBounds = 1.0f; // Optional
        depthStencil.stencilTestEnable = VK_FALSE;
        depthStencil.front = {}; // Optional
        depthStencil.back = {}; // Optional
    VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};
    VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = shaderStages;
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pDepthStencilState = &depthStencil; // Optional
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDynamicState = &dynamicState;
        
        pipelineInfo.layout = rayGenPipelineLayout;
        
        pipelineInfo.renderPass = rayGenRenderPass;
        pipelineInfo.subpass = 0; //first and only subpass in renderPass will use the pipeline
        
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
        pipelineInfo.basePipelineIndex = -1; // Optional

        VK_CHECK(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, NULL, &rayGenPipeline));
}

void Renderer::create_N_Framebuffers(vector<VkFramebuffer>* framebuffers, vector<vector<VkImageView>>* views, VkRenderPass renderPass, u32 N, u32 Width, u32 Height){
    (*framebuffers).resize(N);

    for (u32 i = 0; i < N; i++) {
        vector<VkImageView> attachments = {};
        for (auto viewVec : *views) {
            attachments.push_back(viewVec[i]);
        }

        VkFramebufferCreateInfo framebufferInfo = {};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = renderPass;
            framebufferInfo.attachmentCount = attachments.size();
            framebufferInfo.pAttachments    = attachments.data();
            framebufferInfo.width  = Width;
            framebufferInfo.height = Height;
            framebufferInfo.layers = 1;

        VK_CHECK(vkCreateFramebuffer(device, &framebufferInfo, NULL, &(*framebuffers)[i]));
    }
}

void Renderer::create_Command_Pool(){
    //TODO: could be saved after physical device choosen
    //btw TODO is ignored
    QueueFamilyIndices queueFamilyIndices = find_Queue_Families(physicalDevice);

    VkCommandPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = queueFamilyIndices.graphicalAndCompute.value();

    VK_CHECK(vkCreateCommandPool(device, &poolInfo, NULL, &commandPool));
}

void Renderer::create_Command_Buffers(vector<VkCommandBuffer>* commandBuffers, u32 size){
    (*commandBuffers).resize(size);

    VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = commandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = (*commandBuffers).size();
    
    VK_CHECK(vkAllocateCommandBuffers(device, &allocInfo, (*commandBuffers).data())); 
}

// void Renderer::record_Command_Buffer_Graphical(VkCommandBuffer commandBuffer, u32 imageIndex){
//     VkCommandBufferBeginInfo beginInfo = {};
//         beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
//         beginInfo.flags = 0; // Optional
//         beginInfo.pInheritanceInfo = NULL; // Optional

//     VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));

//     transition_Image_Layout_Cmdb(commandBuffer, raytraced_frame[currentFrame].image, RAYTRACED_IMAGE_FORMAT, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
//         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
//         0, VK_ACCESS_SHADER_READ_BIT
//     );
//     // transition_Image_Layout_Cmdb(commandBuffer, gBuffer[currentFrame].image, VK_FORMAT_UNDEFINED, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
//     //     VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
//     //     0, VK_ACCESS_SHADER_READ_BIT
//     // );
//     // VkCopyImageToImageInfoEXT info = {};
//     // info.sType = VK_STRUCTURE_TYPE_COPY_IMAGE_TO_IMAGE_INFO_EXT;
//     // info.
//     // vkCopyImageToImageEXT(device, info)

//     VkClearValue clearColor = {{{0.111f, 0.444f, 0.666f, 1.0f}}};
//     VkRenderPassBeginInfo renderPassInfo = {};
//         renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
//         renderPassInfo.renderPass = graphicalRenderPass;
//         renderPassInfo.framebuffer = swapChainFramebuffers[imageIndex];
//         renderPassInfo.renderArea.offset = {0, 0};
//         renderPassInfo.renderArea.extent = swapChainExtent;
//         renderPassInfo.clearValueCount = 1;
//         renderPassInfo.pClearValues = &clearColor;
    
//     vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
//     vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsLayout, 0, 1, &graphicalDescriptorSets[currentFrame], 0, NULL);
//     vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

//     VkViewport viewport = {};
//         viewport.x = 0.0f;
//         viewport.y = 0.0f;
//         viewport.width  = (float)(swapChainExtent.width );
//         viewport.height = (float)(swapChainExtent.height);
//         viewport.minDepth = 0.0f;
//         viewport.maxDepth = 1.0f;
//     vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

//     VkRect2D scissor = {};
//         scissor.offset = {0, 0};
//         scissor.extent = swapChainExtent;
//     vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

//     vkCmdDraw(commandBuffer, 3, 1, 0, 0);
//     // vkCmdDraw(commandBuffer, 3, 1, 0, 0);

//     vkCmdEndRenderPass(commandBuffer);

//     VK_CHECK(vkEndCommandBuffer(commandBuffer));
// }

// void Renderer::record_Command_Buffer_Compute(VkCommandBuffer commandBuffer){
//     VkCommandBufferBeginInfo beginInfo = {};
//         beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
//         beginInfo.flags = 0; // Optional
//         beginInfo.pInheritanceInfo = NULL; // Optional

//     VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));

//     // raygen;
//     vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, raytracePipeline);


//     transition_Image_Layout_Cmdb(commandBuffer, raytraced_frame[currentFrame].image, RAYTRACED_IMAGE_FORMAT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
//         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
//         0, VK_ACCESS_SHADER_WRITE_BIT
//     );

//     vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, raytracePipeline);

//     //move to argument  
//     vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, raytraceLayout, 0, 1, &raytraceDescriptorSets[currentFrame], 0, 0);

//     // vkCmdDispatch(commandBuffer, 1920/8, 1080/4, 1);
//     // float data[32] = {111, 222};
//     // for(i32 i=0; i<100; i++){
//     //     vkCmdPushConstants(commandBuffer, raytraceLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(float)*2 , &data);
//     //     vkCmdDispatch(commandBuffer, 5, 5, 45);
//     //     data[0]++;
//     //     data[1]++;
//     // }
//     vkCmdDispatch(commandBuffer, raytraceExtent.width/8, raytraceExtent.height/4, 1);

//     VK_CHECK(vkEndCommandBuffer(commandBuffer));
// }

void Renderer::create_Sync_Objects(){
    imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    // blockifyFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    // copyFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    // mapFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    raytraceFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    rayGenFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);

       rayGenInFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
    graphicalInFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
    // blockifyInFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
    // copyInFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
    // mapInFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
      raytraceInFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

    VkSemaphoreCreateInfo semaphoreInfo = {};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (i32 i=0; i < MAX_FRAMES_IN_FLIGHT; i++){
        VK_CHECK(vkCreateSemaphore(device, &semaphoreInfo, NULL,  &imageAvailableSemaphores[i]));
        VK_CHECK(vkCreateSemaphore(device, &semaphoreInfo, NULL,  &renderFinishedSemaphores[i]));
        // VK_CHECK(vkCreateSemaphore(device, &semaphoreInfo, NULL, &blockifyFinishedSemaphores[i]));
        // VK_CHECK(vkCreateSemaphore(device, &semaphoreInfo, NULL, &copyFinishedSemaphores[i]));
        // VK_CHECK(vkCreateSemaphore(device, &semaphoreInfo, NULL, &mapFinishedSemaphores[i]));
        VK_CHECK(vkCreateSemaphore(device, &semaphoreInfo, NULL, &raytraceFinishedSemaphores[i]));
        VK_CHECK(vkCreateSemaphore(device, &semaphoreInfo, NULL,  &rayGenFinishedSemaphores[i]));
        VK_CHECK(vkCreateFence(device, &fenceInfo, NULL, &graphicalInFlightFences[i]));
        // VK_CHECK(vkCreateFence(device, &fenceInfo, NULL,   &blockifyInFlightFences[i]));
        // VK_CHECK(vkCreateFence(device, &fenceInfo, NULL,   &copyInFlightFences[i]));
        // VK_CHECK(vkCreateFence(device, &fenceInfo, NULL,   &mapInFlightFences[i]));
        VK_CHECK(vkCreateFence(device, &fenceInfo, NULL,   &raytraceInFlightFences[i]));
        VK_CHECK(vkCreateFence(device, &fenceInfo, NULL,    &rayGenInFlightFences[i]));
    }
}

void Renderer::start_Frame(){
    // update transformation matrix for raygen:
    // const vec3 cameraRayDir = normalize(vec3(0.1, 1.0, -0.5));
    // const vec3 cameraPos = vec3(60, 0, 50);

    // camera_pos = cameraPos;
    // camera_dir = cameraRayDir;

// vec3 cameraRayDirPlane = normalize(vec3(cameraRayDir.x, cameraRayDir.y, 0));
// vec3 horizline = normalize(cross(cameraRayDirPlane, vec3(0,0,1)));
// vec3 vertiline = normalize(cross(cameraRayDir, horizline));

    vec3 up = glm::vec3(0.0f, 0.0f, 1.0f); // Up vector
    // vec3 up1 = up.xyz;
    // vec3 up1 = up.xxx();

    mat4 view = glm::lookAt(camera_pos, camera_pos+camera_dir, up);
    mat4 projection = glm::ortho(-1920.f/42.f, 1920.f/42.f, 1080.f/42.f, -1080.f/42.f, -0.f, 200.f); // => *100.0 decoding
    mat4     worldToScreen = projection * view;
    mat4 old_worldToScreen = projection * view;

    struct unicopy {mat4 trans; mat4 old_trans;} unicopy = {};
    unicopy.trans = worldToScreen;
    unicopy.old_trans = old_trans;
    
    memcpy(RayGenUniformMapped[currentFrame], &unicopy, sizeof(unicopy));

    old_trans = old_worldToScreen;
}

void Renderer::startRaygen(){
    vkWaitForFences(device, 1, &rayGenInFlightFences[currentFrame], VK_TRUE, UINT32_MAX);
    vkResetFences  (device, 1, &rayGenInFlightFences[currentFrame]);

    VkCommandBuffer &commandBuffer = rayGenCommandBuffers[currentFrame];
    vkResetCommandBuffer(commandBuffer, 0);
    VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = 0; // Optional
        beginInfo.pInheritanceInfo = NULL; // Optional
    VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));
        vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            0,
            0, NULL,
            0, NULL,
            0, NULL
        );

    vector<VkClearValue> clearColors = {
        {}, 
        // {}, 
        {}, 
        {}
    };
    VkRenderPassBeginInfo renderPassInfo = {};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = rayGenRenderPass;
        renderPassInfo.framebuffer = rayGenFramebuffers[currentFrame];
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = swapChainExtent;
        renderPassInfo.clearValueCount = clearColors.size();
        renderPassInfo.pClearValues    = clearColors.data();
    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, rayGenPipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, rayGenPipelineLayout, 0, 1, &RayGenDescriptorSets[currentFrame], 0, 0);

    VkViewport viewport = {};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width  = (float)(swapChainExtent.width );
        viewport.height = (float)(swapChainExtent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor = {};
        scissor.offset = {0, 0};
        scissor.extent = swapChainExtent;
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
}
void Renderer::RaygenMesh(Mesh *mesh){
    
    VkCommandBuffer &commandBuffer = rayGenCommandBuffers[currentFrame];
        VkBuffer vertexBuffers[] = {(*mesh).vertexes[currentFrame].buffer};
        VkDeviceSize offsets[] = {0};
    

    //TODO:
    vkCmdPushConstants(commandBuffer, rayGenPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(mat4), &(*mesh).transform);
    vkCmdPushConstants(commandBuffer, rayGenPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, sizeof(mat4), sizeof(mat4), &(*mesh).old_transform);
    // vkCmdPushConstants(commandBuffer, rayGenPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, sizeof(mat4), sizeof(mat4), &worldToScreen);
    (*mesh).old_transform = (*mesh).transform;

    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(commandBuffer, (*mesh).indexes[currentFrame].buffer, 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(commandBuffer, (*mesh).icount, 1, 0, 0, 0);
}
void Renderer::endRaygen(){
    VkCommandBuffer &commandBuffer = rayGenCommandBuffers[currentFrame];

    // VkOffset3D srcOffsets[2] = {};
    // VkOffset3D dstOffsets[2] = {};

    VkImageBlit blit = {};
        blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.srcSubresource.layerCount = 1;
        blit.dstSubresource.layerCount = 1;
        blit.srcOffsets[0].x = 0;
        blit.srcOffsets[0].y = 0;
        blit.srcOffsets[0].z = 0;
        blit.srcOffsets[1].x = swapChainExtent.width;
        blit.srcOffsets[1].y = swapChainExtent.height;
        blit.srcOffsets[1].z = 1;

        blit.dstOffsets[0].x = 0;
        blit.dstOffsets[0].y = 0;
        blit.dstOffsets[0].z = 0;
        blit.dstOffsets[1].x = swapChainExtent.width;
        blit.dstOffsets[1].y = swapChainExtent.height;
        blit.dstOffsets[1].z = 1;
    VkFilter blit_filter = VK_FILTER_NEAREST;

    //LOL drawing to depth twice is more efficient than drawing once and copying
        // blit_filter.
    // vkCmdBlitImage(commandBuffer, depthForChecks[currentFrame].image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, depth[currentFrame].image, VK_IMAGE_LAYOUT_GENERAL, 1, &blit, blit_filter);
    // transition_Image_Layout_Cmdb(commandBuffer, depthForChecks[currentFrame].image, VK_FORMAT_D32_SFLOAT, DEPTH_LAYOUT, VK_IMAGE_LAYOUT_GENERAL,
    //     VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
    //     VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT);
    
    // VkBufferImageCopy detpth2bufferCopyRegion = {};
    //     detpth2bufferCopyRegion.bufferOffset = 0;

    //     // detpth2bufferCopyRegion.bufferRowLength = swapChainExtent.width;
    //     // detpth2bufferCopyRegion.bufferImageHeight = swapChainExtent.height;
    //     detpth2bufferCopyRegion.bufferRowLength   = {};
    //     detpth2bufferCopyRegion.bufferImageHeight = {};

    //     detpth2bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    //     detpth2bufferCopyRegion.imageSubresource.baseArrayLayer = 0;
    //     detpth2bufferCopyRegion.imageSubresource.layerCount = 1;
    //     detpth2bufferCopyRegion.imageSubresource.mipLevel = 0;

    //     detpth2bufferCopyRegion.imageOffset = {0,0,0};
    //     detpth2bufferCopyRegion.imageExtent = {swapChainExtent.width, swapChainExtent.height, 1};
    // vkCmdCopyImageToBuffer(commandBuffer, depthForChecks[currentFrame].image, DEPTH_LAYOUT, depthStaging[currentFrame].buffer, 1, &detpth2bufferCopyRegion);
    //todo sync
            // detpth2bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    // vkCmdCopyBufferToImage(commandBuffer, depthStaging[currentFrame].buffer, depth[currentFrame].image, VK_IMAGE_LAYOUT_GENERAL, 1, &detpth2bufferCopyRegion);
    if (is_scaled) {
            blit.dstOffsets[1].x = raytraceExtent.width;
            blit.dstOffsets[1].y = raytraceExtent.height;
        // vkCmdBlitImage(commandBuffer, depth[currentFrame].image, VK_IMAGE_LAYOUT_GENERAL, depth_downscaled[currentFrame].image, VK_IMAGE_LAYOUT_GENERAL, 1, &blit, blit_filter);
            // blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            // blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        vkCmdBlitImage(commandBuffer, matNorm[currentFrame].image, VK_IMAGE_LAYOUT_GENERAL, matNorm_downscaled[currentFrame].image, VK_IMAGE_LAYOUT_GENERAL, 1, &blit, VK_FILTER_NEAREST);
        vkCmdBlitImage(commandBuffer, oldUv[currentFrame].image, VK_IMAGE_LAYOUT_GENERAL, oldUv_downscaled[currentFrame].image, VK_IMAGE_LAYOUT_GENERAL, 1, &blit, VK_FILTER_LINEAR);
    }
    //  barrier{};
    //     barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER; 
    //     barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    //     barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    //     barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    //     barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    //     barrier.image = depth[currentFrame].image;
    //     barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    //     barrier.subresourceRange.baseMipLevel = 0;
    //     barrier.subresourceRange.levelCount = 1;
    //     barrier.subresourceRange.baseArrayLayer = 0;
    //     barrier.subresourceRange.layerCount = 1;
    //     barrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT;
    //     barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT  | VK_ACCESS_MEMORY_WRITE_BIT;    
    // vkCmdPipelineBarrier(
    //         commandBuffer,
    //         VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
    //         0,
    //         0, NULL,
    //         0, NULL,
    //         1, &barrier
    //     ); 
    VkImageMemoryBarrier barrier = {};
    if (is_scaled) {
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER; 
        barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = matNorm_downscaled[currentFrame].image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT  | VK_ACCESS_MEMORY_WRITE_BIT;    
        vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            0,
            0, NULL,
            0, NULL,
            1, &barrier
        );
        barrier.image = oldUv_downscaled[currentFrame].image;
        vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            0,
            0, NULL,
            0, NULL,
            1, &barrier
        );
    } 
    // barrier ={};
    //     barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER; 
    //     barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    //     barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    //     barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    //     barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    //     barrier.image = matNorm[currentFrame].image;
    //     barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    //     barrier.subresourceRange.baseMipLevel = 0;
    //     barrier.subresourceRange.levelCount = 1;
    //     barrier.subresourceRange.baseArrayLayer = 0;
    //     barrier.subresourceRange.layerCount = 1;
    //     barrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT;
    //     barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT  | VK_ACCESS_MEMORY_WRITE_BIT;    
    // vkCmdPipelineBarrier(
    //         commandBuffer,
    //         VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
    //         0,
    //         0, NULL,
    //         0, NULL,
    //         1, &barrier
    //     ); 
    // barrier.image = depth_downscaled[currentFrame].image;
    // vkCmdPipelineBarrier(
    //         commandBuffer,
    //         VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
    //         0,
    //         0, NULL,
    //         0, NULL,
    //         1, &barrier
    //     ); 
    // barrier.image = matNorm_downscaled[currentFrame].image;
    // vkCmdPipelineBarrier(
    //         commandBuffer,
    //         VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
    //         0,
    //         0, NULL,
    //         0, NULL,
    //         1, &barrier
    //     ); 
    vkCmdEndRenderPass(commandBuffer);
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER; 
        barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = matNorm[currentFrame].image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT  | VK_ACCESS_MEMORY_WRITE_BIT;    
        vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            0,
            0, NULL,
            0, NULL,
            1, &barrier
        );
        barrier.image = oldUv[currentFrame].image;
        vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            0,
            0, NULL,
            0, NULL,
            1, &barrier
        );
        barrier.image = depthForChecks[currentFrame].image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            0,
            0, NULL,
            0, NULL,
            1, &barrier
        );
    vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            0,
            0, NULL,
            0, NULL,
            0, NULL
        ); 

    VK_CHECK(vkEndCommandBuffer(commandBuffer));
    VkSubmitInfo submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &rayGenCommandBuffers[currentFrame];
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = &rayGenFinishedSemaphores[currentFrame];
    VK_CHECK(vkQueueSubmit(graphicsQueue, 1, &submitInfo, rayGenInFlightFences[currentFrame]));
}
void Renderer::startCompute(){
    VkCommandBuffer &commandBuffer = computeCommandBuffers[currentFrame];
    vkWaitForFences(device, 1, &raytraceInFlightFences[currentFrame], VK_TRUE, UINT32_MAX);
    vkResetFences  (device, 1, &raytraceInFlightFences[currentFrame]);

    
    vkResetCommandBuffer(computeCommandBuffers[currentFrame], /*VkCommandBufferResetFlagBits*/ 0);
    VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = 0; // Optional
        beginInfo.pInheritanceInfo = NULL; // Optional
    VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));

    // vkCmdFillBuffer(commandBuffer,    copyCounterBuffers[currentFrame], sizeof(int)*2, sizeof(int)*1, 0);
    // vkCmdFillBuffer(commandBuffer,    copyCounterBuffers[currentFrame], sizeof(int)*3, sizeof(int)*1000, 0);
    // vkCmdFillBuffer(commandBuffer, paletteCounterBuffers[currentFrame], sizeof(int)*0, VK_WHOLE_SIZE, 0);

    vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            0,
            0, NULL,
            0, NULL,
            0, NULL
        ); 
        
    VkImageCopy copy_op = {};
        copy_op.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copy_op.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copy_op.srcSubresource.layerCount = 1;
        copy_op.dstSubresource.layerCount = 1;
        copy_op.srcSubresource.baseArrayLayer = 0;
        copy_op.dstSubresource.baseArrayLayer = 0;
        copy_op.srcSubresource.mipLevel = 0;
        copy_op.dstSubresource.mipLevel = 0;
        copy_op.srcOffset = {0,0,0};
        copy_op.dstOffset = {0,0,0};
        copy_op.extent = {16*BLOCK_PALETTE_SIZE_X,16*BLOCK_PALETTE_SIZE_Y,16};
    vkCmdCopyImage(commandBuffer, 
        origin_block_palette[currentFrame].image,
        VK_IMAGE_LAYOUT_GENERAL, //TODO:
        block_palette[currentFrame].image,
        VK_IMAGE_LAYOUT_GENERAL, //TODO:
        1,
        &copy_op
    );
    VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = block_palette[currentFrame].image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT ;
        barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;

    vkCmdPipelineBarrier(
        commandBuffer,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        0,
        0, NULL,
        0, NULL,
        1, &barrier
    );
    // copy_Whole_Image(, commandBuffer, 
    //     , );
    // copy_Whole_Image({8,8,8}, commandBuffer, 
    //     originWorldvector<Image>[currentFrame],       world[currentFrame].image);
}
void Renderer::startBlockify(){
    VkCommandBuffer &commandBuffer = computeCommandBuffers[currentFrame];

    // vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, blockifyPipeline);
    // vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, blockifyLayout, 0, 1, &blockifyDescriptorSets[currentFrame], 0, 0);

    // VkBufferMemoryBarrier copy_barrier = {};
    //     copy_barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    //     copy_barrier.size = VK_WHOLE_SIZE;
    //     copy_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    //     copy_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    //     copy_barrier.buffer = paletteCounterBuffers[currentFrame];
    //     copy_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT| VK_ACCESS_TRANSFER_READ_BIT;
    //     copy_barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    // // vkcmdbarrer
    // vkCmdPipelineBarrier(
    //     commandBuffer,
    //     VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
    //     0,
    //     0, NULL,
    //     1, &copy_barrier,
    //     0, NULL
    copy_queue.clear();

    palette_counter = 6; //TODO: change to static block count. Currently its size of 1 cause of air
    size_t size_to_copy = world_size.x * world_size.y * world_size.z * sizeof(BlockID_t);
    memcpy(current_world.data(), origin_world.data(), size_to_copy);

    vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            0,
            0, NULL,
            0, NULL,
            0, NULL
        ); 
}
struct AABB {
  vec3 min;
  vec3 max;
};
static AABB get_shift(mat4 trans, ivec3 size){
    vec3 box = vec3(size-1);
    vec3 zero = vec3(0);
    vec3 corners[8];
    corners[0] = zero;
    corners[1] = vec3(zero.x, box.y, zero.z);
    corners[2] = vec3(zero.x, box.y, box.z);
    corners[3] = vec3(zero.x, zero.y, box.z);
    corners[4] = vec3(box.x, zero.y, zero.z);
    corners[5] = vec3(box.x, box.y, zero.z);
    corners[6] = box;
    corners[7] = vec3(box.x, zero.y, box.z);
        
    // transform the first corner
    vec3 tmin = vec3(trans * vec4(corners[0],1.0));
    vec3 tmax = tmin;
        
    // transform the other 7 corners and compute the result AABB
    for(int i = 1; i < 8; i++)
    {
        vec3 point = vec3(trans * vec4(corners[i],1.0));

        tmin = min(tmin, point);
        tmax = max(tmax, point);
    }
    AABB rbox;
    
    rbox.min = tmin;
    rbox.max = tmax;
    
    return rbox;
}
// allocates temp block in palette for every block that intersects with every mesh blockified
//TODO: MAKE THIS ACTUALLY DYNAMIC
void Renderer::blockifyMesh(Mesh* mesh){
    //cpu code that does smth in mapped memory and than "flushes" to gpu
    // int* block_image = (int*)stagingWorldvector<BuffersMapped>[currentFrame];

    struct AABB border_in_voxel = get_shift((*mesh).transform, (*mesh).size);
    
    struct {ivec3 min; ivec3 max;} border; 
    border.min = ivec3(border_in_voxel.min) / ivec3(16) -ivec3(1);
    border.max = ivec3(border_in_voxel.max) / ivec3(16) +ivec3(1);

    // border.min = ivec3(0);
    // border.max = ivec3(7);

    border.min = clamp(border.min, ivec3(0), ivec3(7));
    border.max = clamp(border.max, ivec3(0), ivec3(7));
    
    for(int xx = border.min.x; xx <= border.max.x; xx++){
    for(int yy = border.min.y; yy <= border.max.y; yy++){
    for(int zz = border.min.z; zz <= border.max.z; zz++){
        //if inside model TODO
        // if(
        //     any(greaterThanEqual(ivec3(xx,yy,zz), ivec3(8))) ||
        //     any(   lessThan     (ivec3(xx,yy,zz), ivec3(0)))) {
        //         continue;
        //     }
        //static blocks TODO
        int current_block = current_world(xx,yy,zz); 
        if (current_block == 0){ //if not allocated
            current_world(xx,yy,zz) = palette_counter;
            palette_counter++;
        } else {
            if(current_block <= 5) { // static and non-empty
                //add to copy queue
                ivec2 src_block = {}; 
                tie(src_block.x, src_block.y) = get_block_xy(current_block);
                ivec2 dst_block = {}; 
                tie(dst_block.x, dst_block.y) = get_block_xy(palette_counter);
                VkImageCopy 
                    static_block_copy = {};
                    static_block_copy.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                    static_block_copy.srcSubresource.baseArrayLayer = 0;
                    static_block_copy.srcSubresource.layerCount = 1;
                    static_block_copy.srcSubresource.mipLevel = 0;
                    static_block_copy.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                    static_block_copy.dstSubresource.baseArrayLayer = 0;
                    static_block_copy.dstSubresource.layerCount = 1;
                    static_block_copy.dstSubresource.mipLevel = 0;
                    static_block_copy.extent = {16,16,16};
                    static_block_copy.srcOffset = {src_block.x*16, src_block.y*16, 0};
                    static_block_copy.dstOffset = {dst_block.x*16, dst_block.y*16, 0};
                    // static_block_copy.
                    copy_queue.push_back(static_block_copy);

                    current_world(xx,yy,zz) = palette_counter;
                    palette_counter++; // yeah i dont trust myself in this  
            } else {
                //already new block, just leave it
            }
        }
        // block_image[xx + yy*8 + zz*8*8] = palette_counter++;
    }}}
    // for(int xx = 0; xx < world_size.x; xx++){
    // for(int yy = 0; yy < world_size.y; yy++){
    // for(int zz = 0; zz < world_size.z; zz++){
    //     block_image[xx + yy*8 + zz*8*8] = xx + yy*8 + zz*8*8 + 2;
    // }}}
    // vkFlushMappedMemoryRanges(device, 1, blockvector<ImagesMappe>[currentFrame]);
    // VkCommandBuffer &commandBuffer = computeCommandBuffers[currentFrame];

    // vkCmdPushConstants(commandBuffer, blockifyLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(mat4), &mesh.transform);
    // // vkCmdPushConstants(commandBuffer, blockifyLayout, VK_SHADER_STAGE_COMPUTE_BIT, sizeof(mat4), sizeof(int), &itime);
    // VkImageMemoryBarrier block_barrier{};
    //     block_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    //     block_barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    //     block_barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    //     block_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    //     block_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    //     block_barrier.image = world[currentFrame].image;
    //     block_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    //     block_barrier.subresourceRange.baseMipLevel = 0;
    //     block_barrier.subresourceRange.levelCount = 1;
    //     block_barrier.subresourceRange.baseArrayLayer = 0;
    //     block_barrier.subresourceRange.layerCount = 1;
    //     block_barrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT| VK_ACCESS_MEMORY_READ_BIT;
    //     block_barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    // vkCmdPipelineBarrier( 
    //     commandBuffer,
    //     VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
    //     0,
    //     0, NULL,
    //     0, NULL,
    //     1, &block_barrier
    // );
    
    // vkCmdDispatch(commandBuffer, (((mesh.size.x+15+16*2)/16)*2 +1)/4, 
    //                              (((mesh.size.y+15+16*2)/16)*2 +1)/4, 
    //                              (((mesh.size.z+15+16*2)/16)*2 +1)/4); //adds 1 block to size for each axis
    // block_barrier = {};
    //     block_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    //     block_barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    //     block_barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    //     block_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    //     block_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    //     block_barrier.image = world[currentFrame].image;
    //     block_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    //     block_barrier.subresourceRange.baseMipLevel = 0;
    //     block_barrier.subresourceRange.levelCount = 1;
    //     block_barrier.subresourceRange.baseArrayLayer = 0;
    //     block_barrier.subresourceRange.layerCount = 1;
    //     block_barrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT| VK_ACCESS_MEMORY_READ_BIT;
    //     block_barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    // vkCmdPipelineBarrier( 
    //     commandBuffer,
    //     VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
    //     0,
    //     0, NULL,
    //     0, NULL,
    //     1, &block_barrier
    // );
//     vkCmdDispatch(commandBuffer, 8, 
//                                  8, 
//                                  8); //adds 1 block to size for each axis
}
void Renderer::endBlockify(){
    VkCommandBuffer &commandBuffer = computeCommandBuffers[currentFrame];
    // int* block_image = (int*)stagingWorldvector<BuffersMapped>[currentFrame];
    size_t size_to_copy = world_size.x * world_size.y * world_size.z * sizeof(BlockID_t);
    assert(size_to_copy != 0);
    memcpy(staging_world_mapped[currentFrame], current_world.data(), size_to_copy);
    vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            0,
            0, NULL,
            0, NULL,
            0, NULL
        ); 
    vmaFlushAllocation(VMAllocator, staging_world[currentFrame].alloc, 0, size_to_copy);

    vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            0,
            0, NULL,
            0, NULL,
            0, NULL
        ); 

    if(copy_queue.size() != 0){
        vkCmdCopyImage(commandBuffer, origin_block_palette[currentFrame].image, VK_IMAGE_LAYOUT_GENERAL, block_palette[currentFrame].image, VK_IMAGE_LAYOUT_GENERAL, copy_queue.size(), copy_queue.data());
    // VkBufferMemoryBarrier copy_barrier{};
    //     copy_barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    //     copy_barrier.size = VK_WHOLE_SIZE;
    //     copy_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    //     copy_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    //     copy_barrier.buffer = copyCounterBuffers[currentFrame];  
    //     copy_barrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT| VK_ACCESS_MEMORY_READ_BIT;
    //     copy_barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    // // vkcmdbarrer
    // vkCmdPipelineBarrier(
    //     commandBuffer,
    //     VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
    //     0,
    //     0, NULL,
    //     1, &copy_barrier,
    //     0, NULL
    // );
    vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            0,
            0, NULL,
            0, NULL,
            0, NULL
        ); 
        VkImageMemoryBarrier block_palette_barrier{};
            block_palette_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            block_palette_barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
            block_palette_barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
            block_palette_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            block_palette_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            block_palette_barrier.image = block_palette[currentFrame].image;
            block_palette_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            block_palette_barrier.subresourceRange.baseMipLevel = 0;
            block_palette_barrier.subresourceRange.levelCount = 1;
            block_palette_barrier.subresourceRange.baseArrayLayer = 0;
            block_palette_barrier.subresourceRange.layerCount = 1;
            block_palette_barrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT| VK_ACCESS_MEMORY_READ_BIT;
            block_palette_barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
        vkCmdPipelineBarrier( 
            commandBuffer,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            0,
            0, NULL,
            0, NULL,
            1, &block_palette_barrier
        );
    }
    VkBufferImageCopy copyRegion = {};
    // copyRegion.size = size;
        copyRegion.imageExtent.width  = world_size.x;
        copyRegion.imageExtent.height = world_size.y;
        copyRegion.imageExtent.depth  = world_size.z;
        copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copyRegion.imageSubresource.layerCount = 1;
    vkCmdCopyBufferToImage(commandBuffer, staging_world[currentFrame].buffer, world[currentFrame].image, VK_IMAGE_LAYOUT_GENERAL, 1, &copyRegion);
vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            0,
            0, NULL,
            0, NULL,
            0, NULL
        ); 
    VkImageMemoryBarrier staging_world_barrier{};
        staging_world_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        staging_world_barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        staging_world_barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        staging_world_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        staging_world_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        staging_world_barrier.image = world[currentFrame].image;
        staging_world_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        staging_world_barrier.subresourceRange.baseMipLevel = 0;
        staging_world_barrier.subresourceRange.levelCount = 1;
        staging_world_barrier.subresourceRange.baseArrayLayer = 0;
        staging_world_barrier.subresourceRange.layerCount = 1;
        staging_world_barrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT| VK_ACCESS_MEMORY_READ_BIT;
        staging_world_barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    vkCmdPipelineBarrier( 
        commandBuffer,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        0,
        0, NULL,
        0, NULL,
        // 0, NULL
        1, &staging_world_barrier
    );
    vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            0,
            0, NULL,
            0, NULL,
            0, NULL
        ); 
}
void Renderer::blockifyCustom(void* ptr){
    size_t size_to_copy = world_size.x * world_size.y * world_size.z * sizeof(BlockID_t);
    memcpy(staging_world_mapped[currentFrame], ptr, size_to_copy);
    vmaFlushAllocation(VMAllocator, staging_world[currentFrame].alloc, 0, size_to_copy);
}

void Renderer::execCopies(){
    // VkCommandBuffer &commandBuffer = computeCommandBuffers[currentFrame];

    // vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, copyPipeline);//TODO:move away
    //     vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, copyLayout, 0, 1, &copyDescriptorSets[currentFrame], 0, 0);
    //     vkCmdDispatchIndirect(commandBuffer, copyCounterBuffers[currentFrame], 0);
    //     // vkCmdDispatch(commandBuffer, 16,16,16);
    //     VkImageMemoryBarrier barrier{};
    //         barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    //         barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    //         barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    //         barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    //         barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    //         barrier.image = block_palette[currentFrame].image;
    //         barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    //         barrier.subresourceRange.baseMipLevel = 0;
    //         barrier.subresourceRange.levelCount = 1;
    //         barrier.subresourceRange.baseArrayLayer = 0;
    //         barrier.subresourceRange.layerCount = 1;
    //         barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT| VK_ACCESS_SHADER_READ_BIT;
    //         barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    //     vkCmdPipelineBarrier(
    //         commandBuffer,
    //         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
    //         0,
    //         0, NULL,
    //         0, NULL,
    //         1, &barrier
    //     );
}

void Renderer::startMap(){
    VkCommandBuffer &commandBuffer = computeCommandBuffers[currentFrame];
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, mapPipeline);

//     VkImageMemoryBarrier barrier{};
//     barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
//     barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
//     barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
//     barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
//     barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
//     barrier.image = world[currentFrame].image;
//     barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
//     barrier.subresourceRange.baseMipLevel = 0;
//     barrier.subresourceRange.levelCount = 1;
//     barrier.subresourceRange.baseArrayLayer = 0;
//     barrier.subresourceRange.layerCount = 1;
//     barrier.srcAccessMask = VK_ACCESS_HOST_READ_BIT| VK_ACCESS_HOST_WRITE_BIT;
//     barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
// vkCmdPipelineBarrier(
//     commandBuffer,
//     VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
//     0,
//     0, NULL,
//     0, NULL,
//     1, &barrier
// );
}

void Renderer::mapMesh(Mesh* mesh){
    VkCommandBuffer &commandBuffer = computeCommandBuffers[currentFrame];
    
    //setting dynamic descriptors
        VkDescriptorImageInfo
            modelVoxelsInfo = {};
            modelVoxelsInfo.imageView = (*mesh).voxels[currentFrame].view; //CHANGE ME
            modelVoxelsInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        VkDescriptorImageInfo 
            blocksInfo = {};
            blocksInfo.imageView = world[currentFrame].view;
            blocksInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        VkDescriptorImageInfo 
            blockPaletteInfo = {};
            blockPaletteInfo.imageView = block_palette[currentFrame].view;
            blockPaletteInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

        VkWriteDescriptorSet 
            modelVoxelsWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            modelVoxelsWrite.dstSet = NULL;
            modelVoxelsWrite.dstBinding = 0;
            modelVoxelsWrite.dstArrayElement = 0;
            modelVoxelsWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            modelVoxelsWrite.descriptorCount = 1;
            modelVoxelsWrite.pImageInfo = &modelVoxelsInfo;
        VkWriteDescriptorSet 
            blocksWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            blocksWrite.dstSet = NULL;
            blocksWrite.dstBinding = 1;
            blocksWrite.dstArrayElement = 0;
            blocksWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            blocksWrite.descriptorCount = 1;
            blocksWrite.pImageInfo = &blocksInfo;
        VkWriteDescriptorSet 
            blockPaletteWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            blockPaletteWrite.dstSet = NULL;
            blockPaletteWrite.dstBinding = 2;
            blockPaletteWrite.dstArrayElement = 0;      
            blockPaletteWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            blockPaletteWrite.descriptorCount = 1;
            blockPaletteWrite.pImageInfo = &blockPaletteInfo;
        vector<VkWriteDescriptorSet> descriptorWrites = {modelVoxelsWrite, blocksWrite, blockPaletteWrite};

    vkCmdPushDescriptorSetKHR(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, mapLayout, 0, descriptorWrites.size(), descriptorWrites.data());
    // mat4 inversed_transform = inverse(mesh.transform); //from world to model space
    //  vec3 shift = (get_shift(mesh.transform, mesh.size).min);
    // ivec3 dispatch_size = ivec3(ceil(get_shift(mesh.transform, mesh.size).max));
    // // mat4 shift_matrix = translate(identity<mat4>(), shift);
    // // mat4 shifted_inversed_transform = inversed_transform * shift_matrix;
    // // printl(shift.x);printl(shift.y);printl(shift.z);
    // // printl(dispatch_size.x);printl(dispatch_size.y);printl(dispatch_size.z);

    // struct {mat4 trans; vec3 shift;} trans_size = {inversed_transform, shift};
    // //  trans = glm::scale(trans, vec3(0.95));
    // vkCmdPushConstants(commandBuffer, mapLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(trans_size), &trans_size);
    // // ivec3 adjusted_size = ivec3(round(vec3(mesh.size) * vec3(5.0)));
    // vkCmdDispatch(commandBuffer, dispatch_size.x*2 / 4, dispatch_size.y*2 / 4, dispatch_size.z*2 / 4);
    struct AABB border_in_voxel = get_shift((*mesh).transform, (*mesh).size);
    
    struct {ivec3 min; ivec3 max;} border; 
    border.min = ivec3(border_in_voxel.min) -ivec3(1);
    border.max = ivec3(border_in_voxel.max) +ivec3(1);
    //todo: clamp here not in shader
    ivec3 map_area = border.max - border.min;

    // struct {mat4 trans; ivec3 size;} trans_size = {mesh.transform, mesh.size};
    struct {mat4 trans; ivec4 size;} itrans_shift = {inverse((*mesh).transform), ivec4(border.min, 0)};
    // trans = glm::scale(trans, vec3(0.95));
    vkCmdPushConstants(commandBuffer, mapLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(itrans_shift), &itrans_shift);
    // ivec3 adjusted_size = ivec3(vec3(mesh.size) * vec3(2.0));
    vkCmdDispatch(commandBuffer, (map_area.x+3) / 4, (map_area.y+3) / 4, (map_area.z+3) / 4);   
}

void Renderer::endMap(){
    VkCommandBuffer &commandBuffer = computeCommandBuffers[currentFrame];

    vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            0,
            0, NULL,
            0, NULL,
            0, NULL
        ); 

    VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = block_palette[currentFrame].image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT  | VK_ACCESS_MEMORY_WRITE_BIT;
    vkCmdPipelineBarrier(
        commandBuffer,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        0,
        0, NULL,
        0, NULL,
        1, &barrier
    );
    vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            0,
            0, NULL,
            0, NULL,
            0, NULL
        ); 
}

void Renderer::recalculate_df(){
    VkCommandBuffer &commandBuffer = computeCommandBuffers[currentFrame];

    VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = block_palette[currentFrame].image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT  | VK_ACCESS_MEMORY_WRITE_BIT;
        
    // vkCmdPipelineBarrier(
    //     commandBuffer,
    //     VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
    //     0,
    //     0, NULL,
    //     0, NULL,
    //     1, &barrier
    // );
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, dfxLayout, 0, 1, &dfDescriptorSets[currentFrame], 0, 0);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, dfxPipeline);
        vkCmdDispatch(commandBuffer, 1, 1, 16*palette_counter);
        vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            0,
            0, NULL,
            0, NULL,
            1, &barrier
        );
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, dfyLayout, 0, 1, &dfDescriptorSets[currentFrame], 0, 0);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, dfyPipeline);
        vkCmdDispatch(commandBuffer, 1, 1, 16*palette_counter);
        vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            0,
            0, NULL,
            0, NULL,
            1, &barrier
        ); 
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, dfzLayout, 0, 1, &dfDescriptorSets[currentFrame], 0, 0);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, dfzPipeline);
        vkCmdDispatch(commandBuffer, 1, 1, 16*palette_counter);
        vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            0,
            0, NULL,
            0, NULL,
            1, &barrier
        );
}
void Renderer::raytrace(){
    VkCommandBuffer &commandBuffer = computeCommandBuffers[currentFrame];
    vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            0,
            0, NULL,
            0, NULL,
            0, NULL
        ); 

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, raytracePipeline);
        // transition_Image_Layout_Cmdb(commandBuffer, raytraced_frame[currentFrame].image, RAYTRACED_IMAGE_FORMAT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
        //     VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        //     0, VK_ACCESS_SHADER_WRITE_BIT);

        // vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, raytracePipeline);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, raytraceLayout, 0, 1, &raytraceDescriptorSets[currentFrame], 0, 0);

        itime++;
        // vec3(60, 0, 50);
        // normalize(vec3(0.1, 1.0, -0.5));
        struct tmp {ivec4 i; vec4 v1; vec4 v2;} tmp = {ivec4(itime,0,0,0), vec4(camera_pos,0), vec4(camera_dir,0)};
        vkCmdPushConstants(commandBuffer, raytraceLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(tmp), &tmp);
        // vkCmdPushConstants(commandBuffer, raytraceLayout, VK_SHADER_STAGE_COMPUTE_BIT, sizeof(int),              sizeof(vec3), &camera_pos);
        // vkCmdPushConstants(commandBuffer, raytraceLayout, VK_SHADER_STAGE_COMPUTE_BIT, sizeof(vec3)+sizeof(int), sizeof(vec3), &camera_dir);
        
        vkCmdDispatch(commandBuffer, raytraceExtent.width/8, raytraceExtent.height/8, 1);

    VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = raytraced_frame[currentFrame].image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT  | VK_ACCESS_MEMORY_WRITE_BIT;    
    vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            0,
            0, NULL,
            0, NULL,
            1, &barrier
        );    
        vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            0,
            0, NULL,
            0, NULL,
            0, NULL
        ); 
    barrier = {};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = denoised_frame[previousFrame].image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT  | VK_ACCESS_MEMORY_WRITE_BIT;    
    vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            0,
            0, NULL,
            0, NULL,
            1, &barrier
        );    
    barrier = {};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = denoised_frame[currentFrame].image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT  | VK_ACCESS_MEMORY_WRITE_BIT;    
    vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            0,
            0, NULL,
            0, NULL,
            1, &barrier
        );   
    vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            0,
            0, NULL,
            0, NULL,
            0, NULL
        ); 
}
void Renderer::denoise(int iterations){
    VkCommandBuffer &commandBuffer = computeCommandBuffers[currentFrame];
    vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            0,
            0, NULL,
            0, NULL,
            0, NULL
        ); 
    VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = denoised_frame[currentFrame].image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT |VK_ACCESS_MEMORY_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, denoisePipeline);
        // transition_Image_Layout_Cmdb(commandBuffer, raytraced_frame[currentFrame].image, RAYTRACED_IMAGE_FORMAT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
        //     VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        //     0, VK_ACCESS_SHADER_WRITE_BIT);

        // vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, raytracePipeline);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, denoiseLayout, 0, 1, &denoiseDescriptorSets[currentFrame], 0, 0);

        for(int atrous_iter = 0; atrous_iter<iterations; atrous_iter++){
        // for(int atrous_iter = 5; atrous_iter>0; atrous_iter--){
            vkCmdPushConstants(commandBuffer, denoiseLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(int), &atrous_iter);
            // vkCmdPushConstants()
            vkCmdDispatch(commandBuffer, raytraceExtent.width/8, raytraceExtent.height/8, 1);
            vkCmdPipelineBarrier(
                    commandBuffer,
                    VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                    0,
                    0, NULL,
                    0, NULL,
                    1, &barrier
                ); 
        } 

    barrier = {};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = raytraced_frame[currentFrame].image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT  | VK_ACCESS_MEMORY_WRITE_BIT;    
    vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            0,
            0, NULL,
            0, NULL,
            1, &barrier
        ); 
    vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            0,
            0, NULL,
            0, NULL,
            0, NULL
        ); 
}
void Renderer::upscale(){
    if(!is_scaled) crash(upscale with ratio 1? Are you stupid?);
    
    VkCommandBuffer &commandBuffer = computeCommandBuffers[currentFrame];

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, upscalePipeline);
        // transition_Image_Layout_Cmdb(commandBuffer, raytraced_frame[currentFrame].image, RAYTRACED_IMAGE_FORMAT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
        //     VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        //     0, VK_ACCESS_SHADER_WRITE_BIT);

        // vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, raytracePipeline);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, upscaleLayout, 0, 1, &upscaleDescriptorSets[currentFrame], 0, 0);

        vkCmdDispatch(commandBuffer, (swapChainExtent.width)/8, (swapChainExtent.height)/8, 1);

    VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = upscaled_frame[currentFrame].image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT  | VK_ACCESS_MEMORY_WRITE_BIT;    
    vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            0,
            0, NULL,
            0, NULL,
            1, &barrier
        ); 
}
void Renderer::endCompute(){
    VkCommandBuffer &commandBuffer = computeCommandBuffers[currentFrame];
    vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            0,
            0, NULL,
            0, NULL,
            0, NULL
        ); 
    VK_CHECK(vkEndCommandBuffer(commandBuffer));
        vector<VkSemaphore> computeWaitSemaphores = {rayGenFinishedSemaphores[currentFrame]};
        VkPipelineStageFlags computeWaitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        VkSubmitInfo submitInfo = {};
            submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submitInfo.pWaitDstStageMask = computeWaitStages;
            submitInfo.waitSemaphoreCount = computeWaitSemaphores.size();
            submitInfo.pWaitSemaphores    = computeWaitSemaphores.data();
            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = &computeCommandBuffers[currentFrame];
            submitInfo.signalSemaphoreCount = 1;
            submitInfo.pSignalSemaphores = &raytraceFinishedSemaphores[currentFrame];
    VK_CHECK(vkQueueSubmit(computeQueue, 1, &submitInfo, raytraceInFlightFences[currentFrame]));
}
void Renderer::present(){
    VkCommandBuffer &commandBuffer = graphicalCommandBuffers[currentFrame];

    VkResult result = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        cout << KRED"failed to present swap chain image!\n" KEND;
        recreate_Swapchain();
        return; // can be avoided, but it is just 1 frame 
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        cout << KRED"failed to acquire swap chain image!\n" KEND;
        exit(result);
    }
    vkWaitForFences(device, 1, &graphicalInFlightFences[currentFrame], VK_TRUE, UINT32_MAX);
    vkResetFences  (device, 1, &graphicalInFlightFences[currentFrame]);

    vkResetCommandBuffer(graphicalCommandBuffers[currentFrame], 0);
    VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = 0; // Optional
        beginInfo.pInheritanceInfo = NULL; // Optional

    VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));

    // transition_Image_Layout_Cmdb(commandBuffer, raytraced_frame[currentFrame].image, RAYTRACED_IMAGE_FORMAT, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    //     VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
    //     0, VK_ACCESS_SHADER_READ_BIT
    // );

    VkClearValue clearColor = {{{0.111f, 0.444f, 0.666f, 1.0f}}};
    VkRenderPassBeginInfo renderPassInfo = {};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = graphicalRenderPass;
        renderPassInfo.framebuffer = swapChainFramebuffers[imageIndex];
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = swapChainExtent;
        renderPassInfo.clearValueCount = 1;
        renderPassInfo.pClearValues = &clearColor;
    
    vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            0,
            0, NULL,
            0, NULL,
            0, NULL
        ); 
    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsLayout, 0, 1, &graphicalDescriptorSets[currentFrame], 0, NULL);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

    VkViewport viewport = {};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width  = (float)(swapChainExtent.width );
        viewport.height = (float)(swapChainExtent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor = {};
        scissor.offset = {0, 0};
        scissor.extent = swapChainExtent;
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
    
    vkCmdDraw(commandBuffer, 3, 1, 0, 0);
    // vkCmdDraw(commandBuffer, 3, 1, 0, 0);

    vkCmdEndRenderPass(commandBuffer);

vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            0,
            0, NULL,
            0, NULL,
            0, NULL
        ); 

    VK_CHECK(vkEndCommandBuffer(commandBuffer));

    // vector<VkSemaphore> waitSemaphores = {imageAvailableSemaphores[currentFrame]};
    vector<VkSemaphore> waitSemaphores = {raytraceFinishedSemaphores[currentFrame], imageAvailableSemaphores[currentFrame]};
    vector<VkSemaphore> signalSemaphores = {renderFinishedSemaphores[currentFrame]};
    //TODO: replace with VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    // VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    // VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    VkSubmitInfo submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.waitSemaphoreCount = waitSemaphores.size();
        submitInfo.pWaitSemaphores    = waitSemaphores.data();
        submitInfo.pWaitDstStageMask = waitStages;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &graphicalCommandBuffers[currentFrame];
        submitInfo.signalSemaphoreCount = signalSemaphores.size();
        submitInfo.pSignalSemaphores    = signalSemaphores.data();

    VK_CHECK(vkQueueSubmit(graphicsQueue, 1, &submitInfo, graphicalInFlightFences[currentFrame]));

    VkSwapchainKHR swapchains[] = {swapchain};
    VkPresentInfoKHR presentInfo = {};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = signalSemaphores.size();
        presentInfo.pWaitSemaphores = signalSemaphores.data();
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapchains;
        presentInfo.pImageIndices = &imageIndex;
        presentInfo.pResults = NULL; // Optional

    result = vkQueuePresentKHR(presentQueue, &presentInfo);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        cout << KRED"failed to present swap chain image!\n" KEND;
        recreate_Swapchain();
    } else if (result != VK_SUCCESS) {
        cout << KRED"failed to present swap chain image!\n" KEND;
        exit(result);
    }
}
void Renderer::end_Frame(){
    previousFrame = currentFrame;
    currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void Renderer::create_Image_Storages(vector<Image>* images, 
    VkImageType type, VkFormat format, VkImageUsageFlags usage, VmaMemoryUsage vma_usage, VmaAllocationCreateFlags vma_flags, VkImageAspectFlags aspect, VkImageLayout layout, VkPipelineStageFlags pipeStage, VkAccessFlags access, 
    uvec3 size){
    (*images).resize(MAX_FRAMES_IN_FLIGHT);
    // (*image).allocs.resize(MAX_FRAMES_IN_FLIGHT);
    // (*image). views.resize(MAX_FRAMES_IN_FLIGHT);
    
    for (i32 i=0; i < MAX_FRAMES_IN_FLIGHT; i++){
        VkImageCreateInfo imageInfo = {};
            imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            imageInfo.imageType = type;
            imageInfo.format = format;
            imageInfo.mipLevels = 1;
            imageInfo.arrayLayers = 1;
            imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
            imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;

            // if(!(vma_usage & VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT)){
            //     imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
            // } else {
            //     imageInfo.tiling = VK_IMAGE_TILING_LINEAR;
            // }

            imageInfo.usage = usage;
            imageInfo.extent.width  = size.x;
            imageInfo.extent.height = size.y;
            imageInfo.extent.depth  = size.z;
            imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        VmaAllocationCreateInfo allocInfo = {};
            allocInfo.usage = vma_usage;
            allocInfo.flags = vma_flags;
            // allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
            // allocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;

            // allocInfo.requiredFlags = 
        VK_CHECK(vmaCreateImage(VMAllocator, &imageInfo, &allocInfo, &(*images)[i].image, &(*images)[i].alloc, NULL));

        VkImageViewCreateInfo viewInfo = {};
            viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            viewInfo.image = (*images)[i].image;
            if(type == VK_IMAGE_TYPE_2D){
                viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            } else if(type == VK_IMAGE_TYPE_3D) {
                viewInfo.viewType = VK_IMAGE_VIEW_TYPE_3D;
            }
            viewInfo.format = format;
            viewInfo.subresourceRange.aspectMask = aspect;
            viewInfo.subresourceRange.baseMipLevel = 0;
            viewInfo.subresourceRange.baseArrayLayer = 0;
            viewInfo.subresourceRange.levelCount = 1;
            viewInfo.subresourceRange.layerCount = 1;
        VK_CHECK(vkCreateImageView(device, &viewInfo, NULL, &(*images)[i].view));

        transition_Image_Layout_Singletime((*images)[i].image, format, aspect, VK_IMAGE_LAYOUT_UNDEFINED, layout,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, pipeStage,
            0, access);
        // if(!(vma_usage & VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT)){
        // }
    }
}
//fill manually with cmd or copy
void Renderer::create_Buffer_Storages(vector<Buffer>* buffers, VkBufferUsageFlags usage, u32 size, bool host){
    (*buffers).resize(MAX_FRAMES_IN_FLIGHT);
    // allocs.resize(MAX_FRAMES_IN_FLIGHT);
    
    for (i32 i=0; i < MAX_FRAMES_IN_FLIGHT; i++){
        VkBufferCreateInfo bufferInfo = {};
            bufferInfo.size = size;
            bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            bufferInfo.usage = usage;
        VmaAllocationCreateInfo allocInfo = {};
            // if (required_flags == VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
            // }
            // allocInfo.requiredFlags = required_flags;
            if(host) {
                allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
            }
            allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        VK_CHECK(vmaCreateBuffer(VMAllocator, &bufferInfo, &allocInfo, &(*buffers)[i].buffer, &(*buffers)[i].alloc, NULL));
    }
}

// vector<VkImage>          depth.image;
// vector<VkImageView>          depth.view;
// vector<VmaAllocation>           depth.alloc;

//TODO: need to find memory, verify flags
//make so each chunk is stored in staging buffer and then copied to fast memory

tuple<vector<Buffer>, vector<Buffer>> Renderer::create_RayGen_VertexBuffers(vector<Vertex> vertices, vector<u32> indices){
    return create_RayGen_VertexBuffers(vertices.data(), vertices.size(), indices.data(), indices.size());
}
//exists so i can easelly use ogt allocation without manual moving. BTW i also changed it src code for this
tuple<vector<Buffer>, vector<Buffer>> Renderer::create_RayGen_VertexBuffers(Vertex* vertices, u32 vcount, u32* indices, u32 icount){
    VkDeviceSize bufferSizeV = sizeof(Vertex)*vcount;
    // cout << bufferSizeV;
    VkDeviceSize bufferSizeI = sizeof(u32   )*icount;

    vector<Buffer> vertexes (MAX_FRAMES_IN_FLIGHT);
    vector<Buffer>  indexes (MAX_FRAMES_IN_FLIGHT);

    // vector<VkBuffer> vertexBuffers(MAX_FRAMES_IN_FLIGHT);
    // vector<VkBuffer>  indexBuffers(MAX_FRAMES_IN_FLIGHT);
    // vector<VmaAllocation> vertexAllocations(MAX_FRAMES_IN_FLIGHT);
    // vector<VmaAllocation>  indexAllocations(MAX_FRAMES_IN_FLIGHT);

    VkBufferCreateInfo stagingBufferInfo = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        stagingBufferInfo.size = bufferSizeV;
        stagingBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    VmaAllocationCreateInfo stagingAllocInfo = {};
        stagingAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        stagingAllocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    VkBuffer stagingBufferV = {};
    VmaAllocation stagingAllocationV = {};
    VkBuffer stagingBufferI = {};
    VmaAllocation stagingAllocationI = {};
    vmaCreateBuffer(VMAllocator, &stagingBufferInfo, &stagingAllocInfo, &stagingBufferV, &stagingAllocationV, NULL);
    stagingBufferInfo.size = bufferSizeI;
    vmaCreateBuffer(VMAllocator, &stagingBufferInfo, &stagingAllocInfo, &stagingBufferI, &stagingAllocationI, NULL);

    void* data;
assert (VMAllocator);
assert (stagingAllocationV);
assert (&data);
    vmaMapMemory(VMAllocator, stagingAllocationV, &data);
        memcpy(data, vertices, bufferSizeV);
    vmaUnmapMemory(VMAllocator, stagingAllocationV);

    vmaMapMemory(VMAllocator, stagingAllocationI, &data);
        memcpy(data, indices, bufferSizeI);
    vmaUnmapMemory(VMAllocator, stagingAllocationI);

    for(i32 i=0; i<MAX_FRAMES_IN_FLIGHT; i++){
        VkBufferCreateInfo bufferInfo = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
            bufferInfo.size = bufferSizeV;
            bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        VmaAllocationCreateInfo allocInfo = {};
            allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
        vmaCreateBuffer(VMAllocator, &bufferInfo, &allocInfo, &vertexes[i].buffer, &vertexes[i].alloc, NULL);
            bufferInfo.size = bufferSizeI;
            bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
        vmaCreateBuffer(VMAllocator, &bufferInfo, &allocInfo, &indexes[i].buffer, &indexes[i].alloc, NULL);
               
        copy_Buffer(stagingBufferV, vertexes[i].buffer, bufferSizeV);
        copy_Buffer(stagingBufferI,  indexes[i].buffer, bufferSizeI);
    }

    vmaDestroyBuffer(VMAllocator, stagingBufferV, stagingAllocationV);
    vmaDestroyBuffer(VMAllocator, stagingBufferI, stagingAllocationI);

    return {vertexes, indexes};
}
// static void blit_buffer_to_(VkBuffer srcBuffer, VkImage dstImage, uvec3 size, VkImageLayout layout) {
//     VkCommandBuffer commandBuffer= begin_Single_Time_Commands();

//     VkBufferImageCopy copyRegion = {};
//     // copyRegion.size = size;
//         copyRegion.imageExtent.width  = size.x;
//         copyRegion.imageExtent.height = size.y;
//         copyRegion.imageExtent.depth  = size.z;
//         copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
//         copyRegion.imageSubresource.layerCount = 1;
//     vkCmdCopyBufferToImage(commandBuffer, srcBuffer, dstImage, layout, 1, &copyRegion);

//     end_Single_Time_Commands(commandBuffer);
// }

vector<Image> Renderer::create_RayTrace_VoxelImages(u16* voxels, ivec3 size){
    VkDeviceSize bufferSize = (sizeof(u8)*2)*size.x*size.y*size.z;

    vector<Image> voxelImages = {};

    // vector<VkImage>   images(MAX_FRAMES_IN_FLIGHT);
    // vector<VkImageView>  views(MAX_FRAMES_IN_FLIGHT);
    // vector<VmaAllocation>  allocations(MAX_FRAMES_IN_FLIGHT);

    create_Image_Storages(&voxelImages,
        VK_IMAGE_TYPE_3D,
        VK_FORMAT_R8G8_UINT,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT,
        VK_IMAGE_LAYOUT_GENERAL,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_MEMORY_READ_BIT,
        size);


    VkBufferCreateInfo stagingBufferInfo = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        stagingBufferInfo.size = bufferSize;
        stagingBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    VmaAllocationCreateInfo stagingAllocInfo = {};
        stagingAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        stagingAllocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    VkBuffer stagingBuffer = {};
    VmaAllocation stagingAllocation = {};
    vmaCreateBuffer(VMAllocator, &stagingBufferInfo, &stagingAllocInfo, &stagingBuffer, &stagingAllocation, NULL);

    void* data;
    vmaMapMemory(VMAllocator, stagingAllocation, &data);
        memcpy(data, voxels, bufferSize);
    vmaUnmapMemory(VMAllocator, stagingAllocation);

    for(i32 i=0; i<MAX_FRAMES_IN_FLIGHT; i++){
        copy_Buffer(stagingBuffer, voxelImages[i].image, size, VK_IMAGE_LAYOUT_GENERAL);
    }
    vmaDestroyBuffer(VMAllocator, stagingBuffer, stagingAllocation);
    return voxelImages;
}

static u32 STORAGE_BUFFER_DESCRIPTOR_COUNT = 0;
static u32 STORAGE_IMAGE_DESCRIPTOR_COUNT = 0;
static u32 COMBINED_IMAGE_SAMPLER_DESCRIPTOR_COUNT = 0;
static u32 UNIFORM_BUFFER_DESCRIPTOR_COUNT = 0;
static void count_descriptor(const VkDescriptorType type){
    if(type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER){
        STORAGE_BUFFER_DESCRIPTOR_COUNT++;
    } else if(type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER){
        COMBINED_IMAGE_SAMPLER_DESCRIPTOR_COUNT++;
    } else if(type == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE){
        STORAGE_IMAGE_DESCRIPTOR_COUNT++;
    } else if(type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER){
        UNIFORM_BUFFER_DESCRIPTOR_COUNT++;    
    } else {
        cout << KRED "ADD DESCRIPTOR TO COUNTER\n" KEND;
        abort();
    }
}
//in order
static void create_Descriptor_Set_Layout_Helper(vector<VkDescriptorType> descriptorTypes, VkShaderStageFlags stageFlags, VkDescriptorSetLayout& layout, VkDevice device, VkDescriptorSetLayoutCreateFlags flags = 0){
    vector<VkDescriptorSetLayoutBinding> bindings = {};

    for (i32 i=0; i<descriptorTypes.size(); i++) {
        count_descriptor(descriptorTypes[i]);
        
        VkDescriptorSetLayoutBinding 
            bind = {};
            bind.binding = i;
            bind.descriptorType = descriptorTypes[i];
            bind.descriptorCount = 1;
            bind.stageFlags = stageFlags;
        bindings.push_back(bind);
    }

    VkDescriptorSetLayoutCreateInfo 
        layoutInfo = {};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.flags = flags;
        layoutInfo.bindingCount = bindings.size();
        layoutInfo.pBindings    = bindings.data();
    VK_CHECK(vkCreateDescriptorSetLayout(device, &layoutInfo, NULL, &layout));
}

void Renderer::create_Descriptor_Set_Layouts(){
    //descriptors are counted after this
    create_Descriptor_Set_Layout_Helper({
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, //palette counter
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, //copy buffer - XYZ for indirect and copy operaitions
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,  //blocks
        }, 
        VK_SHADER_STAGE_COMPUTE_BIT, blockifyDescriptorSetLayout, device);
    
    create_Descriptor_Set_Layout_Helper({
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,  //1frame voxel palette
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, //copy buffer - XYZ for indirect and copy operaitions
        }, 
        VK_SHADER_STAGE_COMPUTE_BIT, copyDescriptorSetLayout, device);

    create_Descriptor_Set_Layout_Helper({
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, // modelVoxels
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, // blocks
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, // blockPalette
        }, 
        VK_SHADER_STAGE_COMPUTE_BIT, mapDescriptorSetLayout, device,
        VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR);
    create_Descriptor_Set_Layout_Helper({
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, // blockPalette
        }, 
        VK_SHADER_STAGE_COMPUTE_BIT, dfDescriptorSetLayout, device);

    create_Descriptor_Set_Layout_Helper({
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, //mat norm (downscaled)
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, //old uv   (downscaled)
        // VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, //depth    (downscaled)
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, //depth    (downscaled)
        // VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, //old mat norm (downscaled)
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, //old mat norm (downscaled)

        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, //blocks
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, //block palette
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, //mat palette
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, //step counts
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, //out frame (raytraced)
        // VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, //in previous frame (raytraced) to temporaly denoise
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, //in previous frame (raytraced) to temporaly denoise
        // VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, //in previous frame (raytraced) to temporaly denoise
        }, 
        VK_SHADER_STAGE_COMPUTE_BIT, raytraceDescriptorSetLayout, device);

    create_Descriptor_Set_Layout_Helper({
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, //mat norm (downscaled)
        // VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, //depth    (downscaled)
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, //depth    (downscaled)
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, //temp_denoised raytraced
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, //svgf denoised (final output)
        }, 
        VK_SHADER_STAGE_COMPUTE_BIT, denoiseDescriptorSetLayout, device);

    create_Descriptor_Set_Layout_Helper({
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, //mat norm (downscaled)
        // VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, //depth    (downscaled)
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, //mat norm (highres)
        // VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, //depth    (highres)
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, //denoised in
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, //upscaled out
        }, 
        VK_SHADER_STAGE_COMPUTE_BIT, upscaleDescriptorSetLayout, device);

    create_Descriptor_Set_Layout_Helper({
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, //frame-constant world_to_screen mat4
        }, 
        VK_SHADER_STAGE_VERTEX_BIT, RayGenDescriptorSetLayout, device);

    create_Descriptor_Set_Layout_Helper({
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, //in frame (raytraced)
        }, 
        VK_SHADER_STAGE_FRAGMENT_BIT, graphicalDescriptorSetLayout, device);
}

void Renderer::create_Descriptor_Pool(){
    // printl(STORAGE_IMAGE_DESCRIPTOR_COUNT);
    // printl(COMBINED_IMAGE_SAMPLER_DESCRIPTOR_COUNT);
    // printl(STORAGE_BUFFER_DESCRIPTOR_COUNT);

    VkDescriptorPoolSize STORAGE_IMAGE_PoolSize = {};
        STORAGE_IMAGE_PoolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        STORAGE_IMAGE_PoolSize.descriptorCount = STORAGE_IMAGE_DESCRIPTOR_COUNT*8;
    VkDescriptorPoolSize COMBINED_IMAGE_SAMPLER_PoolSize = {};
        COMBINED_IMAGE_SAMPLER_PoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        COMBINED_IMAGE_SAMPLER_PoolSize.descriptorCount = COMBINED_IMAGE_SAMPLER_DESCRIPTOR_COUNT*8;
    VkDescriptorPoolSize STORAGE_BUFFER_PoolSize = {};
        STORAGE_BUFFER_PoolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        STORAGE_BUFFER_PoolSize.descriptorCount = STORAGE_BUFFER_DESCRIPTOR_COUNT*8;
    VkDescriptorPoolSize UNIFORM_BUFFER_PoolSize = {};
        UNIFORM_BUFFER_PoolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        UNIFORM_BUFFER_PoolSize.descriptorCount = STORAGE_BUFFER_DESCRIPTOR_COUNT*8;

    vector<VkDescriptorPoolSize> poolSizes = {
        STORAGE_IMAGE_PoolSize, 
        COMBINED_IMAGE_SAMPLER_PoolSize, 
        STORAGE_BUFFER_PoolSize,
        UNIFORM_BUFFER_PoolSize};

    VkDescriptorPoolCreateInfo poolInfo = {};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = poolSizes.size();
        poolInfo.pPoolSizes = poolSizes.data();
        poolInfo.maxSets = MAX_FRAMES_IN_FLIGHT*8; //becuase frames_in_flight multiply of 5 differents sets, each for shader 

    VK_CHECK(vkCreateDescriptorPool(device, &poolInfo, NULL, &descriptorPool));
}

static void allocate_Descriptors_helper(vector<VkDescriptorSet>& sets, VkDescriptorSetLayout layout, VkDescriptorPool pool, VkDevice device){
    sets.resize(MAX_FRAMES_IN_FLIGHT);
    vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, layout);
    VkDescriptorSetAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = pool;
        allocInfo.descriptorSetCount = layouts.size();
        allocInfo.pSetLayouts        = layouts.data();
    VK_CHECK(vkAllocateDescriptorSets(device, &allocInfo, sets.data()));
}
void Renderer::allocate_Descriptors(){
    allocate_Descriptors_helper( raytraceDescriptorSets,  raytraceDescriptorSetLayout, descriptorPool, device); 
    allocate_Descriptors_helper(  denoiseDescriptorSets,   denoiseDescriptorSetLayout, descriptorPool, device); 
    allocate_Descriptors_helper(  upscaleDescriptorSets,   upscaleDescriptorSetLayout, descriptorPool, device); 
    allocate_Descriptors_helper( blockifyDescriptorSets,  blockifyDescriptorSetLayout, descriptorPool, device); 
    allocate_Descriptors_helper(     copyDescriptorSets,      copyDescriptorSetLayout, descriptorPool, device); 
    //we do not allocate this because it's descriptors are managed trough push_descriptor mechanism
    // allocate_Descriptors_helper(      mapDescriptorSets,       mapDescriptorSetLayout, descriptorPool, device); 
    allocate_Descriptors_helper(     dfDescriptorSets,      dfDescriptorSetLayout, descriptorPool, device); 
    allocate_Descriptors_helper(graphicalDescriptorSets, graphicalDescriptorSetLayout, descriptorPool, device);     
    allocate_Descriptors_helper(RayGenDescriptorSets, RayGenDescriptorSetLayout, descriptorPool, device);     
}
// void Renderer::setup_descriptors_helper(vector<VkDescriptorSet>& descriptor_sets, vector<vector<Buffers>> buffers, vector<vector<Image>> images){
//     vector<VkDescriptorBufferInfo> buffer_infos (buffers.size());
//     vector<VkDescriptorImageInfo> image_infos (images.size());
//     vector<VkWriteDescriptorSet> writes (buffers.size() + images.size());

//     //reusing same vectors for every frame in flight but its fine cause vk copies it anyways
//     for (u32 frame = 0; frame < MAX_FRAMES_IN_FLIGHT; frame++) {
//         //filling buffer infos
//         for(int b=0; b<buffers.size(); b++){
//             buffer_infos[b].buffer = buffers[b][frame].buffer;
//             buffer_infos[b].offset = 0;
//             buffer_infos[b].range = VK_WHOLE_SIZE;

//             writes[b+0] = {};
//             writes[b+0].dstSet = descriptor_sets[frame];
//             writes[b+0].dstBinding = b+0;
//             writes[b+0].dstArrayElement = 0;
//             writes[b+0].descriptorCount = 1;
//             writes[b+0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
//             writes[b+0].pBufferInfo = &buffer_infos[b];
//         }
//         for(int i=0; i<images.size(); i++){
//             image_infos[i].imageView = images[i][frame].view;
//             image_infos[i].imageLayout = VK_IMAGE_LAYOUT_GENERAL;

//             writes[i+buffers.size()] = {};
//             writes[i+buffers.size()].dstSet = descriptor_sets[frame];
//             writes[i+buffers.size()].dstBinding = i+buffers.size();
//             writes[i+buffers.size()].dstArrayElement = 0;
//             writes[i+buffers.size()].descriptorCount = 1;
//             writes[i+buffers.size()].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
//             writes[i+buffers.size()].pImageInfo = &image_infos[i];
//         }
        
//         vkUpdateDescriptorSets(device, writes.size(), writes.data(), 0, NULL);
//     }
// }
// static void setup_descriptors_helper(vector<vector<VkImageView>> views
// void Renderer::setup_Blockify_Descriptors(){
//     for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
//         VkDescriptorBufferInfo paletteCounterBufferInfo = {};
//             paletteCounterBufferInfo.buffer = depthStaginBuffers[i];
//             paletteCounterBufferInfo.offset = 0;
//             paletteCounterBufferInfo.range = VK_WHOLE_SIZE;
//         VkDescriptorBufferInfo copyCounterBufferInfo = {};
//             copyCounterBufferInfo.buffer = copyCounterBuffers[i];
//             copyCounterBufferInfo.offset = 0;
//             copyCounterBufferInfo.range = VK_WHOLE_SIZE;
//         VkDescriptorImageInfo inputBlocksInfo = {};
//             inputBlocksInfo.imageView = world[i].view;
//             inputBlocksInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

//         VkWriteDescriptorSet paletteCounterWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
//             paletteCounterWrite.dstSet = blockifyDescriptorSets[i];
//             paletteCounterWrite.dstBinding = 0;
//             paletteCounterWrite.dstArrayElement = 0;
//             paletteCounterWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
//             paletteCounterWrite.descriptorCount = 1;
//             paletteCounterWrite.pBufferInfo = &paletteCounterBufferInfo;
//         VkWriteDescriptorSet copyCounterWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
//             copyCounterWrite.dstSet = blockifyDescriptorSets[i];
//             copyCounterWrite.dstBinding = 1;
//             copyCounterWrite.dstArrayElement = 0;
//             copyCounterWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
//             copyCounterWrite.descriptorCount = 1;
//             copyCounterWrite.pBufferInfo = &copyCounterBufferInfo;
//         VkWriteDescriptorSet blocksWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
//             blocksWrite.dstSet = blockifyDescriptorSets[i];
//             blocksWrite.dstBinding = 2;
//             blocksWrite.dstArrayElement = 0;
//             blocksWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
//             blocksWrite.descriptorCount = 1;
//             blocksWrite.pImageInfo = &inputBlocksInfo;
//         vector<VkWriteDescriptorSet> descriptorWrites = {paletteCounterWrite, copyCounterWrite, blocksWrite};

//         vkUpdateDescriptorSets(device, descriptorWrites.size(), descriptorWrites.data(), 0, NULL);
//     }
// }
void Renderer::setup_Copy_Descriptors(){
    for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkDescriptorImageInfo inputBlockPaletteInfo = {};
            inputBlockPaletteInfo.imageView = block_palette[i].view;
            inputBlockPaletteInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        VkDescriptorBufferInfo copyCounterBufferInfo = {};
            copyCounterBufferInfo.buffer = copyCounterBuffers[i];
            copyCounterBufferInfo.offset = 0;
            copyCounterBufferInfo.range = VK_WHOLE_SIZE;

        VkWriteDescriptorSet blockPaletteWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            blockPaletteWrite.dstSet = copyDescriptorSets[i];
            blockPaletteWrite.dstBinding = 0;
            blockPaletteWrite.dstArrayElement = 0;
            blockPaletteWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            blockPaletteWrite.descriptorCount = 1;
            blockPaletteWrite.pImageInfo = &inputBlockPaletteInfo;
        VkWriteDescriptorSet copyCounterWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            copyCounterWrite.dstSet = copyDescriptorSets[i];
            copyCounterWrite.dstBinding = 1;
            copyCounterWrite.dstArrayElement = 0;
            copyCounterWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            copyCounterWrite.descriptorCount = 1;
            copyCounterWrite.pBufferInfo = &copyCounterBufferInfo;
        vector<VkWriteDescriptorSet> descriptorWrites = {blockPaletteWrite, copyCounterWrite};

        vkUpdateDescriptorSets(device, descriptorWrites.size(), descriptorWrites.data(), 0, NULL);
    }
}
//SHOULD NOT EXIST push descriptors to command buffer instead
void Renderer::setup_Map_Descriptors(){
    // for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    // }
}
void Renderer::setup_Df_Descriptors(){
    for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkDescriptorImageInfo inputBlockPaletteInfo = {};
            inputBlockPaletteInfo.imageView = block_palette[i].view;
            inputBlockPaletteInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        // VkDescriptorBufferInfo copyCounterBufferInfo = {};
        //     copyCounterBufferInfo.buffer = copyCounterBuffers[i];
        //     copyCounterBufferInfo.offset = 0;
        //     copyCounterBufferInfo.range = VK_WHOLE_SIZE;

        VkWriteDescriptorSet blockPaletteWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            blockPaletteWrite.dstSet = dfDescriptorSets[i];
            blockPaletteWrite.dstBinding = 0;
            blockPaletteWrite.dstArrayElement = 0;
            blockPaletteWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            blockPaletteWrite.descriptorCount = 1;
            blockPaletteWrite.pImageInfo = &inputBlockPaletteInfo;
        // VkWriteDescriptorSet copyCounterWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        //     copyCounterWrite.dstSet = copyDescriptorSets[i];
        //     copyCounterWrite.dstBinding = 1;
        //     copyCounterWrite.dstArrayElement = 0;
        //     copyCounterWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        //     copyCounterWrite.descriptorCount = 1;
        //     copyCounterWrite.pBufferInfo = &copyCounterBufferInfo;
        vector<VkWriteDescriptorSet> descriptorWrites = {blockPaletteWrite};

        vkUpdateDescriptorSets(device, descriptorWrites.size(), descriptorWrites.data(), 0, NULL);
    }
}
void Renderer::setup_Raytrace_Descriptors(){
    for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        int previous_frame = i - 1;
        if (previous_frame < 0) previous_frame = MAX_FRAMES_IN_FLIGHT-1; // so frames do not intersect

        VkDescriptorImageInfo matNormInfo = {};
            matNormInfo.imageView = is_scaled? matNorm_downscaled[i].view : matNorm[i].view;
            matNormInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        VkDescriptorImageInfo oldUvInfo = {};
            oldUvInfo.imageView = is_scaled? oldUv_downscaled[i].view : oldUv[i].view;
            oldUvInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        VkDescriptorImageInfo depthInfo = {};
            // depthInfo.imageView = is_scaled? depth_downscaled[i].view : depth[i].view;
            depthInfo.imageView = depthForChecks[currentFrame].view;
            depthInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            depthInfo.sampler = raytracedImageSamplers[currentFrame];
        VkDescriptorImageInfo old_matNormInfo = {};
            old_matNormInfo.imageView = is_scaled? matNorm_downscaled[previous_frame].view : matNorm[previous_frame].view;
            old_matNormInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            old_matNormInfo.sampler = raytracedImageSamplers[currentFrame];
        // VkDescriptorImageInfo inputNormInfo = {};
        //     inputNormInfo.imageView = rayGenNormImageViews[i];
        //     inputNormInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        // VkDescriptorImageInfo inputPosDiffInfo = {};
        //     inputPosDiffInfo.imageView = rayGenPosDiffImageViews[i];
        //     inputPosDiffInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        VkDescriptorImageInfo inputBlockInfo = {};
            inputBlockInfo.imageView = world[i].view;
            inputBlockInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            // inputBlockInfo.sampler = blockPaletteImageSamplers[i];
        VkDescriptorImageInfo inputBlockPaletteInfo = {};
            inputBlockPaletteInfo.imageView = block_palette[i].view;
            inputBlockPaletteInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        VkDescriptorImageInfo inputVoxelPaletteInfo = {};
            inputVoxelPaletteInfo.imageView = material_palette[i].view;
            inputVoxelPaletteInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        VkDescriptorImageInfo outputFrameInfo = {};
            outputFrameInfo.imageView = raytraced_frame[i].view;
            outputFrameInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            outputFrameInfo.sampler = raytracedImageSamplers[i];
        VkDescriptorImageInfo stepCountInfo = {};
            stepCountInfo.imageView = step_count[i].view;
            stepCountInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        VkDescriptorImageInfo readFrameInfo = {};
            readFrameInfo.imageView = denoised_frame[previous_frame].view;
            // readFrameInfo.imageView = raytraced_frame[previous_frame].view;
            readFrameInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            readFrameInfo.sampler = denoisedImageSamplers[previous_frame];
            
        VkWriteDescriptorSet matNormWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            matNormWrite.dstSet = raytraceDescriptorSets[i];
            matNormWrite.dstBinding = 0;
            matNormWrite.dstArrayElement = 0;
            matNormWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            matNormWrite.descriptorCount = 1;
            matNormWrite.pImageInfo = &matNormInfo;
        VkWriteDescriptorSet oldUvWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            oldUvWrite.dstSet = raytraceDescriptorSets[i];
            oldUvWrite.dstBinding = 1;
            oldUvWrite.dstArrayElement = 0;
            oldUvWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            oldUvWrite.descriptorCount = 1;
            oldUvWrite.pImageInfo = &oldUvInfo;
        VkWriteDescriptorSet depthWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            depthWrite.dstSet = raytraceDescriptorSets[i];
            depthWrite.dstBinding = 2;
            depthWrite.dstArrayElement = 0;
            // depthWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            depthWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            depthWrite.descriptorCount = 1;
            depthWrite.pImageInfo = &depthInfo;
        VkWriteDescriptorSet old_matNormWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            old_matNormWrite.dstSet = raytraceDescriptorSets[i];
            old_matNormWrite.dstBinding = 3;
            old_matNormWrite.dstArrayElement = 0;
            old_matNormWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            old_matNormWrite.descriptorCount = 1;
            old_matNormWrite.pImageInfo = &old_matNormInfo;

        
        // VkWriteDescriptorSet gBufferWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        //     gBufferWrite.dstSet = raytraceDescriptorSets[i];
        //     gBufferWrite.dstBinding = 0;
        //     gBufferWrite.dstArrayElement = 0;
        //     gBufferWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        //     gBufferWrite.descriptorCount = 1;
        //     gBufferWrite.pImageInfo = &gBufferInfo;
        // VkWriteDescriptorSet old_gBufferWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        //     old_gBufferWrite.dstSet = raytraceDescriptorSets[i];
        //     old_gBufferWrite.dstBinding = 7;
        //     old_gBufferWrite.dstArrayElement = 0;
        //     old_gBufferWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        //     old_gBufferWrite.descriptorCount = 1;
        //     old_gBufferWrite.pImageInfo = &old_gBufferInfo;
        // VkWriteDescriptorSet NormWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        //     NormWrite.dstSet = raytraceDescriptorSets[i];
        //     NormWrite.dstBinding = 1;
        //     NormWrite.dstArrayElement = 0;
        //     NormWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        //     NormWrite.descriptorCount = 1;
        //     NormWrite.pImageInfo = &inputNormInfo;
        // VkWriteDescriptorSet PosDiffWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        //     PosDiffWrite.dstSet = raytraceDescriptorSets[i];
        //     PosDiffWrite.dstBinding = 2;
        //     PosDiffWrite.dstArrayElement = 0;
        //     PosDiffWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        //     PosDiffWrite.descriptorCount = 1;
        //     PosDiffWrite.pImageInfo = &inputPosDiffInfo;
        VkWriteDescriptorSet blockWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            blockWrite.dstSet = raytraceDescriptorSets[i];
            blockWrite.dstBinding = 4;
            blockWrite.dstArrayElement = 0;
            blockWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            blockWrite.descriptorCount = 1;
            blockWrite.pImageInfo = &inputBlockInfo;
        VkWriteDescriptorSet blockPaletteWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            blockPaletteWrite.dstSet = raytraceDescriptorSets[i];
            blockPaletteWrite.dstBinding = 5;
            blockPaletteWrite.dstArrayElement = 0;
            blockPaletteWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            blockPaletteWrite.descriptorCount = 1;
            blockPaletteWrite.pImageInfo = &inputBlockPaletteInfo;
        VkWriteDescriptorSet voxelPaletteWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            voxelPaletteWrite.dstSet = raytraceDescriptorSets[i];
            voxelPaletteWrite.dstBinding = 6;
            voxelPaletteWrite.dstArrayElement = 0;
            voxelPaletteWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            voxelPaletteWrite.descriptorCount = 1;
            voxelPaletteWrite.pImageInfo = &inputVoxelPaletteInfo;
        VkWriteDescriptorSet stepCount = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            stepCount.dstSet = raytraceDescriptorSets[i];
            stepCount.dstBinding = 7;
            stepCount.dstArrayElement = 0;
            stepCount.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            stepCount.descriptorCount = 1;
            stepCount.pImageInfo = &stepCountInfo;
        VkWriteDescriptorSet outNewRaytracedWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            outNewRaytracedWrite.dstSet = raytraceDescriptorSets[i];
            outNewRaytracedWrite.dstBinding = 8;
            outNewRaytracedWrite.dstArrayElement = 0;
            outNewRaytracedWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            outNewRaytracedWrite.descriptorCount = 1;
            outNewRaytracedWrite.pImageInfo = &outputFrameInfo;
        //binds to previous frame as intended
        VkWriteDescriptorSet oldDenoisedWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            oldDenoisedWrite.dstSet = raytraceDescriptorSets[i];
            oldDenoisedWrite.dstBinding = 9;
            oldDenoisedWrite.dstArrayElement = 0;
            // outReadSampler.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            oldDenoisedWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            oldDenoisedWrite.descriptorCount = 1;
            oldDenoisedWrite.pImageInfo = &readFrameInfo; // to prevent reading what you write
        vector<VkWriteDescriptorSet> descriptorWrites = {matNormWrite, oldUvWrite, depthWrite, old_matNormWrite, blockWrite, blockPaletteWrite, voxelPaletteWrite, stepCount, outNewRaytracedWrite, oldDenoisedWrite/*blockPaletteSampler*/};

        vkUpdateDescriptorSets(device, descriptorWrites.size(), descriptorWrites.data(), 0, NULL);
    }
}
void Renderer::setup_Denoise_Descriptors(){
    for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkDescriptorImageInfo matNormInfo = {};
            matNormInfo.imageView = is_scaled? matNorm_downscaled[i].view : matNorm[i].view;
            matNormInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        VkDescriptorImageInfo depthInfo = {};
            // depthInfo.imageView = is_scaled? depth_downscaled[i].view : depth[i].view;
            depthInfo.imageView = depthForChecks[currentFrame].view;
            depthInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            depthInfo.sampler = raytracedImageSamplers[currentFrame];
            
        VkDescriptorImageInfo readFrameInfo = {};
            readFrameInfo.imageView = raytraced_frame[i].view;
            readFrameInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            // readFrameInfo.sampler = raytracedImageSamplers[previous_frame];
        VkDescriptorImageInfo outputFrameInfo = {};
            outputFrameInfo.imageView = denoised_frame[i].view;
            outputFrameInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            
        VkWriteDescriptorSet matNormWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            matNormWrite.dstSet = denoiseDescriptorSets[i];
            matNormWrite.dstBinding = 0;
            matNormWrite.dstArrayElement = 0;
            matNormWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            matNormWrite.descriptorCount = 1;
            matNormWrite.pImageInfo = &matNormInfo;
        VkWriteDescriptorSet depthWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            depthWrite.dstSet = denoiseDescriptorSets[i];
            depthWrite.dstBinding = 1;
            depthWrite.dstArrayElement = 0;
            // depthWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            depthWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            depthWrite.descriptorCount = 1;
            depthWrite.pImageInfo = &depthInfo;
        VkWriteDescriptorSet raytracedFrameWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
           raytracedFrameWrite.dstSet = denoiseDescriptorSets[i];
           raytracedFrameWrite.dstBinding = 2;
           raytracedFrameWrite.dstArrayElement = 0;
           raytracedFrameWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
           raytracedFrameWrite.descriptorCount = 1;
           raytracedFrameWrite.pImageInfo = &readFrameInfo;
        //binds to previous frame as intended
        VkWriteDescriptorSet denoisedFrameWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            denoisedFrameWrite.dstSet = denoiseDescriptorSets[i];
            denoisedFrameWrite.dstBinding = 3;
            denoisedFrameWrite.dstArrayElement = 0;
            denoisedFrameWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            denoisedFrameWrite.descriptorCount = 1;
            denoisedFrameWrite.pImageInfo = &outputFrameInfo; // to prevent reading what you write
        vector<VkWriteDescriptorSet> descriptorWrites = {matNormWrite, depthWrite, raytracedFrameWrite, denoisedFrameWrite/*blockPaletteSampler*/};

        vkUpdateDescriptorSets(device, descriptorWrites.size(), descriptorWrites.data(), 0, NULL);
    }
}
void Renderer::setup_Upscale_Descriptors(){
    for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkDescriptorImageInfo lowres_matNormInfo = {};
            lowres_matNormInfo.imageView = matNorm_downscaled[i].view;
            lowres_matNormInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        VkDescriptorImageInfo highres_matNormInfo = {};
            highres_matNormInfo.imageView = matNorm[i].view;
            highres_matNormInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        VkDescriptorImageInfo denoisedFrameInfo = {};
            denoisedFrameInfo.imageView = denoised_frame[i].view;
            denoisedFrameInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        VkDescriptorImageInfo upscaledFrameInfo = {};
            upscaledFrameInfo.imageView = upscaled_frame[i].view;
            upscaledFrameInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            
        VkWriteDescriptorSet lowres_matNormWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            lowres_matNormWrite.dstSet = upscaleDescriptorSets[i];
            lowres_matNormWrite.dstBinding = 0;
            lowres_matNormWrite.dstArrayElement = 0;
            lowres_matNormWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            lowres_matNormWrite.descriptorCount = 1;
            lowres_matNormWrite.pImageInfo = &lowres_matNormInfo;
        VkWriteDescriptorSet highres_matNormWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
           highres_matNormWrite.dstSet = upscaleDescriptorSets[i];
           highres_matNormWrite.dstBinding = 1;
           highres_matNormWrite.dstArrayElement = 0;
           highres_matNormWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
           highres_matNormWrite.descriptorCount = 1;
           highres_matNormWrite.pImageInfo = &highres_matNormInfo;
        VkWriteDescriptorSet denoisedFrameWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
           denoisedFrameWrite.dstSet = upscaleDescriptorSets[i];
           denoisedFrameWrite.dstBinding = 2;
           denoisedFrameWrite.dstArrayElement = 0;
           denoisedFrameWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
           denoisedFrameWrite.descriptorCount = 1;
           denoisedFrameWrite.pImageInfo = &denoisedFrameInfo;
        VkWriteDescriptorSet upscaledFrameWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
           upscaledFrameWrite.dstSet = upscaleDescriptorSets[i];
           upscaledFrameWrite.dstBinding = 3;
           upscaledFrameWrite.dstArrayElement = 0;
           upscaledFrameWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
           upscaledFrameWrite.descriptorCount = 1;
           upscaledFrameWrite.pImageInfo = &upscaledFrameInfo;
        vector<VkWriteDescriptorSet> descriptorWrites = {lowres_matNormWrite, highres_matNormWrite, denoisedFrameWrite, upscaledFrameWrite/*blockPaletteSampler*/};

        vkUpdateDescriptorSets(device, descriptorWrites.size(), descriptorWrites.data(), 0, NULL);
    }
}

void Renderer::create_samplers(){
    VkSamplerCreateInfo samplerInfo = {};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_NEAREST;
        samplerInfo.minFilter = VK_FILTER_NEAREST;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.anisotropyEnable = VK_FALSE;
        // samplerInfo.maxAnisotropy = 0;
        samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;
        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.mipLodBias = 0.0f;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = 0.0f;
    
    raytracedImageSamplers.resize(MAX_FRAMES_IN_FLIGHT);
     denoisedImageSamplers.resize(MAX_FRAMES_IN_FLIGHT);
    for (auto& sampler : raytracedImageSamplers) {
        VK_CHECK(vkCreateSampler(device, &samplerInfo, NULL, &sampler));
    }
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    for (auto& sampler : denoisedImageSamplers) {
        VK_CHECK(vkCreateSampler(device, &samplerInfo, NULL, &sampler));
    }

    // samplerInfo.unnormalizedCoordinates = VK_TRUE;
    // samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    // samplerInfo.minFilter = VK_FILTER_NEAREST;
    // samplerInfo.magFilter = VK_FILTER_NEAREST;
    // blockPaletteImageSamplers.resize(MAX_FRAMES_IN_FLIGHT);
    // for (auto& sampler : blockPaletteImageSamplers) {
    //     VK_CHECK(vkCreateSampler(device, &samplerInfo, NULL, &sampler));
    // }
}

void Renderer::setup_Graphical_Descriptors(){
    for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkDescriptorImageInfo storageImageInfo = {};
            if(is_scaled) {
                storageImageInfo.imageView = upscaled_frame[i].view;
            } else {
                storageImageInfo.imageView = denoised_frame[i].view;
            }
            storageImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            storageImageInfo.sampler = denoisedImageSamplers[i];
        VkWriteDescriptorSet descriptorWrite = {};
            descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrite.dstSet = graphicalDescriptorSets[i];
            descriptorWrite.dstBinding = 0;
            descriptorWrite.dstArrayElement = 0;
            descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptorWrite.descriptorCount = 1;
            descriptorWrite.pImageInfo = &storageImageInfo;
        vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, NULL);
    }
}
void Renderer::setup_RayGen_Descriptors(){
    for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkDescriptorBufferInfo transInfo = {};
            transInfo.offset = 0;
            transInfo.buffer = RayGenUniformBuffers[i].buffer;
            transInfo.range = VK_WHOLE_SIZE;
        VkWriteDescriptorSet descriptorWrite = {};
            descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrite.dstSet = RayGenDescriptorSets[i];
            descriptorWrite.dstBinding = 0;
            descriptorWrite.dstArrayElement = 0;
            descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            descriptorWrite.descriptorCount = 1;
            descriptorWrite.pBufferInfo = &transInfo;
        vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, NULL);
    }
}
void Renderer::create_compute_pipelines_helper(const char* name, VkDescriptorSetLayout  descriptorSetLayout, VkPipelineLayout* pipelineLayout, VkPipeline* pipeline, u32 push_size){
    auto compShaderCode = read_Shader(name);

    VkShaderModule module = create_Shader_Module(&compShaderCode);
    
    //single stage
    VkPipelineShaderStageCreateInfo compShaderStageInfo = {};
        compShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        compShaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        compShaderStageInfo.module = module;
        compShaderStageInfo.pName = "main";

    VkPushConstantRange pushRange = {};
        // pushRange.size = 256;
        pushRange.size = push_size; //trans
        pushRange.offset = 0;
        pushRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO; 
        pipelineLayoutInfo.setLayoutCount = 1; // 1 input (image from swapchain)  for now
        pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;
        if(push_size != 0) {
            pipelineLayoutInfo.pushConstantRangeCount = 1;
            pipelineLayoutInfo.pPushConstantRanges = &pushRange;
        }
        // pipelineLayoutInfo.pushConstantRangeCount = 0;
        // pipelineLayoutInfo.pPushConstantRanges = NULL;
    VK_CHECK(vkCreatePipelineLayout(device, &pipelineLayoutInfo, NULL, pipelineLayout));

    VkComputePipelineCreateInfo pipelineInfo = {};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        pipelineInfo.stage = compShaderStageInfo; //always single stage so no pointer :)
        pipelineInfo.layout = *pipelineLayout;

    VK_CHECK(vkCreateComputePipelines(device, NULL, 1, &pipelineInfo, NULL, pipeline));
    vkDestroyShaderModule(device, module, NULL);
}

void Renderer::create_compute_pipelines(){
    create_compute_pipelines_helper("shaders/compiled/blockify.spv", blockifyDescriptorSetLayout, &blockifyLayout, &blockifyPipeline, sizeof(mat4));
    create_compute_pipelines_helper("shaders/compiled/copy.spv"    , copyDescriptorSetLayout    , &copyLayout    , &copyPipeline    , 0);
    create_compute_pipelines_helper("shaders/compiled/map.spv"     , mapDescriptorSetLayout     , &mapLayout     , &mapPipeline     , sizeof(mat4) + sizeof(ivec4));
    create_compute_pipelines_helper("shaders/compiled/dfx.spv"      , dfDescriptorSetLayout      , &dfxLayout      , &dfxPipeline      , 0);
    create_compute_pipelines_helper("shaders/compiled/dfy.spv"      , dfDescriptorSetLayout      , &dfyLayout      , &dfyPipeline      , 0);
    create_compute_pipelines_helper("shaders/compiled/dfz.spv"      , dfDescriptorSetLayout      , &dfzLayout      , &dfzPipeline      , 0);
    create_compute_pipelines_helper("shaders/compiled/comp.spv"    , raytraceDescriptorSetLayout, &raytraceLayout, &raytracePipeline, sizeof(i32)*+sizeof(vec3)*2);
    create_compute_pipelines_helper("shaders/compiled/denoise.spv" ,  denoiseDescriptorSetLayout,  &denoiseLayout,  &denoisePipeline, sizeof(int));
    // create_compute_pipelines_helper("shaders/compiled/upscale.spv"    ,  upscaleDescriptorSetLayout,  &upscaleLayout,  &upscalePipeline, 0);
    create_compute_pipelines_helper("shaders/compiled/upscale.spv" ,  upscaleDescriptorSetLayout,  &upscaleLayout,  &upscalePipeline, 0);
    // create_Blockify_Pipeline();
    // create_Copy_Pipeline();
    // create_Map_Pipeline();
    // create_Df_Pipeline();
    // create_Raytrace_Pipeline();

}

// void Renderer::create_Blockify_Pipeline(){
//     auto compShaderCode = read_Shader("shaders/compiled/blockify.spv");

//     blockifyShaderModule = create_Shader_Module(compShaderCode);
    
//     //single stage
//     VkPipelineShaderStageCreateInfo compShaderStageInfo = {};
//         compShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
//         compShaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
//         compShaderStageInfo.module = blockifyShaderModule;
//         compShaderStageInfo.pName = "main";

//     VkPushConstantRange pushRange = {};
//         // pushRange.size = 256;
//         pushRange.size = sizeof(mat4) + sizeof(int); //trans
//         pushRange.offset = 0;
//         pushRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

//     VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
//         pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO; 
//         pipelineLayoutInfo.setLayoutCount = 1; // 1 input (image from swapchain)  for now
//         pipelineLayoutInfo.pSetLayouts = &blockifyDescriptorSetLayout;
//         pipelineLayoutInfo.pushConstantRangeCount = 1;
//         pipelineLayoutInfo.pPushConstantRanges = &pushRange;
//         // pipelineLayoutInfo.pushConstantRangeCount = 0;
//         // pipelineLayoutInfo.pPushConstantRanges = NULL;
//     VK_CHECK(vkCreatePipelineLayout(device, &pipelineLayoutInfo, NULL, &blockifyLayout));

//     VkComputePipelineCreateInfo pipelineInfo = {};
//         pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
//         pipelineInfo.stage = compShaderStageInfo; //always single stage so no pointer :)
//         pipelineInfo.layout = blockifyLayout;

//     VK_CHECK(vkCreateComputePipelines(device, NULL, 1, &pipelineInfo, NULL, &blockifyPipeline));
// //I<3Vk
// }
// void Renderer::create_Copy_Pipeline(){
//     auto compShaderCode = read_Shader("shaders/compiled/copy.spv");

//     copyShaderModule = create_Shader_Module(compShaderCode);
    
//     //single stage
//     VkPipelineShaderStageCreateInfo compShaderStageInfo = {};
//         compShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
//         compShaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
//         compShaderStageInfo.module = copyShaderModule;
//         compShaderStageInfo.pName = "main";

//     VkPushConstantRange pushRange = {};
//         // pushRange.size = 256;
//         pushRange.size = 0;
//         pushRange.offset = 0;
//         pushRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

//     VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
//         pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO; 
//         pipelineLayoutInfo.setLayoutCount = 1; // 1 input (image from swapchain)  for now
//         pipelineLayoutInfo.pSetLayouts = &copyDescriptorSetLayout;
//         // pipelineLayoutInfo.pushConstantRangeCount = 1;
//         // pipelineLayoutInfo.pPushConstantRanges = &pushRange;
//         pipelineLayoutInfo.pushConstantRangeCount = 0;
//         pipelineLayoutInfo.pPushConstantRanges = NULL;
//     VK_CHECK(vkCreatePipelineLayout(device, &pipelineLayoutInfo, NULL, &copyLayout));

//     VkComputePipelineCreateInfo pipelineInfo = {};
//         pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
//         pipelineInfo.stage = compShaderStageInfo; //always single stage so no pointer :)
//         pipelineInfo.layout = copyLayout;

//     VK_CHECK(vkCreateComputePipelines(device, NULL, 1, &pipelineInfo, NULL, &copyPipeline));
// //I<3Vk
// }
// void Renderer::create_Map_Pipeline(){
//     auto compShaderCode = read_Shader("shaders/compiled/map.spv");

//     mapShaderModule = create_Shader_Module(compShaderCode);
    
//     //single stage
//     VkPipelineShaderStageCreateInfo compShaderStageInfo = {};
//         compShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
//         compShaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
//         compShaderStageInfo.module = mapShaderModule;
//         compShaderStageInfo.pName = "main";

//     VkPushConstantRange pushRange = {};
//         // pushRange.size = 256;
//         pushRange.size = sizeof(mat4)+sizeof(ivec3); //trans + model_size
//         pushRange.offset = 0;
//         pushRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

//     VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
//         pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO; 
//         pipelineLayoutInfo.setLayoutCount = 1; // 1 input (image from swapchain)  for now
//         pipelineLayoutInfo.pSetLayouts = &mapDescriptorSetLayout;
//         pipelineLayoutInfo.pushConstantRangeCount = 1;
//         pipelineLayoutInfo.pPushConstantRanges = &pushRange;
//         // pipelineLayoutInfo.pushConstantRangeCount = 0;
//         // pipelineLayoutInfo.pPushConstantRanges = NULL;
//     VK_CHECK(vkCreatePipelineLayout(device, &pipelineLayoutInfo, NULL, &mapLayout));

//     VkComputePipelineCreateInfo pipelineInfo = {};
//         pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
//         pipelineInfo.stage = compShaderStageInfo; //always single stage so no pointer :)
//         pipelineInfo.layout = mapLayout;

//     VK_CHECK(vkCreateComputePipelines(device, NULL, 1, &pipelineInfo, NULL, &mapPipeline));
// //I<3Vk
// }
// void Renderer::create_Df_Pipeline(){
//     auto compShaderCode = read_Shader("shaders/compiled/df.spv");

//     dfShaderModule = create_Shader_Module(compShaderCode);
    
//     //single stage
//     VkPipelineShaderStageCreateInfo compShaderStageInfo = {};
//         compShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
//         compShaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
//         compShaderStageInfo.module = dfShaderModule;
//         compShaderStageInfo.pName = "main";

//     VkPushConstantRange pushRange = {};
//         pushRange.size = 0;
//         // pushRange.size = sizeof(mat4)+sizeof(ivec3); //trans + model_size
//         pushRange.offset = 0;
//         pushRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

//     VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
//         pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO; 
//         pipelineLayoutInfo.setLayoutCount = 1; // 1 input (image from swapchain)  for now
//         pipelineLayoutInfo.pSetLayouts = &dfDescriptorSetLayout;
//         pipelineLayoutInfo.pushConstantRangeCount = 0;
//         pipelineLayoutInfo.pPushConstantRanges = NULL;
//         // pipelineLayoutInfo.pushConstantRangeCount = 0;
//         // pipelineLayoutInfo.pPushConstantRanges = NULL;
//     VK_CHECK(vkCreatePipelineLayout(device, &pipelineLayoutInfo, NULL, &dfLayout));

//     VkComputePipelineCreateInfo pipelineInfo = {};
//         pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
//         pipelineInfo.stage = compShaderStageInfo; //always single stage so no pointer :)
//         pipelineInfo.layout = dfLayout;

//     VK_CHECK(vkCreateComputePipelines(device, NULL, 1, &pipelineInfo, NULL, &dfPipeline));
// //I<3Vk
// }
// void Renderer::create_Raytrace_Pipeline(){
//     auto compShaderCode = read_Shader("shaders/compiled/comp.spv");

//     raytraceShaderModule = create_Shader_Module(compShaderCode);
    
//     //single stage
//     VkPipelineShaderStageCreateInfo compShaderStageInfo = {};
//         compShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
//         compShaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
//         compShaderStageInfo.module = raytraceShaderModule;
//         compShaderStageInfo.pName = "main";

//     VkPushConstantRange pushRange = {};
//         // pushRange.size = 256;
//         pushRange.size = sizeof(i32)*1;
//         pushRange.offset = 0;
//         pushRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

//     VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
//         pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO; 
//         pipelineLayoutInfo.setLayoutCount = 1; // 1 input (image from swapchain)  for now
//         pipelineLayoutInfo.pSetLayouts = &raytraceDescriptorSetLayout;
//         pipelineLayoutInfo.pushConstantRangeCount = 1;
//         pipelineLayoutInfo.pPushConstantRanges = &pushRange;
//         // pipelineLayoutInfo.pushConstantRangeCount = 0;
//         // pipelineLayoutInfo.pPushConstantRanges = NULL;
//     VK_CHECK(vkCreatePipelineLayout(device, &pipelineLayoutInfo, NULL, &raytraceLayout));

//     VkComputePipelineCreateInfo pipelineInfo = {};
//         pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
//         pipelineInfo.stage = compShaderStageInfo; //always single stage so no pointer :)
//         pipelineInfo.layout = raytraceLayout;

//     VK_CHECK(vkCreateComputePipelines(device, NULL, 1, &pipelineInfo, NULL, &raytracePipeline));
// //I<3Vk
// }

u32 Renderer::find_Memory_Type(u32 typeFilter, VkMemoryPropertyFlags properties){
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);    

    for (u32 i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    cout << "memory type not found";
    abort();
}

void Renderer::create_Buffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer* buffer, VkDeviceMemory* bufferMemory) {
    VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = usage;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VK_CHECK(vkCreateBuffer(device, &bufferInfo, NULL, buffer));

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, (*buffer), &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = find_Memory_Type(memRequirements.memoryTypeBits, properties);

    VK_CHECK(vkAllocateMemory(device, &allocInfo, NULL, bufferMemory));

    VK_CHECK(vkBindBufferMemory(device, (*buffer), (*bufferMemory), 0));
}

#undef copy_Buffer
void Renderer::copy_Buffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
    VkCommandBuffer commandBuffer= begin_Single_Time_Commands();

    VkBufferCopy copyRegion{};
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

    end_Single_Time_Commands(commandBuffer);
}

void Renderer::copy_Buffer(VkBuffer srcBuffer, VkImage dstImage, uvec3 size, VkImageLayout layout) {
    VkCommandBuffer commandBuffer= begin_Single_Time_Commands();

    VkBufferImageCopy copyRegion = {};
    // copyRegion.size = size;
        copyRegion.imageExtent.width  = size.x;
        copyRegion.imageExtent.height = size.y;
        copyRegion.imageExtent.depth  = size.z;
        copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copyRegion.imageSubresource.layerCount = 1;
    vkCmdCopyBufferToImage(commandBuffer, srcBuffer, dstImage, layout, 1, &copyRegion);

    end_Single_Time_Commands(commandBuffer);
}

// #define copy_Buffer(a, ...) do {println; (copy_Buffer)(a, __VA_ARGS__);} while(0)

VkCommandBuffer Renderer::begin_Single_Time_Commands() {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    return commandBuffer;
}
void Renderer::end_Single_Time_Commands(VkCommandBuffer commandBuffer) {
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    //TODO: change to barrier
    vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicsQueue);

    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}

void Renderer::transition_Image_Layout_Singletime(VkImage image, VkFormat format, VkImageAspectFlags aspect,VkImageLayout oldLayout, VkImageLayout newLayout,
        VkPipelineStageFlags sourceStage, VkPipelineStageFlags destinationStage, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask) {

    VkCommandBuffer commandBuffer= begin_Single_Time_Commands();

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = aspect;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    barrier.srcAccessMask = srcAccessMask;
    barrier.dstAccessMask = dstAccessMask;

    vkCmdPipelineBarrier(
        commandBuffer,
        sourceStage, destinationStage,
        0,
        0, NULL,
        0, NULL,
        1, &barrier
    );

    end_Single_Time_Commands(commandBuffer);
}

void Renderer::copy_Whole_Image(VkExtent3D extent, VkCommandBuffer cmdbuf, VkImage src, VkImage dst){
    VkImageCopy copy_op = {};
        copy_op.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copy_op.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copy_op.srcSubresource.layerCount = 1;
        copy_op.dstSubresource.layerCount = 1;
        copy_op.srcSubresource.baseArrayLayer = 0;
        copy_op.dstSubresource.baseArrayLayer = 0;
        copy_op.srcSubresource.mipLevel = 0;
        copy_op.dstSubresource.mipLevel = 0;
        copy_op.srcOffset = {0, 0,0};
        copy_op.dstOffset = {0, 0,0};
        copy_op.extent = extent;
    vkCmdCopyImage(cmdbuf, 
        src,
        VK_IMAGE_LAYOUT_GENERAL, //TODO:
        dst,
        VK_IMAGE_LAYOUT_GENERAL, //TODO:
        1,
        &copy_op
    );
    VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = dst;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT ;
        barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;

    vkCmdPipelineBarrier(
        cmdbuf,
        VK_PIPELINE_STAGE_TRANSFER_BIT, 
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0,
        0, NULL,
        0, NULL,
        1, &barrier
    );
}

void Renderer::transition_Image_Layout_Cmdb(VkCommandBuffer commandBuffer, VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout,
        VkPipelineStageFlags sourceStage, VkPipelineStageFlags destinationStage, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask) {

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    barrier.srcAccessMask = srcAccessMask;
    barrier.dstAccessMask = dstAccessMask;

    vkCmdPipelineBarrier(
        commandBuffer,
        sourceStage, destinationStage,
        0,
        0, NULL,
        0, NULL,
        1, &barrier
    );
}

//NOTE: all following funcs are not updates - they are just "loads"

//TODO: make use of frames in flight - do not wait for this things
//TODO: do smth with frames in flight
static tuple<int, int> get_block_xy(int N){
    int x = N % BLOCK_PALETTE_SIZE_X;
    int y = N / BLOCK_PALETTE_SIZE_X;
    assert(y <= BLOCK_PALETTE_SIZE_Y);
    return tuple(x,y);
}

struct bp_tex {u8 r; u8 g;};
//block palette on cpu side is expected to be array of pointers to blocks
void Renderer::update_Block_Palette(Block** blockPalette){
    //block palette is in u8, but for easy copying we convert it to u16 cause block palette is R16_UNIT
    VkDeviceSize bufferSize = (sizeof(bp_tex))*16*BLOCK_PALETTE_SIZE_X* 16*BLOCK_PALETTE_SIZE_Y* 16;
    table3d<bp_tex> blockPaletteLinear = {};
        blockPaletteLinear.allocate(16*BLOCK_PALETTE_SIZE_X, 16*BLOCK_PALETTE_SIZE_Y, 16);
        blockPaletteLinear.set({0,0});
        
    // bp_tex* mem = (bp_tex*)malloc(((sizeof(Block)/sizeof(Voxel))*sizeof(bp_tex))*BLOCK_PALETTE_SIZE_X*BLOCK_PALETTE_SIZE_Y);
    for(i32 N=0; N<BLOCK_PALETTE_SIZE; N++){
        for(i32 x=0; x<BLOCK_SIZE; x++){
        for(i32 y=0; y<BLOCK_SIZE; y++){
        for(i32 z=0; z<BLOCK_SIZE; z++){
            auto [block_x, block_y] = get_block_xy(N);
            // blockPaletteLinear(x+16*block_x, y+16*block_y, z) = blockPalette[N].voxels[x][y][z];
            if(blockPalette[N] == NULL) {
                blockPaletteLinear(x+16*block_x, y+16*block_y, z).r = 0;
                // mem[(x+16*block_x) + (y+16*block_y)*256 + z*256*256] = {0,0};
            }
            else {
                //                 blockPaletteLinear(x+16*block_x, y+16*block_y, z).r = (u16) blockPalette[N]->voxels[x][y][z];
                // blockPaletteLinear(x+16*block_x, y+16*block_y, z).r = 16;
                // mem[(x+16*block_x) + (y+16*block_y)*256 + z*256*256] = {(u8) blockPalette[N]->voxels[x][y][z],0};
            }
    }}}}
    // cout << "(i32)blockPaletteLinear(0,0,0)" " "<< (i32)((i32*)blockPaletteLinear.data())[0] << "\n";
    // printl((i32)blockPaletteLinear(0,0,0));
    VkBufferCreateInfo stagingBufferInfo = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        stagingBufferInfo.size = bufferSize;
        stagingBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    VmaAllocationCreateInfo stagingAllocInfo = {};
        stagingAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        stagingAllocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    VkBuffer stagingBuffer = {};
    VmaAllocation stagingAllocation = {};
    void* data = NULL;
    printl(blockPaletteLinear.size().x);
    printl(blockPaletteLinear.size().y);
    printl(blockPaletteLinear.size().z);
    // printl((u64) stagingAllocation);
    // printl((u64) data);
    // fflush(stdout);
    VK_CHECK(vmaCreateBuffer(VMAllocator, &stagingBufferInfo, &stagingAllocInfo, &stagingBuffer, &stagingAllocation, NULL));

    VK_CHECK(vmaMapMemory(VMAllocator, stagingAllocation, &data));

        assert(data!=NULL);
        assert(bufferSize!=0);
        assert(blockPaletteLinear.data()!=NULL);
        memcpy(data, blockPaletteLinear.data(), bufferSize);
    vmaUnmapMemory(VMAllocator, stagingAllocation);

    for(i32 i=0; i<MAX_FRAMES_IN_FLIGHT; i++){
        copy_Buffer(stagingBuffer, origin_block_palette[i].image, uvec3(16*BLOCK_PALETTE_SIZE_X, 16*BLOCK_PALETTE_SIZE_Y, 16), VK_IMAGE_LAYOUT_GENERAL);
    }

    vmaDestroyBuffer(VMAllocator, stagingBuffer, stagingAllocation);
}

//TODO: do smth with frames in flight
void Renderer::update_Material_Palette(Material* materialPalette){
    VkDeviceSize bufferSize = sizeof(Material)*256;

    VkBufferCreateInfo stagingBufferInfo = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        stagingBufferInfo.size = bufferSize;
        stagingBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    VmaAllocationCreateInfo stagingAllocInfo = {};
        stagingAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        stagingAllocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    VkBuffer stagingBuffer = {};
    VmaAllocation stagingAllocation = {};
    vmaCreateBuffer(VMAllocator, &stagingBufferInfo, &stagingAllocInfo, &stagingBuffer, &stagingAllocation, NULL);

    void* data;
    vmaMapMemory(VMAllocator, stagingAllocation, &data);
        memcpy(data, materialPalette, bufferSize);
    vmaUnmapMemory(VMAllocator, stagingAllocation);

    for(i32 i=0; i<MAX_FRAMES_IN_FLIGHT; i++){
        copy_Buffer(stagingBuffer, material_palette[i].image, uvec3(6,256,1), VK_IMAGE_LAYOUT_GENERAL);
    }

    vmaDestroyBuffer(VMAllocator, stagingBuffer, stagingAllocation);
}

//TODO: do smth with frames in flight
//TODO: currently its update chunk
//has to match world_size
//useless btw
// void Renderer::update_SingleChunk(BlockID_t* blocks){
// //     VkDeviceSize bufferSize = sizeof(BlockID_t)*world_size.x*world_size.y*world_size.z;
//     printl(bufferSize);

//     VkBufferCreateInfo stagingBufferInfo = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
//         stagingBufferInfo.size = bufferSize;
//         stagingBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
//     VmaAllocationCreateInfo stagingAllocInfo = {};
//         stagingAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
//         stagingAllocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
//     VkBuffer stagingBuffer = {};
//     VmaAllocation stagingAllocation = {};
// // //     // printl(&stagingBufferInfo);
//     // printl(&stagingAllocInfo);
//     // printl(&stagingBuffer);
//     // printl(&stagingAllocation);
//     VK_CHECK(vmaCreateBuffer(VMAllocator, &stagingBufferInfo, &stagingAllocInfo, &stagingBuffer, &stagingAllocation, NULL));
//     void* data;
// // //     vmaMapMemory(VMAllocator, stagingAllocation, &data);
//         memcpy(data, blocks, bufferSize);
//     vmaUnmapMemory(VMAllocator, stagingAllocation);
// // 
//     for(i32 i=0; i<MAX_FRAMES_IN_FLIGHT; i++){
//         VkBufferCreateInfo bufferInfo = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
//             bufferInfo.size = bufferSize;
//             bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
//         VmaAllocationCreateInfo allocInfo = {};

//         copy_Buffer(stagingBuffer, originWorldvector<Image>[i], world_size, VK_IMAGE_LAYOUT_GENERAL);
//     }
// // 
//     vmaDestroyBuffer(VMAllocator, stagingBuffer, stagingAllocation);
// // }

char* readFileBuffer(const char* path, size_t* size) {
    FILE* fp = fopen(path, "rb");
    *size = _filelength(_fileno(fp));
    char* buffer = (char*) malloc(*size);
    fread(buffer, *size, 1, fp);
    fclose(fp);

    return buffer;
}

// namespace ogt { //this library for some reason uses ogt_ in cases when it will never intersect with others BUT DOES NOT WHEN IT FOR SURE WILL
#include <ogt_vox.hpp>
#include <ogt_voxel_meshify.hpp>
// } //so we'll have to use ogt::ogt_func :(

//TODO: alloca
// void load_mesh(Mesh* mesh, const char* vox_file, bool _make_vertices=true, bool extrude_palette=true);
// void load_mesh(Mesh* mesh, Voxel* voxel_mem, int x_size, int y_size, int z_size, bool _make_vertices=true, bool extrude_palette=true);
void Renderer::load_mesh(Mesh* mesh, const char* vox_file, bool _make_vertices, bool extrude_palette){
    size_t buffer_size;
    char* buffer = readFileBuffer(vox_file, &buffer_size);
    const ogt::vox_scene* scene = ogt::vox_read_scene((u8*)buffer, buffer_size);
    free(buffer);

    assert(scene->num_models == 1);

    load_mesh(mesh, (Voxel*)scene->models[0]->voxel_data, scene->models[0]->size_x, scene->models[0]->size_y, scene->models[0]->size_z, _make_vertices);
    
    if(extrude_palette and not has_palette){
        has_palette = true;
        for(int i=0; i<MATERIAL_PALETTE_SIZE; i++){   
            mat_palette[i].color = vec4(
                scene->palette.color[i].r / 256.0,
                scene->palette.color[i].g / 256.0,
                scene->palette.color[i].b / 256.0,
                scene->palette.color[i].a / 256.0
            );
            // mat_palette[i].emmit = scene->materials.matl[i].;
            mat_palette[i].emmit = 0;
            float rough;
            switch (scene->materials.matl[i].type) {
                case ogt::matl_type_diffuse: {
                    rough = 0.85;
                    break;
                }
                case ogt::matl_type_emit: {
                    mat_palette[i].emmit = scene->materials.matl[i].emit * (2.0 + scene->materials.matl[i].flux*2.0);
                    rough = 0.5;
                    break;
                }
                case ogt::matl_type_metal: {
                    rough = (scene->materials.matl[i].rough + (1.0-scene->materials.matl[i].metal))/2.0;
                    break;
                }
                default: {
                    rough = 0;
                    break;
                } 
            }   
            mat_palette[i].rough = rough;
            // if(scene->materials.matl[i].type == ogt::matl_type_metal) {
            //     if(scene->materials.matl[i].content_flags & ogt::k_vox_matl_have_metal){
            //         mat_palette[i].rough = (scene->materials.matl[i].rough + 0.2)/2.0;
            //     } else mat_palette[i].rough = scene->materials.matl[i].rough;
            //     // mat_palette[i].rough = 0.2;
                
            // } else if(scene->materials.matl[i].type == ogt::matl_type_diffuse){
            //     mat_palette[i].rough = 0.8;
            // } else printl(scene->materials.matl[i].type);
        }
        // (scene->palette);
    }
    
/*
roughness
IOR
Specular
Metallic

Emmission
Power - radiant flux
Ldr
*/
    ogt::vox_destroy_scene(scene);
}

void Renderer::load_block(Block** block, const char* vox_file){
    size_t buffer_size;
    char* buffer = readFileBuffer(vox_file, &buffer_size);
    const ogt::vox_scene* scene = ogt::vox_read_scene((u8*)buffer, buffer_size);
    free(buffer);
    assert(scene->num_models == 1);

    if((*block) == NULL) {
        *block = new Block;
    }

    load_mesh(&(*block)->mesh, (Voxel*)scene->models[0]->voxel_data, scene->models[0]->size_x, scene->models[0]->size_y, scene->models[0]->size_z, true);
    // voxels_extended.allocate(x_size, y_size, z_size);
    for (int x=0; x < scene->models[0]->size_x; x++){
    for (int y=0; y < scene->models[0]->size_y; y++){
    for (int z=0; z < scene->models[0]->size_z; z++){
        (*block)->voxels[x][y][z] = (u16) scene->models[0]->voxel_data[x + y*scene->models[0]->size_x + z*scene->models[0]->size_x*scene->models[0]->size_y];
    }}}
    
    ogt::vox_destroy_scene(scene);
}

void Renderer::load_mesh(Mesh* mesh, Voxel* Voxels, int x_size, int y_size, int z_size, bool _make_vertices){
    mesh->size = ivec3(x_size, y_size, z_size);
    if(_make_vertices){
        make_vertices(mesh, Voxels, x_size, y_size, z_size);
    }

    table3d<u16> voxels_extended = {};
    voxels_extended.allocate(x_size, y_size, z_size);
    for (int x=0; x < x_size; x++){
    for (int y=0; y < y_size; y++){
    for (int z=0; z < z_size; z++){
        voxels_extended(x,y,z) = (u16) Voxels[x + y*x_size + z*x_size*y_size];
    }}}

    mesh->voxels = create_RayTrace_VoxelImages(voxels_extended.data(), mesh->size);
    mesh->transform = identity<mat4>();
    
    voxels_extended.deallocate();
}
//frees only gpu side stuff, not mesh ptr
void Renderer::free_mesh(Mesh* mesh){
    for(int i=0; i<MAX_FRAMES_IN_FLIGHT; i++){
        if(not mesh->vertexes.empty()) vmaDestroyBuffer(VMAllocator, mesh->vertexes[i].buffer, mesh->vertexes[i].alloc);
        if(not mesh->indexes.empty()) vmaDestroyBuffer(VMAllocator, mesh->indexes[i].buffer, mesh->indexes[i].alloc);
        if(not mesh->voxels.empty()) vmaDestroyImage(VMAllocator, mesh->voxels[i].image, mesh->voxels[i].alloc);
        if(not mesh->voxels.empty()) vkDestroyImageView(device, mesh->voxels[i].view, NULL);
    }
}

void Renderer::make_vertices(Mesh* mesh, Voxel* Voxels, int x_size, int y_size, int z_size){
    ogt::ogt_voxel_meshify_context ctx = {};
    ogt::ogt_mesh* ogt_mesh = ogt::ogt_mesh_from_paletted_voxels_polygon(&ctx, (const u8*)Voxels, x_size, y_size, z_size, NULL);
    ogt::ogt_mesh_remove_duplicate_vertices(&ctx, ogt_mesh);

    mesh->transform = identity<mat4>();
    tie(mesh->vertexes, mesh->indexes) = create_RayGen_VertexBuffers((Vertex*)ogt_mesh->vertices, ogt_mesh->vertex_count, ogt_mesh->indices, ogt_mesh->index_count);
    mesh->icount = ogt_mesh->index_count;
}

// void Renderer::extrude_palette(Material* mat_palette){
    
// }

void Renderer::load_vertices(Mesh* mesh, Vertex* vertices){
    crash("not implemented yet");
}