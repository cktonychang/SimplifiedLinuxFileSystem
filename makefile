all: BDS BDC_cmd BDC_rand IDS FS FC clear
BDS.o: BDS.c header.h
	gcc -c BDS.c
BDS: BDS.o
	gcc BDS.o -o BDS
BDC_command.o: BDC_command.c header.h
	gcc -c BDC_command.c
BDC_cmd: BDC_command.o
	gcc BDC_command.o -o BDC_cmd
BDC_random.o: BDC_random.c header.h
	gcc -c BDC_random.c
BDC_rand: BDC_random.o
	gcc BDC_random.o -o BDC_rand 
IDS.o: IDS.c header.h
	gcc -c IDS.c
IDS: IDS.o
	gcc IDS.o -o IDS 
FS.o: FS.c header.h
	gcc -c FS.c
FS: FS.o
	gcc FS.o -o FS 
FC.o: FC.c header.h
	gcc -c FC.c
FC: FC.o
	gcc FC.o -o FC 
clear: 
	rm *.o
