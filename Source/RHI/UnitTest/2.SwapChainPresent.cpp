#include <Kaleido3D.h>
#include <Core/App.h>
#include <Core/Message.h>
#include <Interface/IRHI.h>
#include <Renderer/Render.h>
#if K3DPLATFORM_OS_WIN
#include <RHI/Vulkan/VkCommon.h>
#include <RHI/Vulkan/Public/IVkRHI.h>
#elif K3DPLATFORM_OS_MAC||K3DPLATFORM_OS_IOS
#include <RHI/Metal/Common.h>
#include <RHI/Metal/Public/IMetalRHI.h>
#else
#include <RHI/Vulkan/VkCommon.h>
#include <RHI/Vulkan/Public/IVkRHI.h>
#endif

using namespace k3d;
using namespace render;

rhi::GfxSetting setting{ 1920, 1080, rhi::EPF_RGBA8Unorm, rhi::EPF_D32Float, true, 2 };

class VkSwapChainPresent : public App
{
public:
	explicit VkSwapChainPresent(kString const & appName)
		: App(appName, 1920, 1080)
	{}
	VkSwapChainPresent(kString const & appName, uint32 width, uint32 height)
		: App(appName, width, height)
	{}

	bool OnInit() override;
	void OnDestroy() override;
	void OnProcess(Message & msg) override;

private:
  SharedPtr<IModule>  m_pRHI;

	rhi::DeviceRef			m_pDevice;
	rhi::RenderViewportRef	m_RenderVp;
};

K3D_APP_MAIN(VkSwapChainPresent);

bool VkSwapChainPresent::OnInit()
{
	App::OnInit();
#if !(K3DPLATFORM_OS_MAC || K3DPLATFORM_OS_IOS)
  m_pRHI = ACQUIRE_PLUGIN(RHI_Vulkan);
  auto pVkRHI = k3d::StaticPointerCast<IVkRHI>(m_pRHI);
  pVkRHI->Initialize("SwapChainTest", true);
  pVkRHI->Start();
  m_pDevice = pVkRHI->GetPrimaryDevice();
#else 
  m_pRHI = ACQUIRE_PLUGIN(RHI_Metal);
  auto pMtlRHI = k3d::StaticPointerCast<IMetalRHI>(m_pRHI);
  if (pMtlRHI)
  {
    pMtlRHI->Start();
    m_pDevice = pMtlRHI->GetPrimaryDevice();
  }
#endif
	m_RenderVp = m_pDevice->NewRenderViewport(HostWindow()->GetHandle(), setting);
	m_RenderVp->InitViewport(nullptr, nullptr, setting);
	return true;
}

void VkSwapChainPresent::OnDestroy()
{
}

void VkSwapChainPresent::OnProcess(Message& msg)
{
	if (m_RenderVp)
	{
		m_RenderVp->PrepareNextFrame();
    auto Cmd = m_pDevice->NewCommandContext(rhi::ECMD_Graphics);
    Cmd->Begin();
    Cmd->TransitionResourceBarrier(m_RenderVp->GetCurrentBackRenderTarget()->GetBackBuffer(), rhi::ERS_Present);
    Cmd->End();
    Cmd->Execute(false);
    Cmd->PresentInViewport(m_RenderVp);
//		m_RenderVp->Present(false);
	}
}
