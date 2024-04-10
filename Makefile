
CFLAGS = -D USE_DEV_LIB -O
CC = gcc
DIR_LCD      = ./LCD
DIR_PICS	 = ./pic
DIR_BIN      = ./bin
NASSIE_UTILS = NASsie_utils.c NASsie_utils.h
LIB = -llgpio -lm -lc
OBJ_C = $(wildcard ${DIR_LCD}/*.c , wildcard ${DIR_PICS}/*.c)
OBJ_O = $(patsubst %.c,${DIR_BIN}/%.o,$(notdir ${OBJ_C}))
TARGET = NASsie


${TARGET}:${OBJ_O} NASsie.o NASsie_utils.o
	$(CC) $(CFLAGS) $(OBJ_O) NASsie.o NASsie_utils.o -o $@ $(LIB)

	
NASsie.o: NASsie.c $(DIR_PICS)/%.h
	$(CC) $(CFLAGS) -c NASsie.c -o $@ $(LIB)
	
NASsie_utils.o: NASsie_utils.c NASsie_utils.h
	$(CC) $(CFLAGS) -c NASsie_utils.c -o $@ $(LIB)

${DIR_BIN}/%.o:$(DIR_LCD)/%.c
	$(CC) $(CFLAGS) -c  $< -o $@ 
	
${DIR_BIN}/%.o:$(DIR_PICS)/%.c
	$(CC) $(CFLAGS) -c  $< -o $@ 
	
$(DIR_PICS)/%.h:
	
clean :
	rm -f $(DIR_BIN)/*.* 
	rm -f $(TARGET) 
	rm -f *.o
	