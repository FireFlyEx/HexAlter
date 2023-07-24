
HexAlter v0.3
=============

This is a quick (but hopefully not dirty) implementation of a primitive command-line hex editor.  Usage
is very simple:

    hexalter [-i] file address1=byte1,..,byten ... addressn=byte1,...,byten

HexAlter will either modify files directly, or with the -i option it will create an IPS file for
ROM patching.

For example:

    hexalter foo.txt 0x4=1,2,3 0x20=0xa0 0xff=4 0x29f8=32

Will make the following changes:

**[ Offset Address | New Hex Value | New Char8_t Value ]**

    0x4       |    31    |    1  
    0x5       |    32    |    2  
    0x6       |    33    |    3  
    0x20      |    A0  
    0xFF      |    34    |    4  
    0x29F8    |    20  

Or:

    hexalter -i foo.ips 0x4=1,2,3 0x20=0xa0 0xff=4

Creates foo.ips, which can be used to make the above mentioned changes.

Addresses may not overlap.  If address is out of range or byte values are too high/low or
address or byte values aren't entered in ***decimal*** or ***hexademical***, hexalter should refuse
to patch.  IPS files are bound to 16MB, due to a limitation in IPS's ability to only
store 24-bit addresses.

If during the actual apply phase some error happens, only some changes might be applied.
