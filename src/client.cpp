/*
 *  libslp-sensor
 *
 * Copyright (c) 2000 - 2011 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact: JuHyun Kim <jh8212.kim@samsung.com>
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */ 



#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/un.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <sys/time.h>
#include <pthread.h>
#include <math.h>
#include <sys/time.h>
#include <time.h>

#include <cobject_type.h>

#include <csync.h>
#include <cmutex.h>
#include <clist.h>
#include <cworker.h>
#include <csock.h>

#include <cpacket.h>

#include <common.h>
#include <sf_common.h>

#include <vconf.h>
#include <glib.h>
#include <sensor.h>
#include <errno.h>

extern int errno;

#ifndef EXTAPI
#define EXTAPI __attribute__((visibility("default")))
#endif

#define MAX_BIND_SLOT				16
#define MAX_CB_SLOT_PER_BIND		16
#define MAX_CB_BIND_SLOT			64
#define MAX_EVENT_LIST				19

#define PITCH_MIN 		35
#define PITCH_MAX 		145

#define MAX_CHANNEL_NAME_LEN	50
#define BASE_GATHERING_INTERVAL	100

#define ACCEL_SENSOR_BASE_CHANNEL_NAME		"accel_datastream"
#define GEOMAG_SENSOR_BASE_CHANNEL_NAME	"geomag_datastream"
#define LIGHT_SENSOR_BASE_CHANNEL_NAME		"lumin_datastream"
#define PROXI_SENSOR_BASE_CHANNEL_NAME		"proxi_datastream"
#define MOTION_ENGINE_BASE_CHANNEL_NAME	"motion_datastream"
#define GYRO_SENSOR_BASE_CHANNEL_NAME		"gyro_datastream"
#define BAROMETER_SENSOR_BASE_CHANNEL_NAME       "barometer_datastream"
#define FUSION_SENSOR_BASE_CHANNEL_NAME "fusion_datastream"

#define ROTATION_0 0  
#define ROTATION_90 90  
#define ROTATION_180 180  
#define ROTATION_270 270  
#define ROTATION_360 360  
#define ROTATION_THD 45

#define RADIAN_VALUE (57.2957)
#define XY_POSITIVE_THD 2.0
#define XY_NEGATIVE_THD -2.0

#define ON_TIME_REQUEST_COUNTER 1

#define VCONF_SF_SERVER_POWER_OFF "memory/private/sensor/poweroff"

const char *STR_SF_CLIENT_IPC_SOCKET	= "/tmp/sf_socket";

static cmutex _lock;
static int g_system_off_cb_ct = 0;

enum _sensor_current_state {
	SENSOR_STATE_UNKNOWN = -1,
	SENSOR_STATE_STOPPED = 0,
	SENSOR_STATE_STARTED = 1,	
	SENSOR_STATE_PAUSED = 2
};

enum _sensor_wakeup_state {
	SENSOR_WAKEUP_UNKNOWN = -1,
	SENSOR_WAKEUP_UNSETTED = 0,
	SENSOR_WAKEUP_SETTED = 1,
};

enum _sensor_poweroff_state {
	SENSOR_POWEROFF_UNKNOWN = -1,
	SENSOR_POWEROFF_AWAKEN  =  1,
};

struct sf_bind_table_t {
	csock *ipc;	
	sensor_type_t sensor_type;	
	int cb_event_max_num;					/*limit by MAX_BIND_PER_CB_SLOT*/
	int cb_slot_num[MAX_CB_SLOT_PER_BIND];
	int my_handle;
	int sensor_state;
	int wakeup_state;
	int sensor_option;
};

struct cb_bind_table_t {
	char call_back_key[MAX_KEY_LEN];
	void *client_data;
	void (*sensor_callback_func_t)(unsigned int, sensor_event_data_t *, void *);
	unsigned int cb_event_type;
	int my_cb_handle;
	int my_sf_handle;
	
	unsigned int request_count;
	unsigned int request_data_id;
	void *collected_data;
	unsigned int current_collected_idx;
	
	GSource *source;
	guint gsource_interval;
	guint gID;
};

static struct rotation_event rotation_mode[] =
{
	{ ROTATION_UNKNOWN,	  {	ROTATION_UNKNOWN,		  ROTATION_UNKNOWN		   }},
	{ ROTATION_EVENT_90,  {	ROTATION_LANDSCAPE_LEFT,  ROTATION_PORTRAIT_BTM	   }},	
	{ ROTATION_EVENT_0,	  {	ROTATION_PORTRAIT_TOP,	  ROTATION_LANDSCAPE_LEFT  }},	
	{ ROTATION_EVENT_180, {	ROTATION_PORTRAIT_BTM,	  ROTATION_LANDSCAPE_RIGHT }},
	{ ROTATION_EVENT_270, {	ROTATION_LANDSCAPE_RIGHT, ROTATION_PORTRAIT_TOP	   }},
};

struct event_counter_t {
	const unsigned int event_type;
	unsigned int event_counter;
	unsigned int cb_list[MAX_BIND_SLOT];
};

static event_counter_t g_event_list[MAX_EVENT_LIST] = {
	{ ACCELEROMETER_EVENT_ROTATION_CHECK,      0, {0, }},
	{ ACCELEROMETER_EVENT_CALIBRATION_NEEDED,  0, {0, }},
	{ ACCELEROMETER_EVENT_SET_HORIZON,         0, {0, }},
	{ ACCELEROMETER_EVENT_SET_WAKEUP,          0, {0, }},
	{ GEOMAGNETIC_EVENT_CALIBRATION_NEEDED,    0, {0, }},
	{ PROXIMITY_EVENT_CHANGE_STATE, 		   0, {0, }},
	{ LIGHT_EVENT_CHANGE_LEVEL, 			   0, {0, }},
	{ MOTION_ENGINE_EVENT_SNAP, 			   0, {0, }},
	{ MOTION_ENGINE_EVENT_SHAKE, 			   0, {0, }},
	{ MOTION_ENGINE_EVENT_DOUBLETAP, 		   0, {0, }},
	{ MOTION_ENGINE_EVENT_PANNING, 			   0, {0, }},
	{ MOTION_ENGINE_EVENT_TOP_TO_BOTTOM, 	   0, {0, }},
	{ MOTION_ENGINE_EVENT_DIRECT_CALL, 	       0, {0, }},
	{ MOTION_ENGINE_EVENT_TILT_TO_UNLOCK,      0, {0, }},
	{ MOTION_ENGINE_EVENT_LOCK_EXECUTE_CAMERA, 0, {0, }},
	{ MOTION_ENGINE_EVENT_SMART_ALERT	 , 0, {0, }},
	{ MOTION_ENGINE_EVENT_TILT		, 0, {0, }},
	{ MOTION_ENGINE_EVENT_PANNING_BROWSE	, 0, {0, }},
	{ MOTION_ENGINE_EVENT_NO_MOVE    , 0, {0, }},
};

static sf_bind_table_t g_bind_table[MAX_BIND_SLOT];

static cb_bind_table_t g_cb_table[MAX_CB_BIND_SLOT];

static gboolean sensor_timeout_handler(gpointer data);

inline static void add_cb_number(int list_slot, unsigned int cb_number)
{
	if(list_slot < 0 || list_slot > MAX_EVENT_LIST - 1)
		return;

	unsigned int i = 0;
	const unsigned int EVENT_COUNTER = g_event_list[list_slot].event_counter;

	for(i = 0 ; i < EVENT_COUNTER ; i++) {
		if(g_event_list[list_slot].cb_list[i] == cb_number){
			return;
		}
	}

	if(EVENT_COUNTER < MAX_BIND_SLOT) {
		g_event_list[list_slot].event_counter++;
		g_event_list[list_slot].cb_list[EVENT_COUNTER] = cb_number;
	} else {
		return ;
	}
}


inline static void del_cb_number(int list_slot, unsigned int cb_number)
{
	if(list_slot < 0 || list_slot > MAX_EVENT_LIST - 1)
		return;

	unsigned int i = 0, j = 0;
	const unsigned int EVENT_COUNTER = g_event_list[list_slot].event_counter;

	for(i = 0 ; i < EVENT_COUNTER ; i++){
		if(g_event_list[list_slot].cb_list[i] == cb_number){
			for(j = i ; j < EVENT_COUNTER - 1; j++){
				g_event_list[list_slot].cb_list[j] = g_event_list[list_slot].cb_list[j+1];
			}
			g_event_list[list_slot].cb_list[EVENT_COUNTER - 1] = 0;
			g_event_list[list_slot].event_counter--;
			return ;
		}
	}

	DBG("cb number [%d] is not registered\n", cb_number);
}


inline static void del_cb_by_event_type(unsigned int event_type, unsigned int cb_number)
{
	int list_slot = 0;

	for(list_slot = 0 ; list_slot < MAX_EVENT_LIST ; list_slot++)
		if(g_event_list[list_slot].event_type == event_type)
			break;

	del_cb_number(list_slot, cb_number);
}


inline static int acquire_handle(void)
{
	register int i;
	_lock.lock();
	for (i = 0; i < MAX_BIND_SLOT; i ++) {
		if (g_bind_table[i].ipc == NULL) break;
	}
	_lock.unlock();

	return i;
}


inline static int cb_acquire_handle(void)
{
	register int i;
	_lock.lock();
	for (i = 0; i < MAX_CB_BIND_SLOT; i ++) {
		if (g_cb_table[i].sensor_callback_func_t == NULL) break;
	}
	_lock.unlock();

	return i;
}


inline static void release_handle(int i)
{
	register int j;
	
	_lock.lock();
	delete g_bind_table[i].ipc;
	g_bind_table[i].ipc = NULL;
	g_bind_table[i].sensor_type = UNKNOWN_SENSOR;
	
	g_bind_table[i].my_handle = -1;
	g_bind_table[i].sensor_state = SENSOR_STATE_UNKNOWN;
	g_bind_table[i].wakeup_state = SENSOR_WAKEUP_UNKNOWN;
	g_bind_table[i].sensor_option = SENSOR_OPTION_DEFAULT;

	for (j=0; j<g_bind_table[i].cb_event_max_num; j++) {
		if (   (j<MAX_CB_SLOT_PER_BIND) && (g_bind_table[i].cb_slot_num[j] > -1)  ) {
			del_cb_by_event_type(g_cb_table[g_bind_table[i].cb_slot_num[j]].cb_event_type, g_bind_table[i].cb_slot_num[j]);
			g_cb_table[g_bind_table[i].cb_slot_num[j]].client_data= NULL;
			g_cb_table[g_bind_table[i].cb_slot_num[j]].sensor_callback_func_t = NULL;
			g_cb_table[g_bind_table[i].cb_slot_num[j]].cb_event_type = 0x00;
			g_cb_table[g_bind_table[i].cb_slot_num[j]].my_cb_handle = -1;
			g_bind_table[i].cb_slot_num[j] = -1;
		}
	}
	
	g_bind_table[i].cb_event_max_num = 0;
	
	_lock.unlock();
}


