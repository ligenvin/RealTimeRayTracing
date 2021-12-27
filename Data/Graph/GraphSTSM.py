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
    STSM_MultiViewShadowMap = createPass('STSM_MultiViewShadowMap')
    g.addPass(STSM_MultiViewShadowMap, 'STSM_MultiViewShadowMap')
    STSM_BilateralFilter = createPass('STSM_BilateralFilter')
    g.addPass(STSM_BilateralFilter, 'STSM_BilateralFilter')
    STSM_TemporalReuse = createPass('STSM_TemporalReuse')
    g.addPass(STSM_TemporalReuse, 'STSM_TemporalReuse')
    GBufferRaster = createPass('GBufferRaster', {'samplePattern': SamplePattern.Center, 'sampleCount': 16, 'useAlphaTest': True, 'adjustShadingNormals': True, 'forceCullMode': False, 'cull': CullMode.CullBack})
    g.addPass(GBufferRaster, 'GBufferRaster')
    g.addEdge('GBufferRaster.depth', 'STSM_MultiViewShadowMap.Depth')
    g.addEdge('GBufferRaster.mvec', 'STSM_TemporalReuse.Motion Vector')
    g.addEdge('GBufferRaster.posW', 'STSM_TemporalReuse.Position')
    g.addEdge('GBufferRaster.normW', 'STSM_TemporalReuse.Normal')
    g.addEdge('STSM_MultiViewShadowMap.Visibility', 'STSM_BilateralFilter.Input')
    g.addEdge('STSM_BilateralFilter.Result', 'STSM_TemporalReuse.Visibility')
    g.markOutput('STSM_TemporalReuse.Visibility')
    g.markOutput('STSM_TemporalReuse.Debug')
    return g

DefaultRenderGraph = render_graph_DefaultRenderGraph()
try: m.addGraph(DefaultRenderGraph)
except NameError: None
