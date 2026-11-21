# 战场兵种大图

参考图从 `sango.lib` 和 `sango.clr` 解码。16×16 原帧按最近邻放大 24 倍，每格 384×384 像素。使用 Codex 内置 GPT Image 以四张放大图为参考生成大图。此处保存原始输出，背景为深灰色。

| 图集 | 参考图 | GPT Image 大图 | 大图尺寸 | 帧号（从左到右，先上后下） |
| --- | --- | --- | --- | --- |
| 1 | [参考](reference-1.png) | [骑兵、持刀步兵](master-1.png) | 1774×887 | 0–3，16–19 |
| 2 | [参考](reference-2.png) | [其余步兵](master-2.png) | 1774×887 | 4–7，20–23 |
| 3 | [参考](reference-3.png) | [持盾兵、弓兵](master-3.png) | 1774×887 | 8–11，24–27 |
| 4 | [参考](reference-4.png) | [战船](master-4.png) | 1254×1254 | 12–13，28–29 |

## 去马赛克版

以第一轮 GPT Image 大图为构图参考，用内置 GPT Image 重绘轮廓与色块交界。新图采用连续曲线和较简洁的明暗关系，人物比第一轮更偏手绘。帧号沿用上表。

[骑兵和持刀步兵](smooth-1.png) · [其余步兵](smooth-2.png) · [持盾兵和弓兵](smooth-3.png) · [战船](smooth-4.png)

三张横图为 1774×887，战船图为 1254×1254。

游戏构建使用导入后的 [24×24 帧条](../../native/battle-units-gpt-image-hd.png)。导入脚本去除深灰色背景，按原帧的边界放置 28 帧兵种和战船，四帧混乱与石阵状态图沿用原版。资源库生成后，在仓库根目录运行以下命令可更新帧条。

```sh
python h1/import_battle_units.py assets/h1-landscape/generated/battle-units-large assets/h1-landscape/native/battle-units-gpt-image-hd.png --library build/h1/sango.lib --colors build/h1/sango.clr
```

### 去马赛克版提示词

每张图共用以下提示词，再附上对应的图集描述。

> Redraw the attached sprite sheet as sharp high-definition 2D game art so the mosaic is completely gone at 100% zoom. The input is ONLY a placement and character-design guide. Reconstruct every figure from scratch with continuous hand-inked contours, smoothly curved clean shapes, properly anti-aliased at the native output resolution, and restrained 2–3 tone painted cel shading. Pixel staircases, square mosaic blocks, uniform dithering, square brush dabs, jagged edges and visible coarse pixels are absolutely forbidden anywhere, including inside figures. The dark outlines should be clear, slender and smooth, with corners only where forms naturally have corners. No blur filter, no upscaling of source pixels. A high-definition remaster of retro Chinese Three Kingdoms game sprites with their original muted red, indigo, bronze, cream, and wood colors, crisp enough to shrink to small icons. Use a few readable shapes and clear value separation at icon scale. Preserve each individual subject's silhouette, proportions, pose, direction, equipment, and flat camera angle. Preserve the obvious differences between the adjacent animation cells; do not repeat or mirror a different cell. Keep the exact cell centers, size, and placement from the reference. Every subject sits isolated on the same uniform solid dark slate background (#262C33), no grounding shadow, no decorative texture, no labels, no separator lines, no extra elements.

1. Wide landscape atlas, aspect ratio 2:1, exact four columns and two rows, eight sprites. Two riders followed by two sword infantry per row. Preserve each rider's actual hand, horse-leg, and spear position; preserve the original high blade, dark cap outline and cream shield on each infantry sprite.

2. Wide landscape atlas, aspect ratio 2:1, exact four columns and two rows, eight sprites. Left half with held equipment and prominent helmets, right half with compact walking units. Keep each costume's color, orientation, equipment, and foot position.

3. Wide landscape atlas, aspect ratio 2:1, exact four columns and two rows, eight sprites. Shield units in left half and bow units in right half. Carefully preserve which side each fighter faces and carries their shield or bow. The bow curves are now uninterrupted clean strokes, and shield surfaces form broad coherent shapes.

4. Square atlas, exact two columns and two rows, four sprites. Keep the original boat shapes and framing, pale sails, wood, red hulls, and two different faction-color rows. Use elegant continuous curved sails, clean hull contours and a clean clear outline. Preserve the precise orientation and proportion of each boat.

## 提示词

每张图都使用以下提示词，再附上对应的图集描述。

> Precision sprite restoration, for large source art. Edit this one enlarged source image into a very high-resolution master game sprite sheet. Treat each source sprite as a literal tracing guide, independently. The edit is to build crisp, finer-grained pixel-art structure out of the coarse color blocks, using clean hard-edged stair steps, small coherent color clusters and dark one-to-two pixel outlines at an imagined logical 64x64 sprite resolution. The final output should be a large sheet at maximum practical resolution and each entire sprite needs to occupy the same footprint and position within its cell as its exact corresponding source. Preserve the original compact retro proportions, projection, outline geometry, gaze, pose, weapon orientation, helmet, costume, and muted original regional colors. Preserve all the subtle animation pose differences between adjacent cells. Reference is the exact placement and color guide. Add only definition to visible shapes, not new design details. No long legs, no realistic rendering, no cartoon or vector styling, no change of faction colors, no gradient or airbrush, no extra weapons, no new features, no extra ground shadows, no glow, no bases, no captions, no panel borders or grid lines. Keep the flat dark slate background #282E35 from the reference, uniform edge to edge, with no texture.

### 图集 1

> Output a wide 2:1 large raster master atlas (at least 2048x1024; 3072x1536 if available). Exactly eight separate sprites on a precise invisible 4-column by 2-row lattice. The left two cells of each row depict the mounted unit; keep horse, rider and the hand/weapon angle recognizable from the exact source pose. The right two cells of each row depict a compact walking foot unit with red cap and prominent tall blade. Top/bottom are different faction facings and palettes. Keep every one of the eight distinct silhouettes and all positions.

### 图集 2

> Output a wide 2:1 large raster master atlas (at least 2048x1024; 3072x1536 if available). Exactly eight separate sprites on a precise invisible 4-column by 2-row lattice. The four left-half cells preserve the original large helmets and vertically held gear; the four right-half cells preserve the compact slim soldiers and their changed stepping positions. Trace every individually colored helmet, weapon, hand and shoe from its own source cell. Do not turn these subtle poses into dramatically different animations.

### 图集 3

> Output a wide 2:1 large raster master atlas (at least 2048x1024; 3072x1536 if available). Exactly eight separate sprites on a precise invisible 4-column by 2-row lattice. The four left-half cells preserve the soldiers' distinctive broad rounded pale side equipment and squat stance. The four right-half cells preserve the dark curved bow shapes as seen in each exact source. Keep relative weapon sizes and shading clusters. Do not put a shield or bow on the opposite side or change the framing.

### 图集 4

> Output a square high resolution raster master atlas (at least 1024x1024, 1536x1536 if available). Exactly four separate sprites in the same 2 by 2 grid, with each shape centered where it is in its source. These are compact boat game icons of the same game, not characters; trace the original top-oblique boat silhouettes, pale upper sail shape, dark crossbeam and reddish lower hull. Preserve the diagonal directions as shown. Keep the top two right-accented red boats and bottom two left-accented blue boats, and keep each of the subtle original pose variations. No characters, water, background scenery, decorative ropes or new geometry.
