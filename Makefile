TARGET = pspvecx
OBJS = pspint.o vecx.o e6809.o graphics.o framebuffer.o flib.o mp3player.o

INCDIR =
CFLAGS = -G0 -Wall -O2
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)

LIBDIR = 
LDFLAGS =
LIBS= -lpsppower -lpspgu -lpng -lz -lm -lfreetype -lmad -lpspaudiolib -lpspaudio

EXTRA_TARGETS = EBOOT.PBP SCEkxploit
PSP_EBOOT_TITLE = PSP Vectrex Emu 1.51
PSP_EBOOT_ICON = ICON0.PNG
PSP_EBOOT_PIC1 = PIC1.PNG

PSPSDK=$(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
