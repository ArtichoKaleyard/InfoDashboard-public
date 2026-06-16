#include "user_app.h"
#include "dashboard_layout.h"
#include "dashboard_style.h"
#include "dashboard_text.h"
#include "dashboard_widgets.h"

#include <cctype>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <cJSON.h>
#include <driver/gpio.h>
#include <driver/i2c_master.h>
#include <esp_adc/adc_cali.h>
#include <esp_adc/adc_cali_scheme.h>
#include <esp_adc/adc_oneshot.h>
#include <esp_crt_bundle.h>
#include <esp_err.h>
#include <esp_event.h>
#include <esp_heap_caps.h>
#include <esp_http_client.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <esp_sntp.h>
#include <esp_timer.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <lvgl.h>
#include <nvs.h>
#include <nvs_flash.h>
#include <sdkconfig.h>

#include "lvgl_bsp.h"

namespace {

constexpr char kTag[] = "InfoDashboard";
namespace layout = dashboard_layout;
namespace style = dashboard_style;
namespace text = dashboard_text;
namespace widgets = dashboard_widgets;
using widgets::CreateBadgeSmall;
using widgets::CreateDashedRule;
using widgets::CreateDashedRuleY;
using widgets::CreateFill;
using widgets::CreateLabel;
using widgets::CreateLedRow;
using widgets::CreateMiniChart;
using widgets::CreateMiniChartBudgetLine;
using widgets::CreateSegmentBar;
using widgets::CreateText;
using widgets::CreateThinPanel;
using widgets::DrawSegmentBar;
using widgets::DrawLedRow;
using widgets::LedRow;
using widgets::SegmentBar;
using widgets::SetMiniChart;
using widgets::SetMiniChartBudgetLine;

struct GpuCardSnapshot {
    char label[10];
    uint8_t util_pct;
    uint8_t mem_pct;
    uint8_t temp_c;
    uint16_t power_w;
    uint8_t processes;
    bool valid;
};

// 页面唯一的数据模型。ApplySnapshot() 只读取这个结构，不直接访问网络、
// NVS 或传感器，方便以后替换 UI 时保持数据入口稳定。
struct DashboardSnapshot {
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    char date[18];
    char weather_location[18];
    char weather_summary[18];
    float weather_temp_c;
    uint8_t weather_humidity_pct;
    float weather_wind_kmh;
    float indoor_temp_c;
    uint8_t indoor_humidity_pct;
    bool indoor_valid;
    char server_target[18];
    char server_role[18];
    char server_state[12];
    char server_summary[36];
    uint8_t server_cpu_pct;
    uint8_t server_mem_pct;
    uint16_t server_latency_ms;
    uint8_t gpu_util_pct;
    uint8_t gpu_mem_pct;
    uint8_t gpu_temp_c;
    uint8_t gpu_processes;
    uint8_t gpu_card_count;
    GpuCardSnapshot gpu_cards[2];
    char codex_state[12];
    char codex_source[12];
    uint8_t codex_week_remaining_pct;
    uint8_t codex_five_hour_remaining_pct;
    char codex_burn_rate[18];
    char codex_week_reset_at[24];
    char codex_five_hour_reset_at[24];
    uint8_t codex_week_trend[style::kMiniChartWeekBarCount];
    uint8_t codex_five_hour_trend[style::kMiniChartFiveHourBarCount];
    uint8_t codex_week_budget_line_pct;
    uint8_t codex_five_hour_budget_line_pct;
    uint16_t codex_trend_active_mask;
    int64_t codex_received_epoch;
    char updated_at[32];
    char link_state[18];
};

// esp_http_client 的流式回调缓冲。Codex history JSON 必须小于 kHttpResponseCapacity。
struct HttpBuffer {
    char *data;
    int capacity;
    int length;
    bool overflow;
};

enum class NetworkJob : uint8_t {
    kServerMonitor = 0,
    kCodex,
    kWeather,
    kCount,
};

const char *NetworkJobName(NetworkJob job);

struct NetworkJobRequest {
    bool pending;
    uint32_t sequence;
    int64_t due_us;
    int64_t submitted_us;
    int64_t stale_after_ms;
};

struct NetworkJobResult {
    bool ready;
    uint32_t sequence;
    bool ok;
    bool usage_summary_ok;
    int64_t elapsed_ms;
};

struct DiagnosticEntry {
    uint32_t sequence;
    int64_t uptime_ms;
    char source[12];
    char event[20];
    char detail[96];
};

struct DiagnosticState {
    uint32_t next_sequence;
    uint32_t total_entries;
    DiagnosticEntry entries[16];
    int last_http_status;
    int last_http_errno;
    int last_http_tls_error;
    int last_http_tls_flags;
    int64_t last_http_elapsed_ms;
    int last_http_len;
    bool last_http_overflow;
    char last_http_source[12];
    char last_http_result[20];
    char last_codex_state[12];
    char last_codex_link_state[18];
    int last_codex_week_remaining_pct;
    int last_codex_five_hour_remaining_pct;
    int64_t last_codex_received_epoch;
    bool last_codex_fetch_ok;
    bool last_codex_usage_summary_ok;
    bool last_codex_json_ok;
    char last_codex_error[28];
    char last_worker_job[12];
    bool last_worker_ok;
    bool last_worker_usage_summary_ok;
    int64_t last_worker_elapsed_ms;
};

struct WifiProfile {
    const char *ssid;
    const char *password;
    uint8_t index;
};

struct ServerMonitorEndpoint {
    const char *url;
    const char *label;
    uint8_t wifi_profile;
};

enum class HttpAuth {
    kNone,
    kCodexApiKey,
    kServerMonitorToken,
};

// LVGL 控件引用。字段顺序基本跟画面从上到下、从左到右一致；
// 新增可动态刷新的控件时，在这里加指针，并在 UserApp_UiInit()/ApplySnapshot()
// 中分别完成创建和赋值。
struct DashboardUi {
    lv_obj_t *screen;
    lv_obj_t *header;
    lv_obj_t *server_panel;
    lv_obj_t *server_header;
    lv_obj_t *codex_panel;
    lv_obj_t *codex_header;
    lv_obj_t *footer_bar;
    lv_obj_t *time;
    lv_obj_t *date;
    lv_obj_t *status;
    lv_obj_t *server_target;
    lv_obj_t *server_gpu_label;
    lv_obj_t *server_gpu_util;
    lv_obj_t *server_state_badge;
    lv_obj_t *server_role;
    lv_obj_t *server_summary;
    lv_obj_t *server_gpu_bar_text;
    lv_obj_t *server_gpu_bar;
    lv_obj_t *server_gpu_bar_pct;
    lv_obj_t *server_vram_text;
    lv_obj_t *server_vram_bar;
    lv_obj_t *server_vram_bar_pct;
    lv_obj_t *server_tmp_label;
    lv_obj_t *server_tmp_value;
    lv_obj_t *server_ping_label;
    lv_obj_t *server_ping_value;
    lv_obj_t *server_cpu_label;
    lv_obj_t *server_cpu_value;
    lv_obj_t *server_mem_label;
    lv_obj_t *server_mem_value;
    lv_obj_t *codex_trend_bars[12];
    lv_obj_t *codex_trend_title;
    lv_obj_t *codex_trend_range;
    lv_obj_t *codex_trend_mark_left;
    lv_obj_t *codex_trend_mark_mid;
    lv_obj_t *codex_trend_mark_right;
    lv_obj_t *codex_week;
    lv_obj_t *codex_week_bar;
    lv_obj_t *codex_week_pct;
    lv_obj_t *codex_five_hour;
    lv_obj_t *codex_five_hour_bar;
    lv_obj_t *codex_five_hour_pct;
    lv_obj_t *codex_state;
    lv_obj_t *codex_rate_label;
    lv_obj_t *codex_rate_value;
    lv_obj_t *codex_week_reset_label;
    lv_obj_t *codex_week_reset_value;
    lv_obj_t *codex_five_hour_reset_label;
    lv_obj_t *codex_five_hour_reset_value;
    lv_obj_t *weather_title;
    lv_obj_t *weather_value;
    lv_obj_t *indoor_title;
    lv_obj_t *indoor_value;
    lv_obj_t *footer_updated;
    lv_obj_t *footer_battery;
    lv_obj_t *footer_link;
    lv_obj_t *gpu_label[2];
    lv_obj_t *gpu_temp[2];
    lv_obj_t *gpu_util[2];
    lv_obj_t *gpu_pct[2];
    LedRow gpu_leds[2];
    lv_obj_t *vram_label[2];
    lv_obj_t *vram_pct[2];
    SegmentBar vram_bar[2];
    lv_obj_t *gpu_power[2];
    lv_obj_t *codex_week_chart_bars[style::kMiniChartWeekBarCount];
    lv_obj_t *codex_five_hour_chart_bars[style::kMiniChartFiveHourBarCount];
    lv_obj_t *codex_week_budget_line;
    lv_obj_t *codex_five_hour_budget_line;
    SegmentBar codex_week_bar_custom;
    SegmentBar codex_five_hour_bar_custom;
};

DashboardUi g_ui = {};

// 运行参数。UI 几何尺寸不要放这里，统一放 dashboard_layout.h。
constexpr EventBits_t kWifiConnectedBit = BIT0;
constexpr EventBits_t kWifiFailBit = BIT1;
constexpr size_t kWifiProfileCount = 3;
constexpr size_t kServerMonitorEndpointCapacity = 3;
constexpr uint8_t kWifiInvalidProfile = 0xFF;
constexpr int kWifiConnectTimeoutMs = 15000;
constexpr int64_t kWifiInitialRetryDelayMs = 5000;
constexpr int64_t kWifiMaxRetryDelayMs = 300000;
constexpr int kHttpResponseCapacity = 16384;
constexpr int kHttpTimeoutMs = 20000;
constexpr int64_t kHttpMutexWaitMs = 25000;
constexpr int64_t kHttpsTimeSyncWaitMs = 12000;
constexpr int kHttpKeepAliveIdleSeconds = 30;
constexpr int kHttpKeepAliveIntervalSeconds = 5;
constexpr int kHttpKeepAliveCount = 3;
constexpr int64_t kServerMonitorSlowThresholdMs = 950;
constexpr int64_t kServerMonitorVerySlowThresholdMs = 1000;
constexpr int64_t kServerMonitorFastThresholdMs = 500;
constexpr int64_t kServerMonitorAdaptiveSlowMs = 3000;
constexpr int64_t kServerMonitorAdaptiveFailMs = 10000;
constexpr int64_t kServerMonitorAdaptiveMaxMs = 30000;
constexpr int64_t kServerMonitorFastReconnectRetryMs = 250;
constexpr int kServerMonitorSlowScoreToDegrade = 5;
constexpr int kServerMonitorVerySlowStreakToDegrade = 3;
constexpr int64_t kCodexInitialDelayMs = 6000;
constexpr int64_t kCodexRetryDelayMs = 30000;
constexpr int64_t kWeatherInitialDelayMs = 3000;
constexpr int64_t kWeatherRetryDelayMs = 30000;
constexpr int64_t kNetworkWorkerIdleDelayMs = 50;
constexpr int64_t kServerMonitorQueueStaleMs = 2500;
constexpr int64_t kCodexQueueStaleMs = 45000;
constexpr int64_t kWeatherQueueStaleMs = 45000;
constexpr int64_t kDashboardRenderIntervalUs = 1000 * 1000;
constexpr int64_t kDashboardRenderSlowWarnMs = 250;
constexpr int64_t kDashboardRenderLateWarnMs = 250;
constexpr double kMiniChartBudgetLinePct =
    100.0 * static_cast<double>(style::kMiniChartBudgetLineNumerator) /
    static_cast<double>(style::kMiniChartBudgetLineDenominator);
constexpr char kOpenMeteoHttpsPrefix[] = "https://api.open-meteo.com/";
constexpr char kOpenMeteoHttpPrefix[] = "http://api.open-meteo.com/";
constexpr char kCodexUsageSummarySuffix[] = "usage-summary";
// Codex quota history is time-bounded: live API values may display normally,
// but cached or refresh-failure values must be marked stale and eventually cleared.
constexpr int64_t kCodexHistoricalMaxAgeSeconds = 15 * 60;
constexpr char kNvsNamespace[] = "dashboard";
constexpr char kNvsLastJsonKey[] = "last_json";
constexpr char kNvsLastJsonEpochKey[] = "last_json_epoch";
constexpr char kNvsLastWifiKey[] = "last_wifi";
constexpr uint8_t kShtc3Address = 0x70;
constexpr uint16_t kShtc3Wakeup = 0x3517;
constexpr uint16_t kShtc3Sleep = 0xB098;
constexpr uint16_t kShtc3SoftReset = 0x805D;
constexpr uint16_t kShtc3ReadId = 0xEFC8;
constexpr uint16_t kShtc3MeasureTempRhPolling = 0x7866;
constexpr int64_t kIndoorRefreshUs = 5LL * 1000LL * 1000LL;
constexpr int64_t kFetchLoopDelayMs = 1000;
constexpr int kLocalUtcOffsetMinutes = 8 * 60;
EventGroupHandle_t g_wifi_events = nullptr;
bool g_wifi_initialized = false;
bool g_transport_ready = false;
bool g_nvs_ready = false;
uint8_t g_active_wifi_profile = kWifiInvalidProfile;
SemaphoreHandle_t g_snapshot_mutex = nullptr;
SemaphoreHandle_t g_http_mutex = nullptr;
SemaphoreHandle_t g_network_queue_mutex = nullptr;
SemaphoreHandle_t g_diagnostic_mutex = nullptr;
DashboardSnapshot g_remote_snapshot = {};
bool g_remote_snapshot_valid = false;
DiagnosticState g_diagnostics = {};
char g_codex_response[kHttpResponseCapacity] = {};
char g_weather_response[kHttpResponseCapacity] = {};
char g_server_monitor_response[kHttpResponseCapacity] = {};
NetworkJobRequest g_network_requests[static_cast<size_t>(NetworkJob::kCount)] = {};
NetworkJobResult g_network_results[static_cast<size_t>(NetworkJob::kCount)] = {};
uint32_t g_network_sequence = 0;
esp_http_client_handle_t g_server_monitor_client = nullptr;
char g_server_monitor_client_url[256] = {};
size_t g_server_monitor_endpoint_cursor = 0;
uint8_t g_server_monitor_endpoint_wifi_profile = kWifiInvalidProfile;
adc_oneshot_unit_handle_t g_battery_adc = nullptr;
adc_cali_handle_t g_battery_adc_cali = nullptr;
bool g_battery_adc_ready = false;
bool g_clock_sync_started = false;
i2c_master_bus_handle_t g_i2c_bus = nullptr;
i2c_master_dev_handle_t g_shtc3_device = nullptr;
bool g_shtc3_ready = false;
bool g_indoor_valid = false;
float g_indoor_temp_c = 0.0f;
uint8_t g_indoor_humidity_pct = 0;
int64_t g_last_indoor_attempt_us = 0;

void InitNvs();

int64_t PollIntervalUs(int interval_ms) {
    return static_cast<int64_t>(interval_ms) * 1000LL;
}

bool CodexApiConfigured() {
    return std::strlen(CONFIG_DASHBOARD_API_URL) > 0 &&
           std::strlen(CONFIG_DASHBOARD_API_KEY) > 0;
}

bool CodexApiTouched() {
    return std::strlen(CONFIG_DASHBOARD_API_URL) > 0 ||
           std::strlen(CONFIG_DASHBOARD_API_KEY) > 0;
}

bool ServerMonitorConfigured() {
    return (std::strlen(CONFIG_DASHBOARD_SERVER_MONITOR_LOCAL_URL_1) > 0 ||
            std::strlen(CONFIG_DASHBOARD_SERVER_MONITOR_LOCAL_URL_2) > 0 ||
            std::strlen(CONFIG_DASHBOARD_SERVER_MONITOR_URL) > 0) &&
           std::strlen(CONFIG_DASHBOARD_SERVER_MONITOR_TOKEN) > 0;
}

size_t GetConfiguredWifiProfiles(WifiProfile *profiles, size_t capacity) {
    const WifiProfile candidates[kWifiProfileCount] = {
        {CONFIG_DASHBOARD_WIFI_SSID, CONFIG_DASHBOARD_WIFI_PASSWORD, 0},
        {CONFIG_DASHBOARD_WIFI_SSID_1, CONFIG_DASHBOARD_WIFI_PASSWORD_1, 1},
        {CONFIG_DASHBOARD_WIFI_SSID_2, CONFIG_DASHBOARD_WIFI_PASSWORD_2, 2},
    };
    size_t count = 0;
    for (const WifiProfile &candidate : candidates) {
        if (candidate.ssid[0] == '\0') {
            continue;
        }
        if (profiles && count < capacity) {
            profiles[count] = candidate;
        }
        ++count;
    }
    return count;
}

bool AnyWifiConfigured() {
    return GetConfiguredWifiProfiles(nullptr, 0) > 0;
}

bool AnyRemoteSourceConfigured() {
    return CodexApiConfigured() ||
           ServerMonitorConfigured() ||
           std::strlen(CONFIG_DASHBOARD_WEATHER_URL) > 0;
}

bool SameText(const char *left, const char *right) {
    if (!left || !right) {
        return false;
    }
    return std::strcmp(left, right) == 0;
}

bool AddServerMonitorEndpoint(ServerMonitorEndpoint *endpoints,
                              size_t *count,
                              size_t capacity,
                              const ServerMonitorEndpoint &endpoint) {
    if (!endpoints || !count || *count >= capacity || !endpoint.url || endpoint.url[0] == '\0') {
        return false;
    }
    for (size_t i = 0; i < *count; ++i) {
        if (SameText(endpoints[i].url, endpoint.url)) {
            return false;
        }
    }
    endpoints[*count] = endpoint;
    ++(*count);
    return true;
}

size_t BuildServerMonitorEndpointOrder(ServerMonitorEndpoint *endpoints, size_t capacity) {
    if (!endpoints || capacity == 0) {
        return 0;
    }
    const ServerMonitorEndpoint local_1 = {
        CONFIG_DASHBOARD_SERVER_MONITOR_LOCAL_URL_1,
        "local1",
        0,
    };
    const ServerMonitorEndpoint local_2 = {
        CONFIG_DASHBOARD_SERVER_MONITOR_LOCAL_URL_2,
        "local2",
        1,
    };
    const ServerMonitorEndpoint fallback = {
        CONFIG_DASHBOARD_SERVER_MONITOR_URL,
        "fallback",
        kWifiInvalidProfile,
    };
    size_t count = 0;
    if (g_active_wifi_profile == local_1.wifi_profile) {
        AddServerMonitorEndpoint(endpoints, &count, capacity, local_1);
    } else if (g_active_wifi_profile == local_2.wifi_profile) {
        AddServerMonitorEndpoint(endpoints, &count, capacity, local_2);
    }
    AddServerMonitorEndpoint(endpoints, &count, capacity, local_1);
    AddServerMonitorEndpoint(endpoints, &count, capacity, local_2);
    AddServerMonitorEndpoint(endpoints, &count, capacity, fallback);
    return count;
}

bool ServerMonitorEndpointIsPreferred(const ServerMonitorEndpoint &endpoint) {
    return endpoint.wifi_profile != kWifiInvalidProfile &&
           endpoint.wifi_profile == g_active_wifi_profile;
}

bool CurrentWifiHasPreferredServerMonitorEndpoint() {
    if (g_active_wifi_profile == 0) {
        return std::strlen(CONFIG_DASHBOARD_SERVER_MONITOR_LOCAL_URL_1) > 0;
    }
    if (g_active_wifi_profile == 1) {
        return std::strlen(CONFIG_DASHBOARD_SERVER_MONITOR_LOCAL_URL_2) > 0;
    }
    return false;
}

// ===== 文本与 JSON 规整 =====
// 桥接服务输出可以比屏幕字段更完整，这里统一裁剪、大小写和兜底值。
void CopyText(char *dest, size_t dest_size, const char *value) {
    if (dest_size == 0) {
        return;
    }
    std::snprintf(dest, dest_size, "%s", value ? value : "");
}

void CopyUpperText(char *dest, size_t dest_size, const char *value) {
    CopyText(dest, dest_size, value);
    for (size_t i = 0; dest[i] != '\0'; ++i) {
        dest[i] = static_cast<char>(std::toupper(static_cast<unsigned char>(dest[i])));
    }
}

const char *DiagnosticSourceForUrl(const char *url) {
    if (!url) {
        return "http";
    }
    if (std::strstr(url, "codex-quota")) {
        return "codex";
    }
    if (std::strstr(url, "server-monitor")) {
        return "server";
    }
    if (std::strstr(url, "open-meteo")) {
        return "weather";
    }
    return "http";
}

void DiagnosticLog(const char *source, const char *event, const char *detail, ...) {
    if (!g_diagnostic_mutex) {
        return;
    }
    if (xSemaphoreTake(g_diagnostic_mutex, pdMS_TO_TICKS(20)) != pdTRUE) {
        return;
    }
    DiagnosticState &state = g_diagnostics;
    const uint32_t sequence = ++state.next_sequence;
    DiagnosticEntry &entry = state.entries[(sequence - 1) % (sizeof(state.entries) / sizeof(state.entries[0]))];
    entry.sequence = sequence;
    entry.uptime_ms = esp_timer_get_time() / 1000LL;
    CopyText(entry.source, sizeof(entry.source), source ? source : "--");
    CopyText(entry.event, sizeof(entry.event), event ? event : "--");
    if (detail && detail[0] != '\0') {
        va_list args;
        va_start(args, detail);
        std::vsnprintf(entry.detail, sizeof(entry.detail), detail, args);
        va_end(args);
    } else {
        entry.detail[0] = '\0';
    }
    ++state.total_entries;
    xSemaphoreGive(g_diagnostic_mutex);
}

void DiagnosticRecordHttp(const char *url,
                          const char *result,
                          int status,
                          int length,
                          bool overflow,
                          int64_t elapsed_ms,
                          int socket_errno,
                          int tls_error,
                          int tls_flags) {
    if (!g_diagnostic_mutex) {
        return;
    }
    const char *source = DiagnosticSourceForUrl(url);
    const bool high_frequency_success = std::strcmp(source, "server") == 0 &&
                                        std::strcmp(result, "http_ok") == 0;
    if (!high_frequency_success &&
        xSemaphoreTake(g_diagnostic_mutex, pdMS_TO_TICKS(20)) == pdTRUE) {
        g_diagnostics.last_http_status = status;
        g_diagnostics.last_http_errno = socket_errno;
        g_diagnostics.last_http_tls_error = tls_error;
        g_diagnostics.last_http_tls_flags = tls_flags;
        g_diagnostics.last_http_elapsed_ms = elapsed_ms;
        g_diagnostics.last_http_len = length;
        g_diagnostics.last_http_overflow = overflow;
        CopyText(g_diagnostics.last_http_source, sizeof(g_diagnostics.last_http_source), source);
        CopyText(g_diagnostics.last_http_result, sizeof(g_diagnostics.last_http_result), result);
        xSemaphoreGive(g_diagnostic_mutex);
    }
    if (!high_frequency_success) {
        DiagnosticLog(source, result, "status=%d len=%d overflow=%d elapsed=%lldms errno=%d tls=0x%x flags=0x%x",
                      status, length, overflow ? 1 : 0, elapsed_ms, socket_errno, tls_error, tls_flags);
    }
}

void DiagnosticRecordCodex(const DashboardSnapshot &snapshot,
                           bool fetch_ok,
                           bool usage_summary_ok,
                           bool json_ok,
                           const char *error) {
    if (g_diagnostic_mutex &&
        xSemaphoreTake(g_diagnostic_mutex, pdMS_TO_TICKS(20)) == pdTRUE) {
        CopyText(g_diagnostics.last_codex_state, sizeof(g_diagnostics.last_codex_state), snapshot.codex_state);
        CopyText(g_diagnostics.last_codex_link_state, sizeof(g_diagnostics.last_codex_link_state), snapshot.link_state);
        g_diagnostics.last_codex_week_remaining_pct = snapshot.codex_week_remaining_pct;
        g_diagnostics.last_codex_five_hour_remaining_pct = snapshot.codex_five_hour_remaining_pct;
        g_diagnostics.last_codex_received_epoch = snapshot.codex_received_epoch;
        g_diagnostics.last_codex_fetch_ok = fetch_ok;
        g_diagnostics.last_codex_usage_summary_ok = usage_summary_ok;
        g_diagnostics.last_codex_json_ok = json_ok;
        CopyText(g_diagnostics.last_codex_error, sizeof(g_diagnostics.last_codex_error), error ? error : "");
        xSemaphoreGive(g_diagnostic_mutex);
    }
    DiagnosticLog("codex", fetch_ok ? "fetch_ok" : "fetch_fail",
                  "json=%d usage_summary=%d state=%s week=%u five_hour=%u err=%s",
                  json_ok ? 1 : 0,
                  usage_summary_ok ? 1 : 0,
                  snapshot.codex_state,
                  snapshot.codex_week_remaining_pct,
                  snapshot.codex_five_hour_remaining_pct,
                  error ? error : "");
}

void DiagnosticRecordWorker(NetworkJob job, bool ok, bool usage_summary_ok, int64_t elapsed_ms) {
    const bool high_frequency_success = job == NetworkJob::kServerMonitor && ok;
    if (!high_frequency_success &&
        g_diagnostic_mutex &&
        xSemaphoreTake(g_diagnostic_mutex, pdMS_TO_TICKS(20)) == pdTRUE) {
        CopyText(g_diagnostics.last_worker_job, sizeof(g_diagnostics.last_worker_job), NetworkJobName(job));
        g_diagnostics.last_worker_ok = ok;
        g_diagnostics.last_worker_usage_summary_ok = usage_summary_ok;
        g_diagnostics.last_worker_elapsed_ms = elapsed_ms;
        xSemaphoreGive(g_diagnostic_mutex);
    }
    if (!high_frequency_success) {
        DiagnosticLog(NetworkJobName(job), "worker_finish", "ok=%d usage_summary=%d elapsed=%lldms",
                      ok ? 1 : 0, usage_summary_ok ? 1 : 0, elapsed_ms);
    }
}

void CopyCodexSource(char *dest, size_t dest_size, const char *value) {
    if (!value || value[0] == '\0') {
        CopyText(dest, dest_size, "CONFIG");
        return;
    }
    if (std::strcmp(value, "codex_cli_oauth") == 0) {
        CopyText(dest, dest_size, "OAUTH");
        return;
    }
    if (std::strcmp(value, "status_file") == 0) {
        CopyText(dest, dest_size, "FILE");
        return;
    }
    if (std::strcmp(value, "quota_api") == 0) {
        CopyText(dest, dest_size, "API");
        return;
    }
    CopyUpperText(dest, dest_size, value);
}

uint8_t ClampPercent(int value) {
    if (value < 0) {
        return 0;
    }
    if (value > 100) {
        return 100;
    }
    return static_cast<uint8_t>(value);
}

int JsonInt(const cJSON *object, const char *name, int fallback) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, name);
    if (cJSON_IsNumber(item)) {
        return item->valueint;
    }
    return fallback;
}

