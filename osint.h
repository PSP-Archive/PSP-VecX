#ifndef __OSINT_H
#define __OSINT_H


/* define the machine type to compile for (choose only one!) */
//#define MACHINE_WINDOWS
#define MACHINE_PSP

extern char gbuffer[1024];

void osint_render(void);
int osint_msgs(void);
void osint_errmsg(const char *errmsg);

#endif
