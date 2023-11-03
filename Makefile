IDIR = ./inc
CC = CL
#CFLAGS = -I
CURRENT_HASH := '"$(shell git rev-parse HEAD)"'
CURRENT_DATE := '"$(shell date /t)"'
CURRENT_NAME := '"L1IFtap"'

L1IFtap: L1IFtap.c inc/circular_buffer.c
	$(CC) L1IFtap.c inc/circular_buffer.c /DCURRENT_HASH=$(CURRENT_HASH) \
	/DCURRENT_DATE=$(CURRENT_DATE) /DCURRENT_NAME=$(CURRENT_NAME)
	rm *.obj

clean:
	rm *.exe
