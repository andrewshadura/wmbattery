#include "config.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <X11/xpm.h>
#include <X11/extensions/shape.h>
#include <stdarg.h>
#include <signal.h>

#ifdef HAVE_SYS_FILE_H
#include <sys/file.h>
#endif

#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif

#ifdef HAVE_MACHINE_APM_BIOS_H	/* for FreeBSD */
#include <machine/apm_bios.h>
#endif

#ifdef HAVE_I386_APMVAR_H	/* for NetBSD and OpenBSD */
#include <i386/apmvar.h>
#endif

#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif

#include "wmbattery.h"
#include "mask.xbm"

Pixmap images[NUM_IMAGES];
Window root, iconwin, win;
int screen;
XpmIcon icon;
Display *display;
GC NormalGC;
int pos[2] = {0, 0};

#ifdef HAVE__DEV_APM
#define APM_STATUS_FILE "/dev/apm"
#else
#define APM_STATUS_FILE "/proc/apm"
#endif

char *apm_status_file = APM_STATUS_FILE;

void error(const char *fmt, ...) {
  	va_list arglist;
  
	va_start(arglist, fmt);
	fprintf(stderr, "Error: ");
	vfprintf(stderr, fmt, arglist);
	fprintf(stderr, "\n");
	va_end(arglist);
  
  	exit(1);
}

#if defined (HAVE_MACHINE_APM_BIOS_H) || defined (HAVE_I386_APMVAR_H) /* BSD */

int apm_read(apm_info *i) {
	int fd;
#ifdef HAVE_MACHINE_APM_BIOS_H /* FreeBSD */
	unsigned long request = APMIO_GETINFO;
	struct apm_info info;
#else /* NetBSD or OpenBSD */
	unsigned long request= APM_IOC_GETPOWER;
	struct apm_power_info info;
#endif

	if ((fd = open(apm_status_file, O_RDONLY)) == -1) {
    		return 0;
	}
	if (ioctl(fd, request, &info) == -1) {
		return 0;
	}
	close(fd);

#ifdef HAVE_MACHINE_APM_BIOS_H /* FreeBSD */
	i->ac_line_status = info.ai_acline;
	i->battery_status = info.ai_batt_stat;
	i->battery_flags = (info.ai_batt_stat == 3) ? 8: 0;
	i->battery_percentage = info.ai_batt_life;
	i->battery_time = info.ai_batt_time;
	i->using_minutes = 0;
#else /* NetBSD or OpenBSD */
	i->ac_line_status = info.ac_state;
	i->battery_status = info.battery_state;
	i->battery_flags = (info.battery_state == 3) ? 8: 0;
	i->battery_percentage = info.battery_life;
	i->battery_time = info.minutes_left;
	i->using_minutes = 1;
#endif
	
	return 1;
}

#else /* Linux */

int apm_read(apm_info *i) {
	FILE *str;
  	char units[10];
	char buffer[100];

	if (!(str = fopen(apm_status_file, "r")))
    		return 0;
	fgets(buffer, sizeof(buffer) - 1, str);
	buffer[sizeof(buffer) - 1] = '\0';
	sscanf(buffer, "%s %d.%d %x %x %x %x %d%% %d %s\n",
	       (char *)i->driver_version,
	       &i->apm_version_major,
	       &i->apm_version_minor,
	       &i->apm_flags,
	       &i->ac_line_status,
	       &i->battery_status,
		       &i->battery_flags,
	       &i->battery_percentage,
	       &i->battery_time,
	       units);
	i->using_minutes = !strncmp(units, "min", 3) ? 1 : 0;
	if (i->driver_version[0] == 'B') { /* old style.  argh. */
		strcpy((char *)i->driver_version, "pre-0.7");
		i->apm_version_major  = 0;
		i->apm_version_minor  = 0;
		i->apm_flags          = 0;
		i->ac_line_status     = 0xff;
		i->battery_status     = 0xff;
		i->battery_flags      = 0xff;
		i->battery_percentage = -1;
		i->battery_time       = -1;
		i->using_minutes      = 1;
		sscanf(buffer, "BIOS version: %d.%d",
			&i->apm_version_major, &i->apm_version_minor);
		fgets(buffer, sizeof(buffer) - 1, str);
		sscanf(buffer, "Flags: 0x%02x", &i->apm_flags);
		if (i->apm_flags & APM_32_BIT_SUPPORT) {
			fgets(buffer, sizeof(buffer) - 1, str);
			fgets(buffer, sizeof(buffer) - 1, str);
			if (buffer[0] != 'P') {
				if (!strncmp(buffer+4, "off line", 8))
					i->ac_line_status = 0;
				else if (!strncmp(buffer+4, "on line", 7))
	  				i->ac_line_status = 1;
				else if (!strncmp(buffer+4, "on back", 7))
	  				i->ac_line_status = 2;
				fgets(buffer, sizeof(buffer) - 1, str);
				if (!strncmp(buffer+16, "high", 4))
	  				i->battery_status = 0;
				else if (!strncmp(buffer+16, "low", 3))
	  				i->battery_status = 1;
				else if (!strncmp(buffer+16, "crit", 4))
	  				i->battery_status = 2;
				else if (!strncmp(buffer+16, "charg", 5))
	  				i->battery_status = 3;
				fgets(buffer, sizeof(buffer) - 1, str);
				if (strncmp(buffer+14, "unknown", 7))
	  				i->battery_percentage = atoi(buffer + 14);
				if (i->apm_version_major >= 1 && i->apm_version_minor >= 1) {
	  				fgets(buffer, sizeof(buffer) - 1, str);
	  				sscanf(buffer, "Battery flag: 0x%02x", &i->battery_flags);
	  				fgets(buffer, sizeof(buffer) - 1, str);
	  				if (strncmp(buffer+14, "unknown", 7))
	    					i->battery_time = atoi(buffer + 14);
				}
      			}
    		}
	}

       	/*
	 * Fix possible kernel bug -- percentage
         * set to 0xff (==255) instead of -1.
	 */
  	if (i->battery_percentage > 100)
    		i->battery_percentage = -1;
  
  	fclose(str);
  	return 1;
}

