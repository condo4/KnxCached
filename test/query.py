#!/usr/bin/python3

# Can be call with filter:
# python3 notify.py tcp://localhost:5563 text
# python3 notify.py tcp://localhost:5563 b


import sys
#import zmq
from nanomsg import Socket, REQ
import struct


with Socket(REQ) as socket:
    socket.connect ("tcp://localhost:6722")
    query = " ".join(sys.argv[1:])
    print(query)
    socket.send(query)
    print(socket.recv())
