#!/bin/bash

TARBALL="projekt.tar"

tar cvf $TARBALL Makefile miniclisp.c util.h
scp $TARBALL team7@praksrv.sec.in.tum.de:.
