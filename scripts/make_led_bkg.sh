#!/bin/bash
# use to create background for all the LEDs

IMG_DIR=./graphics/images
FACE=face.png
MASK=led_mask.png
LED_BKG=led_background.png

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


if [ ! -f $IMG_DIR/$LED_BKG ]; then
    make_led_bkg;
fi
