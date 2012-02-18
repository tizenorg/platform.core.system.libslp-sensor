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



const char *STR_SF_CLIENT_IPC_SOCKET	= "/tmp/sf_socket";
const char *LCD_TYPE_KEY = "memory/sensor/lcd_type";

static cmutex _lock;

enum _sensor_current_state {
	SENSOR_STATE_UNKNOWN = -1,
	SENSOR_STATE_STOPPED = 0,
	SENSOR_STATE_STARTED = 1,	
};

struct sf_bind_table_t {
	csock *ipc;	
	sensor_type_t sensor_type;	
	int cb_event_max_num;					/*limit by MAX_BIND_PER_CB_SLOT*/
	int cb_slot_num[MAX_CB_SLOT_PER_BIND];
	int my_handle;
	int sensor_state;
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
	{
		ROTATION_UNKNOWN,
		{
			ROTATION_UNKNOWN,
			ROTATION_UNKNOWN
		},
	},
	{
		ROTATION_EVENT_90,
		{
			ROTATION_LANDSCAPE_LEFT,
			ROTATION_PORTRAIT_BTM
		},
	},
	{
		ROTATION_EVENT_0,
		{
			ROTATION_PORTRAIT_TOP,
			ROTATION_LANDSCAPE_LEFT
		},
	},
	{
		ROTATION_EVENT_180,
		{
			ROTATION_PORTRAIT_BTM,
			ROTATION_LANDSCAPE_RIGHT
		},
	},
	{
		ROTATION_EVENT_270,
		{
			ROTATION_LANDSCAPE_RIGHT,
			ROTATION_PORTRAIT_TOP
		},
	},
};


static sf_bind_table_t g_bind_table[MAX_BIND_SLOT];

