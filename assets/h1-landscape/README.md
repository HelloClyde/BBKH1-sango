# 480×272 横屏素材

两张美术母图由内置 image_gen.imagegen 生成：generated/main-menu.png 是重新构图的木雕菜单与山城场景，generated/panel.png 是木框与宣纸内容区。提示词见 prompts/main-menu.txt、prompts/panel.txt。

h1/prepare_landscape.py 在目标尺寸排字，生成 native/ 中四个菜单选中态、通用面板、横排剧本卡片和战斗栏，并编码 sango.h1r。剧本插图、君主和地图沿用上游图像索引；没有重绘所有游戏精灵。年份 190 / 198 / 208 / 225 从实际剧本资源核对。

主菜单标题 24px、菜单项 18px；剧本页标题 20px、卡片名称 16px。运行时正文 / 按钮 14px，页面标题 20px。动态字体见 h1/font.inc，生成与无裁切检查由 h1/make_font.py 完成。

manifest.json 记录尺寸、来源和指纹。native/sango.h1r 为运行依赖，须与新版 BDA 一起更新。本目录打入 H1Sango-landscape-assets.zip，供编辑复查。许可边界沿用项目来源声明。