inline static void cb_release_handle(int i)
{
	_lock.lock();
	g_cb_table[i].client_data= NULL;
	g_cb_table[i].sensor_callback_func_t = NULL;
	g_cb_table[i].cb_event_type = 0x00;
	g_cb_table[i].my_cb_handle = -1;
	g_cb_table[i].my_sf_handle = -1;
	
	g_cb_table[i].request_count = 0;
	g_cb_table[i].request_data_id = 0;
	
	if ( g_cb_table[i].collected_data )
	{
		free (g_cb_table[i].collected_data);
		g_cb_table[i].collected_data = NULL;
	}
	
	g_cb_table[i].collected_data = NULL;
	g_cb_table[i].current_collected_idx = 0;

	g_cb_table[i].source = NULL;
	g_cb_table[i].gsource_interval = 0;
	g_cb_table[i].gID = 0;
	_lock.unlock();
}


void power_off_cb(keynode_t *node, void *data)
{
	int val = -1;
	int handle = -1, j = -1;
	int state = -1;

	if(vconf_keynode_get_type(node) != VCONF_TYPE_INT)
	{
		ERR("Errer invailed keytype");
		return;
	}

	val = vconf_keynode_get_int(node);

	switch(val)
	{
		case SENSOR_POWEROFF_AWAKEN:
			for(handle = 0 ; handle < MAX_BIND_SLOT ; handle++)
			{
				if(g_bind_table[handle].ipc != NULL)
				{
					state = sf_stop(handle);

					if(state < 0)
					{
						ERR("Cannot stop handle [%d]",handle);
						continue;
					}
					else
					{
						DBG("LCD OFF and sensor handle [%d] stopped",handle);
					}

					for(j = 0 ; j < g_bind_table[handle].cb_event_max_num ; j++)
					{
						if((j<MAX_CB_SLOT_PER_BIND) && (g_bind_table[handle].cb_slot_num[j] > -1))
						{
							state = sf_unregister_event(handle ,g_cb_table[g_bind_table[handle].cb_slot_num[j]].cb_event_type);

							if(state < 0)
								ERR("cannot unregster_event for event [%x], handle [%d]",g_cb_table[g_bind_table[handle].cb_slot_num[j]].cb_event_type,	handle);
						}
					}

					sf_disconnect(handle);
				}
			}
			break;

		default:
			break;

	}
}


void lcd_off_cb(keynode_t *node, void *data)
{
	int val = -1;
	int i = -1, j = -1;

	if(vconf_keynode_get_type(node) != VCONF_TYPE_INT)
	{
		ERR("Errer invailed keytype");
		return;
	}

	val = vconf_keynode_get_int(node);

	switch(val)
	{
		case VCONFKEY_PM_STATE_LCDOFF:   // LCD OFF
			for(i = 0 ; i < MAX_BIND_SLOT ; i++)
			{
				if((g_bind_table[i].wakeup_state != SENSOR_WAKEUP_SETTED && g_bind_table[i].sensor_option != SENSOR_OPTION_ALWAYS_ON ) && g_bind_table[i].ipc != NULL)
				{
					if(g_bind_table[i].sensor_state == SENSOR_STATE_STARTED)
					{
						if(sf_stop(i) < 0)
						{
							ERR("Cannot stop handle [%d]",i);
						}
						else
						{
							g_bind_table[i].sensor_state = SENSOR_STATE_PAUSED;

							for(j = 0; j < g_bind_table[i].cb_event_max_num; j++) 
							{
								if(g_cb_table[g_bind_table[i].cb_slot_num[j]].cb_event_type != 0)
									DBG("LCD OFF stopped event_type [%x]",g_cb_table[g_bind_table[i].cb_slot_num[j]].cb_event_type);
								if(g_cb_table[g_bind_table[i].cb_slot_num[j]].collected_data !=  NULL)
								{
									g_source_destroy(g_cb_table[g_bind_table[i].cb_slot_num[j]].source);
									g_source_unref(g_cb_table[g_bind_table[i].cb_slot_num[j]].source);
									g_cb_table[g_bind_table[i].cb_slot_num[j]].source = NULL;
								}
							}

						}
					}
					DBG("LCD OFF and sensor handle [%d] stopped",i);
				}
			}

			break;
		case VCONFKEY_PM_STATE_NORMAL:  // LCD ON
			for(i = 0 ; i < MAX_BIND_SLOT ; i++)
			{
				if(g_bind_table[i].sensor_state == SENSOR_STATE_PAUSED)
				{
					if(sf_start(i,g_bind_table[i].sensor_option) < 0)
					{
						ERR("Cannot start handle [%d]",i);
					}
					else
					{
						for(j = 0; j < g_bind_table[i].cb_event_max_num; j++) 
						{
							if(g_cb_table[g_bind_table[i].cb_slot_num[j]].cb_event_type != 0)
								DBG("LCD ON started event_type [%x]",g_cb_table[g_bind_table[i].cb_slot_num[j]].cb_event_type);
							if(g_cb_table[g_bind_table[i].cb_slot_num[j]].collected_data !=  NULL)
							{
								g_cb_table[g_bind_table[i].cb_slot_num[j]].source =	g_timeout_source_new(g_cb_table[g_bind_table[i].cb_slot_num[j]].gsource_interval);
								g_source_set_callback(g_cb_table[g_bind_table[i].cb_slot_num[j]].source, sensor_timeout_handler, (gpointer)&g_cb_table[g_bind_table[i].cb_slot_num[j]].my_cb_handle,NULL);
								g_cb_table[g_bind_table[i].cb_slot_num[j]].gID = g_source_attach(g_cb_table[g_bind_table[i].cb_slot_num[j]].source,	NULL);
							}
						}
					}
					DBG("LCD ON and sensor handle [%d] started",i);
				}
			}

			break;
		default :
			break ;
	}
}

int system_off_set(void)
{
	int result = -1;
	if(g_system_off_cb_ct == 0)
	{
		result = vconf_notify_key_changed(VCONFKEY_PM_STATE, lcd_off_cb, NULL);
		if(result < 0)
		{
			ERR("cannot setting lcd_off_set_cb");
		}
		
		result = vconf_notify_key_changed(VCONF_SF_SERVER_POWER_OFF, power_off_cb, NULL);
		if(result < 0)
		{
			ERR("cannot setting power_off_set_cb");
		}

		g_system_off_cb_ct++;
	}
	else if (g_system_off_cb_ct > 0)
	{
		g_system_off_cb_ct++;
	}
	else
	{
		ERR("g_system_off_cb_ct is negative");
		return -1;
	}

	DBG("system_off_set success");
	return 1;
}
	
int system_off_unset(void)
{
	int result = -1;
	if(g_system_off_cb_ct == 1)
	{
		result = vconf_ignore_key_changed(VCONFKEY_PM_STATE, lcd_off_cb);
		if(result < 0)
		{
			ERR("cannot setting lcd_off_set_cb");
		}

		result = vconf_ignore_key_changed(VCONF_SF_SERVER_POWER_OFF, power_off_cb);
		if(result < 0)
		{
			ERR("cannot setting power_off_set_cb");
		}

		g_system_off_cb_ct = 0;
	}
	else if (g_system_off_cb_ct > 1)
	{
		g_system_off_cb_ct--;
	}
	else
	{
		ERR("g_system_off_cb_ct is negative");
		return -1;
	}
	DBG("system_off_unset success");

	return 1;
}
	

void lcd_off_set_wake_up(keynode_t *node, void *data)
{
	int state = 0;

	state = sf_set_property(ACCELEROMETER_SENSOR, ACCELEROMETER_PROPERTY_SET_WAKEUP, WAKEUP_SET);

	if(state < 0)
	{
		ERR("ACCELEROMETER_PROPERTY_SET_WAKEUP  fail");
	}
}


static void sensor_changed_cb(keynode_t *node, void *data)
{
	int event_number = (int)(data);
	unsigned int i = 0;
	int val;
	int cb_number = 0;
	sensor_event_data_t cb_data;
	sensor_panning_data_t panning_data;

	if(!node)
	{
		ERR("Node is NULL");
		return;
	}

	if (vconf_keynode_get_type(node) !=  VCONF_TYPE_INT )
	{
		ERR("Err invaild key_type , incomming key_type : %d , key_name : %s , key_value : %d", vconf_keynode_get_type(node), vconf_keynode_get_name(node),vconf_keynode_get_int(node));
		return;
	}

	val = vconf_keynode_get_int(node);

	for( i = 0 ; i < g_event_list[event_number].event_counter ; i++)
	{
		cb_number = g_event_list[event_number].cb_list[i];

		if(g_bind_table[g_cb_table[cb_number].my_sf_handle].sensor_state ==	SENSOR_STATE_STARTED)
		{
			if (g_cb_table[cb_number].sensor_callback_func_t)
			{
				if(g_cb_table[cb_number].cb_event_type == MOTION_ENGINE_EVENT_PANNING || g_cb_table[cb_number].cb_event_type == MOTION_ENGINE_EVENT_TILT || g_cb_table[cb_number].cb_event_type == MOTION_ENGINE_EVENT_PANNING_BROWSE)
				{
					if(val != 0)
					{
						panning_data.x = (short)(val >> 16);
						panning_data.y = (short)(val & 0x0000FFFF);
						cb_data.event_data_size = sizeof(sensor_panning_data_t);
						cb_data.event_data = (void *)&panning_data;
						g_cb_table[cb_number].sensor_callback_func_t(g_cb_table[cb_number].cb_event_type,&cb_data, g_cb_table[cb_number].client_data);
					}
				}
				else
				{
					if ( val<0 )
					{
						ERR("vconf_keynode_get_int fail for key : %s , handle_num : %d ,get_value : %d\n",g_cb_table[cb_number].call_back_key,cb_number , val);
						return ;
					}

					switch (g_cb_table[cb_number].cb_event_type) {
						case ACCELEROMETER_EVENT_SET_WAKEUP :
							/* fall through */
						case ACCELEROMETER_EVENT_ROTATION_CHECK :
							/* fall through */
						case ACCELEROMETER_EVENT_SET_HORIZON :
							/* fall through */
						case GEOMAGNETIC_EVENT_CALIBRATION_NEEDED :
							/* fall through */
						case LIGHT_EVENT_CHANGE_LEVEL :
							/* fall through */
						case PROXIMITY_EVENT_CHANGE_STATE:
							/* fall through */
						case MOTION_ENGINE_EVENT_SNAP:
							/* fall through */
						case MOTION_ENGINE_EVENT_SHAKE:
							/* fall through */
						case MOTION_ENGINE_EVENT_DOUBLETAP:
							/* fall through */
						case MOTION_ENGINE_EVENT_TOP_TO_BOTTOM:
							/* fall through */
						case MOTION_ENGINE_EVENT_DIRECT_CALL:
							/* fall through */
						case MOTION_ENGINE_EVENT_TILT_TO_UNLOCK:
							/* fall through */
						case MOTION_ENGINE_EVENT_LOCK_EXECUTE_CAMERA:
							/* fall through */
						case MOTION_ENGINE_EVENT_SMART_ALERT:
                                                        /* fall through */
                                                case MOTION_ENGINE_EVENT_NO_MOVE:
							cb_data.event_data_size = sizeof(val);
							cb_data.event_data = (void *)&val;
							g_cb_table[cb_number].sensor_callback_func_t(g_cb_table[cb_number].cb_event_type, &cb_data , g_cb_table[cb_number].client_data);
							break;
						default :
							ERR("Undefined cb_event_type");
							return ;
							break;
					}
				}
			}
			else
			{
				ERR("Empty Callback func in event : %x\n",g_cb_table[cb_number].cb_event_type);
			}
		}
		else
		{
			ERR("Sensor doesn't start for event : %x",g_cb_table[cb_number].cb_event_type);
		}
	}
}


