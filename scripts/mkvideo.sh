#!/bin/bash
# use avconv to make a video from a sequence of images
# arguments:
# ./scripts/mkvideo.sh [hour] [min]

if [ -z "$1" ]; then
    echo "arg1 must be hour"
    exit 1
elif [ -z "$2" ]; then
    echo "arg2 must be minute"
    exit 1
fi

hour=$1
min=$2

tname=$hour$(printf %02d $min)
IMGDIR=graphics/images
outname=$IMGDIR/$tname
outname+=.avi

mk_video() {
    imbase=$IMGDIR/$tname
    imbase+=_
    avconv \
        -framerate 25 -f image2 -start_number 1 -i $imbase%02d.png \
        -r 25 $outname
}

show_video() {
    mplayer $outname
}


mk_video;
retcode=$?
if [ "$retcode" -eq "0" ]; then
    show_video;
fi
