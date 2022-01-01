#include "osint.h"

#ifdef MACHINE_PSP  // if we're not compiling for PSP, skip this file

#include <pspkernel.h>
#include <pspctrl.h>
#include <pspdisplay.h>
#include <pspdebug.h>
#include <pspgu.h>
#include <psppower.h>
#include <psputils.h>
#include <pspaudio.h>
#include <pspaudiolib.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <math.h>
#include <png.h>
#include <sys/stat.h>

#include "graphics.h"
#include "vecx.h"
#include "flib.h"
#include "mp3player.h"

#ifdef __DEBUG_PSPVECX__
#include "Screenshot.h"
#endif


PSP_MODULE_INFO("PSPVECX 1.32", 0, 1, 1);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);


#define printf	pspDebugScreenPrintf
#define BGCOLOR 0
#define BUF_WIDTH (512)
#define SCR_WIDTH (480)
#define SCR_HEIGHT (272)
#define PIXEL_SIZE (4) /* change this if you change to another screenmode */
#define FRAME_SIZE (BUF_WIDTH * SCR_HEIGHT * PIXEL_SIZE)
#define ZBUF_SIZE (BUF_WIDTH SCR_HEIGHT * 2) /* zbuffer seems to be 16-bit? */

#define byte unsigned char

struct Vertex
{
	float x,y,z;
};


struct rom_info
{
	char name[64];			// rom name (not filename)
	char filename[10];		// rom filename
	int checksum;			// rom checksum for ID verification (crc32)
	byte overlay[64];			// overlay filename (if available)
	int x_offset;			// screen X offset
	int y_offset;			// screen Y offset
	byte button1;			// button 1 (leftmost) assignment
	byte button2;			// button 2 assignment
	byte button3;			// button 3 assignment
	byte button4;			// button 4 assignment
	byte buttonup;			// joystick up assignment
	byte buttondown;			// joystick down assignment
	byte buttonleft;			// joystick left assignment
	byte buttonright;			// joystick right assignment
	char b1_name[32];			// button 1 text
	char b2_name[32];			// button 2 text
	char b3_name[32];			// button 3 text
	char b4_name[32];			// button 4 text
	int year;				// year published
	char author[32];			// game author
	char description[3][32];	// short game description
	char thumbnail[64];		// screenshot thumbnail filename
};


struct button_data
{
	int psp_key;				// key psp button
	char repeat;				// key repeat enable
	int last;				// was key pressed on last pass?
	long timeout;				// countdown to repeat timeout (in ms)
	int help_page;				// keyhelp page number
	int help_slot;				// keyhelp slot number
	char *name;					// keyhelp label
};


#define NUM_BUTTONS 12

static int osint_defaults(void);
static void osint_updatescale(void);
void osint_clearscreen(void);
void osint_emuloop(void);
static void osint_line(long x0, long y0, long x1, long y1, unsigned char color, long index);
int check_controls(void);
float getlinelength(struct Vertex *p1, struct Vertex *p2);
char waitforcross(void);
void getpixel(struct Vertex *p1, struct Vertex *p2, struct Vertex *result);
int SetupCallbacks();
void set_image_alpha(Image *image, unsigned char alpha);
void set_image_color(Image *image, unsigned char r, unsigned char g, unsigned char b);
Image *copy_image(Image *from);
int load_cart(char *filename);
void unload_cart(void);
void load_overlay(char *filename);
void unload_overlay(void);
void show_splash(Image *pic, Image *background, int fade_rate, int timeout, int fade_both);
int load_config(void);
void scan_for_roms(void);
void run_graphic_menu(void);
char *underscore_to_space(char *text);
void label_buttons(Image *image, char *first, char *second, char *third, char*fourth);
void init_menu_text(Image *image);
int setup_thread(SceSize args, void *argp);
void run_intro(void);
void clear_controls(void);
void add_control(int key, int repeat, int page, int slot, char *name);
int check_controls2(void);


/* a global string buffer user for message output */
char gbuffer[1024];
SceCtrlData input;
struct Vertex scr_offset;
struct Vertex lines[32768][2];
char debug_lines = 0;
int cart_loaded = 0;
int overlay_loaded = 0;
int emu_exit = 0;
unsigned char empty_cart[4];
int first_pass = 1;
struct button_data button_list[NUM_BUTTONS];

