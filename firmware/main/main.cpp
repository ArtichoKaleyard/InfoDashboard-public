
#include <stdio.h>
#include <cstdlib>
#include <freertos/FreeRTOS.h>
#include <esp_timer.h>
#include <esp_log.h>
#include <esp_event.h>
#include <esp_http_server.h>
#include <esp_netif.h>
#include <sdkconfig.h>

#include "display_bsp.h"
#include "lvgl_bsp.h"
#include "user_app.h"
#include "user_config.h"

DisplayPort RlcdPort(12,11,5,40,41,LCD_WIDTH,LCD_HEIGHT);
static const char *kScreenshotTag = "ScreenShot";
static httpd_handle_t g_screenshot_server = nullptr;

static esp_err_t ScreenshotPbmHandler(httpd_req_t *req)
{
	const int width = RlcdPort.Width();
	const int height = RlcdPort.Height();
	const size_t row_bytes = (static_cast<size_t>(width) + 7U) / 8U;
	const size_t image_len = row_bytes * static_cast<size_t>(height);
	uint8_t *image = static_cast<uint8_t *>(std::malloc(image_len));
	if (!image) {
		httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "screenshot allocation failed");
		return ESP_FAIL;
	}
	if (!RlcdPort.RLCD_CopyPbmBits(image, image_len)) {
		std::free(image);
		httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "screenshot capture failed");
		return ESP_FAIL;
	}

	char header[32] = {};
	const int header_len = snprintf(header, sizeof(header), "P4\n%d %d\n", width, height);
	httpd_resp_set_type(req, "image/x-portable-bitmap");
	httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=screen.pbm");
	esp_err_t err = httpd_resp_send_chunk(req, header, header_len);
	if (err == ESP_OK) {
		err = httpd_resp_send_chunk(req, reinterpret_cast<const char *>(image), image_len);
	}
	std::free(image);
	if (err == ESP_OK) {
		err = httpd_resp_send_chunk(req, nullptr, 0);
	}
	return err;
}

static void StartScreenshotServer()
{
#if CONFIG_DASHBOARD_SCREENSHOT_ENABLE
	if (g_screenshot_server) {
		return;
	}
	httpd_config_t config = HTTPD_DEFAULT_CONFIG();
	config.server_port = CONFIG_DASHBOARD_SCREENSHOT_PORT;
	config.ctrl_port = CONFIG_DASHBOARD_SCREENSHOT_PORT + 1;
	config.max_open_sockets = 2;
	config.lru_purge_enable = true;
	if (httpd_start(&g_screenshot_server, &config) != ESP_OK) {
		ESP_LOGW(kScreenshotTag, "screenshot server start failed");
		g_screenshot_server = nullptr;
		return;
	}
	httpd_uri_t screenshot_uri = {};
	screenshot_uri.uri = "/screenshot.pbm";
	screenshot_uri.method = HTTP_GET;
	screenshot_uri.handler = ScreenshotPbmHandler;
	screenshot_uri.user_ctx = nullptr;
	ESP_ERROR_CHECK(httpd_register_uri_handler(g_screenshot_server, &screenshot_uri));
	ESP_LOGI(kScreenshotTag, "screenshot endpoint ready: /screenshot.pbm on port %d", CONFIG_DASHBOARD_SCREENSHOT_PORT);
#endif
}

static void Lvgl_FlushCallback(lv_display_t *drv, const lv_area_t *area, uint8_t *color_map)
{
	RlcdPort.RLCD_BeginFrame();
  	uint16_t *buffer = (uint16_t *)color_map;
  	for(int y = area->y1; y <= area->y2; y++) 
  	{
  	 	for(int x = area->x1; x <= area->x2; x++) 
  	 	{
  	 	   	uint8_t color = (*buffer < 0x7fff) ? ColorBlack : ColorWhite;
  	 	   	RlcdPort.RLCD_SetPixel(x, y, color);
  	 	   	buffer++;
  	 	}
  	}
  	RlcdPort.RLCD_Display();
	RlcdPort.RLCD_EndFrame();
	lv_disp_flush_ready(drv);
}

extern "C" void app_main(void)
{
	UserApp_AppInit();
	ESP_ERROR_CHECK(esp_netif_init());
	esp_err_t event_loop_result = esp_event_loop_create_default();
	if (event_loop_result != ESP_OK && event_loop_result != ESP_ERR_INVALID_STATE) {
		ESP_ERROR_CHECK(event_loop_result);
	}
	RlcdPort.RLCD_Init();
	Lvgl_PortInit(400,300,Lvgl_FlushCallback);
	if(Lvgl_lock(-1)) {
		UserApp_UiInit();
  	  	Lvgl_unlock();
  	}
	StartScreenshotServer();
	UserApp_TaskInit();
}