#endif /* linux */

int apm_change(apm_info *cur) {
	static int ac_line_status = 0, battery_status = 0, battery_flags = 0,
		battery_percentage = 0, battery_time = 0, using_minutes = 0;

	int i = cur->ac_line_status     == ac_line_status     &&
		cur->battery_status     == battery_status     &&
		cur->battery_flags      == battery_flags      &&
		cur->battery_percentage == battery_percentage &&
		cur->battery_time       == battery_time       &&
		cur->using_minutes      == using_minutes;

	ac_line_status = cur->ac_line_status;
	battery_status = cur->battery_status;
	battery_flags = cur->battery_flags;
	battery_percentage = cur->battery_percentage;
	battery_time = cur->battery_time;
	using_minutes = cur->using_minutes;

	return i;
}

int apm_exists() {
	apm_info i;
  
        if (access(apm_status_file, R_OK))
        	return 0;
	return apm_read(&i);
}

/* Load up the images this program uses. */
void load_images() {
  	int x;
	char fn[128]; /* enough? */

  	for(x=0; x < NUM_IMAGES; x++) {
         	sprintf(fn, "%s/%s.xpm", ICONDIR, image_info[x].filename);
                if (XpmReadFileToPixmap(display, root, fn, &images[x], NULL, NULL)) {
		  	/* Check in current direcotry for fallback. */
		  	sprintf(fn, "%s.xpm", image_info[x].filename);
		  	if (XpmReadFileToPixmap(display, root, fn, &images[x], NULL, NULL)) {
			 	error("Failed to load %s\n",fn);
		}
	}
    }
}

/* Returns the display to run on (or NULL for default). */
char *parse_commandline(int argc, char *argv[]) {
	int c=0;
	char *ret=NULL;
        char *s;
	extern char *optarg;
	
  	while (c != -1) {
  		c=getopt(argc, argv, "hd:g:f:");
		switch (c) {
		  case 'h':
			printf("\nUsage: wmbattery [options]\n");
              		printf("\t-d <display>\tselects target display\n");
               		printf("\t-h\t\tdisplay this help\n");
                        printf("\t-g +x+y\t\tposition of the window\n");
			printf("\t-f file\t\tapm status file to use instead of " APM_STATUS_FILE "\n\n");
               		exit(0);
		 	break;
		  case 'd':
		  	ret=strdup(optarg);
                        break;
  		  case 'g':
                        s = strtok(optarg, "+");
                        if (s) {
                          pos[0]=atoi(s);
                          if ((s = strtok(NULL, "+")) != NULL) {
                            pos[1]=atoi(s);
                          }
                          else {
                            pos[0]=0;
                          }
                        }
                        break;
		  case 'f':
			apm_status_file = strdup(optarg);
			break;
      		}
    	}
  
  	return ret;
}

