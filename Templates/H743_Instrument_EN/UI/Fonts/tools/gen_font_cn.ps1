# 一键重新生成中文字库 font_cn_<size>.c
#
# 依赖（本机均已具备）：
#   - node/npm 全局包 lv_font_conv（npm install -g lv_font_conv）
#   - uv（用于跑 fonttools，从可变字体实例化静态字重）
#   - python 3（生成字符集）
#   - 字体源：C:\Windows\Fonts\NotoSansSC-VF.ttf（系统已安装的 Noto Sans SC 可变字体）
#     若缺失，从官方仓库获取：https://github.com/notofonts/noto-cjk（OFL 许可）
#
# 用法：
#   .\gen_font_cn.ps1                 # 默认 16px, wght=500 (Medium)
#   .\gen_font_cn.ps1 -Size 20        # 生成 20px 档（另需自行加进 CMake 与 lv_conf.h）
#   .\gen_font_cn.ps1 -ExtraChars x.txt  # 补充生僻字：x.txt 里出现过的字并入字库
param(
    [int]$Size = 16,
    [int]$Weight = 500,
    [string]$ExtraChars = ""
)
$ErrorActionPreference = "Stop"

$toolsDir = $PSScriptRoot
$fontsDir = Split-Path $toolsDir -Parent          # UI/Fonts
$work = Join-Path $env:TEMP "cn_font_build"
New-Item -ItemType Directory -Force $work | Out-Null

$vf = Join-Path $work "NotoSansSC-VF.ttf"
$static = Join-Path $work "NotoSansSC-w$Weight.ttf"
$charset = Join-Path $work "charset_cn.txt"
$outName = "font_cn_$Size"
$outFile = Join-Path $fontsDir "$outName.c"

# 1. 取字体源（系统字体目录只读，先复制出来）
if (-not (Test-Path $vf)) {
    Copy-Item "C:\Windows\Fonts\NotoSansSC-VF.ttf" $vf
}

# 2. 可变字体 -> 静态字重（uv 缓存如遇 D 盘不可用，用本地缓存目录兜底）
if (-not (Test-Path $static)) {
    $env:UV_CACHE_DIR = "$env:LOCALAPPDATA\uv-cache-local"
    uvx --from fonttools fonttools varLib.instancer $vf "wght=$Weight" -o $static
}

# 3. 字符集：GB2312 一级 + 标点 + 工程符号（ASCII 由 --range 覆盖）
if ($ExtraChars) {
    python (Join-Path $toolsDir "gen_charset.py") $charset $ExtraChars
} else {
    python (Join-Path $toolsDir "gen_charset.py") $charset
}
$symbols = Get-Content $charset -Raw

# 4. 转 LVGL C 字库（4bpp 抗锯齿、不压缩＝渲染零开销；缺字回落 montserrat_14）
lv_font_conv --font $static --size $Size --bpp 4 --format lvgl --no-compress `
    --lv-fallback lv_font_montserrat_14 `
    --range 0x20-0x7E --symbols $symbols -o $outFile

Write-Host "OK -> $outFile  ($([math]::Round((Get-Item $outFile).Length/1MB,2)) MB 源文件)"
Write-Host "提示：新增字号档时，需在根 CMakeLists.txt 的 target_sources 与 lv_conf.h 的 LV_FONT_CUSTOM_DECLARE 中同步登记。"