static float screen_x;
static float screen_y;
static float scl_factor;
static float screen_x_offset = 40.0;

int average_time;

Image *intro1, *intro2, *intro3, *introbg, *menu;
Image *overlay, *marquee, *buttons;
Image *menutext, *first_image;

int paused = 0;
int setup_thread_done = 0;

int num_games = 13;
char *game_list[] =
{
	"Armor_Attack",
	"Bedlam",
	"Berzerk",
	"Blitz",
	"Clean_Sweep",
	"Cosmic_Chasm",
	"Dark_Tower",
	"Fortress_of_Narzod",
	"Heads_Up",
	"Mine_Storm",
	"Patriots",
	"Polar_Rescue",
	"Star_Castle"
};


struct rom_info *rom_list;		// the online list of roms


#define EMU_TIMER	20	/* the emulators heart beats at 20 milliseconds */
#define TEXT_GAP	16	// space between text lines
#define TEXT_X_START	80	// starting x position of menu text
#define FADE_BOTH_IN	1
#define FADE_BOTH_OUT	2


int main(int argc, char* argv[])
{
	int setup_thread_id;

	scePowerSetClockFrequency(333, 333, 166);
	SetupCallbacks();

	pspAudioInit();
	MP3_Init(1);
	MP3_Load("PSPVecX.mp3");
//	MP3_Load("got_game-mixdown.mp3");
	MP3_Play();

	initGraphics();

	setup_thread_id = sceKernelCreateThread("pspvecx_setup", setup_thread, 0x1f, 0xFA0, 0, 0);
	if(setup_thread_id < 0)
	{
		setup_thread_done = -1;
		setup_thread(0, 0);
	} else
		sceKernelStartThread(setup_thread_id, 0, 0);

	run_intro();

	if(setup_thread_done != 1)
	{
		int i = 0xff000000;
		clearScreen(i);
		//set_font_color(0xffffc0c0);
		//set_font_angle(0.0);
		//set_font_size(22);
		//text_to_screen("Loading...", 400, 265);
		flipScreen();
		i += 0x40;
		while(setup_thread_done != 1)
			sceKernelDelayThread(40000);
	}

	while(!emu_exit)
	{
		//run_menu();
		run_graphic_menu();
		if(!emu_exit)
			osint_emuloop();
		if(!emu_exit)
			MP3_Play();
	}

	sceGuTerm();
	sceKernelExitGame();
	return 0;
}


void run_intro(void)
{
	intro1 = loadImage("PSPVecX-01.png");
	intro2 = loadImage("PSPVecX-02b.png");
	intro3 = loadImage("PSPVecX-03b.png");
	introbg = loadImage("Background_Grid.png");
	show_splash(intro1, introbg, 8, 4, FADE_BOTH_IN);
	show_splash(intro2, introbg, 8, 4, 0);
	show_splash(intro3, introbg, 8, 8, FADE_BOTH_OUT);
	if(intro1)
		freeImage(intro1);
	if(intro3)
		freeImage(intro3);
	if(intro2)
		freeImage(intro2);
	if(introbg)
		freeImage(introbg);
}


// if called directly (not as a thread) setup_thread_done should be non-zero
// so we don't delete and exit the main thread
int setup_thread(SceSize args, void *argp)
{
	int is_thread = setup_thread_done;

//	sceKernelDelayThread(15000000);

	// set up the font library
	if(!load_font("OCR-a.ttf"))	// load "times new roman" font
	{
		osint_errmsg("Could not load font file.");
		return 1;
	}

	osint_defaults();
	memset(lines,0,sizeof(lines));
	memset(empty_cart, 0, 4);

	menu = loadImage("Menu-red.png");
	marquee = loadImage("title_panel.png");
	buttons = loadImage("button_panel.png");

	//if(!load_config())
	//{
		// need to generate default config file
	//}

	menutext = createImage(512, 512);
	if(!menutext)
	{
		osint_errmsg("Menutext creation failed!\n");
		return 1;
	}

	set_font_size(14);
	set_font_angle(90.0);
	set_font_color(0xffffffff);

	init_menu_text(menutext);

	first_image = createImage(480, 272);
	if(first_image)
	{
		if(marquee)
			blitImageToImage(0 ,0 ,46, 272, marquee, 0, 0, first_image);
		if(buttons)
			blitImageToImage(0 ,0 ,88, 272, buttons, 379, 0, first_image);
		//label_buttons(first_image, "Up", "Down", "Select", "Exit");
		if(menu)
		{
			set_image_alpha(menu, 70);
			blitAlphaImageToImage(0 ,0 ,345, 272, menu, 34, 0, first_image);
		}
		set_font_size(14);
		set_font_color(0xff6060b0);
		init_menu_text(first_image);
	}

	setup_thread_done = 1;
	if(!is_thread)
		sceKernelExitDeleteThread(0);

	return 0;
}


