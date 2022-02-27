from falcor import *

def render_graph_STSMRenderGraph():
    g = RenderGraph('STSMRenderGraph')
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
    STSM_MultiViewShadowMapViewWarp = createPass('STSM_MultiViewShadowMapViewWarp')
    g.addPass(STSM_MultiViewShadowMapViewWarp, 'STSM_MultiViewShadowMapViewWarp')
    STSM_TemporalReuse = createPass('STSM_TemporalReuse')
    g.addPass(STSM_TemporalReuse, 'STSM_TemporalReuse')
    STSM_BilateralFilter = createPass('STSM_BilateralFilter')
    g.addPass(STSM_BilateralFilter, 'STSM_BilateralFilter')
    GBufferRaster = createPass('GBufferRaster', {'samplePattern': SamplePattern.Center, 'sampleCount': 16, 'useAlphaTest': True, 'adjustShadingNormals': True, 'forceCullMode': False, 'cull': CullMode.CullBack})
    g.addPass(GBufferRaster, 'GBufferRaster')
    STSM_CalculateVisibility = createPass('STSM_CalculateVisibility')
    g.addPass(STSM_CalculateVisibility, 'STSM_CalculateVisibility')
    STSM_ShadowMapSelector = createPass('STSM_ShadowMapSelector')
    g.addPass(STSM_ShadowMapSelector, 'STSM_ShadowMapSelector')
    STSM_MultiViewShadowMapRasterize = createPass('STSM_MultiViewShadowMapRasterize')
    g.addPass(STSM_MultiViewShadowMapRasterize, 'STSM_MultiViewShadowMapRasterize')
    STSM_ReuseFactorEstimation = createPass('STSM_ReuseFactorEstimation')
    g.addPass(STSM_ReuseFactorEstimation, 'STSM_ReuseFactorEstimation')
    MS_Visibility = createPass('MS_Visibility')
    g.addPass(MS_Visibility, 'MS_Visibility')
    g.addEdge('GBufferRaster.posW', 'STSM_TemporalReuse.Position')
    g.addEdge('GBufferRaster.normW', 'STSM_TemporalReuse.Normal')
    g.addEdge('GBufferRaster.depth', 'STSM_CalculateVisibility.Depth')
    g.addEdge('STSM_ShadowMapSelector.ShadowMap', 'STSM_CalculateVisibility.ShadowMap')
    g.addEdge('GBufferRaster.normW', 'STSM_BilateralFilter.Normal')
    g.addEdge('GBufferRaster.depth', 'STSM_BilateralFilter.Depth')
    g.addEdge('STSM_MultiViewShadowMapViewWarp.ShadowMapSet', 'STSM_ShadowMapSelector.ViewWarp')
    g.addEdge('STSM_MultiViewShadowMapRasterize.ShadowMapSet', 'STSM_ShadowMapSelector.Rasterize')
    g.addEdge('STSM_MultiViewShadowMapRasterize.IdSet', 'MS_Visibility.Ids')
    g.addEdge('STSM_ShadowMapSelector.ShadowMap', 'MS_Visibility.SMs')
    g.addEdge('STSM_CalculateVisibility.LightUv', 'MS_Visibility.LOffs')
    g.addEdge('STSM_CalculateVisibility.Visibility', 'STSM_TemporalReuse.Visibility')
    g.addEdge('STSM_TemporalReuse.TR_Visibility', 'STSM_BilateralFilter.Color')
    g.addEdge('GBufferRaster.posW', 'MS_Visibility.PosW')
    g.addEdge('GBufferRaster.posW', 'STSM_ReuseFactorEstimation.Position_RF')
    g.addEdge('GBufferRaster.normW', 'STSM_ReuseFactorEstimation.Normal_RF')
    g.addEdge('MS_Visibility.smvr', 'STSM_ReuseFactorEstimation.Reliability_RF')
    g.addEdge('STSM_CalculateVisibility.Visibility', 'STSM_ReuseFactorEstimation.Visibility_RF')
    g.addEdge('STSM_BilateralFilter.Result', 'STSM_ReuseFactorEstimation.PrevVisibility_RF')
    g.addEdge('GBufferRaster.mvec', 'STSM_TemporalReuse.Motion Vector')
    g.addEdge('GBufferRaster.mvec', 'STSM_ReuseFactorEstimation.MotionVector_RF')
    g.markOutput('STSM_TemporalReuse.TR_Visibility')
    g.markOutput('STSM_TemporalReuse.Debug')
    g.markOutput('STSM_ReuseFactorEstimation.Variation')
    g.markOutput('STSM_ReuseFactorEstimation.VarOfVar')
    g.markOutput('STSM_BilateralFilter.Result')
    g.markOutput('STSM_BilateralFilter.Debug')
    return g

STSMRenderGraph = render_graph_STSMRenderGraph()
try: m.addGraph(STSMRenderGraph)
except NameError: None
