#!/bin/bash

sudo docker run -it --rm -v /dev:/dev -v $(pwd):/home/Scanpy --privileged insurance-reader
