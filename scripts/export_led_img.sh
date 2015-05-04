#!/bin/bash
# use to make drawing exports from watch image

IMG_FILE=./graphics/DECKO_pcb_layers.svg
EXPORT_LED_DIR=./graphics/images
EXPORT_LED_BASE=$EXPORT_LED_DIR/led
OPACITY=0

layer_off () {
    FROM=inline
    TO=none
    TAG=$1
    sed -i "N;N;s/\($TAG\".*\)$FROM/\1$TO/" $IMG_FILE
    sed -i "N;N;s/$FROM\(.*$TAG\)/$TO\1/" $IMG_FILE
}

layer_on () {
    FROM=none
    TO=inline
    TAG=$1
    sed -i "N;N;s/\($TAG\".*\)$FROM/\1$TO/" $IMG_FILE
    sed -i "N;N;s/$FROM\(.*$TAG\)/$TO\1/" $IMG_FILE
}

mkdir -p $EXPORT_LED_DIR

layer_off "LED - ON";
layer_off "Labels";
inkscape -f $IMG_FILE -C -e $EXPORT_LED_DIR/face.png
layer_on "LED - ON";


for i in $( seq 0 59 ); do
    EXP_NAME=$EXPORT_LED_BASE
    EXP_NAME+=$(printf "%02d" $i)
    EXP_NAME+=.png
    echo "Save to $EXP_NAME"
    inkscape -f $IMG_FILE -C -e $EXP_NAME -j -i LED$i -y $OPACITY
done;

layer_on "Labels";
inkscape -f $IMG_FILE -C -e $EXPORT_LED_DIR/labels.png -j -i label_layer -y $OPACITY
