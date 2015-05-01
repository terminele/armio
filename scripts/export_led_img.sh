#!/bin/bash
# use to make drawing exports from watch image


#      transform="translate(-1.56563,-165.55181)"
#-     style="display:inline"
#+     style="display:none"
#      inkscape:label="LED - ON"
#      id="g20789"

IMG_FILE=./graphics/DECKO_pcb_layers.svg
EXPORT_LED_DIR=./graphics/images
EXPORT_LED_BASE=$EXPORT_LED_DIR/led
OPACITY=0

leds_off () {
    FROM=inline
    TO=none
    sed -i "N;s/$FROM\(.*LED - ON\)/$TO\1/" $IMG_FILE
}

leds_on () {
    FROM=none
    TO=inline
    sed -i "N;s/$FROM\(.*LED - ON\)/$TO\1/" $IMG_FILE
}

mkdir -p $EXPORT_LED_DIR

leds_off;

inkscape -f $IMG_FILE -C -e $EXPORT_LED_DIR/face.png

EXP_NAME=$EXPORT_LED_BASE
EXP_NAME+=.png
inkscape -f $IMG_FILE -C -e $EXP_NAME -j -i LED0 -y $OPACITY

leds_on;

for i in $( seq 0 59 ); do
    EXP_NAME=$EXPORT_LED_BASE
    EXP_NAME+=$i
    EXP_NAME+=.png
    echo "Save to $EXP_NAME"
    inkscape -f $IMG_FILE -C -e $EXP_NAME -j -i LED$i -y $OPACITY
done;
