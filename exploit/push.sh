#!/bin/bash

TARBALL="projekt.tar"

tar cvf $TARBALL Makefile miniclisp.c util.h exploit.sh
scp $TARBALL team7@praksrv.sec.in.tum.de:.
