build:
	gcc -Wall -DMEMWATCH -DMW_STDIO main.c memwatch.c -o procnanny
run:
	./procnanny /home/bobby/Dropbox/CMPUT379/config.txt
clean:
	rm -rf procnanny 
