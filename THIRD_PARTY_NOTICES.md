# 来源与第三方声明

| 内容 | 来源与署名 | 许可状态 |
| --- | --- | --- |
| 原游戏恢复代码、初始化数据、接口与布局材料 | 本次用户提供的 9588 三国霸业 BDA；策划 BBK gamegroup，程序 通宵虫、南方小鬼，美工 Sunday | 未提供源码开源/再分发许可证；不纳入本项目新增代码的 MIT 授权 |
| 原游戏运行资源 `sango.LIB` | 用户已有的游戏安装，439720 字节；按用户要求完整解包到 `assets/library/`，以 PNG、JSON 及原始记录种子保存，并可重新打包随安装包使用 | 原作者开源/再分发授权未提供；不纳入 MIT |
| 9288S 原图参考 | 用户提供的 9288S 三国霸业 EXE 中的应用图标、底栏和制作群组资源；参考 PNG 及三份草稿参考图集随 `assets/` 保存 | 原始 EXE 不分发；未提供单独美术许可 |
| GPT Image 母图、目标 PNG、打包 include | 参考前述原图生成并适配，提示词与生成记录随 `assets/` 保存 | 不据此声称取得原作图像的开源授权；不按 MIT 美术素材分发 |
| 全部游戏内彩色素材及`sango.clr` | 参考原LIB素材和补充底栏/制作群组，通过GPT Image上色；22张图形美术母图（另保留校正前版本）、提示词、1204帧语义PNG和映射位于`assets/color/` | 原图派生素材，保留原作许可边界；不按MIT美术素材授权 |
| 固定标签的六个点阵字形 | 本机Windows安装的宋体（SimSun）以16px、单比特点阵方式离线栅格化；仅保存6张PNG，字体文件不分发；来源文件哈希见`assets/color/battle-ui/glyphs/provenance.json` | 字体衍生输出，不据此授予原字体许可，不纳入MIT美术授权 |
| 三张彩色草稿 | 此前生成的 64 头像图集、剧本选择与君主/存档底图，位于 `assets/drafts/color9288s/` | 保留供后续适配，当前不打入 BDA/LIB；许可范围同其他派生素材 |
| 真实运行截图 | 本项目确切 BDA 的 C200 固件模拟器运行画面，战场使用受控夹具 | 截图包含原游戏内容，无额外开源素材授权 |
| SDK 与 BDA 编码/校验工具 | HelloClyde/BBK9588-bda-sdk，固定 ef3bf3b7328a3a8fa9ba48593c63285300725fb7 | 以上游子模块 LICENSE/NOTICE 为准，未将其许可套用于原游戏 |
| 新增构建、打包、适配代码 | BBK9588-sango contributors | 仅根 LICENSE.md 列出的独立新代码采用 MIT |

用于确定来源的原文件哈希：

- 9588 原 BDA：`38f3740af4d21ea3108e964b2824b518b1b5cc837e81fcb9b3cfefced7ff8ac6`。
- 9588 `sango.LIB`：`42920210b5a130781ed1f1cce4bdaca3fc1bb95f14fac8d88d170a9ad77490a5`。
- 9288S 原 EXE：`4eafbd3c59b64bf56fb810e47de43cb9d1075530801a8f991cf007f510954f9c`。

`src/sango_data.c` 是恢复工程必需的 45536 字节初始化数据的 C 表示；没有将它冒称为独立创作的数据。
原 `src/sango_game.c` 已拆成42个功能模块，原函数地址和当前名称/位置在 `source-map.json` 中保留。
模块拆分、命名与格式化不改变恢复代码的来源或许可范围。
原机完整 BDA/EXE、系统固件、NAND、系统字库转储、个人存档、USB 驱动、工具链和大型实验日志均不随仓库分发。
`sango.LIB` 内原有的游戏字模作为游戏资源的一部分完整保留，不将其宣称为独立授权字库。
感谢及来源记录不等同于许可证。

`assets/music/runtime/` 收录用户指定的三首完整音乐的压缩转换结果，标题、母带SHA-256、
FFmpeg转换版本及参数见 `assets/music/manifest.json`。原FLAC母带保留在用户本地且未修改。
用户提供文件用于本项目背景音乐；未另行提供音乐作者及再授权条款，不将音乐归入MIT代码许可。

升级包所含 SDK 编译部分采用上游提供的 Apache-2.0 许可选项，随包保留 SDK 的 NOTICE
和完整 Apache-2.0 文本；这不改变原游戏及其素材的许可状态。

## H1 移植来源

- H1 适配基于 HelloClyde/BBK9588-sango 提交 8786793bd9c7c102b05b72cd9d88887d100ac60d；上述原作者、恢复代码与素材的许可缺项仍然适用。
- 当前 H1 SDK 为 MrDefinition1999/bbk-h1-bda-sdk，固定 067fe072477861dfc8949d7b1a55279fb92d2548，Apache-2.0，正文与 NOTICE 保留在 sdk/，发行包附 LICENSES/H1-SDK-Apache-2.0.txt 及 H1-SDK-NOTICE.txt。
- assets/h1-landscape/ 的母图通过内置 image_gen 生成，随后按 480×272 排版；来源和提示词随目录保存，原游戏派生内容不因此获得额外授权。
- Noto Sans CJK SC 字体位于 assets/fonts/，来源 https://github.com/notofonts/noto-cjk ，SHA256 为 2c76254f6fc379fddfce0a7e84fb5385bb135d3e399294f6eeb6680d0365b74b，SIL OFL 1.1。h1/font.inc 为其栅格化输出。
- H1 音频实现 h1/audio.c、audio.h 源自本工作区 src/platform/audio.c 的 H1 适配，GPL-2.0，完整源码随仓库提供。原游戏与其素材不因音频实现的许可获得额外授权。
- reports/hardware-20261117/ 的截图来自物理 H1 / V1.41，其他截图来源按验证记录区分；包含的游戏内容不另行授予美术许可。

## 0.2.3 开发候选点阵字体

中文采用文泉驿点阵宋体的原生 12 / 14 / 16px BDF，版权 (C)2004-2010 WenQuanYi Project Board of Trustees and Qianqian Fang，GPL-2.0 及字体嵌入例外；ASCII 采用 Terminus 4.49 原生 6×12 / 7×14 / 8×16 BDF，版权 (C)2020 Dimitar Toshkov Zhekov，SIL OFL 1.1，保留其 Reserved Font Name 声明。源 BDF 未改，版本、来源与校验值见 assets/fonts/bitmap/manifest.json。少量兼容字符通过原始像素组合或无缩放来源回退补齐；24px 字号仅对 12px 点阵作 2 倍整数放大。生成字形保留各自字体许可和上游例外，不因此将原游戏或素材额外授权。旧 Noto 文件仅为此前版本来源记录，当前 H1 字库及主菜单排字不再调用 Noto 矢量字栅格化。
