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



/**
 *
 * @ingroup   SLP_PG
 * @defgroup   SENSOR_FRAMEWORK_PG SensorFW
@{


<h1 class="pg">Introduction</h1>
<h2 class="pg">Purpose</h2>

The purpose of this document is to describe how applications can use the Sensor Framework APIs to perform different operations on different supported sensors through the sensor-server. This document gives programming guidelines to application engineers.



<h2 class="pg">Scope</h2>

The scope of this document is limited to Sensor framework library API usage.

<h2 class="pg">Abbreviations</h2>

<table>
	<tr>
		<td>
		API
		</td>
		<td>
		Application Programming Interface
		</td>
	</tr>
	<tr>
		<td>
		TCP
		</td>
		<td>
		Transmission Control Protocol
		</td>
	</tr>
</table>

<h1 class="pg">Sensor Framework Overview</h1>

<h2 class="pg">What is sensor framework</h2>

Sensor devices are used widely in mobile devices to enhance user experience. Most modern mobile OS have a framework which manages sensor hardware on the platform and provides convenient APIs for applications.

<b>Sensors Framework</b> is a middleware, which provides a sensor server for creating plug-ins and a medium through which the client applications are connected with the sensor hardware to exchange sensor data. The sensor plug-ins retrieve data from sensor hardware and enable the client applications to use the data for specific requirements.

The following sensor types are supported by the sensor framework provided that the device supports them:


<b>Accelerometer</b> sensor<br>	
<b>Geomagnetic</b> sensor<br>
<b>Proximity</b> sensor<br>	
<b>Light</b> sensor<br> 
<b>Gyroscope</b> sensor<br> 

@}
@defgroup SENSOR_Architecture 1.Architecture 
@ingroup SENSOR_FRAMEWORK_PG
@{

<h2 class="pg">Sensor Framework Architecture</h2>

Sensor framework is basically a client-server architecture. It is mainly composed of two parts:
- <b>Sensor Server</b>: Sensor server is a daemon, which uniquely talks to sensor drivers in the system and dispatches sensor data to applications. Sensor server takes care of interacting with sensor driver in hardware initialization, driver configuration and data fetching, to manage all sensors on platform.

- <b>Sensor API Library</b> (also called as <b>client</b> library): Applications wanting to access sensor services should communicate with the daemon through the sensor API library. An API library allows application to access sensor service. As shown in the diagram applications/middleware frameworks can have the sensor-framework client library in their process context.

Unix Socket is used as IPC for communication between API library and daemon, including data transportation and command exchange.


The client library provides different functionality to the application like attaching an application to a sensor, getting data from it, getting updates if a value is changed etc. It contains API's designed to fulfill these features.

@image html SLP_SensorFW_PG_image1.png
@}
@defgroup SENSOR_Feature 2.Feature
@ingroup SENSOR_FRAMEWORK_PG
@{

<h2 class="pg">Sensor Framework Features</h2>
<b>Sensor framework client features</b>
-# Available in shared library form.
-# Provides set of APIs for accessing data from sensors.
-# Provide unique access control mechanism through client server architecture.
-# Provision to get processed/raw data from sensors.
-# Provides functionality for application to register its own callback for the case of a sensor value change.
-# Provides information about a sensor like maximum/minimum polling time.
-# Direct knowledge of rotation status with raw data access.

<b>Sensor framework server features</b>
-# The actual implementation of the features supported by sensor-framework library is done in the sensor-server.
-# Sensor-server checks the devices periodically and if there is an event that meets any conditions that are being monitored, the event is reported to applications which have registered for it.
-# Sensor framework maintains a separate thread for every connection request which is called a "ipc worker".
-# IPC Workers extract the work to be done in the form of command packets.
-# These command packets are maintained in a command queue in server.
-# This queue is further fed to a command executer which extracts the command from the packets.
-# As per the command, the server allocates a plug-in worker which interacts with the device-driver interfaces.
-# Server has predefined information about the sensor.

@}
<h1 class="pg">Sensor Framework Functions</h1>

@defgroup Use_Cases1 Getting Information about available sensors
@ingroup SENSOR_UC Use Cases
@{

<h2 class="pg">Getting Information about available sensors</h2>

<b>API : sf_is_sensor_event_available</b><br>
<b>Parameter In</b> : sensor_type_t desired_sensor_type , unsigned int desired_event_type<br>
<b>Parameter Return</b> : int <br>
<b>Functionality</b> : This API checks the availability of the sensor type and event type specified by the input parameters. Some of the supported sensor types are ACCELEROMETER_SENSOR_TYPE, GEOMAGNETIC_SENSOR_TYPE etc.  This API will return 0 when the event is available and negative value when it is not available.<br>


<b>API : sf_get_properties</b><br>
<b>Parameter In</b> : sensor_type_t sensor_type <br>
<b>Parameter Out</b> : sensor_properties_t *sensor_properties<br>
<b>Parameter Return</b> : int <br> 
<b>Functionality</b> This API retrieves the properties of a sensor type, such as unit of sensor data, max/min range of sensor data and sensor resolution to the output parameter sensor_properties.<br>




Sample Code (Showing how to get basic information about available sensors)

@code
#include <stdio.h>
#include <sensor.h>
#include <malloc.h>
int main()
{
	printf("\n Sensor test : Start of the program \n");
	int ret_val=0;
             sensor_properties_t *sensor_properties;
             sensor_type_t sensor_type = ACCELEROMETER_SENSOR;
             int event_type  = ACCELEROMETER_EVENT_ROTATION_CHECK;

	//Check whether the sensor type and the event is available 

	ret_val = sf_is_sensor_event_available (sensor_type,event_type);
	printf("\n Sensor Error : Return value is %d \n",ret_val);
	if(ret_val != 0)
	{
		printf("\n Sensor test : sensor %d is not present\n ",   
                          sensor_type);
		return -1;
	}
	printf("\n Sensor test : Sensor type is available and valid \n");

	//Get the sensor properties
	sensor_properties =  (sensor_properties_t *)malloc(sizeof(sensor_properties_t));

	ret_val = sf_get_properties (sensor_type, sensor_properties);
	if(ret_val != 0)
	{
		printf("\n Sensor Error : sensor_property get FAILED with code %d",ret_val);
		return -1;
	}
	printf("\nSensor test : sensor_properties are \n unit ID : %d \n max range : %f \n 
		min range : %f \n sensor_resolution : %f \n ",
		sensor_properties->sensor_unit_idx, sensor_properties->sensor_min_range,
		sensor_properties->sensor_max_range, sensor_properties->sensor_resolution);

     if(sensor_properties)
          free(sensor_properties);

     return 0;
@endcode


@}
@defgroup Use_Cases2 Connecting and disconnecting a sensor
@ingroup SENSOR_UC Use Cases
@{


<h2 class="pg">Connecting and disconnecting a sensor</h2>

<b>Accelerometer sensor:</b><br>
The accelerometer sensor is used to measure the acceleration of the device. The three dimensional coordinate system is used to illustrate direction of the acceleration as shown in figure 2 below. The X- and Y- axis defines a plane where Z-axis direction is perpendicular to the XY plane. When a phone is moving along an axis, the acceleration is positive if movement is towards positive direction and negative if movement is on negative direction. E.g. when a phone is moving along x-axis to the direction of -x, the acceleration is negative.

<b>Geomagnetic sensor:</b><br>
Geomagnetic sensor indicates the strength of the geomagnetic flux density in the X, Y and Z axes. This sensor is used to finds the orientation or attitude of a body is a description of how it is aligned to the space it is in. 

<b>Proximity sensor:</b><br>
A proximity sensor is a sensor able to detect the presence of nearby objects without any physical contact. i.e. It indicates if the device is close or not close to the user. 
A proximity sensor often emits an electromagnetic or electrostatic field, or a beam of electromagnetic radiation (infrared, for instance), and looks for changes in the field or return signal. The object being sensed is often referred to as the proximity sensor's target

<b>Light sensor</b><br>
Light sensor measures the amount of light that it receives or the surrounding light conditions.  The ambient light state is measured as a step value, where 0 is very dark and 10 is bright sunlight. A number of predefined brightness level constants are specified in the data class reference.

<b>Gyroscope sensor:</b><br>
A gyroscope is a device used primarily for navigation and measurement of angular velocity. Gyroscopes, also called angular rate sensors, measure how quickly an object rotates. This rate of rotation can be measured along any of the three axes X, Y and Z. 3-axis gyroscopes are often implemented with a 3-axis accelerometer to provide a full 6 degree-of-freedom (DoF) motion tracking system.

@image html SLP_SensorFW_PG_image2.png

<b>API : sf_connect</b><br>
<b>Parameter In</b>: sensor_type_t sensor_type<br>
<b>Parameter Return</b>: int  <br>
<b>Functionality</b>:  This API connects a sensor type to respective sensor. The application calls this API with the type of the sensor (e.g. ACCELEROMETER_SENSOR) on the basis that the server takes the decision of which plug-in is to be connected.  Once the sensor is connected, the application can proceed with data processing.
This API returns a positive handle which should be used by application to communicate on  sensor type.
<br>
<b>API : sf_disconnect</b><br>
<b>Parameter In</b>: int handle<br>
<b>Parameter Return</b>: int  <br>
<b>Functionality</b>:  This API disconnects an attached sensor from an application. Application must use the handle retuned after attaching the sensor.  After detaching, the corresponding handle will be released.<br>

@}
@defgroup Use_Cases3 Processing data after connecting to a sensor
@ingroup SENSOR_UC Use Cases
@{

<h2 class="pg">Processing data after connecting to a sensor</h2>
Once the application is connected to a sensor, data processing can be done using these API's

<b>API : sf_start</b><br>
<b>Parameter In</b>: Int handle, int option  <br>
<b>Parameter Return</b>: int  <br>
<b>Functionality</b>:  This API sends a start command to the sensor server. This informs the server that the client side is ready to handle data and start processing. The option input parameter should be set to '0' for current usage.<br>


<b>API : sf_get_data</b><br>
<b>Parameter In</b>:  int handle, int data_id<br>
<b>Parameter Out</b>:sensor_data_t* values<br>
<b>Parameter Return</b>: int  <br>
<b>Functionality</b>:  This API gets the processed data (sensor information) from a sensor through the sensor server. The data is loaded in the values output parameter.<br>


<b>API : sf_stop</b><br>
<b>Parameter In</b>:  int handle<br>
<b>Parameter Return</b>:  int  <br>
<b>Functionality</b>: This API sends a stop command to the Sensor server indicating that the data processing has stopped from the application side for this time.<br>

@}
@defgroup Use_Cases4 Processing data without connecting to a sensor
@ingroup SENSOR_UC Use Cases
@{

<h2 class="pg">Processing data without connecting to a sensor</h2>

<b>API : sf_read_raw_data</b><br>
<b>Parameter In</b>: sensor_type_t sensor_type<br>
<b>Parameter Out</b>: float values[] , size_t *values_size<br>
<b>Parameter Return</b>: int  <br>
<b>Functionality</b>:   This API gets raw data from a sensor without connecting to the sensor-server. This doesn't need a connection handle. The type of sensor is supplied and return data is stored in the output parameter values [].<br>



<b>API : sf_check_rotation</b><br>
<b>Parameter Out</b>:  unsigned long *state<br>
<b>Parameter Return</b>:  int  <br>
<b>Functionality</b>: This API used to get the current rotation state. (i.e. ROTATION_EVENT_0, ROTATION_EVENT_90, ROTATION_EVENT_180 & ROTATION_EVENT_270 ). This API will directly access the sensor without connecting the process with the sensor-server. Result will be stored in the output parameter state.<br>




Sample Code (Showing how to connect to server and get data (processed/raw))

@code
#include <stdio.h>
#include <sensor.h>
#include <malloc.h>
	
int main()
{
             printf("\n Sensor test : Start of the program \n");
	  
	int ret_val=0;
	int my_handle = 0;
	int get_data_id =ACCELEROMETER_BASE_DATA_SET;
		
	float raw_data[3];
	size_t size = 3;

	unsigned long rotaion_state = 0;
	sensor_data_t *sensor_data = (sensor_data_t*)malloc(sizeof(sensor_data_t));
      	sensor_properties_t *sensor_properties = NULL;

             sensor_type_t sensor_type = ACCELEROMETER_SENSOR;
             int event_type  = ACCELEROMETER_EVENT_ROTATION_CHECK;

	//Check whether the Accelerometer sensor type and the event is available 
	ret_val = sf_is_sensor_event_available (sensor_type,event_type);

	printf("\n Sensor Error : Return value is %d \n",ret_val);
	if(ret_val != 0)
	{
		printf("\n Sensor test : sensor %d is not present\n ", sensor_type);
		return -1;
	}
	printf("\n Sensor test : Sensor type is available and valid \n");

	//Get the sensor properties
	sensor_properties =  (sensor_properties_t *)malloc(sizeof(sensor_properties_t));

	ret_val = sf_get_properties (sensor_type, sensor_properties);
	if(ret_val != 0)
	{
		printf("\n Sensor Error : sf_get_property get FAILED with code  %d",ret_val);
		return -1;
	}
	printf("\nSensor test : sensor_properties are \n unit ID : %d \n max range : %f \n 
		min range : %f \n sensor_resolution : %f \n ",
		sensor_properties->sensor_unit_idx, sensor_properties->sensor_min_range,
		sensor_properties->sensor_max_range, sensor_properties->sensor_resolution);

	
	//Connect to the sensor through server . 
	my_handle = sf_connect(sensor_type);
	if(my_handle < 0)
	{
		printf("\n Sensor Error : sf_connect FAILED with code %d",my_handle);
		return -1;
	}

	printf("\n Sensor test : sf_connect :  handle is %d",my_handle);


	//Start the communication
	ret_val = sf_start(my_handle,0);
	if(ret_val != 0)
	{
		printf("\n Sensor Error : sensor_start FAILED with code  %d",ret_val);
		return -1;
	}


	//Get the processed data from server 
	ret_val = sf_get_data(my_handle,get_data_id,sensor_data);
	if(ret_val != 0)
	{
		printf("\n Sensor Error : sensor_get_data FAILED with code %d",ret_val);
		return -1;
	}
	printf("\n Sensor Test:  Gathered Data : \n X : %f \n Y : %f \n Z : %f \n Accuracy : %d \n 
		Unit ID : %d \n Time Stamp : %lld", sensor_data->values[0], sensor_data->values[1],
 		sensor_data->values[2], sensor_data->data_accuracy, sensor_data->data_unit_idx,
 		sensor_data->time_stamp);
		

	//Get the Raw Data(Bypass server)
	ret_val = sf_read_raw_data(sensor_type,(float *)raw_data, &size);
	if(ret_val != 0)
	{
		printf("\n Sensor Error : sensor_get_data FAILED with code %d",ret_val);
		return -1;
	}
	
	printf("\n Seneor test : My raw data is x= %f y= %f and z= %f \n",
		raw_data[0], raw_data[1], raw_data[2]);
	

	//Get the rotation/orientation 
	ret_val = sf_check_rotation(&rotaion_state); 

	if(ret_val != 0)
	{
		printf("\n Sensor Error : sf_check_rotation FAILED with code %d",ret_val);
		return -1;
	}
	
	switch(rotaion_state)
	{
		case ROTATION_EVENT_0 :
		printf("\n Sensor test : Rotation ROTATION_EVENT_0 \n");
			break;
		case ROTATION_EVENT_90:
		printf("\n Sensor test : Rotation is ROTATION_EVENT_90 \n");
			break;
		case ROTATION_EVENT_180 : 
		printf("\n Sensor test : Rotation is ROTATION_EVENT_180 \n");
			break;
		case ROTATION_EVENT_270 :
		printf("\n Sensor test : Rotation ROTATION_EVENT_270 \n");
			break;
		default : 
            	printf("\n Sensor test : Rotation is ROTATION_UNKNOWN \n");

	}     
 
             if(sensor_properties)
                 free(sensor_properties);
	  	  
	     if(sensor_data)
	     free(sensor_data);
	
           return 0;
}
@endcode


@}
@defgroup Use_Cases5 Subscribing and unsubscribing a Callback function for sensor events
@ingroup SENSOR_UC Use Cases
@{

<h2 class="pg">Subscribing and unsubscribing a Callback function for sensor events</h2>

<b>API : sf_register_event</b><br>
<b>Parameter In</b> :   int handle , unsigned int event_type , event_conditon_t *event_condition , sensor_callback_func_t cb , void *cb_data<br>
<b>Parameter Return</b> :  int  <br>
<b>Functionality</b> :   This API registers a user defined callback function with a connected sensor for a particular event. This callback function will be called when there is a change in the state of respective sensor. cb_data will be the parameter used during the callback call. Callback interval can be adjusted using the event_condition_t argument.<br>



<b>API : sf_unregister_event</b><br>
<b>Parameter In</b> :   int handle, unsigned int event_type<br>
<b>Parameter Return</b> :  int  <br>
<b>Functionality</b> :   This API de-registers a user defined callback function with a sensor registered with the specified handle. After unsubscribing, no event will be sent to the application.<br>


Sample code (This is an example showing how rotation event can be handled easily just by using these sensor framework API's)



@code
#include <stdio.h>
#include <glib.h>
#include <sensor.h>

static GMainLoop *mainloop;

void my_callback_func(unsigned int event_type, sensor_event_data_t *event, void *data)
{
	int *my_event_data;
		
	if (event_type != ACCELEROMETER_EVENT_ROTATION_CHECK) {
		printf("my_callback_func received data fail\n");	
		return;
	}
	my_event_data = (int *)(event);

	switch( *my_event_data ) {
		case ROTATION_EVENT_0:
			printf("my_callback_func received data ROTATION_EVENT_0\n");
			break;
		case ROTATION_EVENT_90:
			printf("my_callback_func received data ROTATION_EVENT_90\n");
			break;
		case ROTATION_EVENT_180:
			printf("my_callback_func received data ROTATION_EVENT_180\n");
			break;
		case ROTATION_EVENT_270:
			printf("my_callback_func received data ROTATION_EVENT_270\n");
			break;
			
		default:
			printf("my_callback_func received data unexpected\n");
			break;
	}

}

static void sig_quit(int signo)
{
	if (mainloop)
	{
		g_main_loop_quit(mainloop);
	}
}

int main(int argc, char *argv[])
{
	int handle;
	int state;

	signal(SIGINT, sig_quit);
	signal(SIGTERM, sig_quit);
	signal(SIGQUIT, sig_quit);

	mainloop = g_main_loop_new(NULL, FALSE);

	handle = sf_connect( ACCELEROMETER_SENSOR );
	if (handle<0) {
		printf("sensor attach fail\n");
		return -1;
		
	}


	state = sf_register_event(handle, ACCELEROMETER_EVENT_ROTATION_CHECK, NULL , my_callback_func,NULL);
	if (state<0) {
		printf("sensor_register_cb fail\n");
		
	}

	printf("Start SF\n");
	state = sf_start(handle, 0);
	if (state<0) {
		printf("SLP_sensor_start fail\n");
		
	}
	printf("Start SF done\n");

	g_main_loop_run(mainloop);
	g_main_loop_unref(mainloop);

	state = sf_unregister_event(handle, ACCELEROMETER_EVENT_ROTATION_CHECK );
	if (handle<0) {
		printf("t sensor_unregister_cb fail\n");
	}
	sf_stop(handle);
	sf_disconnect(handle);
	return 0;
}
@endcode



@}
@defgroup Reference
@ingroup SENSOR_FRAMEWORK_PG
@{

<sub class="ref">Also see</sub> [ @ref SENSOR_FRAMEWORK API reference]

<h1 class="pg">Appendixes</h1>

<h2 class="pg">API Overview</h2>



<table>
	<tr>
		<th>S No</th>
		<th>API NAME</th>
		<th>In parameter</th>
		<th>Functionality</th>
	</tr>
	<tr>
		<td>1</td>
		<td>sf_is_sensor_event_available</td>
		<td>sensor_type_t desired_sensor_type , unsigned int desired_event_type</td>
		<td>This checks the availability of the sensor type and event type specified by the input parameters.</td>
	</tr>
	<tr>
		<td>2</td>
		<td>sf_get_properties</td>
		<td>sensor_type_t sensor_type, sensor_properties_t *return_properties</td>
		<td>This API retrieves the properties of a sensor type like data unit ID, max range, min range and resolution</td>
	</tr>
	<tr>
		<td>3</td>
		<td>sf_connect</td>
		<td>sensor_type_t sensor_type</td>
		<td>This API attaches/connects to a supplied type of sensor and returns an handle</td>
	</tr>
	<tr>
		<td>4</td>
		<td>sf_disconnect</td>
		<td>sensor_type_t sensor_type</td>
		<td>This API detaches/disconnects the handle from the sensor</td>
	</tr>
	<tr>
		<td>5</td>
		<td>sf_start</td>
		<td>Int handle</td>
		<td>This API sends start command to the sensor</td>
	</tr>
	<tr>
		<td>6</td>
		<td>sf_stop</td>
		<td>Int handle</td>
		<td>This API sends stop command to the sensor</td>
	</tr>
	<tr>
		<td>7</td>
		<td>sf_register_event</td>
		<td>int handle , unsigned int event_type , event_conditon_t *event_condition , sensor_callback_func_t cb , void *cb_data</td>
		<td>This API registers a user callback function with sensor . Whenever there is a change in the sensor state on a predefined event, this callback will be called</td> 
	</tr>
	<tr>
		<td>8</td>
		<td>sf_unregister_event</td>
		<td>int handle, unsigned int event_type</td>
		<td>This API de-registers the user callback function for the change in stage of a predefined event</td>
	</tr>
	<tr>
		<td>9</td>
		<td>sf_get_data</td>
		<td>int handle , unsigned int data_id , sensor_data_t* values</td>
		<td>This API gets tje data (processed)from a sensor through the sensor server</td>
	</tr>
	<tr>
		<td>10</td>
		<td>sf_check_rotation</td>
		<td>unsigned long *curr_state</td>
		<td>This API gets the current rotation value such as ROTATION_LANDSCAPE_LEFT ,  ROTATION_PORTRAIT_TOP  etc</td>
	</tr>
	<tr>
		<td>11</td>
		<td>sf_read_raw_data</td>
		<td>sensor_type_t sensor_type , float values[] , size_t *values_size</td>
		<td>This API reads raw data without connecting the process with the sensor</td>
	</tr>
</table>



<h2 class="pg">Data structures</h2>
Few reference sample codes are attached for application developer to well understand the System Framework Module.

<b>Sensor Type:</b>
@code
typedef enum {
	UNKNOWN_SENSOR			= 0x0000,
	ACCELEROMETER_SENSOR		= 0x0001,		
	GEOMAGNETIC_SENSOR		= 0x0002,	
	LIGHT_SENSOR			= 0x0004,	
	PROXIMITY_SENSOR		= 0x0008,		
	THERMOMETER_SENSOR		= 0x0010,	
	GYROSCOPE_SENSOR		= 0x0020,	
	PRESSURE_SENSOR			= 0x0040	
} sensor_type_t;
@endcode

<b>Event Condition:</b>
@code
typedef enum {
	CONDITION_NO_OP,
	CONDITION_EQUAL,
	CONDITION_GREAT_THAN,
	CONDITION_LESS_THAN,	
} condition_op_t;

typedef struct {
	condition_op_t cond_op;
	float cond_value1;
} event_conditon_t;
@endcode

<b>Sensor Event Data:</b>
@code
typedef struct {
		size_t event_data_size;
		void *event_data;
} sensor_event_data_t;
@endcode


<b>Call back function:</b>
@code
typedef void (*sensor_callback_func_t)(unsigned int, sensor_event_data_t *, void *);  

<b>Sensor Data:</b>
@code
typedef struct {
	int data_accuracy;
	int data_unit_idx;
	unsigned long long time_stamp;
	int values_num;
	float values[12];
} sensor_data_t;
@endcode


<b>Sensor Properties:</b>
@code
typedef struct {
	int sensor_unit_idx;
	float sensor_min_range;
	float sensor_max_range;
	float sensor_resolution;
} sensor_properties_t;
@endcode


<b>Sensor data unit index:</b>
@code
enum sensor_data_unit_idx {
	SENSOR_UNDEFINED_UNIT,
	SENSOR_UNIT_METRE_PER_SECOND_SQUARED,
	SENSOR_UNIT_MICRO_TESLA,
	SENSOR_UNIT_DEGREE,
	SENSOR_UNIT_LUX,
	SENSOR_UNIT_CENTIMETER,
	SENSOR_UNIT_LEVEL_1_TO_10,
	SENSOR_UNIT_STATE_ON_OFF,
	SENSOR_UNIT_DEGREE_PER_SECOND,
	SENSOR_UNIT_IDX_END
};
@endcode


<b>Accelerometer Data ID:</b>
@code
enum accelerometer_data_id {
	ACCELEROMETER_BASE_DATA_SET = (ACCELEROMETER_SENSOR<<16) | 0x0001,
};
@endcode


<b>Accelerometer Event Type:</b>
@code
enum accelerometer_evet_type {		
	ACCELEROMETER_EVENT_ROTATION_CHECK
			= (ACCELEROMETER_SENSOR<<16) |0x0001,
	ACCELEROMETER_EVENT_RAW_DATA_REPORT_ON_TIME
			= (ACCELEROMETER_SENSOR<<16) |0x0002,
	ACCELEROMETER_EVENT_CALIBRATION_NEEDED
			= (ACCELEROMETER_SENSOR<<16) |0x0004,
};
@endcode


<b>Accelerometer Rotate state:</b>
@code
enum accelerometer_rotate_state {
	ROTATION_UNKNOWN			= 0,
	ROTATION_LANDSCAPE_LEFT			= 1,
	ROTATION_PORTRAIT_TOP			= 2,
	ROTATION_PORTRAIT_BTM			= 3,
	ROTATION_LANDSCAPE_RIGHT		= 4,
	ROTATION_EVENT_0			= 2,
	ROTATION_EVENT_90			= 1,	
	ROTATION_EVENT_180			= 3,
	ROTATION_EVENT_270			= 4,
};
@endcode


<b>Geo Magnetic Data ID:</b>
@code
enum geomag_data_id {
	GEOMAGNETIC_BASE_DATA_SET = (GEOMAGNETIC_SENSOR<<16) | 0x0001,
	GEOMAGNETIC_RAW_DATA_SET = (GEOMAGNETIC_SENSOR<<16) | 0x0002,
};
@endcode


<b>Geo Magnetic Event Type:</b>
@code
enum geomag_evet_type {			
	GEOMAGNETIC_EVENT_CALIBRATION_NEEDED		= (GEOMAGNETIC_SENSOR<<16) |0x0001,
	GEOMAGNETIC_EVENT_ATTITUDE_DATA_REPORT_ON_TIME 	= (GEOMAGNETIC_SENSOR<<16) |0x0002,
	GEOMAGNETIC_EVENT_RAW_DATA_REPORT_ON_TIME 	= (GEOMAGNETIC_SENSOR<<16) |0x0004,
};
@endcode


<b>Gyro Data ID:</b>
@code
enum gyro_data_id {
	GYRO_BASE_DATA_SET = (GYROSCOPE_SENSOR<<16) | 0x0001,
};
@endcode


<b>Gyro Event Type:</b>
@code
enum gyro_evet_type {	
	GYROSCOPE_EVENT_RAW_DATA_REPORT_ON_TIME		= (GYROSCOPE_SENSOR<<16) |0x0001,
};
@endcode


<b>Light Data ID:</b>
@code
enum light_data_id {
	LIGHT_BASE_DATA_SET = (LIGHT_SENSOR<<16) | 0x0001,
};
@endcode


<b>Light Event Type:</b>
@code
enum light_evet_type {			
	LIGHT_EVENT_CHANGE_LEVEL		= (LIGHT_SENSOR<<16) |0x0001,
	LIGHT_EVENT_LEVEL_DATA_REPORT_ON_TIME   = (LIGHT_SENSOR<<16) |0x0002,
};
@endcode

	
<b>Proximity Data ID:</b>
@code
enum proxi_data_id {
	PROXIMITY_BASE_DATA_SET = (PROXIMITY_SENSOR<<16) | 0x0001,
};
@endcode


<b>Proximity Event Type:</b>
@code
enum proxi_evet_type {			
	PROXIMITY_EVENT_CHANGE_STATE		= (PROXIMITY_SENSOR<<16) |0x0001,
	PROXIMITY_EVENT_STATE_REPORT_ON_TIME	= (PROXIMITY_SENSOR<<16) |0x0002,
};
@endcode


<b>Proximity Change State:</b>
@code
enum proxi_change_state {
	PROXIMITY_STATE_FAR	 	= 0,
	PROXIMITY_STATE_NEAR		= 1,
};
@endcode



*/
/**
*@}
*/

/**
 * @defgroup SENSOR_UC Use Cases
 * @ingroup  SENSOR_FRAMEWORK_PG
*/
