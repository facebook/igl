Param(
  [string]$Config = "Release",
  [string]$OutDir = "artifacts/baselines/vulkan",
  [string]$Resolution = "640x360"
)

$ErrorActionPreference = 'Stop'

function Ensure-Dir($p) { if (!(Test-Path $p)) { New-Item -ItemType Directory -Force -Path $p | Out-Null } }

# Build Vulkan-only shell apps
cmake -S . -B build/vs_vulkan -G "Visual Studio 17 2022" -A x64 `
  -DIGL_WITH_VULKAN=ON -DIGL_WITH_SHELL=ON -DIGL_WITH_SAMPLES=ON `
  -DIGL_WITH_OPENGL=OFF -DIGL_WITH_D3D12=OFF -DIGL_WITH_TESTS=OFF -DCMAKE_BUILD_TYPE=$Config
# Build only required shell session targets to avoid unrelated sample failures
$targets = @(
  'EmptySession_vulkan',
  'BasicFramebufferSession_vulkan',
  'HelloWorldSession_vulkan',
  'ThreeCubesRenderSession_vulkan',
  'Textured3DCubeSession_vulkan',
  'DrawInstancedSession_vulkan',
  'MRTSession_vulkan'
)
foreach ($t in $targets) {
  cmake --build build/vs_vulkan --config $Config --target $t -j 8
}

$bin = Join-Path "build/vs_vulkan/shell" $Config
$sessions = @(
  "EmptySession_vulkan.exe",
  "BasicFramebufferSession_vulkan.exe",
  "HelloWorldSession_vulkan.exe",
  "ThreeCubesRenderSession_vulkan.exe",
  "Textured3DCubeSession_vulkan.exe",
  "DrawInstancedSession_vulkan.exe",
  "MRTSession_vulkan.exe"
)

foreach ($s in $sessions) {
  $name = [System.IO.Path]::GetFileNameWithoutExtension($s).Replace('_vulkan','')
  $dst = Join-Path $OutDir (Join-Path $name $Resolution)
  Ensure-Dir $dst
  $png = Join-Path $dst ("{0}.png" -f $name)
  $meta = Join-Path $dst "meta.json"
  Write-Host "Capturing $name -> $png"
  & (Join-Path $bin $s) --disable-vulkan-validation-layers `
    --screenshot-number 0 --screenshot-file $png --viewport-size $Resolution | Out-Null
  $w,$h = $Resolution.Split('x')
  $metaObj = @{ backend = "Vulkan"; width = [int]$w; height = [int]$h; format = "RGBA8_UNORM"; frame = 0 }
  ($metaObj | ConvertTo-Json) | Set-Content -Encoding ASCII $meta
}

Write-Host "Vulkan baselines captured to $OutDir"
