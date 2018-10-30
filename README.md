# KnxCached
## PRE-VERSION
Linux daemon, manage a cache to Knx object, and provide a SCPI interface to managed objects.
 
# Requirement
This daemon need knxd (libeibclient.so need to build, and knxd daemon need at runtime)

# Compile
```bash
mkdir build
cd build
cmake ..
make
```
# Principle, and difference with linknx
It can be compared to linknx project, but, the goal of this one is to use a more simple programming approach.
knxcached only depends to libeibclient; it is writing in a pure c++11 and only for linux. (and optinnaly openssl)
It's goal is only a object proxy; it doesn't do data logging or rules.
For this tasks, there will be another daemon connected to knxcached.
The first will be knxlogerd, in order to store all change in a database.
The second will be knxruled; in order to write rules a QML language (QML binding is a very good approach to write rules).
All the configuration will be in sources files; not in database, and not generated by a web interface.

# Usage
First, you need to generate a configuration file.


/etc/knxcached.conf
```
# Main server port
server_port=6722

# KNXD configuration
knxd_url=ip:127.0.0.1

# SSL Port configuration
ssl_server_port=6723
ssl_cert_file=/etc/knxcached/server.cert
ssl_private_key_file=/etc/knxcached/server.key
ssl_private_key_password=<PASSWORD>
ssl_ca_file=/etc/knxcached/ca.cert.pem


# Client configuration (for python scrypt)
ssl_client_private_key_file=/etc/knxcached/client.key
ssl_client_private_key_password=<PASSWORD>
ssl_client_cert_file=/etc/knxcached/client.cert
client_decode_file=/etc/knxcached/devices.xml
```


{{{
openssl genrsa -des3 -out ca.key 2048
openssl genrsa -des3 -out server.key 2048

openssl req -x509 -new -nodes -key ca.key -sha256 -days 7000 -out ca.cert.pem
openssl req -new -key server.key -out server.csr
openssl x509 -req -in server.csr -CA ca.cert.pem -CAkey ca.key -CAcreateserial -out server.cert -days 6000 -sha256
}}}


{{{
openssl genrsa -des3 -out client.key 2048
openssl req -new -key client.key -out client.csr
openssl x509 -req -in client.csr -CA ca.cert.pem -CAkey ca.key -CAcreateserial -out client.cert -days 6000 -sha256
./GenConfig.py Project.knxproj > /etc/knxcached/devices.xml
}}}


