#include <tet_api.h>
#include <sensor.h>

int handle = 0;

static void startup(void);
static void cleanup(void);

void (*tet_startup)(void) = startup;
void (*tet_cleanup)(void) = cleanup;

static void utc_SensorFW_sf_start_func_01(void);
static void utc_SensorFW_sf_start_func_02(void);

enum {
	POSITIVE_TC_IDX = 0x01,
	NEGATIVE_TC_IDX,
};

struct tet_testlist tet_testlist[] = {
	{ utc_SensorFW_sf_start_func_01, POSITIVE_TC_IDX },
	{ utc_SensorFW_sf_start_func_02, NEGATIVE_TC_IDX },
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
 * @brief Positive test case of sf_start()
 */
static void utc_SensorFW_sf_start_func_01(void)
{
	int r = 0;


   	r = sf_start(handle, 0);

	if (r < 0) {
		tet_infoline("sf_start() failed in positive test case");
		tet_result(TET_FAIL);
		return;
	}
	tet_result(TET_PASS);
}

/**
 * @brief Negative test case of ug_init sf_start()
 */
static void utc_SensorFW_sf_start_func_02(void)
{
	int r = 0;

	r = sf_start(-1, 0);
	
	if (r > 0) {
		tet_infoline("sf_start() failed in negative test case");
		tet_result(TET_FAIL);
		return;
	}
	tet_result(TET_PASS);
}
