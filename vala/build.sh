#!/bin/bash

valac --pkg=gio-2.0 list.vala

valac -C --save-temps --pkg=gio-2.0 list.vala
