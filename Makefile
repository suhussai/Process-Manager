build:
	gcc -Wall -DMEMWATCH -DMW_STDIO main.c memwatch.c -o procnanny
	./procnanny /home/bobby/procnanny/config.txt
clean:
	rm -rf procnanny 