float JsonFloat(const cJSON *object, const char *name, float fallback) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, name);
    if (cJSON_IsNumber(item)) {
        return static_cast<float>(item->valuedouble);
    }
    return fallback;
}

double JsonNumber(const cJSON *object, const char *name, double fallback) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, name);
    if (cJSON_IsNumber(item)) {
        return item->valuedouble;
    }
    return fallback;
}

const char *JsonString(const cJSON *object, const char *name, const char *fallback) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, name);
    if (cJSON_IsString(item) && item->valuestring) {
        return item->valuestring;
    }
    return fallback;
}

const cJSON *JsonObjectItem(const cJSON *object, const char *name) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, name);
    return cJSON_IsObject(item) ? item : nullptr;
}

const cJSON *JsonArrayItem(const cJSON *object, const char *name) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, name);
    return cJSON_IsArray(item) ? item : nullptr;
}

double JsonNumberAny(const cJSON *object, const char *const *names, int name_count, double fallback) {
    if (!object) {
        return fallback;
    }
    for (int i = 0; i < name_count; ++i) {
        const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, names[i]);
        if (cJSON_IsNumber(item)) {
            return item->valuedouble;
        }
    }
    return fallback;
}

const char *JsonStringAny(const cJSON *object, const char *const *names, int name_count, const char *fallback) {
    if (!object) {
        return fallback;
    }
    for (int i = 0; i < name_count; ++i) {
        const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, names[i]);
        if (cJSON_IsString(item) && item->valuestring) {
            return item->valuestring;
        }
    }
    return fallback;
}

void ParsePercentArray(const cJSON *object, const char *name, uint8_t *values, int max_count) {
    const cJSON *array = cJSON_GetObjectItemCaseSensitive(object, name);
    if (!cJSON_IsArray(array)) {
        return;
    }
    const int count = cJSON_GetArraySize(array);
    for (int i = 0; i < max_count; ++i) {
        values[i] = 0;
    }
    for (int i = 0; i < max_count && i < count; ++i) {
        const cJSON *item = cJSON_GetArrayItem(array, i);
        if (cJSON_IsNumber(item)) {
            values[i] = ClampPercent(item->valueint);
        }
    }
}

void FormatApiReset(char *dest, size_t dest_size, const char *prefix, double resets_at) {
    if (!dest || dest_size == 0) {
        return;
    }
    if (resets_at <= 0) {
        std::snprintf(dest, dest_size, "%s UNKNOWN", prefix);
        return;
    }
    const time_t reset_time = static_cast<time_t>(resets_at);
    struct tm local = {};
    localtime_r(&reset_time, &local);
    if (std::strcmp(prefix, "WEEK") == 0) {
        static constexpr const char *kWeekdays[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
        std::snprintf(dest, dest_size, "WEEK %s %02d:%02d", kWeekdays[local.tm_wday], local.tm_hour, local.tm_min);
        return;
    }

    time_t now_time = time(nullptr);
    struct tm now_local = {};
    localtime_r(&now_time, &now_local);
    const int today = now_local.tm_yday + now_local.tm_year * 370;
    const int reset_day = local.tm_yday + local.tm_year * 370;
    const char *day = nullptr;
    char date_text[16] = {};
    if (reset_day == today) {
        day = "TODAY";
    } else if (reset_day == today + 1) {
        day = "TMRW";
    } else {
        const int month = (local.tm_mon >= 0 && local.tm_mon < 12) ? local.tm_mon + 1 : 0;
        const int month_day = (local.tm_mday >= 1 && local.tm_mday <= 31) ? local.tm_mday : 0;
        std::snprintf(date_text, sizeof(date_text), "%02d-%02d", month, month_day);
        day = date_text;
    }
    std::snprintf(dest, dest_size, "%s %s %02d:%02d", prefix, day, local.tm_hour, local.tm_min);
}

void CopyApiTimestamp(char *dest, size_t dest_size, const char *value) {
    if (!value || value[0] == '\0') {
        return;
    }
    CopyText(dest, dest_size, value);
}

void CopyLocalUpdatedTimestamp(char *dest, size_t dest_size) {
    if (!dest || dest_size == 0) {
        return;
    }
    const time_t now = time(nullptr);
    if (now < 1700000000) {
        return;
    }
    struct tm local = {};
    localtime_r(&now, &local);
    std::snprintf(dest, dest_size, "%02d:%02d:%02d",
                  local.tm_hour, local.tm_min, local.tm_sec);
}

int64_t CurrentEpochSeconds() {
    const time_t now = time(nullptr);
    if (now < 1700000000) {
        return 0;
    }
    return static_cast<int64_t>(now);
}

void BuildCodexApiSiblingUrl(char *dest, size_t dest_size, const char *suffix) {
    CopyText(dest, dest_size, CONFIG_DASHBOARD_API_URL);
    char *latest = std::strstr(dest, "/latest");
    if (latest) {
        latest[1] = '\0';
    } else {
        const size_t len = std::strlen(dest);
        if (len > 0 && dest[len - 1] != '/' && len + 1 < dest_size) {
            dest[len] = '/';
            dest[len + 1] = '\0';
        }
    }
    std::strncat(dest, suffix, dest_size - std::strlen(dest) - 1);
}

const char *WeatherCodeText(int code) {
    if (code == 0) {
        return "Clear";
    }
    if (code >= 1 && code <= 3) {
        return "Cloudy";
    }
    if (code == 45 || code == 48) {
        return "Fog";
    }
    if ((code >= 51 && code <= 57)) {
        return "Drizzle";
    }
    if ((code >= 61 && code <= 67) || (code >= 80 && code <= 82)) {
        return "Rain";
    }
    if ((code >= 71 && code <= 77) || code == 85 || code == 86) {
        return "Snow";
    }
    if (code == 95 || code == 96 || code == 99) {
        return "Storm";
    }
    return "Unknown";
}

uint8_t GpuMemoryPercent(const cJSON *gpu) {
    static constexpr const char *kMemoryPctKeys[] = {"memory_percent", "gpu_memory_percent", "gpu_mem_percent",
                                                     "vram_percent"};
    const double explicit_pct = JsonNumberAny(gpu, kMemoryPctKeys, 4, -1.0);
    if (explicit_pct >= 0.0) {
        return ClampPercent(static_cast<int>(explicit_pct + 0.5));
    }
    static constexpr const char *kUsedKeys[] = {"memory_used_mib", "memory_used_mb", "used_memory_mib"};
    static constexpr const char *kTotalKeys[] = {"memory_total_mib", "memory_total_mb", "total_memory_mib"};
    const double used = JsonNumberAny(gpu, kUsedKeys, 3, 0.0);
    const double total = JsonNumberAny(gpu, kTotalKeys, 3, 0.0);
    if (total <= 0.0) {
        return 0;
    }
    return ClampPercent(static_cast<int>((used * 100.0 / total) + 0.5));
}

uint8_t CountGpuProcesses(int gpu_index, const cJSON *busy_processes) {
    if (!cJSON_IsArray(busy_processes)) {
        return 0;
    }
    static constexpr const char *kIndexKeys[] = {"gpu_index", "gpu", "device_index", "index"};
    int count = 0;
    const int process_count = cJSON_GetArraySize(busy_processes);
    for (int i = 0; i < process_count; ++i) {
        const cJSON *process = cJSON_GetArrayItem(busy_processes, i);
        if (!cJSON_IsObject(process)) {
            continue;
        }
        const int index = static_cast<int>(JsonNumberAny(process, kIndexKeys, 4, -1.0));
        if (index == gpu_index) {
            ++count;
        }
    }
    return static_cast<uint8_t>(count > 255 ? 255 : count);
}

void ResetGpuCards(DashboardSnapshot *snapshot) {
    snapshot->gpu_card_count = 0;
    for (int i = 0; i < 2; ++i) {
        std::snprintf(snapshot->gpu_cards[i].label, sizeof(snapshot->gpu_cards[i].label), "GPU%u", i);
        snapshot->gpu_cards[i].util_pct = 0;
        snapshot->gpu_cards[i].mem_pct = 0;
        snapshot->gpu_cards[i].temp_c = 0;
        snapshot->gpu_cards[i].power_w = 0;
        snapshot->gpu_cards[i].processes = 0;
        snapshot->gpu_cards[i].valid = false;
    }
}

void ParseGpuCards(const cJSON *server, DashboardSnapshot *snapshot) {
    const cJSON *cards = cJSON_GetObjectItemCaseSensitive(server, "gpu_cards");
    if (!cJSON_IsArray(cards)) {
        return;
    }

    ResetGpuCards(snapshot);
    const int count = cJSON_GetArraySize(cards);
    for (int i = 0; i < count && snapshot->gpu_card_count < 2; ++i) {
        const cJSON *card_json = cJSON_GetArrayItem(cards, i);
        if (!cJSON_IsObject(card_json)) {
            continue;
        }
        GpuCardSnapshot *card = &snapshot->gpu_cards[snapshot->gpu_card_count];
        CopyUpperText(card->label, sizeof(card->label), JsonString(card_json, "label", card->label));
        card->util_pct = ClampPercent(JsonInt(card_json, "util_percent", card->util_pct));
        card->mem_pct = ClampPercent(JsonInt(card_json, "memory_percent", card->mem_pct));
        card->temp_c = static_cast<uint8_t>(JsonInt(card_json, "temp_c", card->temp_c));
        card->power_w = static_cast<uint16_t>(JsonInt(card_json, "power_w", card->power_w));
        card->processes = static_cast<uint8_t>(JsonInt(card_json, "processes", card->processes));
        card->valid = true;
        ++snapshot->gpu_card_count;
    }
}

void ParseServerMonitorGpuList(const cJSON *data, DashboardSnapshot *snapshot) {
    const cJSON *gpus = JsonArrayItem(data, "gpus");
    if (!gpus) {
        return;
    }

    const cJSON *busy_processes = JsonArrayItem(data, "busy_processes");
    ResetGpuCards(snapshot);
    int aggregate_util = 0;
    int aggregate_mem = 0;
    int aggregate_temp = 0;
    const int count = cJSON_GetArraySize(gpus);
    for (int i = 0; i < count && snapshot->gpu_card_count < 2; ++i) {
        const cJSON *gpu = cJSON_GetArrayItem(gpus, i);
        if (!cJSON_IsObject(gpu)) {
            continue;
        }
        const int index = JsonInt(gpu, "index", snapshot->gpu_card_count);
        GpuCardSnapshot *card = &snapshot->gpu_cards[snapshot->gpu_card_count];
        std::snprintf(card->label, sizeof(card->label), "GPU%d", index);
        static constexpr const char *kUtilKeys[] = {"utilization_percent", "util_percent", "gpu_util_percent",
                                                    "gpu_percent"};
        static constexpr const char *kTempKeys[] = {"temperature_c", "temp_c", "gpu_temp_c", "gpu_temperature_c"};
        static constexpr const char *kPowerKeys[] = {"power_w", "power_draw_w", "power_draw", "power"};
        card->util_pct = ClampPercent(static_cast<int>(JsonNumberAny(gpu, kUtilKeys, 4, 0.0) + 0.5));
        card->mem_pct = GpuMemoryPercent(gpu);
        card->temp_c = static_cast<uint8_t>(JsonNumberAny(gpu, kTempKeys, 4, 0.0));
        card->power_w = static_cast<uint16_t>(JsonNumberAny(gpu, kPowerKeys, 4, 0.0) + 0.5);
        card->processes = CountGpuProcesses(index, busy_processes);
        card->valid = true;
        if (card->util_pct > aggregate_util) {
            aggregate_util = card->util_pct;
        }
        if (card->mem_pct > aggregate_mem) {
            aggregate_mem = card->mem_pct;
        }
        if (card->temp_c > aggregate_temp) {
            aggregate_temp = card->temp_c;
        }
        ++snapshot->gpu_card_count;
    }
    if (snapshot->gpu_card_count > 0) {
        snapshot->gpu_util_pct = static_cast<uint8_t>(aggregate_util);
        snapshot->gpu_mem_pct = static_cast<uint8_t>(aggregate_mem);
        snapshot->gpu_temp_c = static_cast<uint8_t>(aggregate_temp);
    }
}

bool ParseOpenMeteoJson(const char *payload, DashboardSnapshot *snapshot) {
    cJSON *root = cJSON_Parse(payload);
    if (!root) {
        return false;
    }
    const cJSON *current = JsonObjectItem(root, "current");
    if (!current) {
        cJSON_Delete(root);
        return false;
    }
    CopyText(snapshot->weather_location, sizeof(snapshot->weather_location), CONFIG_DASHBOARD_WEATHER_LOCATION);
    CopyText(snapshot->weather_summary, sizeof(snapshot->weather_summary),
             WeatherCodeText(JsonInt(current, "weather_code", -1)));
    snapshot->weather_temp_c = JsonFloat(current, "temperature_2m", snapshot->weather_temp_c);
    snapshot->weather_humidity_pct = ClampPercent(JsonInt(current, "relative_humidity_2m",
                                                          snapshot->weather_humidity_pct));
    snapshot->weather_wind_kmh = JsonFloat(current, "wind_speed_10m", snapshot->weather_wind_kmh);
    CopyApiTimestamp(snapshot->updated_at, sizeof(snapshot->updated_at),
                     JsonString(current, "time", snapshot->updated_at));
    cJSON_Delete(root);
    return true;
}

const cJSON *ServerMonitorDataObject(const cJSON *root) {
    const cJSON *target = JsonObjectItem(root, "target");
    if (target) {
        return target;
    }
    const cJSON *status = JsonObjectItem(root, "status");
    if (status) {
        return status;
    }
    const cJSON *data = JsonObjectItem(root, "data");
    if (data) {
        const cJSON *data_target = JsonObjectItem(data, "target");
        if (data_target) {
            return data_target;
        }
        const cJSON *data_status = JsonObjectItem(data, "status");
        if (data_status) {
            return data_status;
        }
        return data;
    }
    return root;
}

bool ParseServerMonitorJson(const char *payload, DashboardSnapshot *snapshot) {
    cJSON *root = cJSON_Parse(payload);
    if (!root) {
        return false;
    }
    const cJSON *data = ServerMonitorDataObject(root);
    if (!cJSON_IsObject(data)) {
        cJSON_Delete(root);
        return false;
    }

    CopyUpperText(snapshot->server_target, sizeof(snapshot->server_target),
                  CONFIG_DASHBOARD_SERVER_MONITOR_DISPLAY_TARGET);
    CopyUpperText(snapshot->server_role, sizeof(snapshot->server_role), CONFIG_DASHBOARD_SERVER_MONITOR_ROLE);

    static constexpr const char *kStateKeys[] = {"state", "status", "health"};
    const char *state = JsonStringAny(data, kStateKeys, 3, snapshot->server_state);
    const cJSON *ssh_available = cJSON_GetObjectItemCaseSensitive(data, "ssh_available");
    if (cJSON_IsBool(ssh_available)) {
        state = cJSON_IsTrue(ssh_available) ? "online" : "offline";
    }
    const bool has_error = cJSON_GetObjectItemCaseSensitive(data, "error") != nullptr;

    const cJSON *system = JsonObjectItem(data, "system");
    static constexpr const char *kCpuKeys[] = {"cpu_percent", "cpu", "cpu_usage"};
    static constexpr const char *kMemKeys[] = {"memory_percent", "mem_percent", "memory"};
    snapshot->server_cpu_pct = ClampPercent(static_cast<int>(
        JsonNumberAny(data, kCpuKeys, 3, JsonNumberAny(system, kCpuKeys, 3, snapshot->server_cpu_pct)) + 0.5));
    snapshot->server_mem_pct = ClampPercent(static_cast<int>(
        JsonNumberAny(data, kMemKeys, 3, JsonNumberAny(system, kMemKeys, 3, snapshot->server_mem_pct)) + 0.5));
    static constexpr const char *kLatencyKeys[] = {"latency_ms", "ping_ms", "response_ms"};
    snapshot->server_latency_ms = static_cast<uint16_t>(JsonNumberAny(data, kLatencyKeys, 3,
                                                                      snapshot->server_latency_ms) + 0.5);

    static constexpr const char *kGpuUtilKeys[] = {"gpu_util_percent", "gpu_percent", "gpu_utilization", "gpu"};
    static constexpr const char *kGpuMemKeys[] = {"gpu_memory_percent", "gpu_mem_percent", "vram_percent"};
    static constexpr const char *kGpuTempKeys[] = {"gpu_temp_c", "gpu_temperature_c", "gpu_temp"};
    static constexpr const char *kGpuProcKeys[] = {"gpu_processes", "gpu_process_count", "process_count"};
    snapshot->gpu_util_pct = ClampPercent(static_cast<int>(JsonNumberAny(data, kGpuUtilKeys, 4,
                                                                         snapshot->gpu_util_pct) + 0.5));
    snapshot->gpu_mem_pct = ClampPercent(static_cast<int>(JsonNumberAny(data, kGpuMemKeys, 3,
                                                                        snapshot->gpu_mem_pct) + 0.5));
    snapshot->gpu_temp_c = static_cast<uint8_t>(JsonNumberAny(data, kGpuTempKeys, 3, snapshot->gpu_temp_c));
    snapshot->gpu_processes = static_cast<uint8_t>(JsonNumberAny(data, kGpuProcKeys, 3, snapshot->gpu_processes));

    const cJSON *busy_processes = JsonArrayItem(data, "busy_processes");
    if (busy_processes) {
        snapshot->gpu_processes = static_cast<uint8_t>(cJSON_GetArraySize(busy_processes));
    }
    ParseServerMonitorGpuList(data, snapshot);
    const bool has_live_telemetry = snapshot->gpu_card_count > 0 ||
                                    snapshot->gpu_util_pct > 0 ||
                                    snapshot->gpu_mem_pct > 0 ||
                                    snapshot->gpu_temp_c > 0 ||
                                    snapshot->gpu_processes > 0 ||
                                    snapshot->server_cpu_pct > 0 ||
                                    snapshot->server_mem_pct > 0;
    if (has_error && !has_live_telemetry) {
        state = "error";
    } else if (has_live_telemetry &&
               (!state || state[0] == '\0' || std::strcmp(state, "error") == 0)) {
        state = "online";
    }
    CopyUpperText(snapshot->server_state, sizeof(snapshot->server_state), state);

    const char *summary = JsonString(data, "summary", "");
    if (summary[0] != '\0') {
        CopyText(snapshot->server_summary, sizeof(snapshot->server_summary), summary);
    } else {
        std::snprintf(snapshot->server_summary, sizeof(snapshot->server_summary), "%u GPU procs active",
                      snapshot->gpu_processes);
    }
    static constexpr const char *kUpdatedKeys[] = {"updated_at", "updated_at_raw", "collected_at", "time"};
    const char *updated_at = JsonStringAny(data, kUpdatedKeys, 4, "");
    if (updated_at[0] == '\0') {
        updated_at = JsonStringAny(root, kUpdatedKeys, 4, snapshot->updated_at);
    }
    CopyApiTimestamp(snapshot->updated_at, sizeof(snapshot->updated_at), updated_at);
    cJSON_Delete(root);
    return true;
}

const char *LastToken(const char *value) {
    if (!value) {
        return "";
    }
    const char *token = std::strrchr(value, ' ');
    return token ? token + 1 : value;
}

const char *ResetTimeDetail(const char *value) {
    if (!value) {
        return "";
    }
    while (*value == ' ') {
        ++value;
    }
    if (std::strncmp(value, "WEEK ", 5) == 0) {
        return value + 5;
    }
    if (std::strncmp(value, "W ", 2) == 0) {
        return value + 2;
    }
    if (std::strncmp(value, "5H ", 3) == 0) {
        return value + 3;
    }
    if (std::strncmp(value, "5-HOUR ", 7) == 0) {
        return value + 7;
    }
    return value;
}

const char *DisplayWord(const char *value) {
    if (!value) {
        return "";
    }
    if (std::strcmp(value, "GOOD") == 0) {
        return "Good";
    }
    if (std::strcmp(value, "LOW") == 0) {
        return "Low";
    }
    if (std::strcmp(value, "LIMIT") == 0) {
        return "Limit";
    }
    if (std::strcmp(value, "STALE") == 0) {
        return "Stale";
    }
    if (std::strcmp(value, "AUTH") == 0) {
        return "Auth";
    }
    if (std::strcmp(value, "NO_DATA") == 0) {
        return "No";
    }
    if (std::strcmp(value, "OAUTH") == 0) {
        return "OAuth";
    }
    if (std::strcmp(value, "CONFIG") == 0) {
        return "Config";
    }
    if (std::strcmp(value, "ONLINE") == 0) {
        return "Online";
    }
    if (std::strcmp(value, "OFFLINE") == 0) {
        return "Offline";
    }
    if (std::strcmp(value, "ERROR") == 0) {
        return "Error";
    }
    if (std::strcmp(value, "MON") == 0) {
        return "Mon";
    }
    if (std::strcmp(value, "TUE") == 0) {
        return "Tue";
    }
    if (std::strcmp(value, "WED") == 0) {
        return "Wed";
    }
    if (std::strcmp(value, "THU") == 0) {
        return "Thu";
    }
    if (std::strcmp(value, "FRI") == 0) {
        return "Fri";
    }
    if (std::strcmp(value, "SAT") == 0) {
        return "Sat";
    }
    if (std::strcmp(value, "SUN") == 0) {
        return "Sun";
    }
    return value;
}

void StartClockSync() {
    if (g_clock_sync_started) {
        return;
    }
    setenv("TZ", "CST-8", 1);
    tzset();
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "ntp.aliyun.com");
    esp_sntp_setservername(1, "pool.ntp.org");
    esp_sntp_init();
    g_clock_sync_started = true;
}

