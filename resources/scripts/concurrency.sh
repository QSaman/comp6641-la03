#!/bin/bash

root_path="../httpfs_root/Star Wars"
url_path="Star%20Wars"
reader_file="Ben Kenobi vs Darth Vader - A New Hope [1080p HD]-sq51w34Hg9I.webm"
writer_file="Luke Learns Vader Is His Father [1080p]-GueBXRYVhe0.webm"
common_name="foo.webm"
port=8080
httpc="../../httpc"

echo "Renaming ${reader_file} to ${common_name}"
cp -a "${root_path}/${reader_file}" "${root_path}/${common_name}"

echo "Chaning ${common_name} through HTTP POST..."
${httpc} post -h "Content-Type: video/mp4" -f "${root_path}/${writer_file}" "http://localhost:${port}/${url_path}/${common_name}"
