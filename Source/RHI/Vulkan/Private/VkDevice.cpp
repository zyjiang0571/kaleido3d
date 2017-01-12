#include "VkCommon.h"
#include "VkRHI.h"
#include "VkUtils.h"
#include "VkConfig.h"

using namespace rhi;

K3D_VK_BEGIN

::k3d::DynArray<GpuRef> Instance::EnumGpus()
{
	DynArray<GpuRef> gpus;
	DynArray<VkPhysicalDevice> gpuDevices;
	uint32_t gpuCount = 0;
	K3D_VK_VERIFY(vkEnumeratePhysicalDevices(m_Instance, &gpuCount, nullptr));
	//gpus.Resize(gpuCount);
	gpuDevices.Resize(gpuCount);
	K3D_VK_VERIFY(vkEnumeratePhysicalDevices(m_Instance, &gpuCount, gpuDevices.Data()));
	for (auto gpu : gpuDevices)
	{
		auto gpuRef = GpuRef(new Gpu(gpu, SharedFromThis()));
		gpus.Append(gpuRef);
	}
	return gpus;
}

//DeviceAdapter::DeviceAdapter(GpuRef const & gpu)
//	: m_Gpu(gpu)
//{
//	m_pDevice = MakeShared<Device>();
//	m_pDevice->Create(this, gpu->m_Inst->WithValidation());
//	//m_Gpu->m_Inst->AppendLogicalDevice(m_pDevice);
//}
//
//DeviceAdapter::~DeviceAdapter()
//{
//}
//
//DeviceRef DeviceAdapter::GetDevice()
//{
//	return m_pDevice;
//}

Device::Device() 
	: m_Gpu(nullptr)
	, m_Device(VK_NULL_HANDLE)
{
}

Device::~Device()
{
	Destroy();
}

void Device::Destroy()
{
	if (VK_NULL_HANDLE == m_Device)
		return;
	vkDeviceWaitIdle(m_Device);
	VKLOG(Info, "Device Destroying .  -- %0x.", m_Device);
	///*if (!m_CachedDescriptorSetLayout.empty())
	//{
	//	m_CachedDescriptorSetLayout.erase(m_CachedDescriptorSetLayout.begin(),
	//		m_CachedDescriptorSetLayout.end());
	//}
	//if (!m_CachedDescriptorPool.empty())
	//{
	//	m_CachedDescriptorPool.erase(m_CachedDescriptorPool.begin(),
	//		m_CachedDescriptorPool.end());
	//}
	//if (!m_CachedPipelineLayout.empty())
	//{
	//	m_CachedPipelineLayout.erase(m_CachedPipelineLayout.begin(),
	//		m_CachedPipelineLayout.end());
	//}*/
	//m_PendingPass.~vector();
	//m_ResourceManager->~ResourceManager();
	//m_ContextPool->~CommandContextPool();
	if (m_CmdBufManager)
	{
		m_CmdBufManager->~CommandBufferManager();
	}
	vkDestroyDevice(m_Device, nullptr);
	VKLOG(Info, "Device Destroyed .  -- %0x.", m_Device);
	m_Device = VK_NULL_HANDLE;
}

IDevice::Result
Device::Create(rhi::IDeviceAdapter* pAdapter, bool withDbg)
{
	//m_Gpu = static_cast<DeviceAdapter*>(pAdapter)->m_Gpu;
	//m_Device = m_Gpu->CreateLogicDevice(withDbg);
	//if(m_Device)
	//{
	//	//LoadVulkan(m_Gpu->m_Inst->m_Instance, m_Device);
	//	if (withDbg)
	//	{
	//		RHIRoot::SetupDebug(VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT 
	//			| VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT, DebugReportCallback);
	//	}
	//	//m_ResourceManager = std::make_unique<ResourceManager>(SharedFromThis(), 1024, 1024);
	//	//m_ContextPool = std::make_unique<CommandContextPool>(SharedFromThis());
	//	m_DefCmdQueue = InitCmdQueue(VK_QUEUE_GRAPHICS_BIT, m_Gpu->m_GraphicsQueueIndex, 0);
	//	m_ComputeCmdQueue = InitCmdQueue(VK_QUEUE_COMPUTE_BIT, m_Gpu->m_ComputeQueueIndex, 0);
	//	return rhi::IDevice::DeviceFound;
	//}
	return rhi::IDevice::DeviceNotFound;
}