static cb_bind_table_t g_cb_table[MAX_CB_BIND_SLOT];

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

	for (j=0; j<g_bind_table[i].cb_event_max_num; j++) {
		if (   (j<MAX_CB_SLOT_PER_BIND) && (g_bind_table[i].cb_slot_num[j] > -1)  ) {			
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
		free (g_cb_table[i].collected_data);
	
	g_cb_table[i].collected_data = NULL;
	g_cb_table[i].current_collected_idx = 0;

	g_cb_table[i].source = NULL;
	g_cb_table[i].gsource_interval = 0;
	g_cb_table[i].gID = 0;

	_lock.unlock();
}


static void sensor_changed_cb(keynode_t *node, void *data)
{
	unsigned int event_type = (unsigned int)(data);
	int i = 0;
	int val;
	sensor_event_data_t cb_data;
	sensor_panning_data_t panning_data;

	if(!node)
	{
		ERR("Node is NULL");
		return;
	}

	if (vconf_keynode_get_type(node) !=  VCONF_TYPE_INT ) {
		ERR("Err invaild key_type , incomming key_type : %d , key_name : %s , key_value : %d", vconf_keynode_get_type(node), vconf_keynode_get_name(node),vconf_keynode_get_int(node));
		return;
	}

	val = vconf_keynode_get_int(node);

	for( i = 0 ; i < MAX_CB_BIND_SLOT ; i++)
	{
		if(g_cb_table[i].cb_event_type == event_type)
		{
			if (g_cb_table[i].sensor_callback_func_t)
			{
				if(g_cb_table[i].cb_event_type == MOTION_ENGINE_EVENT_PANNING)
				{
					if(val != 0)
					{
						panning_data.x = (short)(val >> 16);
						panning_data.y = (short)(val & 0x0000FFFF);
						cb_data.event_data_size = sizeof(sensor_panning_data_t);
						cb_data.event_data = (void *)&panning_data;
						g_cb_table[i].sensor_callback_func_t(g_cb_table[i].cb_event_type, &cb_data, g_cb_table[i].client_data);
					}
				} else {
					if ( val<0 ) 
					{
						ERR("vconf_keynode_get_int fail for key : %s , handle_num : %d , get_value : %d\n",	g_cb_table[i].call_back_key,i , val);
						return ;
					}

					switch (g_cb_table[i].cb_event_type) {
						case ACCELEROMETER_EVENT_ROTATION_CHECK :
							/* fall through */
						case ACCELEROMETER_EVENT_SET_HORIZON :
							/* fall through */
						case ACCELEROMETER_EVENT_SET_WAKEUP :
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
							cb_data.event_data_size = sizeof(val);
							cb_data.event_data = (void *)&val;
							break;
						default :
							ERR("Undefined cb_event_type");
							return ;
							break;
					}
					g_cb_table[i].sensor_callback_func_t(g_cb_table[i].cb_event_type ,	&cb_data , g_cb_table[i].client_data);
				}
			}
			else {
				 ERR("Empty Callback func in event : %d\n", event_type);
			}
		}
	}
}


static gboolean sensor_timeout_handler(gpointer data)
{
	int *cb_handle = (int*)(data);
	int state;
	sensor_event_data_t cb_data;

	if ( g_bind_table[g_cb_table[*cb_handle].my_sf_handle].sensor_state != SENSOR_STATE_STARTED ) {
		ERR("Check sensor_state, current sensor state : %d",g_bind_table[g_cb_table[*cb_handle].my_sf_handle].sensor_state);
		return TRUE;
	}

	DBG("sensor_timeout_handler started , cb_handle value : %d\n", *cb_handle);

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
	cpacket packet(sizeof(cmd_return_property_t) + sizeof(base_property_struct)+ 4);
	cmd_get_property_t *cmd_payload;
	INFO("server_get_properties called with handle : %d \n", handle);
	if (handle < 0) {
		ERR("Invalid handle\n");
		errno = EINVAL;
		return -1;
	}

	cmd_payload = (cmd_get_property_t*)packet.data();
	if (!cmd_payload) {
		ERR("cannot find memory for send packet.data");
		errno = ENOMEM;
		return -2;
	}	

	packet.set_version(PROTOCOL_VERSION);
	packet.set_cmd(CMD_GET_PROPERTY);
	packet.set_payload_size(sizeof(cmd_get_property_t));

	if(data_id)
		cmd_payload->get_level = data_id;
	else
		cmd_payload->get_level = ((unsigned int)g_bind_table[handle].sensor_type<<16) | 0x0001;



	INFO("Send CMD_GET_PROPERTY command\n");
	if (g_bind_table[handle].ipc && g_bind_table[handle].ipc->send(packet.packet(), packet.size()) == false) {
		ERR("Faield to send a packet\n");		
		release_handle(handle);
		errno = ECOMM;
		return -2;
	}

	if (g_bind_table[handle].ipc && g_bind_table[handle].ipc->recv(packet.packet(), packet.header_size()) == false) {
		ERR("Faield to receive a packet\n");
		release_handle(handle);
		errno = ECOMM;
		return -2;
	}

	if (packet.payload_size()) {
		if (g_bind_table[handle].ipc && g_bind_table[handle].ipc->recv((char*)packet.packet() + packet.header_size(), packet.payload_size()) == false) {
			ERR("Faield to receive a packet\n");
			release_handle(handle);
			errno = ECOMM;
			return -2;
		}

		if (packet.cmd() == CMD_GET_PROPERTY) {
			cmd_return_property_t *return_payload;
			return_payload = (cmd_return_property_t*)packet.data();
			if (return_payload->state < 0) {
				ERR("sever get property fail , return state : %d\n",return_payload->state);
				errno = EBADE;
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
			return -2;
		}
	}

	return 0;
}


static int server_set_property(int handle , unsigned int property_id, long value )
{
	cpacket packet(sizeof(cmd_set_value_t) + 4);
	cmd_set_value_t *cmd_payload;
	INFO("server_set_property called with handle : %d \n", handle);
	if (handle < 0) {
		ERR("Invalid handle\n");
		errno = EINVAL;
		return -1;
	}

	cmd_payload = (cmd_set_value_t*)packet.data();
	if (!cmd_payload) {
		ERR("cannot find memory for send packet.data");
		errno = ENOMEM;
		return -2;
	}	

	packet.set_version(PROTOCOL_VERSION);
	packet.set_cmd(CMD_SET_VALUE);
	packet.set_payload_size(sizeof(cmd_set_value_t));

	cmd_payload->sensor_type = g_bind_table[handle].sensor_type;
	cmd_payload->property = property_id;
	cmd_payload->value = value;


	INFO("Send CMD_SET_VALUE command\n");
	if (g_bind_table[handle].ipc && g_bind_table[handle].ipc->send(packet.packet(), packet.size()) == false) {
		ERR("Faield to send a packet\n");		
		release_handle(handle);
		errno = ECOMM;
		return -2;
	}

	if (g_bind_table[handle].ipc && g_bind_table[handle].ipc->recv(packet.packet(), packet.header_size()) == false) {
		ERR("Faield to receive a packet\n");
		release_handle(handle);
		errno = ECOMM;
		return -2;
	}

	if (packet.payload_size()) {
		if (g_bind_table[handle].ipc && g_bind_table[handle].ipc->recv((char*)packet.packet() + packet.header_size(), packet.payload_size()) == false) {
			ERR("Faield to receive a packet\n");
			release_handle(handle);
			errno = ECOMM;
			return -2;
		}

		if (packet.cmd() == CMD_DONE) {
			cmd_done_t *payload;
			payload = (cmd_done_t*)packet.data();
			if (payload->value == -1) {
				ERR("cannot support input property\n");
				errno = ENODEV;
				return -1;
			} 
		} else {
			ERR("unexpected server cmd\n");
			errno = ECOMM;
			return -2;
		}
	}

	return 0;
}

///////////////////////////////////for external ///////////////////////////////////

EXTAPI int sf_is_sensor_event_available ( sensor_type_t desired_sensor_type , unsigned int desired_event_type )
{
	int handle;
	cpacket packet(sizeof(cmd_reg_t)+4);
	cmd_reg_t *payload;
	
	handle = sf_connect(desired_sensor_type);
	if ( handle < 0 ) {
		errno = ENODEV;
		return -2;		
	} 

	if ( desired_event_type != 0 ) {

		payload = (cmd_reg_t*)packet.data();
		if (!payload) {
			ERR("cannot find memory for send packet.data");
			errno = ENOMEM;
			return -2;
		}

		packet.set_version(PROTOCOL_VERSION);
		packet.set_cmd(CMD_REG);
		packet.set_payload_size(sizeof(cmd_reg_t));
		payload->type = REG_CHK;
		payload->event_type = desired_event_type;

		INFO("Send CMD_REG command\n");
		if (g_bind_table[handle].ipc && g_bind_table[handle].ipc->send(packet.packet(), packet.size()) == false) {
			ERR("Faield to send a packet\n");		
			release_handle(handle);
			errno = ECOMM;
			return -2;
		}

		if (g_bind_table[handle].ipc && g_bind_table[handle].ipc->recv(packet.packet(), packet.header_size()) == false) {
			ERR("Faield to receive a packet\n");
			release_handle(handle);
			errno = ECOMM;
			return -2;
		}

		if (packet.payload_size()) {
			if (g_bind_table[handle].ipc && g_bind_table[handle].ipc->recv((char*)packet.packet() + packet.header_size(), packet.payload_size()) == false) {
				ERR("Faield to receive a packet\n");
				release_handle(handle);
				errno = ECOMM;
				return -2;
			}

			if (packet.cmd() == CMD_DONE) {
				cmd_done_t *payload;
				payload = (cmd_done_t*)packet.data();
				if (payload->value == -1) {
					ERR("sever check fail\n");
					errno = ENODEV;
					return -2;
				} 
			} else {
				ERR("unexpected server cmd\n");
				errno = ECOMM;
				return -2;
			}
		}
		
	} 
	
	sf_disconnect(handle);

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
	cpacket packet(sizeof(cmd_hello_t)+MAX_CHANNEL_NAME_LEN+4);	//need to check real payload size !!!
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
		return -2;
	}
	
	INFO("Empty slot : %d\n", i);

	switch (sensor_type) {
		case ACCELEROMETER_SENSOR :
			sf_channel_name = (char *)ACCEL_SENSOR_BASE_CHANNEL_NAME;						
			g_bind_table[i].cb_event_max_num = 6;
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
			g_bind_table[i].cb_event_max_num = 7;
			break;

		case GYROSCOPE_SENSOR:
			sf_channel_name = (char *)GYRO_SENSOR_BASE_CHANNEL_NAME;
			g_bind_table[i].cb_event_max_num = 1;
			break;
			
		case THERMOMETER_SENSOR:		
		case PRESSURE_SENSOR:		
		case UNKNOWN_SENSOR:
		default :
			ERR("Undefined sensor_type");
			release_handle(i);
			errno = ENODEV;
			return -2;			
			break;
	}

	g_bind_table[i].sensor_type = sensor_type ;
	g_bind_table[i].my_handle = i;
	g_bind_table[i].sensor_state = SENSOR_STATE_STOPPED;

	for(j = 0 ; j < g_bind_table[i].cb_event_max_num  ; j++)
		g_bind_table[i].cb_slot_num[j] = -1;

	try {
		g_bind_table[i].ipc = new csock( (char *)STR_SF_CLIENT_IPC_SOCKET, csock::SOCK_TCP|csock::SOCK_IPC|csock::SOCK_WORKER, 0, 0);
	} catch (...) {
		release_handle(i);
		errno = ECOMM;
		return -2;
	}

	if (g_bind_table[i].ipc && g_bind_table[i].ipc->connect_to_server() == false) {
		delete g_bind_table[i].ipc;
		g_bind_table[i].ipc = NULL;
		release_handle(i);
		errno = ECOMM;
		return -2;
	}


	INFO("Connected to server\n");
	payload = (cmd_hello_t*)packet.data();
	if (!payload) {
		ERR("cannot find memory for send packet.data");
		release_handle(i);
		errno = ENOMEM;
		return -2;
	}

	if (!sf_channel_name ) {
		ERR("cannot find matched-sensor name!!!");
		release_handle(i);
		errno = ENODEV;
		return -2;
	} else {
		channel_name_length = strlen(sf_channel_name);
		if ( channel_name_length > MAX_CHANNEL_NAME_LEN  ) {
			ERR("error, channel_name_length too long !!!");
			release_handle(i);
			errno = EINVAL;
			return -1;
		}
	}
	
	packet.set_version(PROTOCOL_VERSION);
	packet.set_cmd(CMD_HELLO);
	packet.set_payload_size(sizeof(cmd_hello_t) + channel_name_length);
	strcpy(payload->name, sf_channel_name);

	if (g_bind_table[i].ipc && g_bind_table[i].ipc->send(packet.packet(), packet.size()) == false) {
		ERR("Failed to send a hello packet\n");
		release_handle(i);
		errno = ECOMM;
		return -2;
	}

	INFO("Wait for recv a reply packet\n");
	if (g_bind_table[i].ipc && g_bind_table[i].ipc->recv(packet.packet(), packet.header_size()) == false) {
		release_handle(i);
		errno = ECOMM;
		return -2;
	}

	if (packet.payload_size()) {
		if (g_bind_table[i].ipc && g_bind_table[i].ipc->recv((char*)packet.packet() + packet.header_size(), packet.payload_size()) == false) {
			release_handle(i);
			errno = ECOMM;
			return -2;
		}
	}

	return_payload = (cmd_done_t*)packet.data();
	if (!return_payload) {
		ERR("cannot find memory for return packet.data");
		release_handle(i);
		return -1;
	}

	if ( return_payload->value < 0) {
		ERR("There is no sensor \n");
		release_handle(i);
		return -1;
	}

	INFO("Connected sensor type : %x , handle : %d \n", sensor_type , i);	
	return i;	
}

EXTAPI int sf_disconnect(int handle)
{
	cpacket packet(sizeof(cmd_byebye_t)+4);
	cmd_byebye_t *payload;
		
	retvm_if( (g_bind_table[handle].ipc == NULL) ||(handle < 0) , -1 , "sensor_detach_channel fail , invalid handle value : %d",handle);

	INFO("Detach, so remove %d from the table\n", handle);

	payload = (cmd_byebye_t*)packet.data();
	if (!payload) {
		ERR("cannot find memory for send packet.data");
		errno = ENOMEM;
		return -2;
	}

	packet.set_version(PROTOCOL_VERSION);
	packet.set_cmd(CMD_BYEBYE);
	packet.set_payload_size(sizeof(cmd_byebye_t));

	if (g_bind_table[handle].ipc && g_bind_table[handle].ipc->send(packet.packet(), packet.size()) == false) {
		ERR("Failed to send, but delete handle\n");
		errno = ECOMM;
		goto out;
	}

	INFO("Recv a reply packet\n");
	if (g_bind_table[handle].ipc && g_bind_table[handle].ipc->recv(packet.packet(), packet.header_size()) == false) {
		ERR("Send to reply packet fail\n");
		errno = ECOMM;
		goto out;
	}

	if (packet.payload_size()) {
		if (g_bind_table[handle].ipc && g_bind_table[handle].ipc->recv((char*)packet.packet() + packet.header_size(), packet.payload_size()) == false) {
			ERR("Failed to recv packet\n");
			errno = ECOMM;
		}
	}

out:
	release_handle(handle);
	return 0;
	
}

EXTAPI int sf_start(int handle , int option)
{
	cpacket packet(sizeof(cmd_start_t)+4);
	cmd_start_t *payload;

	retvm_if( (g_bind_table[handle].ipc == NULL) ||(handle < 0) , -1 , "sensor_start fail , invalid handle value : %d",handle);
	retvm_if( option != 0 , -1 , "sensor_start fail , invalid option value : %d",option);


	INFO("Sensor S/F Started\n");

	payload = (cmd_start_t*)packet.data();
	if (!payload) {
		ERR("cannot find memory for send packet.data");
		errno = ENOMEM;
		return -2;
	}

	packet.set_version(PROTOCOL_VERSION);
	packet.set_cmd(CMD_START);
	packet.set_payload_size(sizeof(cmd_start_t));
	payload->option = option;

	INFO("Send CMD_START command\n");
	if (g_bind_table[handle].ipc && g_bind_table[handle].ipc->send(packet.packet(), packet.size()) == false) {
		ERR("Faield to send a packet\n");
		release_handle(handle);
		errno = ECOMM;
		return -2;
	}

	INFO("Recv a reply packet\n");
	
	if (g_bind_table[handle].ipc &&  g_bind_table[handle].ipc->recv(packet.packet(), packet.header_size()) == false) {
		ERR("Send to reply packet fail\n");
		errno = ECOMM;
		return -2;		
	}

	DBG("packet received\n");
	if (packet.payload_size()) {
		if (g_bind_table[handle].ipc && g_bind_table[handle].ipc->recv((char*)packet.packet() +	packet.header_size(), packet.payload_size()) == false) {
			errno = ECOMM;
			return -2;
		}

		if (packet.cmd() == CMD_DONE) {
			cmd_done_t *payload;
			payload = (cmd_done_t*)packet.data();
			if (payload->value < 0) {
				ERR("Error from sensor server [-1 or -2 : socket error, -3 : stopped by	sensor plugin]   value = [%d]\n", payload->value);
				errno = ECOMM;
				return payload->value;
			}
		} else {
			ERR("unexpected server cmd\n");
			errno = ECOMM;
			return -2;
		}
	}

	g_bind_table[handle].sensor_state = SENSOR_STATE_STARTED;

	return 0;

}

EXTAPI int sf_stop(int handle)
{
	cpacket packet(sizeof(cmd_stop_t)+4);
	cmd_stop_t *payload;

	retvm_if( (g_bind_table[handle].ipc == NULL) ||(handle < 0) , -1 , "sensor_stop fail , invalid handle value : %d",handle);


	INFO("Sensor S/F Stopped\n");

	payload = (cmd_stop_t*)packet.data();
	if (!payload) {
		ERR("cannot find memory for send packet.data");
		errno = ENOMEM;
		return -2;
	}

	packet.set_version(PROTOCOL_VERSION);
	packet.set_cmd(CMD_STOP);
	packet.set_payload_size(sizeof(cmd_stop_t));

	if (g_bind_table[handle].ipc && g_bind_table[handle].ipc->send(packet.packet(), packet.size()) == false) {
		ERR("Failed to send a packet\n");
		release_handle(handle);
		errno = ECOMM;
		return -2;
	}

	g_bind_table[handle].sensor_state = SENSOR_STATE_STOPPED;

	return 0;

}

EXTAPI int sf_register_event(int handle , unsigned int event_type ,  event_condition_t *event_condition , sensor_callback_func_t cb , void *cb_data )
{
	cpacket packet(sizeof(cmd_reg_t)+4);
	cmd_reg_t *payload;
	int i;
	int avail_cb_slot_idx = -1;

	int collect_data_flag = 0;

	retvm_if( (g_bind_table[handle].ipc == NULL) ||(handle < 0) , -1 , "sensor_register_cb fail , invalid handle value : %d",handle);

	payload = (cmd_reg_t*)packet.data();
	if (!payload) {
		ERR("cannot find memory for send packet.data");
		errno = ENOMEM;
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
		return -2;
	}

	i = cb_acquire_handle();
	if (i == MAX_CB_BIND_SLOT) {
		ERR("MAX_BIND_SLOT, Too many slot required");
		errno = ENOMEM;
		return -2;
	}
	INFO("Empty cb_slot : %d\n", i);

	g_cb_table[i].my_cb_handle = i;
	g_cb_table[i].my_sf_handle = handle;
		
	INFO("Sensor S/F register cb\n");	

	packet.set_version(PROTOCOL_VERSION);
	packet.set_cmd(CMD_REG);
	packet.set_payload_size(sizeof(cmd_reg_t));
	payload->type = REG_ADD;
	payload->event_type = event_type;

	 if(!event_condition)
		 payload->interval = BASE_GATHERING_INTERVAL;
	 else if((event_condition->cond_op == CONDITION_EQUAL) && (event_condition->cond_value1 > 0 ))
		 payload->interval = event_condition->cond_value1;
	 else
		 payload->interval = BASE_GATHERING_INTERVAL;
	

	INFO("Send CMD_REG command with reg_type : %x , event_typ : %x\n",payload->type , payload->event_type );
	if (g_bind_table[handle].ipc && g_bind_table[handle].ipc->send(packet.packet(), packet.size()) == false) {
		ERR("Faield to send a packet\n");
		cb_release_handle(i);
		release_handle(handle);
		errno = ECOMM;
		return -2;
	}

	if (g_bind_table[handle].ipc && g_bind_table[handle].ipc->recv(packet.packet(), packet.header_size()) == false) {
		ERR("Faield to receive a packet\n");
		cb_release_handle(i);
		release_handle(handle);
		errno = ECOMM;
		return -2;
	}

	if (packet.payload_size()) {
		if (g_bind_table[handle].ipc && g_bind_table[handle].ipc->recv((char*)packet.packet() + packet.header_size(), packet.payload_size()) == false) {
			ERR("Faield to receive a packet\n");
			cb_release_handle(i);
			release_handle(handle);
			errno = ECOMM;
			return -2;
		}

		if (packet.cmd() == CMD_DONE) {
			cmd_done_t *payload;
			payload = (cmd_done_t*)packet.data();
			if (payload->value == -1) {
				ERR("server register fail\n");
				cb_release_handle(i);
				errno = ECOMM;
				return -2;
			} 
		} else {
			ERR("unexpected server cmd\n");
			cb_release_handle(i);
			errno = ECOMM;
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
			case GEOMAGNETIC_EVENT_ATTITUDE_DATA_REPORT_ON_TIME:
				/* fall through */
			case PROXIMITY_EVENT_STATE_REPORT_ON_TIME:
				/* fall through */
			case GYROSCOPE_EVENT_RAW_DATA_REPORT_ON_TIME:
				/* fall through */
			case LIGHT_EVENT_LEVEL_DATA_REPORT_ON_TIME:
				collect_data_flag = 1;
				g_cb_table[i].request_data_id = (event_type & (0xFFFF<<16)) |0x0001;
				break;
			case LIGHT_EVENT_LUX_DATA_REPORT_ON_TIME:
				/* fall through */
			case GEOMAGNETIC_EVENT_RAW_DATA_REPORT_ON_TIME:
				/* fall through */
			case ACCELEROMETER_EVENT_ORIENTATION_DATA_REPORT_ON_TIME:
				/* fall through */
			case PROXIMITY_EVENT_DISTANCE_DATA_REPORT_ON_TIME:
				collect_data_flag = 1;
				g_cb_table[i].request_data_id = (event_type & (0xFFFF<<16)) |0x0002;
				break;
	}

	INFO("key : %s(p:%p), cb_handle value : %d\n", g_cb_table[i].call_back_key ,g_cb_table[i].call_back_key, i );

	if ( collect_data_flag ) {			
		sensor_data_t *collected_data_set;		
		
		collected_data_set = new sensor_data_t [ON_TIME_REQUEST_COUNTER];
		if ( !collected_data_set ) {
			ERR("memory allocation fail for gathering datas\n");
			cb_release_handle(i);
			errno = ECOMM;
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
				return -1;
			}
		}


		if ( g_cb_table[i].gsource_interval != 0 ) {
			g_cb_table[i].source = g_timeout_source_new(g_cb_table[i].gsource_interval);
		} else {
			ERR("Error , gsource_interval value : %u",g_cb_table[i].gsource_interval);
			cb_release_handle(i);
			errno = EINVAL;
			return -1;
		}
		g_source_set_callback (g_cb_table[i].source, sensor_timeout_handler, (gpointer)&g_cb_table[i].my_cb_handle,NULL);			
		g_cb_table[i].gID = g_source_attach (g_cb_table[i].source, NULL);		
		
	}else {		
		g_cb_table[i].request_count = 0;
		g_cb_table[i].request_data_id = 0;
		g_cb_table[i].collected_data = NULL;
			
		if (vconf_notify_key_changed( g_cb_table[i].call_back_key,sensor_changed_cb,(void*)(g_cb_table[i].cb_event_type)) < 0 ) {
			DBG("vconf_add_changed_cb is already registered for key : %s  , my_cb_handle value : %d\n", g_cb_table[i].call_back_key, g_cb_table[i].my_cb_handle);
		} else {
			DBG("vconf_add_chaged_cb success for key : %s  , my_cb_handle value : %d\n", g_cb_table[i].call_back_key, g_cb_table[i].my_cb_handle);
		}
	}
	g_bind_table[handle].cb_slot_num[avail_cb_slot_idx] = i;
	
	return 0;
}


EXTAPI int sf_unregister_event(int handle, unsigned int event_type)
{
	int state = 0;
	cpacket packet(sizeof(cmd_reg_t)+4);
	cmd_reg_t *payload;
	int find_cb_handle =-1;
	int i;

	int collect_data_flag = 0;

	retvm_if( (g_bind_table[handle].ipc == NULL) ||(handle < 0) , -1 , "sensor_unregister_cb fail , invalid handle value : %d",handle);

	payload = (cmd_reg_t*)packet.data();
	if (!payload) {
		ERR("cannot find memory for send packet.data");
		errno = ENOMEM;
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
		return -1;
	}
	

	INFO("Sensor S/F unregister cb\n");

	packet.set_version(PROTOCOL_VERSION);
	packet.set_cmd(CMD_REG);
	packet.set_payload_size(sizeof(cmd_reg_t));	
	payload->type = REG_DEL; 
	payload->event_type = event_type;

	switch (event_type ) {
		case ACCELEROMETER_EVENT_RAW_DATA_REPORT_ON_TIME:
			/* fall through */
		case ACCELEROMETER_EVENT_ORIENTATION_DATA_REPORT_ON_TIME:
			/* fall through */ 
		case GEOMAGNETIC_EVENT_ATTITUDE_DATA_REPORT_ON_TIME:
			/* fall through */ 			
		case PROXIMITY_EVENT_STATE_REPORT_ON_TIME:
			/* fall through */
		case GYROSCOPE_EVENT_RAW_DATA_REPORT_ON_TIME:
			/* fall through */
		case LIGHT_EVENT_LEVEL_DATA_REPORT_ON_TIME:
			/* fall through */
		case GEOMAGNETIC_EVENT_RAW_DATA_REPORT_ON_TIME:
			/* fall through */
		case LIGHT_EVENT_LUX_DATA_REPORT_ON_TIME:
			/* fall through */
		case PROXIMITY_EVENT_DISTANCE_DATA_REPORT_ON_TIME:
			collect_data_flag = 1;				
			break;
	}

	INFO("Send CMD_REG command\n");
	if (g_bind_table[handle].ipc && g_bind_table[handle].ipc->send(packet.packet(), packet.size()) == false) {
		ERR("Faield to send a packet\n");		
		release_handle(handle);
		errno = ECOMM;
		return -2;
	}

	if (g_bind_table[handle].ipc && g_bind_table[handle].ipc->recv(packet.packet(), packet.header_size()) == false) {
		ERR("Failed to recv packet_header\n");
		release_handle(handle);
		errno = ECOMM;
		return -2;
	}

	if (packet.payload_size()) {
		if (g_bind_table[handle].ipc && g_bind_table[handle].ipc->recv((char*)packet.packet() + packet.header_size(), packet.payload_size()) == false) {
			ERR("Failed to recv packet\n");			
			release_handle(handle);
			errno = ECOMM;
			return -2;
		}

	}	

	if ( collect_data_flag ) {
		g_source_destroy(g_cb_table[find_cb_handle].source);
		g_source_unref(g_cb_table[find_cb_handle].source);
		g_cb_table[find_cb_handle].request_count = 0;
		g_cb_table[find_cb_handle].request_data_id = 0;
		g_cb_table[find_cb_handle].gsource_interval = 0;
		
	} else {
		state = vconf_ignore_key_changed(g_cb_table[find_cb_handle].call_back_key, sensor_changed_cb);
		if ( state < 0 ) {		
			ERR("Failed to del callback using by vconf_del_changed_cb for key : %s\n",g_cb_table[find_cb_handle].call_back_key);
			errno = ENODEV;
			state = -2;
		}
	}

	cb_release_handle(find_cb_handle);
	g_bind_table[handle].cb_slot_num[i] = -1;

	return state;
}


EXTAPI int sf_get_data(int handle , unsigned int data_id ,  sensor_data_t* values)
{
	cpacket packet(sizeof(cmd_get_struct_t)+sizeof(base_data_struct)+4);
	cmd_get_data_t *payload;
	cmd_get_struct_t *return_payload;
	int i;
	struct timeval sv;	

	
	retvm_if( (!values) , -1 , "sf_get_data fail , invalid get_values pointer %p", values);
	retvm_if( ( (data_id & 0xFFFF) < 1) || ( (data_id & 0xFFFF) > 0xFFF), -1 , "sf_get_data fail , invalid data_id %d", data_id);
	retvm_if( (g_bind_table[handle].ipc == NULL) ||(handle < 0) , -1 , "sf_get_data fail , invalid handle value : %d",handle);
 
	if(g_bind_table[handle].sensor_state != SENSOR_STATE_STARTED)
	{
		ERR("sensor framewoker doesn't started");
		values->data_accuracy = SENSOR_ACCURACY_UNDEFINED;
		values->data_unit_idx = SENSOR_UNDEFINED_UNIT;
		values->time_stamp = 0;
		values->values_num = 0;
		errno = ECOMM;
		return -2;
	}

	payload = (cmd_get_data_t*)packet.data();
	if (!payload) {
		ERR("cannot find memory for send packet.data");
		errno = ENOMEM;
		return -2;
	}

	packet.set_version(PROTOCOL_VERSION);
	packet.set_cmd(CMD_GET_STRUCT);
	packet.set_payload_size(sizeof(cmd_get_data_t));
	payload->data_id = data_id;

	if (g_bind_table[handle].ipc && g_bind_table[handle].ipc->send(packet.packet(), packet.size()) == false) {		
		release_handle(handle);
		errno = ECOMM;
		return -2;
	}

	if (g_bind_table[handle].ipc && g_bind_table[handle].ipc->recv(packet.packet(), packet.header_size()) == false) {
		release_handle(handle);
		errno = ECOMM;
		return -2;
	}

	if (packet.payload_size()) {
		if (g_bind_table[handle].ipc && g_bind_table[handle].ipc->recv((char*)packet.packet() + packet.header_size(), packet.payload_size()) == false) {
			release_handle(handle);
			errno = ECOMM;
			return -2;
		}
	}

	return_payload = (cmd_get_struct_t*)packet.data();
	if (!return_payload) {
		ERR("cannot find memory for return packet.data");
		errno = ENOMEM;
		return -2;
	}

	if ( return_payload->state < 0 ) {
		ERR("get values fail from server \n");
		values->data_accuracy = SENSOR_ACCURACY_UNDEFINED;
		values->data_unit_idx = SENSOR_UNDEFINED_UNIT;
		values->time_stamp = 0;
		values->values_num = 0;
		errno = ECOMM;
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
		DBG("client , get_data_value , [%d] : %f \n", i , values->values[i]);
	}
	

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

	sensor_data_t *base_data_values = (sensor_data_t*)malloc(sizeof(sensor_data_t));

	retvm_if( curr_state==NULL , -1 , "sf_check_rotation fail , invalid curr_state");

	*curr_state = ROTATION_UNKNOWN;
	
	INFO("Sensor_attach_channel from pid : %d",getpid());
	
	handle = sf_connect( ACCELEROMETER_SENSOR);
	if (handle<0)
	{
		ERR("sensor attach fail\n");
		return 0;
	}

	state = sf_start(handle, 0);
	if(state < 0)
	{
		ERR("sf_start fail\n");
		return 0;
	}

	state = sf_get_data(handle, ACCELEROMETER_BASE_DATA_SET, base_data_values);
	if(state < 0)
	{
		ERR("sf_get_data fail\n");
		return 0;
	}

	state = sf_stop(handle);
	if(state < 0)
	{
		ERR("sf_stop fail\n");
		return 0;
	}

	state = sf_disconnect(handle);
	if(state < 0)
	{
		ERR("sf_disconnect fail\n");
		return 0;
	}

	if(vconf_get_int(LCD_TYPE_KEY, &lcd_type) != 0)
		lcd_type = 0;

	if((base_data_values->values[0] > XY_POSITIVE_THD || base_data_values->values[0] < XY_NEGATIVE_THD) || (base_data_values->values[1] > XY_POSITIVE_THD || base_data_values->values[1] < XY_NEGATIVE_THD))
	{
		atan_value = atan2(base_data_values->values[1],base_data_values->values[0]);
		acc_theta = ((int)(atan_value * (RADIAN_VALUE) + 270.0))%360;
		raw_z = (double)(base_data_values->values[2]) / 9.8;

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
	
	state = 0;	
	if(base_data_values)
		free(base_data_values);

	return state;

}

//! End of a file
