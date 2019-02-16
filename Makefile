CC := gcc
LDFLAGS += -lm 
LDFLAGS += -lasound

ifeq ($(wildcard /usr/include/alsa/asoundlib.h),)
$(error /usr/include/alsa/asoundlib.h does not exist.  Install it with "sudo apt-get install libasound2-dev" or "sudo yum install alsa-lib-devel" )
endif



# --------------------------------  Main application  -----------------------------------------



bug : bug.o audio.o 
	$(CC) -o $@ $^ $(LDFLAGS)
	@echo " "


clean : 
	rm -f bug *.o

