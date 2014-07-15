#!/bin/bash

gcc -o test xmount_cache.c xmount_log.c xmount_options.c test.c -Wall -D_LARGEFILE64_SOURCE