static gboolean sensor_timeout_handler(gpointer data)
{
	int *cb_handle = (int*)(data);
	int state;
	sensor_event_data_t cb_data;

	if ( g_bind_table[g_cb_table[*cb_handle].my_sf_handle].sensor_state != SENSOR_STATE_STARTED ) {
		return TRUE;
	}

	if (g_cb_table[*cb_handle].sensor_callback_func_t) {		

		if ( ((g_cb_table[*cb_handle].request_data_id & 0xFFFF) > 0) && ((g_cb_table[*cb_handle].request_data_id & 0xFFFF) < 10) ) {
			sensor_data_t *base_data_values;
			base_data_values =(sensor_data_t *)g_cb_table[*cb_handle].collected_data;		

			if ( !base_data_values ) {
				ERR("ERR get  saved_gather_data stuct fail in sensor_timeout_handler \n");
				return FALSE;
			}
			state = sf_get_data(g_cb_table[*cb_handle].my_sf_handle, g_cb_table[*cb_handle].request_data_id, base_data_values);
		
			if ( state < 0 ) {
				ERR("ERR sensor_get_struct_data fail in sensor_timeout_handler : %d\n",state);
				return TRUE;
			}

			cb_data.event_data_size = sizeof (sensor_data_t);
			cb_data.event_data = g_cb_table[*cb_handle].collected_data;

			g_cb_table[*cb_handle].sensor_callback_func_t( g_cb_table[*cb_handle].cb_event_type , &cb_data , g_cb_table[*cb_handle].client_data);


		} else {
			ERR("Does not support data_type");
			return TRUE;
		}			

		
	} else {
		ERR("Empty Callback func in cb_handle : %d\n", *cb_handle);
	}

	return TRUE;
}

///////////////////////////////////for internal ///////////////////////////////////
static int server_get_properties(int handle , unsigned int data_id, void *property_data)
{
	cpacket *packet;

	try {
		packet = new cpacket(sizeof(cmd_return_property_t) + sizeof(base_property_struct)+ 4);
	} catch (int ErrNo) {
		ERR("packet creation fail , errno : %d , errstr : %s\n",ErrNo, strerror(ErrNo));
		return -1;
	}

	cmd_get_property_t *cmd_payload;

	INFO("server_get_properties called with handle : %d \n", handle);
	if (handle < 0) {
		ERR("Invalid handle\n");
		errno = EINVAL;
		delete packet;
		return -1;
	}

	cmd_payload = (cmd_get_property_t*)packet->data();
	if (!cmd_payload) {
		ERR("cannot find memory for send packet->data");
		errno = ENOMEM;
		delete packet;
		return -2;
	}	

	packet->set_version(PROTOCOL_VERSION);
	packet->set_cmd(CMD_GET_PROPERTY);
	packet->set_payload_size(sizeof(cmd_get_property_t));

	if(data_id)
		cmd_payload->get_level = data_id;
	else
		cmd_payload->get_level = ((unsigned int)g_bind_table[handle].sensor_type<<16) | 0x0001;



	INFO("Send CMD_GET_PROPERTY command\n");
	if (g_bind_table[handle].ipc && g_bind_table[handle].ipc->send(packet->packet(), packet->size()) == false) {
		ERR("Faield to send a packet\n");		
		release_handle(handle);
		errno = ECOMM;
		delete packet;
		return -2;
	}

	if (g_bind_table[handle].ipc && g_bind_table[handle].ipc->recv(packet->packet(), packet->header_size()) == false) {
		ERR("Faield to receive a packet\n");
		release_handle(handle);
		errno = ECOMM;
		delete packet;
		return -2;
	}

	if (packet->payload_size()) {
		if (g_bind_table[handle].ipc && g_bind_table[handle].ipc->recv((char*)packet->packet() + packet->header_size(), packet->payload_size()) == false) {
			ERR("Faield to receive a packet\n");
			release_handle(handle);
			errno = ECOMM;
			delete packet;
			return -2;
		}

		if (packet->cmd() == CMD_GET_PROPERTY) {
			cmd_return_property_t *return_payload;
			return_payload = (cmd_return_property_t*)packet->data();
			if (return_payload->state < 0) {
				ERR("sever get property fail , return state : %d\n",return_payload->state);
				errno = EBADE;
				delete packet;
				return -2;
			} else {	
				base_property_struct *base_return_property;
				base_return_property = (base_property_struct *)return_payload->property_struct;
				
				if(data_id)
				{
					sensor_data_properties_t *return_properties;
					return_properties = (sensor_data_properties_t *)property_data;
					return_properties->sensor_unit_idx = base_return_property->sensor_unit_idx ;
					return_properties->sensor_min_range= base_return_property->sensor_min_range;
					return_properties->sensor_max_range= base_return_property->sensor_max_range;
					return_properties->sensor_resolution = base_return_property->sensor_resolution;				
				}
				else
				{
					sensor_properties_t *return_properties;
					return_properties = (sensor_properties_t *)property_data;
					return_properties->sensor_unit_idx = base_return_property->sensor_unit_idx ;
					return_properties->sensor_min_range= base_return_property->sensor_min_range;
					return_properties->sensor_max_range= base_return_property->sensor_max_range;
					return_properties->sensor_resolution = base_return_property->sensor_resolution;
					memset(return_properties->sensor_name, '\0', sizeof(return_properties->sensor_name));
					memset(return_properties->sensor_vendor, '\0', sizeof(return_properties->sensor_vendor));
					strncpy(return_properties->sensor_name, base_return_property->sensor_name, strlen(base_return_property->sensor_name));
					strncpy(return_properties->sensor_vendor, base_return_property->sensor_vendor, strlen(base_return_property->sensor_vendor));
				}
			}
		} else {
			ERR("unexpected server cmd\n");
			errno = EBADE;
			delete packet;
			return -2;
		}
	}

	delete packet;
	return 0;
}


static int server_set_property(int handle , unsigned int property_id, long value )
{
	cpacket *packet;
        try {
                packet = new cpacket(sizeof(cmd_set_value_t) + 4);
        } catch (int ErrNo) {
                ERR("packet creation fail , errno : %d , errstr : %s\n",ErrNo, strerror(ErrNo));
                return -1;
        }
	cmd_set_value_t *cmd_payload;
	INFO("server_set_property called with handle : %d \n", handle);
	if (handle < 0) {
		ERR("Invalid handle\n");
		errno = EINVAL;
		delete packet;
		return -1;
	}

	cmd_payload = (cmd_set_value_t*)packet->data();
	if (!cmd_payload) {
		ERR("cannot find memory for send packet->data");
		errno = ENOMEM;
		delete packet;
		return -2;
	}	

	packet->set_version(PROTOCOL_VERSION);
	packet->set_cmd(CMD_SET_VALUE);
	packet->set_payload_size(sizeof(cmd_set_value_t));

	cmd_payload->sensor_type = g_bind_table[handle].sensor_type;
	cmd_payload->property = property_id;
	cmd_payload->value = value;


	INFO("Send CMD_SET_VALUE command\n");
	if (g_bind_table[handle].ipc && g_bind_table[handle].ipc->send(packet->packet(), packet->size()) == false) {
		ERR("Faield to send a packet\n");		
		release_handle(handle);
		errno = ECOMM;
		delete packet;
		return -2;
	}

	if (g_bind_table[handle].ipc && g_bind_table[handle].ipc->recv(packet->packet(), packet->header_size()) == false) {
		ERR("Faield to receive a packet\n");
		release_handle(handle);
		errno = ECOMM;
		delete packet;
		return -2;
	}

	if (packet->payload_size()) {
		if (g_bind_table[handle].ipc && g_bind_table[handle].ipc->recv((char*)packet->packet() + packet->header_size(), packet->payload_size()) == false) {
			ERR("Faield to receive a packet\n");
			release_handle(handle);
			errno = ECOMM;
			delete packet;
			return -2;
		}

		if (packet->cmd() == CMD_DONE) {
			cmd_done_t *payload;
			payload = (cmd_done_t*)packet->data();

			if (payload->value == -1) {
				ERR("cannot support input property\n");
				errno = ENODEV;
				delete packet;
				return -1;
			} 
		} else {
			ERR("unexpected server cmd\n");
			errno = ECOMM;
			delete packet;
			return -2;
		}
	}

	delete packet;
	return 0;
}

///////////////////////////////////for external ///////////////////////////////////

