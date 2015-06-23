#!/bin/bash
# script to process images and convert them into a gif with multiple watch img
# ln this into the directory with all your images


find_min_size() {
    min_width=10000
    min_height=10000

    for file in $(ls *.JPG); do
        width=$(identify -format "%w" $file)
        height=$(identify -format "%h" $file)
        echo "$file is $width by $height px"
        if [ "$width" -lt "$min_width" ]; then
            min_width=$width
        fi
        if [ "$height" -lt "$min_height" ]; then
            min_height=$height
        fi
    done

    echo "Min width $min_width x min height $min_height"
}

crop_images() {
    crop_height=2500
    crop_width=1700
    resize_height=1000
    resize_width=680
    crop_dim=$(printf %dx%d+0+0 $crop_width $crop_height)
    resize_dim=$(printf %dx%d $resize_width $resize_height)
    img_num=0
    for file in $(ls *.JPG); do
        echo $((img_num+=1)) >> /dev/null
        newname=img_$(printf %03d $img_num).jpg
        convert \( $file -gravity center -crop $crop_dim +repage \) \
            -resize $resize_dim $newname
    done
}

rename_img() {
    bands="navy navy navy navy "
    bands+="br_leath br_leath br_leath br_leath br_leath "
    bands+="gr_leath gr_leath gr_leath gr_leath "
    bands+="pink pink pink pink "
    bands+="orange orange orange orange "
    bands+="red red red red "
    bands+="black "

    bkgs="tan gry red wht wht wht red tan gry gry tan red wht wht red "
    bkgs+=" tan gry gry tan red wht wht red tan gry gry "
    i=0
    for bkg in $bkgs; do
        echo $((i+=1)) >> /dev/null
        name=img_$(printf %03d $i).jpg
        newname=img_$(printf %s_%03d $bkg $i).jpg
        mv $name $newname
    done
}

join_images() {
    if [ -z "$1" ]; then
        width=3
    else
        width=$1
    fi

    color=$2            #tan gry wht red
    output_slides=15
    num_img=$(ls img_$color*.jpg | wc -l)
    #echo $num_img
    for slide in $( seq 1 $output_slides); do
        slide_no=$(($(ls slides/ | grep slide_ | wc -l)+1))
        outname=slides/slide_$(printf %03d $slide_no).jpg
        imgs=$(shuf -i 1-$num_img -n $width)
        ARGS="+append "
        for img in $imgs; do
            inname=$(ls img_$color* | head -$img | tail -1 )
            ARGS+=" $inname "
        done
        ARGS+=" $outname"
        convert $ARGS
    done
}

make_gif() {
    width_pic=3                         # number of images to use across
    timing=$(( 150 / $width_pic ))      # x / 100 sec (ideal)
    gif_img_cnt=0           # number of images used in final gif (or all w/ 0)
    loops=2

    if [ -n "$1" ]; then
        outname=$1
    else
        outname=out.gif
    fi

    if [ -n "$2" ]; then
        width_pic=$2
    fi

    width_px=680        # width of gif in pixels
    im_width=$(($width_px / $width_pic))
    final_width=$(($im_width * $width_pic))

    num_raw_img=$(ls img*.jpg | wc -l)
    if [ "$gif_img_cnt" -eq "0" ]; then
        gif_img_cnt=$num_raw_img
    fi
    imgs=$(shuf -i 1-$num_raw_img -n $gif_img_cnt)
    echo "Making a gif with these image numbers" $imgs

    # make gif
    ARGS=" -delay $timing "

    # make background (starting image)
    ARGS+=" ( ( +append "
    for i in $(seq 1 $width_pic); do
        next_img=$(echo $imgs | sed 's/\([^ ]*\) \(.*\)/\1/')
        imgs=$(echo $imgs | sed 's/\([^ ]*\) \(.*\)/\2/')
        inname=img_$(printf %03d $next_img).jpg
        ARGS+=" $inname "
    done
    ARGS+=" ) -resize $final_width ) "

    # add other images
    rep_order=""
    for i in $(seq 1 $(( $gif_img_cnt - $width_pic ))); do
        rep_order+="$(( $(shuf -i 1-$width_pic -n 1) - 1 )) "
    done
    echo "and this replacement order ment order" $rep_order
    for i in $(seq 1 $(( $gif_img_cnt - $width_pic ))); do
        next_img=$(echo $imgs | sed 's/\([^ ]*\) \(.*\)/\1/')
        imgs=$(echo $imgs | sed 's/\([^ ]*\) \(.*\)/\2/')
        page_num=$(echo $rep_order | sed 's/\([^ ]*\) \(.*\)/\1/')
        rep_order=$(echo $rep_order | sed 's/\([^ ]*\) \(.*\)/\2/')
        inname=img_$(printf %03d $next_img).jpg
        #echo -n " $page_num"
        #echo "Page number is $page_num"
        page=$(( $width_px * $page_num ))
        #echo "Page pixel is $page"
        page_arg=+$page+0
        ARGS+=" -page $page_arg ( $inname -resize $im_width ) "
        #ARGS+=" -page $width_px+$page+0 $inname "
    done

    ARGS+=" -loop $loops $outname "
    echo "Args: $ARGS"
    convert $ARGS
}

#find_min_size;
#crop_images;
#rename_img;
#join_images 4;
#join_images 3 tan;
#join_images 3 gry;
#join_images 3 wht;
#join_images 3 red;

gifname=gifs/gif_$(printf %03d $(( $(ls gifs | wc -l) + 1 ))).gif
make_gif $gifname 2;
gifname=gifs/gif_$(printf %03d $(( $(ls gifs | wc -l) + 1 ))).gif
make_gif $gifname 3;
gifname=gifs/gif_$(printf %03d $(( $(ls gifs | wc -l) + 1 ))).gif
make_gif $gifname 4;
gifname=gifs/gif_$(printf %03d $(( $(ls gifs | wc -l) + 1 ))).gif
make_gif $gifname 5;
if [ "$?" -eq 0 ]; then
    eog $gifname
fi
