#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <dirent.h>
#include <string.h>
#include <apm.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "acpi.h"

#define MAXITEM 8

int batt_count = 0;
/* Filenames of the battery info files for each system battery. */
char battery_info[MAXITEM][128];
/* Filenames of the battery status files for each system battery. */
char battery_status[MAXITEM][128];
/* Stores battery capacity, or 0 if the battery is absent. */
int battery_capacity[MAXITEM];

int ac_count = 0;
char ac_adapter_info[MAXITEM][128];
char ac_adapter_status[MAXITEM][128];

/* Read in an entire ACPI proc file (well, the first 1024 bytes anyway), and
 * return a statically allocated array containing it. */
inline char *get_acpi_file (const char *file) {
	int fd;
	int end;
	static char buf[1024];
	fd = open(file, O_RDONLY);
	if (fd == -1) return NULL;
	end = read(fd, buf, sizeof(buf));
	buf[end] = '\0';
	close(fd);
	return buf;
}

/* Given a buffer holding an acpi file, searches for the given key in it,
 * and returns the numeric value. 0 is returned on failure. */
inline int scan_acpi_num (const char *buf, const char *key) {
	char *ptr;
	int ret;
	if ((ptr = strstr(buf, key))) {
		if (sscanf(ptr + strlen(key), "%d", &ret) == 1)
			return ret;
	}
	return 0;
}

/* Given a buffer holding an acpi file, searches for the given key in it,
 * and returns its value in a statically allocated string. */
inline char *scan_acpi_value (const char *buf, const char *key) {
	char *ptr;
	static char ret[256];
	if ((ptr = strstr(buf, key))) {
		if (sscanf(ptr + strlen(key), "%s", ret) == 1)
			return ret;
	}
	return NULL;
}

/* Read an ACPI proc file, pull out the requested piece of information, and
 * return it (statically allocated string). Returns NULL on error, This is 
 * the slow, dumb way, fine for initialization or if only one value is needed
 * from a file, slow if called many times. */
char *get_acpi_value (const char *file, const char *key) {
	char *buf = get_acpi_file(file);
	return scan_acpi_value(buf, key);
}

/* Returns the last full capacity of a battery. */
int get_batt_capacity(int battery) {
	int cap;
	cap = atoi(get_acpi_value(battery_info[battery], "Last Full Capacity:"));
	/* This is ACPI's broken way of saying that there is no battery. */
	if (cap == 655350)
		return 0;
	return cap;
}

/* Find something (batteries, ac adpaters, etc), and set up a string array
 * to hold the paths to info and status files of the things found. Must be 
 * in /proc/acpi to call this. Returns the number of items found. */
int find_items (char *itemname, char infoarray[MAXITEM][128],
		                char statusarray[MAXITEM][128]) {
	DIR *dir;
	struct dirent *ent;
	int count = 0;
	
	dir = opendir(itemname);
	if (dir == NULL)
		return -1;

	while ((ent = readdir(dir))) {
		if (!strncmp(".", ent->d_name, 1) || 
		    !strncmp("..", ent->d_name, 2))
			continue;

		sprintf(infoarray[count], "%s/%s/%s", itemname, ent->d_name, "info");
		sprintf(statusarray[count], "%s/%s/%s", itemname, ent->d_name, "status");
		count++;
		if (count > MAXITEM)
			break;
	}

	closedir(dir);
	return count;
}

/* Find batteries, return the number, and set batt_count to it as well. */
int find_batteries(void) {
	int i;
	batt_count = find_items("battery", battery_info, battery_status);
	/* Read in the last charged capacity of the batteries. */
	for (i = 0; i < batt_count; i++)
		battery_capacity[i] = get_batt_capacity(i);
	return batt_count;
}

/* Find AC power adapters, return the number found, and set ac_count to it
 * as well. */
int find_ac_adapters(void) {
	ac_count = find_items("ac_adapter", ac_adapter_info, ac_adapter_status);
	return ac_count;
}

/* Returns true if the system is on ac power. Call find_ac_adapters first. */
int on_ac_power (void) {
	int i;
	for (i = 0; i < ac_count; i++) {
		if (strcmp("on-line", get_acpi_value(ac_adapter_status[i], "Status:")))
			return 1;
		else
			return 0;
	}
	return 0;
}

/* See if we have ACPI support and check version. Also find batteries and
 * ac power adapters. */
