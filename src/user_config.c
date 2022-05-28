
#include <string.h>

#include "user_config.h"
#include "cJSON/cJSON.h"

unsigned char crc_calc(unsigned char *data, int len)
{
	unsigned int crc = 0;
	int i;
	for (i = 0; i < len; i++)
		crc += data[i];

	return crc;
}


void get_user_config_str(char *res)
{
	cJSON *json_send = cJSON_CreateObject();
	cJSON_AddStringToObject(json_send, "mac", strMac);
	cJSON_AddStringToObject(json_send, "IP", strIp);
	cJSON_AddStringToObject(json_send, "ssid", (char *)g_hf_config_file.sta_ssid);
	cJSON_AddStringToObject(json_send, "password", (char *)g_hf_config_file.sta_key);
	char time_s[20] = {0};
	get_time_string(time_s, 20);
	cJSON_AddStringToObject(json_send, "time", time_s);

	int i, j;
	char strTemp1[] = "plug_X";

	for (i = 0; i < PLUG_NUM; i++)
	{
		cJSON *json_plug_send = cJSON_CreateObject();
		cJSON_AddStringToObject(json_plug_send, "name", user_plug_config.plug[i].name);
		cJSON_AddNumberToObject(json_plug_send, "status", plug_status.plug[i]);

		cJSON *json_tasks_send = cJSON_CreateArray();
		for (j = 0; j < PLUG_TIME_TASK_NUM; j++)
		{
			char plug_task_config_str[100];
			sprintf(plug_task_config_str,
					"id: %d, time: %d:%d:%d, repeat:%d, days:%d,%d,%d,%d,%d,%d,%d, action:%d, enable:%d",
					j,
					user_plug_config.plug[i].task[j].hour, user_plug_config.plug[i].task[j].minute, user_plug_config.plug[i].task[j].second,
					user_plug_config.plug[i].task[j].repeat[0],
					1 * user_plug_config.plug[i].task[j].repeat[1],
					2 * user_plug_config.plug[i].task[j].repeat[2],
					3 * user_plug_config.plug[i].task[j].repeat[3],
					4 * user_plug_config.plug[i].task[j].repeat[4],
					5 * user_plug_config.plug[i].task[j].repeat[5],
					6 * user_plug_config.plug[i].task[j].repeat[6],
					7 * user_plug_config.plug[i].task[j].repeat[7],
					user_plug_config.plug[i].task[j].action,
					user_plug_config.plug[i].task[j].enable);
			cJSON_AddItemToArray(json_tasks_send, cJSON_CreateString(plug_task_config_str));
		}
		cJSON_AddItemToObject(json_plug_send, "tasks", json_tasks_send);

		strTemp1[5] = i + '0';
		cJSON_AddItemToObject(json_send, strTemp1, json_plug_send);
	}

	cJSON *json_mqtt_send = cJSON_CreateObject();
	cJSON_AddStringToObject(json_mqtt_send, "ip", user_mqtt_config.seraddr);
	cJSON_AddStringToObject(json_mqtt_send, "sub_topic", user_mqtt_config.sub_topic);
	cJSON_AddItemToObject(json_send, "mqtt", json_mqtt_send);

	char *json_str = cJSON_Print(json_send);
	strcpy(res, json_str);

	hfmem_free(json_str);
	cJSON_Delete(json_send);
}


void get_user_config_simple_str(char *res)
{
	cJSON *json_send = cJSON_CreateObject();
	cJSON_AddStringToObject(json_send, "mac", strMac);
	cJSON_AddStringToObject(json_send, "IP", strIp);
	cJSON_AddStringToObject(json_send, "ssid", (char *)g_hf_config_file.sta_ssid);
	cJSON_AddStringToObject(json_send, "password", (char *)g_hf_config_file.sta_key);
	char time_s[20] = {0};
	get_time_string(time_s, 20);
	cJSON_AddStringToObject(json_send, "time", time_s);

	int i, j;
	char strTemp1[] = "plug_X";

	for (i = 0; i < PLUG_NUM; i++)
	{
		cJSON *json_plug_send = cJSON_CreateObject();
		cJSON_AddStringToObject(json_plug_send, "name", user_plug_config.plug[i].name);
		cJSON_AddNumberToObject(json_plug_send, "status", plug_status.plug[i]);

		strTemp1[5] = i + '0';
		cJSON_AddItemToObject(json_send, strTemp1, json_plug_send);
	}

	cJSON *json_mqtt_send = cJSON_CreateObject();
	cJSON_AddStringToObject(json_mqtt_send, "ip", user_mqtt_config.seraddr);
	cJSON_AddStringToObject(json_mqtt_send, "sub_topic", user_mqtt_config.sub_topic);
	cJSON_AddNumberToObject(json_mqtt_send, "connected", mqtt_is_connected);

	cJSON_AddItemToObject(json_send, "mqtt", json_mqtt_send);
	cJSON_AddNumberToObject(json_send, "plug_config_loaded", plug_config_loaded);
	cJSON_AddNumberToObject(json_send, "plug_status_loaded", plug_status_loaded);
	cJSON_AddNumberToObject(json_send, "mqtt_config_loaded", mqtt_config_loaded);

	char *json_str = cJSON_Print(json_send);
	strcpy(res, json_str);

	hfmem_free(json_str);
	cJSON_Delete(json_send);
}


