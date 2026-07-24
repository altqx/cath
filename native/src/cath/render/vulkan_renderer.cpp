#include "cath/render/vulkan_renderer.hpp"

#include "cath/platform/log.hpp"

#include <SDL3/SDL_vulkan.h>
#include <glm/gtc/type_ptr.hpp>
#include <volk.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <fstream>
#include <optional>
#include <vector>

namespace cath {
namespace {

struct FrameUBO {
  glm::mat4 mvp;
  glm::mat4 model;
};

struct QueueFamilyIndices {
  std::optional<uint32_t> graphics;
  std::optional<uint32_t> present;
  bool complete() const { return graphics && present; }
};

QueueFamilyIndices find_queue_families(VkPhysicalDevice device, VkSurfaceKHR surface) {
  QueueFamilyIndices indices;
  uint32_t count = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);
  std::vector<VkQueueFamilyProperties> props(count);
  vkGetPhysicalDeviceQueueFamilyProperties(device, &count, props.data());
  for (uint32_t i = 0; i < count; ++i) {
    if (props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
      indices.graphics = i;
    }
    VkBool32 present = VK_FALSE;
    vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &present);
    if (present) {
      indices.present = i;
    }
    if (indices.complete()) {
      break;
    }
  }
  return indices;
}

std::vector<char> read_file(const std::filesystem::path& path) {
  std::ifstream in(path, std::ios::binary | std::ios::ate);
  if (!in) {
    return {};
  }
  const auto size = in.tellg();
  std::vector<char> buf(static_cast<size_t>(size));
  in.seekg(0);
  in.read(buf.data(), size);
  return buf;
}

VkShaderModule make_shader(VkDevice device, const std::vector<char>& code) {
  VkShaderModuleCreateInfo ci{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
  ci.codeSize = code.size();
  ci.pCode = reinterpret_cast<const uint32_t*>(code.data());
  VkShaderModule mod = VK_NULL_HANDLE;
  vkCreateShaderModule(device, &ci, nullptr, &mod);
  return mod;
}

}  // namespace

struct VulkanRenderer::Impl {
  SDL_Window* window = nullptr;
  VkInstance instance = VK_NULL_HANDLE;
  VkSurfaceKHR surface = VK_NULL_HANDLE;
  VkPhysicalDevice phys = VK_NULL_HANDLE;
  VkDevice device = VK_NULL_HANDLE;
  VkQueue graphics_queue = VK_NULL_HANDLE;
  VkQueue present_queue = VK_NULL_HANDLE;
  uint32_t graphics_family = 0;
  uint32_t present_family = 0;
  VkSwapchainKHR swapchain = VK_NULL_HANDLE;
  VkFormat swap_format = VK_FORMAT_B8G8R8A8_UNORM;
  VkExtent2D swap_extent{};
  std::vector<VkImage> swap_images;
  std::vector<VkImageView> swap_views;
  VkRenderPass render_pass = VK_NULL_HANDLE;
  std::vector<VkFramebuffer> framebuffers;
  VkCommandPool cmd_pool = VK_NULL_HANDLE;
  VkCommandBuffer cmd = VK_NULL_HANDLE;
  VkSemaphore image_available = VK_NULL_HANDLE;
  VkSemaphore render_finished = VK_NULL_HANDLE;
  VkFence in_flight = VK_NULL_HANDLE;
  VkDescriptorSetLayout desc_layout = VK_NULL_HANDLE;
  VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
  VkPipeline pipeline = VK_NULL_HANDLE;
  VkDescriptorPool desc_pool = VK_NULL_HANDLE;
  VkDescriptorSet desc_set = VK_NULL_HANDLE;
  VkBuffer vbo = VK_NULL_HANDLE;
  VkDeviceMemory vbo_mem = VK_NULL_HANDLE;
  VkBuffer ibo = VK_NULL_HANDLE;
  VkDeviceMemory ibo_mem = VK_NULL_HANDLE;
  VkBuffer ubo = VK_NULL_HANDLE;
  VkDeviceMemory ubo_mem = VK_NULL_HANDLE;
  void* ubo_mapped = nullptr;
  VkImage texture = VK_NULL_HANDLE;
  VkDeviceMemory texture_mem = VK_NULL_HANDLE;
  VkImageView texture_view = VK_NULL_HANDLE;
  VkSampler sampler = VK_NULL_HANDLE;
  VkImage depth_image = VK_NULL_HANDLE;
  VkDeviceMemory depth_mem = VK_NULL_HANDLE;
  VkImageView depth_view = VK_NULL_HANDLE;
  uint32_t index_count = 0;
  uint32_t tex_width = 0;
  uint32_t tex_height = 0;
  VkDeviceSize tex_row_pitch = 0;
  VkDeviceSize tex_layout_offset = 0;
  std::filesystem::path shader_dir;

  uint32_t find_memory_type(uint32_t type_bits, VkMemoryPropertyFlags props) const {
    VkPhysicalDeviceMemoryProperties mp{};
    vkGetPhysicalDeviceMemoryProperties(phys, &mp);
    for (uint32_t i = 0; i < mp.memoryTypeCount; ++i) {
      if ((type_bits & (1u << i)) && (mp.memoryTypes[i].propertyFlags & props) == props) {
        return i;
      }
    }
    return 0;
  }

