#!/bin/sh


read_map_file () {
while read MAP_LINE
do
  if [ `expr substr "$MAP_LINE" 1 1` = '%' ]; 
  then
    echo $MAP_LINE
  else
    set -- $MAP_LINE
    TEX_NAME=$1
    ENCODING=$2 
    PFB=$4 # May be empty or "default"
# If no AFM file explicitly specified, use font name
    if [ -z "$3" -o "default" = "$3" ];
    then
       AFM_NAME=$TEX_NAME
    else
       AFM_NAME=$3
    fi       
# Try to find the AFM file    
    AFM_FILE=`kpsewhich $AFM_NAME`
    if [ -z "$AFM_FILE" ];
    then
       AFM_FILE=`kpsewhich $AFM_NAME.afm`
    fi
# If we got one, process it    
    if [ -z "$AFM_FILE" ];
    then
      echo "****** NO AFM FILE for $TEX_NAME/$3 *****" 1>&2
      echo "Setting PS name for $TEX_NAME to $TEX_NAME (This probably isn't right)" 1>&2
      FONTNAME=$TEX_NAME
    else
      set -- `grep FontName $AFM_FILE`
      FONTNAME=$2
    fi  
    echo $TEX_NAME $FONTNAME $ENCODING $PFB
  fi
done  
}


if [ "$1" = "" ];
then
   echo Usage:  $0 mapfile
   exit 1
fi   

read_map_file < $1