bool IsHttpsUrl(const char *url) {
    return url && std::strncmp(url, "https://", 8) == 0;
}

bool SystemTimeReadyForTls() {
    const time_t now = time(nullptr);
    return now >= 1609459200;  // 2021-01-01; enough to avoid 1970 TLS validation.
}

bool WaitForSystemTimeIfHttps(const char *url) {
    if (!IsHttpsUrl(url) || SystemTimeReadyForTls()) {
        return true;
    }
    const int64_t deadline_us = esp_timer_get_time() + PollIntervalUs(kHttpsTimeSyncWaitMs);
    while (esp_timer_get_time() < deadline_us) {
        if (SystemTimeReadyForTls()) {
            return true;
        }
        vTaskDelay(pdMS_TO_TICKS(250));
    }
    ESP_LOGW(kTag, "HTTPS fetch before clock sync: url=%s", url ? url : "");
    DiagnosticLog(DiagnosticSourceForUrl(url), "clock_wait_timeout", "https request blocked before time sync");
    return false;
}

// ===== 板端本地数据 =====
// 时间、电池、SHTC3 都在板子本地生成；不要从桥接服务覆盖室内传感器。
bool FormatIsoTimestampLocal(char *dest, size_t dest_size, const char *value) {
    if (!value || std::strlen(value) < 16 || value[10] != 'T') {
        return false;
    }

    int year = 0;
    int month = 0;
    int day = 0;
    int hour = 0;
    int minute = 0;
    int second = 0;
    const int parsed = std::sscanf(value, "%4d-%2d-%2dT%2d:%2d:%2d",
                                   &year, &month, &day, &hour, &minute, &second);
    if (parsed < 5 || year < 2020 || month < 1 || month > 12 || day < 1 || day > 31 ||
        hour < 0 || hour > 23 || minute < 0 || minute > 59) {
        return false;
    }
    if (parsed < 6) {
        second = 0;
    }

    int source_offset_minutes = kLocalUtcOffsetMinutes;
    const char *cursor = value + 16;
    while (*cursor != '\0' && *cursor != 'Z' && *cursor != '+' && *cursor != '-') {
        ++cursor;
    }
    if (*cursor == 'Z') {
        source_offset_minutes = 0;
    } else if (*cursor == '+' || *cursor == '-') {
        int offset_hour = 0;
        int offset_minute = 0;
        if (std::sscanf(cursor + 1, "%2d:%2d", &offset_hour, &offset_minute) == 2 &&
            offset_hour >= 0 && offset_hour <= 23 && offset_minute >= 0 && offset_minute <= 59) {
            source_offset_minutes = offset_hour * 60 + offset_minute;
            if (*cursor == '-') {
                source_offset_minutes = -source_offset_minutes;
            }
        }
    }

    std::tm parsed_time = {};
    parsed_time.tm_year = year - 1900;
    parsed_time.tm_mon = month - 1;
    parsed_time.tm_mday = day;
    parsed_time.tm_hour = hour;
    parsed_time.tm_min = minute;
    parsed_time.tm_sec = second;
    const time_t local_interpreted = mktime(&parsed_time);
    if (local_interpreted < 0) {
        return false;
    }

    const time_t local_epoch =
        local_interpreted + static_cast<time_t>((kLocalUtcOffsetMinutes - source_offset_minutes) * 60);
    std::tm local = {};
    localtime_r(&local_epoch, &local);
    std::snprintf(dest, dest_size, "Updated %02d:%02d:%02d", local.tm_hour, local.tm_min, local.tm_sec);
    return true;
}

void BuildUpdatedText(char *dest, size_t dest_size, const char *updated_at) {
    if (!updated_at || updated_at[0] == '\0') {
        CopyText(dest, dest_size, "Updated --");
        return;
    }
    if (FormatIsoTimestampLocal(dest, dest_size, updated_at)) {
        return;
    }
    const char *time_part = std::strchr(updated_at, 'T');
    if (time_part && std::strlen(time_part) >= 6) {
        if (std::strlen(time_part) >= 9) {
            std::snprintf(dest, dest_size, "Updated %.8s", time_part + 1);
        } else {
            std::snprintf(dest, dest_size, "Updated %.5s:00", time_part + 1);
        }
        return;
    }
    if (std::strlen(updated_at) == 5 && updated_at[2] == ':') {
        std::snprintf(dest, dest_size, "Updated %s:00", updated_at);
        return;
    }
    std::snprintf(dest, dest_size, "Updated %s", updated_at);
}

uint8_t BatteryPercent(uint16_t millivolts) {
    if (millivolts <= 3000) {
        return 0;
    }
    if (millivolts >= 4120) {
        return 100;
    }
    return static_cast<uint8_t>(((millivolts - 3000) * 100) / 1120);
}

uint16_t ReadBatteryMillivolts() {
    if (!g_battery_adc_ready || !g_battery_adc) {
        return 0;
    }
    int raw_sum = 0;
    int samples = 0;
    for (int i = 0; i < 4; ++i) {
        int raw = 0;
        if (adc_oneshot_read(g_battery_adc, ADC_CHANNEL_3, &raw) == ESP_OK) {
            raw_sum += raw;
            ++samples;
        }
    }
    if (samples == 0) {
        return 0;
    }
    const uint32_t raw_average = static_cast<uint32_t>(raw_sum / samples);
    uint32_t pin_mv = (raw_average * 3300U) / 4095U;
    if (g_battery_adc_cali) {
        int calibrated_mv = 0;
        if (adc_cali_raw_to_voltage(g_battery_adc_cali, static_cast<int>(raw_average),
                                    &calibrated_mv) == ESP_OK && calibrated_mv > 0) {
            pin_mv = static_cast<uint32_t>(calibrated_mv);
        }
    }
    return static_cast<uint16_t>(pin_mv * 3U);
}

void BuildBatteryText(char *dest, size_t dest_size) {
    const uint16_t millivolts = ReadBatteryMillivolts();
    if (millivolts == 0) {
        CopyText(dest, dest_size, "Battery --");
        return;
    }
    std::snprintf(dest, dest_size, "Battery %u %%", BatteryPercent(millivolts));
}

bool Shtc3CheckCrc(const uint8_t *data, uint8_t length, uint8_t expected) {
    uint8_t crc = 0xFF;
    for (uint8_t byte_index = 0; byte_index < length; ++byte_index) {
        crc ^= data[byte_index];
        for (uint8_t bit = 0; bit < 8; ++bit) {
            crc = (crc & 0x80) ? static_cast<uint8_t>((crc << 1) ^ 0x31) : static_cast<uint8_t>(crc << 1);
        }
    }
    return crc == expected;
}

esp_err_t Shtc3WriteCommand(uint16_t command) {
    const uint8_t buffer[2] = {
        static_cast<uint8_t>(command >> 8),
        static_cast<uint8_t>(command & 0xFF),
    };
    return i2c_master_transmit(g_shtc3_device, buffer, sizeof(buffer), pdMS_TO_TICKS(100));
}

bool ReadShtc3(float *temperature_c, uint8_t *humidity_pct) {
    if (!g_shtc3_ready || !g_shtc3_device) {
        return false;
    }
    if (Shtc3WriteCommand(kShtc3Wakeup) != ESP_OK) {
        return false;
    }
    vTaskDelay(pdMS_TO_TICKS(2));
    if (Shtc3WriteCommand(kShtc3MeasureTempRhPolling) != ESP_OK) {
        Shtc3WriteCommand(kShtc3Sleep);
        return false;
    }
    vTaskDelay(pdMS_TO_TICKS(20));

    uint8_t data[6] = {};
    const esp_err_t read_result = i2c_master_receive(g_shtc3_device, data, sizeof(data), pdMS_TO_TICKS(100));
    Shtc3WriteCommand(kShtc3Sleep);
    if (read_result != ESP_OK) {
        return false;
    }
    if (!Shtc3CheckCrc(data, 2, data[2]) || !Shtc3CheckCrc(&data[3], 2, data[5])) {
        return false;
    }

    const uint16_t raw_temp = static_cast<uint16_t>((data[0] << 8) | data[1]);
    const uint16_t raw_humidity = static_cast<uint16_t>((data[3] << 8) | data[4]);
    *temperature_c = 175.0f * static_cast<float>(raw_temp) / 65536.0f - 45.0f - 4.0f;
    const float humidity = 100.0f * static_cast<float>(raw_humidity) / 65536.0f;
    *humidity_pct = ClampPercent(static_cast<int>(humidity + 0.5f));
    return true;
}

// 传感器读数做 5 秒缓存，避免每秒刷新 UI 时频繁唤醒 SHTC3。
void UpdateIndoorSnapshot(DashboardSnapshot *snapshot) {
    const int64_t now_us = esp_timer_get_time();
    if (g_shtc3_ready && (g_last_indoor_attempt_us == 0 || now_us - g_last_indoor_attempt_us >= kIndoorRefreshUs)) {
        g_last_indoor_attempt_us = now_us;
        float temperature = 0.0f;
        uint8_t humidity = 0;
        if (ReadShtc3(&temperature, &humidity)) {
            g_indoor_temp_c = temperature;
            g_indoor_humidity_pct = humidity;
            g_indoor_valid = true;
        }
    }

    snapshot->indoor_valid = g_indoor_valid;
    if (g_indoor_valid) {
        snapshot->indoor_temp_c = g_indoor_temp_c;
        snapshot->indoor_humidity_pct = g_indoor_humidity_pct;
    }
}

// 空状态只表达“尚未拿到真实数据”。硬件固件不得用模拟/伪造业务数据兜底；
// 真实远端数据来自 ParseDashboardJson()，本地温湿度会在 ApplySnapshot() 前覆盖进去。
void BuildEmptySnapshot(DashboardSnapshot *snapshot) {
    snapshot->hour = 0;
    snapshot->minute = 0;
    snapshot->second = 0;
    CopyText(snapshot->date, sizeof(snapshot->date), "--");
    CopyText(snapshot->weather_location, sizeof(snapshot->weather_location), "--");
    CopyText(snapshot->weather_summary, sizeof(snapshot->weather_summary), "--");
    snapshot->weather_temp_c = 0.0f;
    snapshot->weather_humidity_pct = 0;
    snapshot->weather_wind_kmh = 0.0f;
    snapshot->indoor_temp_c = 0.0f;
    snapshot->indoor_humidity_pct = 0;
    snapshot->indoor_valid = false;
    CopyText(snapshot->server_target, sizeof(snapshot->server_target), "--");
    CopyText(snapshot->server_role, sizeof(snapshot->server_role), "--");
    CopyText(snapshot->server_state, sizeof(snapshot->server_state), "NO_DATA");
    CopyText(snapshot->server_summary, sizeof(snapshot->server_summary), "NO DATA");
    snapshot->server_cpu_pct = 0;
    snapshot->server_mem_pct = 0;
    snapshot->server_latency_ms = 0;
    snapshot->gpu_util_pct = 0;
    snapshot->gpu_mem_pct = 0;
    snapshot->gpu_temp_c = 0;
    snapshot->gpu_processes = 0;
    ResetGpuCards(snapshot);
    CopyText(snapshot->codex_state, sizeof(snapshot->codex_state), "NO_DATA");
    CopyText(snapshot->codex_source, sizeof(snapshot->codex_source), "--");
    snapshot->codex_week_remaining_pct = 0;
    snapshot->codex_five_hour_remaining_pct = 0;
    CopyText(snapshot->codex_burn_rate, sizeof(snapshot->codex_burn_rate), "--");
    CopyText(snapshot->codex_week_reset_at, sizeof(snapshot->codex_week_reset_at), "--");
    CopyText(snapshot->codex_five_hour_reset_at, sizeof(snapshot->codex_five_hour_reset_at), "--");
    for (int i = 0; i < style::kMiniChartWeekBarCount; ++i) {
        snapshot->codex_week_trend[i] = 0;
    }
    for (int i = 0; i < style::kMiniChartFiveHourBarCount; ++i) {
        snapshot->codex_five_hour_trend[i] = 0;
    }
    snapshot->codex_week_budget_line_pct = static_cast<uint8_t>(kMiniChartBudgetLinePct + 0.5);
    snapshot->codex_five_hour_budget_line_pct = static_cast<uint8_t>(kMiniChartBudgetLinePct + 0.5);
    snapshot->codex_trend_active_mask = 0;
    snapshot->codex_received_epoch = 0;
    CopyText(snapshot->updated_at, sizeof(snapshot->updated_at), "");
    CopyText(snapshot->link_state, sizeof(snapshot->link_state), "NO DATA");
}

