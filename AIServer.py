import os
import cv2
import time
import json
import struct
import socket
import logging
from threading import Thread, Lock


# TODO:
# from mmdetection.whatever import Detector   


class AIServer(object):
    def __init__(self, address='127.0.0.1', port=13669):
        try:
            self._address = address
            self._port = port
            self._server_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self._server_sock.bind((self._address, self._port))
            self._server_sock.listen()
            self._client_sock_addr_map = {}
            self._server_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            self._lock = Lock()

            # TODO:
            # self._detector = Detector()

        except Exception as e:
            logger.critical(e)


    def main_loop(self):
        try:
            while  True:
                # print('==== ==== ==== AIServer Waiting for client... @', self._address, ":", self._port)
                logger.info('==== ==== ==== AIServer Waiting for client... @%s:%s'%(self._address, self._port))
                client_sock, addr = self._server_sock.accept()
                # print('>>>> >>>> >>>> Client(%s) success connect...'%str(addr))
                logger.info('>>>> >>>> >>>> Client(%s) success connect...'%str(addr))
                
                self._client_sock_addr_map[addr] = client_sock
                # 每循环一次就会产生一个线程
                thread = Thread(target = self.worker, args = (client_sock, addr))
                thread.start()
        except Exception as e:
            logger.critical(e)
        finally:
            self._server_sock.close()


    def worker(self, sock, addr):
        # print(">>>> >>>> >>>> Client list: ", self._client_sock_addr_map.keys())
        logger.info(">>>> >>>> >>>> Client list: %s"%str(self._client_sock_addr_map.keys()))

        while True:
            recv_data = sock.recv(1024)
            if len(recv_data) > 0:
                # print('++++ addr: ', addr, ' accept: ', recv_data)
                logger.info('++++ ++++ ++++ ++++ Receive Message From addr: %s accept: \n    %s'%(str(addr), recv_data))
                replyMsg = self.AIProcessMessage(recv_data)
                if(replyMsg):
                    sock.send(replyMsg)
            else:
                # print('>>>> >>>> >>>> Client %s closed !!! '%str(addr))
                logger.info('>>>> >>>> >>>> Client %s closed !!! '%str(addr))
                break

        print(">>>> >>>> >>>> Client [ ", addr, " ] thread exit !!!")
        logger.info(">>>> >>>> >>>> Client [ %s ] thread exit !!!"%str(addr))
        del self._client_sock_addr_map[addr]

    
    def AIDetect(self, imgPath, analyseType):
        if (imgPath is None) or (analyseType is None):
            logger.error("**** **** **** **** imgPath or analyseType Wrong !!!")
            return ""

        cap = cv2.VideoCapture(imgPath)
        hasFrame, frame = cap.read()
        if not hasFrame:
            logger.error("**** **** **** **** imgPath cannot open %s !!!"%imgPath)
            return ""
        
        self._lock.acquire()
        # TODO:
        # mmdetection 推理，根据analyseType加载不同模型推理（或使用同一模型的话，返回筛选后的结果）
        # resultStr = self._detector.inference(frame)
        # return resultStr
        self._lock.release()
        return "wcgz 10 10 100 100"


    def AIProcessMessage(self, data):
        msg = struct.unpack("=%ds"%len(data), data)
        data_json = json.loads(msg[0])
        # print("AIProcessMessage: ", data_json, type(data_json))
        
        msgType = data_json.get("msgType")
        if(msgType == None):
            # print("**** Json Message Missing Key: [msgType] !!!")
            logger.error("**** **** **** **** Json Message Missing Key: [msgType] !!!")
            return None
        
        msgData = data_json.get("msgData")
        if(msgData == None):
            # print("**** Json Message Missing Key: [msgData] !!!")
            logger.error("**** **** **** **** Json Message Missing Key: [msgData] !!!")
            return None
        
        msgReply = {}
        msgDataReply = {}
        
        desNode = msgData.get("desNode")
        srcNode = msgData.get("srcNode")
        
        if(desNode and srcNode):
            msgDataReply["desNode"] = srcNode
            msgDataReply["srcNode"] = desNode
        
        if(msgType == "1"):      #数据消息
            msgReply["msgType"] = "2"
            msgReply["msgID"] = data_json.get("msgID")
            pictureData = msgData.get("data")
            if pictureData == None:
                logger.error("**** **** **** **** Json Message Missing Key: [msgData/data] !!!")
                return None
            dataReply = {}
            for i, (picKey, picValue) in enumerate(pictureData.items()):
                # print(i, picKey, picValue)
                imagePath = picValue.get("imagePath")
                analyseType = picValue.get("analyseType")
                taskId = picValue.get("taskId")
                instanceId = picValue.get("instanceId")

                resultValue = self.AIDetect(imagePath, analyseType)

                dataReply["resultInfo%d"%(i+1)] = {
                    "taskId" : taskId,
                    "instanceId" : instanceId,
                    "analyseType" : analyseType,
                    "resultValue" : resultValue
                }
            msgDataReply["data"] = dataReply

        elif(msgType == "3"):    #注册消息
            msgReply["msgType"] = "4"
            registerKey = msgData.get("registerKey")
            if(registerKey == "yijiahe"):
                msgDataReply["errorCode"] = "0"
            else:
                msgDataReply["errorCode"] = "-1"
                          
        elif(msgType == "5"):    #心跳消息
            msgReply["msgType"] = "6"
            msgDataReply["errorCode"] = "0"
            
        else:
            # print("**** Json Message msgType wrong !!! msgType = ", msgType)
            logger.error("**** **** **** **** Json Message msgType wrong !!! msgType = %s"%msgType)
            return None
        
        msgReply["msgData"] = msgDataReply
        
        replyMsg = json.dumps(msgReply)
        frmt = "=%ds" % len(replyMsg)
        packedMsg = struct.pack(frmt, bytes(replyMsg.encode('utf-8')))

        # print("=>=> =>=> =>=> Send msgReply: ", replyMsg)
        logger.info("=>=> =>=> =>=> Send msgReply: %s"%replyMsg)
        return packedMsg


def init_log(output_dir):
    if not os.path.exists(output_dir):
        os.mkdir(output_dir)
    log_name = '{}.log'.format(time.strftime('%Y%m%d'))

    logging.basicConfig(level = logging.DEBUG,
                        format = '%(asctime)s [%(levelname)s] [%(lineno)d] %(message)s',
                        datefmt = '%Y-%m-%d %H:%M:%S',
                        filename = os.path.join(output_dir, log_name),
                        filemode = 'a')
    console = logging.StreamHandler()
    console.setLevel(logging.DEBUG)
    logging.getLogger('').addHandler(console)
    return logging

logger = init_log("AIServerLog")

# 获取本机ip
def get_host_ip():
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(('8.8.8.8', 80))
        ip = s.getsockname()[0]
    finally:
        s.close()
    return ip


if __name__ == '__main__':
    logger.info('==== ==== ==== ==== ==== ==== ==== ==== ==== Start Process ... ==== ==== ==== ==== ==== ==== ==== ==== ====\n')
    ai_server = AIServer(address=get_host_ip(), port=13669)
    ai_server.main_loop()
