# 480×272 横屏素材

主菜单山城、通用面板、金色「三国霸业」题字、四张历史时期插图和三组君主肖像母图均由内置 GPT Image 生成。原图保存在 generated/，提示词按同名文件保存在 prompts/。

h1/prepare_landscape.py 将透明题字合成到四个主菜单选中态，将历史插图缩至 100×87，并按原君主编号打包 33 张 72×72 肖像。普通菜单项、卡片文字和底栏使用原生点阵字库。H1 界面的君主预览、战略底栏和对应人物详情可使用新肖像。

战略地图使用 GPT Image 按 38 座城池的原有坐标绘制的整幅地图 `generated/strategy-world-integrated.png`。地形和建筑一起缩至 480×360。游戏按像素取出 480×240 的地图画面，并按城池数据叠加 8×14 的垂旗。其他势力挂红旗，玩家挂蓝旗，空城只显示城楼。旗帜以 `generated/strategy-banner-hd.png` 为母图。像素规格和校验值见 `manifest.json`。

sango.h1r 第 7 版新增 480×272 制作群组页。底板由 GPT Image 参考竖版重画。通宵虫、南方小鬼和 Sunday 的头像取自竖版彩色页，HelloClyde 使用 GitHub 公开头像。主菜单四项使用 24px 马善政楷体。安装时将 H1Sango.bda 和 sango.h1r 一起更新，原画及排版产物随 H1Sango-landscape-assets.zip 提供。[HelloClyde 的 GitHub 主页](https://github.com/HelloClyde)

用 python h1/test_landscape_art.py 检查编译后 MIPS 页面渲染。测试覆盖四个主菜单选中态、四张剧本插图、跨四期的五个君主头像、旧七素材包兼容和错误肖像尺寸拒绝。见 reports/h1-landscape-art-mips.json。
