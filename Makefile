IDIR = ./inc
CC = CL
#CFLAGS = -I

L1IFtap.exe: L1IFtap.c inc/circular_buffer.c
	$(CC) L1IFtap.c inc/circular_buffer.c
	rm *.obj

clean:
	rm *.exe
