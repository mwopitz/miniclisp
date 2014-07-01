#!/bin/bash

TARBALL="projekt.tar"

tar cvf $TARBALL Makefile miniclisp.c util.h exploit.sh s.sh hexto32byte.c
scp $TARBALL team7@praksrv.sec.in.tum.de:.
