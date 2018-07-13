""" What do I have to test?
## TESTS FOR SERVER
-> test that server is running on IP and PORT
-> test that server responds -ERR when faced with a GET cmd of more than 255.
-> test that server responds -ERR when command is not GET
-> test that server does handle timeouts
-> test that server only accepts normal files, not directories
-> test that server handles client abruptly closing connection
-> test that server correctly closes connection with QUIT cmd.
"""
import socket
import time
import sys
import os

tests_suite = []
BUF = 4096

def tests(func):
    tests_suite.append(func)
    return func

def connect_socket():
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.connect((sys.argv[1], int(sys.argv[2])))
    except Exception as e:
        raise e
    return s

@tests
def testRunning():
    try:
        s = connect_socket()
    except Exception as e:
        print(e)
        return False
    s.close()
    return True

@tests
def testTooLong():
    s = connect_socket()
    payload = 'GET ' + 'A'*400 + '\r\n'
    s.send(payload.encode())
    data = s.recv(BUF).decode()
    s.close()
    return True if data == '-ERR\r\n' else False

@tests
def testTimeout():
    s = connect_socket()
    time.sleep(15)
    data = s.recv(BUF).decode()
    s.close()
    return True if len(data) is 0 else False

@tests
def testDirectories():
    s = connect_socket()
    payload = b'GET files/file.txt\r\n'
    s.send(payload)
    data = s.recv(BUF).decode()
    s.close()
    return True if data == '-ERR\r\n' else False

@tests
def testAbruptClose():
    s = connect_socket()
    payload = b'GET rockyou.txt\r\n'
    s.send(payload)
    time.sleep(2)
    s.close()
    return True

@tests
def testQUIT():
    s = connect_socket()
    payload = b'QUIT\r\n'
    s.send(payload)
    data = s.recv(BUF)
    s.close()
    return True if len(data) is 0 else False

@tests
def testNotGET():
    s = connect_socket()
    payload = b'GOT filename.txt\r\n'
    s.send(payload)
    data = s.recv(BUF).decode()
    s.close()
    return True if data == '-ERR\r\n' else False

@tests
def testMultipleFilenames():
    s = connect_socket()
    payload = b'GET file.txt file2.txt\r\n'
    s.send(payload)
    data = s.recv(BUF).decode()
    s.close()
    return True if data == '-ERR\r\n' else False

@tests

def main():
    if len(sys.argv) < 3:
        print(f'Usage: {sys.argv[0]} <server_IP> <server_PORT>')
        exit(0)
    for test in tests_suite:
        print(f'{test.__name__} ==> ')
        if(test()):
            print(f'TRUE')
        else:
            print(f'FALSE -> EXITING')
            exit(0)

if __name__ == '__main__':
    main()
