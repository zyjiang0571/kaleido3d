#include "rendererwidget.h"

using namespace k3d;
using namespace rhi;

RendererWidget::RendererWidget(QWidget *parent)
    : QWidget(parent)
{
}

void RendererWidget::init()
{
  RHI = StaticPointerCast<IVkRHI>(ACQUIRE_PLUGIN(RHI_Vulkan));
  RHI->Initialize("Widget", false);
  RHI->Start();
  Device = RHI->GetPrimaryDevice();
  GfxSetting setting(width(), height(), rhi::EPF_RGB8Unorm, rhi::EPF_D32Float, false, 2);
  RenderVp = Device->NewRenderViewport((void*)winId(), setting);
}
