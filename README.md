## RokuECP
C library to interact with Roku devices remotely with ECP.

### Dependencies
* libsoup 3
* gssdp 1.6
* libxml2 2.13

### Compatibility
Should work with anything supported by the dependencies above. Tested on Linux, macOS, and Windows (build with MinGW).

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
    int devicesFound = findRokuDevices(NULL, 4, 30, urlList);

    // Get info about found devices and print their names
    for (int i = 0; i < devicesFound; i++) {
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
A complete API reference is available at https://benthetechguy.github.io/rokuecp/rokuecp_8h.html

### Build and install
```
mkdir build && cd build
cmake ..
cmake --build .
sudo cmake --install .
```
