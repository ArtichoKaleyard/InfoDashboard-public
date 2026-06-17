#include <cctype>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <esp_app_desc.h>
#include <esp_app_format.h>
#include <esp_event.h>
#include <esp_http_server.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <mbedtls/sha256.h>
#include <sdkconfig.h>

#include "display_bsp.h"
#include "lvgl_bsp.h"
#include "user_app.h"
#include "user_config.h"

DisplayPort RlcdPort(12,11,5,40,41,LCD_WIDTH,LCD_HEIGHT);

namespace {

constexpr char kDiagnosticTag[] = "DashboardHttp";
constexpr char kOtaTag[] = "DashboardOta";
constexpr char kProjectName[] = "info_dashboard";
constexpr char kTokenHeader[] = "X-Dashboard-Token";
constexpr char kFirmwareShaHeader[] = "X-Firmware-SHA256";
constexpr size_t kResponseCapacity = 8192;
constexpr size_t kOtaChunkSize = 4096;
constexpr size_t kImageProbeSize = sizeof(esp_image_header_t) +
                                   sizeof(esp_image_segment_header_t) +
                                   sizeof(esp_app_desc_t);
constexpr int kSseTaskStackWords = 6144;
constexpr int kOtaTaskStackBytes = 6144;
constexpr int kSseIdleDelayMs = 1000;
constexpr int kSseMaxLifetimeMs = 10 * 60 * 1000;
constexpr int kDiagnosticServerStartDelayMs = 10000;
constexpr int kOtaRebootDelayMs = 5000;

struct OtaRuntimeState {
	bool in_progress;
	uint32_t last_bytes;
	uint32_t last_total;
	uint32_t last_percent;
	int last_error;
	char last_state[18];
	char last_message[96];
	char last_version[32];
	char last_partition[16];
	char last_sha256[65];
};

struct SseTaskContext {
	httpd_req_t *req;
};

struct OtaTaskContext {
	httpd_req_t *req;
};

httpd_handle_t g_diagnostic_server = nullptr;
SemaphoreHandle_t g_ota_mutex = nullptr;
OtaRuntimeState g_ota_state = {
	false,
	0,
	0,
	0,
	0,
	"idle",
	"ready",
	"",
	"",
	"",
};

uint32_t OtaPercent(uint32_t bytes, uint32_t total)
{
	if (total == 0) {
		return 0;
	}
	const uint32_t percent = static_cast<uint32_t>((static_cast<uint64_t>(bytes) * 100ULL) / total);
	return percent > 100 ? 100 : percent;
}

const char *OtaPhaseForState(const char *state, const char *message)
{
	if (state && std::strcmp(state, "failed") == 0) {
		return "FAIL";
	}
	if (state && std::strcmp(state, "success") == 0) {
		return "REBOOT";
	}
	if (message) {
		if (std::strstr(message, "receiving")) {
			return "RX";
		}
		if (std::strstr(message, "header")) {
			return "HEADER";
		}
		if (std::strstr(message, "sha")) {
			return "SHA";
		}
		if (std::strstr(message, "validating") || std::strstr(message, "verified") ||
			std::strstr(message, "checking")) {
			return "VERIFY";
		}
		if (std::strstr(message, "boot")) {
			return "BOOT";
		}
	}
	return "OTA";
}

void CopyString(char *dest, size_t dest_size, const char *value)
{
	if (!dest || dest_size == 0) {
		return;
	}
	std::snprintf(dest, dest_size, "%s", value ? value : "");
}

void UpdateOtaState(const char *state, const char *message, uint32_t bytes = 0,
					int error = 0, const char *version = nullptr,
					const char *partition = nullptr, const char *sha256 = nullptr,
					uint32_t total = 0)
{
	if (g_ota_mutex && xSemaphoreTake(g_ota_mutex, pdMS_TO_TICKS(500)) != pdTRUE) {
		return;
	}
	if (total == 0) {
		total = g_ota_state.last_total;
	}
	g_ota_state.in_progress = state && std::strcmp(state, "running") == 0;
	g_ota_state.last_bytes = bytes;
	g_ota_state.last_total = total;
	g_ota_state.last_percent = OtaPercent(bytes, total);
	g_ota_state.last_error = error;
	CopyString(g_ota_state.last_state, sizeof(g_ota_state.last_state), state);
	CopyString(g_ota_state.last_message, sizeof(g_ota_state.last_message), message);
	if (version) {
		CopyString(g_ota_state.last_version, sizeof(g_ota_state.last_version), version);
	}
	if (partition) {
		CopyString(g_ota_state.last_partition, sizeof(g_ota_state.last_partition), partition);
	}
	if (sha256) {
		CopyString(g_ota_state.last_sha256, sizeof(g_ota_state.last_sha256), sha256);
	}
	if (g_ota_mutex) {
		xSemaphoreGive(g_ota_mutex);
	}
	UserApp_UpdateOtaProgress(OtaPhaseForState(state, message), message, bytes, total,
							  state && std::strcmp(state, "failed") != 0);
}

bool OtaInProgress()
{
	bool in_progress = false;
	if (g_ota_mutex && xSemaphoreTake(g_ota_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
		in_progress = g_ota_state.in_progress;
		xSemaphoreGive(g_ota_mutex);
	}
	return in_progress;
}

bool OtaSucceeded()
{
	bool success = false;
	if (g_ota_mutex && xSemaphoreTake(g_ota_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
		success = std::strcmp(g_ota_state.last_state, "success") == 0;
		xSemaphoreGive(g_ota_mutex);
	}
	return success;
}

bool IsHexChar(char ch)
{
	return std::isxdigit(static_cast<unsigned char>(ch)) != 0;
}

bool HeaderEquals(httpd_req_t *req, const char *name, const char *expected)
{
	if (!expected || expected[0] == '\0') {
		return false;
	}
	const size_t expected_len = std::strlen(expected);
	char value[96] = {};
	if (expected_len >= sizeof(value)) {
		return false;
	}
	if (httpd_req_get_hdr_value_str(req, name, value, sizeof(value)) != ESP_OK) {
		return false;
	}
	return std::strlen(value) == expected_len && std::memcmp(value, expected, expected_len) == 0;
}

bool Authorized(httpd_req_t *req)
{
	return HeaderEquals(req, kTokenHeader, CONFIG_DASHBOARD_OTA_TOKEN);
}

esp_err_t SendAuthError(httpd_req_t *req)
{
	if (std::strlen(CONFIG_DASHBOARD_OTA_TOKEN) == 0) {
		return httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "dashboard token is not configured");
	}
	return httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "invalid dashboard token");
}

bool ReadShaHeader(httpd_req_t *req, char *sha, size_t sha_size)
{
	if (!sha || sha_size < 65) {
		return false;
	}
	if (httpd_req_get_hdr_value_str(req, kFirmwareShaHeader, sha, sha_size) != ESP_OK) {
		return false;
	}
	if (std::strlen(sha) != 64) {
		return false;
	}
	for (size_t i = 0; i < 64; ++i) {
		if (!IsHexChar(sha[i])) {
			return false;
		}
		sha[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(sha[i])));
	}
	sha[64] = '\0';
	return true;
}