IDevice::Result
Device::Create(GpuRef const & gpu, bool withDebug)
{
	m_Gpu = gpu;
	m_Device = m_Gpu->CreateLogicDevice(withDebug);
	if (m_Device)
	{
		//LoadVulkan(m_Gpu->m_Inst->m_Instance, m_Device);
		if (withDebug)
		{
			RHIRoot::SetupDebug(VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT
				| VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT, DebugReportCallback);
		}
		//m_ResourceManager = std::make_unique<ResourceManager>(SharedFromThis(), 1024, 1024);
		//m_ContextPool = std::make_unique<CommandContextPool>(SharedFromThis());
		m_DefCmdQueue = InitCmdQueue(VK_QUEUE_GRAPHICS_BIT, m_Gpu->m_GraphicsQueueIndex, 0);
		m_ComputeCmdQueue = InitCmdQueue(VK_QUEUE_COMPUTE_BIT, m_Gpu->m_ComputeQueueIndex, 0);
		return rhi::IDevice::DeviceFound;
	}
	return rhi::IDevice::DeviceNotFound;
}

RenderTargetRef
Device::NewRenderTarget(rhi::RenderTargetLayout const & layout)
{
	return RenderTargetRef(new RenderTarget(SharedFromThis(), layout));
}

uint64 Device::GetMaxAllocationCount()
{
	return m_Gpu->m_Prop.limits.maxMemoryAllocationCount;
}

SpCmdQueue Device::InitCmdQueue(VkQueueFlags queueTypes, uint32 queueFamilyIndex, uint32 queueIndex)
{
	return std::make_shared<CommandQueue>(m_Device, queueTypes, queueFamilyIndex, queueIndex);
}

bool Device::FindMemoryType(uint32 typeBits, VkFlags requirementsMask, uint32 *typeIndex) const
{
#ifdef max
#undef max
	*typeIndex = std::numeric_limits<std::remove_pointer<decltype(typeIndex)>::type>::max();
#endif
	auto memProp = m_Gpu->m_MemProp;
	for (uint32_t i = 0; i < memProp.memoryTypeCount; ++i) {
		if (typeBits & 0x00000001) {
			if (requirementsMask == (memProp.memoryTypes[i].propertyFlags & requirementsMask)) {
				*typeIndex = i;
				return true;
			}
		}
		typeBits >>= 1;
	}

	return false;
}

CommandContextRef
Device::NewCommandContext(rhi::ECommandType Type)
{
	if (!m_CmdBufManager)
	{
		m_CmdBufManager = CmdBufManagerRef(new CommandBufferManager(m_Device, VK_COMMAND_BUFFER_LEVEL_PRIMARY, m_Gpu->m_GraphicsQueueIndex));
	}
	return rhi::CommandContextRef(new CommandContext(SharedFromThis(), m_CmdBufManager->RequestCommandBuffer(), VK_NULL_HANDLE, Type));
	//return m_ContextPool->RequestContext(Type);
}

SamplerRef
Device::NewSampler(const rhi::SamplerState& samplerDesc)
{
	return MakeShared<Sampler>(SharedFromThis(), samplerDesc);
}

rhi::PipelineLayoutRef
Device::NewPipelineLayout(rhi::PipelineLayoutDesc const & table)
{
	// Hash the table parameter here, 
	// Lookup the layout by hash code
	/*rhi::PipelineLayoutKey key = HashPipelineLayoutDesc(table);
	if (m_CachedPipelineLayout.find(key) == m_CachedPipelineLayout.end())
	{
		auto plRef = rhi::PipelineLayoutRef(new PipelineLayout(this, table));
		m_CachedPipelineLayout.insert({ key, plRef });
	}*/
	return rhi::PipelineLayoutRef(new PipelineLayout(SharedFromThis(), table));
}

