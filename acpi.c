/* 
 * A not-yet-general-purpose ACPI library, by Joey Hess <joey@kitenet.net>
 */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <dirent.h>
#include <string.h>
#ifdef ACPI_APM
#include "apm.h"
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "acpi.h"

#define PROC_ACPI "/proc/acpi"
#define ACPI_MAXITEM 8

int acpi_batt_count = 0;
/* Filenames of the battery info files for each system battery. */
char acpi_batt_info[ACPI_MAXITEM][128];
/* Filenames of the battery status files for each system battery. */
char acpi_batt_status[ACPI_MAXITEM][128];
/* Stores battery capacity, or 0 if the battery is absent. */
int acpi_batt_capacity[ACPI_MAXITEM];

int acpi_ac_count = 0;
char acpi_ac_adapter_info[ACPI_MAXITEM][128];
char acpi_ac_adapter_status[ACPI_MAXITEM][128];

/* These are the strings used in the ACPI shipped with the 2.4 kernels */
char *acpi_labels_old[] = {
	"info",
	"status",
	"battery",
	"ac_adapter",
	"on-line",
	"unknown",
	"Last Full Capacity:",
	"Present:",
	"Remaining Capacity:",
	"Present Rate:",
	"State:",
#if ACPI_THERMAL
	"thermal",
#endif
	NULL
};

/* These are the strings used in ACPI in the 2.5 kernels, circa version
 * 20020214 */
char *acpi_labels_20020214[] = {
	"info",
	"state",
	"battery",
	"ac_adapter",
	"on-line",
	"unknown",
	"last full capacity:",
	"present:",
	"remaining capacity:",
	"present rate:",
	"charging state:",
#if ACPI_THERMAL
	"thermal_zone",
#endif
	NULL
};

char **acpi_labels = NULL;

#if ACPI_THERMAL
int acpi_thermal_count = 0;
char acpi_thermal_info[ACPI_MAXITEM][128];
char acpi_thermal_status[ACPI_MAXITEM][128];
#endif

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
	if (! buf) return NULL;
	return scan_acpi_value(buf, key);
}

/* Returns the last full capacity of a battery. */
int get_acpi_batt_capacity(int battery) {
	int cap;
	char *caps=get_acpi_value(acpi_batt_info[battery], acpi_labels[label_last_full_capacity]);
	if (caps == NULL)
		cap=0; /* battery not present */
	else
		cap=atoi(caps);
	/* This is ACPI's broken way of saying that there is no battery. */
	if (cap == 655350)
		return 0;
	return cap;
}

/* Comparison function for qsort. */
int _acpi_compare_strings (const void *a, const void *b) {
	const char **pa = (const char **)a;
	const char **pb = (const char **)b;
	return strcasecmp((const char *)*pa, (const char *)*pb);
}

/* Find something (batteries, ac adpaters, etc), and set up a string array
 * to hold the paths to info and status files of the things found. Must be 
 * in /proc/acpi to call this. Returns the number of items found. */
int find_items (char *itemname, char infoarray[ACPI_MAXITEM][128],
		                char statusarray[ACPI_MAXITEM][128]) {
	DIR *dir;
	struct dirent *ent;
	int num_devices=0;
	int i;
	char **devices = malloc(ACPI_MAXITEM * sizeof(char *));
	
	dir = opendir(itemname);
	if (dir == NULL)
		return 0;
	while ((ent = readdir(dir))) {
		if (!strncmp(".", ent->d_name, 1) || 
		    !strncmp("..", ent->d_name, 2))
			continue;

		devices[num_devices]=strdup(ent->d_name);
		num_devices++;
		if (num_devices >= ACPI_MAXITEM)
			break;
	}
	closedir(dir);
	
	/* Sort, since readdir can return in any order. /proc/does
	 * sometimes list BATT2 before BATT1. */
	qsort(devices, num_devices, sizeof(char *), _acpi_compare_strings);

	for (i = 0; i < num_devices; i++) {
		sprintf(infoarray[i], "%s/%s/%s", itemname, devices[i],
			acpi_labels[label_info]);
		sprintf(statusarray[i], "%s/%s/%s", itemname, devices[i],
			acpi_labels[label_status]);
		free(devices[i]);
	}

	return num_devices;
}

/* Find batteries, return the number, and set acpi_batt_count to it as well. */
int find_batteries(void) {
	int i;
	acpi_batt_count = find_items(acpi_labels[label_battery], acpi_batt_info, acpi_batt_status);
	/* Read in the last charged capacity of the batteries. */
	for (i = 0; i < acpi_batt_count; i++)
		acpi_batt_capacity[i] = get_acpi_batt_capacity(i);
	return acpi_batt_count;
}

/* Find AC power adapters, return the number found, and set acpi_ac_count to it
 * as well. */
int find_ac_adapters(void) {
	acpi_ac_count = find_items(acpi_labels[label_ac_adapter], acpi_ac_adapter_info, acpi_ac_adapter_status);
	return acpi_ac_count;
}

#if ACPI_THERMAL
/* Find thermal information sources, return the number found, and set
 * thermal_count to it as well. */
