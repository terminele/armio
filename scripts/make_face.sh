#!/bin/bash
# use to create a face for the clock

IMG_DIR=./graphics/images

# set USE_FACE_IN_ALL_FRAMES to true to embed the watch face in all frames and
# anything other than true to only embed the face in the first frame
USE_FACE_IN_ALL_FRAMES=false

paint_time() {      # [hr] [min]
    if [ -z "$1" ]; then
        hr=$(( $(date +%H) % 12 ))
        min=$(echo $(date +%M) | sed 's/^0//')
    elif [ -z "$2" ]; then
        hr=$1
        min=$(echo $(date +%M) | sed 's/^0//')
    else
        hr=$1
        min=$2
    fi

    fnbase=$hr$(printf %02d $min)_min_

    if [ $(ls $IMG_DIR | grep $fnbase | wc -l) -eq 2 ]; then
        echo "$hr:$(printf %02d $min) on/off files already exist"
        return
    else
        if [ "$USE_FACE_IN_ALL_FRAMES" != "true" ]; then
            ./scripts/make_led_bkg.sh
        fi

        echo "creating $hr:$(printf %02d $min) on/off files"
    fi

    # show the 'face' as the background??
    if [ $USE_FACE_IN_ALL_FRAMES = true ]; then
        ARGS="$IMG_DIR/face.png "
    else
        ARGS="$IMG_DIR/led_background.png "
    fi

    min_len=$(( ($min * 5) / 60 + 1 ))
    dim_scale=$(bc <<< "scale=2; 1/$min_len")

    hstart=$(( $hr * 5 + $min_len - 1))
    hend=$(( $hr * 5 ))
    led_level=1

    #echo "hour starts on $hstart and ends on $hend"
    #echo "$min_len dims by $dim_scale"

    for i in $(seq $hstart -1 $hend); do
        if [ "$i" -ne "$min" ]; then
            #echo "led$(printf %02d $i).png has intensity $led_level"
            ARGS+=" ( $IMG_DIR/led$(printf %02d $i).png "
            ARGS+=" -channel A -evaluate multiply $led_level +channel ) "
        #else
        #    echo "suppressing led$(printf %02d $i).png because of minute overlap"
        fi
        led_level=$(bc <<< "scale=2; $led_level - $dim_scale")
    done

    fn=$IMG_DIR/$fnbase
    fn+=off.png
    FARGS="$ARGS -background transparent -flatten $fn"
    #echo "convert $FARGS"
    convert $FARGS

    #echo "adding minute led$(printf %02d $min).png at 0.75 intensity"
    ARGS+=" ( $IMG_DIR/led$(printf %02d $min).png "
    ARGS+=" -channel A -evaluate multiply 0.75 +channel ) "

    fn=$IMG_DIR/$fnbase
    fn+=on.png
    FARGS="$ARGS -background transparent -flatten $fn"
    #echo "convert $FARGS"
    convert $FARGS
}

paint_time $1 $2;
