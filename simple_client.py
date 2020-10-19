#git tag: v1.1-pypi
""" contains a json object message passing server and client """
 
import json
import socket
import struct
import logging
import time

print("client")
 
logger = logging.getLogger("jsonSocket")
logger.setLevel(logging.DEBUG)
FORMAT = '[%(asctime)-15s][%(levelname)s][%(funcName)s] %(message)s'
logging.basicConfig(format=FORMAT)
 
class JsonSocket(object):
    def __init__(self, address='127.0.0.1', port=13669):
        self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.conn = self.socket
        self._timeout = None
        self._address = address
        self._port = port
 
    def sendObj(self, obj):
        msg = json.dumps(obj)
        if self.socket:
            frmt = "=%ds" % len(msg)
            packedMsg = struct.pack(frmt, bytes(msg.encode('utf-8')))
            # packedHdr = struct.pack('=I', len(packedMsg))
            print("packedMsg : ", packedMsg)
            # print("packedHdr : ", packedHdr)
            print("len(packedMsg) : ", len(packedMsg))
            # print("len(packedHdr) : ", len(packedHdr))
 
            # self._send(packedHdr)
            self._send(packedMsg)
 
    def _send(self, msg):
        sent = 0
        while sent < len(msg):
            sent += self.conn.send(msg[sent:])
 
    def _read(self, size):
        data = ''
        while len(data) < size:
            dataTmp = self.conn.recv(size-len(data))
            data += dataTmp
            if dataTmp == '':
                raise RuntimeError("socket connection broken")
        return data
 
    def _msgLength(self):
        d = self._read(4)
        s = struct.unpack('=I', d)
        return s[0]
 
    def readObj(self):
        # size = self._msgLength()
        # data = self._read(1024)
        data = self.conn.recv(1024)
        frmt = "=%ds" % len(data)
        msg = struct.unpack(frmt, data)
        return json.loads(msg[0])
 
    def close(self):
        logger.debug("closing main socket")
        self._closeSocket()
        if self.socket is not self.conn:
            logger.debug("closing connection socket")
            self._closeConnection()
 
    def _closeSocket(self):
        self.socket.close()
 
    def _closeConnection(self):
        self.conn.close()
 
    def _get_timeout(self):
        return self._timeout
 
    def _set_timeout(self, timeout):
        self._timeout = timeout
        self.socket.settimeout(timeout)
 
    def _get_address(self):
        return self._address
 
    def _set_address(self, address):
        pass
 
    def _get_port(self):
        return self._port
 
    def _set_port(self, port):
        pass
 
    timeout = property(_get_timeout, _set_timeout,doc='Get/set the socket timeout')
    address = property(_get_address, _set_address,doc='read only property socket address')
    port = property(_get_port, _set_port,doc='read only property socket port')


#git tag: v1.1-pypi
class JsonServer(JsonSocket):
    def __init__(self, address='127.0.0.1', port=5489):
        super(JsonServer, self).__init__(address, port)
        self._bind()
 
    def _bind(self):
        self.socket.bind( (self.address,self.port) )
 
    def _listen(self):
        self.socket.listen(1)
 
    def _accept(self):
        return self.socket.accept()
 
    def acceptConnection(self):
        self._listen()
 
        self.conn, addr = self._accept()
        self.conn.settimeout(self.timeout)
        logger.debug("connection accepted, conn socket (%s,%d)" % (addr[0],addr[1]))


#git tag: v1.1-pypi
class JsonClient(JsonSocket):
    def __init__(self, address='127.0.0.1', port=13669):
        super(JsonClient, self).__init__(address, port)
 
    def connect(self):
        for i in range(10):
            try:
                self.socket.connect( (self.address, self.port) )
            except socket.error as msg:
                logger.error("SockThread Error: %s" % msg)
                time.sleep(3)
                continue
            logger.info("...Socket Connected")
            return True
        return False

t = JsonClient()
t.connect()


data = {
    "msgType": "3", 
    "msgData": {
        "desNode": "serverSocket", 
        "srcNode": "clientSocket001",
        "registerKey": "yijiahe"
    }
}

data3 = {
        "msgID" : "1234567",
        "msgType" : "1",
        "msgData" : 
        {
            "desNode": "serverSocket",
            "srcNode" : "clientSocket001",
            "data" : 
            {
                "pictureInfo1" : 
                {
                    "analyseType" : "1",
                    "imagePath" : "http://192.168.9.40:18715/image/21000000049.jpeg1",
                    "modelPath" : "http://192.168.9.37:18715/image/21000000049.jpeg",
                    "taskId" : "0fe0a872f2c9490089d0e7bf7d687655",
                    "instanceId" : "11000000052"
                },
                "pictureInfo2" : 
                {
                    "analyseType" : "27",
                    "imagePath" : "http://192.168.9.40:18715/image/21000000049.jpeg1",
                    "modelPath" : "http://192.168.9.37:18715/image/21000000049.jpeg",
                    "taskId" : "0fe0a872f2c9490089d0e7bf7d687655",
                    "instanceId" : "11000000052"
                }
            }
         }     
}


# print(data)
t.sendObj(data3)
print("++++ ", t.readObj())
