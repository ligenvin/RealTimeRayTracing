from falcor import *

def render_graph_forward_renderer():
    g = RenderGraph('forward_renderer')
    loadRenderPassLibrary('ErrorMeasurePass.dll')
    loadRenderPassLibrary('Utils.dll')
    loadRenderPassLibrary('CSM.dll')
    loadRenderPassLibrary('BSDFViewer.dll')
    loadRenderPassLibrary('MinimalPathTracer.dll')
    loadRenderPassLibrary('AccumulatePass.dll')
    loadRenderPassLibrary('RayTrac.dll')
    loadRenderPassLibrary('Antialiasing.dll')
    loadRenderPassLibrary('BlitPass.dll')
    loadRenderPassLibrary('DebugPasses.dll')
    loadRenderPassLibrary('SimplePostFX.dll')
    loadRenderPassLibrary('ForwardLightingPass.dll')
    loadRenderPassLibrary('DepthPass.dll')
    loadRenderPassLibrary('SpatioTemporalFilter.dll')
    loadRenderPassLibrary('SkyBox.dll')
    loadRenderPassLibrary('FLIPPass.dll')
    loadRenderPassLibrary('SVGFPass.dll')
    loadRenderPassLibrary('GBuffer.dll')
    loadRenderPassLibrary('ImageLoader.dll')
    loadRenderPassLibrary('MegakernelPathTracer.dll')
    loadRenderPassLibrary('MotionedShadow.dll')
    loadRenderPassLibrary('MergePass.dll')
    loadRenderPassLibrary('OptixDenoiser.dll')
    loadRenderPassLibrary('PassLibraryTemplate.dll')
    loadRenderPassLibrary('PixelInspectorPass.dll')
    loadRenderPassLibrary('SceneDebugger.dll')
    loadRenderPassLibrary('ShadowTaaPass.dll')
    loadRenderPassLibrary('SpatioTemporalSM.dll')
    loadRenderPassLibrary('SSAO.dll')
    loadRenderPassLibrary('TemporalDelayPass.dll')
    loadRenderPassLibrary('TestPasses.dll')
    loadRenderPassLibrary('ToneMapper.dll')
    loadRenderPassLibrary('VPathTracer.dll')
    loadRenderPassLibrary('WhittedRayTracer.dll')
    MS_Visibility = createPass('MS_Visibility')
    g.addPass(MS_Visibility, 'MS_Visibility')
    DepthPrePass = createPass('DepthPass', {'depthFormat': ResourceFormat.D32Float})
    g.addPass(DepthPrePass, 'DepthPrePass')
    LightingPass = createPass('ForwardLightingPass', {'sampleCount': 1, 'enableSuperSampling': False})
    g.addPass(LightingPass, 'LightingPass')
    SkyBox = createPass('SkyBox', {'texName': '', 'loadAsSrgb': True, 'filter': SamplerFilter.Linear})
    g.addPass(SkyBox, 'SkyBox')
    MS_Shadow = createPass('MS_Shadow')
    g.addPass(MS_Shadow, 'MS_Shadow')
    g.addEdge('DepthPrePass.depth', 'SkyBox.depth')
    g.addEdge('SkyBox.target', 'LightingPass.color')
    g.addEdge('DepthPrePass.depth', 'LightingPass.depth')
    g.addEdge('MS_Shadow.depth', 'MS_Visibility.SM')
    g.addEdge('MS_Shadow.Id', 'MS_Visibility.Id')
    g.addEdge('MS_Visibility.vis', 'LightingPass.visibilityBuffer')
    g.markOutput('LightingPass.color')
    g.markOutput('LightingPass.depth')
    return g

forward_renderer = render_graph_forward_renderer()
try: m.addGraph(forward_renderer)
except NameError: None