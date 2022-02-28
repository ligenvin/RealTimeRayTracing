from falcor import *

def render_graph_RayTraceShadowTA():
    g = RenderGraph('RayTraceShadowTA')
    loadRenderPassLibrary('RayTrac.dll')
    loadRenderPassLibrary('DepthPass.dll')
    loadRenderPassLibrary('GBuffer.dll')
    loadRenderPassLibrary('MotionedShadow.dll')
    loadRenderPassLibrary('SpatioTemporalSM.dll')
    loadRenderPassLibrary('VPathTracer.dll')
    AccumulatePass = createPass('AccumulatePass', {'enabled': True, 'autoReset': True, 'precisionMode': AccumulatePrecision.Single, 'subFrameCount': 0, 'maxAccumulatedFrames': 0})
    g.addPass(AccumulatePass, 'AccumulatePass')
    GBufferRT = createPass('GBufferRT', {'samplePattern': SamplePattern.Center, 'sampleCount': 16, 'useAlphaTest': True, 'adjustShadingNormals': True, 'forceCullMode': False, 'cull': CullMode.CullBack, 'texLOD': TexLODMode.Mip0, 'useTraceRayInline': False})
    g.addPass(GBufferRT, 'GBufferRT')
    STSM_ReuseFactorEstimation = createPass('STSM_ReuseFactorEstimation')
    g.addPass(STSM_ReuseFactorEstimation, 'STSM_ReuseFactorEstimation')
    RayTrac = createPass('RayTrac', {'maxBounces': 1, 'computeDirect': True})
    g.addPass(RayTrac, 'RayTrac')
    STSM_BilateralFilter = createPass('STSM_BilateralFilter')
    g.addPass(STSM_BilateralFilter, 'STSM_BilateralFilter')
    STSM_TemporalReuse = createPass('STSM_TemporalReuse')
    g.addPass(STSM_TemporalReuse, 'STSM_TemporalReuse')
    DepthPass = createPass('DepthPass', {'depthFormat': ResourceFormat.D32Float})
    g.addPass(DepthPass, 'DepthPass')
    g.addEdge('GBufferRT.posW', 'RayTrac.posW')
    g.addEdge('GBufferRT.normW', 'RayTrac.normalW')
    g.addEdge('GBufferRT.tangentW', 'RayTrac.tangentW')
    g.addEdge('GBufferRT.faceNormalW', 'RayTrac.faceNormalW')
    g.addEdge('GBufferRT.viewW', 'RayTrac.viewW')
    g.addEdge('GBufferRT.emissive', 'RayTrac.mtlEmissive')
    g.addEdge('GBufferRT.matlExtra', 'RayTrac.mtlParams')
    g.addEdge('GBufferRT.specRough', 'RayTrac.mtlSpecRough')
    g.addEdge('GBufferRT.diffuseOpacity', 'RayTrac.mtlDiffOpacity')
    g.addEdge('GBufferRT.vbuffer', 'RayTrac.vbuffer')
    g.addEdge('GBufferRT.mvec', 'RayTrac.mvec')
    g.addEdge('DepthPass.depth', 'RayTrac.depth')
    g.addEdge('RayTrac.visibility', 'AccumulatePass.input')
    g.addEdge('AccumulatePass.output', 'STSM_TemporalReuse.Visibility')
    g.addEdge('GBufferRT.mvec', 'STSM_TemporalReuse.Motion Vector')
    g.addEdge('GBufferRT.posW', 'STSM_TemporalReuse.Position')
    g.addEdge('GBufferRT.normW', 'STSM_TemporalReuse.Normal')
    g.addEdge('AccumulatePass.output', 'STSM_ReuseFactorEstimation.Visibility_RF')
    g.addEdge('STSM_TemporalReuse.TR_Visibility', 'STSM_BilateralFilter.Color')
    g.addEdge('GBufferRT.normW', 'STSM_BilateralFilter.Normal')
    g.addEdge('DepthPass.depth', 'STSM_BilateralFilter.Depth')
    g.addEdge('STSM_BilateralFilter.Result', 'STSM_ReuseFactorEstimation.PrevVisibility_RF')
    g.addEdge('GBufferRT.mvec', 'STSM_ReuseFactorEstimation.MotionVector_RF')
    g.addEdge('GBufferRT.posW', 'STSM_ReuseFactorEstimation.Position_RF')
    g.addEdge('GBufferRT.normW', 'STSM_ReuseFactorEstimation.Normal_RF')
    g.addEdge('DepthPass.depth', 'STSM_ReuseFactorEstimation.Reliability_RF')
    g.markOutput('RayTrac.visibility')
    g.markOutput('STSM_BilateralFilter.Result')
    g.markOutput('STSM_ReuseFactorEstimation.Variation')
    g.markOutput('STSM_ReuseFactorEstimation.VarOfVar')
    g.markOutput('STSM_TemporalReuse.TR_Visibility')
    return g

RayTraceShadowTA = render_graph_RayTraceShadowTA()
try: m.addGraph(RayTraceShadowTA)
except NameError: None