bool SetSnapshotClockFromSystem(DashboardSnapshot *snapshot) {
    std::time_t now = 0;
    std::time(&now);
    if (now < 1700000000) {
        return false;
    }

    std::tm local = {};
    localtime_r(&now, &local);
    if (local.tm_wday < 0 || local.tm_wday > 6 || local.tm_mon < 0 || local.tm_mon > 11 ||
        local.tm_mday < 1 || local.tm_mday > 31) {
        return false;
    }
    const uint16_t year = static_cast<uint16_t>(local.tm_year + 1900);
    const uint8_t month = static_cast<uint8_t>(local.tm_mon + 1);
    const uint8_t day = static_cast<uint8_t>(local.tm_mday);
    static constexpr const char *kWeekdays[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    snapshot->hour = static_cast<uint8_t>(local.tm_hour);
    snapshot->minute = static_cast<uint8_t>(local.tm_min);
    snapshot->second = static_cast<uint8_t>(local.tm_sec);
    std::snprintf(snapshot->date, sizeof(snapshot->date), "%s %04u-%02u-%02u",
                  kWeekdays[local.tm_wday], year, month, day);
    return true;
}

void UpdateSnapshotClock(DashboardSnapshot *snapshot) {
    if (SetSnapshotClockFromSystem(snapshot)) {
        return;
    }

    snapshot->hour = 0;
    snapshot->minute = 0;
    snapshot->second = 0;
}

void ClearCodexQuota(DashboardSnapshot *snapshot) {
    if (!snapshot) {
        return;
    }
    CopyText(snapshot->codex_state, sizeof(snapshot->codex_state), "NO_DATA");
    CopyText(snapshot->codex_source, sizeof(snapshot->codex_source), "--");
    snapshot->codex_week_remaining_pct = 0;
    snapshot->codex_five_hour_remaining_pct = 0;
    CopyText(snapshot->codex_burn_rate, sizeof(snapshot->codex_burn_rate), "--");
    CopyText(snapshot->codex_week_reset_at, sizeof(snapshot->codex_week_reset_at), "--");
    CopyText(snapshot->codex_five_hour_reset_at, sizeof(snapshot->codex_five_hour_reset_at), "--");
    for (int i = 0; i < style::kMiniChartWeekBarCount; ++i) {
        snapshot->codex_week_trend[i] = 0;
    }
    for (int i = 0; i < style::kMiniChartFiveHourBarCount; ++i) {
        snapshot->codex_five_hour_trend[i] = 0;
    }
    snapshot->codex_week_budget_line_pct = static_cast<uint8_t>(kMiniChartBudgetLinePct + 0.5);
    snapshot->codex_five_hour_budget_line_pct = static_cast<uint8_t>(kMiniChartBudgetLinePct + 0.5);
    snapshot->codex_trend_active_mask = 0;
    snapshot->codex_received_epoch = 0;
}

void MarkCodexQuotaStale(DashboardSnapshot *snapshot) {
    if (!snapshot) {
        return;
    }
    if (std::strcmp(snapshot->codex_state, "NO_DATA") == 0) {
        return;
    }
    CopyText(snapshot->codex_state, sizeof(snapshot->codex_state), "STALE");
    if (snapshot->codex_source[0] == '\0' || std::strcmp(snapshot->codex_source, "--") == 0) {
        CopyText(snapshot->codex_source, sizeof(snapshot->codex_source), "API");
    }
}

bool CodexSnapshotExpired(const DashboardSnapshot &snapshot) {
    if (snapshot.codex_received_epoch <= 0) {
        return true;
    }
    const int64_t now = CurrentEpochSeconds();
    if (now <= 0 || now < snapshot.codex_received_epoch) {
        return true;
    }
    return now - snapshot.codex_received_epoch > kCodexHistoricalMaxAgeSeconds;
}

void ApplyCodexFreshnessPolicy(DashboardSnapshot *snapshot) {
    if (!snapshot) {
        return;
    }
    if (std::strcmp(snapshot->codex_state, "NO_DATA") == 0) {
        return;
    }
    if (CodexSnapshotExpired(*snapshot)) {
        ClearCodexQuota(snapshot);
    }
}

// ===== 远端 API JSON 与 NVS 缓存 =====
// 正式链路解析 Codex Quota API；ParseDashboardJson() 只保留给旧缓存/桌面预览兼容。
bool ParseCodexQuotaSampleData(const cJSON *data, const cJSON *meta, DashboardSnapshot *snapshot) {
    const cJSON *limits = data ? JsonObjectItem(data, "limits") : nullptr;
    const cJSON *primary = limits ? JsonObjectItem(limits, "primary") : nullptr;
    const cJSON *secondary = limits ? JsonObjectItem(limits, "secondary") : nullptr;
    if (!data || !primary || !secondary) {
        return false;
    }

    const int five_hour_used = ClampPercent(static_cast<int>(JsonNumber(primary, "used_percent", 0.0)));
    const int weekly_used = ClampPercent(static_cast<int>(JsonNumber(secondary, "used_percent", 0.0)));
    snapshot->codex_five_hour_remaining_pct = ClampPercent(100 - five_hour_used);
    snapshot->codex_week_remaining_pct = ClampPercent(100 - weekly_used);
    CopyText(snapshot->codex_source, sizeof(snapshot->codex_source), "API");
    CopyApiTimestamp(snapshot->updated_at, sizeof(snapshot->updated_at), JsonString(data, "collected_at", snapshot->updated_at));
    FormatApiReset(snapshot->codex_five_hour_reset_at, sizeof(snapshot->codex_five_hour_reset_at), "5H",
                   JsonNumber(primary, "resets_at", 0.0));
    FormatApiReset(snapshot->codex_week_reset_at, sizeof(snapshot->codex_week_reset_at), "WEEK",
                   JsonNumber(secondary, "resets_at", 0.0));

    const char *collector_status = JsonString(data, "collector_status", "unknown");
    const char *reached_type = JsonString(data, "rate_limit_reached_type", "");
    const bool stale = meta && cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(meta, "stale"));
    if (stale) {
        CopyText(snapshot->codex_state, sizeof(snapshot->codex_state), "STALE");
    } else if (std::strcmp(collector_status, "ok") != 0) {
        CopyUpperText(snapshot->codex_state, sizeof(snapshot->codex_state), collector_status);
    } else if (reached_type[0] != '\0') {
        CopyText(snapshot->codex_state, sizeof(snapshot->codex_state), "LIMIT");
    } else if (snapshot->codex_five_hour_remaining_pct <= 15 || snapshot->codex_week_remaining_pct <= 15) {
        CopyText(snapshot->codex_state, sizeof(snapshot->codex_state), "LOW");
    } else {
        CopyText(snapshot->codex_state, sizeof(snapshot->codex_state), "GOOD");
    }

    const cJSON *credits = cJSON_GetObjectItemCaseSensitive(data, "credits");
    if (!cJSON_IsObject(credits)) {
        CopyText(snapshot->codex_burn_rate, sizeof(snapshot->codex_burn_rate), "EXTRA OFF");
    } else if (cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(credits, "unlimited"))) {
        CopyText(snapshot->codex_burn_rate, sizeof(snapshot->codex_burn_rate), "EXTRA UNLIMIT");
    } else if (cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(credits, "overage_limit_reached")) ||
               cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(credits, "spend_control_reached"))) {
        CopyText(snapshot->codex_burn_rate, sizeof(snapshot->codex_burn_rate), "EXTRA LIMIT");
    } else {
        const char *balance = JsonString(credits, "balance", "");
        if (balance[0] != '\0') {
            std::snprintf(snapshot->codex_burn_rate, sizeof(snapshot->codex_burn_rate), "EXTRA %s", balance);
        } else {
            CopyText(snapshot->codex_burn_rate, sizeof(snapshot->codex_burn_rate), "EXTRA ON");
        }
    }

    return true;
}

bool ParseCodexQuotaLatestJson(const char *payload, DashboardSnapshot *snapshot) {
    cJSON *root = cJSON_Parse(payload);
    if (!root) {
        return false;
    }

    const cJSON *data = JsonObjectItem(root, "data");
    const cJSON *meta = JsonObjectItem(root, "meta");
    const bool ok = ParseCodexQuotaSampleData(data, meta, snapshot);
    cJSON_Delete(root);
    return ok;
}

bool ParseUsageDeltaBuckets(const cJSON *data,
                            const char *array_name,
                            const char *delta_key,
                            double budget_delta_pct,
                            uint8_t *values,
                            int max_count,
                            uint8_t *budget_line_pct) {
    if (!data || !array_name || !delta_key || !values || max_count <= 0) {
        return false;
    }

    const cJSON *items = cJSON_GetObjectItemCaseSensitive(data, array_name);
    if (!cJSON_IsArray(items)) {
        return false;
    }

    for (int i = 0; i < max_count; ++i) {
        values[i] = 0;
    }

    const int item_count = cJSON_GetArraySize(items);
    const int copy_count = item_count < max_count ? item_count : max_count;
    const int source_start = item_count > max_count ? item_count - max_count : 0;
    const int target_start = max_count - copy_count;
    int scaled[style::kMiniChartMaxBarCount] = {};
    int max_scaled = 0;
    for (int i = 0; i < copy_count; ++i) {
        const cJSON *bucket = cJSON_GetArrayItem(items, source_start + i);
        const double delta = JsonNumber(bucket, delta_key, 0.0);
        const int height_pct = budget_delta_pct > 0.0
                                   ? static_cast<int>((delta * kMiniChartBudgetLinePct / budget_delta_pct) + 0.5)
                                   : 0;
        scaled[target_start + i] = height_pct < 0 ? 0 : height_pct;
        if (scaled[target_start + i] > max_scaled) {
            max_scaled = scaled[target_start + i];
        }
    }
    const int scale_denominator = max_scaled > 100 ? max_scaled : 100;
    if (budget_line_pct) {
        *budget_line_pct = ClampPercent(static_cast<int>((kMiniChartBudgetLinePct * 100.0 / scale_denominator) + 0.5));
    }
    for (int i = 0; i < max_count; ++i) {
        values[i] = ClampPercent((scaled[i] * 100 + scale_denominator / 2) / scale_denominator);
    }
    return true;
}

bool ParseCodexQuotaUsageSummaryJson(const char *payload, DashboardSnapshot *snapshot, bool *trends_ok = nullptr) {
    if (trends_ok) {
        *trends_ok = false;
    }
    cJSON *root = cJSON_Parse(payload);
    if (!root) {
        return false;
    }

    const cJSON *data = JsonObjectItem(root, "data");
    const cJSON *meta = JsonObjectItem(root, "meta");
    const cJSON *latest = data ? JsonObjectItem(data, "latest") : nullptr;
    const bool ok = ParseCodexQuotaSampleData(latest, meta, snapshot);
    if (ok) {
        const bool week_ok = ParseUsageDeltaBuckets(data, "seven_day", "secondary_used_delta",
                                                    100.0 / style::kMiniChartWeekBarCount,
                                                    snapshot->codex_week_trend,
                                                    style::kMiniChartWeekBarCount,
                                                    &snapshot->codex_week_budget_line_pct);
        if (!week_ok) {
            ESP_LOGW(kTag, "Codex usage-summary seven_day buckets missing");
        }
        const bool five_hour_ok = ParseUsageDeltaBuckets(data, "five_hour", "primary_used_delta",
                                                         100.0 / style::kMiniChartFiveHourBarCount,
                                                         snapshot->codex_five_hour_trend,
                                                         style::kMiniChartFiveHourBarCount,
                                                         &snapshot->codex_five_hour_budget_line_pct);
        if (!five_hour_ok) {
            ESP_LOGW(kTag, "Codex usage-summary five_hour buckets missing");
        }
        if (trends_ok) {
            *trends_ok = week_ok && five_hour_ok;
        }
    }
    cJSON_Delete(root);
    return ok;
}

bool ParseDashboardJson(const char *payload, DashboardSnapshot *snapshot) {
    cJSON *root = cJSON_Parse(payload);
    if (!root) {
        return false;
    }

    BuildEmptySnapshot(snapshot);
    const char *updated_at = JsonString(root, "updated_at", snapshot->updated_at);
    CopyText(snapshot->updated_at, sizeof(snapshot->updated_at), updated_at);

    const cJSON *weather = cJSON_GetObjectItemCaseSensitive(root, "weather");
    if (cJSON_IsObject(weather)) {
        CopyText(snapshot->weather_location, sizeof(snapshot->weather_location),
                 JsonString(weather, "location", snapshot->weather_location));
        CopyText(snapshot->weather_summary, sizeof(snapshot->weather_summary),
                 JsonString(weather, "condition", snapshot->weather_summary));
        snapshot->weather_temp_c = JsonFloat(weather, "temp_c", snapshot->weather_temp_c);
        snapshot->weather_humidity_pct = ClampPercent(JsonInt(weather, "humidity_pct", snapshot->weather_humidity_pct));
        snapshot->weather_wind_kmh = JsonFloat(weather, "wind_kmh", snapshot->weather_wind_kmh);
    }

    const cJSON *server = cJSON_GetObjectItemCaseSensitive(root, "server_monitor");
    if (cJSON_IsObject(server)) {
        CopyUpperText(snapshot->server_target, sizeof(snapshot->server_target),
                      JsonString(server, "target", snapshot->server_target));
        CopyUpperText(snapshot->server_role, sizeof(snapshot->server_role),
                      JsonString(server, "role", snapshot->server_role));
        CopyUpperText(snapshot->server_state, sizeof(snapshot->server_state),
                      JsonString(server, "state", snapshot->server_state));
        CopyText(snapshot->server_summary, sizeof(snapshot->server_summary),
                 JsonString(server, "summary", snapshot->server_summary));
        snapshot->server_cpu_pct = ClampPercent(JsonInt(server, "cpu_percent", snapshot->server_cpu_pct));
        snapshot->server_mem_pct = ClampPercent(JsonInt(server, "memory_percent", snapshot->server_mem_pct));
        snapshot->server_latency_ms = static_cast<uint16_t>(JsonInt(server, "latency_ms", snapshot->server_latency_ms));
        snapshot->gpu_util_pct = ClampPercent(JsonInt(server, "gpu_util_percent", snapshot->gpu_util_pct));
        snapshot->gpu_mem_pct = ClampPercent(JsonInt(server, "gpu_memory_percent", snapshot->gpu_mem_pct));
        snapshot->gpu_temp_c = static_cast<uint8_t>(JsonInt(server, "gpu_temp_c", snapshot->gpu_temp_c));
        snapshot->gpu_processes = static_cast<uint8_t>(JsonInt(server, "gpu_processes", snapshot->gpu_processes));
        ParseGpuCards(server, snapshot);
    }

    const cJSON *codex = cJSON_GetObjectItemCaseSensitive(root, "codex");
    if (cJSON_IsObject(codex)) {
        CopyText(snapshot->updated_at, sizeof(snapshot->updated_at),
                 JsonString(codex, "updated_at", snapshot->updated_at));
        CopyUpperText(snapshot->codex_state, sizeof(snapshot->codex_state),
                      JsonString(codex, "state", snapshot->codex_state));
        CopyCodexSource(snapshot->codex_source, sizeof(snapshot->codex_source),
                        JsonString(codex, "source", snapshot->codex_source));
        CopyUpperText(snapshot->codex_burn_rate, sizeof(snapshot->codex_burn_rate),
                      JsonString(codex, "burn_rate", snapshot->codex_burn_rate));
        CopyUpperText(snapshot->codex_week_reset_at, sizeof(snapshot->codex_week_reset_at),
                      JsonString(codex, "weekly_reset_at", snapshot->codex_week_reset_at));
        CopyUpperText(snapshot->codex_five_hour_reset_at, sizeof(snapshot->codex_five_hour_reset_at),
                      JsonString(codex, "five_hour_reset_at", snapshot->codex_five_hour_reset_at));
        snapshot->codex_week_remaining_pct = ClampPercent(JsonInt(codex, "weekly_remaining_pct", snapshot->codex_week_remaining_pct));
        snapshot->codex_five_hour_remaining_pct = ClampPercent(JsonInt(codex, "five_hour_remaining_pct", snapshot->codex_five_hour_remaining_pct));

        ParsePercentArray(codex, "weekly_trend_pct", snapshot->codex_week_trend,
                          style::kMiniChartWeekBarCount);
        ParsePercentArray(codex, "five_hour_trend_pct", snapshot->codex_five_hour_trend,
                          style::kMiniChartFiveHourBarCount);
        if (!cJSON_IsArray(cJSON_GetObjectItemCaseSensitive(codex, "weekly_trend_pct"))) {
            ParsePercentArray(codex, "trend_pct", snapshot->codex_week_trend,
                              style::kMiniChartWeekBarCount);
        }

        snapshot->codex_trend_active_mask = 0;
        const cJSON *active = cJSON_GetObjectItemCaseSensitive(codex, "trend_active_indexes");
        if (cJSON_IsArray(active)) {
            const int count = cJSON_GetArraySize(active);
            for (int i = 0; i < count; ++i) {
                const cJSON *item = cJSON_GetArrayItem(active, i);
                if (cJSON_IsNumber(item) && item->valueint >= 0 && item->valueint < 12) {
                    snapshot->codex_trend_active_mask |= static_cast<uint16_t>(1U << item->valueint);
                }
            }
        }
    }

    CopyText(snapshot->link_state, sizeof(snapshot->link_state), "WIFI OK");
    cJSON_Delete(root);
    return true;
}

// NVS 只保存最后一次成功的 API latest 响应，不保存任何令牌或认证材料。
void SaveDashboardCache(const char *payload) {
    if (!g_nvs_ready || !payload || payload[0] == '\0') {
        return;
    }
    const int64_t now = CurrentEpochSeconds();
    if (now <= 0) {
        ESP_LOGW(kTag, "dashboard cache skipped before clock sync");
        return;
    }

    nvs_handle_t handle = 0;
    esp_err_t err = nvs_open(kNvsNamespace, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "dashboard cache open failed: %s", esp_err_to_name(err));
        return;
    }
    err = nvs_set_str(handle, kNvsLastJsonKey, payload);
    if (err == ESP_OK) {
        err = nvs_set_i64(handle, kNvsLastJsonEpochKey, now);
    }
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "dashboard cache write failed: %s", esp_err_to_name(err));
    }
}

// 启动时优先读缓存，随后后台任务再联网刷新。缓存状态会在顶栏显示 CACHE。
bool LoadCachedSnapshot(DashboardSnapshot *snapshot) {
    if (!g_nvs_ready || !snapshot) {
        return false;
    }

    nvs_handle_t handle = 0;
    esp_err_t err = nvs_open(kNvsNamespace, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        return false;
    }

    size_t length = 0;
    err = nvs_get_str(handle, kNvsLastJsonKey, nullptr, &length);
    if (err != ESP_OK || length == 0 || length > kHttpResponseCapacity) {
        nvs_close(handle);
        return false;
    }
    int64_t cached_epoch = 0;
    err = nvs_get_i64(handle, kNvsLastJsonEpochKey, &cached_epoch);
    if (err != ESP_OK || cached_epoch <= 0) {
        nvs_close(handle);
        return false;
    }
    const int64_t now = CurrentEpochSeconds();
    if (now <= 0 || now < cached_epoch || now - cached_epoch > kCodexHistoricalMaxAgeSeconds) {
        nvs_close(handle);
        return false;
    }

    char *buffer = static_cast<char *>(heap_caps_calloc(kHttpResponseCapacity, 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!buffer) {
        buffer = static_cast<char *>(heap_caps_calloc(kHttpResponseCapacity, 1, MALLOC_CAP_8BIT));
    }
    if (!buffer) {
        nvs_close(handle);
        ESP_LOGW(kTag, "dashboard cache buffer allocation failed");
        return false;
    }

    err = nvs_get_str(handle, kNvsLastJsonKey, buffer, &length);
    nvs_close(handle);
    if (err != ESP_OK) {
        heap_caps_free(buffer);
        return false;
    }
    BuildEmptySnapshot(snapshot);
    const bool parsed = ParseCodexQuotaUsageSummaryJson(buffer, snapshot) ||
                        ParseCodexQuotaLatestJson(buffer, snapshot) ||
                        ParseDashboardJson(buffer, snapshot);
    heap_caps_free(buffer);
    if (!parsed) {
        return false;
    }
    snapshot->codex_received_epoch = cached_epoch;
    MarkCodexQuotaStale(snapshot);
    CopyText(snapshot->link_state, sizeof(snapshot->link_state), "CACHE");
    ESP_LOGI(kTag, "loaded dashboard cache from NVS");
    return true;
}

// esp_http_client 会分块回调数据，这里只追加到固定缓冲。
esp_err_t HttpEventHandler(esp_http_client_event_t *event) {
    if (event->event_id != HTTP_EVENT_ON_DATA || !event->user_data || !event->data) {
        return ESP_OK;
    }
    auto *buffer = static_cast<HttpBuffer *>(event->user_data);
    const int remaining = buffer->capacity - buffer->length - 1;
    if (remaining <= 0) {
        buffer->overflow = true;
        return ESP_OK;
    }
    const int copy_len = (event->data_len < remaining) ? event->data_len : remaining;
    std::memcpy(buffer->data + buffer->length, event->data, copy_len);
    buffer->length += copy_len;
    buffer->data[buffer->length] = '\0';
    if (copy_len < event->data_len) {
        buffer->overflow = true;
    }
    return ESP_OK;
}

void ConfigureHttpHeaders(esp_http_client_handle_t client, HttpAuth auth) {
    if (auth == HttpAuth::kCodexApiKey && std::strlen(CONFIG_DASHBOARD_API_KEY) > 0) {
        esp_http_client_set_header(client, "X-API-Key", CONFIG_DASHBOARD_API_KEY);
    } else if (auth == HttpAuth::kServerMonitorToken && std::strlen(CONFIG_DASHBOARD_SERVER_MONITOR_TOKEN) > 0) {
        char authorization[192] = {};
        std::snprintf(authorization, sizeof(authorization), "Bearer %s", CONFIG_DASHBOARD_SERVER_MONITOR_TOKEN);
        esp_http_client_set_header(client, "Authorization", authorization);
    }
    if (auth != HttpAuth::kNone &&
        std::strlen(CONFIG_DASHBOARD_CF_ACCESS_CLIENT_ID) > 0 &&
        std::strlen(CONFIG_DASHBOARD_CF_ACCESS_CLIENT_SECRET) > 0) {
        esp_http_client_set_header(client, "CF-Access-Client-Id", CONFIG_DASHBOARD_CF_ACCESS_CLIENT_ID);
        esp_http_client_set_header(client, "CF-Access-Client-Secret", CONFIG_DASHBOARD_CF_ACCESS_CLIENT_SECRET);
    }
    esp_http_client_set_header(client, "Accept", "application/json");
}

esp_http_client_handle_t CreateHttpClient(const char *url, HttpBuffer *buffer, bool keep_alive) {
    esp_http_client_config_t config = {};
    config.url = url;
    config.timeout_ms = kHttpTimeoutMs;
    config.event_handler = HttpEventHandler;
    config.user_data = buffer;
    config.buffer_size = 1024;
    config.crt_bundle_attach = esp_crt_bundle_attach;
    config.addr_type = HTTP_ADDR_TYPE_INET;
    config.keep_alive_enable = keep_alive;
    config.keep_alive_idle = kHttpKeepAliveIdleSeconds;
    config.keep_alive_interval = kHttpKeepAliveIntervalSeconds;
    config.keep_alive_count = kHttpKeepAliveCount;
    return esp_http_client_init(&config);
}

bool PerformHttpPayload(esp_http_client_handle_t client,
                        const char *url,
                        HttpBuffer *buffer,
                        int64_t *elapsed_ms = nullptr) {
    if (!client || !url || !buffer) {
        return false;
    }
    buffer->length = 0;
    buffer->overflow = false;
    if (buffer->data && buffer->capacity > 0) {
        buffer->data[0] = '\0';
    }
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_http_client_set_user_data(client, buffer));

    const int64_t start_us = esp_timer_get_time();
    const esp_err_t err = esp_http_client_perform(client);
    const int64_t request_elapsed_ms = (esp_timer_get_time() - start_us) / 1000LL;
    if (elapsed_ms) {
        *elapsed_ms = request_elapsed_ms;
    }
    const int status = esp_http_client_get_status_code(client);
    if (err != ESP_OK || status != 200 || buffer->length == 0 || buffer->overflow) {
        int tls_error = 0;
        int tls_flags = 0;
        esp_http_client_get_and_clear_last_tls_error(client, &tls_error, &tls_flags);
        const int socket_errno = esp_http_client_get_errno(client);
        DiagnosticRecordHttp(url, "http_fail", status, buffer->length, buffer->overflow,
                             request_elapsed_ms, socket_errno, tls_error, tls_flags);
        ESP_LOGW(kTag,
                 "API fetch failed: err=%s status=%d len=%d overflow=%d elapsed=%lldms errno=%d tls=0x%x flags=0x%x url=%s",
                 esp_err_to_name(err), status, buffer->length, buffer->overflow ? 1 : 0,
                 request_elapsed_ms, socket_errno, tls_error, tls_flags, url);
        return false;
    }
    DiagnosticRecordHttp(url, "http_ok", status, buffer->length, buffer->overflow,
                         request_elapsed_ms, 0, 0, 0);
    return true;
}

