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



#ifndef __SAMSUNG_LINUX_SENSOR_MOTION_H__
#define __SAMSUNG_LINUX_SENSOR_MOTION_H__


//! Pre-defined events for the motion sensor
//! Sensor Plugin developer can add more event to their own headers

#ifdef __cplusplus
extern "C"
{
#endif


enum motion_evet_type {		
	MOTION_ENGINE_EVENT_SNAP				= (MOTION_SENSOR<<16) |0x0001,
	MOTION_ENGINE_EVENT_SHAKE				= (MOTION_SENSOR<<16) |0x0002,
	MOTION_ENGINE_EVENT_DOUBLETAP   		= (MOTION_SENSOR<<16) |0x0004,
	MOTION_ENGINE_EVENT_PANNING     		= (MOTION_SENSOR<<16) |0x0008,
	MOTION_ENGINE_EVENT_TOP_TO_BOTTOM       = (MOTION_SENSOR<<16) |0x0010,
};

enum motion_snap_event {
	MOTION_ENGIEN_SNAP_NONE,
	MOTION_ENGIEN_NEGATIVE_SNAP_X,
	MOTION_ENGIEN_POSITIVE_SNAP_X,
	MOTION_ENGIEN_NEGATIVE_SNAP_Y,
	MOTION_ENGIEN_POSITIVE_SNAP_Y,
	MOTION_ENGIEN_NEGATIVE_SNAP_Z,
	MOTION_ENGIEN_POSITIVE_SNAP_Z,
	MOTION_ENGIEN_SNAP_LEFT  = MOTION_ENGIEN_NEGATIVE_SNAP_X,
	MOTION_ENGIEN_SNAP_RIGHT = MOTION_ENGIEN_POSITIVE_SNAP_X,

};

enum motion_shake_event {
	MOTION_ENGIEN_SHAKE_NONE,
	MOTION_ENGIEN_SHAKE_DETECTION,
	MOTION_ENGIEN_SHAKE_CONTINUING,
	MOTION_ENGIEN_SHAKE_FINISH,
	MOTION_ENGINE_SHAKE_BREAK,
};

enum motion_doubletap_event {
	MOTION_ENGIEN_DOUBLTAP_NONE,
	MOTION_ENGIEN_DOUBLTAP_DETECTION,
};

enum motion_top_to_bottom_event {
	MOTION_ENGIEN_TOP_TO_BOTTOM_NONE,
	MOTION_ENGIEN_TOP_TO_BOTTOM_WAIT,
	MOTION_ENGIEN_TOP_TO_BOTTOM_DETECTION,
};


/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif
//! End of a file
