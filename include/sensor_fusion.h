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



#ifndef __SAMSUNG_LINUX_SENSOR_FUSION_H__
#define __SAMSUNG_LINUX_SENSOR_FUSION_H__


//! Pre-defined events for the accelometer sensor
//! Sensor Plugin developer can add more event to their own headers

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @defgroup SENSOR_FUSION Fusion Sensor
 * @ingroup SENSOR_FRAMEWORK
 *
 * These APIs are used to control the Fusion sensor.
 * @{
 */

/**
 * @file        sensor_accel.h
 * @brief       This file contains the prototypes of the Accelerometer sensor API
 * @author  JuHyun Kim(jh8212.kim@samsung.com)
 * @date        2010-01-24
 * @version     0.1
 */


enum fusion_data_id {
	FUSION_BASE_DATA_SET = (FUSION_SENSOR << 16) | 0x0001,
	FUSION_ROTATION_VECTOR_DATA_SET = (FUSION_SENSOR<< 16) | 0x0002,
	FUSION_ROTATION_MATRIX_DATA_SET  = (FUSION_SENSOR << 16) | 0x0004,
};
enum fusion_evet_type_t {
	FUSION_SENSOR_EVENT_RAW_DATA_REPORT_ON_TIME      = (FUSION_SENSOR << 16) | 0x0001,
	FUSION_ROTATION_VECTOR_EVENT_DATA_REPORT_ON_TIME = (FUSION_SENSOR << 16) | 0x0002,
	FUSION_ROTATION_MATRIX_EVENT_DATA_REPORT_ON_TIME = (FUSION_SENSOR << 16) | 0x0004,
};


/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif
//! End of a file
