#include <tet_api.h>
#include <sensor.h>

int handle = 0;

void my_callback_func(unsigned int event_type, sensor_event_data_t *event , void *data)
{
}

static void startup(void);
static void cleanup(void);

void (*tet_startup)(void) = startup;
void (*tet_cleanup)(void) = cleanup;

static void utc_SensorFW_sf_unregister_event_func_01(void);
static void utc_SensorFW_sf_unregister_event_func_02(void);

enum {
	POSITIVE_TC_IDX = 0x01,
	NEGATIVE_TC_IDX,
};

struct tet_testlist tet_testlist[] = {
	{ utc_SensorFW_sf_unregister_event_func_01, POSITIVE_TC_IDX },
	{ utc_SensorFW_sf_unregister_event_func_02, NEGATIVE_TC_IDX },
	{ NULL, 0},
};

static void startup(void)
{
        handle = sf_connect(ACCELEROMETER_SENSOR);
        sf_register_event(handle, ACCELEROMETER_EVENT_ROTATION_CHECK, NULL,  my_callback_func,NULL);

}

static void cleanup(void)
{
        sf_disconnect(handle);
}
/**
 * @brief Positive test case of sf_unregister_event()
 */
static void utc_SensorFW_sf_unregister_event_func_01(void)
{
	int r = 0;


	r = sf_unregister_event(handle, ACCELEROMETER_EVENT_ROTATION_CHECK);

	if (r < 0) {
		tet_infoline("sf_unregister_event() failed in positive test case");
		tet_result(TET_FAIL);
		return;
	}
	tet_result(TET_PASS);
}

/**
 * @brief Negative test case of ug_init sf_unregister_event()
 */
static void utc_SensorFW_sf_unregister_event_func_02(void)
{
	int r = 0;

	r = sf_unregister_event(handle, NULL);

	if (r > 0) {
		tet_infoline("sf_unregister_event() failed in negative test case");
		tet_result(TET_FAIL);
		return;
	}
	tet_result(TET_PASS);
}