int acpi_supported (void) {
	char *version;

	if (chdir("/proc/acpi") == -1) {
		perror("chdir /proc/acpi");
		return 0;
	}
	
	version = get_acpi_value("info", "ACPI-CA Version:");
	if (version == NULL) {
		fprintf(stderr, "Unable to find ACPI version.\n");
		return 0;
	}
	if (atoi(version) < ACPI_VERSION) {
		fprintf(stderr, "ACPI subsystem %s too is old, consider upgrading to %i.\n",
				version, ACPI_VERSION);
		return 0;
	}

	if (! find_batteries()) {
		fprintf(stderr, "No ACPI batteries found!");
		return 0;
	}

	if (! find_ac_adapters()) {
		fprintf(stderr, "No ACPI power adapter found!");
	}

	return 1;
}

/* Read ACPI info on a given power adapter and battery, and fill the passed
 * apm_info struct. */
int acpi_read (int battery, apm_info *info) {
	char *buf, *state;
	char *rate_s;
	
	/* Internally it's zero indexed. */
	battery--;
	
	buf = get_acpi_file(battery_status[battery]);

	info->ac_line_status = 0;
	info->battery_flags = 0;
	info->using_minutes = 1;
	
	/* Work out if the battery is present, and what percentage of full
	 * it is and how much time is left. */
	if (strcmp(scan_acpi_value(buf, "Present:"), "yes") == 0) {
		int pcap, rate;
		
		pcap = scan_acpi_num(buf, "Remaining Capacity:");
		rate = scan_acpi_num(buf, "Present Rate:");

		if (rate) {
			/* time remaining = (current_capacity / discharge rate) */
			info->battery_time = (float) pcap / (float) rate * 60;
			if (info->battery_time <= 0)
				info->battery_time = 0;
		}
		else {
			rate_s = scan_acpi_value(buf, "Present Rate:");
			if (! rate_s) {
				/* Time remaining unknown. */
				info->battery_time = 0;
			}
			/* This is a hack for my picturebook. If
			 * the battery is not present, ACPI still
			 * says it is Present, but sets this to
			 * unknown. I don't know if this is the
			 * correct way to do it. */
			else if (strcmp(rate_s, "unknown") == 0) {
				goto NOBAT;
			}
		}

		if (battery_capacity[battery] == 0) {
			/* The battery was absent, and now is present.
			 * Well, it might be a different battery. So
			 * re-probe the battery. */
			battery_capacity[battery] = get_batt_capacity(battery);
		}
		
		if (pcap) {
			/* percentage = (current_capacity / last_full_capacity) * 100 */
			info->battery_percentage = (float) pcap / (float) battery_capacity[battery] * 100;
		}
		else {
			info->battery_percentage = -1;
		}

		state = scan_acpi_value(buf, "State:");
		if (state) {
			if (state[0] == 'd') { /* discharging */
				info->battery_status = BATTERY_STATUS_CHARGING;
			}
			else if (state[0] == 'c' && state[1] == 'h') { /* charging */
				info->battery_status = BATTERY_STATUS_CHARGING;
				info->ac_line_status = 1;
				info->battery_flags = info->battery_flags | BATTERY_FLAGS_CHARGING;

				/*   */
				info->battery_time = -1 * (float) (battery_capacity[battery] - pcap) / (float) rate * 60;
			}
			else if (state[0] == 'o') { /* ok */
				/* charged, on ac power */
				info->battery_status = BATTERY_STATUS_HIGH;
				info->ac_line_status = 1;
			}
			else if (state[0] == 'c') { /* not charging, so must be critical */
				info->battery_status = BATTERY_STATUS_CRITICAL;
			}
			else
				fprintf(stderr, "unknown battery state: %s\n", state);
		}
		else {
			/* Battery state unknown. */
			info->battery_status = BATTERY_STATUS_ABSENT;
		}
	}
	else {
NOBAT:
		/* No battery. */
		info->battery_percentage = 0;
		info->battery_time = 0;
		info->battery_status = BATTERY_STATUS_ABSENT;
		battery_capacity[battery] = 0;
		if (batt_count == 1) {
			/* Where else would the power come from, eh? ;-) */
			info->ac_line_status = 1;
		}
		else {
			/* Expensive ac power check. */
			info->ac_line_status = on_ac_power();
		}
	}
	
	return 0;
}