EXTAPI int sf_is_sensor_event_available ( sensor_type_t desired_sensor_type , unsigned int desired_event_type )
{
	int handle;
	cpacket *packet;
        try {
                packet = new cpacket(sizeof(cmd_reg_t)+4);
        } catch (int ErrNo) {
                ERR("packet creation fail , errno : %d , errstr : %s\n",ErrNo, strerror(ErrNo));
                return -1;
        }
	cmd_reg_t *payload;
	
	handle = sf_connect(desired_sensor_type);
	if ( handle < 0 ) {
		errno = ENODEV;
		delete packet;
		return -2;		
	} 

	if ( desired_event_type != 0 ) {

		payload = (cmd_reg_t*)packet->data();
		if (!payload) {
			ERR("cannot find memory for send packet->data");
			errno = ENOMEM;
			delete packet;
			return -2;
		}

		packet->set_version(PROTOCOL_VERSION);
		packet->set_cmd(CMD_REG);
		packet->set_payload_size(sizeof(cmd_reg_t));
		payload->type = REG_CHK;
		payload->event_type = desired_event_type;

		INFO("Send CMD_REG command\n");
		if (g_bind_table[handle].ipc && g_bind_table[handle].ipc->send(packet->packet(), packet->size()) == false) {
			ERR("Faield to send a packet\n");		
			release_handle(handle);
			errno = ECOMM;
			delete packet;
			return -2;
		}

		if (g_bind_table[handle].ipc && g_bind_table[handle].ipc->recv(packet->packet(), packet->header_size()) == false) {
			ERR("Faield to receive a packet\n");
			release_handle(handle);
			errno = ECOMM;
			delete packet;
			return -2;
		}

		if (packet->payload_size()) {
			if (g_bind_table[handle].ipc && g_bind_table[handle].ipc->recv((char*)packet->packet() + packet->header_size(), packet->payload_size()) == false) {
				ERR("Faield to receive a packet\n");
				release_handle(handle);
				errno = ECOMM;
				delete packet;
				return -2;
			}

			if (packet->cmd() == CMD_DONE) {
				cmd_done_t *payload;
				payload = (cmd_done_t*)packet->data();
				if (payload->value == -1) {
					ERR("sever check fail\n");
					errno = ENODEV;
					delete packet;
					return -2;
				} 
			} else {
				ERR("unexpected server cmd\n");
				errno = ECOMM;
				delete packet;
				return -2;
			}
		}
		
	} 
	
	sf_disconnect(handle);
	delete packet;

	return 0;
}

EXTAPI int sf_get_data_properties(unsigned int data_id, sensor_data_properties_t *return_data_properties)
{
	int handle;
	int state = -1;
		
	retvm_if( (!return_data_properties )  , -1 , "Invalid return properties pointer : %p", return_data_properties);


	handle = sf_connect((sensor_type_t)(data_id >> 16));
	if ( handle < 0 ) {
		ERR("Sensor connet fail !! for : %x \n", (data_id >> 16));
		return -1;		
	} else {
		state = server_get_properties( handle , data_id, return_data_properties );
		if ( state < 0 ) {
			ERR("server_get_properties fail , state : %d \n",state);		
		}
		sf_disconnect(handle);		
	}	

	return state;
}

EXTAPI int sf_get_properties(sensor_type_t sensor_type, sensor_properties_t *return_properties)
{
	int handle;
	int state = -1;
		
	retvm_if( (!return_properties )  , -1 , "Invalid return properties pointer : %p", return_properties);

	handle = sf_connect(sensor_type);
	if ( handle < 0 ) {
		ERR("Sensor connet fail !! for : %x \n", sensor_type);
		return -1;		
	} else {
		state = server_get_properties( handle , 0, return_properties );
		if ( state < 0 ) {
			ERR("server_get_properties fail , state : %d \n",state);		
		}
		sf_disconnect(handle);		
	}	

	return state;
}


EXTAPI int sf_set_property(sensor_type_t sensor_type, unsigned int property_id, long value)
{
	int handle;
	int state = -1;
		
	handle = sf_connect(sensor_type);
	if ( handle < 0 ) {
		ERR("Sensor connet fail !! for : %x \n", sensor_type);
		return -1;		
	} else {
		state = server_set_property( handle , property_id, value );
		if ( state < 0 ) {
			ERR("server_set_property fail , state : %d \n",state);		
		}
		sf_disconnect(handle);		
	}	

	return state;
}


EXTAPI int sf_connect(sensor_type_t sensor_type)
{
	register int i, j;
	cpacket *packet;
        try {
                packet = new cpacket(sizeof(cmd_hello_t)+MAX_CHANNEL_NAME_LEN+4);
        } catch (int ErrNo) {
                ERR("packet creation fail , errno : %d , errstr : %s\n",ErrNo, strerror(ErrNo));
                return -1;
        }
	cmd_hello_t *payload;
	cmd_done_t *return_payload;
	
	pid_t cpid;
	size_t channel_name_length;

	const char *sf_channel_name = NULL;

	cpid = getpid();

	INFO("Sensor_attach_channel from pid : %d , to sensor_type : %x",cpid ,sensor_type);
	
	i = acquire_handle();
	if (i == MAX_BIND_SLOT) {
		ERR("MAX_BIND_SLOT, Too many slot required");
		errno = ENOMEM;
		delete packet;
		return -2;
	}
	
	INFO("Empty slot : %d\n", i);

	switch (sensor_type) {
		case ACCELEROMETER_SENSOR :
			sf_channel_name = (char *)ACCEL_SENSOR_BASE_CHANNEL_NAME;
			g_bind_table[i].cb_event_max_num = 8;
			break;
			
		case GEOMAGNETIC_SENSOR :
			sf_channel_name = (char *)GEOMAG_SENSOR_BASE_CHANNEL_NAME;
			g_bind_table[i].cb_event_max_num = 3;
			break;
			
		case LIGHT_SENSOR:
			sf_channel_name = (char *)LIGHT_SENSOR_BASE_CHANNEL_NAME;
			g_bind_table[i].cb_event_max_num = 3;
			break;
			
		case PROXIMITY_SENSOR:
			sf_channel_name = (char *)PROXI_SENSOR_BASE_CHANNEL_NAME;
			g_bind_table[i].cb_event_max_num = 3;
			break;

		case MOTION_SENSOR:
			sf_channel_name = (char *)MOTION_ENGINE_BASE_CHANNEL_NAME;
			g_bind_table[i].cb_event_max_num = 12;
			break;

		case GYROSCOPE_SENSOR:
			sf_channel_name = (char *)GYRO_SENSOR_BASE_CHANNEL_NAME;
			g_bind_table[i].cb_event_max_num = 1;
			break;
			
		case THERMOMETER_SENSOR:		
			break;
		case BAROMETER_SENSOR:
			sf_channel_name = (char *)BAROMETER_SENSOR_BASE_CHANNEL_NAME;
			g_bind_table[i].cb_event_max_num = 3;
			break;
		case FUSION_SENSOR:
			sf_channel_name = (char *)FUSION_SENSOR_BASE_CHANNEL_NAME;
			g_bind_table[i].cb_event_max_num = 3;
			break;

		case UNKNOWN_SENSOR:
		default :
			ERR("Undefined sensor_type");
			release_handle(i);
			errno = ENODEV;
			delete packet;
			return -2;
			break;
	}

	g_bind_table[i].sensor_type = sensor_type ;
	g_bind_table[i].my_handle = i;
	g_bind_table[i].sensor_state = SENSOR_STATE_STOPPED;
	g_bind_table[i].wakeup_state = SENSOR_WAKEUP_UNSETTED;
	g_bind_table[i].sensor_option = SENSOR_OPTION_DEFAULT;

	for(j = 0 ; j < g_bind_table[i].cb_event_max_num  ; j++)
		g_bind_table[i].cb_slot_num[j] = -1;

	try {
		g_bind_table[i].ipc = new csock( (char *)STR_SF_CLIENT_IPC_SOCKET, csock::SOCK_TCP|csock::SOCK_IPC|csock::SOCK_WORKER, 0, 0);
	} catch (...) {
		release_handle(i);
		errno = ECOMM;
		delete packet;
		return -2;
	}

	if (g_bind_table[i].ipc && g_bind_table[i].ipc->connect_to_server() == false) {
		delete g_bind_table[i].ipc;
		g_bind_table[i].ipc = NULL;
		release_handle(i);
		errno = ECOMM;
		delete packet;
		return -2;
	}


	INFO("Connected to server\n");
	payload = (cmd_hello_t*)packet->data();
	if (!payload) {
		ERR("cannot find memory for send packet->data");
		release_handle(i);
		errno = ENOMEM;
		delete packet;
		return -2;
	}

	if (!sf_channel_name ) {
		ERR("cannot find matched-sensor name!!!");
		release_handle(i);
		errno = ENODEV;
		delete packet;
		return -2;
	} else {
		channel_name_length = strlen(sf_channel_name);
		if ( channel_name_length > MAX_CHANNEL_NAME_LEN  ) {
			ERR("error, channel_name_length too long !!!");
			release_handle(i);
			errno = EINVAL;
			delete packet;
			return -1;
		}
	}
	
	packet->set_version(PROTOCOL_VERSION);
	packet->set_cmd(CMD_HELLO);
	packet->set_payload_size(sizeof(cmd_hello_t) + channel_name_length);
	strcpy(payload->name, sf_channel_name);

	if (g_bind_table[i].ipc && g_bind_table[i].ipc->send(packet->packet(), packet->size()) == false) {
		ERR("Failed to send a hello packet\n");
		release_handle(i);
		errno = ECOMM;
		delete packet;
		return -2;
	}

	INFO("Wait for recv a reply packet\n");
	if (g_bind_table[i].ipc && g_bind_table[i].ipc->recv(packet->packet(), packet->header_size()) == false) {
		release_handle(i);
		errno = ECOMM;
		delete packet;
		return -2;
	}

	if (packet->payload_size()) {
		if (g_bind_table[i].ipc && g_bind_table[i].ipc->recv((char*)packet->packet() + packet->header_size(), packet->payload_size()) == false) {
			release_handle(i);
			errno = ECOMM;
			delete packet;
			return -2;
		}
	}

	return_payload = (cmd_done_t*)packet->data();
	if (!return_payload) {
		ERR("cannot find memory for return packet->data");
		release_handle(i);
		delete packet;
		return -1;
	}

	if ( return_payload->value < 0) {
		ERR("There is no sensor \n");
		release_handle(i);
		delete packet;
		return -1;
	}

	system_off_set();

	INFO("Connected sensor type : %x , handle : %d \n", sensor_type , i);
	delete packet;
	return i;	
}

