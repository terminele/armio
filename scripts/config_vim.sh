#!/bin/bash
# use to make ctags, cscope and taghighlight files

CTAGS=ctags-exuberant
TAGHL=~/.vim/bundle/taghighlight/plugin/TagHighlight/TagHighlight.py

echo "Write cscope files list"
echo "$(find . -name "*.[ch]")" > cscope.files
echo "$(find . -name "*.vhd")" >> cscope.files
echo "$(find . -name "*.py")" >> cscope.files

echo "Make cscope database"
cscope -Rkb -i cscope.files

echo "Make ctags list"
$CTAGS -R --exclude="*~" --exclude=".git"

echo "Make taghighlight files"
python $TAGHL --ctags-file tags --source-root .
