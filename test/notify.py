#!/usr/bin/python3

# Can be call with filter:
# python3 notify.py tcp://localhost:5563 text
# python3 notify.py tcp://localhost:5563 b


import sys
#import zmq
from nanomsg import Socket, SUB, SUB_SUBSCRIBE
import struct


with Socket(SUB) as socket:
    socket.connect ("tcp://localhost:6723")

    filter = ""
    if len(sys.argv) > 1:
        filter = sys.argv[1]
    socket.set_string_option(SUB, SUB_SUBSCRIBE, filter)
    while 1:
        res = socket.recv()
        if res[0] != 0x62:
            print(res.decode("utf-8"))
        else:
            gad = struct.unpack('>H', res[1:3])[0]
            print("%i/%i/%i: "%( ((gad >> 11) & 0x1f), (gad >> 8) & 0x07, gad & 0xFF), end="" )
            if res[3] == 1:
                # Signed
                val = struct.unpack('>q', res[4:])
                print(val[0])
            elif res[3] == 2:
                # Unsigned
                val = struct.unpack('>Q', res[4:])
                print(val[0])
            elif res[3] == 3:
                # Float
                val = struct.unpack('>d', res[4:])
                print(val[0])
            else:
                # UNKNOWN
                print("undef")
