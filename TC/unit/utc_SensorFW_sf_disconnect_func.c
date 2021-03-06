#include <tet_api.h>
#include <sensor.h>


int handle;


static void startup(void);
static void cleanup(void);

void (*tet_startup)(void) = startup;
void (*tet_cleanup)(void) = cleanup;

static void utc_SensorFW_sf_disconnect_func_01(void);
static void utc_SensorFW_sf_disconnect_func_02(void);

enum {
	POSITIVE_TC_IDX = 0x01,
	NEGATIVE_TC_IDX,
};

struct tet_testlist tet_testlist[] = {
	{ utc_SensorFW_sf_disconnect_func_01, POSITIVE_TC_IDX },
	{ utc_SensorFW_sf_disconnect_func_02, NEGATIVE_TC_IDX },
	{ NULL, 0},
};

static void startup(void)
{
	handle = sf_connect(ACCELEROMETER_SENSOR);
}

static void cleanup(void)
{
	sf_disconnect(handle);
}


/**
 * @brief Positive test case of sf_disconnect()
 */
static void utc_SensorFW_sf_disconnect_func_01(void)
{
	int r = 0;


   	r = sf_disconnect(handle);

	if (r != 0) {
		tet_infoline("sf_disconnect() failed in positive test case");
		tet_result(TET_FAIL);
		return;
	}
	tet_result(TET_PASS);
}

/**
 * @brief Negative test case of ug_init sf_disconnect()
 */
static void utc_SensorFW_sf_disconnect_func_02(void)
{
	int r = 0;


   	r = sf_disconnect(100);

	if (r == 0) {
		tet_infoline("sf_disconnect() failed in negative test case");
		tet_result(TET_FAIL);
		return;
	}
	tet_result(TET_PASS);
}