bool TakeHttpMutex(const char *url) {
    if (!g_http_mutex) {
        return true;
    }
    if (xSemaphoreTake(g_http_mutex, pdMS_TO_TICKS(kHttpMutexWaitMs)) == pdTRUE) {
        return true;
    }
    ESP_LOGW(kTag, "HTTP fetch skipped: mutex wait timeout url=%s", url ? url : "");
    DiagnosticLog(DiagnosticSourceForUrl(url), "http_mutex_timeout", "wait=%dms", kHttpMutexWaitMs);
    return false;
}

void GiveHttpMutex() {
    if (g_http_mutex) {
        xSemaphoreGive(g_http_mutex);
    }
}

bool FetchHttpPayload(const char *url, char *response, size_t response_size, HttpAuth auth) {
    if (!url || url[0] == '\0' || !response || response_size == 0) {
        return false;
    }
    if (!WaitForSystemTimeIfHttps(url)) {
        return false;
    }
    if (!TakeHttpMutex(url)) {
        return false;
    }

    HttpBuffer buffer = {};
    buffer.data = response;
    buffer.capacity = static_cast<int>(response_size);
    esp_http_client_handle_t client = CreateHttpClient(url, &buffer, false);
    if (!client) {
        GiveHttpMutex();
        return false;
    }
    ConfigureHttpHeaders(client, auth);
    const bool ok = PerformHttpPayload(client, url, &buffer);
    esp_http_client_cleanup(client);
    GiveHttpMutex();
    return ok;
}

bool FetchReusableHttpPayload(const char *url,
                              char *response,
                              size_t response_size,
                              HttpAuth auth,
                              esp_http_client_handle_t *client_slot,
                              char *client_url,
                              size_t client_url_size,
                              int64_t *elapsed_ms = nullptr) {
    if (!url || url[0] == '\0' || !response || response_size == 0 || !client_slot ||
        !client_url || client_url_size == 0) {
        return false;
    }
    if (!WaitForSystemTimeIfHttps(url)) {
        return false;
    }
    if (!TakeHttpMutex(url)) {
        return false;
    }

    HttpBuffer buffer = {};
    buffer.data = response;
    buffer.capacity = static_cast<int>(response_size);
    if (*client_slot && !SameText(client_url, url)) {
        esp_http_client_cleanup(*client_slot);
        *client_slot = nullptr;
        client_url[0] = '\0';
    }
    if (!*client_slot) {
        *client_slot = CreateHttpClient(url, &buffer, true);
        if (!*client_slot) {
            GiveHttpMutex();
            return false;
        }
        ConfigureHttpHeaders(*client_slot, auth);
        CopyText(client_url, client_url_size, url);
    }
    const bool ok = PerformHttpPayload(*client_slot, url, &buffer, elapsed_ms);
    if (!ok) {
        esp_http_client_cleanup(*client_slot);
        *client_slot = nullptr;
        client_url[0] = '\0';
    }
    GiveHttpMutex();
    return ok;
}

void ResetServerMonitorClient() {
    if (g_server_monitor_client) {
        esp_http_client_cleanup(g_server_monitor_client);
        g_server_monitor_client = nullptr;
    }
    g_server_monitor_client_url[0] = '\0';
}

void ResetServerMonitorEndpointOrder() {
    ResetServerMonitorClient();
    g_server_monitor_endpoint_cursor = 0;
    g_server_monitor_endpoint_wifi_profile = g_active_wifi_profile;
}

// 网络拉取只在各数据源后台任务中调用，避免阻塞每秒 UI 刷新。
bool FetchCodexSnapshot(DashboardSnapshot *snapshot, bool *usage_summary_ok) {
    if (usage_summary_ok) {
        *usage_summary_ok = false;
    }
    if (std::strlen(CONFIG_DASHBOARD_API_URL) == 0) {
        CopyText(snapshot->link_state, sizeof(snapshot->link_state), "URL CFG");
        DiagnosticRecordCodex(*snapshot, false, false, false, "url_config_missing");
        return false;
    }
    if (std::strlen(CONFIG_DASHBOARD_API_KEY) == 0) {
        CopyText(snapshot->link_state, sizeof(snapshot->link_state), "KEY CFG");
        DiagnosticRecordCodex(*snapshot, false, false, false, "api_key_missing");
        return false;
    }

    char *response = g_codex_response;
    response[0] = '\0';
    char summary_url[192] = {};
    BuildCodexApiSiblingUrl(summary_url, sizeof(summary_url), kCodexUsageSummarySuffix);
    ESP_LOGI(kTag, "dashboard API fetch start: usage-summary");
    if (FetchHttpPayload(summary_url, response, kHttpResponseCapacity, HttpAuth::kCodexApiKey)) {
        bool trends_ok = false;
        if (!ParseCodexQuotaUsageSummaryJson(response, snapshot, &trends_ok)) {
            CopyText(snapshot->link_state, sizeof(snapshot->link_state), "JSON ERR");
            DiagnosticRecordCodex(*snapshot, false, false, false, "usage_summary_json");
            return false;
        }
        if (usage_summary_ok) {
            *usage_summary_ok = trends_ok;
        }
        if (!trends_ok) {
            DiagnosticRecordCodex(*snapshot, true, false, true, "usage_summary_incomplete");
        }
    } else {
        response[0] = '\0';
        ESP_LOGW(kTag, "dashboard API usage-summary failed, retrying latest");
        if (!FetchHttpPayload(CONFIG_DASHBOARD_API_URL, response, kHttpResponseCapacity, HttpAuth::kCodexApiKey)) {
            CopyText(snapshot->link_state, sizeof(snapshot->link_state), "STALE");
            DiagnosticRecordCodex(*snapshot, false, false, false, "latest_http");
            return false;
        }
        if (!ParseCodexQuotaLatestJson(response, snapshot)) {
            CopyText(snapshot->link_state, sizeof(snapshot->link_state), "JSON ERR");
            DiagnosticRecordCodex(*snapshot, false, false, false, "latest_json");
            return false;
        }
    }
    SaveDashboardCache(response);
    snapshot->codex_received_epoch = CurrentEpochSeconds();
    CopyLocalUpdatedTimestamp(snapshot->updated_at, sizeof(snapshot->updated_at));

    ESP_LOGI(kTag, "dashboard API fetch ok: codex=%s week=%u%% 5h=%u%%",
             snapshot->codex_state,
             snapshot->codex_week_remaining_pct,
             snapshot->codex_five_hour_remaining_pct);
    CopyText(snapshot->link_state, sizeof(snapshot->link_state), "API OK");
    DiagnosticRecordCodex(*snapshot, true, usage_summary_ok ? *usage_summary_ok : false, true, "");
    return true;
}

bool FetchWeatherSnapshot(DashboardSnapshot *snapshot) {
    if (std::strlen(CONFIG_DASHBOARD_WEATHER_URL) == 0) {
        return false;
    }

    char *response = g_weather_response;
    response[0] = '\0';
    if (!FetchHttpPayload(CONFIG_DASHBOARD_WEATHER_URL, response, kHttpResponseCapacity, HttpAuth::kNone)) {
        const char *url = CONFIG_DASHBOARD_WEATHER_URL;
        if (std::strncmp(url, kOpenMeteoHttpsPrefix, std::strlen(kOpenMeteoHttpsPrefix)) == 0) {
            char fallback_url[512] = {};
            const int written = std::snprintf(fallback_url,
                                              sizeof(fallback_url),
                                              "%s%s",
                                              kOpenMeteoHttpPrefix,
                                              url + std::strlen(kOpenMeteoHttpsPrefix));
            if (written > 0 && written < static_cast<int>(sizeof(fallback_url))) {
                ESP_LOGW(kTag, "weather HTTPS failed, retrying Open-Meteo over HTTP");
                response[0] = '\0';
                if (!FetchHttpPayload(fallback_url, response, kHttpResponseCapacity, HttpAuth::kNone)) {
                    ESP_LOGW(kTag, "weather fallback fetch failed");
                    return false;
                }
            } else {
                ESP_LOGW(kTag, "weather fallback URL overflow");
                return false;
            }
        } else {
            ESP_LOGW(kTag, "weather fetch failed");
            return false;
        }
    }
    if (!ParseOpenMeteoJson(response, snapshot)) {
        ESP_LOGW(kTag, "weather JSON parse failed");
        return false;
    }
    ESP_LOGI(kTag,
             "weather fetch ok: temp=%.1fC humidity=%u%% wind=%.1fkm/h",
             static_cast<double>(snapshot->weather_temp_c),
             snapshot->weather_humidity_pct,
             static_cast<double>(snapshot->weather_wind_kmh));
    CopyLocalUpdatedTimestamp(snapshot->updated_at, sizeof(snapshot->updated_at));
    return true;
}

bool FetchServerMonitorSnapshot(DashboardSnapshot *snapshot, int64_t *elapsed_ms) {
    if (std::strlen(CONFIG_DASHBOARD_SERVER_MONITOR_TOKEN) == 0) {
        return false;
    }
    ServerMonitorEndpoint endpoints[kServerMonitorEndpointCapacity] = {};
    const size_t endpoint_count = BuildServerMonitorEndpointOrder(endpoints, kServerMonitorEndpointCapacity);
    if (endpoint_count == 0) {
        return false;
    }
    if (g_server_monitor_endpoint_wifi_profile != g_active_wifi_profile ||
        g_server_monitor_endpoint_cursor >= endpoint_count) {
        ResetServerMonitorEndpointOrder();
    }

    char *response = g_server_monitor_response;
    int64_t last_elapsed_ms = -1;
    for (size_t attempt = 0; attempt < endpoint_count; ++attempt) {
        const size_t slot = (g_server_monitor_endpoint_cursor + attempt) % endpoint_count;
        const ServerMonitorEndpoint &endpoint = endpoints[slot];
        response[0] = '\0';
        int64_t attempt_elapsed_ms = -1;
        bool fetched = FetchReusableHttpPayload(endpoint.url, response, kHttpResponseCapacity,
                                                HttpAuth::kServerMonitorToken, &g_server_monitor_client,
                                                g_server_monitor_client_url, sizeof(g_server_monitor_client_url),
                                                &attempt_elapsed_ms);
        if (!fetched && attempt_elapsed_ms >= 0 &&
            attempt_elapsed_ms < kServerMonitorFastReconnectRetryMs) {
            response[0] = '\0';
            fetched = FetchReusableHttpPayload(endpoint.url, response, kHttpResponseCapacity,
                                               HttpAuth::kServerMonitorToken, &g_server_monitor_client,
                                               g_server_monitor_client_url, sizeof(g_server_monitor_client_url),
                                               &attempt_elapsed_ms);
        }
        if (!fetched) {
            ESP_LOGW(kTag, "ServerMonitor endpoint failed: %s elapsed=%lldms",
                     endpoint.label, attempt_elapsed_ms);
            last_elapsed_ms = attempt_elapsed_ms;
            continue;
        }
        if (!ParseServerMonitorJson(response, snapshot)) {
            ESP_LOGW(kTag, "ServerMonitor JSON parse failed: endpoint=%s", endpoint.label);
            ResetServerMonitorClient();
            last_elapsed_ms = attempt_elapsed_ms;
            continue;
        }
        if (!CurrentWifiHasPreferredServerMonitorEndpoint() ||
            ServerMonitorEndpointIsPreferred(endpoint)) {
            g_server_monitor_endpoint_cursor = slot;
        } else {
            g_server_monitor_endpoint_cursor = 0;
        }
        g_server_monitor_endpoint_wifi_profile = g_active_wifi_profile;
        if (elapsed_ms) {
            *elapsed_ms = attempt_elapsed_ms;
        }
        CopyLocalUpdatedTimestamp(snapshot->updated_at, sizeof(snapshot->updated_at));
        CopyText(snapshot->link_state, sizeof(snapshot->link_state), "API OK");
        ESP_LOGI(kTag, "ServerMonitor fetch ok: endpoint=%s state=%s latency=%ums cpu=%u%% mem=%u%% elapsed=%lldms",
                 endpoint.label, snapshot->server_state, snapshot->server_latency_ms,
                 snapshot->server_cpu_pct, snapshot->server_mem_pct, attempt_elapsed_ms);
        return true;
    }
    if (elapsed_ms) {
        *elapsed_ms = last_elapsed_ms;
    }
    return false;
}

// ===== 任务间快照交换 =====
// 拉取任务负责慢速网络，循环任务负责每秒 UI；两者只通过这份缓存通信。
void StoreRemoteSnapshot(const DashboardSnapshot &snapshot) {
    if (!g_snapshot_mutex) {
        return;
    }
    if (xSemaphoreTake(g_snapshot_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return;
    }
    g_remote_snapshot = snapshot;
    g_remote_snapshot_valid = true;
    xSemaphoreGive(g_snapshot_mutex);
}

bool LoadRemoteSnapshot(DashboardSnapshot *snapshot) {
    if (!g_snapshot_mutex) {
        return false;
    }
    if (xSemaphoreTake(g_snapshot_mutex, pdMS_TO_TICKS(10)) != pdTRUE) {
        return false;
    }
    const bool valid = g_remote_snapshot_valid;
    if (valid) {
        *snapshot = g_remote_snapshot;
    }
    xSemaphoreGive(g_snapshot_mutex);
    return valid;
}

void EnsureMergeBase(DashboardSnapshot *snapshot) {
    if (!g_remote_snapshot_valid) {
        BuildEmptySnapshot(snapshot);
        return;
    }
    *snapshot = g_remote_snapshot;
}

bool MergeSnapshot(const DashboardSnapshot &source, void (*merge_fn)(DashboardSnapshot *, const DashboardSnapshot &)) {
    if (!g_snapshot_mutex || !merge_fn) {
        return false;
    }
    if (xSemaphoreTake(g_snapshot_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return false;
    }
    DashboardSnapshot merged = {};
    EnsureMergeBase(&merged);
    merge_fn(&merged, source);
    g_remote_snapshot = merged;
    g_remote_snapshot_valid = true;
    xSemaphoreGive(g_snapshot_mutex);
    return true;
}

void MergeLinkState(DashboardSnapshot *dest, const DashboardSnapshot &source) {
    CopyText(dest->link_state, sizeof(dest->link_state), source.link_state);
    if (source.updated_at[0] != '\0') {
        CopyText(dest->updated_at, sizeof(dest->updated_at), source.updated_at);
    }
}

void MergeServerMonitorSnapshot(DashboardSnapshot *dest, const DashboardSnapshot &source) {
    CopyText(dest->server_target, sizeof(dest->server_target), source.server_target);
    CopyText(dest->server_role, sizeof(dest->server_role), source.server_role);
    CopyText(dest->server_state, sizeof(dest->server_state), source.server_state);
    CopyText(dest->server_summary, sizeof(dest->server_summary), source.server_summary);
    dest->server_cpu_pct = source.server_cpu_pct;
    dest->server_mem_pct = source.server_mem_pct;
    dest->server_latency_ms = source.server_latency_ms;
    dest->gpu_util_pct = source.gpu_util_pct;
    dest->gpu_mem_pct = source.gpu_mem_pct;
    dest->gpu_temp_c = source.gpu_temp_c;
    dest->gpu_processes = source.gpu_processes;
    dest->gpu_card_count = source.gpu_card_count;
    for (uint8_t i = 0; i < 2; ++i) {
        dest->gpu_cards[i] = source.gpu_cards[i];
    }
    MergeLinkState(dest, source);
}

void MergeCodexSnapshot(DashboardSnapshot *dest, const DashboardSnapshot &source) {
    CopyText(dest->codex_state, sizeof(dest->codex_state), source.codex_state);
    CopyText(dest->codex_source, sizeof(dest->codex_source), source.codex_source);
    dest->codex_week_remaining_pct = source.codex_week_remaining_pct;
    dest->codex_five_hour_remaining_pct = source.codex_five_hour_remaining_pct;
    CopyText(dest->codex_burn_rate, sizeof(dest->codex_burn_rate), source.codex_burn_rate);
    CopyText(dest->codex_week_reset_at, sizeof(dest->codex_week_reset_at), source.codex_week_reset_at);
    CopyText(dest->codex_five_hour_reset_at, sizeof(dest->codex_five_hour_reset_at),
             source.codex_five_hour_reset_at);
    for (uint8_t i = 0; i < style::kMiniChartWeekBarCount; ++i) {
        dest->codex_week_trend[i] = source.codex_week_trend[i];
    }
    for (uint8_t i = 0; i < style::kMiniChartFiveHourBarCount; ++i) {
        dest->codex_five_hour_trend[i] = source.codex_five_hour_trend[i];
    }
    dest->codex_week_budget_line_pct = source.codex_week_budget_line_pct;
    dest->codex_five_hour_budget_line_pct = source.codex_five_hour_budget_line_pct;
    dest->codex_trend_active_mask = source.codex_trend_active_mask;
    CopyText(dest->updated_at, sizeof(dest->updated_at), source.updated_at);
}

void MergeWeatherSnapshot(DashboardSnapshot *dest, const DashboardSnapshot &source) {
    CopyText(dest->weather_location, sizeof(dest->weather_location), source.weather_location);
    CopyText(dest->weather_summary, sizeof(dest->weather_summary), source.weather_summary);
    dest->weather_temp_c = source.weather_temp_c;
    dest->weather_humidity_pct = source.weather_humidity_pct;
    dest->weather_wind_kmh = source.weather_wind_kmh;
    CopyText(dest->updated_at, sizeof(dest->updated_at), source.updated_at);
}

uint8_t LoadLastGoodWifiProfile() {
    if (!g_nvs_ready) {
        return kWifiInvalidProfile;
    }
    nvs_handle_t handle = 0;
    esp_err_t err = nvs_open(kNvsNamespace, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        return kWifiInvalidProfile;
    }
    uint8_t index = kWifiInvalidProfile;
    err = nvs_get_u8(handle, kNvsLastWifiKey, &index);
    nvs_close(handle);
    return err == ESP_OK ? index : kWifiInvalidProfile;
}

void SaveLastGoodWifiProfile(uint8_t index) {
    if (!g_nvs_ready || index >= kWifiProfileCount) {
        return;
    }
    nvs_handle_t handle = 0;
    esp_err_t err = nvs_open(kNvsNamespace, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return;
    }
    err = nvs_set_u8(handle, kNvsLastWifiKey, index);
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "save last WiFi profile failed: %s", esp_err_to_name(err));
    }
}

const WifiProfile *FindWifiProfileByIndex(const WifiProfile *profiles, size_t count, uint8_t index) {
    for (size_t i = 0; i < count; ++i) {
        if (profiles[i].index == index) {
            return &profiles[i];
        }
    }
    return nullptr;
}

bool AddWifiAttempt(WifiProfile *attempts, size_t *attempt_count, size_t capacity, const WifiProfile &profile) {
    if (!attempts || !attempt_count || *attempt_count >= capacity) {
        return false;
    }
    for (size_t i = 0; i < *attempt_count; ++i) {
        if (attempts[i].index == profile.index) {
            return true;
        }
    }
    attempts[*attempt_count] = profile;
    ++(*attempt_count);
    return true;
}

bool ProfileVisible(const WifiProfile &profile, const wifi_ap_record_t *records, uint16_t record_count) {
    for (uint16_t i = 0; i < record_count; ++i) {
        if (std::strncmp(reinterpret_cast<const char *>(records[i].ssid), profile.ssid,
                         sizeof(records[i].ssid)) == 0) {
            return true;
        }
    }
    return false;
}

