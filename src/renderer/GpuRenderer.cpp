#include "GpuRenderer.h"

#include <sstream>
#define VK_USE_PLATFORM_WIN32_KHR
#include <windows.h>
#include <vulkan/vulkan.h>
#include "../vr/VrRuntime.h"
#include <cstring>
#include <stdexcept>
#include <sstream>
#include <limits>
#include <bit>
#include <cmath>
#include <algorithm>
namespace retro {
namespace {
void check(VkResult result,const char* operation){if(result!=VK_SUCCESS)throw std::runtime_error(std::string(operation)+" (Vulkan "+std::to_string(result)+")");}
const uint32_t vertexShader[]=
#include "scene.vert.inc"
;
const uint32_t fragmentShader[]=
#include "scene.frag.inc"
;
const uint32_t compositeVertexShader[]=
#include "composite.vert.inc"
;
const uint32_t compositeFragmentShader[]=
#include "composite.frag.inc"
;
struct Vertex {float position[4],uv[2],lighting[4],light0[4],light1[4],surface[4],worldNormal[4];};
struct Buffer {VkBuffer handle=VK_NULL_HANDLE;VkDeviceMemory memory=VK_NULL_HANDLE;void* mapped=nullptr;VkDeviceSize size=0;};
struct Image {VkImage handle=VK_NULL_HANDLE;VkImageView view=VK_NULL_HANDLE;VkDeviceMemory memory=VK_NULL_HANDLE;};
struct EmissionLevel {int width,height;std::vector<std::array<float,4>> pixels;};
// Decode before filtering: averaging encoded phosphor values darkens small CRT
// text, and decoding after bilinear interpolation does not conserve radiance.
std::vector<EmissionLevel> emissionMips(int width,int height,const std::vector<uint32_t>& pixels){
 if(width<1||height<1||pixels.size()!=size_t(width)*height)throw std::runtime_error("Invalid emission atlas dimensions");
 static const auto linear=[] {std::array<float,256> values{};for(size_t i=0;i<values.size();++i)values[i]=std::pow(float(i)/255.f,2.2f);return values;}();
 std::vector<EmissionLevel> result;result.push_back({width,height,{}});auto&base=result.back().pixels;base.reserve(pixels.size());
 for(auto p:pixels)base.push_back({linear[(p>>16)&255],linear[(p>>8)&255],linear[p&255],float(p>>24)/255.f});
 while(result.back().width>1||result.back().height>1){
  const auto&previous=result.back();int w=previous.width,h=previous.height,nw=std::max(1,w/2),nh=std::max(1,h/2);EmissionLevel next{nw,nh,std::vector<std::array<float,4>>(size_t(nw)*nh)};
  // Equal-area footprints include odd last rows/columns. Integer overlap
  // weights avoid boundary rounding and preserve the mean at every level.
  for(int y=0;y<nh;++y)for(int x=0;x<nw;++x){std::array<double,4> sum{};int left=x*w,right=(x+1)*w,top=y*h,bottom=(y+1)*h;
   for(int sy=top/nh;sy<(bottom+nh-1)/nh;++sy)for(int sx=left/nw;sx<(right+nw-1)/nw;++sx){
    int wx=std::min((sx+1)*nw,right)-std::max(sx*nw,left),wy=std::min((sy+1)*nh,bottom)-std::max(sy*nh,top);double weight=double(wx)*wy;
    const auto&sample=previous.pixels[size_t(sy*w+sx)];for(size_t c=0;c<sum.size();++c)sum[c]+=sample[c]*weight;
   }
   auto&sample=next.pixels[size_t(y*nw+x)];for(size_t c=0;c<sum.size();++c)sample[c]=float(sum[c]/(double(w)*h));
  }
  result.push_back(std::move(next));
 }
 return result;
}
uint16_t emissionHalf(float value){
 uint32_t bits=std::bit_cast<uint32_t>(value),sign=(bits>>16)&0x8000u,mantissa=bits&0x7fffffu;int exponent=int((bits>>23)&255)-127+15;
 if(exponent<=0){if(exponent<-10)return uint16_t(sign);mantissa|=0x800000u;int shift=14-exponent;uint32_t rounded=mantissa>>shift,remainder=mantissa&((1u<<shift)-1),tie=1u<<(shift-1);if(remainder>tie||(remainder==tie&&(rounded&1)))++rounded;return uint16_t(sign|rounded);}
 if(exponent>=31)return uint16_t(sign|0x7c00u|(mantissa?0x200u:0u));
 uint32_t rounded=mantissa>>13,remainder=mantissa&0x1fffu;if(remainder>0x1000u||(remainder==0x1000u&&(rounded&1)))++rounded;
 if(rounded==0x400u){rounded=0;++exponent;}return uint16_t(sign|(uint32_t(exponent)<<10)|rounded);
}
float emissionFloat(uint16_t bits){
 float value=(bits&0x7c00u)==0?std::ldexp(float(bits&0x3ffu),-24):std::ldexp(1.f+float(bits&0x3ffu)/1024.f,int((bits>>10)&31)-15);return (bits&0x8000u)?-value:value;
}
std::vector<std::vector<uint8_t>> emissionBytes(const std::vector<EmissionLevel>& levels){
 std::vector<std::vector<uint8_t>> result;result.reserve(levels.size());
 for(const auto&level:levels){std::vector<uint8_t> bytes(level.pixels.size()*8);for(size_t i=0;i<level.pixels.size();++i)for(size_t c=0;c<4;++c){auto half=emissionHalf(level.pixels[i][c]);std::memcpy(bytes.data()+i*8+c*2,&half,2);}result.push_back(std::move(bytes));}
 return result;
}
}
bool GpuRenderer::testEmissionMips(){
 for(auto size:{std::array<int,2>{1,1},{3,5},{5,3},{1,7},{7,1},{9,7},{32,32},{348,307}}){
  int w=size[0],h=size[1];std::vector<uint32_t> source(size_t(w)*h);std::array<double,4> expected{};
  for(size_t i=0;i<source.size();++i){uint32_t r=uint32_t((i*73+17)%256),g=uint32_t((i*19+63)%256),b=uint32_t((i*131+241)%256),a=uint32_t((i*7+29)%256);source[i]=(a<<24)|(r<<16)|(g<<8)|b;
   expected[0]+=std::pow(double(r)/255.,2.2);expected[1]+=std::pow(double(g)/255.,2.2);expected[2]+=std::pow(double(b)/255.,2.2);expected[3]+=double(a)/255.;
  }
  for(auto&channel:expected)channel/=source.size();auto levels=emissionMips(w,h,source);auto bytes=emissionBytes(levels);
  for(size_t mip=0;mip<levels.size();++mip){const auto&level=levels[mip];if(level.width!=std::max(1,w>>mip)||level.height!=std::max(1,h>>mip)||level.pixels.size()!=size_t(level.width)*level.height||bytes[mip].size()!=level.pixels.size()*8)return false;
   std::array<double,4> average{},uploaded{};
   for(size_t i=0;i<level.pixels.size();++i)for(size_t c=0;c<4;++c){float value=level.pixels[i][c];uint16_t half=0;std::memcpy(&half,bytes[mip].data()+i*8+c*2,2);float decoded=emissionFloat(half);
    if(!std::isfinite(value)||value<0||value>1||!std::isfinite(decoded)||std::fabs(value-decoded)>.00025f)return false;average[c]+=value;uploaded[c]+=decoded;
   }
   for(size_t c=0;c<4;++c)if(std::fabs(average[c]/level.pixels.size()-expected[c])>.000002||std::fabs(uploaded[c]/level.pixels.size()-expected[c])>.00025)return false;
  }
  if(levels.back().width!=1||levels.back().height!=1)return false;
 }
 // An odd edge texel must survive all the way to 1x1 instead of being dropped.
 std::vector<uint32_t> edge(15,0xff000000u);edge.back()=0xffffffffu;auto odd=emissionMips(3,5,edge);
 for(const auto&level:odd){double mean=0;for(const auto&p:level.pixels)mean+=p[0];if(std::fabs(mean/level.pixels.size()-1./15.)>.000001)return false;}
 auto black=emissionMips(7,5,std::vector<uint32_t>(35,0xff000000u));for(const auto&level:black)for(const auto&p:level.pixels)if(p[0]!=0||p[1]!=0||p[2]!=0||p[3]!=1)return false;
 // Include IEEE half subnormals and round-to-nearest-even ties used by dim logs.
 if(emissionHalf(0)!=0||emissionHalf(1)!=0x3c00||emissionHalf(std::ldexp(1.f,-24))!=1||emissionHalf(std::ldexp(1.f,-25))!=0||emissionHalf(1.f+std::ldexp(1.f,-11))!=0x3c00||emissionHalf(1.f+3*std::ldexp(1.f,-11))!=0x3c02)return false;
 return true;
}
struct GpuRenderer::Impl {
 VkInstance instance=VK_NULL_HANDLE;VkPhysicalDevice physical=VK_NULL_HANDLE;VkDevice device=VK_NULL_HANDLE;VkQueue queue=VK_NULL_HANDLE;uint32_t family=0;
 HWND hwnd=nullptr;VkSurfaceKHR surface=VK_NULL_HANDLE;VkSwapchainKHR swapchain=VK_NULL_HANDLE;VkFormat swapFormat=VK_FORMAT_UNDEFINED;VkExtent2D swapExtent{};std::vector<VkImage> swapImages;std::vector<VkImageView> swapViews;std::vector<VkFramebuffer> swapFrames;std::vector<VkSemaphore> readySemaphores;VkRenderPass compositePass=VK_NULL_HANDLE;VkDescriptorSetLayout compositeSetLayout=VK_NULL_HANDLE;VkPipelineLayout compositeLayout=VK_NULL_HANDLE;VkPipeline compositePipeline=VK_NULL_HANDLE;VkDescriptorPool compositeDescriptors=VK_NULL_HANDLE;Image overlay;bool overlayInitialized=false;int overlayWidth=0,overlayHeight=0;
 int surfaceClientWidth=0,surfaceClientHeight=0;
 VkExtent2D mirrorExtent{};VkRenderPass mirrorPass=VK_NULL_HANDLE;VkPipeline mirrorPipeline=VK_NULL_HANDLE;std::vector<VkImageView> mirrorViews;std::vector<VkFramebuffer> mirrorFrames;std::uint64_t mirrorPresented=0;
 bool vrMode=false;int vrEye=0;std::array<Image,2> vrOutputs{};std::array<float,20> vrProjection{};VkDescriptorSetLayout projectionLayout=VK_NULL_HANDLE;VkDescriptorPool projectionPool=VK_NULL_HANDLE;std::array<VkDescriptorSet,2> projectionSets{};std::array<Buffer,2> projectionBuffers{};
 VkCommandPool pool=VK_NULL_HANDLE;
 // Two frames in flight: CPU prepares frame N while GPU renders frame N-1.
 // Each slot owns its command buffer, fence, dynamic vertex buffer, overlay
 // staging buffer and swapchain-acquire semaphore so neither frame races
 // the other for write access to these mutable resources.
 static constexpr int FrameCount=2;
 int frameIndex=0;
 std::array<VkCommandBuffer,FrameCount> commands{};
 std::array<VkFence,FrameCount> fences{};
 std::array<bool,FrameCount> frameInFlight{};
 std::array<Buffer,FrameCount> vertexBuffers{};
 std::array<std::vector<Buffer>,FrameCount> retiredBuffers{};
 std::array<Buffer,FrameCount> overlayBuffers{};
 std::array<VkSemaphore,FrameCount> acquiredSems{};
 // Per-frame composite descriptor sets (each frame references the same shared
 // render target images, but we need separate sets so we can update them while
 // the other frame's set is still in use by the GPU).
 std::array<VkDescriptorSet,FrameCount> compositeSets{};
 // Dedicated fence + command buffer for one-shot uploads (texture streaming).
 // These are always waited on immediately and never overlap with frame work.
 VkCommandBuffer uploadCommand=VK_NULL_HANDLE;VkFence uploadFence=VK_NULL_HANDLE;
 VkDescriptorSetLayout setLayout=VK_NULL_HANDLE;VkDescriptorPool descriptors=VK_NULL_HANDLE;
 VkPipelineLayout pipelineLayout=VK_NULL_HANDLE;VkRenderPass renderPass=VK_NULL_HANDLE;VkPipeline opaque=VK_NULL_HANDLE,additive=VK_NULL_HANDLE,transparent=VK_NULL_HANDLE;
 VkSampler wrap=VK_NULL_HANDLE,clamp=VK_NULL_HANDLE,sceneSampler=VK_NULL_HANDLE,pointSampler=VK_NULL_HANDLE;VkFramebuffer framebuffer=VK_NULL_HANDLE;
 Image target,sceneColor,depth,blackEmission;Buffer readback,dynamicUpload;
 struct Material {Image color,normal,emission,relief;VkDescriptorSet set=VK_NULL_HANDLE;bool additive=false,transparent=false;uint32_t sortKey=0;};
 std::unordered_map<uint64_t,Material> materials;
 const void* lastPixels=nullptr;uint64_t lastGeneration=0,lastMaterial=0;uint32_t lastSort=0;
 struct Batch {uint64_t material;uint32_t start,count;bool clear=false,cache=false;int cacheSlot=-1;uint32_t sortKey=0;};
 struct StaticCache {uint64_t key=0;Buffer buffer;std::vector<Vertex> vertices;std::vector<Batch> batches;};
 std::unordered_map<int,StaticCache> staticCaches;int captureSlot=-1;bool capturing=false,renderingViewmodel=false;std::array<float,32> viewState{};std::uint64_t cacheHits=0;
 std::vector<Vertex> vertices;std::vector<Batch> batches;
 std::vector<Vertex> sortedVertices;std::vector<Batch> sortedBatches;
 int width=0,height=0;VkFormat sceneFormat=VK_FORMAT_B8G8R8A8_UNORM;VkSampleCountFlagBits sceneSamples=VK_SAMPLE_COUNT_1_BIT;bool hdrScene=false;std::string name;
 ~Impl(){
  if(device){vkDeviceWaitIdle(device);if(vrMode&&VrRuntime::active())VrRuntime::active()->shutdownGraphics();if(framebuffer)vkDestroyFramebuffer(device,framebuffer,nullptr);
   for(auto f:swapFrames)vkDestroyFramebuffer(device,f,nullptr);for(auto v:swapViews)vkDestroyImageView(device,v,nullptr);for(auto&i:vrOutputs){i.view=VK_NULL_HANDLE;destroy(i);}if(swapchain)vkDestroySwapchainKHR(device,swapchain,nullptr);
   for(auto f:mirrorFrames)vkDestroyFramebuffer(device,f,nullptr);for(auto v:mirrorViews)vkDestroyImageView(device,v,nullptr);if(mirrorPipeline)vkDestroyPipeline(device,mirrorPipeline,nullptr);if(mirrorPass)vkDestroyRenderPass(device,mirrorPass,nullptr);
   for(auto&b:projectionBuffers)destroy(b);if(projectionPool)vkDestroyDescriptorPool(device,projectionPool,nullptr);if(projectionLayout)vkDestroyDescriptorSetLayout(device,projectionLayout,nullptr);
   destroy(target);destroy(sceneColor);destroy(depth);destroy(blackEmission);destroy(overlay);destroy(readback);destroy(dynamicUpload);
   for(int i=0;i<FrameCount;++i){destroy(vertexBuffers[i]);destroy(overlayBuffers[i]);}
   for(auto&entry:staticCaches)destroy(entry.second.buffer);
   for(auto&slot:retiredBuffers)for(auto&buffer:slot)destroy(buffer);
   for(auto&entry:materials){destroy(entry.second.color);destroy(entry.second.normal);destroy(entry.second.emission);destroy(entry.second.relief);}
   if(transparent)vkDestroyPipeline(device,transparent,nullptr);if(opaque)vkDestroyPipeline(device,opaque,nullptr);if(additive)vkDestroyPipeline(device,additive,nullptr);
   if(compositePipeline)vkDestroyPipeline(device,compositePipeline,nullptr);if(compositePass)vkDestroyRenderPass(device,compositePass,nullptr);if(compositeLayout)vkDestroyPipelineLayout(device,compositeLayout,nullptr);if(compositeSetLayout)vkDestroyDescriptorSetLayout(device,compositeSetLayout,nullptr);if(compositeDescriptors)vkDestroyDescriptorPool(device,compositeDescriptors,nullptr);if(renderPass)vkDestroyRenderPass(device,renderPass,nullptr);if(pipelineLayout)vkDestroyPipelineLayout(device,pipelineLayout,nullptr);
   if(descriptors)vkDestroyDescriptorPool(device,descriptors,nullptr);if(setLayout)vkDestroyDescriptorSetLayout(device,setLayout,nullptr);
   if(wrap)vkDestroySampler(device,wrap,nullptr);if(clamp)vkDestroySampler(device,clamp,nullptr);if(sceneSampler)vkDestroySampler(device,sceneSampler,nullptr);if(pointSampler)vkDestroySampler(device,pointSampler,nullptr);
   for(auto sem:readySemaphores)vkDestroySemaphore(device,sem,nullptr);
   for(int i=0;i<FrameCount;++i){if(acquiredSems[i])vkDestroySemaphore(device,acquiredSems[i],nullptr);if(fences[i])vkDestroyFence(device,fences[i],nullptr);}
   if(uploadFence)vkDestroyFence(device,uploadFence,nullptr);
   if(pool)vkDestroyCommandPool(device,pool,nullptr);vkDestroyDevice(device,nullptr);
  }if(surface&&instance)vkDestroySurfaceKHR(instance,surface,nullptr);if(instance)vkDestroyInstance(instance,nullptr);
 }
 void destroy(Buffer&b){if(b.mapped)vkUnmapMemory(device,b.memory);if(b.handle)vkDestroyBuffer(device,b.handle,nullptr);if(b.memory)vkFreeMemory(device,b.memory,nullptr);b={};}
 // Retire geometry with this frame slot, never while the previous frame reads it.
 void retire(Buffer&b){if(b.handle){retiredBuffers[frameIndex].push_back(b);b={};}}
 void destroy(Image&i){if(i.view)vkDestroyImageView(device,i.view,nullptr);if(i.handle)vkDestroyImage(device,i.handle,nullptr);if(i.memory)vkFreeMemory(device,i.memory,nullptr);i={};}
 uint32_t memoryType(uint32_t bits,VkMemoryPropertyFlags flags,VkMemoryPropertyFlags preferred=0){VkPhysicalDeviceMemoryProperties props;vkGetPhysicalDeviceMemoryProperties(physical,&props);if(preferred)for(uint32_t i=0;i<props.memoryTypeCount;++i)if((bits&(1u<<i))&&(props.memoryTypes[i].propertyFlags&(flags|preferred))==(flags|preferred))return i;for(uint32_t i=0;i<props.memoryTypeCount;++i)if((bits&(1u<<i))&&(props.memoryTypes[i].propertyFlags&flags)==flags)return i;throw std::runtime_error("No compatible Vulkan memory type");}
 void makeBuffer(Buffer&b,VkDeviceSize size,VkBufferUsageFlags usage){
  VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};info.size=size;info.usage=usage;info.sharingMode=VK_SHARING_MODE_EXCLUSIVE;check(vkCreateBuffer(device,&info,nullptr,&b.handle),"Create buffer");
  VkMemoryRequirements req;vkGetBufferMemoryRequirements(device,b.handle,&req);VkMemoryAllocateInfo alloc{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};alloc.allocationSize=req.size;alloc.memoryTypeIndex=memoryType(req.memoryTypeBits,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,(usage&VK_BUFFER_USAGE_VERTEX_BUFFER_BIT)?VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT:0);
  check(vkAllocateMemory(device,&alloc,nullptr,&b.memory),"Allocate mapped buffer");check(vkBindBufferMemory(device,b.handle,b.memory,0),"Bind buffer");check(vkMapMemory(device,b.memory,0,VK_WHOLE_SIZE,0,&b.mapped),"Map buffer");b.size=size;
 }
 void makeImage(Image&i,int w,int h,int levels,VkFormat format,VkImageUsageFlags usage,VkImageAspectFlags aspect,VkSampleCountFlagBits samples=VK_SAMPLE_COUNT_1_BIT){
  VkImageCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};info.imageType=VK_IMAGE_TYPE_2D;info.format=format;info.extent={uint32_t(w),uint32_t(h),1};info.mipLevels=levels;info.arrayLayers=1;info.samples=samples;info.tiling=VK_IMAGE_TILING_OPTIMAL;info.usage=usage;info.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
  check(vkCreateImage(device,&info,nullptr,&i.handle),"Create image");VkMemoryRequirements req;vkGetImageMemoryRequirements(device,i.handle,&req);VkMemoryAllocateInfo alloc{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};alloc.allocationSize=req.size;alloc.memoryTypeIndex=memoryType(req.memoryTypeBits,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  check(vkAllocateMemory(device,&alloc,nullptr,&i.memory),"Allocate image");check(vkBindImageMemory(device,i.handle,i.memory,0),"Bind image");
  VkImageViewCreateInfo view{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};view.image=i.handle;view.viewType=VK_IMAGE_VIEW_TYPE_2D;view.format=format;view.subresourceRange={aspect,0,uint32_t(levels),0,1};check(vkCreateImageView(device,&view,nullptr,&i.view),"Create image view");
 }
 // ── Per-frame command recording ─────────────────────────────────────────────
 void startCommands(){
  auto cmd=commands[frameIndex];
  check(vkResetCommandBuffer(cmd,0),"Reset command buffer");
  VkCommandBufferBeginInfo info{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
  info.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  check(vkBeginCommandBuffer(cmd,&info),"Begin commands");
 }
 // Submit current frame async — no wait. GPU runs in parallel with next frame's CPU work.
 void submitCommandsAsync(VkSemaphore waitSem=VK_NULL_HANDLE,VkSemaphore signalSem=VK_NULL_HANDLE){
  auto cmd=commands[frameIndex];auto f=fences[frameIndex];
  check(vkEndCommandBuffer(cmd),"End commands");
  check(vkResetFences(device,1,&f),"Reset fence");
  VkSubmitInfo info{VK_STRUCTURE_TYPE_SUBMIT_INFO};info.commandBufferCount=1;info.pCommandBuffers=&cmd;
  VkPipelineStageFlags waitStage=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  if(waitSem!=VK_NULL_HANDLE){info.waitSemaphoreCount=1;info.pWaitSemaphores=&waitSem;info.pWaitDstStageMask=&waitStage;}
  if(signalSem!=VK_NULL_HANDLE){info.signalSemaphoreCount=1;info.pSignalSemaphores=&signalSem;}
  check(vkQueueSubmit(queue,1,&info,f),"Submit graphics work");
  frameInFlight[frameIndex]=true;
 }
 // ── One-shot blocking upload (texture streaming only) ────────────────────
 void startUploadCommands(){
  check(vkResetCommandBuffer(uploadCommand,0),"Reset upload command buffer");
  VkCommandBufferBeginInfo info{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
  info.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  check(vkBeginCommandBuffer(uploadCommand,&info),"Begin upload commands");
 }
 void submitUploadCommands(){
  check(vkEndCommandBuffer(uploadCommand),"End upload commands");
  check(vkResetFences(device,1,&uploadFence),"Reset upload fence");
  VkSubmitInfo info{VK_STRUCTURE_TYPE_SUBMIT_INFO};info.commandBufferCount=1;info.pCommandBuffers=&uploadCommand;
  check(vkQueueSubmit(queue,1,&info,uploadFence),"Submit upload work");
  check(vkWaitForFences(device,1,&uploadFence,VK_TRUE,5000000000ull),"Wait for upload");
 }
 // ── Barrier helper — works on whichever command buffer is active ──────────
 void barrier(Image&i,VkImageLayout from,VkImageLayout to,VkAccessFlags source,VkAccessFlags dest,VkPipelineStageFlags sourceStage,VkPipelineStageFlags destStage,uint32_t levels=1,VkCommandBuffer cmd=VK_NULL_HANDLE){
  if(cmd==VK_NULL_HANDLE)cmd=commands[frameIndex];
  VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};b.srcAccessMask=source;b.dstAccessMask=dest;b.oldLayout=from;b.newLayout=to;b.srcQueueFamilyIndex=b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;b.image=i.handle;b.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,levels,0,1};
  vkCmdPipelineBarrier(cmd,sourceStage,destStage,0,0,nullptr,0,nullptr,1,&b);
 }
 // ── Texture upload (blocking, uses dedicated upload command buffer) ───────
 void upload(Image&i,int w,int h,VkFormat format,const std::vector<std::vector<uint8_t>>& levels,int pixelSize){
  makeImage(i,w,h,int(levels.size()),format,VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_SAMPLED_BIT,VK_IMAGE_ASPECT_COLOR_BIT);
  size_t size=0;for(auto&level:levels)size+=level.size();Buffer staging;
  try{makeBuffer(staging,size,VK_BUFFER_USAGE_TRANSFER_SRC_BIT);std::vector<VkBufferImageCopy> copies;size_t offset=0;
   for(size_t level=0;level<levels.size();++level){std::memcpy(static_cast<char*>(staging.mapped)+offset,levels[level].data(),levels[level].size());VkBufferImageCopy copy{};copy.bufferOffset=offset;copy.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,uint32_t(level),0,1};copy.imageExtent={uint32_t(std::max(1,w>>int(level))),uint32_t(std::max(1,h>>int(level))),1};copies.push_back(copy);offset+=levels[level].size();}
   startUploadCommands();
   barrier(i,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,0,VK_ACCESS_TRANSFER_WRITE_BIT,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,uint32_t(levels.size()),uploadCommand);
   vkCmdCopyBufferToImage(uploadCommand,staging.handle,i.handle,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,uint32_t(copies.size()),copies.data());
   barrier(i,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,uint32_t(levels.size()),uploadCommand);
   submitUploadCommands();
  }catch(...){destroy(staging);throw;}destroy(staging);(void)pixelSize;
 }
 uint64_t material(const SoftwareRenderer::Texture&t){
  // Plain constant colours share one image. Atlas keys use a lifetime token,
  // never an allocator address that can later belong to different maps.
  // Constant keys are odd and generation keys even, so the domains cannot alias.
  uint64_t key=t.pixels.size()==1&&t.normalLevels.empty()&&t.emission.empty()&&t.relief.empty()&&!t.additive&&!t.transparent?(uint64_t(t.pixels[0])<<32)|0xffffffffu:t.generation.value<<1;auto found=materials.find(key);if(found!=materials.end())return key;
  auto&mat=materials[key];mat.additive=t.additive;mat.transparent=t.transparent;
  mat.sortKey=(mat.transparent?2u:mat.additive?1u:0u)<<24|uint32_t(materials.size()&0xffffffu);
  auto bytes=[](const auto&values){std::vector<uint8_t> result(values.size()*sizeof(values[0]));std::memcpy(result.data(),values.data(),result.size());return result;};
  std::vector<std::vector<uint8_t>> levels{bytes(t.pixels)};for(auto&level:t.mips)levels.push_back(bytes(level));upload(mat.color,t.width,t.height,VK_FORMAT_B8G8R8A8_UNORM,levels,4);
  if(!t.normalLevels.empty()){
   // Preserve the length lost when normal-map mips normalize their average.
   // Alpha carries that concentration, allowing GGX to filter subpixel relief
   // without another texture or descriptor lookup.
   levels.clear();std::vector<float> previousLengths;int previousWidth=t.width,previousHeight=t.height;
   for(size_t mip=0;mip<t.normalLevels.size();++mip){const auto&level=t.normalLevels[mip];int mipWidth=std::max(1,t.width>>mip),mipHeight=std::max(1,t.height>>mip);
    std::vector<float> lengths(level.size(),1.f);
    if(mip){const auto&previous=t.normalLevels[mip-1];for(int y=0;y<mipHeight;++y)for(int x=0;x<mipWidth;++x){Point3 average{};
     for(int j=0;j<2;++j)for(int i=0;i<2;++i){size_t index=size_t(std::min(previousHeight-1,y*2+j)*previousWidth+std::min(previousWidth-1,x*2+i));average=average+previous[index]*previousLengths[index];}
     average=average*.25f;lengths[size_t(y*mipWidth+x)]=std::clamp(std::sqrt(average.x*average.x+average.y*average.y+average.z*average.z),0.f,1.f);
    }}
    std::vector<std::array<int16_t,4>> normals;normals.reserve(level.size());for(size_t i=0;i<level.size();++i){auto n=level[i];normals.push_back({int16_t(std::clamp(n.x,-1.f,1.f)*32767),int16_t(std::clamp(n.y,-1.f,1.f)*32767),int16_t(std::clamp(n.z,-1.f,1.f)*32767),int16_t(lengths[i]*32767)});}
    levels.push_back(bytes(normals));previousLengths=std::move(lengths);previousWidth=mipWidth;previousHeight=mipHeight;
   }
   upload(mat.normal,t.width,t.height,VK_FORMAT_R16G16B16A16_SNORM,levels,8);
  }
  if(!t.emission.empty())upload(mat.emission,t.width,t.height,VK_FORMAT_R16G16B16A16_SFLOAT,emissionBytes(emissionMips(t.width,t.height,t.emission)),8);
  else if(!blackEmission.view)upload(blackEmission,1,1,VK_FORMAT_R16G16B16A16_SFLOAT,emissionBytes(emissionMips(1,1,{0xff000000u})),8);
  if(!t.relief.empty())upload(mat.relief,t.width,t.height,VK_FORMAT_R8_UNORM,{bytes(t.relief)},1);
  VkDescriptorSetAllocateInfo alloc{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};alloc.descriptorPool=descriptors;alloc.descriptorSetCount=1;alloc.pSetLayouts=&setLayout;check(vkAllocateDescriptorSets(device,&alloc,&mat.set),"Allocate material descriptors");
  VkDescriptorImageInfo images[]={{t.clampEdges?clamp:wrap,mat.color.view,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},{wrap,mat.normal.view?mat.normal.view:mat.color.view,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},{t.clampEdges?clamp:wrap,mat.emission.view?mat.emission.view:blackEmission.view,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},{wrap,mat.relief.view?mat.relief.view:mat.color.view,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL}};
  VkWriteDescriptorSet writes[4]{};for(uint32_t n=0;n<4;++n){writes[n].sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;writes[n].dstSet=mat.set;writes[n].dstBinding=n;writes[n].descriptorCount=1;writes[n].descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;writes[n].pImageInfo=&images[n];}vkUpdateDescriptorSets(device,4,writes,0,nullptr);return key;
 }
 // The desktop spectator uses the existing right-eye HDR target and HUD.
 // This is one fullscreen GPU composite, with no third scene render or readback.
 void initMirror(){
  check(vkDeviceWaitIdle(device),"Wait for spectator resize");
  for(auto f:mirrorFrames)vkDestroyFramebuffer(device,f,nullptr);mirrorFrames.clear();for(auto v:mirrorViews)vkDestroyImageView(device,v,nullptr);mirrorViews.clear();for(auto sem:readySemaphores)vkDestroySemaphore(device,sem,nullptr);readySemaphores.clear();swapImages.clear();if(swapchain){vkDestroySwapchainKHR(device,swapchain,nullptr);swapchain=VK_NULL_HANDLE;}
  VkSurfaceCapabilitiesKHR caps{};check(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical,surface,&caps),"Query spectator surface");uint32_t n=0;check(vkGetPhysicalDeviceSurfaceFormatsKHR(physical,surface,&n,nullptr),"Query spectator formats");std::vector<VkSurfaceFormatKHR> formats(n);check(vkGetPhysicalDeviceSurfaceFormatsKHR(physical,surface,&n,formats.data()),"Query spectator formats");auto chosen=formats.front();for(auto f:formats)if(f.format==VK_FORMAT_B8G8R8A8_UNORM){chosen=f;break;}
  RECT rect{};GetClientRect(hwnd,&rect);surfaceClientWidth=rect.right;surfaceClientHeight=rect.bottom;mirrorExtent=caps.currentExtent.width!=UINT32_MAX?caps.currentExtent:VkExtent2D{uint32_t(std::max<LONG>(1,rect.right)),uint32_t(std::max<LONG>(1,rect.bottom))};mirrorExtent.width=std::clamp(mirrorExtent.width,caps.minImageExtent.width,caps.maxImageExtent.width);mirrorExtent.height=std::clamp(mirrorExtent.height,caps.minImageExtent.height,caps.maxImageExtent.height);
  uint32_t imageCount=std::max(caps.minImageCount+1,2u);if(caps.maxImageCount)imageCount=std::min(imageCount,caps.maxImageCount);
  uint32_t count=0;vkGetPhysicalDeviceSurfacePresentModesKHR(physical,surface,&count,nullptr);std::vector<VkPresentModeKHR> modes(count);vkGetPhysicalDeviceSurfacePresentModesKHR(physical,surface,&count,modes.data());VkPresentModeKHR mode=VK_PRESENT_MODE_FIFO_KHR;for(auto value:modes)if(value==VK_PRESENT_MODE_MAILBOX_KHR)mode=value;for(auto value:modes)if(value==VK_PRESENT_MODE_IMMEDIATE_KHR)mode=value;
  VkSwapchainCreateInfoKHR sc{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};sc.surface=surface;sc.minImageCount=imageCount;sc.imageFormat=chosen.format;sc.imageColorSpace=chosen.colorSpace;sc.imageExtent=mirrorExtent;sc.imageArrayLayers=1;sc.imageUsage=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;sc.imageSharingMode=VK_SHARING_MODE_EXCLUSIVE;sc.preTransform=caps.currentTransform;sc.compositeAlpha=(caps.supportedCompositeAlpha&VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR)?VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR:VkCompositeAlphaFlagBitsKHR(1u<<std::countr_zero(caps.supportedCompositeAlpha));sc.presentMode=mode;sc.clipped=VK_TRUE;check(vkCreateSwapchainKHR(device,&sc,nullptr,&swapchain),"Create spectator swapchain");
  if(!mirrorPass){VkAttachmentDescription color{};color.format=chosen.format;color.samples=VK_SAMPLE_COUNT_1_BIT;color.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR;color.storeOp=VK_ATTACHMENT_STORE_OP_STORE;color.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;color.finalLayout=VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;VkAttachmentReference ref{0,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};VkSubpassDescription sub{};sub.pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS;sub.colorAttachmentCount=1;sub.pColorAttachments=&ref;VkRenderPassCreateInfo rp{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};rp.attachmentCount=1;rp.pAttachments=&color;rp.subpassCount=1;rp.pSubpasses=&sub;check(vkCreateRenderPass(device,&rp,nullptr,&mirrorPass),"Create spectator render pass");}
  check(vkGetSwapchainImagesKHR(device,swapchain,&n,nullptr),"Get spectator images");swapImages.resize(n);check(vkGetSwapchainImagesKHR(device,swapchain,&n,swapImages.data()),"Get spectator images");
  for(auto image:swapImages){VkImageViewCreateInfo v{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};v.image=image;v.viewType=VK_IMAGE_VIEW_TYPE_2D;v.format=chosen.format;v.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};VkImageView view;check(vkCreateImageView(device,&v,nullptr,&view),"Create spectator view");mirrorViews.push_back(view);VkFramebufferCreateInfo f{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};f.renderPass=mirrorPass;f.attachmentCount=1;f.pAttachments=&mirrorViews.back();f.width=mirrorExtent.width;f.height=mirrorExtent.height;f.layers=1;VkFramebuffer frame;check(vkCreateFramebuffer(device,&f,nullptr,&frame),"Create spectator framebuffer");mirrorFrames.push_back(frame);VkSemaphoreCreateInfo sem{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};VkSemaphore ready;check(vkCreateSemaphore(device,&sem,nullptr,&ready),"Create spectator semaphore");readySemaphores.push_back(ready);}
 }
 void initPresentation(){
  if(vrMode&&surface)initMirror();
  VkExtent2D extent{};if(vrMode){auto size=VrRuntime::active()->extent();extent={uint32_t(size.first),uint32_t(size.second)};swapExtent=extent;swapFormat=VK_FORMAT_R8G8B8A8_UNORM;for(auto&i:vrOutputs){makeImage(i,extent.width,extent.height,1,swapFormat,VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_SAMPLED_BIT,VK_IMAGE_ASPECT_COLOR_BIT);swapViews.push_back(i.view);}}else{
  VkSurfaceCapabilitiesKHR caps{};check(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical,surface,&caps),"Query surface capabilities");uint32_t n=0;check(vkGetPhysicalDeviceSurfaceFormatsKHR(physical,surface,&n,nullptr),"Query surface formats");std::vector<VkSurfaceFormatKHR> formats(n);check(vkGetPhysicalDeviceSurfaceFormatsKHR(physical,surface,&n,formats.data()),"Query surface formats");
  VkSurfaceFormatKHR chosen=formats.front();for(auto f:formats)if(f.format==VK_FORMAT_B8G8R8A8_UNORM){chosen=f;break;}if(chosen.format!=VK_FORMAT_B8G8R8A8_UNORM&&chosen.format!=VK_FORMAT_R8G8B8A8_UNORM)throw std::runtime_error("Display has no supported 8-bit Vulkan surface format");swapFormat=chosen.format;
  RECT rect{};GetClientRect(hwnd,&rect);surfaceClientWidth=rect.right;surfaceClientHeight=rect.bottom;extent=caps.currentExtent.width!=UINT32_MAX?caps.currentExtent:VkExtent2D{uint32_t(std::max<LONG>(1,rect.right)),uint32_t(std::max<LONG>(1,rect.bottom))};extent.width=std::clamp(extent.width,caps.minImageExtent.width,caps.maxImageExtent.width);extent.height=std::clamp(extent.height,caps.minImageExtent.height,caps.maxImageExtent.height);swapExtent=extent;
  uint32_t imageCount=std::max(caps.minImageCount+1,2u);if(caps.maxImageCount&&imageCount>caps.maxImageCount)imageCount=caps.maxImageCount;
  uint32_t modeCount=0;vkGetPhysicalDeviceSurfacePresentModesKHR(physical,surface,&modeCount,nullptr);std::vector<VkPresentModeKHR> modes(modeCount);vkGetPhysicalDeviceSurfacePresentModesKHR(physical,surface,&modeCount,modes.data());
  // Preference order: IMMEDIATE (uncapped) → MAILBOX (low-latency vsync) → FIFO (vsync fallback).
  VkPresentModeKHR mode=VK_PRESENT_MODE_FIFO_KHR;
  for(auto x:modes)if(x==VK_PRESENT_MODE_IMMEDIATE_KHR){mode=x;break;}
  if(mode==VK_PRESENT_MODE_FIFO_KHR)for(auto x:modes)if(x==VK_PRESENT_MODE_MAILBOX_KHR){mode=x;break;}
  VkSwapchainCreateInfoKHR sc{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};sc.surface=surface;sc.minImageCount=imageCount;sc.imageFormat=swapFormat;sc.imageColorSpace=chosen.colorSpace;sc.imageExtent=extent;sc.imageArrayLayers=1;sc.imageUsage=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;sc.imageSharingMode=VK_SHARING_MODE_EXCLUSIVE;sc.preTransform=caps.currentTransform;sc.compositeAlpha=(caps.supportedCompositeAlpha&VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR)?VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR:VkCompositeAlphaFlagBitsKHR(1u<<std::countr_zero(caps.supportedCompositeAlpha));sc.presentMode=mode;sc.clipped=VK_TRUE;check(vkCreateSwapchainKHR(device,&sc,nullptr,&swapchain),"Create Win32 swapchain");
  check(vkGetSwapchainImagesKHR(device,swapchain,&n,nullptr),"Get swapchain images");swapImages.resize(n);check(vkGetSwapchainImagesKHR(device,swapchain,&n,swapImages.data()),"Get swapchain images");
  for(auto image:swapImages){VkImageViewCreateInfo v{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};v.image=image;v.viewType=VK_IMAGE_VIEW_TYPE_2D;v.format=swapFormat;v.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};VkImageView view;check(vkCreateImageView(device,&v,nullptr,&view),"Create swapchain view");swapViews.push_back(view);}
  }
  VkAttachmentDescription color{};color.format=swapFormat;color.samples=VK_SAMPLE_COUNT_1_BIT;color.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR;color.storeOp=VK_ATTACHMENT_STORE_OP_STORE;color.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;color.finalLayout=vrMode?VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;VkAttachmentReference ref{0,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};VkSubpassDescription sub{};sub.pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS;sub.colorAttachmentCount=1;sub.pColorAttachments=&ref;VkRenderPassCreateInfo rp{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};rp.attachmentCount=1;rp.pAttachments=&color;rp.subpassCount=1;rp.pSubpasses=&sub;check(vkCreateRenderPass(device,&rp,nullptr,&compositePass),"Create presentation render pass");
  for(auto view:swapViews){VkFramebufferCreateInfo f{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};f.renderPass=compositePass;f.attachmentCount=1;f.pAttachments=&view;f.width=extent.width;f.height=extent.height;f.layers=1;VkFramebuffer frame;check(vkCreateFramebuffer(device,&f,nullptr,&frame),"Create presentation framebuffer");swapFrames.push_back(frame);}
  VkDescriptorSetLayoutBinding bindings[2]{};for(uint32_t i=0;i<2;++i){bindings[i].binding=i;bindings[i].descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;bindings[i].descriptorCount=1;bindings[i].stageFlags=VK_SHADER_STAGE_FRAGMENT_BIT;}VkDescriptorSetLayoutCreateInfo sl{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};sl.bindingCount=2;sl.pBindings=bindings;check(vkCreateDescriptorSetLayout(device,&sl,nullptr,&compositeSetLayout),"Create composite descriptor layout");VkPushConstantRange push{VK_SHADER_STAGE_FRAGMENT_BIT,0,6*sizeof(float)};VkPipelineLayoutCreateInfo pl{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};pl.setLayoutCount=1;pl.pSetLayouts=&compositeSetLayout;pl.pushConstantRangeCount=1;pl.pPushConstantRanges=&push;check(vkCreatePipelineLayout(device,&pl,nullptr,&compositeLayout),"Create composite pipeline layout");
  VkDescriptorPoolSize ps{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,uint32_t(2*FrameCount)};VkDescriptorPoolCreateInfo dp{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};dp.maxSets=uint32_t(FrameCount);dp.poolSizeCount=1;dp.pPoolSizes=&ps;check(vkCreateDescriptorPool(device,&dp,nullptr,&compositeDescriptors),"Create composite pool");
  std::array<VkDescriptorSetLayout,FrameCount> layouts;layouts.fill(compositeSetLayout);
  VkDescriptorSetAllocateInfo da{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};da.descriptorPool=compositeDescriptors;da.descriptorSetCount=uint32_t(FrameCount);da.pSetLayouts=layouts.data();check(vkAllocateDescriptorSets(device,&da,compositeSets.data()),"Allocate composite descriptors");
  VkSemaphoreCreateInfo sem{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
  for(int i=0;i<FrameCount;++i)check(vkCreateSemaphore(device,&sem,nullptr,&acquiredSems[i]),"Create acquire semaphore");
  for(size_t i=0;!vrMode&&i<swapImages.size();++i){VkSemaphore ready;check(vkCreateSemaphore(device,&sem,nullptr,&ready),"Create presentation semaphore");readySemaphores.push_back(ready);}
  VkShaderModule vert=VK_NULL_HANDLE,frag=VK_NULL_HANDLE;try{VkShaderModuleCreateInfo sm{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};sm.codeSize=sizeof(compositeVertexShader);sm.pCode=compositeVertexShader;check(vkCreateShaderModule(device,&sm,nullptr,&vert),"Create composite vertex shader");sm.codeSize=sizeof(compositeFragmentShader);sm.pCode=compositeFragmentShader;check(vkCreateShaderModule(device,&sm,nullptr,&frag),"Create composite fragment shader");VkPipelineShaderStageCreateInfo stages[2]{};stages[0]={VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,nullptr,0,VK_SHADER_STAGE_VERTEX_BIT,vert,"main",nullptr};stages[1]={VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,nullptr,0,VK_SHADER_STAGE_FRAGMENT_BIT,frag,"main",nullptr};VkPipelineVertexInputStateCreateInfo input{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};VkPipelineInputAssemblyStateCreateInfo assembly{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};assembly.topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;VkPipelineViewportStateCreateInfo viewport{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};viewport.viewportCount=viewport.scissorCount=1;VkPipelineRasterizationStateCreateInfo raster{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};raster.polygonMode=VK_POLYGON_MODE_FILL;raster.cullMode=VK_CULL_MODE_NONE;raster.frontFace=VK_FRONT_FACE_COUNTER_CLOCKWISE;raster.lineWidth=1;VkPipelineMultisampleStateCreateInfo ms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};ms.rasterizationSamples=VK_SAMPLE_COUNT_1_BIT;VkPipelineColorBlendAttachmentState ba{};ba.colorWriteMask=0xf;VkPipelineColorBlendStateCreateInfo blend{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};blend.attachmentCount=1;blend.pAttachments=&ba;VkDynamicState dyns[]={VK_DYNAMIC_STATE_VIEWPORT,VK_DYNAMIC_STATE_SCISSOR};VkPipelineDynamicStateCreateInfo dyn{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};dyn.dynamicStateCount=2;dyn.pDynamicStates=dyns;VkGraphicsPipelineCreateInfo pi{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};pi.stageCount=2;pi.pStages=stages;pi.pVertexInputState=&input;pi.pInputAssemblyState=&assembly;pi.pViewportState=&viewport;pi.pRasterizationState=&raster;pi.pMultisampleState=&ms;pi.pColorBlendState=&blend;pi.pDynamicState=&dyn;pi.layout=compositeLayout;pi.renderPass=compositePass;check(vkCreateGraphicsPipelines(device,VK_NULL_HANDLE,1,&pi,nullptr,&compositePipeline),"Create composite pipeline");if(mirrorPass){pi.renderPass=mirrorPass;check(vkCreateGraphicsPipelines(device,VK_NULL_HANDLE,1,&pi,nullptr,&mirrorPipeline),"Create spectator pipeline");}}catch(...){if(vert)vkDestroyShaderModule(device,vert,nullptr);if(frag)vkDestroyShaderModule(device,frag,nullptr);throw;}vkDestroyShaderModule(device,vert,nullptr);vkDestroyShaderModule(device,frag,nullptr);
 }
 void recreatePresentation(){
  vkDeviceWaitIdle(device);
  for(auto f:swapFrames)vkDestroyFramebuffer(device,f,nullptr);for(auto v:swapViews)vkDestroyImageView(device,v,nullptr);for(auto&i:vrOutputs){i.view=VK_NULL_HANDLE;destroy(i);}if(swapchain)vkDestroySwapchainKHR(device,swapchain,nullptr);
  for(auto sem:readySemaphores)vkDestroySemaphore(device,sem,nullptr);
  for(int i=0;i<FrameCount;++i){if(acquiredSems[i]){vkDestroySemaphore(device,acquiredSems[i],nullptr);acquiredSems[i]=VK_NULL_HANDLE;}}
  if(compositePipeline)vkDestroyPipeline(device,compositePipeline,nullptr);if(compositePass)vkDestroyRenderPass(device,compositePass,nullptr);if(compositeLayout)vkDestroyPipelineLayout(device,compositeLayout,nullptr);if(compositeSetLayout)vkDestroyDescriptorSetLayout(device,compositeSetLayout,nullptr);if(compositeDescriptors)vkDestroyDescriptorPool(device,compositeDescriptors,nullptr);
  swapFrames.clear();swapViews.clear();swapImages.clear();readySemaphores.clear();swapchain=VK_NULL_HANDLE;
  compositeSets={};compositePipeline=VK_NULL_HANDLE;compositePass=VK_NULL_HANDLE;compositeLayout=VK_NULL_HANDLE;compositeSetLayout=VK_NULL_HANDLE;compositeDescriptors=VK_NULL_HANDLE;
  initPresentation();
 }
};
GpuRenderer::GpuRenderer(void* nativeWindow):m(std::make_unique<Impl>()){
 HWND window=static_cast<HWND>(nativeWindow);m->hwnd=window;
 VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};app.pApplicationName="RawMetal";app.apiVersion=VK_API_VERSION_1_0;
 m->vrMode=VrRuntime::active()!=nullptr;std::vector<std::string> extensionNames;if(window){extensionNames={VK_KHR_SURFACE_EXTENSION_NAME,VK_KHR_WIN32_SURFACE_EXTENSION_NAME};}if(m->vrMode){std::istringstream stream(VrRuntime::active()->instanceExtensions().c_str());for(std::string name;stream>>name;)if(std::find(extensionNames.begin(),extensionNames.end(),name)==extensionNames.end())extensionNames.push_back(name);}std::vector<const char*> extensions;for(auto&name:extensionNames)extensions.push_back(name.c_str());VkInstanceCreateInfo instance{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};instance.pApplicationInfo=&app;instance.enabledExtensionCount=uint32_t(extensions.size());instance.ppEnabledExtensionNames=extensions.data();check(vkCreateInstance(&instance,nullptr,&m->instance),"Create Vulkan instance");
 if(window){VkWin32SurfaceCreateInfoKHR info{VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR};info.hwnd=window;info.hinstance=GetModuleHandleW(nullptr);check(vkCreateWin32SurfaceKHR(m->instance,&info,nullptr,&m->surface),"Create Win32 Vulkan surface");}
 uint32_t count=0;check(vkEnumeratePhysicalDevices(m->instance,&count,nullptr),"Enumerate Vulkan adapters");std::vector<VkPhysicalDevice> adapters(count);check(vkEnumeratePhysicalDevices(m->instance,&count,adapters.data()),"Enumerate Vulkan adapters");int score=-1;
 auto vrDevice=m->vrMode?VrRuntime::active()->outputDevice(m->instance):VK_NULL_HANDLE;
 if(m->vrMode&&!vrDevice)throw std::runtime_error("SteamVR did not select a Vulkan adapter");
 for(auto adapter:adapters){if(m->vrMode&&adapter!=vrDevice)continue;VkPhysicalDeviceProperties props;vkGetPhysicalDeviceProperties(adapter,&props);if(props.deviceType==VK_PHYSICAL_DEVICE_TYPE_CPU)continue;
  uint32_t families=0;vkGetPhysicalDeviceQueueFamilyProperties(adapter,&families,nullptr);std::vector<VkQueueFamilyProperties> queues(families);vkGetPhysicalDeviceQueueFamilyProperties(adapter,&families,queues.data());
  for(uint32_t i=0;i<families;++i)if(queues[i].queueFlags&VK_QUEUE_GRAPHICS_BIT){VkBool32 present=true;if(m->surface)check(vkGetPhysicalDeviceSurfaceSupportKHR(adapter,i,m->surface,&present),"Check Vulkan presentation support");if(!present)continue;int candidate=props.deviceType==VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU?2:1;if(candidate>score){score=candidate;m->physical=adapter;m->family=i;m->name=props.deviceName;}break;}
 }
 if(!m->physical)throw std::runtime_error("No Vulkan hardware graphics adapter available");
 // Keep scene radiance linear and high range until the display composite.
 for(VkFormat format:{VK_FORMAT_R16G16B16A16_SFLOAT,VK_FORMAT_R32G32B32A32_SFLOAT}){
  VkFormatProperties properties{};vkGetPhysicalDeviceFormatProperties(m->physical,format,&properties);
  constexpr VkFormatFeatureFlags required=VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT|VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT|VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
  if((properties.optimalTilingFeatures&required)==required){m->sceneFormat=format;m->hdrScene=true;break;}
 }
  VkPhysicalDeviceProperties properties{};vkGetPhysicalDeviceProperties(m->physical,&properties);VkFormatProperties colorProperties{},depthProperties{};vkGetPhysicalDeviceFormatProperties(m->physical,m->sceneFormat,&colorProperties);vkGetPhysicalDeviceFormatProperties(m->physical,VK_FORMAT_D32_SFLOAT,&depthProperties);VkSampleCountFlags sampleCounts=properties.limits.framebufferColorSampleCounts&properties.limits.framebufferDepthSampleCounts;colorProperties.optimalTilingFeatures&=VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT;depthProperties.optimalTilingFeatures&=VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;if(colorProperties.optimalTilingFeatures&&depthProperties.optimalTilingFeatures){VkImageFormatProperties cp{},dp{};if(vkGetPhysicalDeviceImageFormatProperties(m->physical,m->sceneFormat,VK_IMAGE_TYPE_2D,VK_IMAGE_TILING_OPTIMAL,VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT,0,&cp)==VK_SUCCESS&&vkGetPhysicalDeviceImageFormatProperties(m->physical,VK_FORMAT_D32_SFLOAT,VK_IMAGE_TYPE_2D,VK_IMAGE_TILING_OPTIMAL,VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT,0,&dp)==VK_SUCCESS)sampleCounts&=cp.sampleCounts&dp.sampleCounts;else sampleCounts=VK_SAMPLE_COUNT_1_BIT;}else sampleCounts=VK_SAMPLE_COUNT_1_BIT;
  for(auto candidate:{VK_SAMPLE_COUNT_4_BIT,VK_SAMPLE_COUNT_2_BIT})if(sampleCounts&candidate){m->sceneSamples=candidate;break;}
  m->name+=m->hdrScene?" / linear HDR":" / LDR";
  if(m->sceneSamples!=VK_SAMPLE_COUNT_1_BIT)m->name+=" / "+std::to_string(int(m->sceneSamples))+"x MSAA";
  VkPhysicalDeviceFeatures supported{};vkGetPhysicalDeviceFeatures(m->physical,&supported);VkPhysicalDeviceFeatures enabled{};enabled.samplerAnisotropy=supported.samplerAnisotropy;
 VkPhysicalDeviceProperties deviceProperties{};vkGetPhysicalDeviceProperties(m->physical,&deviceProperties);
 float priority=1;VkDeviceQueueCreateInfo queue{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};queue.queueFamilyIndex=m->family;queue.queueCount=1;queue.pQueuePriorities=&priority;std::vector<std::string> deviceNames;if(m->surface)deviceNames.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);if(m->vrMode){std::istringstream stream(VrRuntime::active()->deviceExtensions(m->physical).c_str());for(std::string name;stream>>name;)deviceNames.push_back(name);}std::vector<const char*> deviceExtensions;for(auto&name:deviceNames)deviceExtensions.push_back(name.c_str());VkDeviceCreateInfo device{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};device.queueCreateInfoCount=1;device.pQueueCreateInfos=&queue;device.pEnabledFeatures=&enabled;device.enabledExtensionCount=uint32_t(deviceExtensions.size());device.ppEnabledExtensionNames=deviceExtensions.data();check(vkCreateDevice(m->physical,&device,nullptr,&m->device),"Create Vulkan device");vkGetDeviceQueue(m->device,m->family,0,&m->queue);
 VkCommandPoolCreateInfo pool{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};pool.flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;pool.queueFamilyIndex=m->family;check(vkCreateCommandPool(m->device,&pool,nullptr,&m->pool),"Create graphics command pool");
 // Allocate all per-frame command buffers plus the dedicated upload buffer in one call.
 VkCommandBufferAllocateInfo cmdAlloc{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};cmdAlloc.commandPool=m->pool;cmdAlloc.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;
 cmdAlloc.commandBufferCount=uint32_t(Impl::FrameCount);check(vkAllocateCommandBuffers(m->device,&cmdAlloc,m->commands.data()),"Allocate frame command buffers");
 cmdAlloc.commandBufferCount=1;check(vkAllocateCommandBuffers(m->device,&cmdAlloc,&m->uploadCommand),"Allocate upload command buffer");
 VkFenceCreateInfo fenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
 for(int i=0;i<Impl::FrameCount;++i)check(vkCreateFence(m->device,&fenceInfo,nullptr,&m->fences[i]),"Create frame fence");
 check(vkCreateFence(m->device,&fenceInfo,nullptr,&m->uploadFence),"Create upload fence");
 VkDescriptorSetLayoutBinding bindings[4]{};for(uint32_t i=0;i<4;++i){bindings[i].binding=i;bindings[i].descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;bindings[i].descriptorCount=1;bindings[i].stageFlags=VK_SHADER_STAGE_FRAGMENT_BIT;}
 VkDescriptorSetLayoutCreateInfo set{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};set.bindingCount=4;set.pBindings=bindings;check(vkCreateDescriptorSetLayout(m->device,&set,nullptr,&m->setLayout),"Create material layout");VkDescriptorPoolSize poolSize{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,8192};VkDescriptorPoolCreateInfo descriptors{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};descriptors.maxSets=2048;descriptors.poolSizeCount=1;descriptors.pPoolSizes=&poolSize;check(vkCreateDescriptorPool(m->device,&descriptors,nullptr,&m->descriptors),"Create descriptor pool");
 VkPushConstantRange viewPush{VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,0,32*sizeof(float)};VkPipelineLayoutCreateInfo layout{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};VkDescriptorSetLayoutBinding projectionBinding{0,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,1,VK_SHADER_STAGE_VERTEX_BIT,nullptr};VkDescriptorSetLayoutCreateInfo projectionInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};projectionInfo.bindingCount=1;projectionInfo.pBindings=&projectionBinding;check(vkCreateDescriptorSetLayout(m->device,&projectionInfo,nullptr,&m->projectionLayout),"Create stereo projection layout");VkDescriptorPoolSize projectionSize{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,2};VkDescriptorPoolCreateInfo projectionPool{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};projectionPool.maxSets=2;projectionPool.poolSizeCount=1;projectionPool.pPoolSizes=&projectionSize;check(vkCreateDescriptorPool(m->device,&projectionPool,nullptr,&m->projectionPool),"Create stereo projection pool");VkDescriptorSetLayout projectionLayouts[]={m->projectionLayout,m->projectionLayout};VkDescriptorSetAllocateInfo projectionAllocate{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};projectionAllocate.descriptorPool=m->projectionPool;projectionAllocate.descriptorSetCount=2;projectionAllocate.pSetLayouts=projectionLayouts;check(vkAllocateDescriptorSets(m->device,&projectionAllocate,m->projectionSets.data()),"Allocate stereo projection descriptors");for(int i=0;i<2;++i){m->makeBuffer(m->projectionBuffers[i],sizeof(m->vrProjection),VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);VkDescriptorBufferInfo buffer{m->projectionBuffers[i].handle,0,sizeof(m->vrProjection)};VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};write.dstSet=m->projectionSets[i];write.descriptorCount=1;write.descriptorType=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;write.pBufferInfo=&buffer;vkUpdateDescriptorSets(m->device,1,&write,0,nullptr);}VkDescriptorSetLayout pipelineSets[]={m->setLayout,m->projectionLayout};layout.setLayoutCount=2;layout.pSetLayouts=pipelineSets;layout.pushConstantRangeCount=1;layout.pPushConstantRanges=&viewPush;check(vkCreatePipelineLayout(m->device,&layout,nullptr,&m->pipelineLayout),"Create graphics pipeline layout");
  const bool multisampled=m->sceneSamples!=VK_SAMPLE_COUNT_1_BIT;uint32_t depthIndex=multisampled?2u:1u;VkAttachmentDescription attachments[3]{};attachments[0].format=m->sceneFormat;attachments[0].samples=m->sceneSamples;attachments[0].loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR;attachments[0].storeOp=multisampled?VK_ATTACHMENT_STORE_OP_DONT_CARE:VK_ATTACHMENT_STORE_OP_STORE;attachments[0].stencilLoadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE;attachments[0].stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE;attachments[0].initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;attachments[0].finalLayout=multisampled?VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
  if(multisampled){attachments[1]=attachments[0];attachments[1].samples=VK_SAMPLE_COUNT_1_BIT;attachments[1].loadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE;attachments[1].storeOp=VK_ATTACHMENT_STORE_OP_STORE;attachments[1].finalLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;}
 attachments[depthIndex]=attachments[0];attachments[depthIndex].format=VK_FORMAT_D32_SFLOAT;attachments[depthIndex].loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR;attachments[depthIndex].storeOp=VK_ATTACHMENT_STORE_OP_DONT_CARE;attachments[depthIndex].finalLayout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
 VkAttachmentReference color{0,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},resolve{1,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},depth{depthIndex,VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};VkSubpassDescription subpass{};subpass.pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS;subpass.colorAttachmentCount=1;subpass.pColorAttachments=&color;subpass.pResolveAttachments=multisampled?&resolve:nullptr;subpass.pDepthStencilAttachment=&depth;
 VkSubpassDependency deps[2]{};deps[0].srcSubpass=VK_SUBPASS_EXTERNAL;deps[0].dstSubpass=0;deps[0].srcStageMask=VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT|VK_PIPELINE_STAGE_TRANSFER_BIT;deps[0].srcAccessMask=VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_TRANSFER_READ_BIT;deps[0].dstStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT|VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;deps[0].dstAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT|VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;deps[1].srcSubpass=0;deps[1].dstSubpass=VK_SUBPASS_EXTERNAL;deps[1].srcStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT|VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT|VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;deps[1].srcAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT|VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;deps[1].dstStageMask=VK_PIPELINE_STAGE_TRANSFER_BIT|VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;deps[1].dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT|VK_ACCESS_SHADER_READ_BIT;
 VkRenderPassCreateInfo pass{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};pass.attachmentCount=depthIndex+1;pass.pAttachments=attachments;pass.subpassCount=1;pass.pSubpasses=&subpass;pass.dependencyCount=2;pass.pDependencies=deps;check(vkCreateRenderPass(m->device,&pass,nullptr,&m->renderPass),"Create render pass");
 VkSamplerCreateInfo sampler{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};sampler.magFilter=sampler.minFilter=VK_FILTER_LINEAR;sampler.anisotropyEnable=enabled.samplerAnisotropy;sampler.maxAnisotropy=enabled.samplerAnisotropy?std::min(8.f,deviceProperties.limits.maxSamplerAnisotropy):1.f;sampler.mipmapMode=VK_SAMPLER_MIPMAP_MODE_LINEAR;sampler.addressModeU=sampler.addressModeV=sampler.addressModeW=VK_SAMPLER_ADDRESS_MODE_REPEAT;sampler.maxLod=VK_LOD_CLAMP_NONE;check(vkCreateSampler(m->device,&sampler,nullptr,&m->wrap),"Create texture sampler");sampler.addressModeU=sampler.addressModeV=sampler.addressModeW=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;check(vkCreateSampler(m->device,&sampler,nullptr,&m->clamp),"Create decal sampler");
 sampler.anisotropyEnable=VK_FALSE;sampler.maxAnisotropy=1.f;sampler.maxLod=0.f;check(vkCreateSampler(m->device,&sampler,nullptr,&m->sceneSampler),"Create scene presentation sampler");
 sampler.magFilter=sampler.minFilter=VK_FILTER_NEAREST;sampler.mipmapMode=VK_SAMPLER_MIPMAP_MODE_NEAREST;check(vkCreateSampler(m->device,&sampler,nullptr,&m->pointSampler),"Create HUD and depth sampler");
 VkShaderModule vert=VK_NULL_HANDLE,frag=VK_NULL_HANDLE;
 try{
  VkShaderModuleCreateInfo shader{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};shader.codeSize=sizeof(vertexShader);shader.pCode=vertexShader;check(vkCreateShaderModule(m->device,&shader,nullptr,&vert),"Create vertex shader");shader.codeSize=sizeof(fragmentShader);shader.pCode=fragmentShader;check(vkCreateShaderModule(m->device,&shader,nullptr,&frag),"Create fragment shader");
  VkPipelineShaderStageCreateInfo stages[2]{};for(auto&s:stages){s.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;s.pName="main";}stages[0].stage=VK_SHADER_STAGE_VERTEX_BIT;stages[0].module=vert;stages[1].stage=VK_SHADER_STAGE_FRAGMENT_BIT;stages[1].module=frag;
  VkVertexInputBindingDescription binding{0,sizeof(Vertex),VK_VERTEX_INPUT_RATE_VERTEX};VkVertexInputAttributeDescription attrs[]={{0,0,VK_FORMAT_R32G32B32A32_SFLOAT,0},{1,0,VK_FORMAT_R32G32_SFLOAT,16},{2,0,VK_FORMAT_R32G32B32A32_SFLOAT,24},{3,0,VK_FORMAT_R32G32B32A32_SFLOAT,40},{4,0,VK_FORMAT_R32G32B32A32_SFLOAT,56},{5,0,VK_FORMAT_R32G32B32A32_SFLOAT,72},{6,0,VK_FORMAT_R32G32B32A32_SFLOAT,88}};
  VkPipelineVertexInputStateCreateInfo input{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};input.vertexBindingDescriptionCount=1;input.pVertexBindingDescriptions=&binding;input.vertexAttributeDescriptionCount=7;input.pVertexAttributeDescriptions=attrs;VkPipelineInputAssemblyStateCreateInfo assembly{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};assembly.topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  VkPipelineViewportStateCreateInfo viewport{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};viewport.viewportCount=viewport.scissorCount=1;VkPipelineRasterizationStateCreateInfo raster{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};raster.polygonMode=VK_POLYGON_MODE_FILL;raster.cullMode=VK_CULL_MODE_NONE;raster.frontFace=VK_FRONT_FACE_COUNTER_CLOCKWISE;raster.lineWidth=1;
  VkPipelineMultisampleStateCreateInfo samples{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};samples.rasterizationSamples=m->sceneSamples;VkPipelineDepthStencilStateCreateInfo depthState{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};depthState.depthTestEnable=depthState.depthWriteEnable=VK_TRUE;depthState.depthCompareOp=VK_COMPARE_OP_LESS;
  VkPipelineColorBlendAttachmentState blend{};blend.colorWriteMask=0xf;blend.srcColorBlendFactor=blend.dstColorBlendFactor=VK_BLEND_FACTOR_ONE;blend.srcAlphaBlendFactor=blend.dstAlphaBlendFactor=VK_BLEND_FACTOR_ONE;blend.colorBlendOp=blend.alphaBlendOp=VK_BLEND_OP_ADD;VkPipelineColorBlendStateCreateInfo blending{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};blending.attachmentCount=1;blending.pAttachments=&blend;
  VkDynamicState states[]={VK_DYNAMIC_STATE_VIEWPORT,VK_DYNAMIC_STATE_SCISSOR};VkPipelineDynamicStateCreateInfo dynamic{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};dynamic.dynamicStateCount=2;dynamic.pDynamicStates=states;
  VkGraphicsPipelineCreateInfo pipeline{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};pipeline.stageCount=2;pipeline.pStages=stages;pipeline.pVertexInputState=&input;pipeline.pInputAssemblyState=&assembly;pipeline.pViewportState=&viewport;pipeline.pRasterizationState=&raster;pipeline.pMultisampleState=&samples;pipeline.pDepthStencilState=&depthState;pipeline.pColorBlendState=&blending;pipeline.pDynamicState=&dynamic;pipeline.layout=m->pipelineLayout;pipeline.renderPass=m->renderPass;
  check(vkCreateGraphicsPipelines(m->device,VK_NULL_HANDLE,1,&pipeline,nullptr,&m->opaque),"Create opaque Vulkan pipeline");blend.blendEnable=VK_TRUE;depthState.depthWriteEnable=VK_FALSE;check(vkCreateGraphicsPipelines(m->device,VK_NULL_HANDLE,1,&pipeline,nullptr,&m->additive),"Create additive Vulkan pipeline");blend.srcColorBlendFactor=VK_BLEND_FACTOR_SRC_ALPHA;blend.dstColorBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;check(vkCreateGraphicsPipelines(m->device,VK_NULL_HANDLE,1,&pipeline,nullptr,&m->transparent),"Create water blend pipeline");
 }catch(...){if(vert)vkDestroyShaderModule(m->device,vert,nullptr);if(frag)vkDestroyShaderModule(m->device,frag,nullptr);throw;}vkDestroyShaderModule(m->device,vert,nullptr);vkDestroyShaderModule(m->device,frag,nullptr);
 if(m->surface||m->vrMode)m->initPresentation();
 m->vertices.reserve(150000);m->batches.reserve(4096);
}
GpuRenderer::~GpuRenderer()=default;
const std::string& GpuRenderer::adapter()const{return m->name;}
bool GpuRenderer::hasSurface()const{return m->surface!=VK_NULL_HANDLE;}
std::pair<int,int> GpuRenderer::surfaceExtent()const{
 if(m->vrMode)return VrRuntime::active()->extent();
 if(!m->surface||!m->swapExtent.width||!m->swapExtent.height)return {m->width,m->height};
 RECT client{};GetClientRect(m->hwnd,&client);
 if(client.right<=0||client.bottom<=0)return {int(m->swapExtent.width),int(m->swapExtent.height)};
 if(client.right==m->surfaceClientWidth&&client.bottom==m->surfaceClientHeight)return {int(m->swapExtent.width),int(m->swapExtent.height)};
 VkSurfaceCapabilitiesKHR caps{};check(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m->physical,m->surface,&caps),"Query render surface");
 VkExtent2D extent=caps.currentExtent.width!=UINT32_MAX?caps.currentExtent:VkExtent2D{uint32_t(client.right),uint32_t(client.bottom)};
 extent.width=std::clamp(extent.width,caps.minImageExtent.width,caps.maxImageExtent.width);
 extent.height=std::clamp(extent.height,caps.minImageExtent.height,caps.maxImageExtent.height);
 return {int(extent.width),int(extent.height)};
}
void GpuRenderer::captureVrEye(std::vector<std::uint32_t>&pixels){if(!m->vrMode)throw std::runtime_error("Eye capture requires VR");Buffer capture;try{m->makeBuffer(capture,VkDeviceSize(m->swapExtent.width)*m->swapExtent.height*4,VK_BUFFER_USAGE_TRANSFER_DST_BIT);m->startUploadCommands();VkBufferImageCopy copy{};copy.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};copy.imageExtent={m->swapExtent.width,m->swapExtent.height,1};vkCmdCopyImageToBuffer(m->uploadCommand,m->vrOutputs[m->vrEye].handle,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,capture.handle,1,&copy);VkMemoryBarrier host{VK_STRUCTURE_TYPE_MEMORY_BARRIER};host.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;host.dstAccessMask=VK_ACCESS_HOST_READ_BIT;vkCmdPipelineBarrier(m->uploadCommand,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,1,&host,0,nullptr,0,nullptr);m->submitUploadCommands();pixels.resize(size_t(m->swapExtent.width)*m->swapExtent.height);auto source=static_cast<const uint8_t*>(capture.mapped);for(size_t i=0;i<pixels.size();++i)pixels[i]=0xff000000u|(uint32_t(source[i*4])<<16)|(uint32_t(source[i*4+1])<<8)|source[i*4+2];m->destroy(capture);}catch(...){m->destroy(capture);throw;}}
void GpuRenderer::updateDynamic(const SoftwareRenderer::Texture&texture){auto key=m->material(texture);auto&image=m->materials.at(key).color;VkDeviceSize size=VkDeviceSize(texture.width)*texture.height*4;if(m->dynamicUpload.size<size){m->destroy(m->dynamicUpload);m->makeBuffer(m->dynamicUpload,size,VK_BUFFER_USAGE_TRANSFER_SRC_BIT);}std::memcpy(m->dynamicUpload.mapped,texture.pixels.data(),size);m->startUploadCommands();auto cmd=m->uploadCommand;m->barrier(image,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_ACCESS_SHADER_READ_BIT,VK_ACCESS_TRANSFER_WRITE_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,1,cmd);VkBufferImageCopy copy{};copy.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};copy.imageExtent={uint32_t(texture.width),uint32_t(texture.height),1};vkCmdCopyBufferToImage(cmd,m->dynamicUpload.handle,image.handle,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&copy);m->barrier(image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,1,cmd);m->submitUploadCommands();}
void GpuRenderer::prepare(const SoftwareRenderer::Texture&texture){m->material(texture);}
void GpuRenderer::begin(int width,int height){
 // Advance to the next frame slot.
 m->frameIndex=(m->frameIndex+1)%Impl::FrameCount;
 // If this slot was in flight (submitted two frames ago), wait for it to finish
 // before we overwrite its command buffer and vertex buffer.
 if(m->frameInFlight[m->frameIndex]){
  check(vkWaitForFences(m->device,1,&m->fences[m->frameIndex],VK_TRUE,5000000000ull),"Wait for frame fence");
  m->frameInFlight[m->frameIndex]=false;
 }
 for(auto&buffer:m->retiredBuffers[m->frameIndex])m->destroy(buffer);m->retiredBuffers[m->frameIndex].clear();
 m->vertices.clear();m->batches.clear();m->capturing=false;m->captureSlot=-1;m->renderingViewmodel=false;
 if(width==m->width&&height==m->height)return;
 // Render target size changed — must drain GPU fully before resize.
 vkDeviceWaitIdle(m->device);for(int i=0;i<Impl::FrameCount;++i)m->frameInFlight[i]=false;
 if(m->framebuffer){vkDestroyFramebuffer(m->device,m->framebuffer,nullptr);m->framebuffer=VK_NULL_HANDLE;}m->destroy(m->target);m->destroy(m->sceneColor);m->destroy(m->depth);m->destroy(m->readback);
 m->makeImage(m->target,width,height,1,m->sceneFormat,VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_SAMPLED_BIT,VK_IMAGE_ASPECT_COLOR_BIT);if(m->sceneSamples!=VK_SAMPLE_COUNT_1_BIT)m->makeImage(m->sceneColor,width,height,1,m->sceneFormat,VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT|VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,VK_IMAGE_ASPECT_COLOR_BIT,m->sceneSamples);m->makeImage(m->depth,width,height,1,VK_FORMAT_D32_SFLOAT,VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,VK_IMAGE_ASPECT_DEPTH_BIT,m->sceneSamples);m->makeBuffer(m->readback,VkDeviceSize(width)*height*(m->sceneFormat==VK_FORMAT_R16G16B16A16_SFLOAT?8:m->sceneFormat==VK_FORMAT_R32G32B32A32_SFLOAT?16:4),VK_BUFFER_USAGE_TRANSFER_DST_BIT);
 VkImageView views[]={m->sceneSamples==VK_SAMPLE_COUNT_1_BIT?m->target.view:m->sceneColor.view,m->sceneSamples==VK_SAMPLE_COUNT_1_BIT?m->depth.view:m->target.view,m->depth.view};VkFramebufferCreateInfo frame{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};frame.renderPass=m->renderPass;frame.attachmentCount=m->sceneSamples==VK_SAMPLE_COUNT_1_BIT?2u:3u;frame.pAttachments=views;frame.width=width;frame.height=height;frame.layers=1;check(vkCreateFramebuffer(m->device,&frame,nullptr,&m->framebuffer),"Create frame target");m->width=width;m->height=height;
}
void GpuRenderer::setView(float x,float y,float z,float yaw,float pitch,float aspect,bool flashlight,float muzzleFlash,float elapsed){
 float cy=std::cos(yaw),sy=std::sin(yaw),cp=std::cos(pitch),sp=std::sin(pitch);
 m->viewState={x,y,z,cy,sy,cp,sp,aspect,flashlight?1.f:0.f,muzzleFlash,elapsed,m->hdrScene?1.f:0.f};
}
void GpuRenderer::setFogLights(const std::array<float,16>& lights){std::copy(lights.begin(),lights.end(),m->viewState.begin()+12);}
void GpuRenderer::setAtmosphere(const std::array<float,4>& atmosphere){std::copy(atmosphere.begin(),atmosphere.end(),m->viewState.begin()+28);}
bool GpuRenderer::beginStaticCache(int slot,std::uint64_t key){
 auto found=m->staticCaches.find(slot);
 if(found!=m->staticCaches.end()&&found->second.key==key){++m->cacheHits;Impl::Batch marker{};marker.cache=true;marker.cacheSlot=slot;m->batches.push_back(marker);return false;}
 auto&cache=m->staticCaches[slot];m->retire(cache.buffer);cache={};cache.key=key;m->captureSlot=slot;m->capturing=true;return true;
}
void GpuRenderer::endStaticCache(){
 if(!m->capturing||m->captureSlot<0)return;
 int slot=m->captureSlot;auto&cache=m->staticCaches.at(slot);std::vector<Vertex> sorted;std::vector<Impl::Batch> batches;sorted.reserve(cache.vertices.size());
 std::stable_sort(cache.batches.begin(),cache.batches.end(),[](const Impl::Batch&a,const Impl::Batch&b){return a.sortKey<b.sortKey;});
 for(auto batch:cache.batches){auto oldStart=batch.start;batch.start=uint32_t(sorted.size());sorted.insert(sorted.end(),cache.vertices.begin()+oldStart,cache.vertices.begin()+oldStart+batch.count);if(!batches.empty()&&batches.back().material==batch.material)batches.back().count+=batch.count;else batches.push_back(batch);}
 cache.vertices.swap(sorted);cache.batches.swap(batches);
 m->capturing=false;m->captureSlot=-1;
 if(!cache.vertices.empty()){size_t bytes=cache.vertices.size()*sizeof(Vertex);m->makeBuffer(cache.buffer,bytes,VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);std::memcpy(cache.buffer.mapped,cache.vertices.data(),bytes);}
 Impl::Batch marker{};marker.cache=true;marker.cacheSlot=slot;m->batches.push_back(marker);
}
void GpuRenderer::clearStaticCaches(){for(auto&entry:m->staticCaches)m->retire(entry.second.buffer);m->staticCaches.clear();m->capturing=false;m->captureSlot=-1;}
std::uint64_t GpuRenderer::staticCacheHits()const{return m->cacheHits;}
void GpuRenderer::clearDepth(){m->batches.push_back({0,0,0,true});m->renderingViewmodel=true;}
void GpuRenderer::submit(MeshVertex a,MeshVertex b,MeshVertex c,const SoftwareRenderer::Texture&texture,float light,const std::array<Point3,2>&directions,const std::array<float,2>&weights,float flatResponse,bool normals,float emissionScale){
 uint64_t key;uint32_t sortKey;
 if(texture.pixels.size()>1&&m->lastPixels==texture.pixels.data()&&m->lastGeneration==texture.generation.value){key=m->lastMaterial;sortKey=m->lastSort;}
 else{key=m->material(texture);sortKey=m->materials.at(key).sortKey;m->lastPixels=texture.pixels.data();m->lastGeneration=texture.generation.value;m->lastMaterial=key;m->lastSort=sortKey;}
 auto&target=m->capturing?m->staticCaches.at(m->captureSlot).vertices:m->vertices;uint32_t start=uint32_t(target.size());
 auto worldPoint=[&](Point3 p){float cy=m->viewState[3],sy=m->viewState[4],cp=m->viewState[5],sp=m->viewState[6];float forward=p.z*cp-p.y*sp;return Point3{m->viewState[0]-p.x*sy+forward*cy,m->viewState[1]+p.x*cy+forward*sy,m->viewState[2]+p.y*cp+p.z*sp};};
 Point3 worldNormal{};if(m->capturing){auto e1=b.p-a.p,e2=c.p-a.p;Point3 n{e1.y*e2.z-e1.z*e2.y,e1.z*e2.x-e1.x*e2.z,e1.x*e2.y-e1.y*e2.x};float nl=std::sqrt(n.x*n.x+n.y*n.y+n.z*n.z);if(nl>.00001f)n=n*(1.f/nl);worldNormal={-n.x*m->viewState[4]-n.y*m->viewState[6]*m->viewState[3]+n.z*m->viewState[5]*m->viewState[3],n.x*m->viewState[3]-n.y*m->viewState[6]*m->viewState[4]+n.z*m->viewState[5]*m->viewState[4],n.y*m->viewState[5]+n.z*m->viewState[6]};}
 for(auto v:{a,b,c}){Vertex out{};if(m->capturing){auto p=worldPoint(v.p);out.position[0]=p.x;out.position[1]=p.y;out.position[2]=p.z;out.position[3]=1.f;}else{out.position[0]=v.p.x*1.3f;out.position[1]=-v.p.y*1.3f*float(m->width)/m->height;out.position[2]=v.p.z-.06f;out.position[3]=v.p.z;}
  out.uv[0]=v.u;out.uv[1]=v.v;out.lighting[0]=light;out.lighting[1]=v.light;out.lighting[2]=flatResponse;out.lighting[3]=normals?1.f:0.f;
  for(int i=0;i<2;++i){auto*dest=i?out.light1:out.light0;dest[0]=directions[i].x;dest[1]=directions[i].y;dest[2]=directions[i].z;dest[3]=weights[i];}out.surface[0]=v.p.z;out.surface[1]=texture.emission.empty()?0.f:emissionScale;out.surface[2]=(texture.transparent?2.f:texture.additive?1.f:0.f)+texture.glossStrength;out.surface[3]=m->capturing?1.f:m->renderingViewmodel?2.f:0.f;out.worldNormal[0]=worldNormal.x;out.worldNormal[1]=worldNormal.y;out.worldNormal[2]=worldNormal.z;out.worldNormal[3]=texture.parallaxScale;target.push_back(out);
 }
 if(m->capturing){auto&batches=m->staticCaches.at(m->captureSlot).batches;if(!batches.empty()&&!batches.back().clear&&batches.back().material==key)batches.back().count+=3;else batches.push_back({key,start,3,false,false,-1,sortKey});}
 else if(!m->batches.empty()&&!m->batches.back().clear&&!m->batches.back().cache&&m->batches.back().material==key)m->batches.back().count+=3;else m->batches.push_back({key,start,3,false,false,-1,sortKey});
}
void GpuRenderer::finish(std::vector<std::uint32_t>&pixels){
 // Batch opaque geometry by material, then additive effects, independently for
 // world and view-model depth ranges. Thousands of small modules become tens
 // of hardware draw calls without clipping the visible world.
 auto& vb=m->vertexBuffers[m->frameIndex];
 size_t bytes=m->vertices.size()*sizeof(Vertex);
 if(bytes>vb.size){m->destroy(vb);m->makeBuffer(vb,std::max(bytes*2,size_t(1048576)),VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);}
 Vertex* dest=static_cast<Vertex*>(vb.mapped);uint32_t vertexOffset=0;
 m->sortedBatches.clear();
 for(size_t begin=0;begin<m->batches.size();){
  if(m->batches[begin].clear||m->batches[begin].cache){m->sortedBatches.push_back(m->batches[begin++]);continue;}
  size_t end=begin;while(end<m->batches.size()&&!m->batches[end].clear&&!m->batches[end].cache)++end;
  std::stable_sort(m->batches.begin()+begin,m->batches.begin()+end,[](const auto&a,const auto&b){return a.sortKey<b.sortKey;});
  for(size_t i=begin;i<end;++i){auto batch=m->batches[i];uint32_t start=vertexOffset;
   if(dest)std::memcpy(dest+vertexOffset,m->vertices.data()+batch.start,batch.count*sizeof(Vertex));
   vertexOffset+=batch.count;
   if(!m->sortedBatches.empty()&&!m->sortedBatches.back().clear&&m->sortedBatches.back().material==batch.material)m->sortedBatches.back().count+=batch.count;
   else m->sortedBatches.push_back({batch.material,start,batch.count,false,false,-1,batch.sortKey});
  }begin=end;
 }
 m->batches.swap(m->sortedBatches);
 m->startCommands();
  VkClearValue clear[3]{};clear[0].color=m->hdrScene?VkClearColorValue{{std::pow(12/255.f,2.2f),std::pow(16/255.f,2.2f),std::pow(18/255.f,2.2f),1}}:VkClearColorValue{{12/255.f,16/255.f,18/255.f,1}};uint32_t depthIndex=m->sceneSamples==VK_SAMPLE_COUNT_1_BIT?1u:2u;clear[depthIndex].depthStencil={1,0};
 VkRenderPassBeginInfo pass{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};pass.renderPass=m->renderPass;pass.framebuffer=m->framebuffer;pass.renderArea.extent={uint32_t(m->width),uint32_t(m->height)};pass.clearValueCount=depthIndex+1;pass.pClearValues=clear;
 auto cmd=m->commands[m->frameIndex];
 vkCmdBeginRenderPass(cmd,&pass,VK_SUBPASS_CONTENTS_INLINE);
 VkViewport viewport{0,0,float(m->width),float(m->height),0,1};VkRect2D scissor{{0,0},{uint32_t(m->width),uint32_t(m->height)}};
 vkCmdSetViewport(cmd,0,1,&viewport);vkCmdSetScissor(cmd,0,1,&scissor);
 VkDeviceSize offset=0;VkBuffer currentBuffer=VK_NULL_HANDLE;
 if(bytes){currentBuffer=vb.handle;vkCmdBindVertexBuffers(cmd,0,1,&currentBuffer,&offset);}
 std::memcpy(m->projectionBuffers[m->frameIndex].mapped,m->vrProjection.data(),sizeof(m->vrProjection));vkCmdBindDescriptorSets(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,m->pipelineLayout,1,1,&m->projectionSets[m->frameIndex],0,nullptr);
 vkCmdPushConstants(cmd,m->pipelineLayout,VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,0,sizeof(m->viewState),m->viewState.data());
 VkPipeline previous=VK_NULL_HANDLE;
 struct TransparentDraw{VkBuffer buffer;VkDescriptorSet material;uint32_t count,start;};std::vector<TransparentDraw> transparentDraws;
 auto flushTransparent=[&]{for(const auto& draw:transparentDraws){if(currentBuffer!=draw.buffer){vkCmdBindVertexBuffers(cmd,0,1,&draw.buffer,&offset);currentBuffer=draw.buffer;}if(previous!=m->transparent){vkCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,m->transparent);previous=m->transparent;}vkCmdBindDescriptorSets(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,m->pipelineLayout,0,1,&draw.material,0,nullptr);vkCmdDraw(cmd,draw.count,1,draw.start,0);}transparentDraws.clear();};
 for(auto&batch:m->batches){
  if(batch.clear){flushTransparent();VkClearAttachment attachment{};attachment.aspectMask=VK_IMAGE_ASPECT_DEPTH_BIT;attachment.clearValue.depthStencil={1,0};VkClearRect rect{scissor,0,1};vkCmdClearAttachments(cmd,1,&attachment,1,&rect);continue;}
  if(batch.cache){auto found=m->staticCaches.find(batch.cacheSlot);if(found==m->staticCaches.end()||!found->second.buffer.handle)continue;auto buffer=found->second.buffer.handle;if(currentBuffer!=buffer){vkCmdBindVertexBuffers(cmd,0,1,&buffer,&offset);currentBuffer=buffer;}for(auto&cached:found->second.batches){auto&mat=m->materials.at(cached.material);if(mat.transparent){transparentDraws.push_back({buffer,mat.set,cached.count,cached.start});continue;}auto pipeline=mat.additive?m->additive:m->opaque;if(pipeline!=previous){vkCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,pipeline);previous=pipeline;}vkCmdBindDescriptorSets(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,m->pipelineLayout,0,1,&mat.set,0,nullptr);vkCmdDraw(cmd,cached.count,1,cached.start,0);}continue;}
  if(currentBuffer!=vb.handle){vkCmdBindVertexBuffers(cmd,0,1,&vb.handle,&offset);currentBuffer=vb.handle;}
  auto&mat=m->materials.at(batch.material);if(mat.transparent){transparentDraws.push_back({vb.handle,mat.set,batch.count,batch.start});continue;}auto pipeline=mat.additive?m->additive:m->opaque;
  if(pipeline!=previous){vkCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,pipeline);previous=pipeline;}
  vkCmdBindDescriptorSets(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,m->pipelineLayout,0,1,&mat.set,0,nullptr);
  vkCmdDraw(cmd,batch.count,1,batch.start,0);
 }
 flushTransparent();vkCmdEndRenderPass(cmd);
 if(m->surface||m->vrMode){
  // Windowed path: leave command buffer open — present() will append the
  // composite pass and submit everything together in one async submit.
  return;
 }
 // Headless screenshot path: copy to readback buffer then wait (blocking is fine here).
 VkBufferImageCopy copy{};copy.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};copy.imageExtent={uint32_t(m->width),uint32_t(m->height),1};
 vkCmdCopyImageToBuffer(cmd,m->target.handle,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,m->readback.handle,1,&copy);
 VkBufferMemoryBarrier host{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};host.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;host.dstAccessMask=VK_ACCESS_HOST_READ_BIT;host.srcQueueFamilyIndex=host.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;host.buffer=m->readback.handle;host.size=VK_WHOLE_SIZE;
 vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,0,nullptr,1,&host,0,nullptr);
 // Blocking submit for headless — we need pixels immediately.
 m->submitCommandsAsync();
 check(vkWaitForFences(m->device,1,&m->fences[m->frameIndex],VK_TRUE,5000000000ull),"Wait for headless frame");
 m->frameInFlight[m->frameIndex]=false;
 pixels.resize(size_t(m->width*m->height));
 auto half=[](uint16_t bits){float value;if((bits&0x7c00u)==0x7c00u)value=(bits&0x03ffu)?std::numeric_limits<float>::quiet_NaN():std::numeric_limits<float>::infinity();else if((bits&0x7c00u)==0)value=std::ldexp(float(bits&0x03ffu),-24);else value=std::ldexp(1.f+float(bits&0x03ffu)/1024.f,int((bits>>10)&31)-15);return (bits&0x8000u)?-value:value;};
 auto display=[](float value){value=std::max(0.f,value)*1.05f;float mapped=std::clamp((value*(2.51f*value+.03f))/(value*(2.43f*value+.59f)+.14f),0.f,1.f);return uint32_t(std::clamp(int(std::lround(std::pow(mapped,1.f/2.2f)*255.f)),0,255));};
 if(m->sceneFormat==VK_FORMAT_R16G16B16A16_SFLOAT){auto*source=static_cast<const uint16_t*>(m->readback.mapped);for(size_t i=0;i<pixels.size();++i)pixels[i]=0xff000000u|(display(half(source[i*4]))<<16)|(display(half(source[i*4+1]))<<8)|display(half(source[i*4+2]));}
 else if(m->sceneFormat==VK_FORMAT_R32G32B32A32_SFLOAT){auto*source=static_cast<const float*>(m->readback.mapped);for(size_t i=0;i<pixels.size();++i)pixels[i]=0xff000000u|(display(source[i*4])<<16)|(display(source[i*4+1])<<8)|display(source[i*4+2]);}
 else std::memcpy(pixels.data(),m->readback.mapped,pixels.size()*4);
}
void GpuRenderer::present(const std::uint32_t* overlayPixels,int overlayWidth,int overlayHeight,bool underwater,float sceneDim,float damageFlash,float shotKick){
 if(!m->surface&&!m->vrMode)return;
 RECT client{};if(m->vrMode){client.right=m->swapExtent.width;client.bottom=m->swapExtent.height;}else GetClientRect(m->hwnd,&client);if(client.right<=0||client.bottom<=0){m->submitCommandsAsync();return;}
 if(!m->vrMode&&(client.right!=m->surfaceClientWidth||client.bottom!=m->surfaceClientHeight))m->recreatePresentation();
 // Resize overlay staging buffer for this frame slot if needed.
 auto& ob=m->overlayBuffers[m->frameIndex];
 if(m->overlayWidth!=overlayWidth||m->overlayHeight!=overlayHeight){
  // Must drain before destroying shared overlay image.
  vkDeviceWaitIdle(m->device);for(int i=0;i<Impl::FrameCount;++i)m->frameInFlight[i]=false;
  m->destroy(m->overlay);for(int i=0;i<Impl::FrameCount;++i)m->destroy(m->overlayBuffers[i]);
  m->makeImage(m->overlay,overlayWidth,overlayHeight,1,VK_FORMAT_B8G8R8A8_UNORM,VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT,VK_IMAGE_ASPECT_COLOR_BIT);
  for(int i=0;i<Impl::FrameCount;++i)m->makeBuffer(m->overlayBuffers[i],VkDeviceSize(overlayWidth)*overlayHeight*4,VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
  m->overlayWidth=overlayWidth;m->overlayHeight=overlayHeight;m->overlayInitialized=false;
 }
 // Write HUD pixels into this slot's staging buffer (safe — GPU is reading previous slot's).
 std::memcpy(ob.mapped,overlayPixels,size_t(overlayWidth)*overlayHeight*4);
 // Append overlay upload + composite pass to the still-open command buffer from finish().
 auto cmd=m->commands[m->frameIndex];
 VkImageLayout oldOverlay=m->overlayInitialized?(m->vrMode?VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL):VK_IMAGE_LAYOUT_UNDEFINED;
 VkAccessFlags srcAccess=m->overlayInitialized?(m->vrMode?VK_ACCESS_TRANSFER_READ_BIT:VK_ACCESS_SHADER_READ_BIT):0;
 VkPipelineStageFlags srcStage=m->overlayInitialized?(m->vrMode?VK_PIPELINE_STAGE_TRANSFER_BIT:VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT):VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
 m->barrier(m->overlay,oldOverlay,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,srcAccess,VK_ACCESS_TRANSFER_WRITE_BIT,srcStage,VK_PIPELINE_STAGE_TRANSFER_BIT,1,cmd);
 VkBufferImageCopy upload{};upload.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};upload.imageExtent={uint32_t(overlayWidth),uint32_t(overlayHeight),1};
 vkCmdCopyBufferToImage(cmd,ob.handle,m->overlay.handle,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&upload);
 m->barrier(m->overlay,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,1,cmd);
 m->overlayInitialized=true;
 m->barrier(m->target,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,0,VK_ACCESS_SHADER_READ_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,1,cmd);
 // Update this frame slot's composite descriptor set.
 VkDescriptorImageInfo images[2]={{m->sceneSampler,m->target.view,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},{m->pointSampler,m->overlay.view,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL}};
 VkWriteDescriptorSet writes[2]{};for(uint32_t i=0;i<2;++i){writes[i].sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;writes[i].dstSet=m->compositeSets[m->frameIndex];writes[i].dstBinding=i;writes[i].descriptorCount=1;writes[i].descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;writes[i].pImageInfo=&images[i];}
 vkUpdateDescriptorSets(m->device,2,writes,0,nullptr);
 // Acquire swapchain image — use this slot's semaphore.
 uint32_t imageIndex=m->vrMode?uint32_t(m->vrEye):0;
 VkResult acquire=m->vrMode?VK_SUCCESS:vkAcquireNextImageKHR(m->device,m->swapchain,UINT64_MAX,m->acquiredSems[m->frameIndex],VK_NULL_HANDLE,&imageIndex);
 if(acquire==VK_ERROR_OUT_OF_DATE_KHR){m->submitCommandsAsync();m->recreatePresentation();return;}
 // A suboptimal image is still valid and its acquire semaphore must be
 // consumed. Present it, then rebuild if presentation also requests it.
 if(acquire!=VK_SUBOPTIMAL_KHR)check(acquire,"Acquire swapchain image");
 VkClearValue clear{};clear.color={{0,0,0,1}};
 VkRenderPassBeginInfo pass{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};pass.renderPass=m->compositePass;pass.framebuffer=m->swapFrames[imageIndex];pass.renderArea.extent=m->swapExtent;pass.clearValueCount=1;pass.pClearValues=&clear;
 vkCmdBeginRenderPass(cmd,&pass,VK_SUBPASS_CONTENTS_INLINE);
 VkViewport viewport{0,0,float(m->swapExtent.width),float(m->swapExtent.height),0,1};VkRect2D scissor{{0,0},m->swapExtent};
 vkCmdSetViewport(cmd,0,1,&viewport);vkCmdSetScissor(cmd,0,1,&scissor);
 vkCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,m->compositePipeline);
 vkCmdBindDescriptorSets(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,m->compositeLayout,0,1,&m->compositeSets[m->frameIndex],0,nullptr);
 float params[6]={underwater?1.f:0.f,sceneDim,damageFlash,shotKick,m->hdrScene?1.f:0.f,m->vrMode?1.f:0.f};
 vkCmdPushConstants(cmd,m->compositeLayout,VK_SHADER_STAGE_FRAGMENT_BIT,0,sizeof(params),params);
 vkCmdDraw(cmd,6,1,0,0);
 vkCmdEndRenderPass(cmd);
 uint32_t mirrorIndex=0;bool mirrorAcquired=false;
 if(m->vrMode&&m->surface&&m->vrEye==1){
  RECT rect{};GetClientRect(m->hwnd,&rect);
  if(rect.right>0&&rect.bottom>0){
   if(rect.right!=m->surfaceClientWidth||rect.bottom!=m->surfaceClientHeight)m->initMirror();
   // Do not stall headset rendering waiting for a desktop refresh.
   VkResult result=vkAcquireNextImageKHR(m->device,m->swapchain,0,m->acquiredSems[m->frameIndex],VK_NULL_HANDLE,&mirrorIndex);
   if(result==VK_SUCCESS||result==VK_SUBOPTIMAL_KHR){mirrorAcquired=true;pass.renderPass=m->mirrorPass;pass.framebuffer=m->mirrorFrames[mirrorIndex];pass.renderArea.extent=m->mirrorExtent;vkCmdBeginRenderPass(cmd,&pass,VK_SUBPASS_CONTENTS_INLINE);
    float scale=std::min(float(m->mirrorExtent.width)/overlayWidth,float(m->mirrorExtent.height)/overlayHeight);if(scale>=1)scale=std::floor(scale);float w=overlayWidth*scale,h=overlayHeight*scale;viewport={float(m->mirrorExtent.width-w)*.5f,float(m->mirrorExtent.height-h)*.5f,w,h,0,1};scissor={{0,0},m->mirrorExtent};vkCmdSetViewport(cmd,0,1,&viewport);vkCmdSetScissor(cmd,0,1,&scissor);vkCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,m->mirrorPipeline);vkCmdBindDescriptorSets(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,m->compositeLayout,0,1,&m->compositeSets[m->frameIndex],0,nullptr);params[5]=VrRuntime::active()->titlePanel()?0.f:1.f;vkCmdPushConstants(cmd,m->compositeLayout,VK_SHADER_STAGE_FRAGMENT_BIT,0,sizeof(params),params);vkCmdDraw(cmd,6,1,0,0);vkCmdEndRenderPass(cmd);
   }else if(result==VK_ERROR_OUT_OF_DATE_KHR)m->initMirror();else if(result!=VK_NOT_READY&&result!=VK_TIMEOUT)check(result,"Acquire spectator image");
  }
 }
 if(m->vrMode){m->barrier(m->overlay,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_ACCESS_SHADER_READ_BIT,VK_ACCESS_TRANSFER_READ_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT);m->barrier(m->vrOutputs[m->vrEye],VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,VK_ACCESS_TRANSFER_READ_BIT,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT);m->submitCommandsAsync(mirrorAcquired?m->acquiredSems[m->frameIndex]:VK_NULL_HANDLE,mirrorAcquired?m->readySemaphores[mirrorIndex]:VK_NULL_HANDLE);check(vkWaitForFences(m->device,1,&m->fences[m->frameIndex],VK_TRUE,5000000000ull),"Wait for VR eye");m->frameInFlight[m->frameIndex]=false;if(m->vrEye==1)VrRuntime::active()->submitPanel(m->instance,m->physical,m->device,m->queue,m->family,m->overlay.handle,overlayWidth,overlayHeight);VrRuntime::active()->submit(m->vrEye,m->instance,m->physical,m->device,m->queue,m->family,m->vrOutputs[m->vrEye].handle,m->swapExtent.width,m->swapExtent.height);if(mirrorAcquired){VkPresentInfoKHR spectator{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};spectator.waitSemaphoreCount=1;spectator.pWaitSemaphores=&m->readySemaphores[mirrorIndex];spectator.swapchainCount=1;spectator.pSwapchains=&m->swapchain;spectator.pImageIndices=&mirrorIndex;auto result=vkQueuePresentKHR(m->queue,&spectator);if(result==VK_SUCCESS||result==VK_SUBOPTIMAL_KHR)++m->mirrorPresented;else if(result!=VK_ERROR_OUT_OF_DATE_KHR)check(result,"Present spectator");if(result==VK_SUBOPTIMAL_KHR||result==VK_ERROR_OUT_OF_DATE_KHR)m->initMirror();}return;}
 // Submit: wait on image-acquired semaphore, signal ready semaphore, NO CPU wait.
 m->submitCommandsAsync(m->acquiredSems[m->frameIndex],m->readySemaphores[imageIndex]);
 // Present immediately.
 VkPresentInfoKHR present{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};present.waitSemaphoreCount=1;present.pWaitSemaphores=&m->readySemaphores[imageIndex];present.swapchainCount=1;present.pSwapchains=&m->swapchain;present.pImageIndices=&imageIndex;
 VkResult result=vkQueuePresentKHR(m->queue,&present);
 if(result==VK_ERROR_OUT_OF_DATE_KHR||result==VK_SUBOPTIMAL_KHR||acquire==VK_SUBOPTIMAL_KHR){
  // Need to drain before recreating swapchain.
  check(vkWaitForFences(m->device,1,&m->fences[m->frameIndex],VK_TRUE,5000000000ull),"Wait for present fence");
  m->frameInFlight[m->frameIndex]=false;
  m->recreatePresentation();return;
 }
 if(result!=VK_SUCCESS)check(result,"Present swapchain");
 // Do NOT wait here — that's the whole point. The fence for this slot will
 // be waited on at the top of begin() when this slot comes around again.
}
}

namespace retro {std::uint64_t GpuRenderer::mirrorFrames()const{return m->mirrorPresented;}bool GpuRenderer::vrActive()const{return m->vrMode;}void GpuRenderer::setVrEye(int eye,const std::array<float,16>& clip){m->vrEye=eye;std::copy(clip.begin(),clip.end(),m->vrProjection.begin());m->vrProjection[16]=1;}}
