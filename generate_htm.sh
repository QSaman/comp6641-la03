#!/bin/bash
pandoc -s -S --toc -c doc_resources/pandoc.css -A doc_resources/footer.html ReadMe.md -o ReadMe.htm 
