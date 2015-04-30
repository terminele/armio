#!/usr/bin/python
""" script for combining image files to make clock hr / min hands
"""

from __future__ import print_function
from __future__ import division
from __future__ import absolute_import
from __future__ import unicode_literals

try:
    from future_builtins import (ascii, filter, hex, map, oct, zip)
except:
    pass

import os
import Image
import time

import sys
from PyQt4.QtCore import *
from PyQt4.QtGui import *

IMGDIR = [".", "graphics", "led_export"]
FN_FACE = "background.png"
FN_LED_BASE = "led"


FP_FACE = os.path.join( *(IMGDIR + [ FN_FACE ]) )
FP_LED_BASE = os.path.join( *(IMGDIR + [ FN_LED_BASE ]) )


def broken_pilpaint( ):
    """ this is a test for using pil to overlay images and export.. apparently
    there is a problem when the background image uses transparency """
    FACE = Image.open( FP_FACE )
    LEDS = [ Image.open( FP_LED_BASE + "{}.png".format(i) ) for i in xrange(60) ]
    LED0 = LEDS[0].copy()
    base = FACE.copy()
    blend0 = Image.blend( FACE, LED0, 0 )
    #blend0.show()
    blend1 = Image.blend( FACE, LED0, 1 )
    base.paste( LED0, mask=0 )

def get_led( i ):
    led = QImage( FP_LED_BASE + "{}.png".format(i) )
    if led.isNull():
        sys.stderr.write( "Failed to read image for led %i\n" % i )
        sys.exit( 1 )
    return led

def paint_snake(painter, pos = 0, length = 5):

    opacity = 1
    for i in range(length):
        led = get_led(pos + length - i -1)
        painter.setOpacity(opacity)
        painter.drawImage(0,0, led)
        opacity-=0.2

    return painter

def paint_time( h=None, m=None ):

    if h is None:
        h = int(time.localtime().tm_hour)
    if m is None:
        m = int(time.localtime().tm_min)

    h = h % 12

    watch = QImage( FP_FACE )
    if watch.isNull():
        sys.stderr.write( "Failed to read background image: %s\n" % FP_FACE )
        sys.exit( 1 )

    # configure a painter for the 'watch' image
    painter = QPainter()
    painter.begin( watch )

    # draw the minute LED
    led = get_led( m )
    painter.setOpacity( 0.75 )
    painter.drawImage( 0, 0, led )

    length = m*5 // 60 + 1
    paint_snake(painter, h*5, length)
    # draw the hour LED's
#    h_fade = range( h * 5, h * 5 + (m * 5) // 60 + 1 )
#    for i in h_fade:
#        if i == m:
#            continue
#        led = get_led( i )
#        painter.setOpacity( (i - h_fade[0] + 1) / len( h_fade ) )
#        painter.drawImage( 0, 0, led )
#
    painter.end()

    return watch

def qpaint( h=None, m=None ):
    app = QApplication( sys.argv )
    label = QLabel( )
    label.setPixmap( QPixmap.fromImage( paint_time( h, m ) ) )
    label.setFixedSize( 600, 600 )
    label.showNormal( )
    sys.exit( app.exec_() )

def save_pixmap(pixmap, name):
    file = QFile("graphics/images/"+ name + ".png")
    pixmap.save(file, "PNG")

def qpaint( h=None, m=None ):
    app = QApplication( sys.argv )
    imgcnt = 0
    watch = QImage( FP_FACE )
    if watch.isNull():
        sys.stderr.write( "Failed to read background image: %s\n" % FP_FACE )
        sys.exit( 1 )


    ###draw hour animation pixmaps
    for i in range(h*5 - 4):
        # configure a painter for the 'watch' image
        watch = QImage( FP_FACE )
        painter = QPainter()
        painter.begin( watch )
        paint_snake( painter, i, 5 )
        painter.end()
        pixmap = QPixmap.fromImage(watch)
        save_pixmap(pixmap, "{}_{}_{}".format(h, m, imgcnt))
        imgcnt+=1

    ###draw hour 'growing' pixmaps
    final_len = m*5 // 60 + 1
    for i in range(final_len):
        watch = QImage( FP_FACE )
        painter = QPainter()
        painter.begin( watch )
        paint_snake( painter, h*5, i + 1 )
        painter.end()
        pixmap = QPixmap.fromImage(watch)

        save_pixmap(pixmap, "{}_{}_{}".format(h, m, imgcnt))
        imgcnt+=1


    pixmap = QPixmap.fromImage(paint_time(h, m))
    save_pixmap(pixmap, "{}_{}".format(h, m))


    QTimer.singleShot(1, app.exit)
    sys.exit( app.exec_() )

if __name__ == "__main__":
    if not os.path.exists( FP_FACE ):
        print("running ./scripts/export_led_img.sh to generate led images")
        import subprocess
        subprocess.call( "./scripts/export_led_img.sh" )
        #sys.exit()



    if len( sys.argv ) > 1:
        print( "Showing time at {}".format( sys.argv[1] ) )
        try:
            params = [ int(i) for i in sys.argv[1].split( ":" ) ]
        except:
            params = [ None ]
    else:
        params = [ None ]

    qpaint( *params )
