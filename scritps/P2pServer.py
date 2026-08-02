import socket
import struct
import threading
import time
import random


SERVER_PORT = 6556

MAGIC = 0x12345678
VERSION = 1


class PacketType:
    PK_LOGIN = 1
    PK_CHAT = 2
    PK_LOGOUT = 3
    PK_HEARTBEAT = 4
    PK_ACK = 5
    PK_POS = 6
    PK_PUNCH = 7



# ================================
# Packet Header
#
# uint32 magic
# uint16 version
# uint16 type
# uint32 user_id
# uint32 sequence
#
# little endian
# ================================

# <小端  !大端
HEADER_FMT = "<IHHII"
HEADER_SIZE = struct.calcsize(HEADER_FMT)



# ChatPacketBody
#
# uint32 target_user_id
# uint32 payload_length
#
BODY_CHAT_FMT = "<II"



# PosPacketBody
#
# uint32 target_user_id
# sockaddr_in
#
# sockaddr_in:
# uint16 family
# uint16 port
# uint32 ip
# char zero[8]
#
POS_BODY_FMT = "<IHH4s8s"




clients = {}

# user_id:
# {
#    seq:
#       {
#        target,
#        addr,
#        ack
#       }
# }
caches = {}

lock = threading.Lock()



# ================================
# SOCKADDR_IN 转换
# ================================

def addr_to_sockaddr_in(addr):

    ip, port = addr

    return struct.pack(
        "<HH4s8s",
        socket.AF_INET,
        socket.htons(port),
        socket.inet_aton(ip),
        b"\0"*8
    )


def sockaddr_in_to_addr(data):

    family, port, ip, zero = struct.unpack(
        "<HH4s8s",
        data
    )

    return (
        socket.inet_ntoa(ip),
        socket.ntohs(port)
    )



# ================================
# Packet
# ================================

def pack_header(
        ptype,
        user_id,
        seq=0):

    return struct.pack(
        HEADER_FMT,
        MAGIC,
        VERSION,
        ptype,
        user_id,
        seq
    )



def unpack_header(data):

    return struct.unpack(
        HEADER_FMT,
        data[:HEADER_SIZE]
    )




# ================================
# ACK
# ================================

def send_ack(sock,user_id,seq,addr):

    packet = pack_header(
        PacketType.PK_ACK,
        user_id,
        seq
    )

    sock.sendto(packet,addr)



# ================================
# POS
# ================================

def send_pos(
        sock,
        user_id,
        target_id,
        target_addr,
        seq,
        to):


    body = struct.pack(
        POS_BODY_FMT,
        target_id,
        socket.AF_INET,
        socket.htons(target_addr[1]),
        socket.inet_aton(target_addr[0]),
        b"\0"*8
    )


    packet = (
        pack_header(
            PacketType.PK_POS,
            user_id,
            seq
        )
        +
        body
    )


    sock.sendto(
        packet,
        to
    )




def send_logout(sock,user_id,addr):

    sock.sendto(
        pack_header(
            PacketType.PK_LOGOUT,
            user_id
        ),
        addr
    )




# ================================
# 心跳检测
# ================================

def heartbeat_check():

    while True:

        now=time.time()

        with lock:

            remove=[]

            for uid,c in clients.items():

                if now-c["heartbeat"] > 5:

                    print(
                        "disconnect",
                        uid
                    )

                    remove.append(uid)


            for uid in remove:
                del clients[uid]


        time.sleep(3)




# ================================
# 收包线程
# ================================

def recv_thread(sock):

    while True:

        data,addr=sock.recvfrom(65535)


        if len(data)<HEADER_SIZE:
            continue


        magic,ver,ptype,user_id,seq = unpack_header(data)


        if magic != MAGIC:
            continue



        with lock:


            if ptype == PacketType.PK_LOGIN:


                if user_id in clients:

                    send_ack(
                        sock,
                        user_id,
                        seq,
                        addr
                    )

                    continue



                clients[user_id]={
                    "addr":addr,
                    "heartbeat":time.time()
                }


                print(
                    "[LOGIN]",
                    user_id,
                    addr
                )


                send_ack(
                    sock,
                    user_id,
                    seq,
                    addr
                )




            elif ptype == PacketType.PK_HEARTBEAT:


                if user_id in clients:

                    clients[user_id]["heartbeat"]=time.time()

                else:

                    send_logout(
                        sock,
                        user_id,
                        addr
                    )





            elif ptype == PacketType.PK_POS:


                if len(data)<HEADER_SIZE+struct.calcsize(POS_BODY_FMT):
                    continue


                (
                    target_id,
                    family,
                    port,
                    ip,
                    zero
                )=struct.unpack(
                    POS_BODY_FMT,
                    data[HEADER_SIZE:]
                )


                caches.setdefault(
                    user_id,
                    {}
                )[seq]={

                    "target":target_id,
                    "addr":addr,
                    "ack":False
                }



                send_ack(
                    sock,
                    user_id,
                    seq,
                    addr
                )




            elif ptype == PacketType.PK_ACK:


                if user_id in caches:

                    if seq in caches[user_id]:

                        caches[user_id][seq]["ack"]=True





# ================================
# 转发/重发线程
# ================================

def send_thread(sock):

    while True:

        with lock:

            for uid,seqs in list(caches.items()):


                for seq,item in list(seqs.items()):


                    if item["ack"]:

                        del seqs[seq]
                        continue



                    target=item["target"]


                    if target in clients:


                        target_addr=clients[target]["addr"]

                        print(target,target_addr)
                        
                        send_pos(
                            sock,
                            uid,
                            target,
                            target_addr,
                            seq,
                            item["addr"]
                        )


        time.sleep(1)





# ================================
# main
# ================================

def main():

    sock=socket.socket(
        socket.AF_INET,
        socket.SOCK_DGRAM
    )


    sock.bind(
        ("0.0.0.0",SERVER_PORT)
    )


    print(
        "P2P server start",
        SERVER_PORT
    )



    threading.Thread(
        target=heartbeat_check,
        daemon=True
    ).start()



    threading.Thread(
        target=recv_thread,
        args=(sock,),
        daemon=True
    ).start()



    threading.Thread(
        target=send_thread,
        args=(sock,),
        daemon=True
    ).start()



    while True:

        time.sleep(10)



if __name__=="__main__":

    main()