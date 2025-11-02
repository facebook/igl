#include <d3d12.h>
#include <dxgiformat.h>
#include <stdio.h>

int main() {
  printf("DXGI_FORMAT_B8G8R8A8_UNORM = %d\n", DXGI_FORMAT_B8G8R8A8_UNORM);
  printf("DXGI_FORMAT_R8G8B8A8_UNORM = %d\n", DXGI_FORMAT_R8G8B8A8_UNORM);
  printf("DXGI_FORMAT_R8G8B8A8_UNORM_SRGB = %d\n", DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);
  printf("DXGI_FORMAT_B8G8R8A8_UNORM_SRGB = %d\n", DXGI_FORMAT_B8G8R8A8_UNORM_SRGB);
  return 0;
}