void init_menu_text(Image *image)
{
	int i;
	int text_x;

	if(image == menutext)
		text_x = TEXT_GAP;
	else
		text_x = TEXT_X_START;

	for(i = 0; i < num_games; i++)
	{
		text_to_image(underscore_to_space(game_list[i]), image, text_x, 250);
		text_x += TEXT_GAP;
	}
}


void run_graphic_menu(void)
{
	int selected = 0;
	int current = 0;
	int i;
	int change = 0;
	int binput = 0;
//	SceCtrlData last;
	char filename[64];
	int text_x, text_current = 0;
	//Image *first_image;

//	sceCtrlReadBufferPositive(&input, 1);
//	last.Buttons = input.Buttons;

	clear_controls();
	add_control(PSP_CTRL_LEFT, 1, 1, 1, "Up");
	add_control(PSP_CTRL_RIGHT, 1, 1, 2, "Down");
	add_control(PSP_CTRL_CROSS, 0, 1, 3, "Select");
	add_control(PSP_CTRL_SELECT, 0, 1, 4, "Exit");


	if(first_pass)
	{
		first_pass = 0;

		if(first_image)
		{
			for(i = 0; i <= 100; i += 5)
			{
				set_image_alpha(first_image, i);
				clearScreen(0);
				blitAlphaImageToScreen(0, 0, 480, 272, first_image, 0, 0);
				flipScreen();
				sceKernelDelayThread(40000);
			}

			freeImage(first_image);
		}
		else
		{
			clearScreen(0);
			if(marquee)
				blitImageToScreen(0 ,0 ,46, 272, marquee, 0, 0);
			if(buttons)
				blitImageToScreen(0 ,0 ,88, 272, buttons, 379, 0);
			//label_buttons(0, "Up", "Down", "Select", "Exit");
			flipScreen();
			clearScreen(0);
			if(marquee)
				blitImageToScreen(0 ,0 ,46, 272, marquee, 0, 0);
			if(buttons)
				blitImageToScreen(0 ,0 ,88, 272, buttons, 379, 0);
			label_buttons(0, "Up", "Down", "Select", "Exit");
		}

	}

	// set up screen

	set_font_size(14);
	set_font_angle(90.0);
	set_font_color(0xffffffff);
	set_image_alpha(menu, 70);

	while(!selected)
	{
		change = 0;
		text_x = 80;
		//printf("Select a game using D-pad Up/Down and X\n\n");
		if(!first_pass)
		{
			fillScreenRect(0, 34, 0, 345, 272);
			blitAlphaImageToScreen(0 ,0 ,310, 272, menutext, TEXT_X_START - TEXT_GAP, 0);

			if(menu)
				blitAlphaImageToScreen(0 ,0 ,345, 272, menu, 34, 0);
		}

		set_font_color(0xffffffff);
		text_current = TEXT_X_START + (current * TEXT_GAP);
		text_to_screen(underscore_to_space(game_list[current]), text_current, 250);


		sceDisplayWaitVblankStart();
		flipScreen();

		while(!change)
		{
/*			sceCtrlReadBufferPositive(&input, 1);
			if(input.Buttons != last.Buttons)
			{
				last.Buttons = input.Buttons;
				if(input.Buttons & PSP_CTRL_SELECT)
				{
					emu_exit = 1;
					return;
				}
				if(input.Buttons & PSP_CTRL_LEFT)
				{
					if(current != 0)
						current--;
					change = 1;
				}
				if(input.Buttons & PSP_CTRL_RIGHT)
				{
					if(current != (num_games - 1))
						current++;
					change = 1;
				}
				if(input.Buttons & PSP_CTRL_CROSS)
				{
					selected = 1;
					change = 1;
				}
*/
			binput = check_controls2();
//			if(input.Buttons != last.Buttons)
//			{
//				last.Buttons = input.Buttons;
				if(binput & PSP_CTRL_SELECT)
				{
					emu_exit = 1;
					return;
				}
				if(binput & PSP_CTRL_LEFT)
				{
					if(current != 0)
						current--;
					change = 1;
				}
				if(binput & PSP_CTRL_RIGHT)
				{
					if(current != (num_games - 1))
						current++;
					change = 1;
				}
				if(binput & PSP_CTRL_CROSS)
				{
					selected = 1;
					change = 1;
				}
//			}
			if(MP3_EndOfStream())
				MP3_Play();

			sceKernelDelayThread(10000);	// wait 10 milliseconds
		}
	}

	unload_cart();
	MP3_Stop();

	if(current != 9)
	{
		sprintf(filename, "roms/%s.bin", game_list[current]);
		if((load_cart(filename) != 0))
		{
			unload_overlay();
			sprintf(filename, "overlays/%s.png", game_list[current]);
			load_overlay(filename);
			return;
		}
	}

	unload_overlay();
	load_overlay("overlays/Mine_Storm.png");
}