size_t BuildWifiAttemptOrder(const WifiProfile *profiles, size_t profile_count,
                             WifiProfile *attempts, size_t attempt_capacity) {
    uint16_t record_count = 0;
    wifi_ap_record_t records[16] = {};
    esp_err_t err = esp_wifi_scan_start(nullptr, true);
    if (err == ESP_OK) {
        err = esp_wifi_scan_get_ap_num(&record_count);
    }
    if (err == ESP_OK && record_count > 0) {
        if (record_count > sizeof(records) / sizeof(records[0])) {
            record_count = sizeof(records) / sizeof(records[0]);
        }
        err = esp_wifi_scan_get_ap_records(&record_count, records);
    }
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "WiFi scan failed: %s; trying configured order", esp_err_to_name(err));
        record_count = 0;
    }

    size_t attempt_count = 0;
    const uint8_t last_good = LoadLastGoodWifiProfile();
    if (record_count > 0) {
        const WifiProfile *last_profile = FindWifiProfileByIndex(profiles, profile_count, last_good);
        if (last_profile && ProfileVisible(*last_profile, records, record_count)) {
            AddWifiAttempt(attempts, &attempt_count, attempt_capacity, *last_profile);
        }
        for (size_t i = 0; i < profile_count; ++i) {
            if (ProfileVisible(profiles[i], records, record_count)) {
                AddWifiAttempt(attempts, &attempt_count, attempt_capacity, profiles[i]);
            }
        }
    }
    if (attempt_count == 0) {
        const WifiProfile *last_profile = FindWifiProfileByIndex(profiles, profile_count, last_good);
        if (last_profile) {
            AddWifiAttempt(attempts, &attempt_count, attempt_capacity, *last_profile);
        }
        for (size_t i = 0; i < profile_count; ++i) {
            AddWifiAttempt(attempts, &attempt_count, attempt_capacity, profiles[i]);
        }
    }
    return attempt_count;
}

void WifiEventHandler(void *, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        g_transport_ready = false;
        ResetServerMonitorClient();
        if (g_wifi_events) {
            xEventGroupSetBits(g_wifi_events, kWifiFailBit);
        }
        return;
    }
    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        auto *event = static_cast<ip_event_got_ip_t *>(event_data);
        ESP_LOGI(kTag, "WiFi connected, ip=" IPSTR, IP2STR(&event->ip_info.ip));
        SaveLastGoodWifiProfile(g_active_wifi_profile);
        StartClockSync();
        if (g_wifi_events) {
            xEventGroupSetBits(g_wifi_events, kWifiConnectedBit);
        }
    }
}

bool EnsureWifiInitialized() {
    esp_err_t err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(kTag, "netif init failed: %s", esp_err_to_name(err));
        return false;
    }
    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(kTag, "event loop init failed: %s", esp_err_to_name(err));
        return false;
    }
    if (!g_wifi_initialized) {
        esp_netif_create_default_wifi_sta();

        wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_wifi_init(&wifi_init_config));
        g_wifi_events = xEventGroupCreate();
        ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &WifiEventHandler, nullptr, nullptr));
        ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &WifiEventHandler, nullptr, nullptr));
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_start());
        g_wifi_initialized = true;
    }
    return true;
}

bool ConnectWifiProfile(const WifiProfile &profile) {
    wifi_config_t wifi_config = {};
    std::snprintf(reinterpret_cast<char *>(wifi_config.sta.ssid), sizeof(wifi_config.sta.ssid), "%s", profile.ssid);
    std::snprintf(reinterpret_cast<char *>(wifi_config.sta.password), sizeof(wifi_config.sta.password), "%s", profile.password);
    wifi_config.sta.threshold.authmode = profile.password[0] == '\0' ? WIFI_AUTH_OPEN : WIFI_AUTH_WPA2_PSK;

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    if (g_wifi_events) {
        xEventGroupClearBits(g_wifi_events, kWifiConnectedBit | kWifiFailBit);
    }
    g_active_wifi_profile = profile.index;
    ESP_LOGI(kTag, "trying WiFi profile %u: %s", profile.index, profile.ssid);
    ESP_ERROR_CHECK(esp_wifi_connect());

    const EventBits_t bits = xEventGroupWaitBits(g_wifi_events, kWifiConnectedBit | kWifiFailBit,
                                                 pdFALSE, pdFALSE, pdMS_TO_TICKS(kWifiConnectTimeoutMs));
    if ((bits & kWifiConnectedBit) != 0) {
        g_transport_ready = true;
        ResetServerMonitorEndpointOrder();
        return true;
    }
    esp_wifi_disconnect();
    g_active_wifi_profile = kWifiInvalidProfile;
    ESP_LOGW(kTag, "WiFi profile %u connection timed out or failed", profile.index);
    return false;
}

// WiFi 只负责把网络链路拉起来；失败时 UI 仍可显示本地数据或 NVS 缓存。
bool StartDashboardWifi() {
    WifiProfile profiles[kWifiProfileCount] = {};
    const size_t profile_count = GetConfiguredWifiProfiles(profiles, kWifiProfileCount);
    if (profile_count == 0) {
        ESP_LOGW(kTag, "WiFi SSID not configured; using local empty state");
        return false;
    }
    if (!g_nvs_ready) {
        InitNvs();
        if (!g_nvs_ready) {
            return false;
        }
    }
    if (!EnsureWifiInitialized()) {
        return false;
    }

    WifiProfile attempts[kWifiProfileCount] = {};
    const size_t attempt_count = BuildWifiAttemptOrder(profiles, profile_count, attempts, kWifiProfileCount);
    for (size_t i = 0; i < attempt_count; ++i) {
        if (ConnectWifiProfile(attempts[i])) {
            return true;
        }
    }
    ESP_LOGW(kTag, "all configured WiFi profiles failed");
    return false;
}

// ===== UI 数据绑定 =====
// 如果只改控件位置，改 dashboard_layout.h；如果改字段显示内容，改这里。
void ApplySnapshot(const DashboardSnapshot &snapshot) {
    lv_label_set_text_fmt(g_ui.time, "%02u:%02u:%02u", snapshot.hour, snapshot.minute, snapshot.second);
    char date_text[18] = {};
    CopyText(date_text, sizeof(date_text), snapshot.date);
    for (size_t i = 0; date_text[i] != '\0'; ++i) {
        if (date_text[i] == ' ') {
            date_text[i] = '_';
        } else if (date_text[i] == '-') {
            date_text[i] = '.';
        }
    }
    lv_label_set_text(g_ui.date, date_text);
    lv_label_set_text_fmt(g_ui.server_target, "ServerMonitor");
    lv_label_set_text(g_ui.server_state_badge, DisplayWord(snapshot.server_state));

    GpuCardSnapshot cards[2] = {};
    for (int i = 0; i < 2; ++i) {
        std::snprintf(cards[i].label, sizeof(cards[i].label), "GPU%u", i);
        if (i < snapshot.gpu_card_count) {
            cards[i] = snapshot.gpu_cards[i];
        }
    }
    const bool has_aggregate_gpu = snapshot.gpu_card_count == 0 &&
                                   std::strcmp(snapshot.server_state, "NO_DATA") != 0 &&
                                   snapshot.server_target[0] != '\0' &&
                                   std::strcmp(snapshot.server_target, "--") != 0;
    if (has_aggregate_gpu) {
        cards[0].util_pct = snapshot.gpu_util_pct;
        cards[0].mem_pct = snapshot.gpu_mem_pct;
        cards[0].temp_c = snapshot.gpu_temp_c;
        cards[0].processes = snapshot.gpu_processes;
        cards[0].valid = true;
    }
    for (int i = 0; i < 2; ++i) {
        lv_label_set_text_fmt(g_ui.gpu_label[i], text::kGpuLabelFormat, i);
        if (cards[i].valid) {
            lv_label_set_text_fmt(g_ui.gpu_temp[i], text::kGpuTempFormat, cards[i].temp_c);
            lv_label_set_text_fmt(g_ui.gpu_util[i], text::kUnsignedValueFormat, cards[i].util_pct);
            lv_label_set_text(g_ui.gpu_pct[i], text::kGpuPercentSign);
            DrawLedRow(&g_ui.gpu_leds[i], cards[i].util_pct);
        } else {
            lv_label_set_text(g_ui.gpu_temp[i], text::kGpuTempPlaceholder);
            lv_label_set_text(g_ui.gpu_util[i], text::kGpuUtilPlaceholder);
            lv_label_set_text(g_ui.gpu_pct[i], text::kGpuPercentSign);
            DrawLedRow(&g_ui.gpu_leds[i], 0);
        }
        lv_label_set_text_fmt(g_ui.vram_label[i], text::kVramLabelFormat, i);
        if (cards[i].valid) {
            lv_label_set_text_fmt(g_ui.vram_pct[i], text::kPercentValueFormat, cards[i].mem_pct);
        } else {
            lv_label_set_text(g_ui.vram_pct[i], text::kPercentPlaceholder);
        }
        DrawSegmentBar(&g_ui.vram_bar[i], cards[i].valid ? cards[i].mem_pct : 0);
        if (cards[i].valid && cards[i].power_w > 0) {
            lv_label_set_text_fmt(g_ui.gpu_power[i], text::kGpuPowerFormat, i, cards[i].power_w);
        } else {
            lv_label_set_text_fmt(g_ui.gpu_power[i], text::kGpuPowerMissingFormat, i);
        }
    }
    lv_label_set_text_fmt(g_ui.server_cpu_value, text::kCpuFormat, snapshot.server_cpu_pct);
    lv_label_set_text_fmt(g_ui.server_mem_value, text::kMemFormat, snapshot.server_mem_pct);
    lv_label_set_text_fmt(g_ui.server_ping_value, text::kProcFormat, snapshot.gpu_processes);

    const bool codex_values_ready = std::strcmp(snapshot.codex_state, "NO_DATA") != 0 &&
                                    std::strcmp(snapshot.codex_state, "VERIFY") != 0;
    lv_label_set_text(g_ui.codex_state, DisplayWord(snapshot.codex_state));
    if (codex_values_ready) {
        lv_label_set_text_fmt(g_ui.codex_week_pct, text::kPercentValueFormat, snapshot.codex_week_remaining_pct);
        lv_label_set_text_fmt(g_ui.codex_five_hour_pct, text::kPercentValueFormat,
                              snapshot.codex_five_hour_remaining_pct);
        DrawSegmentBar(&g_ui.codex_week_bar_custom, snapshot.codex_week_remaining_pct);
        DrawSegmentBar(&g_ui.codex_five_hour_bar_custom, snapshot.codex_five_hour_remaining_pct);
        lv_label_set_text(g_ui.codex_week_reset_value, ResetTimeDetail(snapshot.codex_week_reset_at));
        lv_label_set_text(g_ui.codex_five_hour_reset_value, ResetTimeDetail(snapshot.codex_five_hour_reset_at));
        SetMiniChart(g_ui.codex_week_chart_bars, style::kMiniChartWeekBarCount,
                     snapshot.codex_week_trend, layout::Codex::kTrend0Chart.y + layout::Codex::kTrend0Chart.h - 2);
        SetMiniChartBudgetLine(g_ui.codex_week_budget_line, layout::Codex::kTrend0Chart,
                               snapshot.codex_week_budget_line_pct);
        SetMiniChart(g_ui.codex_five_hour_chart_bars, style::kMiniChartFiveHourBarCount,
                     snapshot.codex_five_hour_trend,
                     layout::Codex::kTrend1Chart.y + layout::Codex::kTrend1Chart.h - 2);
        SetMiniChartBudgetLine(g_ui.codex_five_hour_budget_line, layout::Codex::kTrend1Chart,
                               snapshot.codex_five_hour_budget_line_pct);
    } else {
        uint8_t empty_week_trend[style::kMiniChartWeekBarCount] = {};
        uint8_t empty_five_hour_trend[style::kMiniChartFiveHourBarCount] = {};
        const uint8_t default_budget_line_pct = static_cast<uint8_t>(kMiniChartBudgetLinePct + 0.5);
        lv_label_set_text(g_ui.codex_week_pct, text::kPercentPlaceholder);
        lv_label_set_text(g_ui.codex_five_hour_pct, text::kPercentPlaceholder);
        DrawSegmentBar(&g_ui.codex_week_bar_custom, 0);
        DrawSegmentBar(&g_ui.codex_five_hour_bar_custom, 0);
        lv_label_set_text(g_ui.codex_week_reset_value, text::kBadgePlaceholder);
        lv_label_set_text(g_ui.codex_five_hour_reset_value, text::kBadgePlaceholder);
        SetMiniChart(g_ui.codex_week_chart_bars, style::kMiniChartWeekBarCount,
                     empty_week_trend, layout::Codex::kTrend0Chart.y + layout::Codex::kTrend0Chart.h - 2);
        SetMiniChartBudgetLine(g_ui.codex_week_budget_line, layout::Codex::kTrend0Chart,
                               default_budget_line_pct);
        SetMiniChart(g_ui.codex_five_hour_chart_bars, style::kMiniChartFiveHourBarCount,
                     empty_five_hour_trend, layout::Codex::kTrend1Chart.y + layout::Codex::kTrend1Chart.h - 2);
        SetMiniChartBudgetLine(g_ui.codex_five_hour_budget_line, layout::Codex::kTrend1Chart,
                               default_budget_line_pct);
    }

    char weather_text[64] = {};
    std::snprintf(weather_text, sizeof(weather_text), text::kWeatherFormat,
                  snapshot.weather_location, static_cast<double>(snapshot.weather_temp_c),
                  snapshot.weather_summary);
    lv_label_set_text(g_ui.weather_value, weather_text);

    char indoor_text[24] = {};
    if (!snapshot.indoor_valid) {
        std::snprintf(indoor_text, sizeof(indoor_text), "%s", text::kIndoorMissing);
    } else {
        std::snprintf(indoor_text, sizeof(indoor_text), text::kIndoorFormat,
                      static_cast<double>(snapshot.indoor_temp_c), snapshot.indoor_humidity_pct);
    }
    lv_label_set_text(g_ui.indoor_value, indoor_text);

    char battery_text[16] = {};
    BuildBatteryText(battery_text, sizeof(battery_text));
    if (std::strstr(battery_text, "Battery ") == battery_text) {
        char compact_battery[12] = {};
        size_t write_index = 0;
        const char *value = battery_text + 8;
        for (size_t i = 0; value[i] != '\0' && write_index + 1 < sizeof(compact_battery); ++i) {
            if (value[i] != ' ') {
                compact_battery[write_index++] = value[i];
            }
        }
        lv_label_set_text_fmt(g_ui.footer_battery, text::kFooterBatteryFormat, compact_battery);
    } else {
        lv_label_set_text(g_ui.footer_battery, text::kFooterBatteryPlaceholder);
    }

    char link_text[36] = {};
    CopyText(link_text, sizeof(link_text), snapshot.link_state);
    for (size_t i = 0; link_text[i] != '\0'; ++i) {
        if (link_text[i] == ' ') {
            link_text[i] = '_';
        }
    }
    lv_label_set_text_fmt(g_ui.footer_link, text::kFooterLinkFormat, link_text);
    if (snapshot.updated_at[0] != '\0') {
        BuildUpdatedText(link_text, sizeof(link_text), snapshot.updated_at);
        lv_label_set_text_fmt(g_ui.footer_updated, text::kFooterUpdatedFormat, LastToken(link_text));
    } else {
        lv_label_set_text(g_ui.footer_updated, text::kFooterUpdatedPlaceholder);
    }
}

// UI 主循环：每秒刷新本地时间、电池、SHTC3 和当前快照。
// 使用固定 deadline，并在更新对象后主动刷新 LVGL，
// 避免实际刷屏落到 LVGL 后台任务 500ms 空闲窗口里的随机位置。
// 不在这里做 HTTP 请求，避免网络慢时秒钟和传感器显示卡住。
void DashboardLoopTask(void *arg) {
    (void)arg;
    DashboardSnapshot snapshot = {};
    int64_t next_render_us = esp_timer_get_time();
    for (;;) {
        const int64_t render_start_us = esp_timer_get_time();
        if (render_start_us > next_render_us + kDashboardRenderLateWarnMs * 1000) {
            ESP_LOGW(kTag, "dashboard render late: late=%lldms",
                     (render_start_us - next_render_us) / 1000LL);
        }

        if (!LoadRemoteSnapshot(&snapshot)) {
            BuildEmptySnapshot(&snapshot);
            if (!AnyWifiConfigured()) {
                CopyText(snapshot.link_state, sizeof(snapshot.link_state), "WIFI CFG");
            } else if (!AnyRemoteSourceConfigured()) {
                CopyText(snapshot.link_state, sizeof(snapshot.link_state), "URL CFG");
            } else {
                CopyText(snapshot.link_state, sizeof(snapshot.link_state), "SYNC");
            }
        }

        ApplyCodexFreshnessPolicy(&snapshot);
        UpdateIndoorSnapshot(&snapshot);
        UpdateSnapshotClock(&snapshot);
        const int64_t apply_start_us = esp_timer_get_time();
        if (Lvgl_lock(-1)) {
            ApplySnapshot(snapshot);
            lv_refr_now(nullptr);
            Lvgl_unlock();
        }
        const int64_t apply_elapsed_ms = (esp_timer_get_time() - apply_start_us) / 1000LL;
        if (apply_elapsed_ms > kDashboardRenderSlowWarnMs) {
            ESP_LOGW(kTag, "dashboard render slow: apply=%lldms", apply_elapsed_ms);
        }

        next_render_us += kDashboardRenderIntervalUs;
        const int64_t now_us = esp_timer_get_time();
        if (next_render_us <= now_us) {
            do {
                next_render_us += kDashboardRenderIntervalUs;
            } while (next_render_us <= now_us);
        }
        int64_t delay_ms = (next_render_us - now_us) / 1000LL;
        if (delay_ms < 1) {
            delay_ms = 1;
        }
        vTaskDelay(pdMS_TO_TICKS(static_cast<uint32_t>(delay_ms)));
    }
}

bool TransportReady() {
    if (g_transport_ready && g_wifi_events &&
        (xEventGroupGetBits(g_wifi_events) & kWifiConnectedBit) == 0) {
        g_transport_ready = false;
    }
    return g_transport_ready;
}

void WaitForTransportReady() {
    while (!TransportReady()) {
        vTaskDelay(pdMS_TO_TICKS(kFetchLoopDelayMs));
    }
}

void AdvanceScheduledTime(int64_t *next_us, int64_t interval_ms) {
    const int64_t interval_us = PollIntervalUs(static_cast<int>(interval_ms));
    const int64_t now_us = esp_timer_get_time();
    if (!next_us || interval_us <= 0) {
        return;
    }
    if (*next_us <= 0) {
        *next_us = now_us + interval_us;
        return;
    }
    do {
        *next_us += interval_us;
    } while (*next_us <= now_us);
}

size_t NetworkJobIndex(NetworkJob job) {
    return static_cast<size_t>(job);
}

const char *NetworkJobName(NetworkJob job) {
    switch (job) {
        case NetworkJob::kServerMonitor:
            return "server";
        case NetworkJob::kCodex:
            return "codex";
        case NetworkJob::kWeather:
            return "weather";
        default:
            return "unknown";
    }
}

