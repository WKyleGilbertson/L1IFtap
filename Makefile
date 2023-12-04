IDIR = ./inc
CC = cl
#CFLAGS = -I
CURRENT_HASH := '"$(shell git rev-parse HEAD)"'
CURRENT_DATE := '"$(shell date /t)"'
CURRENT_NAME := '"L1IFtap"'

L1IFtap: L1IFtap.c
	$(CC) L1IFtap.c /EHsc /DCURRENT_HASH=$(CURRENT_HASH) \
	/DCURRENT_DATE=$(CURRENT_DATE) /DCURRENT_NAME=$(CURRENT_NAME) 
	del *.obj

clean:
	del  L1IFtap.exe
