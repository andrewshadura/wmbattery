#define DELAY 1 /* Delay between refreshes (in seconds) */

typedef struct {
	Pixmap pixmap;
	Pixmap mask;
  	XpmAttributes attributes;
} XpmIcon;

typedef struct {
	const char driver_version[10];
	int apm_version_major;
   	int apm_version_minor;
   	int apm_flags;
   	int ac_line_status;
   	int battery_status;
   	int battery_flags;
   	int battery_percentage;
   	int battery_time;
   	int using_minutes;
} apm_info;

typedef struct image_info_type {
  	const char* filename;
  	const int width;
  	const int height;
  	const int x;
  	const int y;
  	const int charwidth;
} image_info_type;

#ifndef APM_32_BIT_SUPPORT
#define APM_32_BIT_SUPPORT      0x0002
#endif

/* Assign reference numbers to all images that are loaded. */
#define SMALLFONT 0
#define BIGFONT 1
#define BATTERY_HIGH 2
#define BATTERY_MEDIUM 3
#define BATTERY_LOW 4
#define BATTERY_NONE 5
#define BATTERY_BLINK 6
#define UNPLUGGED 7
#define PLUGGED 8
#define NOCHARGING 9
#define CHARGING 10
#define DIAL_BRIGHT 11
#define DIAL_DIM 12
#define FACE 13

#define NUM_IMAGES 14

/*
 * An array of the filenames of all images to load (minus .xpm extention),
 * plus the size of the image, where to draw it on the icon, etc
 */
static struct image_info_type image_info[] = {
  {"smallfont",7,67,0,45,6},
  {"bigfont",9,73,0,23,7},
  {"battery_high",25,13,33,42,0},
  {"battery_medium",25,13,33,42,0},
  {"battery_low",25,13,33,42,0},
  {"battery_none",25,13,33,42,0},
  {"battery_blink",25,13,33,42,0},
  {"unplugged",10,8,6,45,0},
  {"plugged",10,8,6,45,0},
  {"nocharging",15,9,17,43,0},
  {"charging",15,9,17,43,0},
  {"dial_bright",56,31,4,4,0},
  {"dial_dim",56,31,4,4,0},
  {"face",64,64,0,0,0},
};

#define DIAL_MULTIPLIER 0.56

/* Locations of letters in the percent remaining display. */
#define HUNDREDS_OFFSET 35
#define TENS_OFFSET 37
#define ONES_OFFSET 43
#define PERCENT_OFFSET 49

/* Locations of letters in the time remaining display. */
#define HOURS_TENS_OFFSET 15
#define HOURS_ONES_OFFSET 23
#define COLON_OFFSET 30
#define MINUTES_TENS_OFFSET 34
#define MINUTES_ONES_OFFSET 41
