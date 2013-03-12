#include <tet_api.h>
#include <sensor.h>

static void startup(void);
static void cleanup(void);

void (*tet_startup)(void) = startup;
void (*tet_cleanup)(void) = cleanup;

static void utc_SensorFW_sf_is_sensor_event_available_func_01(void);
static void utc_SensorFW_sf_is_sensor_event_available_func_02(void);

enum {
	POSITIVE_TC_IDX = 0x01,
	NEGATIVE_TC_IDX,
};

struct tet_testlist tet_testlist[] = {
	{ utc_SensorFW_sf_is_sensor_event_available_func_01, POSITIVE_TC_IDX },
	{ utc_SensorFW_sf_is_sensor_event_available_func_02, NEGATIVE_TC_IDX },
	{ NULL, 0},
};

static void startup(void)
{
}

static void cleanup(void)
{
}

/**
 * @brief Positive test case of sf_is_sensor_event_available()
 */
static void utc_SensorFW_sf_is_sensor_event_available_func_01(void)
{
	int r = 0;


   	r = sf_is_sensor_event_available(ACCELEROMETER_SENSOR, 0);

	if (r < 0) {
		tet_infoline("sf_is_sensor_event_available() failed in positive test case");
		tet_result(TET_FAIL);
		return;
	}
	tet_result(TET_PASS);
}

/**
 * @brief Negative test case of ug_init sf_is_sensor_event_available()
 */
static void utc_SensorFW_sf_is_sensor_event_available_func_02(void)
{
	int r = 0;


   	r = sf_is_sensor_event_available(UNKNOWN_SENSOR, 0);

	if (r > 0) {
		tet_infoline("sf_is_sensor_event_available() failed in negative test case");
		tet_result(TET_FAIL);
		return;
	}
	tet_result(TET_PASS);
}
