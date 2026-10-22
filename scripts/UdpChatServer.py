import socket
import struct
import threading
import time
from enum import IntEnum


# ===============================
# Packet Define
# ===============================

class PacketType(IntEnum):
    PK_LOGIN = 1
    PK_CHAT = 2
    PK_LOGOUT = 3
    PK_HEARTBEAT = 4
    PK_ACK = 5


MAGIC = 0x12345678
VERSION = 1


# #pragma pack(1)
# uint32 magic
# uint16 version
# uint16 type
# uint32 user_id
# uint32 sequence

# <小端  !大端
HEADER_FMT = "<IHHII"
HEADER_SIZE = struct.calcsize(HEADER_FMT)


# body
# uint32 target_user_id
# uint32 payload_length

# <小端  !大端
BODY_FMT = "<II"
BODY_SIZE = struct.calcsize(BODY_FMT)



# ===============================
# Data
# ===============================

clients = {}
# user_id -> Client

g_caches = {}
# sender_id -> sequence -> PacketCache


lock = threading.Lock()



class Client:

    def __init__(self, user_id, addr):

        self.user_id = user_id
        self.addr = addr
        self.last_heartbeat = time.monotonic()



class PacketCache:

    def __init__(
        self,
        user_id,
        target_user_id,
        sequence,
        msg
    ):

        self.user_id = user_id
        self.target_user_id = target_user_id
        self.sequence = sequence
        self.msg = msg
        self.isAck = False



# ===============================
# Packet Build
# ===============================


def make_header(
        ptype,
        user_id,
        sequence=0
):

    return struct.pack(
        HEADER_FMT,
        MAGIC,
        VERSION,
        int(ptype),
        user_id,
        sequence
    )



def send_ack(sock, user_id, sequence, addr):

    packet = make_header(
        PacketType.PK_ACK,
        user_id,
        sequence
    )

    sock.sendto(
        packet,
        addr
    )



def send_logout(sock,user_id,addr):

    packet = make_header(
        PacketType.PK_LOGOUT,
        user_id
    )

    sock.sendto(
        packet,
        addr
    )



def send_chat(
        sock,
        user_id,
        target_id,
        sequence,
        addr,
        msg
):

    header = make_header(
        PacketType.PK_CHAT,
        user_id,
        sequence
    )


    body = struct.pack(
        BODY_FMT,
        target_id,
        len(msg.encode())
    )


    packet = (
        header +
        body +
        msg.encode()
    )


    sock.sendto(
        packet,
        addr
    )



# ===============================
# Decode
# ===============================


def parse_header(data):

    return struct.unpack(
        HEADER_FMT,
        data[:HEADER_SIZE]
    )



# ===============================
# Heartbeat thread
# ===============================


def heartbeat_thread():

    while True:

        with lock:

            now = time.monotonic()

            remove=[]

            for uid,c in clients.items():

                if now-c.last_heartbeat > 5:

                    print(
                        "[!] Client disconnected:",
                        uid
                    )

                    remove.append(uid)


            for uid in remove:
                del clients[uid]


        time.sleep(3)



# ===============================
# Receive thread
# ===============================


def recv_thread(sock):


    while True:


        try:

            data,addr = sock.recvfrom(65535)


            if len(data)<HEADER_SIZE:
                continue


            (
                magic,
                version,
                ptype,
                user_id,
                sequence
            ) = parse_header(data)



            if magic != MAGIC:
                continue



            ptype = PacketType(ptype)



            with lock:


                # -----------------
                # heartbeat
                # -----------------

                if ptype == PacketType.PK_HEARTBEAT:


                    if user_id not in clients:

                        send_logout(
                            sock,
                            user_id,
                            addr
                        )

                    else:

                        clients[user_id].last_heartbeat = (
                            time.monotonic()
                        )



                # -----------------
                # login
                # -----------------

                elif ptype == PacketType.PK_LOGIN:


                    if user_id not in clients:


                        clients[user_id] = Client(
                            user_id,
                            addr
                        )


                        print(
                            "[Login]",
                            addr,
                            "UserId:",
                            user_id
                        )


                    send_ack(
                        sock,
                        user_id,
                        sequence,
                        addr
                    )




                # -----------------
                # chat
                # -----------------

                elif ptype == PacketType.PK_CHAT:



                    body_start=HEADER_SIZE


                    (
                        target_id,
                        payload_len
                    )=struct.unpack(
                        BODY_FMT,
                        data[
                            body_start:
                            body_start+BODY_SIZE
                        ]
                    )


                    msg=data[
                        body_start+
                        BODY_SIZE:
                        body_start+
                        BODY_SIZE+
                        payload_len
                    ].decode()



                    print(
                        f"user:{user_id} "
                        f"target:{target_id} "
                        f"msg:{msg}"
                    )



                    if user_id not in g_caches:
                        g_caches[user_id]={}



                    cache=PacketCache(
                        user_id,
                        target_id,
                        sequence,
                        msg
                    )


                    g_caches[user_id][sequence]=cache



                    send_ack(
                        sock,
                        user_id,
                        sequence,
                        addr
                    )



                # -----------------
                # ACK
                # -----------------

                elif ptype == PacketType.PK_ACK:


                    if user_id in g_caches:

                        if sequence in g_caches[user_id]:

                            g_caches[user_id][sequence].isAck=True



        except Exception as e:

            print(
                "recv error:",
                e
            )



# ===============================
# Resend thread
# ===============================


def resend_thread(sock):


    while True:


        with lock:


            for sender_id,caches in list(g_caches.items()):


                for seq,cache in list(caches.items()):


                    if cache.isAck:


                        del caches[seq]

                        continue



                    if cache.target_user_id in clients:


                        target=clients[
                            cache.target_user_id
                        ]


                        send_chat(
                            sock,
                            cache.user_id,
                            cache.target_user_id,
                            cache.sequence,
                            target.addr,
                            cache.msg
                        )



        time.sleep(1)



# ===============================
# main
# ===============================


def main():

    print("UDP Chat Server")


    sock=socket.socket(
        socket.AF_INET,
        socket.SOCK_DGRAM
    )


    sock.bind(
        (
            "0.0.0.0",
            6556
        )
    )


    print(
        "listen port 6556"
    )



    threading.Thread(
        target=heartbeat_thread,
        daemon=True
    ).start()



    threading.Thread(
        target=recv_thread,
        args=(sock,),
        daemon=True
    ).start()



    threading.Thread(
        target=resend_thread,
        args=(sock,),
        daemon=True
    ).start()



    while True:

        time.sleep(1)



if __name__=="__main__":

    main()