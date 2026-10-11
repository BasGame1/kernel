#!/bin/bash

ROOT_DIR=$(pwd)
CREDS_DIR=$ROOT_DIR/creds

if [ ! -d $CREDS_DIR ]; then
   mkdir -p $CREDS_DIR
fi
if [ -f $CREDS_DIR/key ]; then
   echo "Already a key file, exiting"
   exit
fi
generate_seed() {
   MONTH="${month:-$(date +%-m)}"
   DAY="${day:-$(date +%-d)}"
   TIME_H="${time_h:-$(date +%-H)}"
   TIME_M="${time_m:-$(date +%-M)}"
   TIME_S="${time_s:-$(date +%-S)}"
   PACKED_TIME=$(( (MONTH << 22) | (DAY << 17) | (TIME_H << 12) | (TIME_M << 6) | TIME_S ))
   BUILD_SALT=$(od -An -N4 -tu4 /dev/urandom | tr -d ' ')
   RAW_MAGIC=$(( (BUILD_SALT << 32) | PACKED_TIME ))
   if [[ $RAW_MAGIC =~ ^-.* ]]; then
      RAW_MAGIC="${RAW_MAGIC:1}"
   fi
   echo $RAW_MAGIC
}
make_key() {
   SEED=$(generate_seed)
   MAGIC=0x31377365636F6E6473
   KEY=$((SEED*MAGIC))
   echo "Do not loose the key"
   echo "$KEY" >> $CREDS_DIR/key
}
make_key