EXTAPI int sf_disconnect(int handle)
{
	cpacket *packet;
	cmd_byebye_t *payload;

	retvm_if( handle > MAX_BIND_SLOT - 1 , -1 , "Incorrect handle");
	retvm_if( (g_bind_table[handle].ipc == NULL) ||(handle < 0) , -1 , "sensor_detach_channel fail , invalid handle value : %d",handle);

        try {
                packet = new cpacket(sizeof(cmd_byebye_t)+4);
        } catch (int ErrNo) {
                ERR("packet creation fail , errno : %d , errstr : %s\n",ErrNo, strerror(ErrNo));
                return -1;
        }

	INFO("Detach, so remove %d from the table\n", handle);

	payload = (cmd_byebye_t*)packet->data();
	if (!payload) {
		ERR("cannot find memory for send packet->data");
		errno = ENOMEM;
		delete packet;
		return -2;
	}

	packet->set_version(PROTOCOL_VERSION);
	packet->set_cmd(CMD_BYEBYE);
	packet->set_payload_size(sizeof(cmd_byebye_t));

	if (g_bind_table[handle].ipc && g_bind_table[handle].ipc->send(packet->packet(), packet->size()) == false) {
		ERR("Failed to send, but delete handle\n");
		errno = ECOMM;
		goto out;
	}

	INFO("Recv a reply packet\n");
	if (g_bind_table[handle].ipc && g_bind_table[handle].ipc->recv(packet->packet(), packet->header_size()) == false) {
		ERR("Send to reply packet fail\n");
		errno = ECOMM;
		goto out;
	}

	if (packet->payload_size()) {
		if (g_bind_table[handle].ipc && g_bind_table[handle].ipc->recv((char*)packet->packet() + packet->header_size(), packet->payload_size()) == false) {
			ERR("Failed to recv packet\n");
			errno = ECOMM;
		}
	}

out:
	release_handle(handle);
	system_off_unset();
	delete packet;
	return 0;
	
}

EXTAPI int sf_start(int handle , int option)
{
	cpacket *packet;
	cmd_start_t *payload;

	int lcd_state = 0;

	retvm_if( handle > MAX_BIND_SLOT - 1, -1 , "Incorrect handle");
	retvm_if( (g_bind_table[handle].ipc == NULL) ||(handle < 0) , -1 , "sensor_start fail , invalid handle value : %d",handle);
	retvm_if( option < 0 , -1 , "sensor_start fail , invalid option value : %d",option);
	retvm_if( g_bind_table[handle].sensor_state == SENSOR_STATE_STARTED , 0 , "sensor already started");

        try {
                packet = new cpacket(sizeof(cmd_start_t)+4);
        } catch (int ErrNo) {
                ERR("packet creation fail , errno : %d , errstr : %s\n",ErrNo, strerror(ErrNo));
                return -1;
        }

	if(option != SENSOR_OPTION_ALWAYS_ON)
	{
		if(vconf_get_int(VCONFKEY_PM_STATE, &lcd_state) == 0)
		{
			if(lcd_state == VCONFKEY_PM_STATE_LCDOFF)
			{
				g_bind_table[handle].sensor_state = SENSOR_STATE_PAUSED;
				DBG("SENSOR_STATE_PAUSED(LCD OFF)");
				delete packet;
				return 0;
			}
		}
		else
		{
			DBG("vconf_get_int Error lcd_state = [%d]",lcd_state);
		}
	}
	else
	{
		DBG("sensor start (SENSOR_OPTION_ALWAYS_ON)");
	}

	INFO("Sensor S/F Started\n");

	payload = (cmd_start_t*)packet->data();
	if (!payload) {
		ERR("cannot find memory for send packet->data");
		errno = ENOMEM;
		delete packet;
		return -2;
	}

	packet->set_version(PROTOCOL_VERSION);
	packet->set_cmd(CMD_START);
	packet->set_payload_size(sizeof(cmd_start_t));
	payload->option = option;

	INFO("Send CMD_START command\n");
	if (g_bind_table[handle].ipc && g_bind_table[handle].ipc->send(packet->packet(), packet->size()) == false) {
		ERR("Faield to send a packet\n");
		release_handle(handle);
		errno = ECOMM;
		delete packet;
		return -2;
	}

	INFO("Recv a reply packet\n");
	
	if (g_bind_table[handle].ipc &&  g_bind_table[handle].ipc->recv(packet->packet(), packet->header_size()) == false) {
		ERR("Send to reply packet fail\n");
		errno = ECOMM;
		delete packet;
		return -2;		
	}

	DBG("packet received\n");
	if (packet->payload_size()) {
		if (g_bind_table[handle].ipc && g_bind_table[handle].ipc->recv((char*)packet->packet() +	packet->header_size(), packet->payload_size()) == false) {
			errno = ECOMM;
			delete packet;
			return -2;
		}

		if (packet->cmd() == CMD_DONE) {
			cmd_done_t *payload;
			payload = (cmd_done_t*)packet->data();
			if (payload->value < 0) {
				ERR("Error from sensor server [-1 or -2 : socket error, -3 : stopped by	sensor plugin]   value = [%d]\n", payload->value);
				errno = ECOMM;
				delete packet;
				return payload->value;
			}
		} else {
			ERR("unexpected server cmd\n");
			errno = ECOMM;
			delete packet;
			return -2;
		}
	}

	g_bind_table[handle].sensor_state = SENSOR_STATE_STARTED;
	g_bind_table[handle].sensor_option = option;

	delete packet;
	return 0;

}

EXTAPI int sf_stop(int handle)
{
	cpacket *packet;
	cmd_stop_t *payload;

	retvm_if( handle > MAX_BIND_SLOT - 1, -1 , "Incorrect handle");
	retvm_if( (g_bind_table[handle].ipc == NULL) ||(handle < 0) , -1 , "sensor_stop fail , invalid handle value : %d",handle);
	retvm_if( (g_bind_table[handle].sensor_state == SENSOR_STATE_STOPPED) || (g_bind_table[handle].sensor_state == SENSOR_STATE_PAUSED) , 0 , "sensor already stopped");

        try {
                packet = new cpacket(sizeof(cmd_stop_t)+4);
        } catch (int ErrNo) {
                ERR("packet creation fail , errno : %d , errstr : %s\n",ErrNo, strerror(ErrNo));
                return -1;
        }

	INFO("Sensor S/F Stopped handle = [%d]\n", handle);

	payload = (cmd_stop_t*)packet->data();
	if (!payload) {
		ERR("cannot find memory for send packet->data");
		errno = ENOMEM;
		delete packet;
		return -2;
	}

	packet->set_version(PROTOCOL_VERSION);
	packet->set_cmd(CMD_STOP);
	packet->set_payload_size(sizeof(cmd_stop_t));

	if (g_bind_table[handle].ipc && g_bind_table[handle].ipc->send(packet->packet(), packet->size()) == false) {
		ERR("Failed to send a packet\n");
		release_handle(handle);
		errno = ECOMM;
		delete packet;
		return -2;
	}

	g_bind_table[handle].sensor_state = SENSOR_STATE_STOPPED;
	delete packet;

	return 0;

}