uint32_t SubmitNetworkJob(NetworkJob job, int64_t due_us, int64_t stale_after_ms) {
    if (!g_network_queue_mutex) {
        return 0;
    }
    if (xSemaphoreTake(g_network_queue_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGW(kTag, "net queue submit skipped: mutex timeout job=%s", NetworkJobName(job));
        DiagnosticLog(NetworkJobName(job), "queue_mutex_timeout", "submit");
        return 0;
    }
    const size_t index = NetworkJobIndex(job);
    NetworkJobRequest &request = g_network_requests[index];
    const bool coalesced = request.pending;
    const uint32_t sequence = ++g_network_sequence;
    const int64_t now_us = esp_timer_get_time();
    if (due_us <= 0) {
        due_us = now_us;
    }
    request.pending = true;
    request.sequence = sequence;
    request.due_us = due_us;
    request.submitted_us = now_us;
    request.stale_after_ms = stale_after_ms;
    g_network_results[index].ready = false;
    xSemaphoreGive(g_network_queue_mutex);
    const int64_t overdue_ms = due_us < now_us ? (now_us - due_us) / 1000LL : 0;
    ESP_LOGI(kTag, "net queue %s: job=%s seq=%u overdue=%lldms stale=%lldms",
             coalesced ? "coalesce" : "submit",
             NetworkJobName(job),
             sequence,
             overdue_ms,
             stale_after_ms);
    if (job != NetworkJob::kServerMonitor) {
        DiagnosticLog(NetworkJobName(job), coalesced ? "queue_coalesce" : "queue_submit",
                      "seq=%u overdue=%lldms stale=%lldms", sequence, overdue_ms, stale_after_ms);
    }
    return sequence;
}

bool TakeNetworkJobResult(NetworkJob job, uint32_t sequence, NetworkJobResult *result) {
    if (!g_network_queue_mutex || sequence == 0 || !result) {
        return false;
    }
    if (xSemaphoreTake(g_network_queue_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return false;
    }
    NetworkJobResult &slot = g_network_results[NetworkJobIndex(job)];
    if (!slot.ready || slot.sequence != sequence) {
        xSemaphoreGive(g_network_queue_mutex);
        return false;
    }
    *result = slot;
    slot.ready = false;
    xSemaphoreGive(g_network_queue_mutex);
    return true;
}

void StoreNetworkJobResult(NetworkJob job,
                           const NetworkJobRequest &request,
                           bool ok,
                           bool usage_summary_ok,
                           int64_t elapsed_ms) {
    if (!g_network_queue_mutex) {
        return;
    }
    if (xSemaphoreTake(g_network_queue_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGW(kTag, "net result dropped: mutex timeout job=%s seq=%u",
                 NetworkJobName(job), request.sequence);
        return;
    }
    NetworkJobResult &result = g_network_results[NetworkJobIndex(job)];
    result.ready = true;
    result.sequence = request.sequence;
    result.ok = ok;
    result.usage_summary_ok = usage_summary_ok;
    result.elapsed_ms = elapsed_ms;
    xSemaphoreGive(g_network_queue_mutex);
}

bool PopNextNetworkJob(NetworkJob *job, NetworkJobRequest *request) {
    if (!g_network_queue_mutex || !job || !request) {
        return false;
    }
    if (xSemaphoreTake(g_network_queue_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return false;
    }
    const int64_t now_us = esp_timer_get_time();
    int best_index = -1;
    int64_t best_due_us = 0;
    for (size_t i = 0; i < NetworkJobIndex(NetworkJob::kCount); ++i) {
        const NetworkJobRequest &candidate = g_network_requests[i];
        if (!candidate.pending || candidate.due_us > now_us) {
            continue;
        }
        if (best_index < 0 || candidate.due_us < best_due_us ||
            (candidate.due_us == best_due_us && i < static_cast<size_t>(best_index))) {
            best_index = static_cast<int>(i);
            best_due_us = candidate.due_us;
        }
    }
    if (best_index < 0) {
        xSemaphoreGive(g_network_queue_mutex);
        return false;
    }

    NetworkJobRequest selected = g_network_requests[best_index];
    const int64_t age_ms = (now_us - selected.submitted_us) / 1000LL;
    g_network_requests[best_index].pending = false;
    xSemaphoreGive(g_network_queue_mutex);

    NetworkJob selected_job = static_cast<NetworkJob>(best_index);
    if (selected.stale_after_ms > 0 && age_ms > selected.stale_after_ms &&
        selected_job != NetworkJob::kServerMonitor) {
        ESP_LOGW(kTag, "net queue drop stale: job=%s seq=%u age=%lldms stale=%lldms",
                 NetworkJobName(selected_job), selected.sequence, age_ms, selected.stale_after_ms);
        DiagnosticLog(NetworkJobName(selected_job), "queue_drop_stale",
                      "seq=%u age=%lldms stale=%lldms", selected.sequence, age_ms, selected.stale_after_ms);
        StoreNetworkJobResult(selected_job, selected, false, false, -1);
        return false;
    }
    if (selected.stale_after_ms > 0 && age_ms > selected.stale_after_ms) {
        ESP_LOGW(kTag, "net queue execute stale realtime job: job=%s seq=%u age=%lldms stale=%lldms",
                 NetworkJobName(selected_job), selected.sequence, age_ms, selected.stale_after_ms);
    }

    *job = selected_job;
    *request = selected;
    return true;
}

// 网络协调循环：先显示 NVS 缓存，再负责 WiFi 初连和断线重连。
// 各远端数据源由独立任务刷新，避免低频慢请求拖慢 ServerMonitor。
void DashboardFetchTask(void *arg) {
    (void)arg;
    DashboardSnapshot cached = {};
    if (!LoadCachedSnapshot(&cached)) {
        BuildEmptySnapshot(&cached);
        CopyText(cached.link_state, sizeof(cached.link_state), "SYNC");
    }
    StoreRemoteSnapshot(cached);
    if (!AnyWifiConfigured()) {
        CopyText(cached.link_state, sizeof(cached.link_state), "WIFI CFG");
        StoreRemoteSnapshot(cached);
        vTaskDelete(nullptr);
        return;
    }
    if (!AnyRemoteSourceConfigured()) {
        CopyText(cached.link_state, sizeof(cached.link_state), "URL CFG");
        StoreRemoteSnapshot(cached);
        vTaskDelete(nullptr);
        return;
    }

    g_transport_ready = StartDashboardWifi();
    if (!g_transport_ready) {
        CopyText(cached.link_state, sizeof(cached.link_state), "WIFI RETRY");
        StoreRemoteSnapshot(cached);
    }

    int64_t next_wifi_retry_us = esp_timer_get_time() + PollIntervalUs(kWifiInitialRetryDelayMs);
    int64_t wifi_retry_delay_ms = kWifiInitialRetryDelayMs;
    for (;;) {
        const int64_t loop_now_us = esp_timer_get_time();
        TransportReady();
        if (!g_transport_ready && loop_now_us >= next_wifi_retry_us) {
            DashboardSnapshot link_state = {};
            BuildEmptySnapshot(&link_state);
            CopyText(link_state.link_state, sizeof(link_state.link_state), "WIFI SCAN");
            MergeSnapshot(link_state, MergeLinkState);
            if (StartDashboardWifi()) {
                g_transport_ready = true;
                wifi_retry_delay_ms = kWifiInitialRetryDelayMs;
                next_wifi_retry_us = loop_now_us + PollIntervalUs(wifi_retry_delay_ms);
                CopyText(link_state.link_state, sizeof(link_state.link_state), "SYNC");
                MergeSnapshot(link_state, MergeLinkState);
            } else {
                CopyText(link_state.link_state, sizeof(link_state.link_state), "WIFI RETRY");
                MergeSnapshot(link_state, MergeLinkState);
                next_wifi_retry_us = loop_now_us + PollIntervalUs(wifi_retry_delay_ms);
                wifi_retry_delay_ms *= 2;
                if (wifi_retry_delay_ms > kWifiMaxRetryDelayMs) {
                    wifi_retry_delay_ms = kWifiMaxRetryDelayMs;
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(kFetchLoopDelayMs));
    }
}

bool ExecuteNetworkJob(NetworkJob job, int64_t *elapsed_ms, bool *usage_summary_ok) {
    if (elapsed_ms) {
        *elapsed_ms = -1;
    }
    if (usage_summary_ok) {
        *usage_summary_ok = false;
    }

    DashboardSnapshot next = {};
    if (!LoadRemoteSnapshot(&next)) {
        BuildEmptySnapshot(&next);
    }

    switch (job) {
        case NetworkJob::kServerMonitor: {
            int64_t server_elapsed_ms = -1;
            const bool ok = FetchServerMonitorSnapshot(&next, &server_elapsed_ms);
            if (elapsed_ms) {
                *elapsed_ms = server_elapsed_ms;
            }
            if (ok) {
                MergeSnapshot(next, MergeServerMonitorSnapshot);
            } else {
                CopyText(next.link_state, sizeof(next.link_state), "NET SLOW");
                MergeSnapshot(next, MergeLinkState);
            }
            return ok;
        }
        case NetworkJob::kCodex: {
            bool summary_ok = false;
            const bool ok = FetchCodexSnapshot(&next, &summary_ok);
            if (usage_summary_ok) {
                *usage_summary_ok = summary_ok;
            }
            if (ok) {
                MergeSnapshot(next, MergeCodexSnapshot);
            } else {
                if (CodexSnapshotExpired(next)) {
                    ClearCodexQuota(&next);
                } else {
                    MarkCodexQuotaStale(&next);
                }
                MergeSnapshot(next, MergeCodexSnapshot);
                if (!ServerMonitorConfigured()) {
                    MergeSnapshot(next, MergeLinkState);
                }
            }
            return ok;
        }
        case NetworkJob::kWeather:
            if (FetchWeatherSnapshot(&next)) {
                MergeSnapshot(next, MergeWeatherSnapshot);
                return true;
            }
            return false;
        default:
            return false;
    }
}

void NetworkWorkerTask(void *arg) {
    (void)arg;
    for (;;) {
        WaitForTransportReady();

        NetworkJob job = NetworkJob::kServerMonitor;
        NetworkJobRequest request = {};
        if (!PopNextNetworkJob(&job, &request)) {
            vTaskDelay(pdMS_TO_TICKS(kNetworkWorkerIdleDelayMs));
            continue;
        }

        const int64_t queue_age_ms = (esp_timer_get_time() - request.submitted_us) / 1000LL;
        ESP_LOGI(kTag, "net worker start: job=%s seq=%u queue_age=%lldms",
                 NetworkJobName(job), request.sequence, queue_age_ms);

        int64_t source_elapsed_ms = -1;
        bool usage_summary_ok = false;
        const int64_t start_us = esp_timer_get_time();
        const bool ok = ExecuteNetworkJob(job, &source_elapsed_ms, &usage_summary_ok);
        const int64_t total_elapsed_ms = (esp_timer_get_time() - start_us) / 1000LL;
        if (source_elapsed_ms < 0) {
            source_elapsed_ms = total_elapsed_ms;
        }
        ESP_LOGI(kTag,
                 "net worker finish: job=%s seq=%u ok=%d elapsed=%lldms total=%lldms usage_summary=%d",
                 NetworkJobName(job), request.sequence, ok ? 1 : 0,
                 source_elapsed_ms, total_elapsed_ms, usage_summary_ok ? 1 : 0);
        DiagnosticRecordWorker(job, ok, usage_summary_ok, source_elapsed_ms);
        StoreNetworkJobResult(job, request, ok, usage_summary_ok, source_elapsed_ms);
    }
}

void PublishLinkState(const char *link_state) {
    DashboardSnapshot snapshot = {};
    if (!LoadRemoteSnapshot(&snapshot)) {
        BuildEmptySnapshot(&snapshot);
    }
    CopyText(snapshot.link_state, sizeof(snapshot.link_state), link_state);
    MergeSnapshot(snapshot, MergeLinkState);
}

void NetworkSchedulerTask(void *arg) {
    (void)arg;
    int64_t next_server_monitor_us = 0;
    int64_t next_codex_us = 0;
    int64_t next_weather_us = 0;
    int64_t server_monitor_interval_ms = CONFIG_DASHBOARD_SERVER_MONITOR_POLL_INTERVAL_MS;
    int server_monitor_failures = 0;
    int server_monitor_fast_successes = 0;
    int server_monitor_slow_score = 0;
    int server_monitor_very_slow_streak = 0;
    bool server_monitor_degraded = false;
    bool low_source_deadlines_ready = false;
    uint32_t server_monitor_sequence = 0;
    uint32_t codex_sequence = 0;
    uint32_t weather_sequence = 0;

    for (;;) {
        WaitForTransportReady();
        const int64_t now_us = esp_timer_get_time();
        if (!low_source_deadlines_ready) {
            next_codex_us = now_us + PollIntervalUs(kCodexInitialDelayMs);
            next_weather_us = now_us + PollIntervalUs(kWeatherInitialDelayMs);
            low_source_deadlines_ready = true;
            ESP_LOGI(kTag, "net scheduler low sources armed: codex_delay=%lldms weather_delay=%lldms",
                     kCodexInitialDelayMs, kWeatherInitialDelayMs);
        }

        if (server_monitor_sequence != 0) {
            NetworkJobResult result = {};
            if (TakeNetworkJobResult(NetworkJob::kServerMonitor, server_monitor_sequence, &result)) {
                server_monitor_sequence = 0;
                if (result.ok) {
                    server_monitor_failures = 0;
                    if (result.elapsed_ms > kServerMonitorSlowThresholdMs) {
                        server_monitor_fast_successes = 0;
                        if (server_monitor_slow_score < kServerMonitorSlowScoreToDegrade) {
                            ++server_monitor_slow_score;
                        }
                        if (result.elapsed_ms > kServerMonitorVerySlowThresholdMs) {
                            ++server_monitor_very_slow_streak;
                        } else {
                            server_monitor_very_slow_streak = 0;
                        }
                        if (server_monitor_slow_score >= kServerMonitorSlowScoreToDegrade ||
                            server_monitor_very_slow_streak >= kServerMonitorVerySlowStreakToDegrade) {
                            server_monitor_interval_ms = kServerMonitorAdaptiveSlowMs;
                            server_monitor_degraded = true;
                        }
                    } else if (result.elapsed_ms >= 0 &&
                               result.elapsed_ms < kServerMonitorFastThresholdMs) {
                        if (server_monitor_slow_score > 0) {
                            --server_monitor_slow_score;
                        }
                        server_monitor_very_slow_streak = 0;
                        ++server_monitor_fast_successes;
                        if (server_monitor_fast_successes >= 3) {
                            server_monitor_interval_ms = CONFIG_DASHBOARD_SERVER_MONITOR_POLL_INTERVAL_MS;
                            server_monitor_degraded = false;
                        }
                    } else {
                        server_monitor_fast_successes = 0;
                        server_monitor_very_slow_streak = 0;
                    }
                    if (server_monitor_degraded) {
                        PublishLinkState("NET SLOW");
                    }
                } else {
                    ++server_monitor_failures;
                    server_monitor_fast_successes = 0;
                    server_monitor_slow_score = kServerMonitorSlowScoreToDegrade;
                    server_monitor_very_slow_streak = kServerMonitorVerySlowStreakToDegrade;
                    server_monitor_interval_ms = server_monitor_failures >= 3
                                                     ? kServerMonitorAdaptiveMaxMs
                                                     : kServerMonitorAdaptiveFailMs;
                    server_monitor_degraded = true;
                    PublishLinkState("NET SLOW");
                }
                AdvanceScheduledTime(&next_server_monitor_us, server_monitor_interval_ms);
            }
        }
        if (codex_sequence != 0) {
            NetworkJobResult result = {};
            if (TakeNetworkJobResult(NetworkJob::kCodex, codex_sequence, &result)) {
                codex_sequence = 0;
                if (result.ok && result.usage_summary_ok) {
                    AdvanceScheduledTime(&next_codex_us, CONFIG_DASHBOARD_CODEX_POLL_INTERVAL_MS);
                } else {
                    if (result.ok) {
                        ESP_LOGW(kTag, "Codex usage-summary incomplete, retrying soon");
                    }
                    next_codex_us = esp_timer_get_time() + PollIntervalUs(kCodexRetryDelayMs);
                }
            }
        }
        if (weather_sequence != 0) {
            NetworkJobResult result = {};
            if (TakeNetworkJobResult(NetworkJob::kWeather, weather_sequence, &result)) {
                weather_sequence = 0;
                if (result.ok) {
                    AdvanceScheduledTime(&next_weather_us, CONFIG_DASHBOARD_WEATHER_POLL_INTERVAL_MS);
                } else {
                    next_weather_us = esp_timer_get_time() + PollIntervalUs(kWeatherRetryDelayMs);
                }
            }
        }

        if (ServerMonitorConfigured() && server_monitor_sequence == 0 &&
            (next_server_monitor_us <= 0 || now_us >= next_server_monitor_us)) {
            const int64_t due_us = next_server_monitor_us > 0 ? next_server_monitor_us : now_us;
            server_monitor_sequence = SubmitNetworkJob(NetworkJob::kServerMonitor, due_us, kServerMonitorQueueStaleMs);
        }
        if (CodexApiTouched() && codex_sequence == 0 && now_us >= next_codex_us) {
            codex_sequence = SubmitNetworkJob(NetworkJob::kCodex, next_codex_us, kCodexQueueStaleMs);
        }
        if (std::strlen(CONFIG_DASHBOARD_WEATHER_URL) > 0 && weather_sequence == 0 && now_us >= next_weather_us) {
            weather_sequence = SubmitNetworkJob(NetworkJob::kWeather, next_weather_us, kWeatherQueueStaleMs);
        }

        vTaskDelay(pdMS_TO_TICKS(kNetworkWorkerIdleDelayMs));
    }
}

// ===== 硬件初始化 =====
void InitNvs() {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "nvs init failed: %s", esp_err_to_name(err));
        return;
    }
    g_nvs_ready = true;
}

void InitBatteryAdc() {
    adc_cali_curve_fitting_config_t cali_config = {};
    cali_config.unit_id = ADC_UNIT_1;
    cali_config.atten = ADC_ATTEN_DB_12;
    cali_config.bitwidth = ADC_BITWIDTH_12;
    esp_err_t err = adc_cali_create_scheme_curve_fitting(&cali_config, &g_battery_adc_cali);
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "battery ADC calibration init failed: %s", esp_err_to_name(err));
        g_battery_adc_cali = nullptr;
    }

    adc_oneshot_unit_init_cfg_t unit_config = {};
    unit_config.unit_id = ADC_UNIT_1;
    err = adc_oneshot_new_unit(&unit_config, &g_battery_adc);
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "battery ADC unit init failed: %s", esp_err_to_name(err));
        if (g_battery_adc_cali) {
            adc_cali_delete_scheme_curve_fitting(g_battery_adc_cali);
            g_battery_adc_cali = nullptr;
        }
        return;
    }

    adc_oneshot_chan_cfg_t channel_config = {};
    channel_config.bitwidth = ADC_BITWIDTH_12;
    channel_config.atten = ADC_ATTEN_DB_12;
    err = adc_oneshot_config_channel(g_battery_adc, ADC_CHANNEL_3, &channel_config);
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "battery ADC channel init failed: %s", esp_err_to_name(err));
        adc_oneshot_del_unit(g_battery_adc);
        g_battery_adc = nullptr;
        if (g_battery_adc_cali) {
            adc_cali_delete_scheme_curve_fitting(g_battery_adc_cali);
            g_battery_adc_cali = nullptr;
        }
        return;
    }
    g_battery_adc_ready = true;
}

void InitShtc3() {
    i2c_master_bus_config_t bus_config = {};
    bus_config.clk_source = I2C_CLK_SRC_DEFAULT;
    bus_config.i2c_port = I2C_NUM_0;
    bus_config.scl_io_num = GPIO_NUM_14;
    bus_config.sda_io_num = GPIO_NUM_13;
    bus_config.glitch_ignore_cnt = 7;
    bus_config.flags.enable_internal_pullup = true;
    esp_err_t err = i2c_new_master_bus(&bus_config, &g_i2c_bus);
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "SHTC3 I2C bus init failed: %s", esp_err_to_name(err));
        return;
    }

    i2c_device_config_t device_config = {};
    device_config.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    device_config.device_address = kShtc3Address;
    device_config.scl_speed_hz = 400000;
    err = i2c_master_bus_add_device(g_i2c_bus, &device_config, &g_shtc3_device);
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "SHTC3 device init failed: %s", esp_err_to_name(err));
        return;
    }

    Shtc3WriteCommand(kShtc3Wakeup);
    Shtc3WriteCommand(kShtc3SoftReset);
    vTaskDelay(pdMS_TO_TICKS(20));
    const uint8_t id_command[2] = {
        static_cast<uint8_t>(kShtc3ReadId >> 8),
        static_cast<uint8_t>(kShtc3ReadId & 0xFF),
    };
    uint8_t id_response[3] = {};
    err = i2c_master_transmit_receive(g_shtc3_device, id_command, sizeof(id_command), id_response, sizeof(id_response),
                                      pdMS_TO_TICKS(100));
    if (err != ESP_OK || !Shtc3CheckCrc(id_response, 2, id_response[2])) {
        ESP_LOGW(kTag, "SHTC3 ID read failed: %s", esp_err_to_name(err));
        return;
    }
    const uint16_t sensor_id = static_cast<uint16_t>((id_response[0] << 8) | id_response[1]);
    ESP_LOGI(kTag, "SHTC3 ready, id=0x%04x", sensor_id);
    Shtc3WriteCommand(kShtc3Sleep);
    g_shtc3_ready = true;
}

void StartDashboardTask(TaskFunction_t task, const char *name, uint32_t stack_words, UBaseType_t priority,
                        BaseType_t core_id) {
    const BaseType_t result = xTaskCreatePinnedToCore(task, name, stack_words, nullptr, priority, nullptr, core_id);
    if (result != pdPASS) {
        ESP_LOGE(kTag, "task start failed: %s", name);
    }
}

}  // 匿名命名空间

namespace {

size_t AppendFormat(char *dest, size_t dest_size, size_t used, const char *format, ...) {
    if (!dest || dest_size == 0 || used >= dest_size) {
        return used;
    }
    va_list args;
    va_start(args, format);
    const int written = std::vsnprintf(dest + used, dest_size - used, format, args);
    va_end(args);
    if (written < 0) {
        return used;
    }
    const size_t available = dest_size - used;
    const size_t advance = static_cast<size_t>(written);
    return used + (advance < available ? advance : available - 1);
}

size_t AppendJsonString(char *dest, size_t dest_size, size_t used, const char *value) {
    used = AppendFormat(dest, dest_size, used, "\"");
    for (const char *cursor = value ? value : ""; *cursor != '\0' && used + 2 < dest_size; ++cursor) {
        const char ch = *cursor;
        if (ch == '"' || ch == '\\') {
            used = AppendFormat(dest, dest_size, used, "\\%c", ch);
        } else if (ch == '\n') {
            used = AppendFormat(dest, dest_size, used, "\\n");
        } else if (ch == '\r') {
            used = AppendFormat(dest, dest_size, used, "\\r");
        } else if (static_cast<unsigned char>(ch) < 0x20) {
            used = AppendFormat(dest, dest_size, used, "?");
        } else {
            used = AppendFormat(dest, dest_size, used, "%c", ch);
        }
    }
    return AppendFormat(dest, dest_size, used, "\"");
}

}  // namespace

