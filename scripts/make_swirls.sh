#!/bin/bash
# use to create swirl still images

IMG_DIR=./graphics/images
SWIRL_FWD_BASE=swirl_fwd_
SWIRL_REV_BASE=swirl_rev_
STARTUP_FNAME=$IMG_DIR/swirl_fwd.miff
SHUTDOWN_FNAME=$IMG_DIR/swirl_rev.miff

TAIL_LEN=7
SWIRL_FWD_DELAY=5               # x / 100 seconds between frames
SWIRL_REV_DELAY=1               # x / 100 seconds between frames
#OFF_TIME=50                     # time off after reverse swirl

# set USE_FACE_IN_ALL_FRAMES to true to embed the watch face in all frames and
# anything other than true to only embed the face in the first frame
USE_FACE_IN_ALL_FRAMES=false


swirl_files() {
    ./scripts/export_led_img.sh
    if [ "$USE_FACE_IN_ALL_FRAMES" != "true" ]; then
        ./scripts/make_led_bkg.sh
    fi

    direction=$1
    if [ "$direction" = "-r" ]; then
        echo "Creating reverse swirl files"
        fnbase=$IMG_DIR/$SWIRL_REV_BASE
    else
        echo "Creating forward swirl files"
        fnbase=$IMG_DIR/$SWIRL_FWD_BASE
    fi
    hend=0

    for hstart in $(seq 0 $(( $TAIL_LEN - 1))); do
        fn=$fnbase$(printf %02d $hstart).png
        if [ -f $fn ]; then
            continue
        fi
        tail_len=$(($hstart - $hend + 1))
        dim_scale=$(bc <<< "scale=2; 1/$tail_len")
        led_level=1
        #echo "LED ending on $hstart has length $tail_len"

        # show the 'face' as the background??
        if [ $USE_FACE_IN_ALL_FRAMES = true ]; then
            ARGS="$IMG_DIR/face.png "
        else
            ARGS="$IMG_DIR/led_background.png"
        fi

        for i in $(seq $hstart -1 $hend); do
            if [ "$direction" = "-r" ]; then
                ARGS+=" ( $IMG_DIR/led$(printf %02d $(( (60-$i) % 60 ))).png "
            else
                ARGS+=" ( $IMG_DIR/led$(printf %02d $(( $i % 60 ))).png "
            fi
            ARGS+=" -channel A -evaluate multiply $led_level +channel ) "
            led_level=$(bc <<< "scale=2; $led_level - $dim_scale")
        done

        FARGS="$ARGS -background transparent -flatten $fn"
        #echo "convert $FARGS"
        convert $FARGS
    done

    tail_len=$TAIL_LEN
    dim_scale=$(bc <<< "scale=2; 1/$tail_len")
    for hstart in $(seq $TAIL_LEN 59); do
        fn=$fnbase$(printf %02d $hstart).png
        if [ -f $fn ]; then
            continue
        fi
        hend=$(($hstart-$TAIL_LEN))
        led_level=1

        # show the 'face' as the background??
        if [ $USE_FACE_IN_ALL_FRAMES = true ]; then
            ARGS="$IMG_DIR/face.png "
        else
            ARGS="$IMG_DIR/led_background.png"
        fi

        for i in $(seq $hstart -1 $hend); do
            if [ "$direction" = "-r" ]; then
                ARGS+=" ( $IMG_DIR/led$(printf %02d $(( (60-$i) % 60 ))).png "
            else
                ARGS+=" ( $IMG_DIR/led$(printf %02d $(( $i % 60 ))).png "
            fi
            ARGS+=" -channel A -evaluate multiply $led_level +channel ) "
            led_level=$(bc <<< "scale=2; $led_level - $dim_scale")
        done

        FARGS="$ARGS -background transparent -flatten $fn"
        #echo "convert $FARGS"
        convert $FARGS
    done
}

swirl_miff() {
    direction=$1
    if [ "$direction" = "-r" ]; then
        if [ $(ls $IMG_DIR | grep $SWIRL_REV_BASE | wc -l) -ne 60 ]; then
            swirl_files $direction;
        fi
        echo "Creating reverse swirl miff"
        fnbase=$IMG_DIR/$SWIRL_REV_BASE
        DELAY=$SWIRL_REV_DELAY
    else
        if [ $(ls $IMG_DIR | grep $SWIRL_FWD_BASE | wc -l) -ne 60 ]; then
            swirl_files $direction;
        fi
        echo "Creating forward swirl miff"
        fnbase=$IMG_DIR/$SWIRL_FWD_BASE
        DELAY=$SWIRL_FWD_DELAY
    fi

    if [ $USE_FACE_IN_ALL_FRAMES = true ]; then
        ARGS=""
    else
        ARGS=" -dispose none -delay 0 $IMG_DIR/face.png -dispose previous "
    fi
    ARGS+=" -delay $DELAY "

    for i in {0..59}; do
        ARGS+=" $fnbase$(printf %02d $i).png "
    done;

    # end with 00 led
    ARGS+=" $fnbase"
    ARGS+="00.png "

    # last arg is output file name
    if [ "$direction" = "-r" ]; then
        #ARGS+=" -delay $OFF_TIME $IMG_DIR/face.png "
        ARGS+=" $SHUTDOWN_FNAME "
    else
        ARGS+=" $STARTUP_FNAME "
    fi

    #echo convert\ $ARGS
    convert $ARGS
}


mkdir -p $IMG_DIR
if [ -n "$1" ]; then
    if [ "$1" = "-f" ]; then
        rm -f $STARTUP_FNAME $SHUTDOWN_FNAME
    fi
fi

if [ ! -f $STARTUP_FNAME ]; then
    swirl_miff;
    #echo "Creating forward swirl gif"
    #convert $STARTUP_FNAME $IMG_DIR/swirl_fwd.gif
fi

if [ ! -f $SHUTDOWN_FNAME ]; then
    swirl_miff -r;
    #echo "Creating reverse swirl gif"
    #convert $SHUTDOWN_FNAME $IMG_DIR/swirl_rev.gif
fi
