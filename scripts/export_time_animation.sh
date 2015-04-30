#!/bin/bash
# use to combine pngs into an animated gif

IMG_DIR=./graphics/images/

HOUR=$1
MIN=$2
IMG_FNAME_BASE=$1_$2
if [ -z "$1" ]
then
    echo "arg1 must be hour"
    exit 1
fi
if [ -z "$2" ]
then
    echo "arg2 must be minute"
    exit 1
fi

let HOUR_ANIM_FRAME_COUNT=$HOUR*5-1

echo "hour frame count is $HOUR_ANIM_FRAME_COUNT"

HOUR_OUTPUT_FNAME=./graphics/images/animated_hour_$IMG_FNAME_BASE.gif
SNAKE_DELAY=5

ARGS+="-delay 20 "
ARGS+=$IMG_DIR
ARGS+="face.png"
ARGS+=" -delay $SNAKE_DELAY "

for i in $( seq 0 $HOUR_ANIM_FRAME_COUNT); do
    ARGS+=" "
    ARGS+=$IMG_DIR
    ARGS+=$IMG_FNAME_BASE
    ARGS+="_"
    ARGS+=$i
    ARGS+=".png"
done;

#last arg is output file name
ARGS+=" "
ARGS+=$HOUR_OUTPUT_FNAME
echo $ARGS

convert $ARGS

MIN_OUTPUT_FNAME=./graphics/images/animated_min_$IMG_FNAME_BASE.gif
MINUTE_BLINK=40
BLINK_CNT=2
CMD="convert -delay $MINUTE_BLINK"
CMD+=" "
for i in $(seq 0 $BLINK_CNT); do
    #first image is last hour anim frame (hour without minute led)
    CMD+=$IMG_DIR$IMG_FNAME_BASE
    CMD+="_"
    CMD+=$HOUR_ANIM_FRAME_COUNT
    CMD+=".png"
    CMD+=" "
    #second image is complete time with minute
    CMD+=$IMG_DIR$IMG_FNAME_BASE
    CMD+=".png"
    CMD+=" "
done;

#show full time on for a bit
FULL_TIME_ON_DELAY=500
CMD+="-delay $FULL_TIME_ON_DELAY "
CMD+=$IMG_DIR$IMG_FNAME_BASE
CMD+=".png"
CMD+=" "
#CMD+="-loop $BLINK_CNT"
#CMD+=" "
CMD+=$MIN_OUTPUT_FNAME
echo $CMD
$CMD

OUTPUT_FNAME=$IMG_DIR
OUTPUT_FNAME+=$IMG_FNAME_BASE
OUTPUT_FNAME+="_anim.gif"
#combine hour and minute animations and loop
convert $HOUR_OUTPUT_FNAME $MIN_OUTPUT_FNAME -loop 1000 $OUTPUT_FNAME
