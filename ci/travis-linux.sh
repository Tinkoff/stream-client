#!/bin/bash

sudo apt install -y build-essential cmake libssl-dev libboost-all-dev python-dev

wget https://github.com/boostorg/beast/archive/v124.tar.gz
tar -xf v124.tar.gz
cd beast-124/
sudo cp -r include/boost/ /usr/include/
cd ..
