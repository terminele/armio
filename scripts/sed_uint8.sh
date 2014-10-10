#!/bin/bash
# use to convert all the empty structure members using derived types to
# standard types ( work-around for a bug in ctags )

for file in $(find ../src -iname "*.[ch]"); do
    #echo $file
    sed -i 's/uint8_t\(\s*\):/unsigned char\1:/g' $file;
    sed -i 's/int8_t\(\s*\):/signed char\1:/g' $file;
    sed -i 's/uint16_t\(\s*\):/unsigned short int\1:/g' $file;
    sed -i 's/int16_t\(\s*\):/signed short int\1:/g' $file;
    sed -i 's/uint32_t\(\s*\):/unsigned long int\1:/g' $file;
    sed -i 's/int32_t\(\s*\):/signed long int\1:/g' $file;
done
