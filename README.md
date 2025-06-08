## RokuECP
C library to control Roku devices remotely with ECP.  

### Dependencies
* libsoup 3
* gssdp 1.6
* libxml2 2.13

### Example usage
This example assumes there are 4 or less Roku devices on the local network and finds them, then prints their names.
```C
#include <rokuecp.h>
#include <stdio.h>
#include <stdlib.h>

int main() {
    // Set up an array of 4 strings with size 30
    char* urlList[4];
    for (int i = 0; i < 4; i++) {
        urlList[i] = malloc(30);
    }
    // Find Roku devices on local network and fill urlList
    findRokuDevices(4, 30, urlList);

    // Get info about found devices and print their names
    for (int i = 0; *urlList[i] != '\0'; i++) {
        RokuDevice device;
        getRokuDevice(urlList[i], &device);
        printf("Device %d: %s\n", i + 1, device.name);
    }

    // Don't forget to free the URL list!
    for (int i = 0; i < 4; i++) {
        free(urlList[i]);
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
