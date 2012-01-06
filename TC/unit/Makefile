CC ?= gcc

TARGETS = 	utc_SensorFW_sf_get_properties_func \
		utc_SensorFW_sf_is_sensor_event_available_func \
		utc_SensorFW_sf_connect_func \
		utc_SensorFW_sf_disconnect_func \
		utc_SensorFW_sf_start_func \
		utc_SensorFW_sf_stop_func \
		utc_SensorFW_sf_register_event_func \
		utc_SensorFW_sf_unregister_event_func \
		utc_SensorFW_sf_get_data_func \
		utc_SensorFW_sf_check_rotation_func \
		utc_SensorFW_sf_read_raw_data_func

PKGS = sf_common sensor

LDFLAGS = `pkg-config --libs $(PKGS)`
LDFLAGS += $(TET_ROOT)/lib/tet3/tcm_s.o
LDFLAGS += -L$(TET_ROOT)/lib/tet3 -ltcm_s
LDFLAGS += -L$(TET_ROOT)/lib/tet3 -lapi_s

CFLAGS = -I. `pkg-config --cflags $(PKGS)`
CFLAGS += -I$(TET_ROOT)/inc/tet3
CFLAGS += -Wall

all: $(TARGETS)

$(TARGETS): %: %.c
	$(CC) -o $@ $< $(CFLAGS) $(LDFLAGS)

clean:
	rm -f $(TARGETS)
