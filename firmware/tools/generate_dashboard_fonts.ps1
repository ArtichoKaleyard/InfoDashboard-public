param(
    [string]$FontRoot = (Join-Path $PSScriptRoot "..\..\..\temp\fonts")
)

$ErrorActionPreference = "Stop"

$componentDir = Resolve-Path (Join-Path $PSScriptRoot "..\components\user_app")
$fusionRoot = Join-Path $FontRoot "extracted\fusion-pixel-font-ttf"
$zhenggeRoot = Join-Path $FontRoot "manual\zhengge-dianhei-16"

$fusion10 = Join-Path $fusionRoot "fusion-pixel-font-10px-proportional-ttf-v2026.05.07\fusion-pixel-10px-proportional-latin.ttf"
$fusion12 = Join-Path $fusionRoot "fusion-pixel-font-12px-proportional-ttf-v2026.05.07\fusion-pixel-12px-proportional-latin.ttf"
$consolasRegular = Join-Path $env:WINDIR "Fonts\consola.ttf"
$consolasBold = Join-Path $env:WINDIR "Fonts\consolab.ttf"
$segoeUi = Join-Path $env:WINDIR "Fonts\segoeui.ttf"
$segoeUiSemibold = Join-Path $env:WINDIR "Fonts\seguisb.ttf"
$zhengge16 = Join-Path $zhenggeRoot "ZhengGeDianHei-16.ttf"

foreach ($path in @($fusion10, $fusion12, $consolasRegular, $consolasBold, $segoeUi, $segoeUiSemibold, $zhengge16)) {
    if (!(Test-Path -LiteralPath $path)) {
        throw "Font source not found: $path"
    }
}

$jobs = @(
    @{
        Name = "dashboard_font_ui_10_1bpp"
        Size = 10
        Font = $fusion10
        Output = "dashboard_font_ui_10_1bpp.c"
    },
    @{
        Name = "dashboard_font_ui_12_1bpp"
        Size = 12
        Font = $fusion12
        Output = "dashboard_font_ui_12_1bpp.c"
    },
    @{
        Name = "dashboard_font_ui_consolas_regular_14_1bpp"
        Size = 14
        Font = $consolasRegular
        Output = "dashboard_font_ui_consolas_regular_14_1bpp.c"
    },
    @{
        Name = "dashboard_font_ui_consolas_bold_14_1bpp"
        Size = 14
        Font = $consolasBold
        Output = "dashboard_font_ui_consolas_bold_14_1bpp.c"
    },
    @{
        Name = "dashboard_font_ui_28_1bpp"
        Size = 32
        Font = $segoeUi
        Output = "dashboard_font_ui_28_1bpp.c"
    },
    @{
        Name = "dashboard_font_ui_16_1bpp"
        Size = 16
        Font = $zhengge16
        Output = "dashboard_font_ui_16_1bpp.c"
    },
    @{
        Name = "dashboard_font_ui_segoe_semibold_16_1bpp"
        Size = 16
        Font = $segoeUiSemibold
        Output = "dashboard_font_ui_segoe_semibold_16_1bpp.c"
    }
)

foreach ($job in $jobs) {
    $output = Join-Path $componentDir $job.Output
    $range = if ($job.Range) { $job.Range } else { @("0x20-0x7F") }
    $args = @("--bpp", "1", "--size", $job.Size, "--font", $job.Font)
    foreach ($item in $range) {
        $args += @("-r", $item)
    }
    $args += @("--format", "lvgl", "--lv-include", "lvgl.h", "--lv-font-name", $job.Name, "-o", $output)
    npx lv_font_conv @args
}
