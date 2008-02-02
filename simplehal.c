/* Not particularly good interface to hal, for programs that used to use
 * apm.
 */

#include <apm.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libhal.h>

static LibHalContext *hal_ctx;

char *udi_base = "/org/freedesktop/Hal/devices";

char *gen_udi (const char *device) {
	static char ret[256];
	sprintf(ret, "%s/%s", udi_base, device);
	return ret;
}

signed int get_hal_int (const char *device, const char *key) {
	int ret;
	DBusError error;

	dbus_error_init(&error);
	ret = libhal_device_get_property_int (hal_ctx, gen_udi(device), key, &error);
	
	if (dbus_error_is_set (&error)) {
		fprintf(stderr, "error: libhal_device_get_property_int: %s: %s\n",
			 error.name, error.message);
		dbus_error_free (&error);
		return -1;
	}
	return ret;
}

signed int get_hal_bool (const char *device, const char *key) {
	int ret;
	DBusError error;

	dbus_error_init(&error);
	ret = libhal_device_get_property_bool (hal_ctx, gen_udi(device), key, &error);
	
	if (dbus_error_is_set (&error)) {
		fprintf(stderr, "error: libhal_device_get_property_int: %s: %s\n",
			 error.name, error.message);
		dbus_error_free (&error);
		return -1;
	}
	return ret;
}

int simplehal_supported (void) {
	DBusError error;
	DBusConnection *conn;

	dbus_error_init(&error);
	conn = dbus_bus_get(DBUS_BUS_SYSTEM, &error);
	if (conn == NULL) {
		fprintf(stderr, "error: dbus_bus_get: %s: %s\n",
			 error.name, error.message);
		LIBHAL_FREE_DBUS_ERROR(&error);
		return 0;
	}
	if ((hal_ctx = libhal_ctx_new()) == NULL) {
		fprintf(stderr, "error: libhal_ctx_new\n");
		return 0;
	}
	if (!libhal_ctx_set_dbus_connection (hal_ctx, conn)) {
		fprintf(stderr, "error: libhal_ctx_set_dbus_connection: %s: %s\n",
			 error.name, error.message);
		return 0;
	}
	if (!libhal_ctx_init(hal_ctx, &error)) {
		if (dbus_error_is_set(&error)) {
			fprintf(stderr, "error: libhal_ctx_init: %s: %s\n", error.name, error.message);
			dbus_error_free(&error);
		}
		fprintf(stderr, "Could not initialise connection to hald.\n"
				 "Normally this means the HAL daemon (hald) is not running or not ready.\n");
		return 0;
	}

	return 1;
}

/* Fill the passed apm_info struct. */
int simplehal_read (int battery, apm_info *info) {
	char device[255];

	info->battery_flags = 0;
	info->using_minutes = 0;
	info->ac_line_status = (get_hal_bool("computer_power_supply", "ac_adapter.present") == 1);

	/* XXX There are probably better ways to enumerate batteries... */
	sprintf(device, "computer_power_supply_%i", battery - 1);

	if (get_hal_bool(device, "battery.present") != 1) {
		info->battery_percentage = 0;
		info->battery_time = 0;
		info->battery_status = BATTERY_STATUS_ABSENT;
		return 0;
	}
	
	/* remaining_time and charge_level.percentage are not a mandatory
	 * keys, so if not present, -1 will be returned */
	info->battery_time = get_hal_int(device, "battery.remaining_time");
	info->battery_percentage = get_hal_int(device, "battery.charge_level.percentage");
	if (get_hal_bool(device, "battery.rechargeable.is_discharging") == 1) {
		info->battery_status = BATTERY_STATUS_CHARGING;
		/* charge_level.warning and charge_level.low are not
		 * required to be available; this is good enough */
		if (info->battery_percentage < 1) {
			info->battery_status = BATTERY_STATUS_CRITICAL;
		}
		else if (info->battery_percentage < 10) {
			info->battery_status = BATTERY_STATUS_LOW;
		}
	}
	else if (get_hal_bool(device, "battery.rechargeable.is_charging") == 1) {
		info->battery_status = BATTERY_STATUS_CHARGING;
		info->battery_flags = info->battery_flags | BATTERY_FLAGS_CHARGING;
	}
	else if (info->ac_line_status) {
		/* Must be fully charged. */
		info->battery_status = BATTERY_STATUS_HIGH;
	}
	else {
		fprintf(stderr, "unknown battery state\n");
	}

	return 0;
}
