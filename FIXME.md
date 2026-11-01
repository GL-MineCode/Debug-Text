# FIXED:
- [x] 生成器改为「一站式」：TTF → 字库 header（不再产出 `font.bin`，也不再需要 `toarray.py`）
- [x] 生成器预览窗口使用 NEAREST 缩放（`--preview`）
- [x] 升级到 fusion pixel 2026.10.25，并提供 8/10/12px mono zh_hans 三档
- [x] 点阵字体属性写进字库 header（`dbtext::FontDesc`）；`DebugText.hpp` 改为注册式多字库 + 按 FontID 选择
- [x] 字库存储升级为 v2（存在性位图 + 半/全角位图 + 紧排 atlas），体积下降 62%~76%

# FIX THIS:
- 生成器逐个码点调用 `TTF_FontHasGlyph` 扫描到 U+10FFFF，三个字库合计约 20s；可按块跳过无字形区域
- 8/10/12px 各有 0~12 个「超出单元格」的字形落进 extras（体积可忽略），如需完全避免可调大 `--cell-h`
- loadtest 还没演示 `dbtext::ttf` 的 TTF 回环导出（目前只在临时验证脚本里跑过）
- 可选：`paintDBText` 目前每帧重建 surface/texture，可以加「字符串 → 纹理」LRU 缓存
