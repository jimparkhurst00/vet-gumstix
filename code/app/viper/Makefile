# Makefile made with help from http://www.hsrl.rutgers.edu/ug/make_help.html
# $@ is the name of the file to be made.
# $? is the names of the changed dependents. 
# $< the name of the related file that caused the action.
# $* the prefix shared by target and dependent files. 

CC=gcc
RM=rm -f

#CFLAGS=-DnotPERFCRIT -fno-stack-protector

# oject files
C_OBJS = simulator.o rand.o arc4.o
# source .c file
SRCS = simulator.c rand.c arc4.c
simul.o:$(C_OBJS)
	$(CC) -o $@ $(C_OBJS)
clean:
	$(RM) .*.cmd *.mod.c *.o *.ko *~ -r .tmp*