EXTAPI int sf_register_event(int handle , unsigned int event_type ,  event_condition_t *event_condition , sensor_callback_func_t cb , void *cb_data )
{
	cpacket *packet;
	cmd_reg_t *payload;
	int i = 0, j = 0;
	int avail_cb_slot_idx = -1;

	int collect_data_flag = 0;

	retvm_if( handle > MAX_BIND_SLOT - 1 , -1 , "Incorrect handle");
	retvm_if( (g_bind_table[handle].ipc == NULL) ||(handle < 0) , -1 , "sensor_register_cb fail , invalid handle value : %d",handle);

        try {
                packet = new cpacket(sizeof(cmd_reg_t)+4);
        } catch (int ErrNo) {
                ERR("packet creation fail , errno : %d , errstr : %s\n",ErrNo, strerror(ErrNo));
                return -1;
        }

	payload = (cmd_reg_t*)packet->data();
	if (!payload) {
		ERR("cannot find memory for send packet->data");
		errno = ENOMEM;
		delete packet;
		return -2;
	}

	DBG("Current handle's(%d) cb_event_max_num : %d\n", handle , g_bind_table[handle].cb_event_max_num);

	for ( i=0 ; i<g_bind_table[handle].cb_event_max_num ; i++ ) {
		if ( ((event_type&0xFFFF)>>i) == 0x0001) {
			if (  (g_bind_table[handle].cb_slot_num[i] == -1) ||(!(g_cb_table[ g_bind_table[handle].cb_slot_num[i] ].sensor_callback_func_t))  ) {
				DBG("Find available slot in g_bind_table for cb\n");
				avail_cb_slot_idx = i;
				break;
			} 
		}
	}
	
	if (avail_cb_slot_idx < 0 ) {
		ERR("Find cb_slot fail, There is  already callback function!!\n");
		errno = ENOMEM;
		delete packet;
		return -2;
	}

	i = cb_acquire_handle();
	if (i == MAX_CB_BIND_SLOT) {
		ERR("MAX_BIND_SLOT, Too many slot required");
		errno = ENOMEM;
		delete packet;
		return -2;
	}
	INFO("Empty cb_slot : %d\n", i);

	g_cb_table[i].my_cb_handle = i;
	g_cb_table[i].my_sf_handle = handle;
		
	INFO("Sensor S/F register cb\n");	

	packet->set_version(PROTOCOL_VERSION);
	packet->set_cmd(CMD_REG);
	packet->set_payload_size(sizeof(cmd_reg_t));
	payload->type = REG_ADD;
	payload->event_type = event_type;

	 if(!event_condition)
		 payload->interval = BASE_GATHERING_INTERVAL;
	 else if((event_condition->cond_op == CONDITION_EQUAL) && (event_condition->cond_value1 > 0 ))
		 payload->interval = event_condition->cond_value1;
	 else
		 payload->interval = BASE_GATHERING_INTERVAL;
	

	INFO("Send CMD_REG command with reg_type : %x , event_typ : %x\n",payload->type , payload->event_type );
	if (g_bind_table[handle].ipc && g_bind_table[handle].ipc->send(packet->packet(), packet->size()) == false) {
		ERR("Faield to send a packet\n");
		cb_release_handle(i);
		release_handle(handle);
		errno = ECOMM;
		delete packet;
		return -2;
	}

	if (g_bind_table[handle].ipc && g_bind_table[handle].ipc->recv(packet->packet(), packet->header_size()) == false) {
		ERR("Faield to receive a packet\n");
		cb_release_handle(i);
		release_handle(handle);
		errno = ECOMM;
		delete packet;
		return -2;
	}

	if (packet->payload_size()) {
		if (g_bind_table[handle].ipc && g_bind_table[handle].ipc->recv((char*)packet->packet() + packet->header_size(), packet->payload_size()) == false) {
			ERR("Faield to receive a packet\n");
			cb_release_handle(i);
			release_handle(handle);
			errno = ECOMM;
			delete packet;
			return -2;
		}

		if (packet->cmd() == CMD_DONE) {
			cmd_done_t *payload;
			payload = (cmd_done_t*)packet->data();
			if (payload->value == -1) {
				ERR("server register fail\n");
				cb_release_handle(i);
				errno = ECOMM;
				delete packet;
				return -2;
			} 
		} else {
			ERR("unexpected server cmd\n");
			cb_release_handle(i);
			errno = ECOMM;
			delete packet;
			return -2;
		}
	}

	memset(g_cb_table[i].call_back_key,'\0',MAX_KEY_LEN);
	snprintf(g_cb_table[i].call_back_key,(MAX_KEY_LEN-1),"%s%x",DEFAULT_SENSOR_KEY_PREFIX, event_type);

	g_cb_table[i].cb_event_type = event_type;
	g_cb_table[i].client_data = cb_data;
	g_cb_table[i].sensor_callback_func_t = cb;

	switch (event_type ) {
			case ACCELEROMETER_EVENT_RAW_DATA_REPORT_ON_TIME:
				/* fall through */
			case GEOMAGNETIC_EVENT_RAW_DATA_REPORT_ON_TIME:
				/* fall through */
			case PROXIMITY_EVENT_STATE_REPORT_ON_TIME:
				/* fall through */
			case GYROSCOPE_EVENT_RAW_DATA_REPORT_ON_TIME:
				/* fall through */
			case BAROMETER_EVENT_RAW_DATA_REPORT_ON_TIME:
				/* fall through */
			case LIGHT_EVENT_LEVEL_DATA_REPORT_ON_TIME:
				/* fall through */
			case FUSION_SENSOR_EVENT_RAW_DATA_REPORT_ON_TIME:
				collect_data_flag = 1;
				g_cb_table[i].request_data_id = (event_type & (0xFFFF<<16)) |0x0001;
				break;
			case LIGHT_EVENT_LUX_DATA_REPORT_ON_TIME:
				/* fall through */
			case GEOMAGNETIC_EVENT_ATTITUDE_DATA_REPORT_ON_TIME:
				/* fall through */
			case ACCELEROMETER_EVENT_ORIENTATION_DATA_REPORT_ON_TIME:
				/* fall through */
			case BAROMETER_EVENT_TEMPERATURE_DATA_REPORT_ON_TIME:
				/* fall through */
			case PROXIMITY_EVENT_DISTANCE_DATA_REPORT_ON_TIME:
				/* fall through */
			case FUSION_ROTATION_VECTOR_EVENT_DATA_REPORT_ON_TIME:
				collect_data_flag = 1;
				g_cb_table[i].request_data_id = (event_type & (0xFFFF<<16)) |0x0002;
				break;
			case ACCELEROMETER_EVENT_LINEAR_ACCELERATION_DATA_REPORT_ON_TIME:
				/* fall through */
			case FUSION_ROTATION_MATRIX_EVENT_DATA_REPORT_ON_TIME:
				/* fall through */
			case BAROMETER_EVENT_ALTITUDE_DATA_REPORT_ON_TIME:
				collect_data_flag = 1;
				g_cb_table[i].request_data_id = (event_type & (0xFFFF<<16)) |0x0004;
				break;
			case ACCELEROMETER_EVENT_GRAVITY_DATA_REPORT_ON_TIME:
				collect_data_flag = 1;
				g_cb_table[i].request_data_id = (event_type & (0xFFFF<<16)) |0x0008;
				break;
			default :
				collect_data_flag = 0;
	}

	INFO("key : %s(p:%p), cb_handle value : %d\n", g_cb_table[i].call_back_key ,g_cb_table[i].call_back_key, i );

	if ( collect_data_flag ) {			
		sensor_data_t *collected_data_set;		
		
		collected_data_set = new sensor_data_t [ON_TIME_REQUEST_COUNTER];
		if ( !collected_data_set ) {
			ERR("memory allocation fail for gathering datas\n");
			cb_release_handle(i);
			errno = ECOMM;
			delete packet;
			return -2;
		}
		g_cb_table[i].collected_data = (void *)collected_data_set;
		g_cb_table[i].current_collected_idx = 0;
		
		if (!event_condition) {
			g_cb_table[i].gsource_interval = BASE_GATHERING_INTERVAL;
		} else {
			if ( (event_condition->cond_op == CONDITION_EQUAL) && (event_condition->cond_value1 > 0 ) ) {
				g_cb_table[i].gsource_interval = (guint)event_condition->cond_value1;
			} else {
				ERR("Invaild input_condition interval , input_interval : %f\n", event_condition->cond_value1);
				cb_release_handle(i);						
				errno = EINVAL;
				delete packet;
				return -1;
			}
		}


		if ( g_cb_table[i].gsource_interval != 0 ) {
			g_cb_table[i].source = g_timeout_source_new(g_cb_table[i].gsource_interval);
		} else {
			ERR("Error , gsource_interval value : %u",g_cb_table[i].gsource_interval);
			cb_release_handle(i);
			errno = EINVAL;
			delete packet;
			return -1;
		}
		g_source_set_callback (g_cb_table[i].source, sensor_timeout_handler, (gpointer)&g_cb_table[i].my_cb_handle,NULL);			
		g_cb_table[i].gID = g_source_attach (g_cb_table[i].source, NULL);		
		
	}else {		
		g_cb_table[i].request_count = 0;
		g_cb_table[i].request_data_id = 0;
		g_cb_table[i].collected_data = NULL;
		g_cb_table[i].gsource_interval = BASE_GATHERING_INTERVAL;

		for(j = 0 ; j < MAX_EVENT_LIST ; j++){
			if(g_event_list[j].event_type == event_type) {
				if(g_event_list[j].event_counter < 1){
					if(vconf_notify_key_changed(g_cb_table[i].call_back_key,sensor_changed_cb,(void*)(j)) == 0 ) {
						DBG("vconf_add_chaged_cb success for key : %s  , my_cb_handle value : %d\n", g_cb_table[i].call_back_key, g_cb_table[i].my_cb_handle);
					} else {
						DBG("vconf_add_chaged_cb fail for key : %s  , my_cb_handle value : %d\n", g_cb_table[i].call_back_key, g_cb_table[i].my_cb_handle);
						cb_release_handle(i);
						errno = ENODEV;
						delete packet;
						return -2;
					}
				}else {
					DBG("vconf_add_changed_cb is already registered for key : %s, my_cb_handle	value : %d\n", g_cb_table[i].call_back_key,	g_cb_table[i].my_cb_handle);
				}
				add_cb_number(j, i);
			}
		}
	}


	g_bind_table[handle].cb_slot_num[avail_cb_slot_idx] = i;
	delete packet;

	return 0;
}


