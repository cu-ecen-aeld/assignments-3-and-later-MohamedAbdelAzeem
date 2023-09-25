
#defult compilter (Native compiler)


ifdef CROSS_COMPILE
CC = $(CROSS_COMPILE)gcc
else
CC = gcc 
endif 

OUTPUT = writer

all: writer

writer:	
		$(CC) writer.c  -o $(OUTPUT)

clean:
		rm -f $(OUTPUT) 



