smbios-poc: smbios-poc.c
	gcc -o smbios-poc smbios-poc.c `pkg-config glib-2.0 --cflags --libs`
clean:
	rm -f smbios-poc