char *underscore_to_space(char *text)
{
	static char holder[64];
	int i;

	for(i = 0; i < 64 && text[i] != 0; i++)
	{
		holder[i] = text[i];
		if(holder[i] == '_')
			holder[i] = ' ';
	}

	holder[i] = 0;

	return holder;
}


static int osint_defaults(void)
{
	screen_x = 272.0;  // 218.0
	screen_y = 480.0;

	osint_updatescale();

	sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

	/* the cart is empty by default */

	cart = 0;

	return 0;
}


static void osint_updatescale(void)
{
	float sclx, scly;

	sclx = (float) ALG_MAX_X / screen_x;
	scly = (float) ALG_MAX_Y / screen_y;

	if (sclx > scly) {
		scl_factor = sclx;
	} else {
		scl_factor = scly;
	}
}


void osint_clearscreen(void)
{
}


void osint_emuloop(void)
{
	clock_t start_time;
	int last_ten_times[10];
	int i = 0;
	unsigned timeout; /* timing flag */

	/* reset the vectrex hardware */

	vecx_reset();

	clearScreen(0);
	if(marquee)
		blitImageToScreen(0 ,0 ,46, 272, marquee, 0, 0);
	if(buttons)
		blitImageToScreen(0 ,0 ,88, 272, buttons, 379, 0);

	/* startup the emulator's heart beat */
	while(1)
	{
		timeout = 0;

		if(!paused)
		{
			start_time = sceKernelLibcClock();
			vecx_emu((VECTREX_MHZ / 1000) * EMU_TIMER, 0);
			last_ten_times[i++] = (int) (sceKernelLibcClock() - start_time);
			if(i == 10)
				i = 0;
			average_time =	(last_ten_times[0] +
					last_ten_times[1] +
					last_ten_times[2] +
					last_ten_times[3] +
					last_ten_times[4] +
					last_ten_times[5] +
					last_ten_times[6] +
					last_ten_times[7] +
					last_ten_times[8] +
					last_ten_times[9]) / 10;
		}

		if(!check_controls())  // 0 = exit pressed
			break;

		/* put timer code in here also */
	}

}


void osint_line(long x0, long y0, long x1, long y1, unsigned char color, long index)
{
	//unsigned int color_24bit = (color << 16) | (color << 8) | color;

	lines[index][0].x = (((float) y0 / scl_factor) * 1.0) + screen_x_offset;
	lines[index][0].y = 272.0 - (float) (x0 / scl_factor);
	lines[index][1].x = (((float) y1 / scl_factor) * 1.0) + screen_x_offset;
	lines[index][1].y = 272.0 - ((float) x1 / scl_factor);

	drawLineScreen((int)lines[index][0].x, (int)lines[index][0].y,
				(int)lines[index][1].x, (int)lines[index][1].y, 0x00ffffff);
}


