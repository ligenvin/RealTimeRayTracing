import os
import re
import shutil
import skimage
import matplotlib.pyplot as plt
import json

useRelease = True
gComparerExe = "../../Bin/x64/%s/ImageCompare.exe" % ("Release" if useRelease else "Debug")

def getRMSE(vFileName1, vFileName2, vOutHeatMapFileName = None):
    cmd = "start /wait /b %s \"%s\" \"%s\" -m mae" % (gComparerExe, vFileName1, vFileName2)
    if vOutHeatMapFileName:
        cmd += " -e \"%s\"" % vOutHeatMapFileName
    res = os.popen(cmd).read()
    return float(res)

def getSSIM(vFile1, vFile2):
    img1 = skimage.io.imread(vFile1)
    img2 = skimage.io.imread(vFile2)
    ssim = skimage.metrics.structural_similarity(img1[:, :, 0], img2[:, :, 0])
    return ssim

def extractFrameId(vFileName):
    Match = re.search(r".*\-(\d+)\..*(exr|png)", vFileName)
    if (Match == None):
        return None
    FrameId = int(Match.group(1))
    return FrameId

def findSameFrame(vDir, vFrameId):
    FileNames = os.listdir(vDir)
    for FileName in FileNames:
        if (os.path.isdir(FileName)):
            continue
        Match = re.search(r".*\-0*" + str(vFrameId) + r"\..*(exr|png)", FileName)
        if Match:
            return FileName
    return None

def calConvergence(vBaseDir, vDirGT, vDirTarget):
    CompareResult = []
    GTImages = os.listdir(vBaseDir + vDirGT)

    Num = len(GTImages)
    TargetResults = {}
    for Target in vDirTarget:
        TargetName = Target['Name']
        TargetResults[TargetName] = {}
        TargetResults[TargetName]['RMSE'] = []
        TargetResults[TargetName]['SSIM'] = []

    print("Calculating measure...")
    for i, GTImage in enumerate(GTImages):
        FrameId = extractFrameId(GTImage)
        if FrameId == None:
            print("Can parse file name ", GTImage)
            continue
        
        print("Current frame id:", FrameId)

        for Target in vDirTarget:
            TargetName = Target['Name']
            TargetImage = findSameFrame(vBaseDir + Target['Dir'], FrameId)
            if not TargetImage:
                print("No match found for file ", GTImage)
                continue
            RMSE = getRMSE(vBaseDir + vDirGT + GTImage, vBaseDir + Target['Dir'] + TargetImage)
            SSIM = getSSIM(vBaseDir + vDirGT + GTImage, vBaseDir + Target['Dir'] + TargetImage)
            print("  ", GTImage, " - ", TargetImage)
            print("  RMSE: ", RMSE, " SSIM", SSIM)

            TargetResults[TargetName]['RMSE'].append(RMSE)
            TargetResults[TargetName]['SSIM'].append(SSIM)
    return [TargetResults, Num]

def calFlicking(vBaseDir, vDirGT, vDirTarget):
    CompareResult = []
    GTImages = os.listdir(vBaseDir + vDirGT)

    Num = len(GTImages)
    TargetResults = {}
    for Target in vDirTarget:
        TargetName = Target['Name']
        TargetResults[TargetName] = {}
        TargetResults[TargetName]['RMSE'] = []
        TargetResults[TargetName]['SSIM'] = []

    print("Calculating measure...")
    for i in range(Num - 1):
        FrameId = extractFrameId(GTImages[i])
        if FrameId == None:
            print("Can parse file name ", GTImage)
            continue
        
        print("Current frame id:", FrameId)

        for Target in vDirTarget:
            TargetName = Target['Name']
            TargetImageCur = findSameFrame(vBaseDir + Target['Dir'], FrameId)
            TargetImageNext = findSameFrame(vBaseDir + Target['Dir'], FrameId + 1)
            if not TargetImageCur or not TargetImageNext:
                print("No match found for file ", GTImage)
                continue
            RMSE = getRMSE(vBaseDir + Target['Dir'] + TargetImageCur, vBaseDir + Target['Dir'] + TargetImageNext)
            SSIM = getSSIM(vBaseDir + Target['Dir'] + TargetImageCur, vBaseDir + Target['Dir'] + TargetImageNext)
            print("  ", TargetImageCur, " - ", TargetImageNext)
            print("  RMSE: ", RMSE, " SSIM", SSIM)

            TargetResults[TargetName]['RMSE'].append(RMSE)
            TargetResults[TargetName]['SSIM'].append(SSIM)
    return [TargetResults, Num - 1]

def plot(vResult, vNum, vTitle):
    fig = plt.figure()
    plt.title(vTitle, fontsize = 24)
    plt.xlabel("Frame", fontsize = 14)
    plt.ylabel("Measure", fontsize = 14)

    xData = range(1, vNum + 1)
    Colors = ['#1f77b4', '#ff7f0e']
    for i, Target in enumerate(vResult):
        Color = Colors[i]
        for Type, Style in [['RMSE', "-"], ['SSIM', '--']]:
            yData = vResult[Target][Type]
            plt.plot(xData, yData, label=Target + "-" + Type, color=Color, linestyle=Style)
    plt.legend(loc = 'upper center')
    plt.margins(x = 0)
    plt.show()

def writeJson(vFileName, vData):
    File = open(vFileName, "w")
    File.write(json.dumps(vData))
    File.close()

def readJson(vFileName):
    File = open(vFileName, "r")
    return json.loads(File.read())

BaseDir = "D:/Out/Convergence/"
DirGT = "GroundTruth/AccumulatePass-output/"
DirTarget = [
    {
        'Name': 'Random',
        'Dir': "Random/STSM_BilateralFilter-Result/"
    },
    {
        'Name': 'Tranditional_16',
        'Dir': "Tranditional_16/STSM_BilateralFilter-Result/"
    },
]

def getOutputFile(Scene):
    return BaseDir + "plotData_%s_%s.json" % (gCalTypes[gCalTypeIdx], Scene)

gCalTypes = ["Convergence", "Flicking"]
gCalTypeIdx = 1
gReadFromFile = True
# for Scene in ['GridObserve', 'DragonObserve', 'ArcadeObserve']:
for Scene in ['DynamicGridObserve', 'DynamicDragonObserve', 'DynamicArcadeObserve']: # dynamic
    print("Run plot for ", Scene)
    SubDir = Scene + "/"
    OutputFile = getOutputFile(Scene)
    if gReadFromFile:
        [Result, Num] = readJson(OutputFile)
    else:
        if (gCalTypeIdx == 0):
            [Result, Num] = calConvergence(BaseDir + SubDir, DirGT, DirTarget)
        else:
            [Result, Num] = calFlicking(BaseDir + SubDir, DirGT, DirTarget)
        writeJson(OutputFile, [Result, Num])
    print("Ploting...")
    plot(Result, Num, Scene + "-" + gCalTypes[gCalTypeIdx])