void BytesToHex(const uint8_t *bytes, size_t length, char *dest, size_t dest_size)
{
	if (!dest || dest_size == 0) {
		return;
	}
	dest[0] = '\0';
	size_t used = 0;
	for (size_t i = 0; i < length && used + 2 < dest_size; ++i) {
		used += std::snprintf(dest + used, dest_size - used, "%02x", bytes[i]);
	}
}

const char *OtaImageStateName(esp_ota_img_states_t state)
{
	switch (state) {
		case ESP_OTA_IMG_NEW:
			return "new";
		case ESP_OTA_IMG_PENDING_VERIFY:
			return "pending_verify";
		case ESP_OTA_IMG_VALID:
			return "valid";
		case ESP_OTA_IMG_INVALID:
			return "invalid";
		case ESP_OTA_IMG_ABORTED:
			return "aborted";
		default:
			return "undefined";
	}
}

bool ProbeImageMetadata(const uint8_t *probe, size_t probe_len, esp_app_desc_t *desc, char *error, size_t error_size)
{
	if (!probe || probe_len < kImageProbeSize || !desc) {
		CopyString(error, error_size, "image header is incomplete");
		return false;
	}
	esp_image_header_t image_header = {};
	std::memcpy(&image_header, probe, sizeof(image_header));
	if (image_header.magic != ESP_IMAGE_HEADER_MAGIC) {
		CopyString(error, error_size, "invalid image magic");
		return false;
	}
	if (image_header.segment_count == 0 || image_header.segment_count > ESP_IMAGE_MAX_SEGMENTS) {
		CopyString(error, error_size, "invalid segment count");
		return false;
	}
	if (image_header.chip_id != ESP_CHIP_ID_ESP32S3) {
		CopyString(error, error_size, "image is not for ESP32-S3");
		return false;
	}

	std::memcpy(desc,
				probe + sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t),
				sizeof(*desc));
	if (desc->magic_word != ESP_APP_DESC_MAGIC_WORD) {
		CopyString(error, error_size, "invalid app descriptor");
		return false;
	}
	if (std::strncmp(desc->project_name, kProjectName, sizeof(desc->project_name)) != 0) {
		CopyString(error, error_size, "project name mismatch");
		return false;
	}
	return true;
}

