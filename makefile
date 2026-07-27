# Makefile of FastqType (2026/07/20)

CC = gcc
CFLAGS = -std=c99
LIBS = -lz -lbz2 -lm
INCLUDE =

DEBUG = 0

ifeq ($(DEBUG), 1)
	CFLAGS += -g -O0 # enable debugging
else
	CFLAGS += -O3
endif


OBJECT = file_read.o file_type.o fastq_type.o
PROG = fastq_type

all: $(PROG)

$(PROG): $(OBJECT) $(HTSLIB)
	$(CC) $(CFLAGS) $(INCLUDE) -o $@ $^ $(LIBS)

# generate object file (*.o) for each source file (*.c)
%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDE) -o $@ -c $<


.PHONY : clean
clean:
	rm -rf $(OBJECT) $(PROG) 

