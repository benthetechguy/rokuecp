## RokuECP
C/C++ library to control Roku devices remotely with ECP.  
RokuECP is written in C++, but it also functions as a C library since it uses primitive types.

### Dependencies
* libsoup 3
* gssdp 1.6
* tinyxml2
* if linked with C, stdc++ is needed

### Example usage (C++)
This example assumes there are 4 or less Roku devices on the local network and finds them, then prints their names.
```cpp
#include <rokuecp.h>
#include <iostream>
const int MAX_DEVICES = 4;

int main() {
    // Set up an array of C-strings with size 30
    char *urlList[MAX_DEVICES];
    for (int i = 0; i < MAX_DEVICES; i++) {
        urlList[i] = new char[30];
    }
    // Find Roku devices on local network and fill urlList
    rokuecp::findRokuDevices(MAX_DEVICES, urlList);

    // Get info about found devices and print their names
    for (int i = 0; urlList[i][0] != '\0'; i++) {
        rokuecp::RokuDevice device = rokuecp::getRokuDevice(urlList[i]);
        std::cout << "Device " << i + 1 << ": " << device.name << std::endl;
    }
}
```
This example could be compiled with `g++ -o main main.cpp -lrokuecp -lsoup-3.0 -lgssdp-1.6 -ltinyxml2 -lglib-2.0 -lgobject-2.0`.

### Example usage (C)
This example accomplishes the same as the previous example, but in plain C.
```C
#include <rokuecp.h>
#include <stdio.h>
#include <stdlib.h>
const int MAX_DEVICES = 4;

int main() {
    // Set up an array of strings with size 30
    char *urlList[MAX_DEVICES];
    for (int i = 0; i < MAX_DEVICES; i++) {
        urlList[i] = malloc(30);
    }
    // Find Roku devices on local network and fill urlList
    findRokuDevices(MAX_DEVICES, urlList);

    // Get info about found devices and print their names
    for (int i = 0; urlList[i][0] != '\0'; i++) {
        RokuDevice device = getRokuDevice(urlList[i]);
        printf("Device %d: %s\n", i + 1, device.name);
    }
}
```
This example could be compiled with `gcc -o main main.c -lrokuecp -lsoup-3.0 -lgssdp-1.6 -ltinyxml2 -lglib-2.0 -lgobject-2.0 -lstdc++`.

### API Reference
A complete API reference is available at https://benthetechguy.github.io/rokuecp/namespacerokuecp.html

### Build and install
```
mkdir build
cmake ..
cmake --build .
cmake --install .
```