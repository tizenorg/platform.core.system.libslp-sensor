#include <tet_api.h>
#include <sensor.h>
#include <stdlib.h>

int handle = 0;
sensor_data_t* values;

static void startup(void);
static void cleanup(void);

void (*tet_startup)(void) = startup;
void (*tet_cleanup)(void) = cleanup;

static void utc_SensorFW_sf_get_data_func_01(void);
static void utc_SensorFW_sf_get_data_func_02(void);

enum {
	POSITIVE_TC_IDX = 0x01,
	NEGATIVE_TC_IDX,
};

struct tet_testlist tet_testlist[] = {
	{ utc_SensorFW_sf_get_data_func_01, POSITIVE_TC_IDX },
	{ utc_SensorFW_sf_get_data_func_02, NEGATIVE_TC_IDX },
	{ NULL, 0},
};

static void startup(void)
{
	handle = sf_connect(ACCELEROMETER_SENSOR);
	sf_start(handle,0);
	
}

static void cleanup(void)
{
	sf_stop(handle);
	sf_disconnect(handle);
}

/**
 * @brief Positive test case of sf_get_data()
 */
static void utc_SensorFW_sf_get_data_func_01(void)
{
	int r = 0;

	values = (sensor_data_t*)malloc(sizeof(sensor_data_t));

   	r = sf_get_data(handle, ACCELEROMETER_BASE_DATA_SET, values);

	if (r < 0) {
		tet_infoline("sf_get_data() failed in positive test case");
		tet_result(TET_FAIL);
		return;
	}
	tet_result(TET_PASS);
}

/**
 * @brief Negative test case of ug_init sf_get_data()
 */
static void utc_SensorFW_sf_get_data_func_02(void)
{
	int r = 0;

//	values = (sensor_data_t*)malloc(sizeof(sensor_data_t));
	
	r = sf_get_data(300, NULL, values);

	printf("n r = %d\n", r);
	
	if (r > 0) {
		tet_infoline("sf_get_data() failed in negative test case");
		tet_result(TET_FAIL);
		return;
	}
	tet_result(TET_PASS);
}