esp_err_t ScreenshotPbmHandler(httpd_req_t *req)
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
	const int header_len = std::snprintf(header, sizeof(header), "P4\n%d %d\n", width, height);
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

esp_err_t SendGeneratedText(httpd_req_t *req, const char *content_type,
							size_t (*writer)(char *, size_t))
{
	char *response = static_cast<char *>(std::malloc(kResponseCapacity));
	if (!response) {
		httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "diagnostic allocation failed");
		return ESP_FAIL;
	}
	const size_t length = writer(response, kResponseCapacity);
	httpd_resp_set_type(req, content_type);
	const esp_err_t err = httpd_resp_send(req, response, length);
	std::free(response);
	return err;
}

esp_err_t DiagnosticsJsonHandler(httpd_req_t *req)
{
	return SendGeneratedText(req, "application/json", UserApp_WriteDiagnosticsJson);
}

esp_err_t LogsTextHandler(httpd_req_t *req)
{
	return SendGeneratedText(req, "text/plain; charset=utf-8", UserApp_WriteLogsText);
}

esp_err_t OtaStatusHandler(httpd_req_t *req)
{
#if CONFIG_DASHBOARD_OTA_ENABLE
	char response[1024] = {};
	const esp_partition_t *running = esp_ota_get_running_partition();
	const esp_partition_t *boot = esp_ota_get_boot_partition();
	const esp_app_desc_t *running_desc = esp_app_get_description();
	esp_ota_img_states_t image_state = ESP_OTA_IMG_UNDEFINED;
	if (running) {
		esp_ota_get_state_partition(running, &image_state);
	}

	OtaRuntimeState snapshot = {};
	if (g_ota_mutex && xSemaphoreTake(g_ota_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
		snapshot = g_ota_state;
		xSemaphoreGive(g_ota_mutex);
	}
	std::snprintf(response, sizeof(response),
				  "{\n"
				  "  \"ota_enabled\":true,\n"
				  "  \"token_configured\":%s,\n"
				  "  \"running\":{\"label\":\"%s\",\"subtype\":%u,\"address\":%" PRIu32 ",\"version\":\"%s\",\"state\":\"%s\"},\n"
				  "  \"boot\":{\"label\":\"%s\",\"subtype\":%u,\"address\":%" PRIu32 "},\n"
				  "  \"last\":{\"state\":\"%s\",\"message\":\"%s\",\"bytes\":%" PRIu32 ",\"total\":%" PRIu32 ",\"percent\":%" PRIu32 ",\"error\":%d,\"version\":\"%s\",\"partition\":\"%s\",\"sha256\":\"%s\"}\n"
				  "}\n",
				  std::strlen(CONFIG_DASHBOARD_OTA_TOKEN) > 0 ? "true" : "false",
				  running ? running->label : "",
				  running ? running->subtype : 0,
				  running ? running->address : 0,
				  running_desc ? running_desc->version : "",
				  OtaImageStateName(image_state),
				  boot ? boot->label : "",
				  boot ? boot->subtype : 0,
				  boot ? boot->address : 0,
				  snapshot.last_state,
				  snapshot.last_message,
				  snapshot.last_bytes,
				  snapshot.last_total,
				  snapshot.last_percent,
				  snapshot.last_error,
				  snapshot.last_version,
				  snapshot.last_partition,
				  snapshot.last_sha256);
	httpd_resp_set_type(req, "application/json");
	return httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
#else
	return httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "OTA disabled");
#endif
}

