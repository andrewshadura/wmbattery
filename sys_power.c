/* 
 * Support for the linux ACPI_SYS_POWER interface.
 * By Joey Hess <joey@kitenet.net>
 */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <dirent.h>
#include <string.h>
#ifdef SYS_POWER_APM
#include "apm.h"
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "sys_power.h"

int sys_power_batt_count = 0;
int sys_power_ac_count = 0;
char sys_power_batt_dirs[SYS_POWER_MAXITEM][128];
char sys_power_ac_dirs[SYS_POWER_MAXITEM][128];
/* Stores battery capacity, or 0 if the battery is absent. */
int sys_power_batt_capacity[SYS_POWER_MAXITEM];

/* Read a single value from a sys file from the specified directory (well,
 * the first 64 bytes anyway), and return a statically allocated array
 * containing it. The newline, if any, is stripped. If the file is not
 * present, "0" is returned. */
char *get_sys_power_value (const char *dir, const char *file) {
	int fd;
	int end;
	static char buf[64];
	char fn[128];
	snprintf(fn, 127, "%s/%s/%s", SYS_POWER, dir, file);
	fd = open(fn, O_RDONLY);
	if (fd == -1) return "0";
	end = read(fd, buf, sizeof(buf));
	close(fd);
	buf[end-1] = '\0';
	if (buf[end-2] == '\n')
		buf[end-2] = '\0';
	return buf;
}

/* Returns the maximum capacity of a battery.
 *
 * Note that this returns the highest possible capacity for the battery,
 * even if it can no longer charge that fully. So normally it uses the
 * design capacity. While the last full capacity of the battery should
 * never exceed the design capacity, some silly hardware might report
 * that it does. So if the last full capacity is greater, it will be
 * returned.
 */
int get_sys_power_batt_capacity(int battery) {
	int dcap=atoi(get_sys_power_value(sys_power_batt_dirs[battery], "charge_full_design"));
	int lcap=atoi(get_sys_power_value(sys_power_batt_dirs[battery], "charge_full"));
	
	if (lcap > dcap)
		return lcap;
	else
		return dcap;
}

/* Comparison function for qsort. */
int _sys_power_compare_strings (const void *a, const void *b) {
	const char **pa = (const char **)a;
	const char **pb = (const char **)b;
	return strcasecmp((const char *)*pa, (const char *)*pb);
}

/* Find something (batteries, ac adpaters, etc), and set up a string array
 * to hold the paths to the things found.
 * Returns the number of items found. */
int find_sys_power_items (char *type, char dirarray[SYS_POWER_MAXITEM][128]) {
	DIR *dir;
	struct dirent *ent;
	int num_devices=0;
	
	dir = opendir(SYS_POWER);
	if (dir == NULL)
		return 0;
	while ((ent = readdir(dir))) {
		if (!strcmp(".", ent->d_name) || 
		    !strcmp("..", ent->d_name))
			continue;

		if (strcmp(get_sys_power_value(ent->d_name, "type"), type) == 0) {
			sprintf(dirarray[num_devices], "%s", ent->d_name);
			num_devices++;
			if (num_devices >= SYS_POWER_MAXITEM)
				break;
		}
	}
	closedir(dir);
	
	/* Sort, since readdir can return in any order. */
	qsort(dirarray, num_devices, sizeof(char *), _sys_power_compare_strings);

	return num_devices;
}

/* Find batteries, return the number, and set sys_power_batt_count to it as
 * well. */
int find_sys_power_batteries(void) {
	int i;
	sys_power_batt_count = find_sys_power_items("Battery", sys_power_batt_dirs);
	for (i = 0; i < sys_power_batt_count; i++)
		sys_power_batt_capacity[i] = get_sys_power_batt_capacity(i);
	return sys_power_batt_count;
}

/* Find AC power adapters, return the number found, and set 
 * sys_power_ac_count to it as well. */
int find_sys_power_ac_adapters(void) {
	sys_power_ac_count = find_sys_power_items("Mains", sys_power_ac_dirs);
	return sys_power_ac_count;
}

/* Returns true if the system is on ac power. Call find_sys_power_ac_adapters
 * first. */
