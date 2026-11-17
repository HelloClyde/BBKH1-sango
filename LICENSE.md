# 许可证范围

本仓库不是整仓统一许可证。只有下列新增、独立编写的构建与适配代码采用 MIT：

- `build.py`、`tools/*.py`、`tools/*.ps1` 和 `.github/workflows/release.yml`；
- `src/sango_entry.c`、`src/sango_statusbar.c`、`src/sango_credits.c`、`src/sango_battle_ui.c`、`src/sango_color.c`、`src/sango_color_sdk.c`、`src/sango_color_animation.c`、`src/sango_touch.c`、`src/sango_battle_info.c`；
- `src/sango_music.c`、`src/sango_music_sdk.c`；
- 与上述新模块对应的头文件，新增类型/内部接口组织头文件，以及项目私有
  `include/bda_sango_compat.h`、`include/sango_sdk_bindings.h`。其中恢复的原接口、结构布局及地址信息不因此取得额外授权。

MIT 正文见 [LICENSES/MIT.txt](LICENSES/MIT.txt)。版权人为 2026 BBK9588-sango contributors。

原游戏恢复部分包括 `src/app/`、`src/battle/`、`src/strategy/`、`src/ui/`、`src/render/`、
`src/resources/`、`src/save/`、`src/platform/`、`src/runtime/` 的42个功能模块（由原 `sango_game.c` 拆分），
以及 `src/sango_data.c` 和头文件中的恢复布局信息；
原作布局/接口材料及原作派生图像，不包含在上述 MIT 授权中。本次没有提供原游戏的源码开源或
素材再分发许可，因此这里不替原作者授予额外权利。新增修复不能使原游戏整体自动获得 MIT 许可。
`assets/library/` 的原始记录种子、导出 PNG/动画/地图，以及由其重新打包的 `sango.lib`，
同样属于原游戏素材，不包含在新增工具脚本的 MIT 授权中。

`sdk/` 子模块保留上游自己的 Apache-2.0 OR GPL-2.0-only 等声明，具体范围以上游为准。
SDK 的许可不覆盖原游戏、字库、原图、`sango.LIB` 或固件。
截图中含原游戏画面，GPT Image 图像也源于原图参考，均不作为 MIT 美术素材授权。

来源、作者、未提供的许可及不分发内容见 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)。

## H1 移植补充

`h1/backend.c`、`h1/native_ui.c`、`h1/music.c`、`h1/entry.S`、H1 接口头文件和 H1 构建 / 素材 / 检查 Python 脚本采用 LICENSES/MIT.txt 中的 MIT 条款，版权人为 2026 BBKH1-sango contributors；恢复接口、数据布局和原作内容的既有权利不受此声明改变。

`h1/audio.c`、`h1/audio.h` 来自本工作区 H1 音频实现，保留 GPL-2.0 许可，正文见 LICENSES/H1-audio-GPL-2.0.txt；不将其改称 MIT。`h1/font.inc` 和 assets/fonts/NotoSansCJKsc-Regular.otf 来自 Noto Sans CJK，使用 SIL OFL 1.1，正文见 LICENSES/Noto-OFL-1.1.txt。

当前 sdk/ 已改为 MrDefinition1999/bbk-h1-bda-sdk 的固定提交 067fe072477861dfc8949d7b1a55279fb92d2548，沿用其 Apache-2.0 LICENSE / NOTICE，旧 9588 SDK 的说明仅描述上游历史。
