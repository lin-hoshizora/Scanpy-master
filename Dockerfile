FROM ubuntu:18.04

RUN adduser --quiet --disabled-password qtuser

RUN apt update && apt install -y python3 python3-dev python3-pyqt5 make g++ gcc git build-essential libboost-all-dev python3-pip
RUN pip3 install opencv-python numpy