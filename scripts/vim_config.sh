#!/bin/bash
# use to make ctags, cscope and taghighlight files

CTAGS=ctags
TAGHL=~/.vim/bundle/taghighlight/plugin/TagHighlight/TagHighlight.py

if hash cscope 2>/dev/null; then
    echo "Write cscope files list"
    rm -f cscope.files cscope.out
    echo "$(find . -name "*.[ch]")" > cscope.files
    echo "$(find . -name "*.vhd")" >> cscope.files
    echo "$(find . -name "*.py")" >> cscope.files

    echo "Make cscope database"
    cscope -Rkb -i cscope.files
fi

if hash $CTAGS 2>/dev/null; then
    echo "Make ctags list"
    rm -f tags
    $CTAGS -R --exclude="*~" --exclude=".git"
fi

if [ -e $TAGHL ]; then
    echo "Make taghighlight files"
    rm -f types_*.taghl
    python $TAGHL --ctags-file tags --source-root .
fi
