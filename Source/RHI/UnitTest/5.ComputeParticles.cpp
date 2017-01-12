#include <Kaleido3D.h>
#include <Base/UTRHIAppBase.h>
#include <Core/App.h>
#include <Core/AssetManager.h>
#include <Core/LogUtil.h>
#include <Core/Message.h>
#include <Math/kMath.hpp>

using namespace k3d;
using namespace kMath;

class UTComputeParticles : public RHIAppBase
{
public:
	UTComputeParticles(kString const & appName, uint32 width, uint32 height)
			: RHIAppBase(appName, width, height)
	{}
	explicit UTComputeParticles(kString const & appName)
		: RHIAppBase(appName, 1920, 1080)
	{}

	bool OnInit() override;
	void OnDestroy() override;
	void OnProcess(Message & msg) override;

	void OnUpdate() override;

protected:
	void PreparePipeline();

private:
	rhi::PipelineStateObjectRef			m_pGfxPso;
	rhi::PipelineLayoutRef				m_pGfxPl;
	rhi::PipelineStateObjectRef			m_pCompPso;
	rhi::PipelineLayoutRef				m_pCompPl;
};

K3D_APP_MAIN(UTComputeParticles);

bool UTComputeParticles::OnInit()
{
	bool inited = RHIAppBase::OnInit();
	if(!inited)
		return inited;
	PreparePipeline();
	return true;
}

void UTComputeParticles::PreparePipeline()
{
	rhi::ShaderBundle vertSh, fragSh, compSh;
	Compile("asset://Test/particles.vert", rhi::ES_Vertex, vertSh);
	Compile("asset://Test/particles.frag", rhi::ES_Fragment, fragSh);
	Compile("asset://Test/particles.comp", rhi::ES_Compute, compSh);
	auto vertBinding = vertSh.BindingTable;
	auto fragBinding = fragSh.BindingTable;
	auto mergedBindings = vertBinding | fragBinding;
	m_pGfxPl = m_pDevice->NewPipelineLayout(mergedBindings);
	m_pCompPl = m_pDevice->NewPipelineLayout(compSh.BindingTable);

	rhi::PipelineDesc gfxDesc;
	gfxDesc.Shaders[rhi::ES_Vertex] = vertSh;
	gfxDesc.Shaders[rhi::ES_Fragment] = fragSh;
	//gfxDesc.InputState = m_CubeMesh->GetInputState();
	rhi::PipelineDesc compDesc;
	compDesc.Shaders[rhi::ES_Compute] = compSh;
	m_pGfxPso = m_pDevice->NewPipelineState(gfxDesc, m_pGfxPl, rhi::EPSO_Graphics);
	m_pCompPso = m_pDevice->NewPipelineState(compDesc, m_pCompPl, rhi::EPSO_Compute);
}

void UTComputeParticles::OnUpdate()
{

}

void UTComputeParticles::OnProcess(Message & msg)
{

}

void UTComputeParticles::OnDestroy()
{

}