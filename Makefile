TARGET=wormy
CFLAGS=`pkg-config --cflags cairo` -Wall -O0 -Werror
LDFLAGS=`pkg-config --libs cairo` -lm -lXcursor -lXfixes

$(TARGET): wormy.o
	gcc $(LDFLAGS) $(^) -o $(@)
clean:
	rm -f *.o $(TARGET) 