void osint_render(void)
{
	long v;


//	clearScreen(0);
//
//	if(marquee)
//		blitImageToScreen(0 ,0 ,46, 272, marquee, 0, 0);
//	if(buttons)
//		blitImageToScreen(0 ,0 ,88, 272, buttons, 379, 0);

	fillScreenRect(0, 34, 0, 345, 272);

	for (v = 0; v < vector_draw_cnt; v++)
	{
		osint_line (vectors_draw[v].x0, vectors_draw[v].y0,
					vectors_draw[v].x1, vectors_draw[v].y1,
					vectors_draw[v].color, v);
	}

	if(overlay)
		blitAlphaImageToScreen(0 ,0 ,345, 272, overlay, 34, 0);

	flipScreen();
}

/* handle error messages from the emulator (bad machine codes etc) */
void osint_errmsg(const char *errmsg)
{
	/* Need to figure out what to do with these.
	   Switch to debug screen, show message, press certain key
	   to either continue emulating or quit the program.

	   Maybe an additional argument should be accepted to get the
	   byte offset where the error occurred?
	*/

	pspDebugScreenInit();
	pspDebugScreenSetXY(0, 2);
	printf("\n\n%s\n\nPress START to continue.", errmsg);

	while(!(input.Buttons & PSP_CTRL_START))
		sceCtrlReadBufferPositive(&input, 1);
}


int check_controls(void)
{
//#ifdef __DEBUG_PSPVECX__
	static char last_ss = 0;//, last_debug = 0;
	static int ss = 36;
	char ssname[64];
//#endif

	static char last_start = 0;

	snd_regs[14] |= 0x0f;
	alg_jch0 = 0x80;
	alg_jch1 = 0x80;

	sceCtrlPeekBufferPositive(&input, 1);

	if(input.Buttons & PSP_CTRL_SELECT)
		return 0;

	if(input.Buttons & PSP_CTRL_START)
	{
		if(!last_start)
			paused = !paused;
		last_start = 1;
	} else last_start = 0;

#ifdef __DEBUG_PSPVECX__
	if(input.Buttons & PSP_CTRL_LTRIGGER)
	{
		if(!last_ss)
		{
			snprintf(ssname, 63, "%i.png", ss++);
			Screenshot(ssname);
			last_ss = 1;
		}
	} else last_ss = 0;

	if(input.Buttons & PSP_CTRL_RTRIGGER)
	{
		if(!last_debug)
			debug_lines = !debug_lines;
		last_debug = 1;
	} else last_debug = 0;
#endif

	if(input.Buttons & PSP_CTRL_RTRIGGER)
	{
		snprintf(gbuffer, 64, "Average time = %d", average_time);
		osint_errmsg(gbuffer);
	}

	if(input.Buttons & PSP_CTRL_CROSS)
		snd_regs[14] &= ~0x08;
	if(input.Buttons & PSP_CTRL_CIRCLE)
		snd_regs[14] &= ~0x04;
	if(input.Buttons & PSP_CTRL_SQUARE)
		snd_regs[14] &= ~0x02;
	if(input.Buttons & PSP_CTRL_TRIANGLE)
		snd_regs[14] &= ~0x01;

	alg_jch0 = 0xff - input.Ly;
	alg_jch1 = input.Lx;

	if(input.Buttons & PSP_CTRL_DOWN)
		alg_jch0 = 0x00;
	if(input.Buttons & PSP_CTRL_UP)
		alg_jch0 = 0xff;
	if(input.Buttons & PSP_CTRL_LEFT)
		alg_jch1 = 0xff;
	if(input.Buttons & PSP_CTRL_RIGHT)
		alg_jch1 = 0x00;

	return 1;
}


float getlinelength(struct Vertex *p1, struct Vertex *p2)
{
	float a = 0.0, b = 0.0;

	a = fabs(p1->x - p2->x);

	b = fabs(p1->y - p2->y);

	return (sqrt(pow(a, 2) + pow(b, 2)));
}


