#!/bin/sh

autoreconf --verbose --force --install --make || {
 echo 'autogen.sh failed';
 exit 1;
}

./configure || {
 echo 'configure failed';
 exit 1;
}

echo
echo "Now type 'make' to compile this module."
echo
