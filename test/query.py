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
    res = socket.recv()
    if res[0] != 0x62:
        print(res.decode("utf-8"))
    else:
        if res[1] == 1:
            # Signed
            val = struct.unpack('<q', res[2:])
            print(val[0])
        elif res[1] == 2:
            # Unsigned
            val = struct.unpack('<Q', res[2:])
            if query.startswith("gad?"):
                gad = val[0]
                print("%d/%d/%d"%((gad >> 11) & 0x1f, (gad >> 8) & 0x07, (gad) & 0xff));
            else:
                print(val[0])
        elif res[1] == 3:
            # Float
            val = struct.unpack('<d', res[2:])
            print(val[0])
        else:
            # UNKNOWN
            print("undef")
