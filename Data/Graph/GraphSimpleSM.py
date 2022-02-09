from falcor import *

def render_graph_DefaultRenderGraph():
    g = RenderGraph('DefaultRenderGraph')
    loadRenderPassLibrary('SceneDebugger.dll')
    loadRenderPassLibrary('BSDFViewer.dll')
    loadRenderPassLibrary('AccumulatePass.dll')
    loadRenderPassLibrary('SpatioTemporalSM.dll')
    loadRenderPassLibrary('DepthPass.dll')
    loadRenderPassLibrary('VPathTracer.dll')
    loadRenderPassLibrary('Antialiasing.dll')
    loadRenderPassLibrary('BlitPass.dll')
    loadRenderPassLibrary('DebugPasses.dll')
    loadRenderPassLibrary('CSM.dll')
    loadRenderPassLibrary('ErrorMeasurePass.dll')
    loadRenderPassLibrary('SkyBox.dll')
    loadRenderPassLibrary('SimplePostFX.dll')
    loadRenderPassLibrary('FLIPPass.dll')
    loadRenderPassLibrary('PixelInspectorPass.dll')
    loadRenderPassLibrary('ForwardLightingPass.dll')
    loadRenderPassLibrary('GBuffer.dll')
    loadRenderPassLibrary('RayTrac.dll')
    loadRenderPassLibrary('ImageLoader.dll')
    loadRenderPassLibrary('MegakernelPathTracer.dll')
    loadRenderPassLibrary('ToneMapper.dll')
    loadRenderPassLibrary('MergePass.dll')
    loadRenderPassLibrary('MinimalPathTracer.dll')
    loadRenderPassLibrary('MotionedShadow.dll')
    loadRenderPassLibrary('OptixDenoiser.dll')
    loadRenderPassLibrary('SpatioTemporalFilter.dll')
    loadRenderPassLibrary('PassLibraryTemplate.dll')
    loadRenderPassLibrary('ShadowTaaPass.dll')
    loadRenderPassLibrary('SSAO.dll')
    loadRenderPassLibrary('SVGFPass.dll')
    loadRenderPassLibrary('TemporalDelayPass.dll')
    loadRenderPassLibrary('TestPasses.dll')
    loadRenderPassLibrary('Utils.dll')
    loadRenderPassLibrary('WhittedRayTracer.dll')
    STSM_MultiViewShadowMapRasterize = createPass('STSM_MultiViewShadowMapRasterize')
    g.addPass(STSM_MultiViewShadowMapRasterize, 'STSM_MultiViewShadowMapRasterize')
    STSM_CalculateVisibility = createPass('STSM_CalculateVisibility')
    g.addPass(STSM_CalculateVisibility, 'STSM_CalculateVisibility')
    GBufferRaster = createPass('GBufferRaster', {'samplePattern': SamplePattern.Center, 'sampleCount': 16, 'useAlphaTest': True, 'adjustShadingNormals': True, 'forceCullMode': False, 'cull': CullMode.CullBack})
    g.addPass(GBufferRaster, 'GBufferRaster')
    g.addEdge('GBufferRaster.depth', 'STSM_CalculateVisibility.Depth')
    g.addEdge('STSM_MultiViewShadowMapRasterize.ShadowMapSet', 'STSM_CalculateVisibility.ShadowMap')
    g.markOutput('STSM_CalculateVisibility.Visibility')
    return g

DefaultRenderGraph = render_graph_DefaultRenderGraph()
try: m.addGraph(DefaultRenderGraph)
except NameError: None
