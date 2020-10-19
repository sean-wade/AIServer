#- * - coding: UTF - 8 -*-
import json
import socket
import struct
import time

clientSock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
clientSock.connect(('localhost', 13669))

data1 = {
    "msgType": "3", 
    "msgData": {
        "desNode": "serverSocket", 
        "srcNode": "clientSocket001",
        "registerKey": "yijiahe"
    }
}

data = {
    "msgType": "5", 
    "msgData": {
        "desNode": "serverSocket", 
        "srcNode": "clientSocket001",
    }
}

msg = json.dumps(data)
frmt = "=%ds" % len(msg)
packedMsg = struct.pack(frmt, bytes(msg.encode('utf-8')))

s = time.time()
sendDataLen = clientSock.send(packedMsg)
recvData = clientSock.recv(1024)
print("sendDataLen: ", sendDataLen)
print("recvData: ", recvData)
print("Success! Time using {:.2f}".format(time.time() - s))


