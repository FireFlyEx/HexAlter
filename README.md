
HexAlter v0.3
=============

This is a quick (but hopefully not dirty) implementation of a primitive command-line hex editor.  Usage
is very simple:

    hexalter [-i] file address1=byte1,..,byten ... addressn=byte1,...,byten

Either hexalter will alter file directly or with the -i option will create an ips file for
ROM patching.

For example:

    hexalter foo.txt 0x4=1,2,3 0x20=0xa0 0xff=4 0x1ffffffff=32

Changes:

address | new value
-------------------
4       | 1  
5       | 2  
6       | 3  
32      | 160  
255     | 4  
8589934591 | 32  

Or:

    hexalter -i foo.ips 0x4=1,2,3 0x20=0xa0 0xff=4

Creates foo.ips, which can be used to make the above mentioned changes.

Addresses may not overlap.  If address is out of range or byte values are too high/low or
address or byte values aren't entered in decimal or hexademical, hexalter should refuse
to patch.  IPS files are bound to 16MiB, due to a limitation in IPS's ability to only
store 24-bit addresses.

If during the actual apply phase some error happens, only some changes might be applied.
