#!/usr/bin/python3

import socket
import sys
import os.path
import sys
import ssl
import lxml.etree as etree

HOST = '127.0.0.1'    # The remote host
s = None
xml = None

config = {}

 
DPT_1_CODE = {
     1: ["Off", "On"],
     2: ["False", "True"],
     3: ["Disable", "Enable"],
     4: ["No ramp", "Ramp"],
     5: ["No alarm", "Alarm"],
     6: ["Low", "High"],
     7: ["Decrease", "Increase"],
     8: ["Up", "Down"],
     9: ["Open", "Close"],
    10: ["Stop", "Start"],
    11: ["Inactive", "Active"],
    12: ["Not inverted", "Inverted"],
    13: ["Start/Stop", "Cyclically"],
    14: ["Fixed", "Calculated"],
    15: ["no action (dummy)", "reset command (trigger)"],
    16: ["no action (dummy)", "acknowledge command (trigger)"],
    17: ["", "trigger"],
    18: ["not occupied", "occupied"],
    19: ["closed", "open"],
    21: ["OR", "AND"],
    22: ["scene A", "scene B"],
    23: ["shutter", "blind"],
}


DPT_5_SCALE_UNIT = {
    1: (100, "%"),
    3: (360, "°"),
    4: (255, "%"),
   10: (255, "")
}

DPT_9_UNIT = {
     1: "°C",
     2: "K",
     3: "K/h",
     4: "Lux",
     5: "m/s",
     6: "Pa",
     7: "%",
     8: "ppm",
    10: "s",
    11: "ms",
    20: "mV",
    21: "mA",
    22: "W/m²",
    23: "K/%",
    24: "kW",
    25: "l/h",
    26: "l/m²",
    27: "°F",
    28: "km/h"
}

DPT_13_UNIT = {
     1: "",
    10: "Wh",
    11: "VAh",
    12: "VARh",
    13: "kWh",
    14: "kVAh",
    15: "kVARh",
   100: "s" 
}

def DPT_9_DECODE(data):
    sign = (data & 0x8000) >> 15
    exp = (data & 0x7800) >> 11
    mant = data & 0x07ff
    if sign != 0:
        mant = -(~(mant - 1) & 0x07ff)
    value = (1 << exp) * 0.01 * mant
    return value


def decode_message(xml, msg):
    def decode_src(msg):
        return "%i.%i.%i"%((msg[3] >> 4) & 0x0F, msg[3] & 0x0F, msg[4])
    def decode_dest(msg):
        return "%i/%i/%i"%((msg[5] >> 3) & 0x1F, msg[5] & 0x07, msg[6])

    if msg[0] == 0xFE:
        l = msg[1]
        
        imsg = msg[0:l+3]
        omsg = msg[l+3:]
        
        if(imsg[2] & 0xF0 == 0x00):
            src = decode_src(imsg)
            dst = decode_dest(imsg)

            if imsg[2] == 0x00:
                print('KRD | ', end='')
            elif imsg[2] == 0x01:
                print('KRS | ', end='')
            elif imsg[2] == 0x02:
                print('KWR | ', end='')
            elif imsg[2] == 0x0A:
                print('KMW | ', end='')

            print("%s%s| "%(src, " "*(9 - len(src))), end='')
            print("%s%s"%(dst, " "*(7 - len(dst))), end='')
            
            decoded = False
                     
            if xml:
                ids = xml.xpath("//object[@gad=\"" + dst + "\"]/@id")
                if len(ids):
                    print("| ", end="")
                    print(ids[0], end="")
                    print(" "*(40-len(ids[0])), end="")
                else:
                    print("| ", end="")
                    print(" "*40, end="")
                
                dpts = xml.xpath("//object[@gad=\"" + dst + "\"]/@dpt")
                dpt = None
                if len(dpts):
                    dpt = dpts[0]
                
                if dpt:
                    tp = int(dpt.split(".")[0])
                    tpu = int(dpt.split(".")[1])
                    if tp == 1:
                        # Boolean
                        print("| ", end="")
                        print(DPT_1_CODE[tpu][imsg[7]], end="")
                        decoded = True
                    if tp == 2:
                        # Control + Boolean
                        print("| ", end="")
                        if imsg[7] & 0x2 == 2:
                            print("control ")
                        else:
                            print("no control ")
                        print(DPT_1_CODE[tpu][imsg[7] & 0x1], end="")
                        decoded = True
                    if tp == 5:
                        # UNSIGNED CHAR
                        print("| %i %s"%((DPT_5_SCALE_UNIT[tpu][0] * imsg[8]) / 255, DPT_5_SCALE_UNIT[tpu][1]),end="")
                        decoded = True
                    if tp == 9:
                        print("| ", end="")
                        print("%.02f"%DPT_9_DECODE(imsg[8] << 8 | imsg[9]), end='')
                        print(" %s"%DPT_9_UNIT[tpu], end='')
                        decoded = True
                    if tp == 13:
                        val = imsg[8] << 24 | imsg[9] << 16 | imsg[10] << 8 | imsg[11]
                        print("| %i %s"%(val, DPT_13_UNIT[tpu]), end='')
                        decoded = True


            if not decoded:
                if imsg[2] != 0x00:
                    s = "| [" + ":".join( [ "%02x"%s for s in imsg[7:] ] ) + "]"
                else:
                    s = "|"
                print(s, end='')
                print(" "*(50 - len(s)), end='')
            
            print("")

        if len(omsg):
            return decode_message(xml, omsg)
        
        return imsg[2]
    
    return 0


