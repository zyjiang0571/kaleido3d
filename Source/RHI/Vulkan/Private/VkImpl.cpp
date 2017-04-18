#include "VkCommon.h"
#include "VkRHI.h"
#include "VkEnums.h"
#include "VkUtils.h"
#include <set>
#include <map>
#include <sstream>
#include <iomanip>
#include <cstdarg>
#include <Core/Os.h>
#include <Core/Module.h>
#include <Log/Public/ILogModule.h>
#include <Core/WebSocket.h>

using namespace rhi::shc;
using namespace rhi;
using namespace std;

PFN_vklogCallBack g_logCallBack = nullptr;

static thread_local char logContent[2048] = { 0 };
#define LOG_SERVER 1

#if K3DPLATFORM_OS_ANDROID
#define OutputDebugStringA(str) __android_log_print(ANDROID_LOG_DEBUG, "VkRHI", "%s", str);
#endif

void VkLog(k3d::ELogLevel const& Lv, const char * tag, const char * fmt, ...) 
{
	if (g_logCallBack != nullptr)
	{
		va_list va;
		va_start(va, fmt);
		g_logCallBack(Lv, tag, fmt, va);
		va_end(va);
	}
	else
	{
#if LOG_SERVER
		va_list va;
		va_start(va, fmt);
		vsprintf(logContent, fmt, va);
		va_end(va);

		auto logModule = k3d::StaticPointerCast<k3d::ILogModule>(k3d::GlobalModuleManager.FindModule("KawaLog"));
		if (logModule)
		{
			k3d::ILogger* logger = logModule->GetLogger(k3d::ELoggerType::EWebsocket);
			if (logger)
			{
				logger->Log(Lv, tag, logContent);
			}

			if (Lv >= k3d::ELogLevel::Debug)
			{
				logger = logModule->GetLogger(k3d::ELoggerType::EConsole);
				if (logger)
				{
					logger->Log(Lv, tag, logContent);
				}
			}

			if (Lv >= k3d::ELogLevel::Warn)
			{
				logger = logModule->GetLogger(k3d::ELoggerType::EFile);
				if (logger)
				{
					logger->Log(Lv, tag, logContent);
				}
			}
		}
#else
		va_list va;
		va_start(va, fmt);
		vsprintf(logContent, fmt, va);
		va_end(va);
		OutputDebugStringA(logContent);
#endif
	}
}

void SetVkLogCallback(PFN_vklogCallBack func)
{
	g_logCallBack = func;
}


K3D_VK_BEGIN

PipelineStateObject::PipelineStateObject(Device::Ptr pDevice, rhi::PipelineDesc const & desc, PipelineLayout * ppl)
  : DeviceChild(pDevice)
  , m_Pipeline(VK_NULL_HANDLE)
  , m_PipelineCache(VK_NULL_HANDLE)
  , m_RenderPass(VK_NULL_HANDLE)
  , m_GfxCreateInfo{}
  , m_PipelineLayout(ppl)
{
  memset(&m_GfxCreateInfo, 0, sizeof(m_GfxCreateInfo));
  InitWithDesc(desc);
}

/**
* @class	PipelineStateObject
*/
PipelineStateObject::PipelineStateObject(Device::Ptr pDevice)
  : DeviceChild(pDevice)
  , m_Pipeline(VK_NULL_HANDLE)
  , m_PipelineCache(VK_NULL_HANDLE)
  , m_RenderPass(VK_NULL_HANDLE)
{
  memset(&m_GfxCreateInfo, 0, sizeof(m_GfxCreateInfo));
}

PipelineStateObject::~PipelineStateObject()
{
  Destroy();
}


void PipelineStateObject::BindRenderPass(VkRenderPass RenderPass)
{
  this->m_RenderPass = RenderPass;
}


VkShaderModule CreateShaderModule(VkDevice Device, rhi::IDataBlob * ShaderBytes)
{
  VkShaderModule shaderModule;
  VkShaderModuleCreateInfo moduleCreateInfo;

  moduleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  moduleCreateInfo.pNext = NULL;

  moduleCreateInfo.codeSize = ShaderBytes->Length();
  moduleCreateInfo.pCode = (const uint32*)ShaderBytes->Bytes();
  moduleCreateInfo.flags = 0;
  K3D_VK_VERIFY(vkCreateShaderModule(Device, &moduleCreateInfo, NULL, &shaderModule));
  return shaderModule;
}

VkShaderModule CreateShaderModule(GpuRef Device, String ShaderBytes)
{
  VkShaderModule shaderModule;
  VkShaderModuleCreateInfo moduleCreateInfo;

  moduleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  moduleCreateInfo.pNext = NULL;

  moduleCreateInfo.codeSize = ShaderBytes.Length();
  moduleCreateInfo.pCode = (const uint32_t*)ShaderBytes.Data();
  moduleCreateInfo.flags = 0;
  K3D_VK_VERIFY(vkCreateShaderModule(Device->m_LogicalDevice, &moduleCreateInfo, NULL, &shaderModule));
  return shaderModule;
}

void PipelineStateObject::SetShader(rhi::EShaderType ShaderType, rhi::ShaderBundle const& ShaderBytes)
{
  auto sm = CreateShaderModule(GetGpuRef(), ShaderBytes.RawData);
  VkPipelineShaderStageCreateInfo shaderStage = {};
  shaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  shaderStage.stage = g_ShaderType[ShaderType];
  shaderStage.module = sm;
  shaderStage.pName = ShaderBytes.Desc.EntryFunction.CStr(); // todo : make param
  K3D_VK_VERIFY(shaderStage.module != NULL);
  m_ShaderStageInfos.push_back(shaderStage);
}

void PipelineStateObject::SetLayout(rhi::PipelineLayoutRef PipelineLayout)
{

}

void PipelineStateObject::Finalize()
{
  if (VK_NULL_HANDLE != m_Pipeline)
    return;
  VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {};
  pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
  K3D_VK_VERIFY(vkCreatePipelineCache(GetRawDevice(), &pipelineCacheCreateInfo, nullptr, &m_PipelineCache));
  if (GetType() == rhi::EPSO_Graphics)
  {
    K3D_VK_VERIFY(vkCreateGraphicsPipelines(GetRawDevice(), m_PipelineCache, 1, &m_GfxCreateInfo, nullptr, &m_Pipeline));
  }
  else
  {
    m_CptCreateInfo.stage = m_ShaderStageInfos[0];
    K3D_VK_VERIFY(vkCreateComputePipelines(GetRawDevice(), m_PipelineCache, 1, &m_CptCreateInfo, nullptr, &m_Pipeline));
  }
}

void PipelineStateObject::SavePSO(const char * path)
{
  size_t szPSO = 0;
  K3D_VK_VERIFY(vkGetPipelineCacheData(GetRawDevice(), m_PipelineCache, &szPSO, nullptr));
  if (!szPSO || !path)
    return;
  DynArray<char> dataBlob;
  dataBlob.Resize(szPSO);
  vkGetPipelineCacheData(GetRawDevice(), m_PipelineCache, &szPSO, dataBlob.Data());
  Os::File psoCacheFile(path);
  psoCacheFile.Open(IOWrite);
  psoCacheFile.Write(dataBlob.Data(), szPSO);
  psoCacheFile.Close();
}

void PipelineStateObject::LoadPSO(const char * path)
{
  Os::MemMapFile psoFile;
  if (!path || !psoFile.Open(path, IORead))
    return;
  VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {};
  pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
  pipelineCacheCreateInfo.pInitialData = psoFile.FileData();
  pipelineCacheCreateInfo.initialDataSize = psoFile.GetSize();
  K3D_VK_VERIFY(vkCreatePipelineCache(GetRawDevice(), &pipelineCacheCreateInfo, nullptr, &m_PipelineCache));
}

