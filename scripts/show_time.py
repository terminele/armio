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

IMGDIR = [".", "graphics", "images"]
FN_FACE = "face.png"
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
    led_index = int( i ) % 60
    led = QImage( FP_LED_BASE + "{0:02}.png".format(i) )
    if led.isNull():
        sys.stderr.write( "Failed to read image for led %i\n" % i )
        sys.exit( 1 )
    return led

def paint_snake( painter, pos = 0, length = 5, reverse=False ):
    """ paint an led at pos with a tail of length with direction
        set by reverse
    """
    opacity = 1
    dim_amt = 1.0 / length
    for i in range( length ):
        led_index = pos + i if reverse else pos - i
        led = get_led( led_index )
        painter.setOpacity( opacity )
        painter.drawImage( 0, 0, led )
        opacity -= dim_amt

    return painter

def paint_time( h, m ):
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

    # draw the hour LED's
    length = m*5 // 60 + 1
    paint_snake( painter, h*5 + length, length )

    painter.end()

    return watch

def show_time( h, m ):
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
    imgcnt = 0
    watch = QImage( FP_FACE )
    if watch.isNull():
        sys.stderr.write( "Failed to read background image: %s\n" % FP_FACE )
        sys.exit( 1 )

    num_images=(h*5-4) + (m*5)//60+1
    print("creating {} images for animation".format(num_images))
    ###draw hour animation pixmaps
    for i in range(h*5 - 4):
        # configure a painter for the 'watch' image
        watch = QImage( FP_FACE )
        painter = QPainter()
        painter.begin( watch )
        paint_snake( painter, i, 5 )
        painter.end()
        pixmap = QPixmap.fromImage( watch )
        imname="{0}{1:02}_{2:02}".format(h, m, imgcnt)
        save_pixmap( pixmap, imname )
        #print("creating image {}".format( imname ))
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

        imname="{0}{1:02}_{2:02}".format(h, m, imgcnt)
        save_pixmap(pixmap, imname)
        #print("creating image {}".format( imname ))
        imgcnt+=1

    ###draw wrap up animation
    if 0:
        for i in range(56):
            # configure a painter for the 'watch' image
            watch = QImage( FP_FACE )
            painter = QPainter()
            painter.begin( watch )
            paint_snake( painter, i, 5, reverse=True )
            painter.end()
            pixmap = QPixmap.fromImage(watch)
            imname="{0}{1:02}_{2:02}".format(h, m, imgcnt)
            save_pixmap(pixmap, imname)
            #print("creating image {}".format( imname ))
            imgcnt+=1


    pixmap = QPixmap.fromImage( paint_time(h, m) )
    save_pixmap(pixmap, "{0}{1:02}".format(h, m))


class ImagePlayer( QWidget ):
    def __init__( self, filename, title, parent=None ):
        QWidget.__init__(self, parent)

        # Load the file into a QMovie
        self.movie = QMovie( filename, QByteArray(), self )

        size = self.movie.scaledSize()
        self.setGeometry( 600, 600, size.width(), size.height() )
        self.setWindowTitle( title )

        self.movie_screen = QLabel()
        # Make label fit the gif
        self.movie_screen.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Expanding)
        self.movie_screen.setAlignment(Qt.AlignCenter)

        # Create the layout
        main_layout = QVBoxLayout()
        main_layout.addWidget(self.movie_screen)

        self.setLayout(main_layout)

        # Add the QMovie object to the label
        self.movie.setCacheMode(QMovie.CacheAll)
        self.movie.setSpeed(100)
        self.movie_screen.setMovie(self.movie)
        self.movie.start()


if __name__ == "__main__":
    import subprocess

    try:
        h, m = [ int(i) for i in sys.argv[1].split( ":" ) ]
        show = False
    except:
        h, m = [ int(time.localtime().tm_hour), int(time.localtime().tm_min) ]
        show = True

    h = h % 12

    print("Show {0}:{1:02}".format(h, m))

    if not os.path.exists( FP_FACE ):
        print("running ./scripts/export_led_img.sh to generate led images")
        subprocess.call( "./scripts/export_led_img.sh" )
        #sys.exit()

    app = QApplication( sys.argv )

    qpaint( h, m )
    print("creating gif using ./scripts/export_time_animation.sh")
    subprocess.call( ["./scripts/export_time_animation.sh", str(h), str(m)] )

    gif = "./graphics/images/{0}{1:02}_anim.gif".format(h, m)
    player = ImagePlayer( gif, "Showing {0}:{1:02}".format( h, m ) )
    player.show()

    QTimer.singleShot( 10e3, app.exit )
    sys.exit( app.exec_() )
