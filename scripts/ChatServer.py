import socket
import threading
import struct
import select
import time


# ==========================
# 协议定义
# ==========================


MAGIC = 0x12345678
VERSION = 1


PK_LOGIN = 1
PK_CHAT = 2
PK_LOGOUT = 3
PK_HEARTBEAT = 4


# C++:
# uint32_t magic
# uint16_t version
# uint16_t type
# uint32_t user_id

# <小端  !大端
HEADER_FORMAT = "<IHHI"

HEADER_SIZE = struct.calcsize(HEADER_FORMAT)


# target_user_id
# payload_length

# <小端  !大端
BODY_FORMAT = "<II"

BODY_SIZE = struct.calcsize(BODY_FORMAT)



# ==========================
# 数据结构
# ==========================


class Client:

    def __init__(self,user_id,sock):

        self.user_id = user_id
        self.sock = sock



class ChatInfo:

    def __init__(self,user_id,target,msg):

        self.user_id = user_id
        self.target_user_id = target
        self.msg = msg



# ==========================
# 全局变量
# ==========================


clients = {}

chat_infos = []


lock = threading.RLock()



# ==========================
# TCP可靠接收
# ==========================


def recv_all(sock,size):

    data = b''

    while len(data)<size:

        try:

            part = sock.recv(size-len(data))

            if not part:
                return None

            data += part


        except:

            return None


    return data




# ==========================
# 发送聊天包
# ==========================


def send_chat_packet(sock,user_id,target_id,text):


    header = struct.pack(
        HEADER_FORMAT,
        MAGIC,
        VERSION,
        PK_CHAT,
        user_id
    )


    body = struct.pack(
        BODY_FORMAT,
        target_id,
        len(text.encode())
    )


    sock.sendall(header)
    sock.sendall(body)

    sock.sendall(
        text.encode()
    )





# ==========================
# 判断断开
# ==========================


def is_client_disconnected(sock):

    try:

        r,_,_=select.select(
            [sock],
            [],
            [],
            0
        )


        if r:

            data=sock.recv(
                1,
                socket.MSG_PEEK
            )

            if len(data)==0:
                return True


    except:

        return True


    return False





# ==========================
# 接收客户端
# ==========================


def accept_thread(server):


    while True:


        client_sock,addr=server.accept()


        print(
            "Client:",
            addr[0],
            addr[1]
        )



        data=recv_all(
            client_sock,
            HEADER_SIZE
        )


        if not data:

            client_sock.close()
            continue



        magic,version,ptype,user_id=struct.unpack(
            HEADER_FORMAT,
            data
        )


        if magic!=MAGIC:

            client_sock.close()
            continue



        client=Client(
            user_id,
            client_sock
        )


        with lock:

            if user_id not in clients:

                clients[user_id]=client

                print(
                    "User login:",
                    user_id
                )

            else:

                client_sock.close()





# ==========================
# 接收消息
# ==========================


def recv_packets():


    while True:


        with lock:

            current=list(clients.values())



        for client in current:


            r,_,_=select.select(
                [client.sock],
                [],
                [],
                0
            )


            if not r:
                continue



            header_data=recv_all(
                client.sock,
                HEADER_SIZE
            )


            if not header_data:
                continue



            magic,version,ptype,user_id=struct.unpack(
                HEADER_FORMAT,
                header_data
            )


            if magic!=MAGIC:
                continue



            body_data=recv_all(
                client.sock,
                BODY_SIZE
            )


            if not body_data:
                continue



            target,length=struct.unpack(
                BODY_FORMAT,
                body_data
            )


            msg_data=recv_all(
                client.sock,
                length
            )


            if not msg_data:
                continue



            msg=msg_data.decode()



            print(
                f"userId:{user_id} "
                f"Target:{target} "
                f"msg:{msg}"
            )



            info=ChatInfo(
                user_id,
                target,
                msg
            )


            with lock:

                chat_infos.append(info)





        time.sleep(0.01)





# ==========================
# 转发消息
# ==========================


def send_packets():


    while True:


        with lock:


            if not chat_infos:

                time.sleep(0.01)
                continue



            infos=list(chat_infos)



        for info in infos:


            with lock:

                target_client=clients.get(
                    info.target_user_id
                )


            if target_client:


                try:


                    send_chat_packet(
                        target_client.sock,
                        info.user_id,
                        info.target_user_id,
                        info.msg
                    )



                    with lock:

                        if info in chat_infos:

                            chat_infos.remove(info)



                except:


                    pass




        time.sleep(0.01)






# ==========================
# 检查掉线
# ==========================


def check_disconnect():


    while True:


        with lock:


            remove=[]


            for uid,client in clients.items():


                if is_client_disconnected(
                    client.sock
                ):

                    remove.append(uid)



            for uid in remove:


                print(
                    "User logout:",
                    uid
                )


                clients[uid].sock.close()

                del clients[uid]



        time.sleep(0.1)





# ==========================
# main
# ==========================


def main():

    server=socket.socket(
        socket.AF_INET,
        socket.SOCK_STREAM
    )


    server.setsockopt(
        socket.SOL_SOCKET,
        socket.SO_REUSEADDR,
        1
    )


    server.bind(
        ("0.0.0.0",8080)
    )


    server.listen()


    print(
        "Server listening 8080"
    )



    threading.Thread(
        target=accept_thread,
        args=(server,),
        daemon=True
    ).start()



    threading.Thread(
        target=recv_packets,
        daemon=True
    ).start()



    threading.Thread(
        target=send_packets,
        daemon=True
    ).start()



    threading.Thread(
        target=check_disconnect,
        daemon=True
    ).start()



    while True:

        time.sleep(1)




if __name__=="__main__":

    main()