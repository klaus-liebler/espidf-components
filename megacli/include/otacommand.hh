#pragma once
#include <esp_log.h>
#include <esp_ota_ops.h>
#include <esp_https_ota.h>
#include <esp_crt_bundle.h>
#define TAG "OTA"

void FreeMemoryForOTA();

class OTACommand : public CLI::AbstractCommand
{
public:
	int ExecuteInternal(IShellCallback *cb, const char *url, bool showInfoOnly, bool force)
	{
		int exitcode{0};
		const esp_partition_t *configured_partition = esp_ota_get_boot_partition();
		const esp_partition_t *running_partition = esp_ota_get_running_partition();
		const esp_partition_t *last_invalid_partition = esp_ota_get_last_invalid_partition();
		const esp_partition_t *update_partition = esp_ota_get_next_update_partition(running_partition);

		esp_app_desc_t app_desc_run;
		esp_app_desc_t app_desc_invalid;
		esp_app_desc_t app_desc_ota;

		esp_http_client_config_t http_client_config = {};
		http_client_config.url = url;
		// config.cert_pem = (char *)netcase_hs_osnabrueck_de_crt_start;
		http_client_config.crt_bundle_attach = esp_crt_bundle_attach;
		http_client_config.timeout_ms = 20000;
		http_client_config.keep_alive_enable = true;
		http_client_config.user_data = (void *)this;
		http_client_config.skip_cert_common_name_check = true;

		esp_https_ota_config_t ota_config = {};
		ota_config.http_config = &http_client_config;
		ota_config.http_client_init_cb = nullptr;

		esp_https_ota_handle_t https_ota_handle{nullptr};

		//ip_addr_t addr;

		int image_size{0};
		int lastPrintedPercentage{-10};

		esp_err_t err{ESP_ERR_HTTPS_OTA_IN_PROGRESS};
		esp_err_t ota_finish_err{ESP_OK};

		if (configured_partition == nullptr || running_partition == nullptr || update_partition == nullptr)
		{
			if(cb)cb->printf(COLOR_RESET COLOR_RED "At least one partition, that is needed for OTA operation, ist not available. Check partition table!\r\n" COLOR_RESET);
			ESP_LOGE(TAG, "At least one partition, that is needed for OTA operation, ist not available. Check partition table!");
			exitcode = -1;
			goto exit;
		}

		if (configured_partition != running_partition)
		{
			if(cb)cb->printf(COLOR_RESET COLOR_RED "Configured OTA boot partition at offset 0x%08x, but running from offset 0x%08x. (This can happen if either the OTA boot data or preferred boot image become corrupted somehow).\r\n" COLOR_RESET, (unsigned int)configured_partition->address, (unsigned int)running_partition->address);
			ESP_LOGE(TAG, "Configured OTA boot partition at offset 0x%08x, but running from offset 0x%08x. (This can happen if either the OTA boot data or preferred boot image become corrupted somehow).", (unsigned int)configured_partition->address, (unsigned int)running_partition->address);
			exitcode = -2;
			goto exit;
		}

		esp_ota_get_partition_description(running_partition, &app_desc_run);

		if(cb)cb->printf("Running is version %s of firmware \"%s\" compiled on %s %s. Partition subtype is %d.\r\n", app_desc_run.version, app_desc_run.project_name, app_desc_run.date, app_desc_run.time, running_partition->subtype);
		ESP_LOGI(TAG, "Running is version %s of firmware \"%s\" compiled on %s %s. Partition subtype is %d.", app_desc_run.version, app_desc_run.project_name, app_desc_run.date, app_desc_run.time, running_partition->subtype);
	
		if (esp_ota_get_partition_description(last_invalid_partition, &app_desc_invalid) == ESP_OK)
		{
			if(cb)cb->printf(COLOR_RESET COLOR_YELLOW "Last invalid firmware version: %s\r\n" COLOR_RESET, app_desc_invalid.version);
			ESP_LOGW(TAG, "Last invalid firmware version: %s", app_desc_invalid.version);
		}

		if (url == nullptr)
		{
			exitcode = 0;
			goto exit;
		}

		FreeMemoryForOTA();
		
		esp_https_ota_abort(https_ota_handle); // just for safety - cancel previous attempts. nullptr-checks are integrated!

		if (esp_https_ota_begin(&ota_config, &https_ota_handle) != ESP_OK)
		{
			if(cb)cb->printf(COLOR_RESET COLOR_RED "ESP HTTPS OTA Begin failed\r\n" COLOR_RESET);
			ESP_LOGE(TAG, "ESP HTTPS OTA Begin failed");
			exitcode = -4;
			goto exit;
		}

		if (esp_https_ota_get_img_desc(https_ota_handle, &app_desc_ota) != ESP_OK)
		{
			esp_https_ota_abort(https_ota_handle);
			if(cb)cb->printf(COLOR_RESET COLOR_RED "esp_https_ota_get_img_desc failed\r\n" COLOR_RESET);
			ESP_LOGE(TAG, "esp_https_ota_get_img_desc failed");
			exitcode = -5;
			goto exit;
		}

		if (showInfoOnly)
		{
			if(cb)cb->printf("Online  is a version %s available for firmware \"%s\" compiled on %s %s.\r\n", app_desc_ota.version, app_desc_ota.project_name, app_desc_ota.date, app_desc_ota.time);
			ESP_LOGI(TAG, "Online  is a version %s available for firmware \"%s\" compiled on %s %s.", app_desc_ota.version, app_desc_ota.project_name, app_desc_ota.date, app_desc_ota.time);
			esp_https_ota_abort(https_ota_handle);
			exitcode = 0;
			goto exit;
		}

		// check current ota version with last invalid partition
		if (last_invalid_partition != NULL && memcmp(app_desc_invalid.version, app_desc_ota.version, sizeof(app_desc_ota.version)) == 0)
		{
			if(cb)cb->printf(COLOR_RESET COLOR_YELLOW "New version is the same as invalid version.\r\n" COLOR_RESET);
			ESP_LOGW(TAG, "New version is the same as invalid version.");
			if (!force)
			{
				esp_https_ota_abort(https_ota_handle);
				exitcode = -6;
				goto exit;
			}
		}

		if (memcmp(app_desc_ota.version, app_desc_run.version, sizeof(app_desc_ota.version)) == 0)
		{
			if(cb)cb->printf(COLOR_RESET COLOR_YELLOW "New version is the same as invalid version.\r\n" COLOR_RESET);
			ESP_LOGW(TAG, "New version is the same as invalid version.");
			if (!force)
			{
				esp_https_ota_abort(https_ota_handle);
				exitcode = -7;
				goto exit;
			}
		}

		if (memcmp(app_desc_ota.project_name, app_desc_run.project_name, sizeof(app_desc_ota.project_name)) != 0)
		{
			if(cb)cb->printf(COLOR_RESET COLOR_YELLOW "Different project name! remote=\"%s\", local=\"%s\"\r\n" COLOR_RESET, app_desc_ota.project_name, app_desc_run.project_name);
			ESP_LOGW(TAG, "Different project name! remote=\"%s\", local=\"%s\"", app_desc_ota.project_name, app_desc_run.project_name);
			if (!force)
			{
				esp_https_ota_abort(https_ota_handle);
				exitcode = -8;
				goto exit;
			}
		}

		image_size = esp_https_ota_get_image_size(https_ota_handle);
		if(cb)cb->printf(COLOR_RESET COLOR_GREEN "There is a new version %s available for firmware \"%s\" compiled on %s %s. We try to write this to partition subtype %d at offset 0x%x\r\n" COLOR_RESET, app_desc_ota.version, app_desc_ota.project_name, app_desc_ota.date, app_desc_ota.time, update_partition->subtype, (unsigned int)update_partition->address);
		ESP_LOGI(TAG, "There is a new version %s available for firmware \"%s\" compiled on %s %s. We try to write this to partition subtype %d at offset 0x%x", app_desc_ota.version, app_desc_ota.project_name, app_desc_ota.date, app_desc_ota.time,  update_partition->subtype, (unsigned int)update_partition->address);
		
		while (err == ESP_ERR_HTTPS_OTA_IN_PROGRESS)
		{
			err = esp_https_ota_perform(https_ota_handle);
			int image_already_read = esp_https_ota_get_image_len_read(https_ota_handle);
			int percentage = (image_already_read * 100) / image_size;
			if(percentage>=lastPrintedPercentage+1){
				if(cb)cb->printf("Flash complete: %d\r\n", percentage);
				ESP_LOGI(TAG, "Flash complete: %d", percentage);
				lastPrintedPercentage=percentage;
			}
			vTaskDelay(2);
		}

		if (esp_https_ota_is_complete_data_received(https_ota_handle) != true)
		{

			esp_https_ota_abort(https_ota_handle);
			if(cb)cb->printf(COLOR_RESET COLOR_RED "Data was not received completely.\r\n" COLOR_RESET);
			ESP_LOGE(TAG, "Data was not received completely.");
			exitcode = -9;
			goto exit;
		}
		ota_finish_err = esp_https_ota_finish(https_ota_handle);

		if ((err != ESP_OK) || (ota_finish_err != ESP_OK))
		{
			if (ota_finish_err == ESP_ERR_OTA_VALIDATE_FAILED)
			{
				if(cb)cb->printf(COLOR_RESET COLOR_RED "Image validation failed, image is corrupted.\r\n" COLOR_RESET);
				ESP_LOGE(TAG, "Image validation failed, image is corrupted.");
			}
			if(cb)cb->printf(COLOR_RESET COLOR_RED "ESP_HTTPS_OTA upgrade failed 0x%x\r\n" COLOR_RESET, ota_finish_err);
			ESP_LOGE(TAG, "ESP_HTTPS_OTA upgrade failed 0x%x", ota_finish_err);
			exitcode = -10;
			goto exit;
		}
		if(cb)cb->printf(COLOR_RESET COLOR_GREEN "ESP_HTTPS_OTA upgrade successful. Rebooting ...\r\n");
		ESP_LOGI(TAG, "ESP_HTTPS_OTA upgrade successful. Rebooting ...");
		vTaskDelay(pdMS_TO_TICKS(1000));
		esp_restart();
	exit:
		return exitcode;
	}

public:
	const char *GetName() override
	{
		return "ota";
	}
	int Execute(IShellCallback *cb, int argc, char *argv[]) override
	{
		FILE *fp = funopen(cb, nullptr, &CLI::Static_writefn, nullptr, nullptr);

		arg_lit *help = arg_litn(NULL, "help", 0, 1, "display this help and exit");
		arg_lit *info = arg_litn("i", "info", 0, 1, "display info and exit");
		arg_lit *update = arg_litn("u", "update", 0, 1, "update from url");
		arg_lit *force = arg_litn("f", "force", 0, 1, "force update even if the local version is newer");
		arg_str *url = arg_strn(NULL, NULL, "url", 0, 1, "firmware url");
		auto end_arg = arg_end(20);
		int exitcode{0};
		void *argtable[] = {help, info, update, force, url, end_arg};
		int nerrors = arg_parse(argc, argv, argtable);
		if (help->count > 0)
		{
			if(cb)cb->printf(COLOR_RESET "Usage: %s\r\n", GetName());
			arg_print_syntax(fp, argtable, "\r\n");
			if(cb)cb->printf("Do OTA processing\r\n");
			arg_print_glossary(fp, argtable, "  %-25s %s\r\n");
			if(cb)cb->printf(COLOR_RESET);
			exitcode = 0;
			goto exit;
		}
		if (nerrors > 0)
		{
			// Display the error details contained in the arg_end struct.
			if(cb)cb->printf(COLOR_RESET COLOR_RED);
			arg_print_errors(fp, end_arg, GetName());
			if(cb)cb->printf("\r\n");
			if(cb)cb->printf("'%s --help' for more information.\r\n COLOR_RESET", GetName());
			exitcode = 1;
			goto exit;
		}
		if (info->count > 0)
		{
			exitcode = ExecuteInternal(cb, url->count > 0 ? url->sval[0] : nullptr, true, false);
		}
		else if (update->count > 0)
		{
			exitcode = ExecuteInternal(cb, url->count > 0 ? url->sval[0] : nullptr, false, force->count > 0);
		}
	exit:
		arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
		fclose(fp);
		return exitcode;
	}
};
#undef TAG