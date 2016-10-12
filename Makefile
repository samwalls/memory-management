CC= gcc
CFLAGS= -g -Wall
LIBOBJS = myalloc.o
LIB=myalloc
LIBFILE=lib$(LIB).a
TESTS = test1 test2 test3 test4 test5 test6 test7
all: $(TESTS)

%.o: %.c
	$(CC) $(CFLAGS) -c $<

test1 : test1.o $(LIB)
	$(CC) test1.o $(CFLAGS) -o test1 -L. -l$(LIB)

test2 : test2.o $(LIB)
	$(CC) test2.o $(CFLAGS) -o test2 -L. -l$(LIB)

test3 : test3.o $(LIB)
	$(CC) test3.o $(CFLAGS) -o test3 -L. -l$(LIB)

test4 : test4.o $(LIB)
	$(CC) test4.o $(CFLAGS) -o test4 -L. -l$(LIB)

test5 : test5.o $(LIB)
	$(CC) test5.o $(CFLAGS) -o test5 -L. -l$(LIB)

test6 : test6.o $(LIB)
	$(CC) test6.o $(CFLAGS) -o test6 -L. -l$(LIB)

test7 : test7.o $(LIB)
	$(CC) test7.o $(CFLAGS) -o test7 -L. -l$(LIB)

$(LIB) : $(LIBOBJS)
	ar -cvr $(LIBFILE) $(LIBOBJS)
	#ranlib $(LIBFILE) # may be needed on some systems
	ar -t $(LIBFILE)

clean:
	/bin/rm -f *.o $(TESTS) $(LIBFILE)