char waitforcross(void)
{
	char got_cross = 0;

	while(!got_cross)
	{
		sceCtrlReadBufferPositive(&input, 1);
		if(input.Buttons & PSP_CTRL_TRIANGLE)
			return 0;
		if(input.Buttons & PSP_CTRL_SELECT)
		{
			sceKernelExitGame();
			return 1;
		}
		got_cross = input.Buttons & PSP_CTRL_CROSS;
	}

	while(got_cross)
	{
		sceCtrlReadBufferPositive(&input, 1);
		got_cross = input.Buttons & PSP_CTRL_CROSS;
	}

	return 1;
}


void getpixel(struct Vertex *p1, struct Vertex *p2, struct Vertex *result)
{
	result->x = (p1->x + p2->x) / 2.0;
	result->y = (p1->y + p2->y) / 2.0;
}


void set_image_alpha(Image *image, unsigned char alpha)
{
	int i, size;  // = image->textureWidth * image->textureHeight;
	Color *pixel; // = image->data;
	unsigned int alpha_mod;

	if(!image || !image->data)
		return;

	size = image->textureWidth * image->textureHeight;
	pixel = image->data;

	if(alpha > 100) alpha = 100;
	
	alpha_mod = ((alpha * 255) / 100) << 24;

	for(i = 0; i <= size; i++)
	{
		*pixel = (*pixel & 0x00ffffff) | alpha_mod;
		//*pixel = *pixel | alpha_mod;
		pixel++;
	}
}


/*void set_font_color(Image *font, byte r, byte g, byte b)
{
	int i, size;
	Color *pixel, red, green, blue, alpha;

	if(!font || !font->data)
		return;

	size = font->textureWidth * font->textureHeight;
	pixel = font->data;

	if(r > 100)
		r = 100;
	if(g > 100)
		g = 100;
	if(b > 100)
		b = 100;
//	if(a > 100)
//		a = 100;

	for(i = 0; i <= size; i++)
	{
		red = R(*pixel);
		green = G(*pixel);
		blue = B(*pixel);
		//alpha = A(*pixel);

		if(red < 10)
			*pixel = 0;
		else
		{
			red = (r * 255) / 100;
			green = ((g * 255) / 100) << 8;
			blue = ((b * 255) / 100) << 16;
			alpha = R(*pixel);

			*pixel = red | green | blue | alpha;
		}

		pixel++;
	}
}*/


void set_image_color(Image *image, unsigned char r, unsigned char g, unsigned char b)
{
	int i, size;
	Color *pixel, red, green, blue, grey;

	if(!image || !image->data)
		return;

	size = image->textureWidth * image->textureHeight;
	pixel = image->data;

	if(r > 100)
		r = 100;
	if(g > 100)
		g = 100;
	if(b > 100)
		b = 100;

	for(i = 0; i <= size; i++)
	{
		//grey = *pixel & 0x000000ff;
		grey = R(*pixel);
		//if(grey > 0)
		{
			red = (r * grey) / 100;
			green = ((g * grey) / 100) << 8;
			blue = ((b * grey) / 100) << 16;
			*pixel = (*pixel & 0xff000000) | red | green | blue;
			pixel++;
		}
	}
}


Image *copy_image(Image *from)
{
	Image *new_image = createImage(from->imageWidth, from->imageHeight);

	if(!new_image || !new_image->data)
		return NULL;

	memcpy(new_image->data, from->data, from->textureWidth * from->textureHeight * sizeof(Color));
	return new_image;
}


int load_cart(char *filename)
{
	FILE *cartfile;
	int read_bytes;
	//struct stat f_stat;

	if(!filename || filename == "")
	{
		osint_errmsg("Missing rom name");
		return 0;
	}

	cartfile = fopen(filename, "r");
	if(!cartfile)
	{
		snprintf(gbuffer, 64, "Could not open %s", filename);
		osint_errmsg(gbuffer);
		return 0;
	}

	/*if(!fstat(fileno(cartfile), &f_stat))
	{
		snprintf(gbuffer, 64, "Could not get file size for %s", filename);
		osint_errmsg(gbuffer);
		fclose(cartfile);
		return 0;
	}*/

	if(cart_loaded)
		unload_cart();

	//cart = (unsigned char *) malloc(f_stat.st_size);
	cart = (unsigned char *) malloc(32768);
	
	if(!cart)
	{
		//snprintf(gbuffer, 64, "Could not allocate %i bytes for %s", (int)f_stat.st_size, filename);
		snprintf(gbuffer, 64, "Could not allocate %i bytes for %s", 32768, filename);
		osint_errmsg(gbuffer);
		fclose(cartfile);
		return 0;
	}

	//read_bytes = fread(cart, 1, f_stat.st_size, cartfile);
	read_bytes = fread(cart, 1, 32768, cartfile);

	/*if(read_bytes != f_stat.st_size)
	{
		free(cart);
		cart = 0;
		snprintf(gbuffer, 64, "Could not load %s", filename);
		osint_errmsg(gbuffer);
		fclose(cartfile);
		return 0;
	}*/

	fclose(cartfile);
	cart_loaded = 1;
	return 1;
}


