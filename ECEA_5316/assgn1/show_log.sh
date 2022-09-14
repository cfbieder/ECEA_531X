#!/bin/bash


ASSIGNMENT_NAME='seqgen'

if [ $# -lt 1 ]
then
  echo "Using default value ${ASSIGNMENT_NAME} for key"
else
  NUMFILES=$1
fi

echo "Extracting data for : ${ASSIGNMENT_NAME}"
grep ${ASSIGNMENT_NAME} /var/log/syslog > outfile.txt