void PipelineStateObject::SetRasterizerState(const rhi::RasterizerState& rasterState)
{
  m_RasterizationState.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  // Solid polygon mode
  m_RasterizationState.polygonMode = g_FillMode[rasterState.FillMode];
  // No culling
  m_RasterizationState.cullMode = g_CullMode[rasterState.CullMode];
  m_RasterizationState.frontFace = rasterState.FrontCCW ? VK_FRONT_FACE_COUNTER_CLOCKWISE : VK_FRONT_FACE_CLOCKWISE;
  m_RasterizationState.depthClampEnable = rasterState.DepthClipEnable ? VK_TRUE : VK_FALSE;
  m_RasterizationState.rasterizerDiscardEnable = VK_FALSE;
  m_RasterizationState.depthBiasEnable = VK_FALSE;
  this->m_GfxCreateInfo.pRasterizationState = &m_RasterizationState;
}

// One blend attachment state
// Blending is not used in this example
// TODO
void PipelineStateObject::SetBlendState(const rhi::BlendState& blendState)
{
  m_ColorBlendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  VkPipelineColorBlendAttachmentState blendAttachmentState[1] = {};
  blendAttachmentState[0].colorWriteMask = 0xf;
  blendAttachmentState[0].blendEnable = blendState.Enable ? VK_TRUE : VK_FALSE;
  m_ColorBlendState.attachmentCount = 1;
  m_ColorBlendState.pAttachments = blendAttachmentState;
  this->m_GfxCreateInfo.pColorBlendState = &m_ColorBlendState;
}

void PipelineStateObject::SetDepthStencilState(const rhi::DepthStencilState& depthStencilState)
{
  m_DepthStencilState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  m_DepthStencilState.depthTestEnable = depthStencilState.DepthEnable ? VK_TRUE : VK_FALSE;
  m_DepthStencilState.depthWriteEnable = VK_TRUE;
  m_DepthStencilState.depthCompareOp = g_ComparisonFunc[depthStencilState.DepthFunc];
  m_DepthStencilState.depthBoundsTestEnable = VK_FALSE;
  m_DepthStencilState.back.failOp = g_StencilOp[depthStencilState.BackFace.StencilFailOp];
  m_DepthStencilState.back.passOp = g_StencilOp[depthStencilState.BackFace.StencilPassOp];
  m_DepthStencilState.back.compareOp = g_ComparisonFunc[depthStencilState.BackFace.StencilFunc];
  m_DepthStencilState.stencilTestEnable = depthStencilState.StencilEnable ? VK_TRUE : VK_FALSE;
  m_DepthStencilState.front = m_DepthStencilState.back;
  this->m_GfxCreateInfo.pDepthStencilState = &m_DepthStencilState;
}

void PipelineStateObject::SetSampler(rhi::SamplerRef)
{

}

void PipelineStateObject::SetVertexInputLayout(rhi::VertexDeclaration const* vertDecs, uint32 Count)
{
  K3D_ASSERT(vertDecs && Count > 0 && m_BindingDescriptions.empty());
  for (uint32 i = 0; i < Count; i++)
  {
    rhi::VertexDeclaration const& vertDec = vertDecs[i];
    VkVertexInputBindingDescription bindingDesc = { vertDec.BindID, vertDec.Stride, VK_VERTEX_INPUT_RATE_VERTEX };
    VkVertexInputAttributeDescription attribDesc = { vertDec.AttributeIndex, vertDec.BindID, g_VertexFormatTable[vertDec.Format], vertDec.OffSet };
    m_BindingDescriptions.push_back(bindingDesc);
    m_AttributeDescriptions.push_back(attribDesc);
  }
  m_VertexInputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  m_VertexInputState.pNext = NULL;
  m_VertexInputState.vertexBindingDescriptionCount = (uint32)m_BindingDescriptions.size();
  m_VertexInputState.pVertexBindingDescriptions = m_BindingDescriptions.data();
  m_VertexInputState.vertexAttributeDescriptionCount = (uint32)m_AttributeDescriptions.size();
  m_VertexInputState.pVertexAttributeDescriptions = m_AttributeDescriptions.data();
  this->m_GfxCreateInfo.pVertexInputState = &m_VertexInputState;
}

void PipelineStateObject::SetPrimitiveTopology(const rhi::EPrimitiveType Type)
{
  m_InputAssemblyState.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  m_InputAssemblyState.topology = g_PrimitiveTopology[Type];
  this->m_GfxCreateInfo.pInputAssemblyState = &m_InputAssemblyState;
}

void PipelineStateObject::SetRenderTargetFormat(const rhi::RenderTargetFormat &)
{

}

// Init Shaders
std::vector<VkPipelineShaderStageCreateInfo>
GetShaderStageInfo(GpuRef device, rhi::PipelineDesc const & desc)
{
  std::vector<VkPipelineShaderStageCreateInfo> infos;
  for (uint32 i = 0; i < rhi::EShaderType::ShaderTypeNum; i++)
  {
    const auto& code = desc.Shaders[i];
    if (code.RawData.Length() > 0)
    {
      infos.push_back({
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0,
        g_ShaderType[i],
        CreateShaderModule(device, code.RawData),
        code.Desc.EntryFunction.CStr()
      });
    }
  }
  return infos;
}

DynArray<VkVertexInputAttributeDescription> RHIInputAttribs(rhi::VertexInputState ia)
{
  DynArray<VkVertexInputAttributeDescription> iad;
  for (uint32 i = 0; i<rhi::VertexInputState::kMaxVertexBindings; i++)
  {
    auto attrib = ia.Attribs[i];
    if (attrib.Slot == rhi::VertexInputState::kInvalidValue)
      break;
    iad.Append({ i, attrib.Slot, g_VertexFormatTable[attrib.Format], attrib.OffSet });
  }
  return iad;
}

DynArray<VkVertexInputBindingDescription> RHIInputLayouts(rhi::VertexInputState const& ia)
{
  DynArray<VkVertexInputBindingDescription> ibd;
  for (uint32 i = 0; i<rhi::VertexInputState::kMaxVertexLayouts; i++)
  {
    auto layout = ia.Layouts[i];
    if (layout.Stride == rhi::VertexInputState::kInvalidValue)
      break;
    ibd.Append({ i, layout.Stride, g_InputRates[layout.Rate] });
  }
  return ibd;
}

