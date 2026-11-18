# 480×272 横屏素材

主菜单山城、通用面板、金色「三国霸业」题字、四张历史时期插图和三组君主肖像母图均由内置 GPT Image 生成。原图保存在 generated/，提示词按同名文件保存在 prompts/。

h1/prepare_landscape.py 将透明题字合成到四个主菜单选中态，将历史插图缩至 100×87，并按原君主编号打包 33 张 72×72 肖像。普通菜单项、卡片文字和底栏使用原生点阵字库。H1 界面的君主预览、战略底栏和对应人物详情可使用新肖像。

sango.h1r 第 3 版包含八项素材，前七项仍是主菜单四态、通用面板、剧本和战斗栏，第八项是肖像条带。manifest.json 记录尺寸和源文件校验值。安装时将 H1Sango.bda 和 sango.h1r 一起更新。原画、提示词及排版产物随 H1Sango-landscape-assets.zip 提供。

用 python h1/test_landscape_art.py 检查编译后 MIPS 页面渲染。测试覆盖四个主菜单选中态、四张剧本插图、跨四期的五个君主头像、旧七素材包兼容和错误肖像尺寸拒绝。见 reports/h1-landscape-art-mips.json。