static void default_plug_config(PLUG_CONFIG *config)
{
	memset((char *)config, 0, sizeof(MQTT_CONFIG));
	for (uint8_t i = 0; i < PLUG_NUM; i++)
	{
		sprintf(config->plug[i].name, "default_name_%d", i);
		for (uint8_t j = 0; j < PLUG_TIME_TASK_NUM; j++) {
			config->plug[i].task[j].enable = 0;
			config->plug[i].task[j].enable = 0;
			config->plug[i].task[j].hour = 24;
			config->plug[i].task[j].minute = 60;
			config->plug[i].task[j].second = 60;
			// config->plug[i].task[j].repeat = {0, 0, 0, 0, 0, 0, 0, 0};
		}

	}
}

static void save_plug_config(PLUG_CONFIG *config)
{
	config->magic_head = PLUG_CONFIG_MAGIC_HEAD;
	config->crc = crc_calc((unsigned char *)config, sizeof(PLUG_CONFIG) - 1);
	hffile_userbin_write(PLUG_CONFIG_USERBIN_ADDR, (char *)config, sizeof(PLUG_CONFIG));
}


void init_plug_config(void)
{
	unsigned char crc;
	memset((char *)&user_plug_config, 0, sizeof(PLUG_CONFIG));
	hffile_userbin_read(PLUG_CONFIG_USERBIN_ADDR, (char *)&user_plug_config, sizeof(PLUG_CONFIG));
	crc = crc_calc((unsigned char *)&user_plug_config, sizeof(PLUG_CONFIG) - 1);
	if (!(PLUG_CONFIG_MAGIC_HEAD == user_plug_config.magic_head && crc == user_plug_config.crc))
	{
		default_plug_config(&user_plug_config);
		save_plug_config(&user_plug_config);
		plug_config_loaded = 2;
	} else {
		plug_config_loaded = 1;
	}
}


static void default_plug_status(PLUG_STATUS *config)
{
	memset((char *)config, 0, sizeof(PLUG_STATUS));
	for (uint8_t i = 0; i < PLUG_NUM; i++)
	{
		config->plug[i] = 1;
	}
}

static void save_plug_status(PLUG_STATUS *config)
{
	config->magic_head = PLUG_STATUS_MAGIC_HEAD;
	config->crc = crc_calc((unsigned char *)config, sizeof(PLUG_STATUS) - 1);
	hffile_userbin_write(PLUG_STATUS_USERBIN_ADDR, (char *)config, sizeof(PLUG_STATUS));
}


void init_plug_status(void)
{
	unsigned char crc;
	memset((char *)&plug_status, 0, sizeof(PLUG_STATUS));
	hffile_userbin_read(PLUG_STATUS_USERBIN_ADDR, (char *)&plug_status, sizeof(PLUG_STATUS));
	crc = crc_calc((unsigned char *)&plug_status, sizeof(PLUG_STATUS) - 1);
	if (!(PLUG_STATUS_MAGIC_HEAD == plug_status.magic_head && crc == plug_status.crc))
	{
		default_plug_status(&plug_status);
		save_plug_status(&plug_status);
		plug_status_loaded = 2;
	} else {
		plug_status_loaded = 1;
	}
}


void user_config_init()
{
	uint8_t i = 0;
	user_plug_config_enable = true;
	strcpy(ntpserver, "ntp1.aliyun.com");
	update_mqtt_config_flag = false;
	update_plug_config_flag = false;
	for (i = 0; i < PLUG_NUM; i++)
	{
		update_plug_status_flag[i] = false;
	}

	plug_config_loaded = 0;
	plug_status_loaded = 0;
	mqtt_config_loaded = 0;
	mqtt_is_connected = 0;

	press_flag = 0;
	release_flag = 0;
	last_press_time = 0;
	last_release_time = 0;

	// init_plug_config();
	// init_plug_status();
}