esp_err_t OtaUploadTaskHandler(httpd_req_t *req)
{
#if CONFIG_DASHBOARD_OTA_ENABLE
	if (req->content_len == 0) {
		return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "missing firmware body");
	}

	char expected_sha[65] = {};
	if (!ReadShaHeader(req, expected_sha, sizeof(expected_sha))) {
		return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "missing or invalid X-Firmware-SHA256");
	}

	const esp_partition_t *update_partition = esp_ota_get_next_update_partition(nullptr);
	if (!update_partition) {
		return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "no OTA update partition");
	}
	if (req->content_len > update_partition->size) {
		UpdateOtaState("failed", "image exceeds OTA partition", 0, ESP_ERR_INVALID_SIZE,
					   nullptr, update_partition->label, nullptr, req->content_len);
		return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "firmware is larger than OTA partition");
	}
	if (req->content_len < kImageProbeSize) {
		UpdateOtaState("failed", "image is too small", 0, ESP_ERR_INVALID_SIZE,
					   nullptr, update_partition->label, nullptr, req->content_len);
		return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "firmware is too small");
	}

	UpdateOtaState("running", "receiving firmware", 0, 0, nullptr, update_partition->label,
				   expected_sha, req->content_len);
	UserApp_RecordDiagnosticLog("ota", "start", "upload started");
	ESP_LOGI(kOtaTag, "OTA upload started: partition=%s bytes=%u",
			 update_partition->label, static_cast<unsigned>(req->content_len));

	mbedtls_sha256_context sha_ctx;
	mbedtls_sha256_init(&sha_ctx);
	mbedtls_sha256_starts(&sha_ctx, 0);

	uint8_t *buffer = static_cast<uint8_t *>(std::malloc(kOtaChunkSize));
	if (!buffer) {
		mbedtls_sha256_free(&sha_ctx);
		UpdateOtaState("failed", "OTA buffer allocation failed", 0, ESP_ERR_NO_MEM,
					   nullptr, update_partition->label, nullptr, req->content_len);
		return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA buffer allocation failed");
	}

	uint8_t probe[kImageProbeSize] = {};
	size_t probe_len = 0;
	size_t total_read = 0;
	bool metadata_checked = false;
	bool ota_started = false;
	esp_app_desc_t new_desc = {};
	esp_ota_handle_t ota_handle = 0;
	esp_err_t result = ESP_OK;
	char fail_message[96] = {};
	size_t remaining = req->content_len;
	size_t last_progress_report = 0;

	while (remaining > 0) {
		const size_t to_read = remaining > kOtaChunkSize ? kOtaChunkSize : remaining;
		const int read = httpd_req_recv(req, reinterpret_cast<char *>(buffer), to_read);
		if (read == HTTPD_SOCK_ERR_TIMEOUT) {
			continue;
		}
		if (read <= 0) {
			result = ESP_FAIL;
			CopyString(fail_message, sizeof(fail_message), "request receive failed");
			break;
		}

		mbedtls_sha256_update(&sha_ctx, buffer, static_cast<size_t>(read));
		total_read += static_cast<size_t>(read);
		remaining -= static_cast<size_t>(read);

		size_t probe_copy_len = 0;
		if (probe_len < sizeof(probe)) {
			probe_copy_len = sizeof(probe) - probe_len < static_cast<size_t>(read)
								 ? sizeof(probe) - probe_len
								 : static_cast<size_t>(read);
			std::memcpy(probe + probe_len, buffer, probe_copy_len);
			probe_len += probe_copy_len;
		}

		if (!metadata_checked && probe_len >= sizeof(probe)) {
			metadata_checked = true;
			if (!ProbeImageMetadata(probe, probe_len, &new_desc, fail_message, sizeof(fail_message))) {
				result = ESP_ERR_INVALID_ARG;
				break;
			}
			result = esp_ota_begin(update_partition, req->content_len, &ota_handle);
			if (result != ESP_OK) {
				std::snprintf(fail_message, sizeof(fail_message), "esp_ota_begin failed: %s", esp_err_to_name(result));
				break;
			}
			ota_started = true;
			result = esp_ota_write(ota_handle, probe, probe_len);
			if (result != ESP_OK) {
				std::snprintf(fail_message, sizeof(fail_message), "esp_ota_write failed: %s", esp_err_to_name(result));
				break;
			}
			if (static_cast<size_t>(read) > probe_copy_len) {
				result = esp_ota_write(ota_handle, buffer + probe_copy_len,
									   static_cast<size_t>(read) - probe_copy_len);
				if (result != ESP_OK) {
					std::snprintf(fail_message, sizeof(fail_message), "esp_ota_write failed: %s", esp_err_to_name(result));
					break;
				}
			}
			UpdateOtaState("running", "validated image header", static_cast<uint32_t>(total_read),
						   0, new_desc.version, update_partition->label, expected_sha, req->content_len);
			UserApp_RecordDiagnosticLog("ota", "header_ok", new_desc.version);
		} else if (ota_started) {
			result = esp_ota_write(ota_handle, buffer, static_cast<size_t>(read));
			if (result != ESP_OK) {
				std::snprintf(fail_message, sizeof(fail_message), "esp_ota_write failed: %s", esp_err_to_name(result));
				break;
			}
		}
		if (result == ESP_OK && (total_read - last_progress_report >= 64U * 1024U || remaining == 0)) {
			last_progress_report = total_read;
			UpdateOtaState("running", "receiving firmware", static_cast<uint32_t>(total_read),
						   0, metadata_checked ? new_desc.version : nullptr,
						   update_partition->label, expected_sha, req->content_len);
		}
	}

	std::free(buffer);

	uint8_t digest[32] = {};
	char actual_sha[65] = {};
	mbedtls_sha256_finish(&sha_ctx, digest);
	mbedtls_sha256_free(&sha_ctx);
	BytesToHex(digest, sizeof(digest), actual_sha, sizeof(actual_sha));

	if (result == ESP_OK) {
		UpdateOtaState("running", "verifying sha256", static_cast<uint32_t>(total_read),
					   0, new_desc.version, update_partition->label, expected_sha, req->content_len);
	}
	if (result == ESP_OK && !ota_started) {
		result = ESP_ERR_INVALID_ARG;
		CopyString(fail_message, sizeof(fail_message), "image metadata was not validated");
	}
	if (result == ESP_OK && total_read != req->content_len) {
		result = ESP_ERR_INVALID_SIZE;
		CopyString(fail_message, sizeof(fail_message), "incomplete firmware upload");
	}
	if (result == ESP_OK && std::strcmp(actual_sha, expected_sha) != 0) {
		result = ESP_ERR_INVALID_CRC;
		CopyString(fail_message, sizeof(fail_message), "firmware sha256 mismatch");
	}

	if (result == ESP_OK) {
		UpdateOtaState("running", "validating image", static_cast<uint32_t>(total_read),
					   0, new_desc.version, update_partition->label, actual_sha, req->content_len);
		result = esp_ota_end(ota_handle);
		ota_handle = 0;
		if (result != ESP_OK) {
			std::snprintf(fail_message, sizeof(fail_message), "esp_ota_end failed: %s", esp_err_to_name(result));
		}
	} else if (ota_started) {
		esp_ota_abort(ota_handle);
		ota_handle = 0;
	}

	if (result == ESP_OK) {
		UpdateOtaState("running", "checking app descriptor", static_cast<uint32_t>(total_read),
					   0, new_desc.version, update_partition->label, actual_sha, req->content_len);
		esp_app_desc_t written_desc = {};
		result = esp_ota_get_partition_description(update_partition, &written_desc);
		if (result != ESP_OK) {
			std::snprintf(fail_message, sizeof(fail_message), "partition description failed: %s", esp_err_to_name(result));
		} else if (std::strncmp(written_desc.project_name, kProjectName, sizeof(written_desc.project_name)) != 0) {
			result = ESP_ERR_INVALID_ARG;
			CopyString(fail_message, sizeof(fail_message), "written project name mismatch");
		}
	}

	if (result == ESP_OK) {
		UpdateOtaState("running", "switching boot partition", static_cast<uint32_t>(total_read),
					   0, new_desc.version, update_partition->label, actual_sha, req->content_len);
		result = esp_ota_set_boot_partition(update_partition);
		if (result != ESP_OK) {
			std::snprintf(fail_message, sizeof(fail_message), "set boot partition failed: %s", esp_err_to_name(result));
		}
	}

	if (result != ESP_OK) {
		UpdateOtaState("failed", fail_message, static_cast<uint32_t>(total_read), result,
					   new_desc.version, update_partition->label, actual_sha, req->content_len);
		UserApp_RecordDiagnosticLog("ota", "failed", fail_message);
		ESP_LOGE(kOtaTag, "OTA upload failed: %s", fail_message);
		return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, fail_message);
	}

	UpdateOtaState("success", "firmware verified; rebooting", static_cast<uint32_t>(total_read), 0,
				   new_desc.version, update_partition->label, actual_sha, req->content_len);
	UserApp_RecordDiagnosticLog("ota", "success", "firmware verified; rebooting");
	ESP_LOGI(kOtaTag, "OTA upload verified: version=%s partition=%s bytes=%u",
			 new_desc.version, update_partition->label, static_cast<unsigned>(total_read));

	char response[256] = {};
	std::snprintf(response, sizeof(response),
				  "{\"ok\":true,\"version\":\"%s\",\"partition\":\"%s\",\"bytes\":%u,\"sha256\":\"%s\",\"reboot_ms\":%d}\n",
				  new_desc.version, update_partition->label, static_cast<unsigned>(total_read), actual_sha,
				  kOtaRebootDelayMs);
	httpd_resp_set_type(req, "application/json");
	const esp_err_t send_result = httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
	return send_result;
