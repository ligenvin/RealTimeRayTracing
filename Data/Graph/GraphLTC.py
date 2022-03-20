from falcor import *

def render_graph_g():
    g = RenderGraph('g')
    loadRenderPassLibrary('DrawLightQuad.dll')
    loadRenderPassLibrary('BSDFViewer.dll')
    loadRenderPassLibrary('AccumulatePass.dll')
    loadRenderPassLibrary('Antialiasing.dll')
    loadRenderPassLibrary('DebugPasses.dll')
    loadRenderPassLibrary('ATrousFilter.dll')
    loadRenderPassLibrary('VPathTracer.dll')
    loadRenderPassLibrary('SimplePostFX.dll')
    loadRenderPassLibrary('BlitPass.dll')
    loadRenderPassLibrary('CSM.dll')
    loadRenderPassLibrary('DepthPass.dll')
    loadRenderPassLibrary('ErrorMeasurePass.dll')
    loadRenderPassLibrary('FLIPPass.dll')
    loadRenderPassLibrary('ForwardLightingPass.dll')
    loadRenderPassLibrary('GBuffer.dll')
    loadRenderPassLibrary('ImageLoader.dll')
    loadRenderPassLibrary('LTCLight.dll')
    loadRenderPassLibrary('SVGFPass.dll')
    loadRenderPassLibrary('MegakernelPathTracer.dll')
    loadRenderPassLibrary('MergeChannels.dll')
    loadRenderPassLibrary('MergePass.dll')
    loadRenderPassLibrary('MinimalPathTracer.dll')
    loadRenderPassLibrary('MotionedShadow.dll')
    loadRenderPassLibrary('OptixDenoiser.dll')
    loadRenderPassLibrary('PixelInspectorPass.dll')
    loadRenderPassLibrary('PassLibraryTemplate.dll')
    loadRenderPassLibrary('TestPasses.dll')
    loadRenderPassLibrary('SceneDebugger.dll')
    loadRenderPassLibrary('ShadowTaaPass.dll')
    loadRenderPassLibrary('SkyBox.dll')
    loadRenderPassLibrary('SpatioTemporalSM.dll')
    loadRenderPassLibrary('SSAO.dll')
    loadRenderPassLibrary('TemporalDelayPass.dll')
    loadRenderPassLibrary('ToneMapper.dll')
    loadRenderPassLibrary('Utils.dll')
    loadRenderPassLibrary('WhittedRayTracer.dll')
    LTCLight = createPass('LTCLight')
    g.addPass(LTCLight, 'LTCLight')
    GBufferRaster = createPass('GBufferRaster', {'samplePattern': SamplePattern.Center, 'sampleCount': 16, 'useAlphaTest': True, 'adjustShadingNormals': True, 'forceCullMode': False, 'cull': CullMode.CullBack})
    g.addPass(GBufferRaster, 'GBufferRaster')
    SkyBox = createPass('SkyBox', {'texName': '', 'loadAsSrgb': True, 'filter': SamplerFilter.Linear})
    g.addPass(SkyBox, 'SkyBox')
    g.addEdge('GBufferRaster.depth', 'LTCLight.Depth')
    g.addEdge('GBufferRaster.posW', 'LTCLight.PosW')
    g.addEdge('GBufferRaster.normW', 'LTCLight.NormalW')
    g.addEdge('GBufferRaster.tangentW', 'LTCLight.Tangent')
    g.addEdge('GBufferRaster.diffuseOpacity', 'LTCLight.DiffuseOpacity')
    g.addEdge('GBufferRaster.specRough', 'LTCLight.SpecRough')
    g.addEdge('GBufferRaster.roughness', 'LTCLight.Roughness')
    g.addEdge('GBufferRaster.depth', 'SkyBox.depth')
    g.addEdge('SkyBox.target', 'LTCLight.SkyBox')
    g.markOutput('LTCLight.Color')
    return g

g = render_graph_g()
try: m.addGraph(g)
except NameError: None
