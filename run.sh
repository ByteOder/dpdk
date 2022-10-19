# created by Alan at 2022-09-30 16:31

#! /bin/bash

# set varibles
WORK_PATH=`pwd`
DEMO_PATH=${WORK_PATH}/build/examples
APPS_PATH=${WORK_PATH}/build/app
PROG_PATH=

# validate params
if [ $# -le 1 ];
then
	echo '[Usage] ./run.sh <demo | app> <PROG> [ARGS]'
	echo '-------------------DEMO--------------------'
	cd ${DEMO_PATH} && ls dpdk-*
	echo '-------------------APPS--------------------'
	cd ${APPS_PATH} && ls dpdk-*
	cd ${WORK_PATH}
	exit
fi

# run demo or apps ?
if [ $1 == "demo" ];
then
	PROG_PATH=${DEMO_PATH}
else
	PROG_PATH=${APPS_PATH};
fi

# run without first args
${PROG_PATH}/${@: 2}
