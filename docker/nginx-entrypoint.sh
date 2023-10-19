#!/bin/sh

service nginx start

# Check if Nginx is running and restart it if necessary
while true; do
    if ! service nginx status >/dev/null 2>&1; then
        service nginx start
    fi
    sleep 60
done