EXTAPI int sf_unregister_event(int handle, unsigned int event_type)
{
	int state = 0;
	cpacket *packet;
	cmd_reg_t *payload;
	int find_cb_handle =-1;
	int i = 0, j = 0;

	int collect_data_flag = 0;

	retvm_if( handle > MAX_BIND_SLOT - 1, -1 , "Incorrect handle");
	retvm_if( (g_bind_table[handle].ipc == NULL) ||(handle < 0) , -1 , "sensor_unregister_cb fail , invalid handle value : %d",handle);

        try {
                packet = new cpacket(sizeof(cmd_reg_t)+4);
        } catch (int ErrNo) {
                ERR("packet creation fail , errno : %d , errstr : %s\n",ErrNo, strerror(ErrNo));
                return -1;
        }

	payload = (cmd_reg_t*)packet->data();
	if (!payload) {
		ERR("cannot find memory for send packet->data");
		errno = ENOMEM;
		delete packet;
		return -2;
	}

	for ( i=0 ; i<g_bind_table[handle].cb_event_max_num ; i++ ) {
		if (  g_bind_table[handle].cb_slot_num[i] != -1 ) {	
			
			if ( event_type == g_cb_table[ g_bind_table[handle].cb_slot_num[i] ].cb_event_type) {
				find_cb_handle = g_bind_table[handle].cb_slot_num[i];
				break;
			}			
		}		
	}

	if (find_cb_handle < 0) {
		ERR("Err , Cannot find cb_slot_num!! for event : %x\n", event_type );
		errno = EINVAL;
		delete packet;
		return -1;
	}
	

	INFO("Sensor S/F unregister cb\n");

	packet->set_version(PROTOCOL_VERSION);
	packet->set_cmd(CMD_REG);
	packet->set_payload_size(sizeof(cmd_reg_t));	
	payload->type = REG_DEL; 
	payload->event_type = event_type;
	payload->interval = g_cb_table[find_cb_handle].gsource_interval;

	switch (event_type ) {
		case ACCELEROMETER_EVENT_RAW_DATA_REPORT_ON_TIME:
			/* fall through */
		case ACCELEROMETER_EVENT_ORIENTATION_DATA_REPORT_ON_TIME:
			/* fall through */
		case ACCELEROMETER_EVENT_LINEAR_ACCELERATION_DATA_REPORT_ON_TIME :
			/* fall through */ 
		case GEOMAGNETIC_EVENT_ATTITUDE_DATA_REPORT_ON_TIME:
			/* fall through */ 			
		case PROXIMITY_EVENT_STATE_REPORT_ON_TIME:
			/* fall through */
		case GYROSCOPE_EVENT_RAW_DATA_REPORT_ON_TIME:
			/* fall through */
		case BAROMETER_EVENT_RAW_DATA_REPORT_ON_TIME:
			/* fall through */
		case BAROMETER_EVENT_TEMPERATURE_DATA_REPORT_ON_TIME:
			/* fall through */
		case BAROMETER_EVENT_ALTITUDE_DATA_REPORT_ON_TIME:
			/* fall through */
		case LIGHT_EVENT_LEVEL_DATA_REPORT_ON_TIME:
			/* fall through */
		case GEOMAGNETIC_EVENT_RAW_DATA_REPORT_ON_TIME:
			/* fall through */
		case LIGHT_EVENT_LUX_DATA_REPORT_ON_TIME:
			/* fall through */
		case PROXIMITY_EVENT_DISTANCE_DATA_REPORT_ON_TIME:
			/* fall through */
		case FUSION_SENSOR_EVENT_RAW_DATA_REPORT_ON_TIME:
			/* fall through */
		case FUSION_ROTATION_VECTOR_EVENT_DATA_REPORT_ON_TIME:
			/* fall through */
		case FUSION_ROTATION_MATRIX_EVENT_DATA_REPORT_ON_TIME:
			/* fall through */
		case ACCELEROMETER_EVENT_GRAVITY_DATA_REPORT_ON_TIME:
			collect_data_flag = 1;
			break;
	}

	INFO("Send CMD_REG command\n");
	if (g_bind_table[handle].ipc && g_bind_table[handle].ipc->send(packet->packet(), packet->size()) == false) {
		ERR("Faield to send a packet\n");		
		release_handle(handle);
		errno = ECOMM;
		delete packet;
		return -2;
	}

	if (g_bind_table[handle].ipc && g_bind_table[handle].ipc->recv(packet->packet(), packet->header_size()) == false) {
		ERR("Failed to recv packet_header\n");
		release_handle(handle);
		errno = ECOMM;
		delete packet;
		return -2;
	}

	if (packet->payload_size()) {
		if (g_bind_table[handle].ipc && g_bind_table[handle].ipc->recv((char*)packet->packet() + packet->header_size(), packet->payload_size()) == false) {
			ERR("Failed to recv packet\n");			
			release_handle(handle);
			errno = ECOMM;
			delete packet;
			return -2;
		}
	}	

	if ( collect_data_flag ) {
		if(g_cb_table[find_cb_handle].source)
		{
			g_source_destroy(g_cb_table[find_cb_handle].source);
			g_source_unref(g_cb_table[find_cb_handle].source);
		}
		g_cb_table[find_cb_handle].request_count = 0;
		g_cb_table[find_cb_handle].request_data_id = 0;
		g_cb_table[find_cb_handle].gsource_interval = 0;
	} else {
		for(j = 0 ; j < MAX_EVENT_LIST ; j++){
			if(g_event_list[j].event_type == event_type){
				if(g_event_list[j].event_counter <= 1){
					state = vconf_ignore_key_changed(g_cb_table[find_cb_handle].call_back_key, sensor_changed_cb);
					if ( state < 0 ) {
						ERR("Failed to del callback using by vconf_del_changed_cb for key : %s\n",g_cb_table[find_cb_handle].call_back_key);
						errno = ENODEV;
						state = -2;
					}
					else {
						DBG("del callback using by vconf success [%s] handle = [%d]",g_cb_table[find_cb_handle].call_back_key, find_cb_handle);
					}
				} else {
					DBG("fake remove");
				}
				del_cb_number(j,find_cb_handle);
			}
		}
	}

	cb_release_handle(find_cb_handle);
	g_bind_table[handle].cb_slot_num[i] = -1;
	delete packet;

	return state;
}


EXTAPI int sf_get_data(int handle , unsigned int data_id ,  sensor_data_t* values)
{
	cpacket *packet;
	cmd_get_data_t *payload;
	cmd_get_struct_t *return_payload;
	int i;
	struct timeval sv;	

	
	retvm_if( (!values) , -1 , "sf_get_data fail , invalid get_values pointer %p", values);
	retvm_if( ( (data_id & 0xFFFF) < 1) || ( (data_id & 0xFFFF) > 0xFFF), -1 , "sf_get_data fail , invalid data_id %d", data_id);
	retvm_if( handle > MAX_BIND_SLOT - 1 , -1 , "Incorrect handle");
	retvm_if( (g_bind_table[handle].ipc == NULL) ||(handle < 0) , -1 , "sf_get_data fail , invalid handle value : %d",handle);
 
        try {
                packet = new cpacket(sizeof(cmd_get_struct_t)+sizeof(base_data_struct)+4);
        } catch (int ErrNo) {
                ERR("packet creation fail , errno : %d , errstr : %s\n",ErrNo, strerror(ErrNo));
                return -1;
        }

	if(g_bind_table[handle].sensor_state != SENSOR_STATE_STARTED)
	{
		ERR("sensor framewoker doesn't started");
		values->data_accuracy = SENSOR_ACCURACY_UNDEFINED;
		values->data_unit_idx = SENSOR_UNDEFINED_UNIT;
		values->time_stamp = 0;
		values->values_num = 0;
		errno = ECOMM;
		delete packet;
		return -2;
	}

	payload = (cmd_get_data_t*)packet->data();
	if (!payload) {
		ERR("cannot find memory for send packet->data");
		errno = ENOMEM;
		delete packet;
		return -2;
	}

	packet->set_version(PROTOCOL_VERSION);
	packet->set_cmd(CMD_GET_STRUCT);
	packet->set_payload_size(sizeof(cmd_get_data_t));
	payload->data_id = data_id;

	if (g_bind_table[handle].ipc && g_bind_table[handle].ipc->send(packet->packet(), packet->size()) == false) {		
		release_handle(handle);
		errno = ECOMM;
		delete packet;
		return -2;
	}

	if (g_bind_table[handle].ipc && g_bind_table[handle].ipc->recv(packet->packet(), packet->header_size()) == false) {
		release_handle(handle);
		errno = ECOMM;
		delete packet;
		return -2;
	}

	if (packet->payload_size()) {
		if (g_bind_table[handle].ipc && g_bind_table[handle].ipc->recv((char*)packet->packet() + packet->header_size(), packet->payload_size()) == false) {
			release_handle(handle);
			errno = ECOMM;
			delete packet;
			return -2;
		}
	}

	return_payload = (cmd_get_struct_t*)packet->data();
	if (!return_payload) {
		ERR("cannot find memory for return packet->data");
		errno = ENOMEM;
		delete packet;
		return -2;
	}

	if ( return_payload->state < 0 ) {
		ERR("get values fail from server \n");
		values->data_accuracy = SENSOR_ACCURACY_UNDEFINED;
		values->data_unit_idx = SENSOR_UNDEFINED_UNIT;
		values->time_stamp = 0;
		values->values_num = 0;
		errno = ECOMM;
		delete packet;
		return -2;
	}

	base_data_struct *base_return_data;
	base_return_data = (base_data_struct *)return_payload->data_struct;

	gettimeofday(&sv, NULL);
	values->time_stamp = MICROSECONDS(sv);

	values->data_accuracy = base_return_data->data_accuracy;
	values->data_unit_idx = base_return_data->data_unit_idx;
	values->values_num = base_return_data->values_num;
	for ( i = 0 ; i <  base_return_data->values_num ; i++ ) {
		values->values[i] = base_return_data->values[i];
//		DBG("client , get_data_value , [%d] : %f \n", i , values->values[i]);
	}
	
	delete packet;
	return 0;
	
}

EXTAPI int sf_check_rotation( unsigned long *curr_state)
{
	int state = -1;

	double raw_z;
	double atan_value = 0, norm_z = 0;
	int acc_theta = 0 , acc_pitch = 0;
	int handle = 0;
	int lcd_type = 0;

	sensor_data_t base_data_values;

	retvm_if( curr_state==NULL , -1 , "sf_check_rotation fail , invalid curr_state");

	*curr_state = ROTATION_UNKNOWN;

	INFO("Sensor_attach_channel from pid : %d",getpid());

	handle = sf_connect( ACCELEROMETER_SENSOR);
	if (handle<0)
	{
		ERR("sensor attach fail\n");
		return -1;
	}

	state = sf_start(handle, 0);
	if(state < 0)
	{
		ERR("sf_start fail\n");
		return -1;
	}

	state = sf_get_data(handle, ACCELEROMETER_BASE_DATA_SET, &base_data_values);
	if(state < 0)
	{
		ERR("sf_get_data fail\n");
		return -1;
	}

	state = sf_stop(handle);
	if(state < 0)
	{
		ERR("sf_stop fail\n");
		return -1;
	}

	state = sf_disconnect(handle);
	if(state < 0)
	{
		ERR("sf_disconnect fail\n");
		return -1;
	}

	if((base_data_values.values[0] > XY_POSITIVE_THD || base_data_values.values[0] < XY_NEGATIVE_THD) || (base_data_values.values[1] > XY_POSITIVE_THD || base_data_values.values[1] < XY_NEGATIVE_THD))
	{
		atan_value = atan2((base_data_values.values[0]),(base_data_values.values[1]));
		acc_theta = ((int)(atan_value * (RADIAN_VALUE) + 360))%360;

		raw_z = (double)(base_data_values.values[2]) / 9.8;

		if ( raw_z > 1.0 ) {
			norm_z = 1.0;
		}
		else if ( raw_z < -1.0 ) {
			norm_z = -1.0;
		}
		else {
			norm_z = raw_z;
		}
		acc_pitch = (int)( acos(norm_z) *(RADIAN_VALUE));
	}

	INFO( "cal value [acc_theta] : %d , [acc_pitch] : %d\n",acc_theta ,acc_pitch);

	if( (acc_pitch>PITCH_MIN) && (acc_pitch<PITCH_MAX) ) {
		if ((acc_theta >= ROTATION_360 - ROTATION_THD && acc_theta <= ROTATION_360 ) ||	(acc_theta >= ROTATION_0 && acc_theta < ROTATION_0 + ROTATION_THD))
		{
			*curr_state = rotation_mode[ROTATION_EVENT_0].rm[lcd_type];	
		}
		else if(acc_theta >= ROTATION_90 - ROTATION_THD && acc_theta < ROTATION_90 + ROTATION_THD)
		{
			*curr_state = rotation_mode[ROTATION_EVENT_90].rm[lcd_type];
		}
		else if(acc_theta >= ROTATION_180 - ROTATION_THD && acc_theta < ROTATION_180 + ROTATION_THD)
		{
			*curr_state = rotation_mode[ROTATION_EVENT_180].rm[lcd_type];
		}
		else if(acc_theta >= ROTATION_270 - ROTATION_THD && acc_theta < ROTATION_270 + ROTATION_THD)
		{
			*curr_state = rotation_mode[ROTATION_EVENT_270].rm[lcd_type];	
		} 
		else {
			ERR("Wrong acc_theta : %d , cannot detect current state of ACCEL_SENSOR",acc_theta);
			*curr_state = rotation_mode[ROTATION_UNKNOWN].rm[lcd_type];
		}		
	}else {
		INFO("Cannot detect current state of ACCEL_SENSOR");
		*curr_state = rotation_mode[ROTATION_UNKNOWN].rm[lcd_type];
	}			

	return 0;

}


