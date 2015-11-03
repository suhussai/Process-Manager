build:
	rm -rf memwatch.log
	gcc -Wall -DMEMWATCH -DMW_STDIO main.c memwatch.c linkedList.c -o procnanny
run:
	rm -rf memwatch.log
	gcc -Wall -DMEMWATCH -DMW_STDIO main.c memwatch.c linkedList.c -o procnanny
	./procnanny /home/bobby/procnanny/config.txt

clean:
	rm -rf procnanny 
