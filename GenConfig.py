#!/usr/bin/env python3

import zipfile, copy, sys, os
from lxml import etree

if len(sys.argv) < 2 or len(sys.argv) > 4:
    print("Export knxproj address group into knxcached.xml file\n\t%s <knxfile> [knxcached_source] [<knxcached_output>]"%sys.argv[0])
    print("\t - If knxcached_output is not set, print on stdout")
    print("\t - If knxcached_output is - , modify knxcached_source file (and generate knxcached_source~ backup")
    exit(1)

def Datatype(name):
    n = name.split("-")
    if n[0] == "DPT":
        return "%s.xxx"%n[1]
    else:
        return "%i.%03i"%(int(n[1]),int(n[2]))

archive = zipfile.ZipFile(sys.argv[1], 'r')

# Extract Project file
projects = [ i for i in archive.filelist if i.filename.split("/")[-1] == "0.xml" ]
if len(projects) != 1:
    print("Problem when loading file, can't find only one 0.xml; please send this file to fabien@kazoe.org")
    exit(-1)

f = archive.open(projects[0].filename)
xml = etree.fromstring(f.read())
namespaces = {"knx":"http://knx.org/xml/project/13" ,"xsd":"http://www.w3.org/2001/XMLSchema" ,"xsi":"http://www.w3.org/2001/XMLSchema-instance"}

GAs = xml.xpath("//knx:GroupAddresses/knx:GroupRanges", namespaces=namespaces)[0]
address_mask = [[0xFFFF], [0xF800, 0x07FF], [0xF800, 0x0700, 0x00FF]]

def ffs(x):
    """Returns the index, counting from 0, of the
    least significant set bit in `x`.
    """
    return (x&-x).bit_length()-1

objectlist = []

def processRange(rng, lvl, name = []):
    names = copy.copy(name)
    new_names = rng.attrib['Name'].title().replace("/","").split()
    for i in names:
        for s in i:
            if s in new_names:
                new_names.remove(s)
    names.append(new_names)
    if "GroupRange" in rng.tag:
        for r in rng.getchildren():
            processRange(r,lvl + 1, names)
    elif "GroupAddress" in rng.tag:
        idname = "_".join([  "".join(s) for s in names])
        if "DatapointType" in rng.attrib.keys():
            datatype = Datatype(rng.attrib['DatapointType'])
        else:
            datatype = None
        addr = int(rng.attrib['Address'])
        addrs = []
        for mask in address_mask[lvl]:
            addrs.append("%i"%((addr & mask) >> ffs(mask)))
        gad = "/".join(addrs)
        objectlist.append( (idname, datatype, gad) )
    else:
        print("TYPE %s NOT SUPPORTED"%rng.tag)

for rng in GAs.getchildren():
    processRange(rng, 0)

objectlist.sort(key=lambda tup: tup[0])


parser = etree.XMLParser(remove_blank_text=True)
if len(sys.argv) > 2:
    knxcached = etree.parse(sys.argv[2], parser)
    parent = knxcached.xpath("/knxcached")[0]
else:
    template = u"""<?xml version='1.0' encoding='UTF-8'?>
<knxcached>
    <knxd url="ip:127.0.0.1" />
</knxcached>
"""
    knxcached = etree.fromstring(template.encode("utf-8"),parser)
    parent = knxcached.xpath("/knxcached")[0]


for src in objectlist:
    if src[1]:
        previous = knxcached.xpath("//object[@id='%s']"%src[0])
        if len(previous) == 1:
            current_object = previous[0]
        else:
            current_object = etree.SubElement(parent,"object")
        current_object.attrib["type"] = src[1]
        current_object.attrib["gad"] = src[2]
        current_object.attrib["id"] = src[0]

def getkey(elem):
    if elem.get('id'):
        s = elem.get('id').split("_")
        if s[0] in ("Blind", "Light", "Meas"):
            return s[2] + "_" + s[0] + "_" + s[1]
        else:
            return elem.get('id')
    return ""


parent[:] = sorted(parent, key=getkey)

if len(sys.argv) == 3:
    out = etree.tostring(knxcached, pretty_print=True, xml_declaration=True, encoding='UTF-8')
    for i in out.decode("utf-8").split("\n"):
        print(i)
    exit()

if len(sys.argv) > 3 and sys.argv[3] != "-":
    outfile = sys.argv[3]
elif len(sys.argv) > 2:
    os.rename(sys.argv[2], sys.argv[2] + "~")
    outfile = sys.argv[2]
else:
    out = etree.tostring(knxcached, pretty_print=True, xml_declaration=True, encoding='UTF-8')
    for i in out.decode("utf-8").split("\n"):
        print(i)
    exit()

knxcached.write(outfile, pretty_print=True, xml_declaration=True, encoding='UTF-8')