void unload_cart(void)
{
	if(cart_loaded)
		free(cart);

	cart = empty_cart;
	cart_loaded = 0;
}


void load_overlay(char *filename)
{
	overlay = loadImage(filename);

	if(!overlay)
	{
		snprintf(gbuffer, 64, "Could not load overlay %s", filename);
		osint_errmsg(gbuffer);
		overlay_loaded = 0;
		return;
	}

	overlay_loaded = 1;
	set_image_alpha(overlay, 70);

}


void unload_overlay(void)
{
	if(overlay_loaded && overlay != NULL)
		freeImage(overlay);

	overlay = 0;
	overlay_loaded = 0;
}


void show_splash(Image *pic, Image *background, int fade_rate, int timeout, int fade_both)
{
	int i, delay_time;
	int x_offset = 0, y_offset = 0;

	delay_time = 40000;
	timeout = (timeout * 1000000) / delay_time;

	if(pic->imageWidth < 480)
		x_offset = (480 - pic->imageWidth) / 2;
	if(pic->imageHeight < 480)
		y_offset = (272 - pic->imageHeight) / 2;

	if(pic)
	{
		for(i = 0; i <= 100; i += fade_rate)
		{
			if(fade_both == FADE_BOTH_IN)
				set_image_alpha(background, i);
			set_image_alpha(pic, i);
			clearScreen(0);
			if(background)
				blitAlphaImageToScreen(0, 0, background->imageWidth,
					background->imageHeight, background, 0, 0);
			blitAlphaImageToScreen(0, 0, pic->imageWidth, pic->imageHeight,
					 pic, x_offset, y_offset);
			flipScreen();
			sceKernelDelayThread(delay_time);
		}

		for(i = 0; i < timeout; i++)
		{
			sceCtrlReadBufferPositive(&input, 1);
			if(input.Buttons)
				break;
			sceKernelDelayThread(delay_time);
		}

		for(i = 100; i >= 0; i -= fade_rate)
		{
			if(i <= 5)
				i = 0;
			set_image_alpha(pic, i);
			if(fade_both == FADE_BOTH_OUT)
				set_image_alpha(background, i);
			clearScreen(0);
			if(background)
				blitAlphaImageToScreen(0, 0, background->imageWidth,
					background->imageHeight, background, 0, 0);
			blitAlphaImageToScreen(0, 0, pic->imageWidth, pic->imageHeight,
					 pic, x_offset, y_offset);
			flipScreen();
			sceKernelDelayThread(delay_time);
		}
	}
}


void label_buttons(Image *image, char *first, char *second, char *third, char*fourth)
{
	set_font_size(12);
	set_font_color(0xffffffff);
	set_font_angle(90.0);

	if(image)
	{
		text_to_image(first, image, 450, 240);
		text_to_image(second, image, 470, 190);
		text_to_image(third, image, 450, 135);
		text_to_image(fourth, image, 470, 60);
	} else {
		text_to_screen(first, 450, 240);
		text_to_screen(second, 470, 190);
		text_to_screen(third, 450, 135);
		text_to_screen(fourth, 470, 60);
	}

}


void clear_controls(void)
{
	int i;

	for(i = 0; i < NUM_BUTTONS; i++)
	{
		button_list[i].psp_key = 0;
		button_list[i].last = 0;
		button_list[i].repeat = 0;
		button_list[i].timeout = 0;
		button_list[i].help_page = 0;
		button_list[i].help_slot = 0;
		button_list[i].name = 0;
	}
}