if __name__ == "__main__":
    with open("/etc/knxcached.conf") as conf:
        for line in conf:
            if line[0] == "#":
                continue
            tab = line.split("=")
            if len(tab) != 2:
                continue

            config[tab[0].strip()] = tab[1].strip()



    if (len(sys.argv) < 2) or (((sys.argv[1] == "QUERY") or (sys.argv[1] == "SUBSCRIBE")) and (len(sys.argv) != 3)) or ((sys.argv[1] == "SET") and (len(sys.argv) != 4)) :
        print("%s QUERY [<addr> | ALL]"%(sys.argv[0]))
        print("%s DMZ"%(sys.argv[0]))
        print("%s SUBSCRIBE <addr>"%(sys.argv[0]))
        print("%s SET <addr> <data>"%(sys.argv[0]))
        exit(1)

    if "ssl" in sys.argv[0]:
        context = ssl.create_default_context(ssl.Purpose.CLIENT_AUTH)
        context.verify_mode = ssl.CERT_REQUIRED
        context.load_cert_chain(
            certfile=config["ssl_client_cert_file"],
            keyfile=config["ssl_client_private_key_file"],
            password=config["ssl_client_private_key_password"])
        context.load_verify_locations(cafile="/etc/knxcached/ca.cert.pem")

        for res in socket.getaddrinfo(HOST, int(config["ssl_server_port"]), socket.AF_UNSPEC, socket.SOCK_STREAM):
            af, socktype, proto, canonname, sa = res
            try:
                sock = socket.socket(af, socktype, proto)
                s = context.wrap_socket(sock)
            except socket.error as msg:
                s = None
                continue
            try:
                s.connect(sa)
            except socket.error as msg:
                print(msg)
                s.close()
                s = None
                continue
            break
    else:
        for res in socket.getaddrinfo(HOST, int(config["server_port"]), socket.AF_UNSPEC, socket.SOCK_STREAM):
            af, socktype, proto, canonname, sa = res
            try:
                s = socket.socket(af, socktype, proto)
            except socket.error as msg:
                s = None
                continue
            try:
                s.connect(sa)
            except socket.error as msg:
                s.close()
                s = None
                continue
            break

    if s is None:
        print('could not open socket')
        sys.exit(1)

    # Receive HEADER
    data = s.recv(1024)

    if "client_decode_file" in config.keys():
        if os.path.exists(config["client_decode_file"]):
            xml = etree.parse(config["client_decode_file"])


    if sys.argv[1] == "QUERY":
        if sys.argv[2] == "ALL":
            data = bytes([0xFF, 0x00, 0x01]) # DUMP_CACHED
            s.sendall(data)
        else:
            taddr = sys.argv[2].split("/")
            data = bytes([0xFF, 0x02, 0x06, int(taddr[0]) << 3 | int(taddr[1]), int(taddr[2])]) # QUERY ADDR 
            s.sendall(data)
    elif sys.argv[1] == "DMZ":
        data = bytes([0xFF, 0x00, 0x04]) # SUBSCRIBE_DMZ
        s.sendall(data)
    elif sys.argv[1] == "SUBSCRIBE":
        taddr = sys.argv[2].split("/")
        data = bytes([0xFF, 0x02, 0x02, int(taddr[0]) << 3 | int(taddr[1]), int(taddr[2])]) # QUERY ADDR 
        s.sendall(data)
    elif sys.argv[1] == "SET":
        taddr = sys.argv[2].split("/")
        data = bytes([0xFF, 0x02, 0x07, int(taddr[0]) << 3 | int(taddr[1]), int(taddr[2])]) # QUERY ADDR 
        for b in sys.argv[3].split(":"):
            data += bytes([int(b,16)])
        adata = bytearray(data)
        adata[1] = len(data) - 3
        data = bytes(adata)
        print(data)
        s.sendall(data)
        exit(0)
    while(1):
        data = s.recv(1024)
        if len(data) == 0:
            break
        
        try:
            if decode_message(xml, data) == 0x81: # NOTIFY_EOT End of transmission
                break
        except Exception as err:
            print("FAIL TO DECODE %s (%s)"%(data, err))
    s.close()

