Param(
  [string]$Config = "Debug",
  [string]$OutDir = "artifacts/captures/d3d12",
  [string]$Resolution = "640x360",
  [switch]$Headless
)

$ErrorActionPreference = 'Stop'

function Ensure-Dir($p) { if (!(Test-Path $p)) { New-Item -ItemType Directory -Force -Path $p | Out-Null } }

# Build D3D12-only shell apps
cmake -S . -B build/vs -G "Visual Studio 17 2022" -A x64 `
  -DIGL_WITH_D3D12=ON -DIGL_WITH_SHELL=ON -DIGL_WITH_SAMPLES=ON `
  -DIGL_WITH_VULKAN=OFF -DIGL_WITH_OPENGL=OFF -DIGL_WITH_TESTS=OFF -DCMAKE_BUILD_TYPE=$Config
# Build only required shell session targets to avoid unrelated sample failures
$targets = @(
  'EmptySession_d3d12',
  'BasicFramebufferSession_d3d12',
  'HelloWorldSession_d3d12',
  'ThreeCubesRenderSession_d3d12',
  'Textured3DCubeSession_d3d12',
  'DrawInstancedSession_d3d12',
  'MRTSession_d3d12'
)
foreach ($t in $targets) {
  cmake --build build/vs --config $Config --target $t -j 8
}

$bin = Join-Path "build/vs/shell" $Config
$sessions = @(
  "EmptySession_d3d12.exe",
  "BasicFramebufferSession_d3d12.exe",
  "HelloWorldSession_d3d12.exe",
  "ThreeCubesRenderSession_d3d12.exe",
  "Textured3DCubeSession_d3d12.exe",
  "DrawInstancedSession_d3d12.exe",
  "MRTSession_d3d12.exe"
)

foreach ($s in $sessions) {
  $name = [System.IO.Path]::GetFileNameWithoutExtension($s).Replace('_d3d12','')
  $dst = Join-Path $OutDir (Join-Path $name $Resolution)
  Ensure-Dir $dst
  $png = Join-Path $dst ("{0}.png" -f $name)
  $meta = Join-Path $dst "meta.json"
  Write-Host "Capturing $name -> $png"
  $args = @('--screenshot-number','0','--screenshot-file', $png, '--viewport-size', $Resolution)
  if ($Headless) { $args = @('--headless') + $args }
  & (Join-Path $bin $s) @args | Out-Null
  $w,$h = $Resolution.Split('x')
  $metaObj = @{ backend = "D3D12"; width = [int]$w; height = [int]$h; format = "RGBA8_UNORM"; frame = 0 }
  ($metaObj | ConvertTo-Json) | Set-Content -Encoding ASCII $meta
}

Write-Host "D3D12 captures saved to $OutDir"
