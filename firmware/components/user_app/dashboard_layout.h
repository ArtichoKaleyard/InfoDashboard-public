#pragma once

#include <cstdint>

namespace dashboard_layout {

// 本文件是手动调整 UI 的主入口。
//
// 坐标系：
// - 屏幕按横屏 400 x 300 设计，原点在左上角。
// - Rect = {x, y, w, h}，单位都是像素。
// - 主卡片内部坐标相对卡片左上角；顶栏/辅助区坐标相对屏幕。
//
// 调整建议：
// - 只改位置/尺寸时优先改这里，不要先动 user_app.cpp。
// - 左右两张主卡建议保持同宽同高，便于扫读。
// - 百分比数字区域必须给足宽度，RLCD 字体边缘容易丢像素。
// - 虚线分隔线、条形图、底部统计是一组视觉节奏，移动其中一个通常要同步移动同区域元素。
struct Rect {
    int16_t x;
    int16_t y;
    int16_t w;
    int16_t h;
};

namespace Screen {
inline constexpr int16_t kWidth = 400;
inline constexpr int16_t kHeight = 300;
}  // 命名空间 Screen

namespace Header {
// 顶栏黑底条。改黑条高度只动 kBar.h；改文字位置只动 kTitle/kDate/kTime。
// 字体和白字颜色在 dashboard_style.h，文字内容在 dashboard_text.h。
inline constexpr Rect kBar{0, 0, 400, 24};
inline constexpr Rect kTitle{8, 7, 130, 12};
inline constexpr Rect kDate{154, 7, 104, 12};
inline constexpr Rect kTime{298, 5, 92, 14};
}  // 顶栏命名空间

namespace Server {
// 左卡外框。所有服务器子项都是相对 kPanel 左上角的坐标。
inline constexpr Rect kPanel{8, 30, 188, 218};

// 卡片顶栏：节点名、状态块、下方实线。
inline constexpr Rect kTarget{6, 3, 132, 14};
inline constexpr Rect kStateBadge{142, 3, 40, 14};
inline constexpr Rect kHeaderRule{0, 20, 188, 1};

// 显卡大数字区。0/1 两列分别改 kGpu0* / kGpu1*；中间虚线改 kGpuDividerY。
inline constexpr Rect kGpuDividerY{94, 26, 1, 66};
inline constexpr Rect kGpu0Label{7, 28, 36, 10};
inline constexpr Rect kGpu0Temp{49, 28, 32, 10};
inline constexpr Rect kGpu0Util{7, 42, 54, 30};
inline constexpr Rect kGpu0Pct{65, 56, 24, 16};
inline constexpr Rect kGpu0Cells{7, 79, 82, 7};
inline constexpr Rect kGpu1Label{99, 28, 36, 10};
inline constexpr Rect kGpu1Temp{141, 28, 32, 10};
inline constexpr Rect kGpu1Util{99, 42, 54, 30};
inline constexpr Rect kGpu1Pct{157, 56, 24, 16};
inline constexpr Rect kGpu1Cells{99, 79, 82, 7};

// 显存两行和上分割线。条形图绘制样式在 dashboard_style.h 的分段条参数。
inline constexpr Rect kDividerTop{7, 96, 174, 1};
inline constexpr Rect kVram0Label{7, 108, 96, 10};
inline constexpr Rect kVram0Pct{145, 108, 36, 10};
inline constexpr Rect kVram0Bar{7, 122, 174, 12};
inline constexpr Rect kVram1Label{7, 140, 96, 10};
inline constexpr Rect kVram1Pct{145, 140, 36, 10};
inline constexpr Rect kVram1Bar{7, 154, 174, 12};

// 底部统计行。你刚调的功耗、处理器、内存、进程数位置都在这里。
inline constexpr Rect kDividerBottom{7, 172, 174, 1};
inline constexpr Rect kPower0{7, 182, 74, 10};
inline constexpr Rect kPower1{104, 182, 76, 10};
inline constexpr Rect kCpu{7, 202, 54, 10};
inline constexpr Rect kMem{70, 202, 58, 10};
inline constexpr Rect kProc{132, 202, 49, 10};
}  // 服务器卡命名空间

namespace Codex {
// 右卡外框。所有 Codex 子项都是相对 kPanel 左上角的坐标。
inline constexpr Rect kPanel{204, 30, 188, 218};

// 卡片顶栏：标题、状态块、下方实线。
inline constexpr Rect kTitle{6, 3, 132, 14};
inline constexpr Rect kState{142, 3, 40, 14};
inline constexpr Rect kHeaderRule{0, 20, 188, 1};

// 上方趋势区。左列是 7_DAY，右列是 5_HR；中间虚线改 kTrendDividerY。
inline constexpr Rect kTrendDividerY{94, 26, 1, 66};
inline constexpr Rect kTrend0Label{7, 28, 70, 10};
inline constexpr Rect kTrend0Chart{7, 44, 82, 42};
inline constexpr Rect kTrend1Label{99, 28, 70, 10};
inline constexpr Rect kTrend1Chart{99, 44, 82, 42};

// 配额条区。周额度/五小时额度的文字、百分比和条形图可以分别移动。
inline constexpr Rect kDividerTop{7, 96, 174, 1};
inline constexpr Rect kWeekLabel{7, 108, 70, 10};
inline constexpr Rect kWeekPct{140, 108, 40, 10};
inline constexpr Rect kWeekBar{7, 122, 174, 12};
inline constexpr Rect kFiveHourLabel{7, 140, 70, 10};
inline constexpr Rect kFiveHourPct{140, 140, 40, 10};
inline constexpr Rect kFiveHourBar{7, 154, 174, 12};

// 底部重置时间。左侧是额度窗口，右侧保留更完整的重置时间。
inline constexpr Rect kDividerBottom{7, 172, 174, 1};
inline constexpr Rect kWeekResetLabel{7, 182, 70, 10};
inline constexpr Rect kWeekResetValue{82, 182, 98, 10};
inline constexpr Rect kFiveHourResetLabel{7, 202, 70, 10};
inline constexpr Rect kFiveHourResetValue{82, 202, 98, 10};
}  // 命名空间 Codex

namespace Aux {
// 辅助信息区。天气和室内传感器在第一行，页脚链路/电池/更新时间在最下方。
inline constexpr Rect kAuxRule{0, 254, 400, 1};
inline constexpr Rect kWeatherValue{14, 259, 238, 12};
inline constexpr Rect kIndoorValue{268, 259, 120, 12};

// 页脚第二行是深色底栏，文字使用白色。
inline constexpr Rect kFooterBar{0, 276, 400, 24};
inline constexpr Rect kFooterLink{14, 283, 154, 12};
inline constexpr Rect kFooterBattery{178, 283, 86, 12};
inline constexpr Rect kFooterUpdated{292, 283, 96, 12};
}  // 辅助区命名空间

}  // 命名空间 dashboard_layout
