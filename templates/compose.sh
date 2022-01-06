#!/bin/bash

out_dir=`pwd`/../data
header=`pwd`/_header.template
footer=`pwd`/_footer.template

#
# go through all templates and append/prepend header and footer
#

date +"%H:%M:%S"

for file in *.template;
do
  if [[ $file != \_* ]];
  then
    name=${file%.*}
    out_file=$out_dir/$name.html
    cat $header $file $footer > $out_file
    echo "$name --> $out_file" `du -b $out_file  | cut -f1`
  fi
done
