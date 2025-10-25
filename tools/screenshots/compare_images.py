#!/usr/bin/env python3
import sys, os, json

try:
    from PIL import Image
except Exception as e:
    Image = None

def load_png_rgba8(path):
    if Image is None:
        raise RuntimeError('Pillow not available to decode PNGs; install with: pip install pillow')
    img = Image.open(path).convert('RGBA')
    return img.size[0], img.size[1], img.tobytes()

def load_raw_rgba8(path, width, height):
    with open(path, 'rb') as f:
        data = f.read()
    expected = width*height*4
    if len(data) != expected:
        raise RuntimeError(f'Raw size mismatch: got {len(data)} expected {expected}')
    return data

def compare_bytes(a, b):
    if len(a) != len(b):
        return False, f'Length mismatch: {len(a)} != {len(b)}'
    for i,(x,y) in enumerate(zip(a,b)):
        if x != y:
            return False, f'Byte mismatch at {i}: {x} != {y}'
    return True, 'OK'

def main():
    if len(sys.argv) < 3:
        print('Usage: compare_images.py <vulkan_dir> <d3d12_dir>')
        sys.exit(2)
    vk_root = sys.argv[1]
    dx_root = sys.argv[2]
    sessions = [d for d in os.listdir(vk_root) if os.path.isdir(os.path.join(vk_root,d))]
    failures = []
    for s in sessions:
        for res in os.listdir(os.path.join(vk_root,s)):
            vk_dir = os.path.join(vk_root, s, res)
            dx_dir = os.path.join(dx_root, s, res)
            if not os.path.isdir(dx_dir):
                print(f'[MISS] {s}/{res}: D3D12 capture missing')
                failures.append((s,res,'missing d3d12'))
                continue
            name = s
            vk_png = os.path.join(vk_dir, f'{name}.png')
            dx_png = os.path.join(dx_dir, f'{name}.png')
            if os.path.exists(vk_png) and os.path.exists(dx_png):
                w0,h0,a = load_png_rgba8(vk_png)
                w1,h1,b = load_png_rgba8(dx_png)
                if (w0,h0)!=(w1,h1):
                    print(f'[FAIL] {s}/{res}: size mismatch {w0}x{h0} vs {w1}x{h1}')
                    failures.append((s,res,'size mismatch'))
                else:
                    ok,msg = compare_bytes(a,b)
                    if ok:
                        print(f'[ OK ] {s}/{res}: byte-identical')
                    else:
                        print(f'[FAIL] {s}/{res}: {msg}')
                        failures.append((s,res,msg))
            else:
                print(f'[MISS] {s}/{res}: missing PNG(s) to compare')
                failures.append((s,res,'missing png'))
    if failures:
        print(f'Parity failures: {len(failures)}')
        sys.exit(1)
    print('All images are byte-identical.')

if __name__ == '__main__':
    main()

