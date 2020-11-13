# cpp-chat
TCP Client Server(Multithreaded)    
Use `-pthread` flag while compiling  
```
./server [server-port-number]
```
```
./client [server-port-number] [client-name]
```


Supported commands:  
- **/join [groupId]**
- **/send [groupId][message]**
- **/leave [groupId]**
- **/quit**
