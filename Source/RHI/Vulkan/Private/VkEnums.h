#ifndef __VkEnums_h__
#define __VkEnums_h__
#pragma once

K3D_VK_BEGIN

namespace
{
	VkShaderStageFlagBits g_ShaderType[rhi::EShaderType::ShaderTypeNum] =
	{
		VK_SHADER_STAGE_FRAGMENT_BIT,
		VK_SHADER_STAGE_VERTEX_BIT,
		VK_SHADER_STAGE_GEOMETRY_BIT,
		VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
		VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
		VK_SHADER_STAGE_COMPUTE_BIT
	};

	VkPrimitiveTopology g_PrimitiveTopology[rhi::EPrimitiveType::PrimTypeNum] =
	{
		VK_PRIMITIVE_TOPOLOGY_POINT_LIST,
		VK_PRIMITIVE_TOPOLOGY_LINE_LIST,
		VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
		VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
	};

	VkCullModeFlagBits g_CullMode[rhi::RasterizerState::CullModeNum] = {
		VK_CULL_MODE_NONE,
		VK_CULL_MODE_FRONT_BIT,
		VK_CULL_MODE_BACK_BIT
	};

	VkPolygonMode g_FillMode[rhi::RasterizerState::EFillMode::FillModeNum] = {
		VK_POLYGON_MODE_LINE,
		VK_POLYGON_MODE_FILL
	};

	VkStencilOp g_StencilOp[rhi::DepthStencilState::StencilOpNum] = {
		VK_STENCIL_OP_KEEP,
		VK_STENCIL_OP_ZERO,
		VK_STENCIL_OP_REPLACE,
		VK_STENCIL_OP_INVERT,
		VK_STENCIL_OP_INCREMENT_AND_CLAMP,
		VK_STENCIL_OP_DECREMENT_AND_CLAMP
	};

	VkBlendOp g_BlendOps[rhi::BlendState::BlendOpNum] = {
		VK_BLEND_OP_ADD,
		VK_BLEND_OP_SUBTRACT,
	};

	VkBlendFactor g_Blend[rhi::BlendState::BlendTypeNum] = {
		VK_BLEND_FACTOR_ZERO,
		VK_BLEND_FACTOR_ONE,
		VK_BLEND_FACTOR_SRC_COLOR,
		VK_BLEND_FACTOR_DST_COLOR,
		VK_BLEND_FACTOR_SRC_ALPHA,
		VK_BLEND_FACTOR_DST_ALPHA
	};

	VkCompareOp g_ComparisonFunc[rhi::DepthStencilState::ComparisonFuncNum] = {
		VK_COMPARE_OP_NEVER,
		VK_COMPARE_OP_LESS,
		VK_COMPARE_OP_EQUAL,
		VK_COMPARE_OP_LESS_OR_EQUAL,
		VK_COMPARE_OP_GREATER,
		VK_COMPARE_OP_NOT_EQUAL,
		VK_COMPARE_OP_GREATER_OR_EQUAL,
		VK_COMPARE_OP_ALWAYS
	};

	VkFormat g_FormatTable[rhi::EPixelFormat::PixelFormatNum] = {
		VK_FORMAT_R16G16B16A16_UINT,
		VK_FORMAT_R32G32B32A32_SFLOAT,
		VK_FORMAT_R8G8B8A8_UNORM,
		VK_FORMAT_R8G8B8A8_SNORM,
		VK_FORMAT_B10G11R11_UFLOAT_PACK32,
		VK_FORMAT_D32_SFLOAT,
		VK_FORMAT_R32G32B32_SFLOAT,
		VK_FORMAT_R8G8B8_UNORM
	};

	VkFormat g_VertexFormatTable[] = {
		VK_FORMAT_R32_SFLOAT,
		VK_FORMAT_R32G32_SFLOAT,
		VK_FORMAT_R32G32B32_SFLOAT,
		VK_FORMAT_R32G32B32A32_SFLOAT
	};

	VkImageLayout g_ResourceState[] = { 
		VK_IMAGE_LAYOUT_GENERAL,
		VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		VK_IMAGE_LAYOUT_UNDEFINED
	};

//			VK_BUFFER_USAGE_TRANSFER_SRC_BIT = 0x00000001,
//			VK_BUFFER_USAGE_TRANSFER_DST_BIT = 0x00000002,
//			VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT = 0x00000004,
//			VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT = 0x00000008,
//			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT = 0x00000010,
//			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT = 0x00000020,
//			VK_BUFFER_USAGE_INDEX_BUFFER_BIT = 0x00000040,
//			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT = 0x00000080,
//			VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT = 0x00000100,
	/**
	 * EGVT_VBV, // For VertexBuffer
	 * EGVT_IBV, // For IndexBuffer
	 * EGVT_CBV, // For ConstantBuffer
	 * EGVT_SRV, // For Texture
	 * EGVT_UAV, // For Buffer
	 * EGVT_RTV,
	 * EGVT_DSV,
	 * EGVT_Sampler,
	 * EGVT_SOV,
	 */
	VkFlags	g_ResourceViewFlag[] = {
		0,
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT,
		VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT,
		0,
		0,
		0,
		0
	};

	//@see rhi::SamplerState::EFilterMethod
	VkFilter g_Filters[] = {
		VK_FILTER_NEAREST,
		VK_FILTER_LINEAR
	};

	VkSamplerMipmapMode g_MipMapModes[] = {
		VK_SAMPLER_MIPMAP_MODE_NEAREST,
		VK_SAMPLER_MIPMAP_MODE_LINEAR,
	};

	// @see rhi::SamplerState::ETextureAddressMode
	//	Wrap,
	//	Mirror, // Repeat
	//	Clamp,
	//	Border,
	//	MirrorOnce,
	VkSamplerAddressMode g_AddressModes[] = {
		VK_SAMPLER_ADDRESS_MODE_REPEAT,
		VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,
		VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
		VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
		VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE
	};
}

K3D_VK_END

#endif