int sys_power_on_ac (void) {
	int i;
	for (i = 0; i < sys_power_ac_count; i++) {
		if (strcmp(get_sys_power_value(sys_power_ac_dirs[i], "online"), "0") != 0)
			return 1;
		else
			return 0;
	}
	return 0;
}

/* See if we have ACPI_SYS_POWER support. Also find batteries and ac power
 * adapters. */
int sys_power_supported (void) {
	DIR *dir;

	if (!(dir = opendir(SYS_POWER))) {
		return 0;
	}
	closedir(dir);
	
	find_sys_power_batteries();
	find_sys_power_ac_adapters();

	return 1;
}

#ifdef SYS_POWER_APM
/* Read ACPI info on a given battery, and fill the passed apm_info struct. */
int sys_power_read (int battery, apm_info *info) {
	char *status;

	if (sys_power_batt_count == 0) {
		info->battery_percentage = 0;
		info->battery_time = 0;
		info->battery_status = BATTERY_STATUS_ABSENT;
		sys_power_batt_capacity[battery] = 0;
		/* Where else would the power come from, eh? ;-) */
		info->ac_line_status = 1;
		return 0;
	}
	
	/* Internally it's zero indexed. */
	battery--;
	
	info->ac_line_status = 0;
	info->battery_flags = 0;
	info->using_minutes = 1;
	
	/* Work out if the battery is present, and what percentage of full
	 * it is and how much time is left. */
	if (strcmp(get_sys_power_value(sys_power_batt_dirs[battery], "present"), "1") == 0) {
		int charge = atoi(get_sys_power_value(sys_power_batt_dirs[battery], "charge_now"));
		int current = atoi(get_sys_power_value(sys_power_batt_dirs[battery], "current_now"));

		if (current) {
			/* time remaining = (charge / current) */
			info->battery_time = (float) charge / (float) current * 60;
		}
		else {
			/* a zero or unknown in the file; time 
			 * unknown so use a negative one to
			 * indicate this */
			info->battery_time = -1;
		}
		
		if (sys_power_batt_capacity[battery] == 0) {
			/* The battery was absent, and now is present.
			 * Well, it might be a different battery. So
			 * re-probe the battery. */
			sys_power_batt_capacity[battery] = get_sys_power_batt_capacity(battery);
		}
		else if (current > sys_power_batt_capacity[battery]) {
			/* Battery is somehow charged to greater than max
			 * capacity. Rescan for a new max capacity. */
			sys_power_batt_capacity[battery] = get_sys_power_batt_capacity(battery);
		}

		status = get_sys_power_value(sys_power_batt_dirs[battery], "status");
		if (strcmp(status, "Discharging") == 0) {
			info->battery_status = BATTERY_STATUS_CHARGING;
			/* Explicit power check used here
			 * because AC power might be on even if a
			 * battery is discharging in some cases. */
			info->ac_line_status = sys_power_on_ac();
		}
		else if (strcmp(status, "Charging") == 0) {
			info->battery_status = BATTERY_STATUS_CHARGING;
			info->ac_line_status = 1;
			info->battery_flags = info->battery_flags | BATTERY_FLAGS_CHARGING;
			if (current)
				info->battery_time = -1 * (float) (sys_power_batt_capacity[battery] - charge) / (float) current * 60;
			else
				info->battery_time = 0;
			if (abs(info->battery_time) < 0.5)
				info->battery_time = 0;
		}
		else {
			fprintf(stderr, "unknown battery status: %s\n", status);
			info->battery_status = BATTERY_STATUS_ABSENT;
		}
		
		if (current && sys_power_batt_capacity[battery]) {
			/* percentage = (current_capacity / max capacity) * 100 */
			info->battery_percentage = (float) current / (float) sys_power_batt_capacity[battery] * 100;
		}
		else {
			info->battery_percentage = -1;
		}

	}
	else {
		info->battery_percentage = 0;
		info->battery_time = 0;
		info->battery_status = BATTERY_STATUS_ABSENT;
		sys_power_batt_capacity[battery] = 0;
		if (sys_power_batt_count == 0) {
			/* Where else would the power come from, eh? ;-) */
			info->ac_line_status = 1;
		}
		else {
			info->ac_line_status = sys_power_supported();
		}
	}
	
	return 0;
}
#endif
