# -*- coding: utf-8 -*-
"""
DataMG 应用图标资源生成脚本
运行: python tools/generate_assets.py
输出: resources/images/logo.ico 及多尺寸 PNG
"""
import os
import numpy as np
from PIL import Image, ImageDraw

OUT_DIR = os.path.join(os.path.dirname(__file__), "..", "resources", "images")
os.makedirs(OUT_DIR, exist_ok=True)

S = 1024
C1 = np.array([74, 173, 255], dtype=np.float32)   # 浅 QQ 蓝
C2 = np.array([19, 119, 235], dtype=np.float32)    # 深 QQ 蓝
RADIUS = 228


def make_master(size=S):
    # 对角渐变
    y, x = np.mgrid[0:size, 0:size].astype(np.float32)
    t = (x + y) / (2.0 * size)
    t = 0.18 + 0.78 * t
    grad = C1 * (1.0 - t[..., None]) + C2 * t[..., None]
    # 顶部柔光
    glow = np.exp(-(((x - size * 0.22) ** 2 + (y - size * 0.12) ** 2)
                    / (2 * (size * 0.42) ** 2))) * 26.0
    grad += glow[..., None]
    # 底部压暗
    shade = np.clip((y / size - 0.55) / 0.45, 0, 1) ** 2 * 18.0
    grad -= shade[..., None]
    arr = np.clip(grad, 0, 255).astype(np.uint8)
    img = Image.fromarray(np.dstack([arr, np.full((size, size), 255, np.uint8)]), "RGBA")

    # 圆角遮罩
    mask = Image.new("L", (size, size), 0)
    ImageDraw.Draw(mask).rounded_rectangle([0, 0, size - 1, size - 1], RADIUS, fill=255)
    img.putalpha(mask)

    d = ImageDraw.Draw(img)
    cx = size // 2
    white = (255, 255, 255, 255)
    stroke = (27, 136, 240, 255)
    w = size * 0.0115                     # 描边宽度
    hw = size * 0.205                     # 圆柱半宽
    top = size * 0.285                    # 顶盖上沿
    eh = size * 0.078                     # 椭圆半轴(竖直)
    bottom = size * 0.745                 # 底部椭圆中心

    # 桶身
    d.rectangle([cx - hw, top + eh, cx + hw, bottom], fill=white)
    d.ellipse([cx - hw, bottom - eh, cx + hw, bottom + eh], fill=white)
    # 中部两道环带
    for cy in (top + eh * 3.4, bottom):
        d.ellipse([cx - hw, cy - eh, cx + hw, cy + eh], outline=stroke, width=int(w))
    # 顶盖(覆盖环带上半部分)
    d.ellipse([cx - hw, top, cx + hw, top + 2 * eh], fill=white, outline=stroke, width=int(w))
    # 两侧竖边补齐
    d.line([cx - hw, top + eh, cx - hw, bottom], fill=stroke, width=int(w))
    d.line([cx + hw, top + eh, cx + hw, bottom], fill=stroke, width=int(w))

    # 右下角小圆点徽章: 数据/连接的寓意 (白心底色蓝点)
    br = size * 0.085
    bx, by = size * 0.715, size * 0.715
    d.ellipse([bx - br, by - br, bx + br, by + br], fill=(255, 255, 255, 255))
    ibr = br * 0.62
    d.ellipse([bx - ibr, by - ibr, bx + ibr, by + ibr], fill=(27, 136, 240, 255))
    cl = br * 0.30
    d.ellipse([bx - cl, by - cl, bx + cl, by + cl], fill=(255, 255, 255, 255))
    return img


def main():
    master = make_master()
    sizes = [16, 20, 24, 32, 40, 48, 64, 96, 128, 256]
    pngs = []
    for s in sizes:
        im = master.resize((s, s), Image.LANCZOS)
        im.save(os.path.join(OUT_DIR, f"logo_{s}.png"))
        pngs.append(im)
    master.resize((512, 512), Image.LANCZOS).save(os.path.join(OUT_DIR, "logo_512.png"))
    ico_sizes = [16, 24, 32, 48, 64, 128, 256]
    ico_imgs = [master.resize((s, s), Image.LANCZOS) for s in ico_sizes]
    ico_imgs[0].save(os.path.join(OUT_DIR, "logo.ico"), format="ICO",
                     sizes=[(s, s) for s in ico_sizes], append_images=ico_imgs[1:])
    print("assets generated:", OUT_DIR)


if __name__ == "__main__":
    main()
