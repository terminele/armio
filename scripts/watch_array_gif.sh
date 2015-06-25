#!/bin/bash
# script to process images and convert them into a gif with multiple watch img
# ln this into the directory with all your images


find_min_size() {
    min_width=10000
    min_height=10000

    for file in $(ls originals/*.JPG); do
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
    img_num=$(ls | grep img_ | wc -l)
    for file in $(ls originals/*.JPG); do
        echo $((img_num+=1)) >> /dev/null
        newname=img_$(printf %03d $img_num).jpg
        convert \( $file -gravity center -crop $crop_dim +repage \) \
            -resize $resize_dim $newname
    done
}

reorder_images() {
    i=0
    for file in $(ls img*.jpg); do
        echo $((i+=1)) >> /dev/null
        newname=img_$(printf %03d $i).jpg
        if [ "$file" != "$newname" ]; then
            mv $file $newname
        fi
    done
}

rename_img() {
    bkgs= " gry red wht wht gry red wht wht gry gry "
    bkgs+=" tan red wht wht tan gry gry gry red wht "
    bkgs+=" wht red tan gry gry tan red wht wht red "
    bkgs+=" tan gry gry tan red wht wht red tan gry "
    bkgs+=" tan red wht red gry gry tan wht red red "
    bkgs+=" wht wht red tan gry gry red wht red gry "
    bkgs+=" gry wht red tan tan red wht wht gry gry "
    bkgs+=" tan red tan tan red gry gry red tan red "
    bkgs+=" gry gry red tan tan red gry red tan tan "
    bkgs+=" red gry gry red tan tan gry red tan tan "
    bkgs+=" red gry wht gry tan red red tan gry wht "
    bkgs+=" wht gry tan red red tan wht "
    i=0
    for bkg in $bkgs; do
        echo $((i+=1)) >> /dev/null
        name=img_$(printf %03d $i).jpg
        newname=img_$(printf %s_%03d $bkg $i).jpg
        mv $name $newname
    done
}

next_slide_name() {
    slidename=slides/slide_$(printf %03d $(( $(ls slides/ | grep slide_ | tail -1 | sed 's/slide_0*\(.*\).jpg/\1/') + 1 ))).jpg
    echo $slidename
}

join_images() {
    if [ -z "$1" ]; then
        width=3
    else
        width=$1
    fi
    color=$2            #tan gry wht red

    outname=$(next_slide_name)
    if [[ "$1" == *" "* ]]; then
        imgs=$1
    else
        num_img=$(ls img_$color*.jpg | wc -l)
        imgs=$(shuf -i 1-$num_img -n $width)
    fi


    ARGS=" +append "
    ARGS+=" ( "
    width_pic=0
    for img in $imgs; do
        echo $((width_pic+=1)) >> /dev/null
        inname=$(ls img_$color* | head -$img | tail -1 )
        ARGS+=" $inname "
    done
    width_px=680        # width of gif in pixels
    im_width=$(($width_px / $width_pic))
    final_width=$(($im_width * $width_pic))
    ARGS+=" ) -resize $final_width "
    ARGS+=" $outname"
    convert $ARGS
}

add_time() {
    img=img_003.jpg
    outfile=out.jpg
    tme="142"
    ARGS=" ( $img "
    #ARGS+=" -resize 680x1100> -size 680x1100 xc:white +swap \
    #    -gravity north -composite ) -fill blue -gravity center -pointsize 100 label:$tme $outfile"
    ARGS+=" -resize 680x1100> -size 680x1100 xc:white +swap \
        -gravity north -composite ) \
        -pointsize 60 -fill green \
        -draw \"text 10,290 $tme\"  "
    #-crop 1280x720+1200+1250 \
    convert $img \
        -pointsize 50 -fill black \
        -draw "text 15,50 '1:42'" $outfile
    #echo $ARGS
    #convert $ARGS
}

make_slides() {
    mkdir -p slides
    rm slides/*
    join_images "8 52 7";       # black blue
    join_images "8 67 7";       # clear blue
    #join_images "1 5 12 13";    # gray bkg, brass
    join_images "3 4 7 15";     # white bkg, brass
    join_images "11 53 23 115 31"    # 16 (light pink strap) white bkg, black al
    #join_images "8 114 28 51 20"     # 19 (light pink strap) gray bkg, black al
    join_images "66 76 82 109 97"      # gray bkg, clear al 87
    join_images "24 32 47 57 102"                       # black band, white bkg
    #join_images "27 35 50 56 103"                       # black band, gray bkg
    #join_images 3 tan;
    #join_images 3 gry;
    #join_images 3 wht;
    #join_images 3 red;
}

next_gif_name() {
    gifname=gifs/gif_$(printf %03d $(( $(ls gifs/ | grep gif_ | tail -1 | sed 's/gif_0*\(.*\).gif.*/\1/') + 1 ))).gif
    echo $gifname
}

make_gif() {
    width_pic=3                         # number of images to use across
    gif_img_cnt=0           # number of images used in final gif (or all w/ 0)
    loops=1000

    if [ -n "$1" ]; then
        outname=$1
    else
        outname=out.gif
    fi

    if [ -n "$2" ]; then
        width_pic=$2
    fi
    timing=$(( 300 / $width_pic ))      # x / 100 sec (ideal)

    width_px=680        # width of gif in pixels
    im_width=$(($width_px / $width_pic))
    final_width=$(($im_width * $width_pic))

    num_raw_img=$(ls img*.jpg | wc -l)
    if [ "$gif_img_cnt" -eq "0" ]; then
        gif_img_cnt=$num_raw_img
    fi
    imgs=$(shuf -i 1-$num_raw_img -n $gif_img_cnt)
    txtname=$outname
    txtname+=".txt"
    echo "gif timing is $timing" >$txtname
    echo "Making $gifname with these image numbers" $imgs >>$txtname

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
    echo "and this replacement order ment order" $rep_order >>$txtname
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
    #echo "Args: $ARGS"
    convert $ARGS
}


mkdir -p gifs

#find_min_size;
#crop_images;
#reorder_images;
make_slides;

#add_time;
gifname=$(next_gif_name)
#echo $gifname
make_gif $gifname 5;
if [ "$?" -eq 0 ]; then
    eog $gifname
fi