#else
	return httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "OTA disabled");
#endif
}

void OtaUploadTask(void *arg)
{
	OtaTaskContext *context = static_cast<OtaTaskContext *>(arg);
	httpd_req_t *req = context ? context->req : nullptr;
	std::free(context);
	if (!req) {
		vTaskDelete(nullptr);
		return;
	}

	OtaUploadTaskHandler(req);
	const bool restart = OtaSucceeded();
	httpd_req_async_handler_complete(req);
	if (restart) {
		vTaskDelay(pdMS_TO_TICKS(kOtaRebootDelayMs));
		esp_restart();
	}
	vTaskDelete(nullptr);
}

esp_err_t OtaUploadHandler(httpd_req_t *req)
{
#if CONFIG_DASHBOARD_OTA_ENABLE
	if (!Authorized(req)) {
		return SendAuthError(req);
	}
	if (OtaInProgress()) {
		return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA already in progress");
	}
	UpdateOtaState("running", "queued upload", 0, 0, nullptr, nullptr, nullptr, req->content_len);

	httpd_req_t *async_req = nullptr;
	if (httpd_req_async_handler_begin(req, &async_req) != ESP_OK) {
		UpdateOtaState("failed", "async OTA unavailable", 0, ESP_FAIL, nullptr, nullptr, nullptr, req->content_len);
		return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "async OTA unavailable");
	}
	OtaTaskContext *context = static_cast<OtaTaskContext *>(std::malloc(sizeof(OtaTaskContext)));
	if (!context) {
		UpdateOtaState("failed", "OTA task allocation failed", 0, ESP_ERR_NO_MEM, nullptr, nullptr, nullptr,
					   req->content_len);
		httpd_resp_send_err(async_req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA task allocation failed");
		httpd_req_async_handler_complete(async_req);
		return ESP_OK;
	}
	context->req = async_req;
	if (xTaskCreate(OtaUploadTask, "OtaUploadTask", kOtaTaskStackBytes, context, 2, nullptr) != pdPASS) {
		std::free(context);
		UpdateOtaState("failed", "OTA task start failed", 0, ESP_FAIL, nullptr, nullptr, nullptr, req->content_len);
		httpd_resp_send_err(async_req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA task start failed");
		httpd_req_async_handler_complete(async_req);
		return ESP_OK;
	}
	return ESP_OK;
#else
	return httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "OTA disabled");
#endif
}

