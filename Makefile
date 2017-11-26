.phony default:
default: diskinfo disklist

diskinfo: diskinfo.c
	gcc diskinfo.c common.c -o diskinfo

disklist: disklist.c
	gcc disklist.c common.c -o disklist

.PHONY clean:
clean:
	-rm -rf *.o *.exe