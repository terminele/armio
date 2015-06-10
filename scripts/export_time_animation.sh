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

HOUR=$1
MIN=$2
IMG_FNAME_BASE=$(printf %d%02d $HOUR $MIN)

let HOUR_ANIM_FRAME_COUNT=$(($HOUR*5-4+$(($MIN*5))/60))

#echo "hour frame count is $(($HOUR_ANIM_FRAME_COUNT+1))"

HOUR_OUTPUT_FNAME=./graphics/images/animated_hour_$IMG_FNAME_BASE.gif
SNAKE_DELAY=5

ARGS+="-delay 20 $IMG_DIR/face.png"
ARGS+=" -delay $SNAKE_DELAY "

for i in $( seq 0 $HOUR_ANIM_FRAME_COUNT); do
    ARGS+=$IMG_DIR/$IMG_FNAME_BASE
    ARGS+="$(printf _%02d.png $i) "
done;

#last arg is output file name
ARGS+=" "
ARGS+=$HOUR_OUTPUT_FNAME

#echo convert\ $ARGS
convert $ARGS

MIN_OUTPUT_FNAME=./graphics/images/animated_min_$IMG_FNAME_BASE.gif
MINUTE_BLINK=40
BLINK_CNT=2
ARGS="-delay $MINUTE_BLINK"
ARGS+=" "
for i in $(seq 0 $BLINK_CNT); do
    #first image is last hour anim frame (hour without minute led)
    ARGS+=$IMG_DIR/$IMG_FNAME_BASE
    ARGS+="_"
    ARGS+=$(printf %02d $HOUR_ANIM_FRAME_COUNT)
    ARGS+=".png"
    ARGS+=" "
    #second image is complete time with minute
    ARGS+=$IMG_DIR/$IMG_FNAME_BASE
    ARGS+=".png"
    ARGS+=" "
done;

#show full time on for a bit
FULL_TIME_ON_DELAY=200
ARGS+="-delay $FULL_TIME_ON_DELAY "
ARGS+=$IMG_DIR/$IMG_FNAME_BASE
ARGS+=".png"
ARGS+=" "
#ARGS+="-loop $BLINK_CNT"
#ARGS+=" "
ARGS+=$MIN_OUTPUT_FNAME

#echo convert\ $ARGS
convert $ARGS

OUTPUT_FNAME=$IMG_DIR/$IMG_FNAME_BASE
OUTPUT_FNAME+="_anim.gif"
#combine hour and minute animations and loop
#echo convert\ $HOUR_OUTPUT_FNAME\ $MIN_OUTPUT_FNAME\ -loop\ 1000\ $OUTPUT_FNAME
convert $HOUR_OUTPUT_FNAME $MIN_OUTPUT_FNAME -loop 1000 $OUTPUT_FNAME
