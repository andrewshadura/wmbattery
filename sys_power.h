/*
 * Support for the linux ACPI_SYS_POWER interface.
 * By Joey Hess <joey@kitenet.net>
 */

/* Define SYS_POWER_APM to get the sys_power_read function, which is
 * like apm_read. */
//#define SYS_POWER_APM 1

/* If you don't have sysfs on /sys, change this.. */
#define SYS_POWER "/sys/class/power_supply"

/* The number of items of each class supported. */
#define SYS_POWER_MAXITEM 8

int sys_power_supported (void);
#ifdef SYS_POWER_APM
int sys_power_read (int battery, apm_info *info);
#endif
char *get_sys_power_value (const char *dir, const char *file);
int get_sys_power_batt_capacity(int battery);

extern int sys_power_batt_count;
extern int sys_power_ac_count;
extern char sys_power_batt_dirs[SYS_POWER_MAXITEM][128];
extern char sys_power_ac_dirs[SYS_POWER_MAXITEM][128];
/* Stores battery capacity, or 0 if the battery is absent. */
extern int sys_power_batt_capacity[SYS_POWER_MAXITEM];