  bool create_buffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags props, VkBuffer& buf,
                     VkDeviceMemory& mem) {
    VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bi.size = size;
    bi.usage = usage;
    bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(device, &bi, nullptr, &buf) != VK_SUCCESS) {
      return false;
    }
    VkMemoryRequirements req{};
    vkGetBufferMemoryRequirements(device, buf, &req);
    VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    ai.allocationSize = req.size;
    ai.memoryTypeIndex = find_memory_type(req.memoryTypeBits, props);
    if (vkAllocateMemory(device, &ai, nullptr, &mem) != VK_SUCCESS) {
      return false;
    }
    vkBindBufferMemory(device, buf, mem, 0);
    return true;
  }

  void destroy_swapchain_objects() {
    if (depth_view) {
      vkDestroyImageView(device, depth_view, nullptr);
      depth_view = VK_NULL_HANDLE;
    }
    if (depth_image) {
      vkDestroyImage(device, depth_image, nullptr);
      depth_image = VK_NULL_HANDLE;
    }
    if (depth_mem) {
      vkFreeMemory(device, depth_mem, nullptr);
      depth_mem = VK_NULL_HANDLE;
    }
    for (auto fb : framebuffers) {
      vkDestroyFramebuffer(device, fb, nullptr);
    }
    framebuffers.clear();
    for (auto v : swap_views) {
      vkDestroyImageView(device, v, nullptr);
    }
    swap_views.clear();
    if (swapchain) {
      vkDestroySwapchainKHR(device, swapchain, nullptr);
      swapchain = VK_NULL_HANDLE;
    }
  }

  bool create_depth() {
    VkImageCreateInfo ii{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    ii.imageType = VK_IMAGE_TYPE_2D;
    ii.extent = {swap_extent.width, swap_extent.height, 1};
    ii.mipLevels = 1;
    ii.arrayLayers = 1;
    ii.format = VK_FORMAT_D32_SFLOAT;
    ii.tiling = VK_IMAGE_TILING_OPTIMAL;
    ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    ii.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    ii.samples = VK_SAMPLE_COUNT_1_BIT;
    if (vkCreateImage(device, &ii, nullptr, &depth_image) != VK_SUCCESS) {
      return false;
    }
    VkMemoryRequirements req{};
    vkGetImageMemoryRequirements(device, depth_image, &req);
    VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    ai.allocationSize = req.size;
    ai.memoryTypeIndex = find_memory_type(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (vkAllocateMemory(device, &ai, nullptr, &depth_mem) != VK_SUCCESS) {
      return false;
    }
    vkBindImageMemory(device, depth_image, depth_mem, 0);
    VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    vi.image = depth_image;
    vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vi.format = VK_FORMAT_D32_SFLOAT;
    vi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    vi.subresourceRange.levelCount = 1;
    vi.subresourceRange.layerCount = 1;
    return vkCreateImageView(device, &vi, nullptr, &depth_view) == VK_SUCCESS;
  }

  bool create_swapchain() {
    destroy_swapchain_objects();
    VkSurfaceCapabilitiesKHR caps{};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(phys, surface, &caps);
    uint32_t format_count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(phys, surface, &format_count, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(format_count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(phys, surface, &format_count, formats.data());
    VkSurfaceFormatKHR chosen = formats[0];
    for (const auto& f : formats) {
      if (f.format == VK_FORMAT_B8G8R8A8_UNORM) {
        chosen = f;
        break;
      }
    }
    swap_format = chosen.format;
    if (caps.currentExtent.width != UINT32_MAX) {
      swap_extent = caps.currentExtent;
    } else {
      int w = 0, h = 0;
      SDL_GetWindowSizeInPixels(window, &w, &h);
      swap_extent.width = std::clamp(uint32_t(w), caps.minImageExtent.width, caps.maxImageExtent.width);
      swap_extent.height = std::clamp(uint32_t(h), caps.minImageExtent.height, caps.maxImageExtent.height);
    }
    uint32_t image_count = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && image_count > caps.maxImageCount) {
      image_count = caps.maxImageCount;
    }
    VkSwapchainCreateInfoKHR sci{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    sci.surface = surface;
    sci.minImageCount = image_count;
    sci.imageFormat = swap_format;
    sci.imageColorSpace = chosen.colorSpace;
    sci.imageExtent = swap_extent;
    sci.imageArrayLayers = 1;
    sci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    const uint32_t qfs[] = {graphics_family, present_family};
    if (graphics_family != present_family) {
      sci.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
      sci.queueFamilyIndexCount = 2;
      sci.pQueueFamilyIndices = qfs;
    } else {
      sci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }
    sci.preTransform = caps.currentTransform;
    sci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    sci.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    sci.clipped = VK_TRUE;
    if (vkCreateSwapchainKHR(device, &sci, nullptr, &swapchain) != VK_SUCCESS) {
      return false;
    }
    uint32_t n = 0;
    vkGetSwapchainImagesKHR(device, swapchain, &n, nullptr);
    swap_images.resize(n);
    vkGetSwapchainImagesKHR(device, swapchain, &n, swap_images.data());
    swap_views.resize(n);
    for (uint32_t i = 0; i < n; ++i) {
      VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
      vi.image = swap_images[i];
      vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
      vi.format = swap_format;
      vi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      vi.subresourceRange.levelCount = 1;
      vi.subresourceRange.layerCount = 1;
      if (vkCreateImageView(device, &vi, nullptr, &swap_views[i]) != VK_SUCCESS) {
        return false;
      }
    }
    if (!create_depth()) {
      return false;
    }
    framebuffers.resize(n);
    for (uint32_t i = 0; i < n; ++i) {
      const VkImageView atts[] = {swap_views[i], depth_view};
      VkFramebufferCreateInfo fi{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
      fi.renderPass = render_pass;
      fi.attachmentCount = 2;
      fi.pAttachments = atts;
      fi.width = swap_extent.width;
      fi.height = swap_extent.height;
      fi.layers = 1;
      if (vkCreateFramebuffer(device, &fi, nullptr, &framebuffers[i]) != VK_SUCCESS) {
        return false;
      }
    }
    return true;
  }
};

VulkanRenderer::~VulkanRenderer() { shutdown(); }

bool VulkanRenderer::init(SDL_Window* window, const std::filesystem::path& shader_dir, std::string* error) {
  shutdown();
  impl_ = new Impl();
  impl_->window = window;
  impl_->shader_dir = shader_dir;

  if (volkInitialize() != VK_SUCCESS) {
    if (error) {
      *error = "volkInitialize failed";
    }
    return false;
  }

  uint32_t ext_count = 0;
  const char* const* ext_names = SDL_Vulkan_GetInstanceExtensions(&ext_count);
  if (!ext_names) {
    if (error) {
      *error = SDL_GetError();
    }
    return false;
  }

  VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
  app.pApplicationName = "cath-viewer";
  app.apiVersion = VK_API_VERSION_1_2;

  VkInstanceCreateInfo ici{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
  ici.pApplicationInfo = &app;
  ici.enabledExtensionCount = ext_count;
  ici.ppEnabledExtensionNames = ext_names;
  if (vkCreateInstance(&ici, nullptr, &impl_->instance) != VK_SUCCESS) {
    if (error) {
      *error = "vkCreateInstance failed";
    }
    return false;
  }
  volkLoadInstance(impl_->instance);

  if (!SDL_Vulkan_CreateSurface(window, impl_->instance, nullptr, &impl_->surface)) {
    if (error) {
      *error = SDL_GetError();
    }
    return false;
  }

  uint32_t device_count = 0;
  vkEnumeratePhysicalDevices(impl_->instance, &device_count, nullptr);
  std::vector<VkPhysicalDevice> devices(device_count);
  vkEnumeratePhysicalDevices(impl_->instance, &device_count, devices.data());
  for (auto d : devices) {
    auto q = find_queue_families(d, impl_->surface);
    if (q.complete()) {
      impl_->phys = d;
      impl_->graphics_family = *q.graphics;
      impl_->present_family = *q.present;
      break;
    }
  }
  if (!impl_->phys) {
    if (error) {
      *error = "no suitable GPU";
    }
    return false;
  }

  float prio = 1.f;
  std::vector<VkDeviceQueueCreateInfo> qcis;
  std::vector<uint32_t> unique = {impl_->graphics_family, impl_->present_family};
  std::sort(unique.begin(), unique.end());
  unique.erase(std::unique(unique.begin(), unique.end()), unique.end());
  for (uint32_t family : unique) {
    VkDeviceQueueCreateInfo qci{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    qci.queueFamilyIndex = family;
    qci.queueCount = 1;
    qci.pQueuePriorities = &prio;
    qcis.push_back(qci);
  }
  const char* device_exts[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
  VkDeviceCreateInfo dci{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
  dci.queueCreateInfoCount = uint32_t(qcis.size());
  dci.pQueueCreateInfos = qcis.data();
  dci.enabledExtensionCount = 1;
  dci.ppEnabledExtensionNames = device_exts;
  if (vkCreateDevice(impl_->phys, &dci, nullptr, &impl_->device) != VK_SUCCESS) {
    if (error) {
      *error = "vkCreateDevice failed";
    }
    return false;
  }
  volkLoadDevice(impl_->device);
  vkGetDeviceQueue(impl_->device, impl_->graphics_family, 0, &impl_->graphics_queue);
  vkGetDeviceQueue(impl_->device, impl_->present_family, 0, &impl_->present_queue);

  VkAttachmentDescription color{};
  color.format = VK_FORMAT_B8G8R8A8_UNORM;  // updated after swapchain; recreate uses actual
  color.samples = VK_SAMPLE_COUNT_1_BIT;
  color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  color.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
  VkAttachmentDescription depth{};
  depth.format = VK_FORMAT_D32_SFLOAT;
  depth.samples = VK_SAMPLE_COUNT_1_BIT;
  depth.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  depth.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  depth.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  depth.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  depth.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  depth.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
  VkAttachmentReference color_ref{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
  VkAttachmentReference depth_ref{1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
  VkSubpassDescription sub{};
  sub.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  sub.colorAttachmentCount = 1;
  sub.pColorAttachments = &color_ref;
  sub.pDepthStencilAttachment = &depth_ref;
  VkSubpassDependency dep{};
  dep.srcSubpass = VK_SUBPASS_EXTERNAL;
  dep.dstSubpass = 0;
  dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
  dep.dstStageMask = dep.srcStageMask;
  dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
  std::array<VkAttachmentDescription, 2> atts{color, depth};
  VkRenderPassCreateInfo rpci{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
  rpci.attachmentCount = 2;
  rpci.pAttachments = atts.data();
  rpci.subpassCount = 1;
  rpci.pSubpasses = &sub;
  rpci.dependencyCount = 1;
  rpci.pDependencies = &dep;
  // Temporary format; recreate render pass after swapchain if needed — use B8G8R8A8_UNORM commonly
  if (vkCreateRenderPass(impl_->device, &rpci, nullptr, &impl_->render_pass) != VK_SUCCESS) {
    if (error) {
      *error = "render pass failed";
    }
    return false;
  }

  if (!impl_->create_swapchain()) {
    if (error) {
      *error = "swapchain failed";
    }
    return false;
  }
  // Fix color attachment format if needed
  (void)impl_->swap_format;

  VkCommandPoolCreateInfo pci{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
  pci.queueFamilyIndex = impl_->graphics_family;
  pci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  vkCreateCommandPool(impl_->device, &pci, nullptr, &impl_->cmd_pool);
  VkCommandBufferAllocateInfo cai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
  cai.commandPool = impl_->cmd_pool;
  cai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  cai.commandBufferCount = 1;
  vkAllocateCommandBuffers(impl_->device, &cai, &impl_->cmd);

  VkSemaphoreCreateInfo sci{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
  VkFenceCreateInfo fci{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
  fci.flags = VK_FENCE_CREATE_SIGNALED_BIT;
  vkCreateSemaphore(impl_->device, &sci, nullptr, &impl_->image_available);
  vkCreateSemaphore(impl_->device, &sci, nullptr, &impl_->render_finished);
  vkCreateFence(impl_->device, &fci, nullptr, &impl_->in_flight);

  VkDescriptorSetLayoutBinding binds[2]{};
  binds[0].binding = 0;
  binds[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  binds[0].descriptorCount = 1;
  binds[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
  binds[1].binding = 1;
  binds[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  binds[1].descriptorCount = 1;
  binds[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
  VkDescriptorSetLayoutCreateInfo dlci{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
  dlci.bindingCount = 2;
  dlci.pBindings = binds;
  vkCreateDescriptorSetLayout(impl_->device, &dlci, nullptr, &impl_->desc_layout);

  VkPipelineLayoutCreateInfo plci{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
  plci.setLayoutCount = 1;
  plci.pSetLayouts = &impl_->desc_layout;
  vkCreatePipelineLayout(impl_->device, &plci, nullptr, &impl_->pipeline_layout);

  auto vert_code = read_file(shader_dir / "mesh.vert.spv");
  auto frag_code = read_file(shader_dir / "mesh.frag.spv");
  if (vert_code.empty() || frag_code.empty()) {
    if (error) {
      *error = "missing shader SPIR-V in " + shader_dir.string();
    }
    return false;
  }
  VkShaderModule vert = make_shader(impl_->device, vert_code);
  VkShaderModule frag = make_shader(impl_->device, frag_code);

  VkPipelineShaderStageCreateInfo stages[2]{};
  stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
  stages[0].module = vert;
  stages[0].pName = "main";
  stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  stages[1].module = frag;
  stages[1].pName = "main";

  VkVertexInputBindingDescription bind{};
  bind.binding = 0;
  bind.stride = sizeof(MeshVertex);
  bind.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
  std::array<VkVertexInputAttributeDescription, 3> attrs{};
  attrs[0] = {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(MeshVertex, px)};
  attrs[1] = {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(MeshVertex, nx)};
  attrs[2] = {2, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(MeshVertex, u)};
  VkPipelineVertexInputStateCreateInfo vi{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
  vi.vertexBindingDescriptionCount = 1;
  vi.pVertexBindingDescriptions = &bind;
  vi.vertexAttributeDescriptionCount = uint32_t(attrs.size());
  vi.pVertexAttributeDescriptions = attrs.data();
  VkPipelineInputAssemblyStateCreateInfo ia{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
  ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  VkPipelineViewportStateCreateInfo vp{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
  vp.viewportCount = 1;
  vp.scissorCount = 1;
  VkPipelineRasterizationStateCreateInfo rs{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
  rs.polygonMode = VK_POLYGON_MODE_FILL;
  rs.cullMode = VK_CULL_MODE_NONE;
  rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
  rs.lineWidth = 1.f;
  VkPipelineMultisampleStateCreateInfo ms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
  ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
  VkPipelineDepthStencilStateCreateInfo ds{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
  ds.depthTestEnable = VK_TRUE;
  ds.depthWriteEnable = VK_TRUE;
  ds.depthCompareOp = VK_COMPARE_OP_LESS;
  VkPipelineColorBlendAttachmentState blend_att{};
  blend_att.colorWriteMask = 0xf;
  VkPipelineColorBlendStateCreateInfo blend{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
  blend.attachmentCount = 1;
  blend.pAttachments = &blend_att;
  std::array<VkDynamicState, 2> dyn_states{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
  VkPipelineDynamicStateCreateInfo dyn{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
  dyn.dynamicStateCount = uint32_t(dyn_states.size());
  dyn.pDynamicStates = dyn_states.data();
  VkGraphicsPipelineCreateInfo gci{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
  gci.stageCount = 2;
  gci.pStages = stages;
  gci.pVertexInputState = &vi;
  gci.pInputAssemblyState = &ia;
  gci.pViewportState = &vp;
  gci.pRasterizationState = &rs;
  gci.pMultisampleState = &ms;
  gci.pDepthStencilState = &ds;
  gci.pColorBlendState = &blend;
  gci.pDynamicState = &dyn;
  gci.layout = impl_->pipeline_layout;
  gci.renderPass = impl_->render_pass;
  if (vkCreateGraphicsPipelines(impl_->device, VK_NULL_HANDLE, 1, &gci, nullptr, &impl_->pipeline) != VK_SUCCESS) {
    if (error) {
      *error = "pipeline failed";
    }
    return false;
  }
  vkDestroyShaderModule(impl_->device, vert, nullptr);
  vkDestroyShaderModule(impl_->device, frag, nullptr);

  if (!impl_->create_buffer(sizeof(FrameUBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, impl_->ubo,
                            impl_->ubo_mem)) {
    if (error) {
      *error = "ubo failed";
    }
    return false;
  }
  vkMapMemory(impl_->device, impl_->ubo_mem, 0, sizeof(FrameUBO), 0, &impl_->ubo_mapped);

  ready_ = true;
  CATH_LOG_INFO("Vulkan renderer ready (%ux%u)", impl_->swap_extent.width, impl_->swap_extent.height);
  return true;
}

void VulkanRenderer::destroy_mesh_resources() {
  if (!impl_ || !impl_->device) {
    return;
  }
  vkDeviceWaitIdle(impl_->device);
  auto destroy_buf = [&](VkBuffer& b, VkDeviceMemory& m) {
    if (b) {
      vkDestroyBuffer(impl_->device, b, nullptr);
      b = VK_NULL_HANDLE;
    }
    if (m) {
      vkFreeMemory(impl_->device, m, nullptr);
      m = VK_NULL_HANDLE;
    }
  };
  destroy_buf(impl_->vbo, impl_->vbo_mem);
  destroy_buf(impl_->ibo, impl_->ibo_mem);
  if (impl_->sampler) {
    vkDestroySampler(impl_->device, impl_->sampler, nullptr);
    impl_->sampler = VK_NULL_HANDLE;
  }
  if (impl_->texture_view) {
    vkDestroyImageView(impl_->device, impl_->texture_view, nullptr);
    impl_->texture_view = VK_NULL_HANDLE;
  }
  if (impl_->texture) {
    vkDestroyImage(impl_->device, impl_->texture, nullptr);
    impl_->texture = VK_NULL_HANDLE;
  }
  if (impl_->texture_mem) {
    vkFreeMemory(impl_->device, impl_->texture_mem, nullptr);
    impl_->texture_mem = VK_NULL_HANDLE;
  }
  if (impl_->desc_pool) {
    vkDestroyDescriptorPool(impl_->device, impl_->desc_pool, nullptr);
    impl_->desc_pool = VK_NULL_HANDLE;
    impl_->desc_set = VK_NULL_HANDLE;
  }
  impl_->index_count = 0;
  impl_->tex_width = 0;
  impl_->tex_height = 0;
}

void VulkanRenderer::clear_geometry() { destroy_mesh_resources(); }

uint32_t VulkanRenderer::index_count() const { return impl_ ? impl_->index_count : 0; }

bool VulkanRenderer::upload_geometry(const Mesh& mesh, std::string* error) {
  const VkDeviceSize vsize = sizeof(MeshVertex) * mesh.vertices.size();
  const VkDeviceSize isize = sizeof(uint32_t) * mesh.indices.size();
  if (mesh.vertices.empty() || mesh.indices.empty()) {
    if (error) {
      *error = "empty mesh";
    }
    return false;
  }
  if (!impl_->create_buffer(vsize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, impl_->vbo,
                            impl_->vbo_mem) ||
      !impl_->create_buffer(isize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, impl_->ibo,
                            impl_->ibo_mem)) {
    if (error) {
      *error = "mesh buffers failed";
    }
    return false;
  }
  void* mapped = nullptr;
  vkMapMemory(impl_->device, impl_->vbo_mem, 0, vsize, 0, &mapped);
  std::memcpy(mapped, mesh.vertices.data(), vsize);
  vkUnmapMemory(impl_->device, impl_->vbo_mem);
  vkMapMemory(impl_->device, impl_->ibo_mem, 0, isize, 0, &mapped);
  std::memcpy(mapped, mesh.indices.data(), isize);
  vkUnmapMemory(impl_->device, impl_->ibo_mem);
  impl_->index_count = uint32_t(mesh.indices.size());
  return true;
}

bool VulkanRenderer::create_texture_from_image(const ImageRgba8& image, std::string* error) {
  if (image.width == 0 || image.height == 0 || image.pixels.size() < size_t(image.width) * image.height * 4) {
    if (error) {
      *error = "bad image";
    }
    return false;
  }
  VkImageCreateInfo ii{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
  ii.imageType = VK_IMAGE_TYPE_2D;
  ii.extent = {image.width, image.height, 1};
  ii.mipLevels = 1;
  ii.arrayLayers = 1;
  ii.format = VK_FORMAT_R8G8B8A8_UNORM;
  ii.tiling = VK_IMAGE_TILING_LINEAR;
  ii.initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
  ii.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
  ii.samples = VK_SAMPLE_COUNT_1_BIT;
  vkCreateImage(impl_->device, &ii, nullptr, &impl_->texture);
  VkMemoryRequirements req{};
  vkGetImageMemoryRequirements(impl_->device, impl_->texture, &req);
  VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
  ai.allocationSize = req.size;
  ai.memoryTypeIndex =
      impl_->find_memory_type(req.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  vkAllocateMemory(impl_->device, &ai, nullptr, &impl_->texture_mem);
  vkBindImageMemory(impl_->device, impl_->texture, impl_->texture_mem, 0);
  VkImageSubresource sub{VK_IMAGE_ASPECT_COLOR_BIT, 0, 0};
  VkSubresourceLayout layout{};
  vkGetImageSubresourceLayout(impl_->device, impl_->texture, &sub, &layout);
  void* tex_map = nullptr;
  vkMapMemory(impl_->device, impl_->texture_mem, 0, layout.size, 0, &tex_map);
  for (uint32_t y = 0; y < image.height; ++y) {
    std::memcpy(static_cast<uint8_t*>(tex_map) + layout.offset + y * layout.rowPitch,
                image.pixels.data() + size_t(y) * image.width * 4, size_t(image.width) * 4);
  }
  vkUnmapMemory(impl_->device, impl_->texture_mem);
  impl_->tex_width = image.width;
  impl_->tex_height = image.height;
  impl_->tex_row_pitch = layout.rowPitch;
  impl_->tex_layout_offset = layout.offset;

  VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
  begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  vkResetCommandBuffer(impl_->cmd, 0);
  vkBeginCommandBuffer(impl_->cmd, &begin);
  VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
  barrier.oldLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
  barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = impl_->texture;
  barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  barrier.subresourceRange.levelCount = 1;
  barrier.subresourceRange.layerCount = 1;
  barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
  barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
  vkCmdPipelineBarrier(impl_->cmd, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0,
                       nullptr, 1, &barrier);
  vkEndCommandBuffer(impl_->cmd);
  VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
  submit.commandBufferCount = 1;
  submit.pCommandBuffers = &impl_->cmd;
  vkQueueSubmit(impl_->graphics_queue, 1, &submit, VK_NULL_HANDLE);
  vkQueueWaitIdle(impl_->graphics_queue);

  VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
  vi.image = impl_->texture;
  vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
  vi.format = VK_FORMAT_R8G8B8A8_UNORM;
  vi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  vi.subresourceRange.levelCount = 1;
  vi.subresourceRange.layerCount = 1;
  vkCreateImageView(impl_->device, &vi, nullptr, &impl_->texture_view);

  VkSamplerCreateInfo sam{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
  sam.magFilter = VK_FILTER_LINEAR;
  sam.minFilter = VK_FILTER_LINEAR;
  sam.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  sam.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  sam.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  vkCreateSampler(impl_->device, &sam, nullptr, &impl_->sampler);

  VkDescriptorPoolSize sizes[2]{{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1}, {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1}};
  VkDescriptorPoolCreateInfo dpci{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
  dpci.maxSets = 1;
  dpci.poolSizeCount = 2;
  dpci.pPoolSizes = sizes;
  vkCreateDescriptorPool(impl_->device, &dpci, nullptr, &impl_->desc_pool);
  VkDescriptorSetAllocateInfo dsai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
  dsai.descriptorPool = impl_->desc_pool;
  dsai.descriptorSetCount = 1;
  dsai.pSetLayouts = &impl_->desc_layout;
  vkAllocateDescriptorSets(impl_->device, &dsai, &impl_->desc_set);

  VkDescriptorBufferInfo ubi{impl_->ubo, 0, sizeof(FrameUBO)};
  VkDescriptorImageInfo uii{impl_->sampler, impl_->texture_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
  VkWriteDescriptorSet writes[2]{};
  writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  writes[0].dstSet = impl_->desc_set;
  writes[0].dstBinding = 0;
  writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  writes[0].descriptorCount = 1;
  writes[0].pBufferInfo = &ubi;
  writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  writes[1].dstSet = impl_->desc_set;
  writes[1].dstBinding = 1;
  writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  writes[1].descriptorCount = 1;
  writes[1].pImageInfo = &uii;
  vkUpdateDescriptorSets(impl_->device, 2, writes, 0, nullptr);
  return true;
}

bool VulkanRenderer::upload_mesh(const Mesh& mesh, const ImageRgba8& image, std::string* error) {
  if (!ready_) {
    return false;
  }
  destroy_mesh_resources();
  if (!upload_geometry(mesh, error)) {
    return false;
  }
  return create_texture_from_image(image, error);
}

bool VulkanRenderer::upload_model(const Model& model, const ImageRgba8& image, std::string* error) {
  Mesh merged;
  merged.name = "merged";
  for (const auto& m : model.meshes) {
    const uint32_t base = uint32_t(merged.vertices.size());
    merged.vertices.insert(merged.vertices.end(), m.vertices.begin(), m.vertices.end());
    for (uint32_t idx : m.indices) {
      merged.indices.push_back(base + idx);
    }
  }
  return upload_mesh(merged, image, error);
}

bool VulkanRenderer::update_texture(const ImageRgba8& image, std::string* error) {
  if (!ready_ || !impl_->texture_mem) {
    return create_texture_from_image(image, error);
  }
  if (image.width != impl_->tex_width || image.height != impl_->tex_height) {
    // recreate
    if (impl_->sampler) {
      vkDestroySampler(impl_->device, impl_->sampler, nullptr);
      impl_->sampler = VK_NULL_HANDLE;
    }
    if (impl_->texture_view) {
      vkDestroyImageView(impl_->device, impl_->texture_view, nullptr);
      impl_->texture_view = VK_NULL_HANDLE;
    }
    if (impl_->texture) {
      vkDestroyImage(impl_->device, impl_->texture, nullptr);
      impl_->texture = VK_NULL_HANDLE;
    }
    if (impl_->texture_mem) {
      vkFreeMemory(impl_->device, impl_->texture_mem, nullptr);
      impl_->texture_mem = VK_NULL_HANDLE;
    }
    if (impl_->desc_pool) {
      vkDestroyDescriptorPool(impl_->device, impl_->desc_pool, nullptr);
      impl_->desc_pool = VK_NULL_HANDLE;
    }
    return create_texture_from_image(image, error);
  }
  void* tex_map = nullptr;
  vkMapMemory(impl_->device, impl_->texture_mem, 0, VK_WHOLE_SIZE, 0, &tex_map);
  for (uint32_t y = 0; y < image.height; ++y) {
    std::memcpy(static_cast<uint8_t*>(tex_map) + impl_->tex_layout_offset + y * impl_->tex_row_pitch,
                image.pixels.data() + size_t(y) * image.width * 4, size_t(image.width) * 4);
  }
  vkUnmapMemory(impl_->device, impl_->texture_mem);
  return true;
}

void VulkanRenderer::resize() {
  if (!ready_) {
    return;
  }
  vkDeviceWaitIdle(impl_->device);
  impl_->create_swapchain();
}

void VulkanRenderer::draw_clear(float r, float g, float b) {
  if (!ready_) {
    return;
  }
  vkWaitForFences(impl_->device, 1, &impl_->in_flight, VK_TRUE, UINT64_MAX);
  uint32_t image_index = 0;
  VkResult acq = vkAcquireNextImageKHR(impl_->device, impl_->swapchain, UINT64_MAX, impl_->image_available,
                                       VK_NULL_HANDLE, &image_index);
  if (acq == VK_ERROR_OUT_OF_DATE_KHR) {
    resize();
    return;
  }
  vkResetFences(impl_->device, 1, &impl_->in_flight);
  vkResetCommandBuffer(impl_->cmd, 0);
  VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
  vkBeginCommandBuffer(impl_->cmd, &begin);
  std::array<VkClearValue, 2> clears{};
  clears[0].color = {{r, g, b, 1.f}};
  clears[1].depthStencil = {1.f, 0};
  VkRenderPassBeginInfo rp{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
  rp.renderPass = impl_->render_pass;
  rp.framebuffer = impl_->framebuffers[image_index];
  rp.renderArea.extent = impl_->swap_extent;
  rp.clearValueCount = 2;
  rp.pClearValues = clears.data();
  vkCmdBeginRenderPass(impl_->cmd, &rp, VK_SUBPASS_CONTENTS_INLINE);
  vkCmdEndRenderPass(impl_->cmd);
  vkEndCommandBuffer(impl_->cmd);
  VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
  submit.waitSemaphoreCount = 1;
  submit.pWaitSemaphores = &impl_->image_available;
  submit.pWaitDstStageMask = &wait_stage;
  submit.commandBufferCount = 1;
  submit.pCommandBuffers = &impl_->cmd;
  submit.signalSemaphoreCount = 1;
  submit.pSignalSemaphores = &impl_->render_finished;
  vkQueueSubmit(impl_->graphics_queue, 1, &submit, impl_->in_flight);
  VkPresentInfoKHR present{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
  present.waitSemaphoreCount = 1;
  present.pWaitSemaphores = &impl_->render_finished;
  present.swapchainCount = 1;
  present.pSwapchains = &impl_->swapchain;
  present.pImageIndices = &image_index;
  vkQueuePresentKHR(impl_->present_queue, &present);
}

void VulkanRenderer::draw(const glm::mat4& mvp, const glm::mat4& model) {
  if (!ready_) {
    return;
  }
  if (impl_->index_count == 0 || !impl_->desc_set) {
    draw_clear(0.08f, 0.07f, 0.10f);
    return;
  }
  FrameUBO ubo{mvp, model};
  std::memcpy(impl_->ubo_mapped, &ubo, sizeof(ubo));

  vkWaitForFences(impl_->device, 1, &impl_->in_flight, VK_TRUE, UINT64_MAX);
  uint32_t image_index = 0;
  VkResult acq = vkAcquireNextImageKHR(impl_->device, impl_->swapchain, UINT64_MAX, impl_->image_available,
                                       VK_NULL_HANDLE, &image_index);
  if (acq == VK_ERROR_OUT_OF_DATE_KHR) {
    resize();
    return;
  }
  vkResetFences(impl_->device, 1, &impl_->in_flight);
  vkResetCommandBuffer(impl_->cmd, 0);
  VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
  vkBeginCommandBuffer(impl_->cmd, &begin);

  std::array<VkClearValue, 2> clears{};
  clears[0].color = {{0.18f, 0.08f, 0.14f, 1.f}};  // dark rose — readable behind lit meshes
  clears[1].depthStencil = {1.f, 0};
  VkRenderPassBeginInfo rp{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
  rp.renderPass = impl_->render_pass;
  rp.framebuffer = impl_->framebuffers[image_index];
  rp.renderArea.extent = impl_->swap_extent;
  rp.clearValueCount = 2;
  rp.pClearValues = clears.data();
  vkCmdBeginRenderPass(impl_->cmd, &rp, VK_SUBPASS_CONTENTS_INLINE);
  vkCmdBindPipeline(impl_->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, impl_->pipeline);
  VkViewport viewport{0, 0, float(impl_->swap_extent.width), float(impl_->swap_extent.height), 0.f, 1.f};
  VkRect2D scissor{{0, 0}, impl_->swap_extent};
  vkCmdSetViewport(impl_->cmd, 0, 1, &viewport);
  vkCmdSetScissor(impl_->cmd, 0, 1, &scissor);
  VkDeviceSize off = 0;
  vkCmdBindVertexBuffers(impl_->cmd, 0, 1, &impl_->vbo, &off);
  vkCmdBindIndexBuffer(impl_->cmd, impl_->ibo, 0, VK_INDEX_TYPE_UINT32);
  vkCmdBindDescriptorSets(impl_->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, impl_->pipeline_layout, 0, 1, &impl_->desc_set, 0,
                          nullptr);
  vkCmdDrawIndexed(impl_->cmd, impl_->index_count, 1, 0, 0, 0);
  vkCmdEndRenderPass(impl_->cmd);
  vkEndCommandBuffer(impl_->cmd);

  VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
  submit.waitSemaphoreCount = 1;
  submit.pWaitSemaphores = &impl_->image_available;
  submit.pWaitDstStageMask = &wait_stage;
  submit.commandBufferCount = 1;
  submit.pCommandBuffers = &impl_->cmd;
  submit.signalSemaphoreCount = 1;
  submit.pSignalSemaphores = &impl_->render_finished;
  vkQueueSubmit(impl_->graphics_queue, 1, &submit, impl_->in_flight);

  VkPresentInfoKHR present{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
  present.waitSemaphoreCount = 1;
  present.pWaitSemaphores = &impl_->render_finished;
  present.swapchainCount = 1;
  present.pSwapchains = &impl_->swapchain;
  present.pImageIndices = &image_index;
  VkResult pr = vkQueuePresentKHR(impl_->present_queue, &present);
  if (pr == VK_ERROR_OUT_OF_DATE_KHR || pr == VK_SUBOPTIMAL_KHR) {
    resize();
  }
}

void VulkanRenderer::shutdown() {
  if (!impl_) {
    return;
  }
  if (impl_->device) {
    vkDeviceWaitIdle(impl_->device);
  }
  auto& i = *impl_;
  auto destroy_buf = [&](VkBuffer& b, VkDeviceMemory& m) {
    if (b) {
      vkDestroyBuffer(i.device, b, nullptr);
      b = VK_NULL_HANDLE;
    }
    if (m) {
      vkFreeMemory(i.device, m, nullptr);
      m = VK_NULL_HANDLE;
    }
  };
  if (i.ubo_mapped) {
    vkUnmapMemory(i.device, i.ubo_mem);
    i.ubo_mapped = nullptr;
  }
  destroy_buf(i.vbo, i.vbo_mem);
  destroy_buf(i.ibo, i.ibo_mem);
  destroy_buf(i.ubo, i.ubo_mem);
  if (i.sampler) {
    vkDestroySampler(i.device, i.sampler, nullptr);
  }
  if (i.texture_view) {
    vkDestroyImageView(i.device, i.texture_view, nullptr);
  }
  if (i.texture) {
    vkDestroyImage(i.device, i.texture, nullptr);
  }
  if (i.texture_mem) {
    vkFreeMemory(i.device, i.texture_mem, nullptr);
  }
  if (i.desc_pool) {
    vkDestroyDescriptorPool(i.device, i.desc_pool, nullptr);
  }
  if (i.pipeline) {
    vkDestroyPipeline(i.device, i.pipeline, nullptr);
  }
  if (i.pipeline_layout) {
    vkDestroyPipelineLayout(i.device, i.pipeline_layout, nullptr);
  }
  if (i.desc_layout) {
    vkDestroyDescriptorSetLayout(i.device, i.desc_layout, nullptr);
  }
  if (i.image_available) {
    vkDestroySemaphore(i.device, i.image_available, nullptr);
  }
  if (i.render_finished) {
    vkDestroySemaphore(i.device, i.render_finished, nullptr);
  }
  if (i.in_flight) {
    vkDestroyFence(i.device, i.in_flight, nullptr);
  }
  if (i.cmd_pool) {
    vkDestroyCommandPool(i.device, i.cmd_pool, nullptr);
  }
  i.destroy_swapchain_objects();
  if (i.render_pass) {
    vkDestroyRenderPass(i.device, i.render_pass, nullptr);
  }
  if (i.device) {
    vkDestroyDevice(i.device, nullptr);
  }
  if (i.surface) {
    vkDestroySurfaceKHR(i.instance, i.surface, nullptr);
  }
  if (i.instance) {
    vkDestroyInstance(i.instance, nullptr);
  }
  delete impl_;
  impl_ = nullptr;
  ready_ = false;
}

}  // namespace cath