esp_err_t SendSseText(httpd_req_t *req, const char *text)
{
	if (!text || text[0] == '\0') {
		return ESP_OK;
	}
	esp_err_t err = httpd_resp_sendstr_chunk(req, "event: logs\n");
	const char *line_start = text;
	while (err == ESP_OK && *line_start != '\0') {
		const char *line_end = std::strchr(line_start, '\n');
		const size_t line_len = line_end ? static_cast<size_t>(line_end - line_start) : std::strlen(line_start);
		err = httpd_resp_sendstr_chunk(req, "data: ");
		if (err == ESP_OK && line_len > 0) {
			err = httpd_resp_send_chunk(req, line_start, line_len);
		}
		if (err == ESP_OK) {
			err = httpd_resp_sendstr_chunk(req, "\n");
		}
		if (!line_end) {
			break;
		}
		line_start = line_end + 1;
	}
	if (err == ESP_OK) {
		err = httpd_resp_sendstr_chunk(req, "\n");
	}
	return err;
}

void LogsStreamTask(void *arg)
{
	SseTaskContext *context = static_cast<SseTaskContext *>(arg);
	httpd_req_t *req = context ? context->req : nullptr;
	std::free(context);
	if (!req) {
		vTaskDelete(nullptr);
		return;
	}

	httpd_resp_set_type(req, "text/event-stream");
	httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
	httpd_resp_set_hdr(req, "X-Accel-Buffering", "no");
	esp_err_t err = httpd_resp_sendstr_chunk(req, ": InfoDashboard logs\n\n");
	uint32_t last_sequence = 0;
	const int64_t start_ms = esp_timer_get_time() / 1000LL;
	char *buffer = static_cast<char *>(std::malloc(2048));
	while (err == ESP_OK && (esp_timer_get_time() / 1000LL) - start_ms < kSseMaxLifetimeMs) {
		if (!buffer) {
			err = httpd_resp_sendstr_chunk(req, "event: error\ndata: log stream allocation failed\n\n");
			break;
		}
		uint32_t latest = last_sequence;
		const size_t length = UserApp_ReadLogEntriesAfter(last_sequence, buffer, 2048, &latest);
		if (length > 0) {
			err = SendSseText(req, buffer);
			last_sequence = latest;
		} else {
			err = httpd_resp_sendstr_chunk(req, ": ping\n\n");
		}
		vTaskDelay(pdMS_TO_TICKS(kSseIdleDelayMs));
	}
	if (buffer) {
		std::free(buffer);
	}
	if (err == ESP_OK) {
		httpd_resp_send_chunk(req, nullptr, 0);
	}
	httpd_req_async_handler_complete(req);
	vTaskDelete(nullptr);
}

