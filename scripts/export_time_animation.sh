#!/bin/bash
# use to combine pngs into an animated gif

IMG_DIR=./graphics/images

if [ -z "$1" ]; then
    echo "arg1 must be hour"
    exit 1
fi
if [ -z "$2" ]; then
    echo "arg2 must be minute"
    exit 1
fi

STARTUP_FNAME=./graphics/images/swirl_fwd.miff
if [ ! -f $STARTUP_FNAME ]; then
    DELAY=5     # units are 1/100th of a second (between frames)
    echo "Creating forward swirl"
    ARGS="-delay $DELAY "

    for i in {0..59}; do
        ARGS+="$IMG_DIR/swirl_fwd_$(printf %02d.png $((i))) "
    done;

    # last arg is output file name
    ARGS+=$STARTUP_FNAME

    #echo convert\ $ARGS
    convert $ARGS
fi

SHUTDOWN_FNAME=./graphics/images/swirl_rev.miff
if [ ! -f $SHUTDOWN_FNAME ]; then
    DELAY=1x50      # delay 1/50 seconds between frames
    echo "Creating reverse swirl"
    ARGS="-delay $DELAY "

    for i in {0..59}; do
        ARGS+="$IMG_DIR/swirl_rev_$(printf %02d.png $((i))) "
    done;

    # end with 00 led
    ARGS+="$IMG_DIR/swirl_rev_00.png "

    # last arg is output file name
    ARGS+=$SHUTDOWN_FNAME

    #echo convert\ $ARGS
    convert $ARGS
fi


HOUR=$1
MIN=$2
IMG_FNAME_BASE=$(printf %d%02d $HOUR $MIN)

HOUR_ANIM_FRAME_COUNT=$(($HOUR*5+$(($MIN*5))/60))

#echo "hour frame count is $(($HOUR_ANIM_FRAME_COUNT+1))"

HOUR_OUTPUT_FNAME=./graphics/images/$IMG_FNAME_BASE
HOUR_OUTPUT_FNAME+=_animated_hour.miff

echo "Converting startup to hour"
convert $STARTUP_FNAME[0-$HOUR_ANIM_FRAME_COUNT] $HOUR_OUTPUT_FNAME

MIN_OUTPUT_FNAME=./graphics/images/$IMG_FNAME_BASE
MIN_OUTPUT_FNAME+=_animated_min.miff
MINUTE_BLINK=40
BLINK_CNT=2
ARGS="-delay $MINUTE_BLINK"
ARGS+=" "
for i in $(seq 0 $BLINK_CNT); do
    # first image is last hour anim frame (hour without minute led)
    ARGS+=$IMG_DIR/$IMG_FNAME_BASE
    ARGS+="_min_off.png "
    # second image is complete time with minute
    ARGS+=$IMG_DIR/$IMG_FNAME_BASE
    ARGS+="_min_on.png "
done;

#show full time on for a bit
FULL_TIME_ON_DELAY=200
ARGS+="-delay $FULL_TIME_ON_DELAY "
ARGS+=$IMG_DIR/$IMG_FNAME_BASE
ARGS+="_min_on.png"
ARGS+=" "
#ARGS+="-loop $BLINK_CNT"
#ARGS+=" "
ARGS+=$MIN_OUTPUT_FNAME

#echo convert\ $ARGS
echo "Creating minute animation"
convert $ARGS

OUTPUT_FNAME=$IMG_DIR/$IMG_FNAME_BASE
OUTPUT_FNAME+="_anim.gif"
#combine hour and minute animations and loop
#echo convert\ $HOUR_OUTPUT_FNAME\ $MIN_OUTPUT_FNAME\ -loop\ 1000\ $OUTPUT_FNAME
echo "Joining animations"
convert $HOUR_OUTPUT_FNAME $MIN_OUTPUT_FNAME $SHUTDOWN_FNAME -loop 1 $OUTPUT_FNAME
