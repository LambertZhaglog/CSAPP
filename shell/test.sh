#!/bin/bash
make;
for((i=10;i<17;i=i+1))
do

    make "rtest${i}"
    make "test${i}"
done
