
pngviewer: png_viewer.c 
	gcc -Wall png_viewer.c -lpng -o pngviewer

clean:
	rm  pngviewer