/* Sets up the window and icon and all the nasty X stuff. */
void make_window(char *display_name, int argc, char *argv[]) {
	XClassHint classhint;
	char *wname = argv[0];
  	XTextProperty name;
  	XGCValues gcv;
  	int dummy=0, borderwidth = 1;
	XSizeHints sizehints;
  	XWMHints wmhints;
	Pixel back_pix, fore_pix;
	Pixmap pixmask;

  	if (!(display = XOpenDisplay(display_name)))
    		error("can't open display %s",XDisplayName(display_name));

	screen=DefaultScreen(display);
  	root=RootWindow(display, screen);

  	/* Create window. */
  	sizehints.flags = USSize | USPosition;
  	sizehints.x = 0;
  	sizehints.y = 0;
  	XWMGeometry(display, screen, "", NULL, borderwidth,
		    &sizehints, &sizehints.x, &sizehints.y,
		    &sizehints.width, &sizehints.height, &dummy);

	sizehints.width = 64;
  	sizehints.height = 64;
  	sizehints.x = pos[0];
  	sizehints.y = pos[1];
        back_pix = WhitePixel(display, screen);
        fore_pix = BlackPixel(display, screen);
  	win = XCreateSimpleWindow(display, root, sizehints.x, sizehints.y,
				  sizehints.width, sizehints.height,
				  borderwidth, fore_pix, back_pix);
  	iconwin = XCreateSimpleWindow(display, win, sizehints.x,
				      sizehints.y, sizehints.width,
				      sizehints.height, borderwidth,
				      fore_pix, back_pix);

  	/* Activate hints */
       	XSetWMNormalHints(display, win, &sizehints);
        classhint.res_name = wname;
   	classhint.res_class = wname;
       	XSetClassHint(display, win, &classhint);
  
  	if (! XStringListToTextProperty(&wname, 1, &name))
	  	error("Can't allocate window name.");
  	
  	XSetWMName(display, win, &name);
  
  	/* Create GC for drawing */
  	gcv.foreground = fore_pix;
  	gcv.background = back_pix;
  	gcv.graphics_exposures = 0;
  	NormalGC = XCreateGC(display, root, 
			     GCForeground | GCBackground | GCGraphicsExposures,
			     &gcv);

  	pixmask = XCreateBitmapFromData(display, win, mask_bits,
					mask_width,mask_height);
  	XShapeCombineMask(display, win, ShapeBounding, 0, 0,
			  pixmask, ShapeSet);
  	XShapeCombineMask(display, iconwin, ShapeBounding, 0, 0,
			  pixmask, ShapeSet);
	
  	wmhints.initial_state = WithdrawnState;
  	wmhints.icon_window = iconwin;
  	wmhints.icon_x = sizehints.x;
  	wmhints.icon_y = sizehints.y;
  	wmhints.window_group = win;
  	wmhints.flags = StateHint | IconWindowHint | 
    			IconPositionHint | WindowGroupHint;
  
  	XSetWMHints(display, win, &wmhints);
  	XSetCommand(display, win, argv, argc);

	XSelectInput(display, iconwin, ExposureMask);
	XSelectInput(display, win, ExposureMask);

 	XMapWindow(display, win);
}

void flush_expose(Window w) {
  	XEvent dummy;
  
  	while (XCheckTypedWindowEvent(display, w, Expose, &dummy));
}

void redraw_window() {
  	flush_expose(iconwin);
  	XCopyArea(display, images[FACE], iconwin, NormalGC, 0, 0,
		  image_info[FACE].width, image_info[FACE].height, 0,0);
  	flush_expose(win);
  	XCopyArea(display, images[FACE], win, NormalGC, 0, 0, 
		  image_info[FACE].width, image_info[FACE].height, 0,0);
}

/*
 * Display an image, using XCopyArea. Can display only part of an image,
 * located anywhere.
 */
void copy_image(int image, int xoffset, int yoffset,
		int width, int height, int x, int y) {
	XCopyArea(display, images[image], images[FACE], NormalGC,
		  xoffset, yoffset, width, height, x, y);
}

/*
 * Display a letter in one of two fonts, at the specified x position.
 * Note that 10 is passed for special characters `:' or `1' at the 
 * end of the font. 
 */
void draw_letter(int letter, int font, int x) {
	copy_image(font, image_info[font].charwidth * letter, 0,
		   image_info[font].charwidth, image_info[font].height,
		   x, image_info[font].y);
}

