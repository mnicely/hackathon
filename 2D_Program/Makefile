NVCC = nvcc
CC = gcc
CFLAGS = -Xcompiler -Wall -Xcompiler -fopenmp -O2
INCLUDES = -I/usr/local/include -I/usr/local/cuda/include
LFLAGS = -L/usr/local/lib -L/usr/local/cuda/lib64 
LIBS = -lm -lnvToolsExt
SRCS = $(wildcard *.c) $(wildcard */*.c)
OBJS = $(SRCS:.c=.o)
MAIN = Direct_Solver

ifneq ($(origin USE_CUFFTW), undefined)
	ifneq ($(USE_CUFFTW), 0)
		CFLAGS += -DUSE_CUFFTW
		LIBS += -lcufft -lcufftw
	else
		LIBS += -lfftw3_omp -lfftw3
	endif
else
	LIBS += -lfftw3_omp -lfftw3
endif

ifneq ($(origin USE_OMP), undefined)
	ifneq ($(USE_OMP), 0)
		CFLAGS += -DUSE_OMP
	endif
endif

.PHONY: depend clean

all: $(MAIN)
	@echo  Program has been compiled

$(MAIN): $(OBJS) 
	$(NVCC) -ccbin $(CC) $(CFLAGS) $(INCLUDES) -o $(MAIN) $(OBJS) $(LFLAGS) $(LIBS)

.c.o:
	$(NVCC) -ccbin $(CC) $(CFLAGS) $(INCLUDES) -c $<  -o $@

clean:
	$(RM) *.o *~ $(MAIN) */*.o

depend: $(SRCS)
	makedepend $(INCLUDES) $^
