#!/bin/bash

gnome-terminal -- taskset -c 0 ./store
gnome-terminal -- taskset -c 0 ./filter