DescriptorAllocRef
Device::NewDescriptorAllocator(uint32 maxSets, BindingArray const & bindings)
{
	uint32 key = util::Hash32((const char*)bindings.Data(), bindings.Count() * sizeof(VkDescriptorSetLayoutBinding));
	/*if (m_CachedDescriptorPool.find(key) == m_CachedDescriptorPool.end())
	{
		DescriptorAllocator::Options options = {}; 
		auto descAllocRef = DescriptorAllocRef(new DescriptorAllocator(this, options, maxSets, bindings));
		m_CachedDescriptorPool.insert({ key, descAllocRef });
	}*/
	DescriptorAllocator::Options options = {};
	return DescriptorAllocRef(new DescriptorAllocator(SharedFromThis(), options, maxSets, bindings));
}

DescriptorSetLayoutRef
Device::NewDescriptorSetLayout(BindingArray const & bindings)
{
	uint32 key = util::Hash32((const char*)bindings.Data(), bindings.Count() * sizeof(VkDescriptorSetLayoutBinding));
	/*if (m_CachedDescriptorSetLayout.find(key) == m_CachedDescriptorSetLayout.end()) 
	{
		auto descSetLayoutRef = DescriptorSetLayoutRef(new DescriptorSetLayout(this, bindings));
		m_CachedDescriptorSetLayout.insert({ key, descSetLayoutRef });
	}*/
	return DescriptorSetLayoutRef(new DescriptorSetLayout(SharedFromThis(), bindings));
}

PipelineStateObjectRef
Device::NewPipelineState(rhi::PipelineDesc const & desc, rhi::PipelineLayoutRef ppl, rhi::EPipelineType type)
{
	return CreatePipelineStateObject(desc, ppl);
}

PipelineStateObjectRef
Device::CreatePipelineStateObject(rhi::PipelineDesc const & desc, rhi::PipelineLayoutRef ppl)
{
	return MakeShared<PipelineStateObject>(SharedFromThis(), desc, static_cast<PipelineLayout*>(ppl.Get()));
}

SyncFenceRef
Device::NewFence()
{
	return MakeShared<Fence>(SharedFromThis());
}

GpuResourceRef
Device::NewGpuResource(rhi::ResourceDesc const& Desc) 
{
	rhi::IGpuResource * resource = nullptr;
	switch (Desc.Type)
	{
	case rhi::EGT_Buffer:
		resource = new Buffer(SharedFromThis(), Desc);
		break;
	case rhi::EGT_Texture1D:
		break;
	case rhi::EGT_Texture2D:
		resource = new Texture(SharedFromThis(), Desc);
		break;
	default:
		break;
	}
	return GpuResourceRef(resource);
}

ShaderResourceViewRef
Device::NewShaderResourceView(rhi::GpuResourceRef pRes, rhi::ResourceViewDesc const & desc)
{
	return MakeShared<ShaderResourceView>(SharedFromThis(), desc, pRes);
}

rhi::IDescriptorPool *
Device::NewDescriptorPool()
{
	return nullptr;
}

RenderViewportRef
Device::NewRenderViewport(void * winHandle, rhi::GfxSetting& setting)
{
	auto pViewport = MakeShared<RenderViewport>(SharedFromThis(), winHandle, setting);
	RHIRoot::AddViewport(pViewport);
	return pViewport;
}

PtrCmdAlloc Device::NewCommandAllocator(bool transient)
{
	return CommandAllocator::CreateAllocator(m_Gpu->m_GraphicsQueueIndex, transient, SharedFromThis());
}

PtrSemaphore Device::NewSemaphore()
{
	return std::make_shared<Semaphore>(SharedFromThis());
}

void Device::QueryTextureSubResourceLayout(rhi::TextureRef resource, rhi::TextureResourceSpec const & spec, rhi::SubResourceLayout * layout)
{
	K3D_ASSERT(resource);
	auto texture = StaticPointerCast<Texture>(resource);
	vkGetImageSubresourceLayout(m_Device, (VkImage)resource->GetLocation(), (const VkImageSubresource*)&spec, (VkSubresourceLayout*)layout);
}

SwapChainRef
Device::NewSwapChain(rhi::GfxSetting const & setting)
{
	return k3d::MakeShared<vk::SwapChain>(SharedFromThis());
}

K3D_VK_END