esp_err_t LogsStreamHandler(httpd_req_t *req)
{
#if CONFIG_DASHBOARD_LOG_STREAM_ENABLE
#if CONFIG_DASHBOARD_LOG_STREAM_REQUIRE_TOKEN
	if (!Authorized(req)) {
		return SendAuthError(req);
	}
#endif
	httpd_req_t *async_req = nullptr;
	if (httpd_req_async_handler_begin(req, &async_req) != ESP_OK) {
		return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "async log stream unavailable");
	}
	SseTaskContext *context = static_cast<SseTaskContext *>(std::malloc(sizeof(SseTaskContext)));
	if (!context) {
		httpd_resp_send_err(async_req, HTTPD_500_INTERNAL_SERVER_ERROR, "log stream allocation failed");
		httpd_req_async_handler_complete(async_req);
		return ESP_OK;
	}
	context->req = async_req;
	if (xTaskCreate(LogsStreamTask, "LogSseTask", kSseTaskStackWords, context, 1, nullptr) != pdPASS) {
		std::free(context);
		httpd_resp_send_err(async_req, HTTPD_500_INTERNAL_SERVER_ERROR, "log stream task failed");
		httpd_req_async_handler_complete(async_req);
		return ESP_OK;
	}
	return ESP_OK;
#else
	return httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "log stream disabled");
