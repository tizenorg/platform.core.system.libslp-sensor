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



#ifndef __SAMSUNG_LINUX_SENSOR_ACCEL_H__
#define __SAMSUNG_LINUX_SENSOR_ACCEL_H__


//! Pre-defined events for the accelometer sensor
//! Sensor Plugin developer can add more event to their own headers

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @defgroup SENSOR_ACCEL Accelerometer Sensor
 * @ingroup SENSOR_FRAMEWORK
 * 
 * These APIs are used to control the Accelerometer sensor.
 * @{
 */

/** 
 * @file        sensor_accel.h
 * @brief       This file contains the prototypes of the Accelerometer sensor API
 * @author  JuHyun Kim(jh8212.kim@samsung.com)
 * @date        2010-01-24
 * @version     0.1
 */


enum accelerometer_data_id {
	ACCELEROMETER_BASE_DATA_SET = (ACCELEROMETER_SENSOR<<16) | 0x0001,
	ACCELEROMETER_ORIENTATION_DATA_SET = (ACCELEROMETER_SENSOR<<16) | 0x0002,
};

enum accelerometer_event_type {		
	ACCELEROMETER_EVENT_ROTATION_CHECK		    		= (ACCELEROMETER_SENSOR<<16) | 0x0001,
	ACCELEROMETER_EVENT_RAW_DATA_REPORT_ON_TIME			= (ACCELEROMETER_SENSOR<<16) | 0x0002,
	ACCELEROMETER_EVENT_CALIBRATION_NEEDED				= (ACCELEROMETER_SENSOR<<16) | 0x0004,
	ACCELEROMETER_EVENT_SET_HORIZON						= (ACCELEROMETER_SENSOR<<16) | 0x0008,
	ACCELEROMETER_EVENT_SET_WAKEUP 						= (ACCELEROMETER_SENSOR<<16) | 0x0010,
	ACCELEROMETER_EVENT_ORIENTATION_DATA_REPORT_ON_TIME = (ACCELEROMETER_SENSOR<<16) | 0x0011,
};


enum accelerometer_rotate_state {
	ROTATION_UNKNOWN	 		= 0,
	ROTATION_LANDSCAPE_LEFT		= 1,
	ROTATION_PORTRAIT_TOP		= 2,
	ROTATION_PORTRAIT_BTM		= 3,
	ROTATION_LANDSCAPE_RIGHT	= 4,
	ROTATION_EVENT_0			= 2,	/*CCW base*/
	ROTATION_EVENT_90			= 1,	/*CCW base*/
	ROTATION_EVENT_180			= 3,	/*CCW base*/
	ROTATION_EVENT_270			= 4,	/*CCW base*/
};


struct rotation_event {
	enum accelerometer_rotate_state rotation;
	enum accelerometer_rotate_state rm[2];
};


/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif
//! End of a file
