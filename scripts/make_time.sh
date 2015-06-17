#!/bin/bash
# use to combine animations into a gif representing the watch animation

if [ -z "$1" ]; then
    HR=$(( $(date +%H) % 12 ))
    MIN=$(echo $(date +%M) | sed 's/^0//')
elif [ -z "$2" ]; then
    HR=$1
    MIN=$(echo $(date +%M) | sed 's/^0//')
else
    HR=$1
    MIN=$2
fi

IMG_DIR=./graphics/images
STARTUP_FNAME=./graphics/images/swirl_fwd.miff
SHUTDOWN_FNAME=./graphics/images/swirl_rev.miff
IMG_FNAME_BASE=$(printf %d%02d $HR $MIN)

HOUR_OUTPUT_FNAME=./graphics/images/$IMG_FNAME_BASE
HOUR_OUTPUT_FNAME+=_anim_hour.miff

MIN_OUTPUT_FNAME=./graphics/images/$IMG_FNAME_BASE
MIN_OUTPUT_FNAME+=_anim_min.miff

OUTPUT_FNAME=$IMG_DIR/$IMG_FNAME_BASE
OUTPUT_FNAME+=".gif"

# set USE_FACE_IN_ALL_FRAMES to true to embed the watch face in all frames and
# anything other than true to only embed the face in the first frame
USE_FACE_IN_ALL_FRAMES=false

OFF_TIME=200                    # time off after reverse swirl

if [ "$USE_FACE_IN_ALL_FRAMES" = "true" ]; then
    INITIAL_BKG_IMAGES=0
else
    INITIAL_BKG_IMAGES=1
fi

clip_hr_animation() {
    # ensure that the swirl forward & swirl reverse animations exist
    ./scripts/make_swirls.sh

    echo "Converting start swirl to hour animation"

    HOUR_ANIM_LAST_FRAME=$(( $HR * 5 + ( ($MIN * 5) / 60 ) + $INITIAL_BKG_IMAGES ))
    #echo "Hour frame count is $(($HOUR_ANIM_LAST_FRAME+1))"

    convert $STARTUP_FNAME[0-$HOUR_ANIM_LAST_FRAME] $HOUR_OUTPUT_FNAME
}

create_minute_animation() {
    ./scripts/make_face.sh $HR $MIN     # create hr / min face patterns

    FULL_TIME_ON_DELAY=200
    MINUTE_BLINK=20
    BLINK_CNT=2

    if [ $USE_FACE_IN_ALL_FRAMES = true ]; then
        ARGS=""
    else
        ARGS=" -dispose none -delay 0 $IMG_DIR/face.png -dispose previous "
    fi
    ARGS+=" -delay $MINUTE_BLINK "

    for i in $(seq 0 $BLINK_CNT); do
        ARGS+=" $IMG_DIR/$IMG_FNAME_BASE"
        ARGS+="_min_off.png "

        #show full time on for a bit
        if [ $i -eq $BLINK_CNT ]; then
            ARGS+=" -delay $FULL_TIME_ON_DELAY "
        fi

        ARGS+=" $IMG_DIR/$IMG_FNAME_BASE"
        ARGS+="_min_on.png "
    done;

    ARGS+=" $MIN_OUTPUT_FNAME "

    #echo convert\ $ARGS
    echo "Creating minute animation"
    convert $ARGS
}

combine_animations() {
    MIN_LAST_FRAME=$(identify $MIN_OUTPUT_FNAME | tail -1 | sed 's/.*\[\(.*\)\].*/\1/')
    CLOSE_LAST_FRAME=$(identify $SHUTDOWN_FNAME | tail -1 | sed 's/.*\[\(.*\)\].*/\1/')

    ARGS=" $HOUR_OUTPUT_FNAME "
    ARGS+=" $MIN_OUTPUT_FNAME[1-$MIN_LAST_FRAME] "
    ARGS+=" $SHUTDOWN_FNAME[1-$CLOSE_LAST_FRAME] "

    ARGS+=" -delay $OFF_TIME $IMG_DIR/led_clear.png "

    ARGS+=" -loop 10 "
    ARGS+=" $OUTPUT_FNAME "

    echo "Joining animations"
    convert $ARGS
}

mkdir -p $IMG_DIR
clip_hr_animation;
create_minute_animation;
combine_animations;

if [ $? -eq 0 ]; then
    eog $OUTPUT_FNAME
fi
