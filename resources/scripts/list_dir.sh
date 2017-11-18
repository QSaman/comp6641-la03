 #!/bin/bash

 echo -e "GET / HTTP/1.0\r\nHost:starwars.com\r\n\r\n" | ncat localhost 7777
 echo -e "POST /malak.txt HTTP/1.0\r\nHost:starwars.com\r\nContent-Length: 5\r\n\r\nmalak" | ncat localhost 7777
