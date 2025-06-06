## RokuECP
C library to control Roku devices remotely with ECP.  

### Dependencies
* libsoup 3
* gssdp 1.6
* tinyxml2

### Example usage
This example assumes there are 4 or less Roku devices on the local network and finds them, then prints their names.
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

### API Reference
A complete API reference is available at https://benthetechguy.github.io/rokuecp/namespacerokuecp.html

### Build and install
```
mkdir build && cd build
cmake ..
cmake --build .
sudo cmake --install .
```
