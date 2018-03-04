#!/bin/sh -e
socat file:/dev/tty,raw,echo=0,ignbrk=1 tcp:localhost:3333