EXTAPI int sf_set_wakeup(sensor_type_t sensor_type)
{
	int i = 0;

	if(sf_is_wakeup_supported(sensor_type) < 0)
	{
		ERR("Cannot support wake up");
		return -1;
	}

	for(i = 0 ; i < MAX_BIND_SLOT ; i++)
	{
		if(g_bind_table[i].sensor_type == sensor_type)
		{
			g_bind_table[i].wakeup_state = SENSOR_WAKEUP_SETTED;
		}
	}

	vconf_notify_key_changed(VCONFKEY_PM_STATE, lcd_off_set_wake_up, NULL);

	return 0;
}

EXTAPI int sf_unset_wakeup(sensor_type_t sensor_type)
{
	int i = 0;
	int state = -1;

	if(sf_is_wakeup_supported(sensor_type) < 0)
	{
		ERR("Cannot support wake up");
		return -1;
	}
	
	if(sensor_type == ACCELEROMETER_SENSOR)
	{
		state = sf_set_property(sensor_type, ACCELEROMETER_PROPERTY_SET_WAKEUP, WAKEUP_UNSET);
		if(state != 0)
		{
			ERR("set wakeup fail");
			return -1;
		}
	}
	else
	{
		ERR("Cannot support wakeup");
		return -1;
	}
	
	for(i = 0 ; i < MAX_BIND_SLOT ; i++)
	{
		if(g_bind_table[i].sensor_type == sensor_type)
		{
			g_bind_table[i].wakeup_state = SENSOR_WAKEUP_UNSETTED;
		}
	}

	vconf_ignore_key_changed(VCONFKEY_PM_STATE, lcd_off_set_wake_up);

	return state;
}

EXTAPI int sf_is_wakeup_supported(sensor_type_t sensor_type)
{
	if(sensor_type == ACCELEROMETER_SENSOR)
		return sf_set_property(sensor_type, ACCELEROMETER_PROPERTY_CHECK_WAKEUP_SUPPORTED,	WAKEUP_SET);
	else
	{
		ERR("Cannot support wakeup");
		return -1;
	}
}

EXTAPI int sf_is_wakeup_enabled(sensor_type_t sensor_type)
{
	if(sensor_type == ACCELEROMETER_SENSOR)
		return sf_set_property(sensor_type,ACCELEROMETER_PROPERTY_CHECK_WAKEUP_STATUS,0);
	else
	{
		 ERR("Cannot support wakeup");
		 return -1;
	}
}

EXTAPI int sf_change_event_condition(int handle, unsigned int event_type, event_condition_t *event_condition)
{
	cpacket *packet;
	cmd_reg_t *payload;
	int sensor_state = SENSOR_STATE_UNKNOWN;

	int i = 0;

        retvm_if( handle > MAX_BIND_SLOT - 1 , -1 , "Incorrect handle");
        retvm_if( (g_bind_table[handle].ipc == NULL) ||(handle < 0) , -1 , "sf_change_event_condition fail , invalid handle value : %d",handle);

        try {
                packet = new cpacket(sizeof(cmd_reg_t) + 4);
        } catch (int ErrNo) {
                ERR("packet creation fail , errno : %d , errstr : %s\n",ErrNo, strerror(ErrNo));
                return -1;
        }

	switch (event_type ) {
		case ACCELEROMETER_EVENT_RAW_DATA_REPORT_ON_TIME:
			/* fall through */
		case ACCELEROMETER_EVENT_ORIENTATION_DATA_REPORT_ON_TIME:
			/* fall through */
		case ACCELEROMETER_EVENT_LINEAR_ACCELERATION_DATA_REPORT_ON_TIME :
			/* fall through */
		case GEOMAGNETIC_EVENT_ATTITUDE_DATA_REPORT_ON_TIME:
			/* fall through */
		case PROXIMITY_EVENT_STATE_REPORT_ON_TIME:
			/* fall through */
		case GYROSCOPE_EVENT_RAW_DATA_REPORT_ON_TIME:
			/* fall through */
		case BAROMETER_EVENT_RAW_DATA_REPORT_ON_TIME:
			/* fall through */
		case BAROMETER_EVENT_TEMPERATURE_DATA_REPORT_ON_TIME:
			/* fall through */
		case BAROMETER_EVENT_ALTITUDE_DATA_REPORT_ON_TIME:
			/* fall through */
		case LIGHT_EVENT_LEVEL_DATA_REPORT_ON_TIME:
			/* fall through */
		case GEOMAGNETIC_EVENT_RAW_DATA_REPORT_ON_TIME:
			/* fall through */
		case LIGHT_EVENT_LUX_DATA_REPORT_ON_TIME:
			/* fall through */
		case PROXIMITY_EVENT_DISTANCE_DATA_REPORT_ON_TIME:
			/* fall through */
		case FUSION_SENSOR_EVENT_RAW_DATA_REPORT_ON_TIME:
			/* fall through */
		case FUSION_ROTATION_VECTOR_EVENT_DATA_REPORT_ON_TIME:
			/* fall through */
		case FUSION_ROTATION_MATRIX_EVENT_DATA_REPORT_ON_TIME:
			break;
		default :
			ERR("Cannot support this API");
			delete packet;
			return -1;
	}

	for(i = 0 ; i < MAX_CB_SLOT_PER_BIND ; i++)
	{
		if(g_cb_table[g_bind_table[handle].cb_slot_num[i]].cb_event_type == event_type)
		{
			if(!event_condition)
			{
				if(g_cb_table[g_bind_table[handle].cb_slot_num[i]].gsource_interval == (guint)BASE_GATHERING_INTERVAL)
				{
					ERR("same interval");
					delete packet;
					return -1;
				}
			}
			else
			{
				if(g_cb_table[g_bind_table[handle].cb_slot_num[i]].gsource_interval == (guint)event_condition->cond_value1)
				{
					ERR("same interval");
					delete packet;
					return -1;
				}
			}

			DBG("find callback number [%d]",i);
			break;
		}
	}

	if(i == MAX_CB_SLOT_PER_BIND)
	{
		ERR("cannot find event_type [%x] in handle [%d]", event_type, handle);
		delete packet;
		return -1;
	}

	sensor_state = g_bind_table[handle].sensor_state;
	g_bind_table[handle].sensor_state = SENSOR_STATE_STOPPED;

	payload = (cmd_reg_t*)packet->data();
	if(!payload) {
		ERR("cannot find memory for send packet->data");
		errno = ENOMEM;
		g_bind_table[handle].sensor_state = SENSOR_STATE_STARTED;
		delete packet;
		return -2;
	}

	packet->set_version(PROTOCOL_VERSION);
	packet->set_cmd(CMD_REG);
	packet->set_payload_size(sizeof(cmd_reg_t));
	payload->type = REG_ADD;
	payload->event_type = event_type;

	if(!event_condition)
		payload->interval = BASE_GATHERING_INTERVAL;
	else if((event_condition->cond_op == CONDITION_EQUAL) && (event_condition->cond_value1 > 0 ))
		payload->interval = event_condition->cond_value1;
	else
		payload->interval = BASE_GATHERING_INTERVAL;


	INFO("Send CMD_REG command with reg_type : %x , event_type : %x\n",payload->type , payload->event_type );
	if (g_bind_table[handle].ipc && g_bind_table[handle].ipc->send(packet->packet(), packet->size()) == false) {
		ERR("Faield to send a packet\n");
		errno = ECOMM;
		g_bind_table[handle].sensor_state = sensor_state;
		delete packet;
		return -2;
	}

	if (g_bind_table[handle].ipc && g_bind_table[handle].ipc->recv(packet->packet(), packet->header_size()) == false) {
		ERR("Faield to receive a packet\n");
		errno = ECOMM;
		g_bind_table[handle].sensor_state = sensor_state;
		delete packet;
		return -2;
	}

	if (packet->payload_size()) {
		if (g_bind_table[handle].ipc && g_bind_table[handle].ipc->recv((char*)packet->packet() + packet->header_size(), packet->payload_size()) == false) {
			ERR("Faield to receive a packet\n");
			errno = ECOMM;
			g_bind_table[handle].sensor_state = sensor_state;
			delete packet;
			return -2;
		}

		if (packet->cmd() == CMD_DONE) {
			cmd_done_t *payload;
			payload = (cmd_done_t*)packet->data();
			if (payload->value == -1) {
				ERR("server register fail\n");
				errno = ECOMM;
				g_bind_table[handle].sensor_state = sensor_state;
				delete packet;
				return -2;
			}
		} else {
			ERR("unexpected server cmd\n");
			errno = ECOMM;
			g_bind_table[handle].sensor_state = sensor_state;
			delete packet;
			return -2;
		}
	}

	if(g_cb_table[i].source != NULL)
	{
		g_source_destroy(g_cb_table[i].source);
		g_source_unref(g_cb_table[i].source);
	}

	g_cb_table[i].gsource_interval = (guint)payload->interval;
	g_cb_table[i].source = g_timeout_source_new(g_cb_table[i].gsource_interval);
	g_source_set_callback (g_cb_table[i].source, sensor_timeout_handler, (gpointer)&g_cb_table[i].my_cb_handle,NULL);
	g_cb_table[i].gID = g_source_attach (g_cb_table[i].source, NULL);

	g_bind_table[handle].sensor_state = sensor_state;
	delete packet;
	
	return 0;
}

int sf_change_sensor_option(int handle, int option)
{
        retvm_if( handle > MAX_BIND_SLOT - 1 , -1 , "Incorrect handle");
        retvm_if( (g_bind_table[handle].ipc == NULL) ||(handle < 0) , -1 , "sensor_start fail , invalid handle value : %d",handle);
        retvm_if( option < 0 || option > 1, -1 , "sensor_start fail , invalid option value : %d",option);	

	g_bind_table[handle].sensor_option = option;

	if(g_bind_table[handle].sensor_state == SENSOR_STATE_PAUSED)
	{
		if( sf_start(handle, option) < 0)
		{
			ERR("sensor start fail");
			return -1;
		}
	}

	return 0;
}
//! End of a file