void add_control(int key, int repeat, int page, int slot, char *name)
{
	int i;

	for(i = 0; i < NUM_BUTTONS; i++)
	{
		if(button_list[i].psp_key)
			continue;

		button_list[i].psp_key = key;
		button_list[i].last = 0;
		button_list[i].repeat = repeat;
		button_list[i].timeout = 0;
		button_list[i].help_page = page;
		button_list[i].help_slot = slot;
		button_list[i].name = name;
		break;
	}
}


#define REPEAT_DELAY 300		// milliseconds before first repeat
#define REPEAT_RATE 100			// milliseconds between subsequent presses
clock_t last_time = 0;
int check_controls2(void)
{
	int i, out, delta_time;	
	SceCtrlData in;

	out = 0;

	if(!last_time)
		delta_time = 0;
	else
		delta_time = (sceKernelLibcClock() - last_time) / 1000;
	last_time = sceKernelLibcClock();

	sceCtrlPeekBufferPositive(&in, 1);

	for(i = 0; i < NUM_BUTTONS; i++)
	{
		if(in.Buttons & button_list[i].psp_key)
		{
//sprintf(gbuffer, "Delta time = %i\n", delta_time);
//osint_errmsg(gbuffer);
			if(!button_list[i].repeat)
				out = out | button_list[i].psp_key;
			else if(button_list[i].last)
			{
				button_list[i].timeout -= delta_time;
				if(button_list[i].timeout <= 0)
				{
					button_list[i].timeout = REPEAT_RATE;
					out = out | button_list[i].psp_key;
				}
			} else
				out = out | button_list[i].psp_key;

			button_list[i].last = 1;
		} else
		{
			button_list[i].last = 0;
			if(button_list[i].repeat)
				button_list[i].timeout = REPEAT_DELAY;
		}
	}

	return out;
}


/* Exit callback */
int exit_callback(int arg1, int arg2, void *common)
{
	sceKernelExitGame();
	return 0;
}


/* Callback thread */
int CallbackThread(SceSize args, void *argp)
{
	int cbid;

	cbid = sceKernelCreateCallback("Exit Callback", exit_callback, NULL);
	sceKernelRegisterExitCallback(cbid);

	sceKernelSleepThreadCB();

	return 0;
}


/* Sets up the callback thread and returns its thread id */
int SetupCallbacks(void)
{
	int thid = 0;

	thid = sceKernelCreateThread("update_thread", CallbackThread, 0x11, 0xFA0, 0, 0);
	if(thid >= 0)
	{
		sceKernelStartThread(thid, 0, 0);
	}

	return thid;
}

#ifdef notDefined

CRC32 stuff

http://www.cl.cam.ac.uk/Research/SRG/bluebook/21/crc/node6.html#SECTION00060000000000000000

   /* Table of CRCs of all 8-bit messages. */
   unsigned long crc_table[256];
   
   /* Flag: has the table been computed? Initially false. */
   int crc_table_computed = 0;
   
   /* Make the table for a fast CRC. */
   void make_crc_table(void)
   {
     unsigned long c;
     int n, k;
   
     for (n = 0; n < 256; n++) {
       c = (unsigned long) n;
       for (k = 0; k < 8; k++) {
         if (c & 1)
           c = 0xedb88320L ^ (c >> 1);
         else
           c = c >> 1;
       }
       crc_table[n] = c;
     }
     crc_table_computed = 1;
   }
   
   /* Update a running CRC with the bytes buf[0..len-1]--the CRC
      should be initialized to all 1's, and the transmitted value
      is the 1's complement of the final running CRC (see the
      crc() routine below)). */
   
   unsigned long update_crc(unsigned long crc, unsigned char *buf,
                            int len)
   {
     unsigned long c = crc;
     int n;
   
     if (!crc_table_computed)
       make_crc_table();
     for (n = 0; n < len; n++) {
       c = crc_table[(c ^ buf[n]) & 0xff] ^ (c >> 8);
     }
     return c;
   }
   
   /* Return the CRC of the bytes buf[0..len-1]. */
   unsigned long crc(unsigned char *buf, int len)
   {
     return update_crc(0xffffffffL, buf, len) ^ 0xffffffffL;
   }
#endif

#endif // MACHINE_PSP