void PipelineStateObject::InitWithDesc(rhi::PipelineDesc const & desc)
{
  m_ShaderStageInfos = GetShaderStageInfo(GetGpuRef(), desc);
  // Init PrimType
  VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = {
    VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO , nullptr, 0,
    g_PrimitiveTopology[desc.PrimitiveTopology] , VK_FALSE
  };

  // Init RasterState
  VkPipelineRasterizationStateCreateInfo rasterizationState = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, NULL };
  rasterizationState.polygonMode = g_FillMode[desc.Rasterizer.FillMode];
  rasterizationState.cullMode = g_CullMode[desc.Rasterizer.CullMode];
  rasterizationState.frontFace = desc.Rasterizer.FrontCCW ? VK_FRONT_FACE_COUNTER_CLOCKWISE : VK_FRONT_FACE_CLOCKWISE;
  rasterizationState.depthClampEnable = desc.Rasterizer.DepthClipEnable ? VK_TRUE : VK_FALSE;
  rasterizationState.rasterizerDiscardEnable = VK_FALSE;
  rasterizationState.depthBiasEnable = VK_FALSE;
  rasterizationState.lineWidth = 1.0f;

  // Init DepthStencilState
  VkPipelineDepthStencilStateCreateInfo depthStencilState = { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO, NULL };
  depthStencilState.depthTestEnable = desc.DepthStencil.DepthEnable ? VK_TRUE : VK_FALSE;
  depthStencilState.depthWriteEnable = VK_TRUE;
  depthStencilState.depthCompareOp = g_ComparisonFunc[desc.DepthStencil.DepthFunc];
  depthStencilState.depthBoundsTestEnable = VK_FALSE;
  depthStencilState.back.failOp = g_StencilOp[desc.DepthStencil.BackFace.StencilFailOp];
  depthStencilState.back.passOp = g_StencilOp[desc.DepthStencil.BackFace.StencilPassOp];
  depthStencilState.back.compareOp = g_ComparisonFunc[desc.DepthStencil.BackFace.StencilFunc];
  depthStencilState.stencilTestEnable = desc.DepthStencil.StencilEnable ? VK_TRUE : VK_FALSE;
  depthStencilState.front = depthStencilState.back;

  // Init BlendState
  VkPipelineColorBlendStateCreateInfo colorBlendState = { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, NULL };
  VkPipelineColorBlendAttachmentState blendAttachmentState[1] = {};
  blendAttachmentState[0].colorWriteMask = 0xf;
  blendAttachmentState[0].blendEnable = desc.Blend.Enable ? VK_TRUE : VK_FALSE;
  colorBlendState.attachmentCount = 1;
  colorBlendState.pAttachments = blendAttachmentState;
  /* @deprecated
  struct VIADLess {
  bool operator() (const VkVertexInputBindingDescription& lhs, const VkVertexInputBindingDescription& rhs) const {
  return lhs.binding < rhs.binding || lhs.stride < rhs.stride || lhs.inputRate < rhs.inputRate;
  }
  };
  std::set <VkVertexInputBindingDescription, VIADLess> bindings;
  // Init VertexLayout
  for (uint32 i = 0; i < desc.VertexLayout.Count(); i++)
  {
  rhi::VertexDeclaration const& vertDec = desc.VertexLayout[i];
  VkVertexInputBindingDescription bindingDesc = { vertDec.BindID, vertDec.Stride, VK_VERTEX_INPUT_RATE_VERTEX };
  VkVertexInputAttributeDescription attribDesc = { vertDec.AttributeIndex, vertDec.BindID, g_VertexFormatTable[vertDec.Format], vertDec.OffSet };
  bindings.insert(bindingDesc);
  m_AttributeDescriptions.push_back(attribDesc);
  }
  m_BindingDescriptions.assign(bindings.begin(), bindings.end());
  */
  auto IAs = RHIInputAttribs(desc.InputState);
  auto IBs = RHIInputLayouts(desc.InputState);
  VkPipelineVertexInputStateCreateInfo vertexInputState = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, NULL };
  vertexInputState.vertexBindingDescriptionCount = IBs.Count();
  vertexInputState.pVertexBindingDescriptions = IBs.Data();
  vertexInputState.vertexAttributeDescriptionCount = IAs.Count();
  vertexInputState.pVertexAttributeDescriptions = IAs.Data();

  VkPipelineViewportStateCreateInfo vpInfo = { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
  vpInfo.viewportCount = 1;
  vpInfo.scissorCount = 1;

  VkPipelineDynamicStateCreateInfo dynamicState = { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
  std::vector<VkDynamicState> dynamicStateEnables;
  dynamicStateEnables.push_back(VK_DYNAMIC_STATE_VIEWPORT);
  dynamicStateEnables.push_back(VK_DYNAMIC_STATE_SCISSOR);
  dynamicStateEnables.push_back(VK_DYNAMIC_STATE_LINE_WIDTH);
  dynamicState.pDynamicStates = dynamicStateEnables.data();
  dynamicState.dynamicStateCount = (uint32)dynamicStateEnables.size();

  VkPipelineMultisampleStateCreateInfo msInfo = { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
  msInfo.pSampleMask = NULL;
  msInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

  this->m_GfxCreateInfo = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
  this->m_GfxCreateInfo.stageCount = (uint32)m_ShaderStageInfos.size();
  this->m_GfxCreateInfo.pStages = m_ShaderStageInfos.data();

  this->m_GfxCreateInfo.pInputAssemblyState = &inputAssemblyState;
  this->m_GfxCreateInfo.pRasterizationState = &rasterizationState;
  this->m_GfxCreateInfo.pDepthStencilState = &depthStencilState;
  this->m_GfxCreateInfo.pColorBlendState = &colorBlendState;
  this->m_GfxCreateInfo.pVertexInputState = &vertexInputState;
  this->m_GfxCreateInfo.pDynamicState = &dynamicState;
  this->m_GfxCreateInfo.pMultisampleState = &msInfo;
  this->m_GfxCreateInfo.pViewportState = &vpInfo;

  m_RenderPass = RHIRoot::GetViewport(0)->GetRenderPass();

  this->m_GfxCreateInfo.renderPass = m_RenderPass;
  this->m_GfxCreateInfo.layout = m_PipelineLayout->NativeHandle();

  // Finalize
  Finalize();
}

void PipelineStateObject::Destroy()
{
  for (auto iter : m_ShaderStageInfos)
  {
    if (iter.module)
    {
      vkDestroyShaderModule(GetRawDevice(), iter.module, nullptr);
    }
  }
  if (m_PipelineCache)
  {
    vkDestroyPipelineCache(GetRawDevice(), m_PipelineCache, nullptr);
    VKLOG(Info, "PipelineCache  Destroyed.. -- %0x.", m_PipelineCache);
    m_PipelineCache = VK_NULL_HANDLE;
  }
  if (m_Pipeline)
  {
    vkDestroyPipeline(GetRawDevice(), m_Pipeline, nullptr);
    VKLOG(Info, "PipelineStateObject  Destroyed.. -- %0x.", m_Pipeline);
    m_Pipeline = VK_NULL_HANDLE;
  }
}

PipelineLayout::PipelineLayout(Device::Ptr pDevice, rhi::PipelineLayoutDesc const & desc)
  : PipelineLayout::ThisObj(pDevice)
  , m_DescSetLayout(nullptr)
  , m_DescSet()
{
  InitWithDesc(desc);
}

PipelineLayout::~PipelineLayout()
{
  Destroy();
}

void PipelineLayout::Destroy()
{
  if (m_NativeObj == VK_NULL_HANDLE)
    return;
  vkDestroyPipelineLayout(NativeDevice(), m_NativeObj, nullptr);
  VKLOG(Info, "PipelineLayout Destroyed . -- %0x.", m_NativeObj);
  m_NativeObj = VK_NULL_HANDLE;
}

uint64 BindingHash(Binding const& binding)
{
  return (uint64)(1 << (3 + binding.VarNumber)) | binding.VarStage;
}

bool operator<(Binding const &lhs, Binding const &rhs)
{
  return rhs.VarStage < lhs.VarStage && rhs.VarNumber < lhs.VarNumber;
}

BindingArray ExtractBindingsFromTable(::k3d::DynArray<Binding> const& bindings)
{
  //	merge image sampler
  std::map<uint64, Binding> bindingMap;
  for (auto const & binding : bindings)
  {
    uint64 hash = BindingHash(binding);
    if (bindingMap.find(hash) == bindingMap.end())
    {
      bindingMap.insert({ hash, binding });
    }
    else // binding slot override
    {
      auto & overrideBinding = bindingMap[hash];
      if (EBindType((uint32)overrideBinding.VarType | (uint32)binding.VarType)
        == EBindType::ESamplerImageCombine)
      {
        overrideBinding.VarType = EBindType::ESamplerImageCombine;
      }
    }
  }

  BindingArray array;
  for (auto & p : bindingMap)
  {
    array.Append(RHIBinding2VkBinding(p.second));
  }
  return array;
}

void PipelineLayout::InitWithDesc(rhi::PipelineLayoutDesc const & desc)
{
  DescriptorAllocator::Options options;
  BindingArray array = ExtractBindingsFromTable(desc.Bindings);
  auto alloc = m_Device->NewDescriptorAllocator(16, array);
  m_DescSetLayout = m_Device->NewDescriptorSetLayout(array);
  m_DescSet = rhi::DescriptorRef(DescriptorSet::CreateDescSet(alloc, m_DescSetLayout->GetNativeHandle(), array, m_Device));

  VkPipelineLayoutCreateInfo pPipelineLayoutCreateInfo = {};
  pPipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pPipelineLayoutCreateInfo.pNext = NULL;
  pPipelineLayoutCreateInfo.setLayoutCount = 1;
  pPipelineLayoutCreateInfo.pSetLayouts = &m_DescSetLayout->m_DescriptorSetLayout;
  K3D_VK_VERIFY(vkCreatePipelineLayout(NativeDevice(), &pPipelineLayoutCreateInfo, nullptr, &m_NativeObj));
}

/*
enum class EBindType
{
EUndefined		= 0,
EBlock			= 0x1,
ESampler		= 0x1 << 1,
EStorageImage	= 0x1 << 2,
EStorageBuffer	= 0x1 << 3,
EConstants		= 0x000000010
};
*/
VkDescriptorType RHIDataType2VkType(rhi::shc::EBindType const & type)
{
  switch (type)
  {
  case rhi::shc::EBindType::EBlock:
    return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  case rhi::shc::EBindType::ESampler:
    return VK_DESCRIPTOR_TYPE_SAMPLER;
  case rhi::shc::EBindType::ESampledImage:
    return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
  case rhi::shc::EBindType::ESamplerImageCombine:
    return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  case rhi::shc::EBindType::EStorageImage:
    return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
  case rhi::shc::EBindType::EStorageBuffer:
    return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  }
  return VK_DESCRIPTOR_TYPE_MAX_ENUM;
}

VkDescriptorSetLayoutBinding RHIBinding2VkBinding(rhi::shc::Binding const & binding)
{
  VkDescriptorSetLayoutBinding vkbinding = {};
  vkbinding.descriptorType = RHIDataType2VkType(binding.VarType);
  vkbinding.binding = binding.VarNumber;
  vkbinding.stageFlags = g_ShaderType[binding.VarStage];
  vkbinding.descriptorCount = 1;
  return vkbinding;
}

DescriptorAllocator::DescriptorAllocator(
  Device::Ptr pDevice,
  DescriptorAllocator::Options const &option,
  uint32 maxSets,
  BindingArray const& bindings)
  : DeviceChild(pDevice)
  , m_Options(option)
  , m_Pool(VK_NULL_HANDLE)
{
  Initialize(maxSets, bindings);
}

DescriptorAllocator::~DescriptorAllocator()
{
  Destroy();
}

void DescriptorAllocator::Initialize(uint32 maxSets, BindingArray const& bindings)
{
  std::map<VkDescriptorType, uint32_t> mappedCounts;
  for (const auto& binding : bindings)
  {
    VkDescriptorType descType = binding.descriptorType;
    auto it = mappedCounts.find(descType);
    if (it != mappedCounts.end())
    {
      ++mappedCounts[descType];
    }
    else
    {
      mappedCounts[descType] = 1;
    }
  }

  std::vector<VkDescriptorPoolSize> typeCounts;
  for (const auto &mc : mappedCounts)
  {
    typeCounts.push_back({ mc.first, mc.second });
  }

  VkDescriptorPoolCreateInfo createInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
  createInfo.flags = m_Options.CreateFlags;
  createInfo.maxSets = 1;
  createInfo.poolSizeCount = typeCounts.size();
  createInfo.pPoolSizes = typeCounts.size() == 0 ? nullptr : typeCounts.data();
  K3D_VK_VERIFY(vkCreateDescriptorPool(GetRawDevice(), &createInfo, NULL, &m_Pool));
}

void DescriptorAllocator::Destroy()
{
  if (VK_NULL_HANDLE == m_Pool || !GetRawDevice())
    return;
  vkDestroyDescriptorPool(GetRawDevice(), m_Pool, nullptr);
  m_Pool = VK_NULL_HANDLE;
  VKLOG(Info, "DescriptorAllocator-destroying vkDestroyDescriptorPool...");
}

DescriptorSetLayout::DescriptorSetLayout(Device::Ptr pDevice, BindingArray const & bindings)
  : DeviceChild(pDevice)
  , m_DescriptorSetLayout(VK_NULL_HANDLE)
{
  Initialize(bindings);
}

DescriptorSetLayout::~DescriptorSetLayout()
{
  //Destroy();
}

void DescriptorSetLayout::Initialize(BindingArray const & bindings)
{
  VkDescriptorSetLayoutCreateInfo createInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
  createInfo.bindingCount = bindings.Count();
  createInfo.pBindings = bindings.Count() == 0 ? nullptr : bindings.Data();
  K3D_VK_VERIFY(vkCreateDescriptorSetLayout(GetRawDevice(), &createInfo, nullptr, &m_DescriptorSetLayout));
}

void DescriptorSetLayout::Destroy()
{
  if (VK_NULL_HANDLE == m_DescriptorSetLayout || !GetRawDevice())
    return;
  vkDestroyDescriptorSetLayout(GetRawDevice(), m_DescriptorSetLayout, nullptr);
  VKLOG(Info, "DescriptorSetLayout  destroying... -- %0x.", m_DescriptorSetLayout);
  m_DescriptorSetLayout = VK_NULL_HANDLE;
}

DescriptorSet::DescriptorSet(DescriptorAllocRef descriptorAllocator, VkDescriptorSetLayout layout, BindingArray const & bindings, Device::Ptr pDevice)
  : DeviceChild(pDevice)
  , m_DescriptorAllocator(descriptorAllocator)
  , m_Bindings(bindings)
{
  Initialize(layout, bindings);
}


DescriptorSet * DescriptorSet::CreateDescSet(DescriptorAllocRef descriptorPool, VkDescriptorSetLayout layout, BindingArray const & bindings, Device::Ptr pDevice)
{
  return new DescriptorSet(descriptorPool, layout, bindings, pDevice);
}

DescriptorSet::~DescriptorSet()
{
  Destroy();
}

void DescriptorSet::Update(uint32 bindSet, rhi::SamplerRef pRHISampler)
{
  auto pSampler = StaticPointerCast<Sampler>(pRHISampler);
  VkDescriptorImageInfo imageInfo = { pSampler->NativeHandle(), VK_NULL_HANDLE, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
  m_BoundDescriptorSet[bindSet].pImageInfo = &imageInfo;
  vkUpdateDescriptorSets(GetRawDevice(), 1, &m_BoundDescriptorSet[bindSet], 0, NULL);
  VKLOG(Info, "%s , Set (0x%0x) updated with sampler(location:0x%x).", __K3D_FUNC__, m_DescriptorSet, pSampler->NativeHandle());
}

void DescriptorSet::Update(uint32 bindSet, rhi::GpuResourceRef gpuResource)
{
  auto desc = gpuResource->GetDesc();
  switch (desc.Type)
  {
  case rhi::EGT_Buffer:
  {
    VkDescriptorBufferInfo bufferInfo = { (VkBuffer)gpuResource->GetLocation(), 0, gpuResource->GetSize() };
    m_BoundDescriptorSet[bindSet].pBufferInfo = &bufferInfo;
    vkUpdateDescriptorSets(GetRawDevice(), 1, &m_BoundDescriptorSet[bindSet], 0, NULL);
    VKLOG(Info, "%s , Set (0x%0x) updated with buffer(location:0x%x, size:%d).", __K3D_FUNC__, m_DescriptorSet,
      gpuResource->GetLocation(), gpuResource->GetSize());
    break;
  }
  case rhi::EGT_Texture1D:
  case rhi::EGT_Texture2D:
  case rhi::EGT_Texture3D:
  case rhi::EGT_Texture2DArray: // combined/seperated image sampler should be considered
  {
    auto pTex = k3d::StaticPointerCast<Texture>(gpuResource);
    auto rSampler = k3d::StaticPointerCast<Sampler>(pTex->GetSampler());
    assert(rSampler);
    auto srv = k3d::StaticPointerCast<ShaderResourceView>(pTex->GetResourceView());
    VkDescriptorImageInfo imageInfo = { rSampler->NativeHandle(), srv->NativeImageView(), pTex->GetImageLayout() }; //TODO : sampler shouldn't be null
    m_BoundDescriptorSet[bindSet].pImageInfo = &imageInfo;
    vkUpdateDescriptorSets(GetRawDevice(), 1, &m_BoundDescriptorSet[bindSet], 0, NULL);
    VKLOG(Info, "%s , Set (0x%0x) updated with image(location:0x%x, size:%d).", __K3D_FUNC__, m_DescriptorSet,
      gpuResource->GetLocation(), gpuResource->GetSize());
    break;
  }
  }
}

uint32 DescriptorSet::GetSlotNum() const
{
  return (uint32)m_BoundDescriptorSet.size();
}

void DescriptorSet::Initialize(VkDescriptorSetLayout layout, BindingArray const & bindings)
{
  std::vector<VkDescriptorSetLayout> layouts = { layout };
  VkDescriptorSetAllocateInfo allocInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
  allocInfo.descriptorPool = m_DescriptorAllocator->m_Pool;
  allocInfo.descriptorSetCount = static_cast<uint32_t>(layouts.size());
  allocInfo.pSetLayouts = layouts.empty() ? nullptr : layouts.data();
  K3D_VK_VERIFY(vkAllocateDescriptorSets(GetRawDevice(), &allocInfo, &m_DescriptorSet));
  VKLOG(Info, "%s , Set (0x%0x) created.", __K3D_FUNC__, m_DescriptorSet);

  for (auto& binding : m_Bindings)
  {
    VkWriteDescriptorSet entry = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
    entry.dstSet = m_DescriptorSet;
    entry.descriptorCount = 1;
    entry.dstArrayElement = 0;
    entry.dstBinding = binding.binding;
    entry.descriptorType = binding.descriptorType;
    m_BoundDescriptorSet.push_back(entry);
  }
}

void DescriptorSet::Destroy()
{
  if (VK_NULL_HANDLE == m_DescriptorSet || !GetRawDevice())
    return;

  if (m_DescriptorAllocator)
  {
    //const auto& options = m_DescriptorAllocator->m_Options;
    //if( options.hasFreeDescriptorSetFlag() ) {
    VkDescriptorSet descSets[1] = { m_DescriptorSet };
    vkFreeDescriptorSets(GetRawDevice(), m_DescriptorAllocator->m_Pool, 1, descSets);
    m_DescriptorSet = VK_NULL_HANDLE;
    //}
  }
  VKRHI_METHOD_TRACE
}

RenderPass::RenderPass(Device::Ptr pDevice, RenderpassOptions const & options)
  : DeviceChild(pDevice)
  , m_Options(options)
{
  Initialize(options);
}

RenderPass::RenderPass(Device::Ptr pDevice, RenderTargetLayout const & rtl)
  : DeviceChild(pDevice)
{
  Initialize(rtl);
}

RenderPass::~RenderPass()
{
  Destroy();
}

void RenderPass::NextSubpass()
{
  m_GfxContext->NextSubpass(VK_SUBPASS_CONTENTS_INLINE);
  ++mSubpass;
}

void RenderPass::Initialize(RenderpassOptions const & options)
{
  if (VK_NULL_HANDLE != m_RenderPass)
  {
    return;
  }

  m_Options = options;

  K3D_ASSERT(options.m_Attachments.size() > 0);
  K3D_ASSERT(options.m_Subpasses.size() > 0);

  // Populate attachment descriptors
  const size_t numAttachmentDesc = options.m_Attachments.size();
  mAttachmentDescriptors.resize(numAttachmentDesc);
  mAttachmentClearValues.resize(numAttachmentDesc);
  for (size_t i = 0; i < numAttachmentDesc; ++i)
  {
    mAttachmentDescriptors[i] = options.m_Attachments[i].GetDescription();
    mAttachmentClearValues[i] = options.m_Attachments[i].GetClearValue();
  }

  // Populate attachment references
  const size_t numSubPasses = options.m_Subpasses.size();
  std::vector<Subpass::AttachReferences> subPassAttachmentRefs(numSubPasses);
  for (size_t i = 0; i < numSubPasses; ++i)
  {
    const auto& subPass = options.m_Subpasses[i];

    // Color attachments
    {
      // Allocate elements for color attachments
      const size_t numColorAttachments = subPass.m_ColorAttachments.size();
      subPassAttachmentRefs[i].Color.resize(numColorAttachments);
      subPassAttachmentRefs[i].Resolve.resize(numColorAttachments);

      // Populate color and resolve attachments
      for (size_t j = 0; j < numColorAttachments; ++j)
      {
        // color
        uint32_t colorAttachmentIndex = subPass.m_ColorAttachments[j];
        VkImageLayout colorImageLayout = mAttachmentDescriptors[colorAttachmentIndex].initialLayout;;
        subPassAttachmentRefs[i].Color[j] = {};
        subPassAttachmentRefs[i].Color[j].attachment = colorAttachmentIndex;
        subPassAttachmentRefs[i].Color[j].layout = colorImageLayout;
        // resolve
        uint32_t resolveAttachmentIndex = subPass.m_ResolveAttachments[j];
        subPassAttachmentRefs[i].Resolve[j] = {};
        subPassAttachmentRefs[i].Resolve[j].attachment = resolveAttachmentIndex;
        subPassAttachmentRefs[i].Resolve[j].layout = colorImageLayout; // Not a mistake, this is on purpose
      }
    }

    // Depth/stencil attachment
    std::vector<VkAttachmentReference> depthStencilAttachmentRef;
    if (!subPass.m_DepthStencilAttachment.empty())
    {
      // Allocate elements for depth/stencil attachments
      subPassAttachmentRefs[i].Depth.resize(1);

      // Populate depth/stencil attachments
      uint32_t attachmentIndex = subPass.m_DepthStencilAttachment[0];
      subPassAttachmentRefs[i].Depth[0] = {};
      subPassAttachmentRefs[i].Depth[0].attachment = attachmentIndex;
      subPassAttachmentRefs[i].Depth[0].layout = mAttachmentDescriptors[attachmentIndex].initialLayout;
    }

    // Preserve attachments
    if (!subPass.m_PreserveAttachments.empty())
    {
      subPassAttachmentRefs[i].Preserve = subPass.m_PreserveAttachments;
    }
  }

  // Populate sub passes
  std::vector<VkSubpassDescription> subPassDescs(numSubPasses);
  for (size_t i = 0; i < numSubPasses; ++i)
  {
    const auto& colorAttachmentRefs = subPassAttachmentRefs[i].Color;
    const auto& resolveAttachmentRefs = subPassAttachmentRefs[i].Resolve;
    const auto& depthStencilAttachmentRef = subPassAttachmentRefs[i].Depth;
    const auto& preserveAttachmentRefs = subPassAttachmentRefs[i].Preserve;

    bool noResolves = true;
    for (const auto& attachRef : resolveAttachmentRefs)
    {
      if (VK_ATTACHMENT_UNUSED != attachRef.attachment)
      {
        noResolves = false;
        break;
      }
    }

    subPassDescs[i] = {};
    auto& desc = subPassDescs[i];
    desc.pipelineBindPoint = options.m_Subpasses[i].m_PipelineBindPoint;
    desc.flags = 0;
    desc.inputAttachmentCount = 0;
    desc.pInputAttachments = nullptr;
    desc.colorAttachmentCount = static_cast<uint32_t>(colorAttachmentRefs.size());
    desc.pColorAttachments = colorAttachmentRefs.empty() ? nullptr : colorAttachmentRefs.data();
    desc.pResolveAttachments = (resolveAttachmentRefs.empty() || noResolves) ? nullptr : resolveAttachmentRefs.data();
    desc.pDepthStencilAttachment = depthStencilAttachmentRef.empty() ? nullptr : depthStencilAttachmentRef.data();
    desc.preserveAttachmentCount = static_cast<uint32_t>(preserveAttachmentRefs.size());
    desc.pPreserveAttachments = preserveAttachmentRefs.empty() ? nullptr : preserveAttachmentRefs.data();
  }

  // Cache the subpass sample counts
  for (auto& subpass : m_Options.m_Subpasses)
  {
    VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT;
    // Look at color attachments first..
    if (!subpass.m_ColorAttachments.empty())
    {
      uint32_t attachmentIndex = subpass.m_ColorAttachments[0];
      sampleCount = m_Options.m_Attachments[attachmentIndex].m_Description.samples;
    }
    // ..and then look at depth attachments
    if ((VK_SAMPLE_COUNT_1_BIT == sampleCount) && (!subpass.m_DepthStencilAttachment.empty()))
    {
      uint32_t attachmentIndex = subpass.m_DepthStencilAttachment[0];
      sampleCount = m_Options.m_Attachments[attachmentIndex].m_Description.samples;
    }
    // Cache it
    mSubpassSampleCounts.push_back(sampleCount);
  }

  std::vector<VkSubpassDependency> dependencies;
  for (auto& subpassDep : m_Options.m_SubpassDependencies) {
    dependencies.push_back(subpassDep.m_Dependency);
  }

  // Create render pass
  VkRenderPassCreateInfo renderPassCreateInfo = {};
  renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  renderPassCreateInfo.pNext = nullptr;
  renderPassCreateInfo.attachmentCount = static_cast<uint32_t>(mAttachmentDescriptors.size());
  renderPassCreateInfo.pAttachments = mAttachmentDescriptors.data();
  renderPassCreateInfo.subpassCount = static_cast<uint32_t>(subPassDescs.size());
  renderPassCreateInfo.pSubpasses = subPassDescs.empty() ? nullptr : subPassDescs.data();
  renderPassCreateInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
  renderPassCreateInfo.pDependencies = dependencies.empty() ? nullptr : dependencies.data();
  K3D_VK_VERIFY(vkCreateRenderPass(GetRawDevice(), &renderPassCreateInfo, nullptr, &m_RenderPass));
}

void RenderPass::Initialize(RenderTargetLayout const & rtl)
{
  VkSubpassDescription subpass = {};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.flags = 0;
  subpass.inputAttachmentCount = 0;
  subpass.pInputAttachments = nullptr;
  subpass.colorAttachmentCount = rtl.GetNumColorAttachments();
  subpass.pColorAttachments = rtl.GetColorAttachmentReferences();
  subpass.pResolveAttachments = rtl.GetResolveAttachmentReferences();
  subpass.pDepthStencilAttachment = rtl.GetDepthStencilAttachmentReference();
  subpass.preserveAttachmentCount = 0;
  subpass.pPreserveAttachments = nullptr;

  VkRenderPassCreateInfo renderPassCreateInfo = {};
  renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  renderPassCreateInfo.pNext = nullptr;

  renderPassCreateInfo.attachmentCount = rtl.GetNumAttachments();
  renderPassCreateInfo.pAttachments = rtl.GetAttachmentDescriptions();

  renderPassCreateInfo.subpassCount = 1;
  renderPassCreateInfo.pSubpasses = &subpass;
  renderPassCreateInfo.dependencyCount = 0;
  renderPassCreateInfo.pDependencies = nullptr;
  K3D_VK_VERIFY(vkCreateRenderPass(GetRawDevice(), &renderPassCreateInfo, nullptr, &m_RenderPass));
}

void RenderPass::Destroy()
{
  if (VK_NULL_HANDLE == m_RenderPass)
  {
    return;
  }
  VKLOG(Info, "RenderPass destroy... -- %0x.", m_RenderPass);
  vkDestroyRenderPass(GetRawDevice(), m_RenderPass, nullptr);
  m_RenderPass = VK_NULL_HANDLE;
}

FrameBuffer::FrameBuffer(Device::Ptr pDevice, VkRenderPass renderPass, FrameBuffer::Option const& op)
  : DeviceChild(pDevice)
  , m_RenderPass(renderPass)
  , m_Width(op.Width)
  , m_Height(op.Height)
{
  VKRHI_METHOD_TRACE
    for (auto& elem : op.Attachments)
    {
      if (elem.ImageAttachment)
      {
        continue;
      }
    }

  std::vector<VkImageView> attachments;
  for (const auto& elem : op.Attachments) {
    attachments.push_back(elem.ImageAttachment);
  }

  VkFramebufferCreateInfo createInfo = {};
  createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  createInfo.pNext = nullptr;
  createInfo.renderPass = m_RenderPass;
  createInfo.attachmentCount = static_cast<uint32_t>(op.Attachments.size());
  createInfo.pAttachments = attachments.data();
  createInfo.width = m_Width;
  createInfo.height = m_Height;
  createInfo.layers = 1;
  createInfo.flags = 0;
  K3D_VK_VERIFY(vkCreateFramebuffer(GetRawDevice(), &createInfo, nullptr, &m_FrameBuffer));
}

FrameBuffer::FrameBuffer(Device::Ptr pDevice, RenderPass * renderPass, RenderTargetLayout const &)
  : DeviceChild(pDevice)
{
}

FrameBuffer::~FrameBuffer()
{
  if (VK_NULL_HANDLE == m_FrameBuffer)
  {
    return;
  }
  VKLOG(Info, "FrameBuffer destroy... -- %0x.", m_FrameBuffer);
  vkDestroyFramebuffer(GetRawDevice(), m_FrameBuffer, nullptr);
  m_FrameBuffer = VK_NULL_HANDLE;
}

FrameBuffer::Attachment::Attachment(VkFormat format, VkSampleCountFlagBits samples)
{
  VkImageAspectFlags aspectMask = DetermineAspectMask(Format);
  if (VK_IMAGE_ASPECT_COLOR_BIT == aspectMask)
  {
    FormatFeatures = VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT;
  }
  else
  {
    FormatFeatures = VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;
  }
}

SwapChain::SwapChain(Device::Ptr pDevice) : DeviceChild(pDevice), m_ReserveBackBufferCount(0)
{
}

SwapChain::~SwapChain()
{
  Destroy();
}

void SwapChain::Initialize(void * WindowHandle, rhi::GfxSetting & gfxSetting)
{
  InitSurface(WindowHandle);
  VkPresentModeKHR swapchainPresentMode = ChoosePresentMode();
  std::pair<VkFormat, VkColorSpaceKHR> chosenFormat = ChooseFormat(gfxSetting);
  m_SelectedPresentQueueFamilyIndex = ChooseQueueIndex();
  m_SwapchainExtent = { gfxSetting.Width, gfxSetting.Height };
  VkSurfaceCapabilitiesKHR surfProperties;
  K3D_VK_VERIFY(GetGpuRef()->GetSurfaceCapabilitiesKHR(m_Surface, &surfProperties));
  m_SwapchainExtent = surfProperties.currentExtent;
  /*gfxSetting.Width = m_SwapchainExtent.width;
  gfxSetting.Height = m_SwapchainExtent.height;*/
  uint32 desiredNumBuffers = kMath::Clamp(
    gfxSetting.BackBufferCount,
    surfProperties.minImageCount,
    surfProperties.maxImageCount);
  m_DesiredBackBufferCount = desiredNumBuffers;
  InitSwapChain(m_DesiredBackBufferCount, chosenFormat, swapchainPresentMode, surfProperties.currentTransform);
  K3D_VK_VERIFY(vkGetSwapchainImagesKHR(GetRawDevice(), m_SwapChain, &m_ReserveBackBufferCount, nullptr));
  m_ColorImages.resize(m_ReserveBackBufferCount);
  K3D_VK_VERIFY(vkGetSwapchainImagesKHR(GetRawDevice(), m_SwapChain, &m_ReserveBackBufferCount, m_ColorImages.data()));
  gfxSetting.BackBufferCount = m_ReserveBackBufferCount;
  VKLOG(Info, "[SwapChain::Initialize] desired imageCount=%d, reserved imageCount = %d.", m_DesiredBackBufferCount, m_ReserveBackBufferCount);
}

uint32 SwapChain::AcquireNextImage(PtrSemaphore presentSemaphore, PtrFence pFence)
{
  uint32 imageIndex;
  VkResult result = vkAcquireNextImageKHR(GetRawDevice(), m_SwapChain, UINT64_MAX,
    presentSemaphore ? presentSemaphore->m_Semaphore : VK_NULL_HANDLE,
    pFence ? pFence->NativeHandle() : VK_NULL_HANDLE,
    &imageIndex);
  switch (result)
  {
  case VK_SUCCESS:
  case VK_SUBOPTIMAL_KHR:
    break;
  case VK_ERROR_OUT_OF_DATE_KHR:
    //OnWindowSizeChanged();
    VKLOG(Info, "Swapchain need update");
  default:
    break;
  }
  return imageIndex;
}

VkResult SwapChain::Present(uint32 imageIndex, PtrSemaphore renderingFinishSemaphore)
{
  VkSemaphore renderSem = renderingFinishSemaphore ? renderingFinishSemaphore->GetNativeHandle() : VK_NULL_HANDLE;
  VkPresentInfoKHR presentInfo = { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR, nullptr };
  presentInfo.pImageIndices = &imageIndex;
  presentInfo.swapchainCount = m_SwapChain ? 1 : 0;
  presentInfo.pSwapchains = &m_SwapChain;
  presentInfo.waitSemaphoreCount = renderSem ? 1 : 0;
  presentInfo.pWaitSemaphores = &renderSem;
  return vkQueuePresentKHR(GetImmCmdQueue()->GetNativeHandle(), &presentInfo);
}

void SwapChain::InitSurface(void * WindowHandle)
{
#if K3DPLATFORM_OS_WIN
  VkWin32SurfaceCreateInfoKHR SurfaceCreateInfo = {};
  SurfaceCreateInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
  SurfaceCreateInfo.hinstance = GetModuleHandle(nullptr);
  SurfaceCreateInfo.hwnd = (HWND)WindowHandle;
  K3D_VK_VERIFY(vkCreateWin32SurfaceKHR(GetGpuRef()->GetInstance()->m_Instance, &SurfaceCreateInfo, nullptr, &m_Surface));
#elif K3DPLATFORM_OS_ANDROID
  VkAndroidSurfaceCreateInfoKHR SurfaceCreateInfo = {};
  SurfaceCreateInfo.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
  SurfaceCreateInfo.window = (ANativeWindow*)WindowHandle;
  K3D_VK_VERIFY(vkCreateAndroidSurfaceKHR(GetGpuRef()->GetInstance()->m_Instance, &SurfaceCreateInfo, nullptr, &m_Surface));
#endif
}

VkPresentModeKHR SwapChain::ChoosePresentMode()
{
  uint32_t presentModeCount;
  K3D_VK_VERIFY(GetGpuRef()->GetSurfacePresentModesKHR(m_Surface, &presentModeCount, NULL));
  std::vector<VkPresentModeKHR> presentModes(presentModeCount);
  K3D_VK_VERIFY(GetGpuRef()->GetSurfacePresentModesKHR(m_Surface, &presentModeCount, presentModes.data()));
  VkPresentModeKHR swapchainPresentMode = VK_PRESENT_MODE_FIFO_KHR;
  for (size_t i = 0; i < presentModeCount; i++)
  {
    if (presentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
    {
      swapchainPresentMode = VK_PRESENT_MODE_MAILBOX_KHR;
      break;
    }
    if ((swapchainPresentMode != VK_PRESENT_MODE_MAILBOX_KHR) && (presentModes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR))
    {
      swapchainPresentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
    }
  }
  return swapchainPresentMode;
}

std::pair<VkFormat, VkColorSpaceKHR> SwapChain::ChooseFormat(rhi::GfxSetting & gfxSetting)
{
  uint32_t formatCount;
  K3D_VK_VERIFY(GetGpuRef()->GetSurfaceFormatsKHR(m_Surface, &formatCount, NULL));
  std::vector<VkSurfaceFormatKHR> surfFormats(formatCount);
  K3D_VK_VERIFY(GetGpuRef()->GetSurfaceFormatsKHR(m_Surface, &formatCount, surfFormats.data()));
  VkFormat colorFormat;
  VkColorSpaceKHR colorSpace;
  if (formatCount == 1 && surfFormats[0].format == VK_FORMAT_UNDEFINED)
  {
    colorFormat = g_FormatTable[gfxSetting.ColorFormat];
  }
  else
  {
    K3D_ASSERT(formatCount >= 1);
    colorFormat = surfFormats[0].format;
  }
  colorSpace = surfFormats[0].colorSpace;
  return std::make_pair(colorFormat, colorSpace);
}

int SwapChain::ChooseQueueIndex()
{
  uint32 chosenIndex = 0;
  std::vector<VkBool32> queuePresentSupport(GetDevice()->GetQueueCount());

  for (uint32_t i = 0; i < GetDevice()->GetQueueCount(); ++i)
  {
    GetGpuRef()->GetSurfaceSupportKHR(i, m_Surface, &queuePresentSupport[i]);
    if (queuePresentSupport[i])
    {
      chosenIndex = i;
      break;
    }
  }

  return chosenIndex;
}

void SwapChain::InitSwapChain(uint32 numBuffers, std::pair<VkFormat, VkColorSpaceKHR> color, VkPresentModeKHR mode, VkSurfaceTransformFlagBitsKHR pretran)
{
  VkSwapchainCreateInfoKHR swapchainCI = { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
  swapchainCI.surface = m_Surface;
  swapchainCI.minImageCount = numBuffers;
  swapchainCI.imageFormat = color.first;
  m_ColorAttachFmt = color.first;
  swapchainCI.imageColorSpace = color.second;
  swapchainCI.imageExtent = m_SwapchainExtent;
  swapchainCI.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  swapchainCI.preTransform = pretran;
  swapchainCI.imageArrayLayers = 1;
  swapchainCI.queueFamilyIndexCount = VK_SHARING_MODE_EXCLUSIVE;
  swapchainCI.queueFamilyIndexCount = 0;
  swapchainCI.pQueueFamilyIndices = NULL;
  swapchainCI.presentMode = mode;
  swapchainCI.oldSwapchain = VK_NULL_HANDLE;
  swapchainCI.clipped = VK_TRUE;
  swapchainCI.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  K3D_VK_VERIFY(vkCreateSwapchainKHR(GetRawDevice(), &swapchainCI, nullptr, &m_SwapChain));
  VKLOG(Info, "Init Swapchain with ColorFmt(%d)", m_ColorAttachFmt);
}

void SwapChain::Destroy()
{
  VKLOG(Info, "SwapChain Destroying..");
  GetDevice()->WaitIdle();
  if (m_SwapChain)
  {
    VkDevice device = GetRawDevice();
    //vkDestroySwapchainKHR(device, m_SwapChain, nullptr);
    m_SwapChain = VK_NULL_HANDLE;
  }
  if (m_Surface)
  {
    auto ref = GetGpuRef();
    vkDestroySurfaceKHR(ref->GetInstance()->m_Instance, m_Surface, nullptr);
    m_Surface = VK_NULL_HANDLE;
  }
}

RenderViewport::RenderViewport(
  Device::Ptr pDevice,
  void * windowHandle,
  rhi::GfxSetting & setting)
  : m_NumBufferCount(-1)
  , DeviceChild(pDevice)
  , m_CurFrameId(0)
{
  if (pDevice)
  {
    m_pSwapChain = pDevice->NewSwapChain(setting);
    m_pSwapChain->Initialize(windowHandle, setting);
    m_NumBufferCount = m_pSwapChain->GetBackBufferCount();
    VKLOG(Info, "RenderViewport-Initialized: width(%d), height(%d), swapImage num(%d).",
      setting.Width, setting.Height, m_NumBufferCount);
    m_PresentSemaphore = GetDevice()->NewSemaphore();
    m_RenderSemaphore = GetDevice()->NewSemaphore();
  }
}

RenderViewport::~RenderViewport()
{
  VKLOG(Info, "RenderViewport-Destroyed..");
}

bool RenderViewport::InitViewport(void *windowHandle, rhi::IDevice * pDevice, rhi::GfxSetting & gfxSetting)
{
  AllocateDefaultRenderPass(gfxSetting);
  AllocateRenderTargets(gfxSetting);
  return true;
}

void RenderViewport::PrepareNextFrame()
{
  VKRHI_METHOD_TRACE
    m_CurFrameId = m_pSwapChain->AcquireNextImage(m_PresentSemaphore, nullptr);
  std::stringstream stream;
  stream << "Current Frame Id = " << m_CurFrameId << " acquiring " << std::hex << std::setfill('0') << m_PresentSemaphore->GetNativeHandle();
  VKLOG(Info, stream.str().c_str());
}

bool RenderViewport::Present(bool vSync)
{
  VKRHI_METHOD_TRACE
    std::stringstream stream;
  stream << "present ----- renderSemaphore " << std::hex << std::setfill('0') << m_RenderSemaphore->GetNativeHandle();
  VKLOG(Info, stream.str().c_str());
  VkResult result = m_pSwapChain->Present(m_CurFrameId, m_RenderSemaphore);
  return result == VK_SUCCESS;
}

rhi::RenderTargetRef RenderViewport::GetRenderTarget(uint32 index)
{
  return m_RenderTargets[index];
}

rhi::RenderTargetRef RenderViewport::GetCurrentBackRenderTarget()
{
  return m_RenderTargets[m_CurFrameId];
}

uint32 RenderViewport::GetSwapChainIndex()
{
  return m_CurFrameId;
}

uint32 RenderViewport::GetSwapChainCount()
{
  return m_pSwapChain->GetBackBufferCount();
}

void RenderViewport::AllocateDefaultRenderPass(rhi::GfxSetting & gfxSetting)
{
  VKRHI_METHOD_TRACE
    VkFormat colorformat = m_pSwapChain ? m_pSwapChain->GetFormat() : g_FormatTable[gfxSetting.ColorFormat];
  RenderpassAttachment colorAttach = RenderpassAttachment::CreateColor(colorformat);
  colorAttach.GetDescription()
    .InitialLayout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
    .FinalLayout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
  RenderpassOptions option;
  option.AddAttachment(colorAttach);
  Subpass subpass_;
  subpass_.AddColorAttachment(0);
  if (gfxSetting.HasDepth)
  {
    VkFormat depthFormat = g_FormatTable[gfxSetting.DepthStencilFormat];
    GetGpuRef()->GetSupportedDepthFormat(&depthFormat);
    RenderpassAttachment depthAttach =
      RenderpassAttachment::CreateDepthStencil(depthFormat);
    depthAttach.GetDescription().InitialLayout(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
      .FinalLayout(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    subpass_.AddDepthStencilAttachment(1);
    option.AddAttachment(depthAttach);
  }
  option.AddSubPass(subpass_);
  m_RenderPass = MakeShared<RenderPass>(GetDevice(), option);
}

void RenderViewport::AllocateRenderTargets(rhi::GfxSetting & gfxSetting)
{
  if (m_RenderPass)
  {
    VkFormat depthFormat = g_FormatTable[gfxSetting.DepthStencilFormat];
    GetGpuRef()->GetSupportedDepthFormat(&depthFormat);
    VkImage depthImage;
    VkImageCreateInfo image = {};
    image.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image.imageType = VK_IMAGE_TYPE_2D;
    image.format = depthFormat;
    // Use example's height and width
    image.extent = { GetWidth(), GetHeight(), 1 };
    image.mipLevels = 1;
    image.arrayLayers = 1;
    image.samples = VK_SAMPLE_COUNT_1_BIT;
    image.tiling = VK_IMAGE_TILING_OPTIMAL;
    image.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    image.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    vkCreateImage(GetRawDevice(), &image, nullptr, &depthImage);


    VkDeviceMemory depthMem;
    // Allocate memory for the image (device local) and bind it to our image
    VkMemoryAllocateInfo memAlloc = {};
    memAlloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(GetRawDevice(), depthImage, &memReqs);
    memAlloc.allocationSize = memReqs.size;
    GetDevice()->FindMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &memAlloc.memoryTypeIndex);
    vkAllocateMemory(GetRawDevice(), &memAlloc, nullptr, &depthMem);
    vkBindImageMemory(GetRawDevice(), depthImage, depthMem, 0);

    auto layoutCmd = k3d::StaticPointerCast<CommandContext>(
      GetDevice()->NewCommandContext(rhi::ECMD_Graphics));
    ImageMemoryBarrierParams params(depthImage,
      VK_IMAGE_LAYOUT_UNDEFINED,
      VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
      VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
      VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
    layoutCmd->Begin();
    params.MipLevelCount(1).AspectMask(VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT).LayerCount(1);
    layoutCmd->PipelineBarrierImageMemory(params);
    layoutCmd->End();
    layoutCmd->Execute(false);

    VkImageView depthView;
    VkImageViewCreateInfo depthStencilView = {};
    depthStencilView.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    depthStencilView.viewType = VK_IMAGE_VIEW_TYPE_2D;
    depthStencilView.format = depthFormat;
    depthStencilView.subresourceRange = {};
    depthStencilView.subresourceRange.aspectMask = DetermineAspectMask(depthFormat);
    depthStencilView.subresourceRange.baseMipLevel = 0;
    depthStencilView.subresourceRange.levelCount = 1;
    depthStencilView.subresourceRange.baseArrayLayer = 0;
    depthStencilView.subresourceRange.layerCount = 1;
    depthStencilView.image = depthImage;
    vkCreateImageView(GetRawDevice(), &depthStencilView, nullptr, &depthView);

    m_RenderTargets.resize(m_NumBufferCount);
    RenderpassOptions option = m_RenderPass->GetOption();
    VkFormat colorFmt = option.GetAttachments()[0].GetFormat();
    for (uint32_t i = 0; i < m_NumBufferCount; ++i)
    {
      VkImage colorImage = m_pSwapChain->GetBackImage(i);
      auto colorImageInfo = ImageViewInfo::CreateColorImageView(GetGpuRef(), colorFmt, colorImage);
      VKLOG(Info, "swapchain imageView created . (0x%0x).", colorImageInfo.first);
      colorImageInfo.second.components = {
        VK_COMPONENT_SWIZZLE_R,
        VK_COMPONENT_SWIZZLE_G,
        VK_COMPONENT_SWIZZLE_B,
        VK_COMPONENT_SWIZZLE_A
      };
      auto colorTex = Texture::CreateFromSwapChain(colorImage, colorImageInfo.first, colorImageInfo.second, GetDevice());

      FrameBuffer::Attachment colorAttach(colorImageInfo.first);
      FrameBuffer::Attachment depthAttach(depthView);
      FrameBuffer::Option op;
      op.Width = GetWidth();
      op.Height = GetHeight();
      op.Attachments.push_back(colorAttach);
      op.Attachments.push_back(depthAttach);
      auto framebuffer = SpFramebuffer(new FrameBuffer(GetDevice(), m_RenderPass->GetPass(), op));
      m_RenderTargets[i] = k3d::MakeShared<RenderTarget>(GetDevice(), colorTex, framebuffer, m_RenderPass->GetPass());
    }
  }
}

VkRenderPass RenderViewport::GetRenderPass() const { return m_RenderPass->GetPass(); }

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