size_t UserApp_WriteDiagnosticsJson(char *dest, size_t dest_size) {
    if (!dest || dest_size == 0) {
        return 0;
    }
    dest[0] = '\0';
    DiagnosticState diagnostic = {};
    DashboardSnapshot remote = {};
    const bool remote_valid = LoadRemoteSnapshot(&remote);
    if (g_diagnostic_mutex &&
        xSemaphoreTake(g_diagnostic_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        diagnostic = g_diagnostics;
        xSemaphoreGive(g_diagnostic_mutex);
    }

    size_t used = 0;
    used = AppendFormat(dest, dest_size, used,
                        "{\n  \"uptime_ms\":%lld,\n  \"epoch\":%lld,\n  \"transport_ready\":%s,\n"
                        "  \"config\":{\"codex_url\":%s,\"codex_api_key\":%s,\"cf_access\":%s,\"server_monitor\":%s},\n"
                        "  \"snapshot_valid\":%s,\n",
                        esp_timer_get_time() / 1000LL,
                        CurrentEpochSeconds(),
                        TransportReady() ? "true" : "false",
                        std::strlen(CONFIG_DASHBOARD_API_URL) > 0 ? "true" : "false",
                        std::strlen(CONFIG_DASHBOARD_API_KEY) > 0 ? "true" : "false",
                        (std::strlen(CONFIG_DASHBOARD_CF_ACCESS_CLIENT_ID) > 0 &&
                         std::strlen(CONFIG_DASHBOARD_CF_ACCESS_CLIENT_SECRET) > 0) ? "true" : "false",
                        ServerMonitorConfigured() ? "true" : "false",
                        remote_valid ? "true" : "false");
    used = AppendFormat(dest, dest_size, used,
                        "  \"codex\":{\"fetch_ok\":%s,\"usage_summary_ok\":%s,\"json_ok\":%s,\"state\":",
                        diagnostic.last_codex_fetch_ok ? "true" : "false",
                        diagnostic.last_codex_usage_summary_ok ? "true" : "false",
                        diagnostic.last_codex_json_ok ? "true" : "false");
    used = AppendJsonString(dest, dest_size, used, diagnostic.last_codex_state);
    used = AppendFormat(dest, dest_size, used, ",\"link_state\":");
    used = AppendJsonString(dest, dest_size, used, diagnostic.last_codex_link_state);
    used = AppendFormat(dest, dest_size, used,
                        ",\"week_remaining_pct\":%d,\"five_hour_remaining_pct\":%d,"
                        "\"received_epoch\":%lld,\"expired\":%s,\"error\":",
                        diagnostic.last_codex_week_remaining_pct,
                        diagnostic.last_codex_five_hour_remaining_pct,
                        diagnostic.last_codex_received_epoch,
                        remote_valid && CodexSnapshotExpired(remote) ? "true" : "false");
    used = AppendJsonString(dest, dest_size, used, diagnostic.last_codex_error);
    used = AppendFormat(dest, dest_size, used, "},\n  \"http\":{\"source\":");
    used = AppendJsonString(dest, dest_size, used, diagnostic.last_http_source);
    used = AppendFormat(dest, dest_size, used, ",\"result\":");
    used = AppendJsonString(dest, dest_size, used, diagnostic.last_http_result);
    used = AppendFormat(dest, dest_size, used,
                        ",\"status\":%d,\"len\":%d,\"overflow\":%s,\"elapsed_ms\":%lld,"
                        "\"errno\":%d,\"tls_error\":%d,\"tls_flags\":%d},\n",
                        diagnostic.last_http_status,
                        diagnostic.last_http_len,
                        diagnostic.last_http_overflow ? "true" : "false",
                        diagnostic.last_http_elapsed_ms,
                        diagnostic.last_http_errno,
                        diagnostic.last_http_tls_error,
                        diagnostic.last_http_tls_flags);
    used = AppendFormat(dest, dest_size, used, "  \"worker\":{\"job\":");
    used = AppendJsonString(dest, dest_size, used, diagnostic.last_worker_job);
    used = AppendFormat(dest, dest_size, used,
                        ",\"ok\":%s,\"usage_summary_ok\":%s,\"elapsed_ms\":%lld},\n  \"logs\":[\n",
                        diagnostic.last_worker_ok ? "true" : "false",
                        diagnostic.last_worker_usage_summary_ok ? "true" : "false",
                        diagnostic.last_worker_elapsed_ms);

    const size_t capacity = sizeof(diagnostic.entries) / sizeof(diagnostic.entries[0]);
    const uint32_t count = diagnostic.total_entries < capacity ? diagnostic.total_entries : capacity;
    const uint32_t start_sequence = diagnostic.next_sequence >= count ? diagnostic.next_sequence - count + 1 : 1;
    for (uint32_t offset = 0; offset < count; ++offset) {
        const uint32_t sequence = start_sequence + offset;
        const DiagnosticEntry &entry = diagnostic.entries[(sequence - 1) % capacity];
        used = AppendFormat(dest, dest_size, used, "    {\"seq\":%u,\"uptime_ms\":%lld,\"source\":",
                            entry.sequence, entry.uptime_ms);
        used = AppendJsonString(dest, dest_size, used, entry.source);
        used = AppendFormat(dest, dest_size, used, ",\"event\":");
        used = AppendJsonString(dest, dest_size, used, entry.event);
        used = AppendFormat(dest, dest_size, used, ",\"detail\":");
        used = AppendJsonString(dest, dest_size, used, entry.detail);
        used = AppendFormat(dest, dest_size, used, "}%s\n", offset + 1 < count ? "," : "");
    }
    used = AppendFormat(dest, dest_size, used, "  ]\n}\n");
    return used;
}

size_t UserApp_WriteLogsText(char *dest, size_t dest_size) {
    if (!dest || dest_size == 0) {
        return 0;
    }
    dest[0] = '\0';
    DiagnosticState diagnostic = {};
    if (g_diagnostic_mutex &&
        xSemaphoreTake(g_diagnostic_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        diagnostic = g_diagnostics;
        xSemaphoreGive(g_diagnostic_mutex);
    }

    size_t used = 0;
    used = AppendFormat(dest, dest_size, used,
                        "InfoDashboard diagnostics\n"
                        "uptime_ms=%lld epoch=%lld transport_ready=%d\n"
                        "codex fetch_ok=%d usage_summary_ok=%d json_ok=%d state=%s week=%d five_hour=%d received_epoch=%lld err=%s\n"
                        "http source=%s result=%s status=%d len=%d overflow=%d elapsed_ms=%lld errno=%d tls_error=%d tls_flags=%d\n"
                        "worker job=%s ok=%d usage_summary_ok=%d elapsed_ms=%lld\n\nRecent logs:\n",
                        esp_timer_get_time() / 1000LL,
                        CurrentEpochSeconds(),
                        TransportReady() ? 1 : 0,
                        diagnostic.last_codex_fetch_ok ? 1 : 0,
                        diagnostic.last_codex_usage_summary_ok ? 1 : 0,
                        diagnostic.last_codex_json_ok ? 1 : 0,
                        diagnostic.last_codex_state,
                        diagnostic.last_codex_week_remaining_pct,
                        diagnostic.last_codex_five_hour_remaining_pct,
                        diagnostic.last_codex_received_epoch,
                        diagnostic.last_codex_error,
                        diagnostic.last_http_source,
                        diagnostic.last_http_result,
                        diagnostic.last_http_status,
                        diagnostic.last_http_len,
                        diagnostic.last_http_overflow ? 1 : 0,
                        diagnostic.last_http_elapsed_ms,
                        diagnostic.last_http_errno,
                        diagnostic.last_http_tls_error,
                        diagnostic.last_http_tls_flags,
                        diagnostic.last_worker_job,
                        diagnostic.last_worker_ok ? 1 : 0,
                        diagnostic.last_worker_usage_summary_ok ? 1 : 0,
                        diagnostic.last_worker_elapsed_ms);
    const size_t capacity = sizeof(diagnostic.entries) / sizeof(diagnostic.entries[0]);
    const uint32_t count = diagnostic.total_entries < capacity ? diagnostic.total_entries : capacity;
    const uint32_t start_sequence = diagnostic.next_sequence >= count ? diagnostic.next_sequence - count + 1 : 1;
    for (uint32_t offset = 0; offset < count; ++offset) {
        const uint32_t sequence = start_sequence + offset;
        const DiagnosticEntry &entry = diagnostic.entries[(sequence - 1) % capacity];
        used = AppendFormat(dest, dest_size, used, "#%u %lldms %-7s %-18s %s\n",
                            entry.sequence, entry.uptime_ms, entry.source, entry.event, entry.detail);
    }
    return used;
}

void UserApp_AppInit(void) {
    ESP_LOGI(kTag, "info dashboard app init");
    InitNvs();
    g_snapshot_mutex = xSemaphoreCreateMutex();
    if (!g_snapshot_mutex) {
        ESP_LOGW(kTag, "snapshot mutex init failed");
    }
    g_http_mutex = xSemaphoreCreateMutex();
    if (!g_http_mutex) {
        ESP_LOGW(kTag, "HTTP mutex init failed");
    }
    g_network_queue_mutex = xSemaphoreCreateMutex();
    if (!g_network_queue_mutex) {
        ESP_LOGW(kTag, "network queue mutex init failed");
    }
    g_diagnostic_mutex = xSemaphoreCreateMutex();
    if (!g_diagnostic_mutex) {
        ESP_LOGW(kTag, "diagnostic mutex init failed");
    }
    InitBatteryAdc();
    InitShtc3();
}

// UI 创建入口。创建顺序按画面结构组织：
// 顶栏 -> 服务器卡 -> Codex 卡 -> 辅助信息/页脚。
// 新增元素时通常需要三步：
// 1. dashboard_layout.h 增加 Rect；
// 2. DashboardUi 增加控件指针；
// 3. 这里创建控件，并在 ApplySnapshot() 里赋值。
void UserApp_UiInit(void) {
    g_ui.screen = lv_obj_create(nullptr);
    lv_obj_set_size(g_ui.screen, layout::Screen::kWidth, layout::Screen::kHeight);
    lv_obj_clear_flag(g_ui.screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(g_ui.screen, style::Paper(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(g_ui.screen, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(g_ui.screen, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(g_ui.screen, 0, LV_PART_MAIN);

    g_ui.header = CreateFill(g_ui.screen, layout::Header::kBar, style::Ink());
    CreateText(g_ui.header, text::kHeaderTitle, layout::Header::kTitle, style::FontFusion12(), style::Paper(), LV_TEXT_ALIGN_LEFT);
    g_ui.date = CreateText(g_ui.header, text::kHeaderDatePlaceholder, layout::Header::kDate, style::FontFusion12(), style::Paper(), LV_TEXT_ALIGN_CENTER);
    g_ui.time = CreateText(g_ui.header, text::kHeaderTimePlaceholder, layout::Header::kTime, style::FontConsolasRegular14(),
                           style::Paper(), LV_TEXT_ALIGN_RIGHT);

    g_ui.server_panel = CreateThinPanel(g_ui.screen, layout::Server::kPanel);
    g_ui.server_target = CreateLabel(g_ui.server_panel, text::kServerTargetPlaceholder, layout::Server::kTarget, style::FontSegoeSemibold16(), LV_TEXT_ALIGN_LEFT);
    g_ui.server_state_badge = CreateBadgeSmall(g_ui.server_panel, text::kBadgePlaceholder, layout::Server::kStateBadge);
    CreateFill(g_ui.server_panel, layout::Server::kHeaderRule, style::Ink());
    CreateDashedRuleY(g_ui.server_panel, layout::Server::kGpuDividerY);
    CreateDashedRule(g_ui.server_panel, layout::Server::kDividerTop);
    CreateDashedRule(g_ui.server_panel, layout::Server::kDividerBottom);

    constexpr layout::Rect kGpuLabelRects[2] = {layout::Server::kGpu0Label, layout::Server::kGpu1Label};
    constexpr layout::Rect kGpuTempRects[2] = {layout::Server::kGpu0Temp, layout::Server::kGpu1Temp};
    constexpr layout::Rect kGpuUtilRects[2] = {layout::Server::kGpu0Util, layout::Server::kGpu1Util};
    constexpr layout::Rect kGpuPctRects[2] = {layout::Server::kGpu0Pct, layout::Server::kGpu1Pct};
    constexpr layout::Rect kGpuCellRects[2] = {layout::Server::kGpu0Cells, layout::Server::kGpu1Cells};
    for (int i = 0; i < 2; ++i) {
        g_ui.gpu_label[i] = CreateLabel(g_ui.server_panel, text::kGpuLabelPlaceholder, kGpuLabelRects[i], style::FontFusion10(), LV_TEXT_ALIGN_LEFT);
        g_ui.gpu_temp[i] = CreateLabel(g_ui.server_panel, text::kGpuTempPlaceholder, kGpuTempRects[i], style::FontFusion10(), LV_TEXT_ALIGN_RIGHT);
        g_ui.gpu_util[i] = CreateLabel(g_ui.server_panel, text::kGpuUtilPlaceholder, kGpuUtilRects[i], style::FontSegoe32(), LV_TEXT_ALIGN_RIGHT);
        g_ui.gpu_pct[i] = CreateLabel(g_ui.server_panel, text::kGpuPercentSign, kGpuPctRects[i], style::FontSegoeSemibold16(), LV_TEXT_ALIGN_LEFT);
        CreateLedRow(g_ui.server_panel, &g_ui.gpu_leds[i], kGpuCellRects[i]);
    }

    constexpr layout::Rect kVramLabelRects[2] = {layout::Server::kVram0Label, layout::Server::kVram1Label};
    constexpr layout::Rect kVramPctRects[2] = {layout::Server::kVram0Pct, layout::Server::kVram1Pct};
    constexpr layout::Rect kVramBarRects[2] = {layout::Server::kVram0Bar, layout::Server::kVram1Bar};
    for (int i = 0; i < 2; ++i) {
        g_ui.vram_label[i] = CreateLabel(g_ui.server_panel, text::kVramLabelPlaceholder, kVramLabelRects[i], style::FontFusion10(), LV_TEXT_ALIGN_LEFT);
        g_ui.vram_pct[i] = CreateLabel(g_ui.server_panel, text::kPercentPlaceholder, kVramPctRects[i], style::FontFusion10(), LV_TEXT_ALIGN_RIGHT);
        CreateSegmentBar(g_ui.server_panel, &g_ui.vram_bar[i], kVramBarRects[i]);
    }
    g_ui.gpu_power[0] = CreateLabel(g_ui.server_panel, text::kGpuPower0Placeholder, layout::Server::kPower0, style::FontFusion12(), LV_TEXT_ALIGN_LEFT);
    g_ui.gpu_power[1] = CreateLabel(g_ui.server_panel, text::kGpuPower1Placeholder, layout::Server::kPower1, style::FontFusion12(), LV_TEXT_ALIGN_RIGHT);
    g_ui.server_cpu_value = CreateLabel(g_ui.server_panel, text::kCpuPlaceholder, layout::Server::kCpu, style::FontFusion12(), LV_TEXT_ALIGN_LEFT);
    g_ui.server_mem_value = CreateLabel(g_ui.server_panel, text::kMemPlaceholder, layout::Server::kMem, style::FontFusion12(), LV_TEXT_ALIGN_LEFT);
    g_ui.server_ping_value = CreateLabel(g_ui.server_panel, text::kProcPlaceholder, layout::Server::kProc, style::FontFusion12(), LV_TEXT_ALIGN_RIGHT);

    g_ui.codex_panel = CreateThinPanel(g_ui.screen, layout::Codex::kPanel);
    CreateLabel(g_ui.codex_panel, text::kCodexTitle, layout::Codex::kTitle, style::FontSegoeSemibold16(), LV_TEXT_ALIGN_LEFT);
    g_ui.codex_state = CreateBadgeSmall(g_ui.codex_panel, text::kBadgePlaceholder, layout::Codex::kState);
    CreateFill(g_ui.codex_panel, layout::Codex::kHeaderRule, style::Ink());
    CreateLabel(g_ui.codex_panel, text::kTrend7Day, layout::Codex::kTrend0Label, style::FontFusion10(), LV_TEXT_ALIGN_LEFT);
    CreateLabel(g_ui.codex_panel, text::kTrend5Hour, layout::Codex::kTrend1Label, style::FontFusion10(), LV_TEXT_ALIGN_LEFT);
    g_ui.codex_week_budget_line = CreateMiniChartBudgetLine(g_ui.codex_panel, layout::Codex::kTrend0Chart);
    g_ui.codex_five_hour_budget_line = CreateMiniChartBudgetLine(g_ui.codex_panel, layout::Codex::kTrend1Chart);
    CreateMiniChart(g_ui.codex_panel, g_ui.codex_week_chart_bars, style::kMiniChartWeekBarCount,
                    layout::Codex::kTrend0Chart);
    CreateMiniChart(g_ui.codex_panel, g_ui.codex_five_hour_chart_bars, style::kMiniChartFiveHourBarCount,
                    layout::Codex::kTrend1Chart);
    CreateDashedRuleY(g_ui.codex_panel, layout::Codex::kTrendDividerY);
    CreateDashedRule(g_ui.codex_panel, layout::Codex::kDividerTop);
    CreateLabel(g_ui.codex_panel, text::kWeekQuotaLabel, layout::Codex::kWeekLabel, style::FontFusion10(), LV_TEXT_ALIGN_LEFT);
    g_ui.codex_week_pct = CreateLabel(g_ui.codex_panel, text::kPercentPlaceholder, layout::Codex::kWeekPct, style::FontFusion10(), LV_TEXT_ALIGN_RIGHT);
    CreateSegmentBar(g_ui.codex_panel, &g_ui.codex_week_bar_custom, layout::Codex::kWeekBar);
    CreateLabel(g_ui.codex_panel, text::kFiveHourQuotaLabel, layout::Codex::kFiveHourLabel, style::FontFusion10(), LV_TEXT_ALIGN_LEFT);
    g_ui.codex_five_hour_pct = CreateLabel(g_ui.codex_panel, text::kPercentPlaceholder, layout::Codex::kFiveHourPct, style::FontFusion10(), LV_TEXT_ALIGN_RIGHT);
    CreateSegmentBar(g_ui.codex_panel, &g_ui.codex_five_hour_bar_custom, layout::Codex::kFiveHourBar);
    CreateDashedRule(g_ui.codex_panel, layout::Codex::kDividerBottom);
    CreateLabel(g_ui.codex_panel, text::kWeekResetLabel, layout::Codex::kWeekResetLabel, style::FontFusion12(), LV_TEXT_ALIGN_LEFT);
    g_ui.codex_week_reset_value = CreateLabel(g_ui.codex_panel, text::kBadgePlaceholder, layout::Codex::kWeekResetValue, style::FontFusion12(), LV_TEXT_ALIGN_RIGHT);
    CreateLabel(g_ui.codex_panel, text::kFiveHourResetLabel, layout::Codex::kFiveHourResetLabel, style::FontFusion12(), LV_TEXT_ALIGN_LEFT);
    g_ui.codex_five_hour_reset_value = CreateLabel(g_ui.codex_panel, text::kBadgePlaceholder, layout::Codex::kFiveHourResetValue, style::FontFusion12(), LV_TEXT_ALIGN_RIGHT);

    CreateFill(g_ui.screen, layout::Aux::kAuxRule, style::Ink());
    g_ui.weather_value = CreateLabel(g_ui.screen, text::kWeatherPlaceholder, layout::Aux::kWeatherValue, style::FontFusion12(), LV_TEXT_ALIGN_LEFT);
    g_ui.indoor_value = CreateLabel(g_ui.screen, text::kIndoorPlaceholder, layout::Aux::kIndoorValue, style::FontFusion12(), LV_TEXT_ALIGN_RIGHT);
    g_ui.footer_bar = CreateFill(g_ui.screen, layout::Aux::kFooterBar, style::Ink());
    lv_obj_set_style_radius(g_ui.footer_bar, style::kFooterBarRadius, LV_PART_MAIN);
    g_ui.footer_link = CreateText(g_ui.screen, text::kFooterLinkPlaceholder, layout::Aux::kFooterLink, style::FontFusion12(), style::Paper(), LV_TEXT_ALIGN_LEFT);
    g_ui.footer_battery = CreateText(g_ui.screen, text::kFooterBatteryPlaceholder, layout::Aux::kFooterBattery, style::FontFusion12(), style::Paper(), LV_TEXT_ALIGN_CENTER);
    g_ui.footer_updated = CreateText(g_ui.screen, text::kFooterUpdatedPlaceholder, layout::Aux::kFooterUpdated, style::FontFusion12(), style::Paper(), LV_TEXT_ALIGN_RIGHT);

    DashboardSnapshot snapshot = {};
    BuildEmptySnapshot(&snapshot);
    ApplySnapshot(snapshot);
    lv_screen_load(g_ui.screen);
}

void UserApp_TaskInit(void) {
    StartDashboardTask(DashboardLoopTask, "DashboardLoopTask", 8192, 2, 1);
    StartDashboardTask(DashboardFetchTask, "DashboardFetchTask", 8192, 2, 0);
    StartDashboardTask(NetworkSchedulerTask, "NetSchedTask", 8192, 2, 0);
    StartDashboardTask(NetworkWorkerTask, "NetWorkerTask", 8192, 2, 0);
}