int find_thermal(void) {
	acpi_thermal_count = find_items(acpi_labels[label_thermal], acpi_thermal_info, acpi_thermal_status);
	return acpi_thermal_count;
}
#endif

/* Returns true if the system is on ac power. Call find_ac_adapters first. */
int on_ac_power (void) {
	int i;
	for (i = 0; i < acpi_ac_count; i++) {
		if (strcmp(acpi_labels[label_online], get_acpi_value(acpi_ac_adapter_status[i], "Status:")) == 0)
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
	int num;

	if (chdir(PROC_ACPI) == -1) {
		return 0;
	}
	
	version = get_acpi_value("info", "ACPI-CA Version:");
	if (version == NULL) {
		/* 2.5 kernel acpi */
		version = get_acpi_value("info", "version:");
	}
	if (version == NULL) {
		return 0;
	}
	num = atoi(version);
	if (num < ACPI_VERSION) {
		fprintf(stderr, "ACPI subsystem %s too is old, consider upgrading to %i.\n",
				version, ACPI_VERSION);
		return 0;
	}
	else if (num >= 20020214) {
		acpi_labels = acpi_labels_20020214;
	}
	else {
		acpi_labels = acpi_labels_old;
	}
	
	find_batteries();
	find_ac_adapters();
#if ACPI_THERMAL
	find_thermal();
#endif
	
	return 1;
}

#ifdef ACPI_APM
/* Read ACPI info on a given power adapter and battery, and fill the passed
 * apm_info struct. */
int acpi_read (int battery, apm_info *info) {
	char *buf, *state;
	
	/* Internally it's zero indexed. */
	battery--;
	
	buf = get_acpi_file(acpi_batt_status[battery]);
	if (buf == NULL) {
		fprintf(stderr, "unable to read %s\n", acpi_batt_status[battery]);
		perror("read");
		exit(1);
	}

	info->ac_line_status = 0;
	info->battery_flags = 0;
	info->using_minutes = 1;
	
	/* Work out if the battery is present, and what percentage of full
	 * it is and how much time is left. */
	if (strcmp(scan_acpi_value(buf, acpi_labels[label_present]), "yes") == 0) {
		int pcap = scan_acpi_num(buf, acpi_labels[label_remaining_capacity]);
		int rate = scan_acpi_num(buf, acpi_labels[label_present_rate]);

		if (rate) {
			/* time remaining = (current_capacity / discharge rate) */
			info->battery_time = (float) pcap / (float) rate * 60;
		}
		else {
			char *rate_s = scan_acpi_value(buf, acpi_labels[label_present_rate]);
			if (! rate_s) {
				/* Time remaining unknown. */
				info->battery_time = 0;
			}
			/* This is a hack for my picturebook. If
			 * the battery is not present, ACPI still
			 * says it is Present, but sets this to
			 * unknown. I don't know if this is the
			 * correct way to do it. */
			else if (strcmp(rate_s, acpi_labels[label_unknown]) == 0) {
				goto NOBATT;
			}
			else {
				/* a zero in the file; time unknown so use
				 * a negative one to indicate this */
				info->battery_time = -1;
			}
		}

		state = scan_acpi_value(buf, acpi_labels[label_charging_state]);
		if (state) {
			if (state[0] == 'd') { /* discharging */
				info->battery_status = BATTERY_STATUS_CHARGING;
			}
			else if (state[0] == 'c' && state[1] == 'h') { /* charging */
				info->battery_status = BATTERY_STATUS_CHARGING;
				info->ac_line_status = 1;
				info->battery_flags = info->battery_flags | BATTERY_FLAGS_CHARGING;
				info->battery_time = -1 * (float) (acpi_batt_capacity[battery] - pcap) / (float) rate * 60;
				if (abs(info->battery_time) < 0.5)
					info->battery_time = 0;
			}
			else if (state[0] == 'o') { /* ok */
				/* charged, on ac power */
				info->battery_status = BATTERY_STATUS_HIGH;
				info->ac_line_status = 1;
			}
			else if (state[0] == 'c') { /* not charging, so must be critical */
				info->battery_status = BATTERY_STATUS_CRITICAL;
			}
			else {
				fprintf(stderr, "unknown battery state: %s\n", state);
			}
		}
		else {
			/* Battery state unknown. */
			info->battery_status = BATTERY_STATUS_ABSENT;
		}
		
		if (acpi_batt_capacity[battery] == 0) {
			/* The battery was absent, and now is present.
			 * Well, it might be a different battery. So
			 * re-probe the battery. */
			/* NOTE that this invalidates buf. No accesses of
			 * buf below this point! */
			acpi_batt_capacity[battery] = get_acpi_batt_capacity(battery);
		}
		
		if (pcap) {
			/* percentage = (current_capacity / last_full_capacity) * 100 */
			info->battery_percentage = (float) pcap / (float) acpi_batt_capacity[battery] * 100;
		}
		else {
			info->battery_percentage = -1;
		}

	}
	else {
NOBATT:
		info->battery_percentage = 0;
		info->battery_time = 0;
		info->battery_status = BATTERY_STATUS_ABSENT;
		acpi_batt_capacity[battery] = 0;
		if (acpi_batt_count == 1) {
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
#endif