#endif
}

void RegisterUri(const char *uri, httpd_method_t method, esp_err_t (*handler)(httpd_req_t *))
{
	httpd_uri_t item = {};
	item.uri = uri;
	item.method = method;
	item.handler = handler;
	item.user_ctx = nullptr;
	ESP_ERROR_CHECK(httpd_register_uri_handler(g_diagnostic_server, &item));
}

void StartDiagnosticServer()
{
#if CONFIG_DASHBOARD_SCREENSHOT_ENABLE
	if (g_diagnostic_server) {
		return;
	}
	if (!g_ota_mutex) {
		g_ota_mutex = xSemaphoreCreateMutex();
	}
	httpd_config_t config = HTTPD_DEFAULT_CONFIG();
	config.server_port = CONFIG_DASHBOARD_SCREENSHOT_PORT;
	config.ctrl_port = CONFIG_DASHBOARD_SCREENSHOT_PORT + 1;
	config.stack_size = 8192;
	config.max_open_sockets = 2;
	config.lru_purge_enable = true;
	if (httpd_start(&g_diagnostic_server, &config) != ESP_OK) {
		ESP_LOGW(kDiagnosticTag, "diagnostic server start failed");
		g_diagnostic_server = nullptr;
		return;
	}
	RegisterUri("/screenshot.pbm", HTTP_GET, ScreenshotPbmHandler);
	RegisterUri("/diagnostics.json", HTTP_GET, DiagnosticsJsonHandler);
	RegisterUri("/logs.txt", HTTP_GET, LogsTextHandler);
	RegisterUri("/logs/stream", HTTP_GET, LogsStreamHandler);
	RegisterUri("/ota/status", HTTP_GET, OtaStatusHandler);
	RegisterUri("/ota", HTTP_POST, OtaUploadHandler);
	ESP_LOGI(kDiagnosticTag,
			 "diagnostic endpoints ready: /screenshot.pbm /diagnostics.json /logs.txt /logs/stream /ota/status /ota on port %d",
			 CONFIG_DASHBOARD_SCREENSHOT_PORT);
#endif
}

void DiagnosticServerTask(void *)
{
	vTaskDelay(pdMS_TO_TICKS(kDiagnosticServerStartDelayMs));
	StartDiagnosticServer();
	vTaskDelete(nullptr);
}

void Lvgl_FlushCallback(lv_display_t *drv, const lv_area_t *area, uint8_t *color_map)
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

}  // namespace

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
	UserApp_TaskInit();
	if (xTaskCreate(DiagnosticServerTask, "diag_http_start", 4096, nullptr, 3, nullptr) != pdPASS) {
		ESP_LOGW(kDiagnosticTag, "diagnostic server delayed start task create failed");
		StartDiagnosticServer();
	}
}