/* Display an image at its normal location. */
void draw_image(int image) {
  	copy_image(image, 0, 0, 
		   image_info[image].width, image_info[image].height,
		   image_info[image].x, image_info[image].y);
}

void recalc_window(apm_info cur_info) {
	int time_left, hour_left, min_left, digit, x;
	static int blinked = 0;
	
	/* Display if it's plugged in. */
      	switch (cur_info.ac_line_status) {
	  case 1:
		draw_image(PLUGGED);
		break;
       	  default:
		draw_image(UNPLUGGED);
      	}
    
      	/* Display the appropriate color battery. */
      	switch (cur_info.battery_status) {
	  case 0: /* high */
	  case 3: /* charging */
		draw_image(BATTERY_HIGH);
		break;
	  case 1:
		draw_image(BATTERY_MEDIUM);
		break;
	  case 2: /* critical -- blinking red battery */
		if (blinked)
			draw_image(BATTERY_LOW);
		else
			draw_image(BATTERY_BLINK);
		blinked=!blinked;
		break;
	  default:
		draw_image(BATTERY_NONE);
      	}

      	/* Show if the battery is charging. */
  	if (cur_info.battery_flags & 8) {
		draw_image(CHARGING);
	}
  	else {
		draw_image(NOCHARGING);
      	}

     	/*
       	 * Display the percent left dial. This has the side effect of
         * clearing the time left field. 
         */
  	x=DIAL_MULTIPLIER * cur_info.battery_percentage;
      	if (x >= 0) {
		/* Start by displaying bright on the dial. */
		copy_image(DIAL_BRIGHT, 0, 0,
			   x, image_info[DIAL_BRIGHT].height,
			   image_info[DIAL_BRIGHT].x,
			   image_info[DIAL_BRIGHT].y);
      	}
      	/* Now display dim on the remainder of the dial. */
  	copy_image(DIAL_DIM, x, 0,
		   image_info[DIAL_DIM].width - x,
		   image_info[DIAL_DIM].height,
		   image_info[DIAL_DIM].x + x,
		   image_info[DIAL_DIM].y);
  
      	/* Show time left */
      	if (cur_info.battery_time >= 0) {
        	if (cur_info.using_minutes)
          		time_left = cur_info.battery_time;
        	else
          		time_left = cur_info.battery_time / 60; 
        	hour_left = time_left / 60;
        	min_left = time_left % 60;
        	digit = hour_left / 10;
        	draw_letter(digit,BIGFONT,HOURS_TENS_OFFSET);
        	digit = hour_left % 10;
		draw_letter(digit,BIGFONT,HOURS_ONES_OFFSET);
       		digit = min_left / 10;
        	draw_letter(digit,BIGFONT,MINUTES_TENS_OFFSET);
        	digit = min_left % 10;
        	draw_letter(digit,BIGFONT,MINUTES_ONES_OFFSET);
      	}

      	/* Show percent remaining */
      	if (cur_info.battery_percentage >= 0) {
        	digit = cur_info.battery_percentage / 10;
       		if (digit == 10) {
		  	/* 11 is the `1' for the hundreds place. */
	  		draw_letter(11,SMALLFONT,HUNDREDS_OFFSET);
	  		digit=0;
		}
		draw_letter(digit,SMALLFONT,TENS_OFFSET);
		digit = cur_info.battery_percentage % 10;
		draw_letter(digit,SMALLFONT,ONES_OFFSET);
      	}
  	else {
	  	/* There is no battery, so we need to dim out the
		 * colon and percent sign that are normally bright. */
	  	draw_letter(10,SMALLFONT,PERCENT_OFFSET);
	  	draw_letter(10,BIGFONT,COLON_OFFSET);
	}

	redraw_window();
}

void alarmhandler(int sig) {
	apm_info cur_info;
	
	if (! apm_read(&cur_info))
		error("Cannot read APM information.");
	
	/* If APM data changes redraw and wait for next update */
	/* Always redraw if the status is critical, to make it blink. */
	if (!apm_change(&cur_info) || cur_info.battery_status == 2)
		recalc_window(cur_info);

	alarm(DELAY);
}

int main(int argc, char *argv[]) {
	make_window(parse_commandline(argc, argv), argc ,argv);

	/*  Check for APM support */
	if (! apm_exists())
		error("No APM support in kernel.");
	
	load_images();
	
	signal(SIGALRM, alarmhandler);
	alarmhandler(SIGALRM);

	while (1) {
		XEvent ev;
		XNextEvent(display, &ev);
		if (ev.type == Expose)
			redraw_window();
	}
}
