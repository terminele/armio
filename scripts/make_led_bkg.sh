#!/bin/bash
# use to create background for all the LEDs

IMG_DIR=./graphics/images
FACE=face.png
MASK=led_mask.png
LED_BKG=led_background.png
LED_CLEAR=led_clear.png

make_led_mask() {      # [hr] [min]
    ledlist=$1      # possibly only create a mask where previous led were??

    echo "Making mask for LEDs"

    ARGS=""

    for i in {0..59}; do
        ARGS+=" ( $IMG_DIR/led$(printf %02d $i).png "
        ARGS+=" -channel A -threshold 1% ) "
    done

    fn=$IMG_DIR/$MASK
    FARGS="$ARGS -background transparent -flatten $fn"

    #echo "convert $FARGS"
    convert $FARGS
    #convert $IMG_DIR/$FN -format %c histogram:info:-
}

make_led_bkg() {
    if [ ! -f $IMG_DIR/$MASK ]; then
        make_led_mask;
    fi

    echo "Making LED background"
    convert $IMG_DIR/$FACE \( $IMG_DIR/$MASK -alpha extract \) \
        -alpha Off -compose CopyOpacity -composite $IMG_DIR/$LED_BKG
}

make_led_clear() {
    if [ ! -f $IMG_DIR/$MASK ]; then
        make_led_mask;
    fi

    echo "Making cleared LED"
    convert $IMG_DIR/$LED_BKG \
        \( $IMG_DIR/led45.png -channel A -evaluate multiply 0.001 +channel \) \
        -background transparent -flatten $IMG_DIR/$LED_CLEAR

    #convert $IMG_DIR/$FACE -alpha off -background Red temp.png
    #convert temp.png \
    #    \( $IMG_DIR/$MASK -alpha extract \) \
    #    -alpha Off -compose CopyOpacity -composite $IMG_DIR/$LED_CLEAR
    convert $IMG_DIR/$LED_CLEAR -format %c histogram:info:-
}


if [ ! -f $IMG_DIR/$LED_BKG ]; then
    make_led_bkg;
fi

if [ ! -f $IMG_DIR/$LED_CLEAR ]; then
    make_led_clear;
fi
