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



#ifndef __SAMSUNG_LINUX_SENSOR_BAROMETER_H__
#define __SAMSUNG_LINUX_SENSOR_BAROMETER_H__

//! Pre-defined events for the barometerscope sensor
//! Sensor Plugin developer can add more event to their own headers

#ifdef __cplusplus
extern "C"
{
#endif


/**
 * @defgroup SENSOR_BAROMETER barometer Sensor
 * @ingroup SENSOR_FRAMEWORK
 *
 * These APIs are used to control the barometer sensor.
 * @{
 */

/**
 * @file        sensor_barometer.h
 * @brief       This file contains the prototypes of the barometer sensor API
 * @author  JuHyun Kim(jh8212.kim@samsung.com)
 * @date        2010-01-24
 * @version     0.1
 */


enum barometer_data_id {
		BAROMETER_BASE_DATA_SET = (BAROMETER_SENSOR<<16) | 0x0001,
		BAROMETER_TEMPERATURE_DATA_SET = (BAROMETER_SENSOR<<16) | 0x0002,
		BAROMETER_ALTITUDE_DATA_SET = (BAROMETER_SENSOR<<16) | 0x0004,
};


enum barometer_evet_type {
	BAROMETER_EVENT_RAW_DATA_REPORT_ON_TIME = (BAROMETER_SENSOR<<16) |0x0001,
	BAROMETER_EVENT_TEMPERATURE_DATA_REPORT_ON_TIME = (BAROMETER_SENSOR<<16) |0x0002,
	BAROMETER_EVENT_ALTITUDE_DATA_REPORT_ON_TIME = (BAROMETER_SENSOR<<16) |0x0004,

};

enum barometer_property_id {
	BAROMETER_PROPERTY_UNKNOWN = 0,
	BAROMETER_PROPERTY_SET_CALIBRATION,
	BAROMETER_PROPERTY_CHECK_CALIBRATION,
};


/**
 * @}
 */

#ifdef __cplusplus
}
#endif


#endif
//! End of a file
