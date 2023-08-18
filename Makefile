IDIR = ./inc
CC = CL
#CFLAGS = -I

L1IFtap.exe: L1IFtap.c
	$(CC) L1IFtap.c
	rm *.obj

clean:
	rm *.exe
