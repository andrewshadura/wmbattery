int acpi_supported (void);
int acpi_read (int battery, apm_info *info);
extern int batt_count;

/* The lowest version of ACPI proc files supported. */
#define ACPI_VERSION 20011018

