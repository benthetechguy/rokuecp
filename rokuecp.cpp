/*
Copyright 2025 Ben Westover <me@benthetechguy.net>

This program is free software: you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program. If not, see <https://www.gnu.org/licenses/>.
*/

#include "rokuecp.h"
#include <libgssdp/gssdp.h>
#include <libsoup/soup.h>
#include <tinyxml2.h>
#include <cstring>
#include <cwchar>
#include <string>
#include <tuple>

// MinGW doesn't have strlcpy
#ifdef __MINGW32__
size_t strlcpy(char *dest, const char *src, size_t size) {
    strncpy(dest, src, size - 1);
    dest[size - 1] = '\0';
    return size - 1;
}
#endif

/** @internal
 * SSDP quit GMainLoop callback: Run after 5 seconds in the loop looking for Roku devices, quits the loop.
 * @param user_data Pointer to the main loop.
 * @return Whether quitting the loop was successful.
 */
gboolean ssdpQuitGMainLoopCallback(gpointer user_data) {
    g_main_loop_quit(static_cast<GMainLoop*>(user_data));
    return G_SOURCE_REMOVE;
}

/** @internal
 * SSDP resource_available callback: When SSDP finds a Roku device, add it to the deviceList and increment devicesFound.
 * @param self Pointer to the SSDP resource browser.
 * @param usn USN of the found resource.
 * @param locations List of C-strings with URLs to the resource.
 * @param user_data Tuple containing: A pointer to the main loop
 *                                  , maximum number of devices that can be found
 *                                  , number of devices currently found
 *                                  , and the list of found device URL C-strings.
 */
static void ssdpResourceAvailableCallback(GSSDPResourceBrowser *self, const char* usn, const GList *locations, void *user_data) {
    auto *deviceFindState = static_cast<std::tuple<GMainLoop*, int, int, char**>*>(user_data);
    GMainLoop *mainLoop = std::get<0>(*deviceFindState);
    const int maxDevices = std::get<1>(*deviceFindState);
    int devicesFound = std::get<2>(*deviceFindState);
    char** deviceList = std::get<3>(*deviceFindState);

    // Stop if the end of the device list has been reached
    if (devicesFound >= maxDevices - 1) {
        g_main_loop_quit(mainLoop);
    }

    // Update the list and increment devicesFound, then update the device find state
    strlcpy(deviceList[devicesFound++], static_cast<char*>(locations->data), 30);
    std::tuple<GMainLoop*, int, int, char**> newState = {mainLoop, maxDevices, devicesFound, deviceList};
    deviceFindState->swap(newState);
}

/** @internal
 * Get text from the child of an XMLElement, making sure nothing is NULL to prevent segfaults
 * @param element parent XMLElement
 * @param child C-string name of the child element to get the text from
 * @return C-string with the text inside that child element, or an empty string if the element is empty or does not exist
 */
const char* safeTextFromChildXMLElement(const tinyxml2::XMLElement *element, const char* child) {
    const tinyxml2::XMLElement *childElement = element->FirstChildElement(child);
    if (childElement == nullptr) {
        return "";
    } else {
        const char* text = childElement->GetText();
        if (text == nullptr) {
            return "";
        } else {
            return text;
        }
    }
}

/** @internal
 * Get attribute from an XMLElement, making sure it's not NULL to prevent segfaults
 * @param element XMLElement to get attribute from
 * @param attr C-string name of the attribute
 * @return C-string with the text inside that attribute, or an empty string if the attribute is empty or does not exist
 */
const char* safeAttrFromXMLElement(const tinyxml2::XMLElement *element, const char* attr) {
    const char* attribute = element->Attribute(attr);
    if (attribute == nullptr) {
        return "";
    } else {
        return attribute;
    }
}

/** @internal
 * Send a GET or POST request to the given URL
 * @param url string containing the URL to request
 * @param method type of request to send (e.g. "GET" or "POST")
 * @return pair of error code and response data
 */
std::pair<int, GBytes*> sendRequest(const std::string& url, const char* method) {
    SoupSession *session = soup_session_new();
    SoupMessage *msg = soup_message_new(method, url.c_str());
    GError *error = nullptr;
    GBytes *response = soup_session_send_and_read (session, msg, nullptr, &error);

    if (error) {
        fprintf(stderr, "libsoup error: %s\n", error->message);
        g_object_unref(msg);
        g_object_unref(session);
        return {error->code, nullptr};
    }

    g_object_unref(msg);
    g_object_unref(session);

    return {0, response};
}

int rokuecp::findRokuDevices(const int maxDevices, char* deviceList[]) {
    int devicesFound = 0;

    // Set up gssdp to look for Roku devices
    GError *error = nullptr;
    GSSDPClient *ssdpClient = gssdp_client_new_for_address(nullptr, 0, GSSDP_UDA_VERSION_1_0, &error);
    if (error != nullptr) {
        fprintf(stderr, "error: failed to open SSDP socket: %s\n", error->message);
        return -error->code;
    }
    GSSDPResourceBrowser *rokuFinder = gssdp_resource_browser_new(ssdpClient, "roku:ecp");

    // Set up detection loop and callback
    GMainLoop *mainLoop = g_main_loop_new(nullptr, false);
    std::tuple<GMainLoop*, int, int, char**> callbackData = {mainLoop, maxDevices, devicesFound, deviceList};
    g_signal_connect(rokuFinder, "resource-available", G_CALLBACK(ssdpResourceAvailableCallback), &callbackData);

    // Activate Roku finder and run detection loop for 5 seconds or until list is full
    gssdp_resource_browser_set_active(rokuFinder, true);
    g_timeout_add_seconds(5, ssdpQuitGMainLoopCallback, mainLoop);
    g_main_loop_run(mainLoop);

    // Cleanup gssdp when loop is done
    g_main_loop_unref(mainLoop);
    g_object_unref(rokuFinder);
    g_object_unref(ssdpClient);

    // Update deviceList and devicesFound with new state after callback
    devicesFound = std::get<2>(callbackData);
    for (int i = 0; i < maxDevices; i++) {
        if (i < devicesFound) {
            strcpy(deviceList[i], std::get<3>(callbackData)[i]);
        } else {
            strcpy(deviceList[i], "");
        }
    }

    return devicesFound;
}

rokuecp::RokuDevice rokuecp::getRokuDevice(const char* url) {
    // Create empty RokuDevice and fill in the URL
    RokuDevice device {};
    strlcpy(device.url, url, 30);

    // Request device-info from device and check for errors
    std::pair<int, GBytes*> response = sendRequest(std::string(device.url) + "/query/device-info", SOUP_METHOD_GET);
    int responseCode = response.first;
    GBytes *responseText = response.second;
    if (responseCode != 0) {
        return device;
    }

    // Parse XML response and get device-info element
    tinyxml2::XMLDocument deviceInfo;
    deviceInfo.Parse(static_cast<const char*>(g_bytes_get_data(responseText, nullptr)));
    if (deviceInfo.Error()) {
        fprintf(stderr, "error: failed to parse Roku device info: %s\n", deviceInfo.ErrorName());
        return device;
    }
    tinyxml2::XMLElement *deviceInfoElement = deviceInfo.FirstChildElement( "device-info" );
    if (deviceInfoElement == nullptr) {
        fprintf(stderr, "error: Roku device info is empty\n");
        return device;
    }

    // Fill in device fields based on device-info response
    strlcpy(device.name, safeTextFromChildXMLElement(deviceInfoElement, "user-device-name"), 122);
    strlcpy(device.location, safeTextFromChildXMLElement(deviceInfoElement, "user-device-location"), 16);
    strlcpy(device.model, safeTextFromChildXMLElement(deviceInfoElement, "friendly-model-name"), 32);
    strlcpy(device.serial, safeTextFromChildXMLElement(deviceInfoElement, "serial-number"), 14);
    strlcpy(device.resolution, safeTextFromChildXMLElement(deviceInfoElement, "ui-resolution"), 8);
    strlcpy(device.macAddress, safeTextFromChildXMLElement(deviceInfoElement, "wifi-mac"), 18);
    strlcpy(device.softwareVersion, safeTextFromChildXMLElement(deviceInfoElement, "software-version"), 10);
    device.isOn = strcmp("Ready", safeTextFromChildXMLElement(deviceInfoElement, "power-mode"));
    device.isTV = strcmp("false", safeTextFromChildXMLElement(deviceInfoElement, "is-tv"));
    device.developerMode = strcmp("false", safeTextFromChildXMLElement(deviceInfoElement, "developer-enabled"));
    device.hasSearchSupport = strcmp("false", safeTextFromChildXMLElement(deviceInfoElement, "search-enabled"));
    device.hasHeadphoneSupport = strcmp("false", safeTextFromChildXMLElement(deviceInfoElement, "supports-private-listening"));
    device.headphonesConnected = strcmp("false", safeTextFromChildXMLElement(deviceInfoElement, "headphones-connected"));

    // Clean up and return device
    g_bytes_unref(responseText);
    return device;
}

int rokuecp::rokuSendKey(const RokuDevice *device, const char* key) {
    // Disallow sending keys meant for TVs to non-TV devices
    const char* tvOnlyKeys[12] = {"VolumeUp", "VolumeDown", "VolumeMute", "PowerOff", "ChannelUp", "ChannelDown",
                                  "InputTuner", "InputHDMI1", "InputHDMI2", "InputHDMI3", "InputHDMI4", "InputAV1"};
    if (!device->isTV) {
        for (const char* tvOnlyKey : tvOnlyKeys) {
            if (strcmp(key, tvOnlyKey) == 0) {
                fprintf(stderr, "error: Cannot send TV-only key %s to non-TV device.\n", key);
                return -1;
            }
        }
    }

    // Return result of keypress request
    return sendRequest(std::string(device->url) + "/keypress/" + std::string(key), SOUP_METHOD_POST).first;
}

int rokuecp::getRokuTVChannels(const RokuDevice *device, const int maxChannels, RokuTVChannel channelList[]) {
    if (!device->isTV) {
        fprintf(stderr, "error: Cannot request TV channel list from non-TV device.\n");
        return -4;
    }

    // Request tv-channels from device and check for errors
    std::pair<int, GBytes*> response = sendRequest(std::string(device->url) + "/query/tv-channels", SOUP_METHOD_GET);
    int responseCode = response.first;
    GBytes *responseText = response.second;
    if (responseCode != 0) {
        return -1;
    }

    // Parse channel list XML
    tinyxml2::XMLDocument channelXML;
    channelXML.Parse(static_cast<const char*>(g_bytes_get_data(responseText, nullptr)));
    if (channelXML.Error()) {
        fprintf(stderr, "error: failed to parse Roku channel list: %s\n", channelXML.ErrorName());
        return -2;
    }

    // Make sure channel list is not empty
    tinyxml2::XMLElement *parentElement = channelXML.FirstChildElement("tv-channels");
    if (parentElement == nullptr) {
        fprintf(stderr, "error: Roku channel list is empty\n");
        return -3;
    }

    // Iterate through channels and add them to the channel list
    int channelsFound = 0;
    tinyxml2::XMLElement *nextChannel = parentElement->FirstChildElement("channel");
    while (channelsFound < maxChannels && nextChannel != nullptr) {
        RokuTVChannel channel {};

        strlcpy(channel.id, safeTextFromChildXMLElement(nextChannel, "channel-id"), 8);
        strlcpy(channel.name, safeTextFromChildXMLElement(nextChannel, "name"), 8);
        strlcpy(channel.type, safeTextFromChildXMLElement(nextChannel, "type"), 14);
        channel.isHidden = strcmp("false", safeTextFromChildXMLElement(nextChannel, "user-hidden"));
        channel.isFavorite = strcmp("false", safeTextFromChildXMLElement(nextChannel, "user-favorite"));

        unsigned physicalChannel = 0;
        unsigned long long frequency = 0;
        tinyxml2::XMLElement *physicalChannelElement = nextChannel->FirstChildElement("physical-channel");
        tinyxml2::XMLElement *frequencyElement = nextChannel->FirstChildElement("physical-frequency");
        if (physicalChannelElement != nullptr) physicalChannelElement->QueryUnsignedText(&physicalChannel);
        if (frequencyElement != nullptr) frequencyElement->QueryUnsigned64Text(&frequency);
        channel.physicalChannel = static_cast<unsigned short int>(physicalChannel);
        channel.frequency = static_cast<unsigned long int>(frequency) * 1000; // there's enough room in a long int to store it as Hz instead of kHz

        // Add channel to channel list, then increment next channel pointer and number of channels found
        channelList[channelsFound++] = channel;
        nextChannel = nextChannel->NextSiblingElement("channel");
    }

    // Clean up and return number of channels found
    g_bytes_unref(responseText);
    return channelsFound;
}

rokuecp::RokuExtTVChannel rokuecp::getActiveRokuTVChannel(const RokuDevice *device) {
    RokuExtTVChannel channel {};

    if (!device->isTV) {
        fprintf(stderr, "error: Cannot request TV channel list from non-TV device.\n");
        return channel;
    }

    // Request tv-active-channel from device and check for errors
    std::pair<int, GBytes*> response = sendRequest(std::string(device->url) + "/query/tv-active-channel", SOUP_METHOD_GET);
    int responseCode = response.first;
    GBytes *responseText = response.second;
    if (responseCode != 0) {
        return channel;
    }

    // Parse active channel XML and get channel element
    tinyxml2::XMLDocument channelXML;
    channelXML.Parse(static_cast<const char*>(g_bytes_get_data(responseText, nullptr)));
    if (channelXML.Error()) {
        fprintf(stderr, "error: failed to parse active Roku channel: %s\n", channelXML.ErrorName());
        return channel;
    }

    // Make sure channel is not empty
    tinyxml2::XMLElement *parentElement = channelXML.FirstChildElement("tv-channel");
    if (parentElement == nullptr) {
        fprintf(stderr, "error: active Roku channel is empty\n");
        return channel;
    }
    tinyxml2::XMLElement *channelElement = parentElement->FirstChildElement("channel");
    if (channelElement == nullptr) {
        fprintf(stderr, "error: active Roku channel is empty\n");
        return channel;
    }

    // Fill in channel with data from XML
    strlcpy(channel.channel.id, safeTextFromChildXMLElement(channelElement, "channel-id"), 8);
    strlcpy(channel.channel.name, safeTextFromChildXMLElement(channelElement, "name"), 8);
    strlcpy(channel.channel.type, safeTextFromChildXMLElement(channelElement, "type"), 14);
    channel.channel.isHidden = strcmp("false", safeTextFromChildXMLElement(channelElement, "user-hidden"));
    channel.channel.isFavorite = strcmp("false", safeTextFromChildXMLElement(channelElement, "user-favorite"));
    unsigned physicalChannel = 0;
    unsigned long long frequency = 0;
    tinyxml2::XMLElement *physicalChannelElement = channelElement->FirstChildElement("physical-channel");
    tinyxml2::XMLElement *frequencyElement = channelElement->FirstChildElement("physical-frequency");
    if (physicalChannelElement != nullptr) physicalChannelElement->QueryUnsignedText(&physicalChannel);
    if (frequencyElement != nullptr) frequencyElement->QueryUnsigned64Text(&frequency);
    channel.channel.physicalChannel = static_cast<unsigned short int>(physicalChannel);
    channel.channel.frequency = static_cast<unsigned long int>(frequency) * 1000; // there's enough room in a long int to store it as Hz instead of kHz

    // Only fill in the rest of the data if the channel is active (inactive channels have these fields blank)
    channel.isActive = strcmp("false", safeTextFromChildXMLElement(channelElement, "active-input"));
    if (channel.isActive) {
        strlcpy(channel.program.title, safeTextFromChildXMLElement(channelElement, "program-title"), 112);
        strlcpy(channel.program.description, safeTextFromChildXMLElement(channelElement, "program-description"), 256);
        strlcpy(channel.program.rating, safeTextFromChildXMLElement(channelElement, "program-ratings"), 15);
        channel.program.hasCC = strcmp("false", safeTextFromChildXMLElement(channelElement, "program-has-cc"));
        channel.signalReceived = strcmp("none", safeTextFromChildXMLElement(channelElement, "signal-state"));
        strlcpy(channel.resolution, safeTextFromChildXMLElement(channelElement,"signal-mode"), 8);

        unsigned signalQuality;
        int signalStrength;
        tinyxml2::XMLElement *signalQualityElement = channelElement->FirstChildElement("signal-quality");
        tinyxml2::XMLElement *signalStrengthElement = channelElement->FirstChildElement("signal-strength");
        if (signalQualityElement != nullptr) signalQualityElement->QueryUnsignedText(&signalQuality);
        if (signalStrengthElement != nullptr) signalStrengthElement->QueryIntText(&signalStrength);
        channel.signalQuality = static_cast<unsigned short int>(signalQuality);
        channel.signalStrength = static_cast<signed short int>(signalStrength);
    } else {
        strcpy(channel.program.title, "");
        strcpy(channel.program.description, "");
        strcpy(channel.program.rating, "");
        strcpy(channel.resolution, "");
        channel.program.hasCC = false;
        channel.signalReceived = false;
        channel.signalQuality = 0;
        channel.signalStrength = 0;
    }

    // Clean up and return active channel
    g_bytes_unref(responseText);
    return channel;
}

int rokuecp::getRokuApps(const RokuDevice *device, const int maxApps, RokuApp appList[]) {
    // Request apps from device and check for errors
    std::pair<int, GBytes*> response = sendRequest(std::string(device->url) + "/query/apps", SOUP_METHOD_GET);
    int responseCode = response.first;
    GBytes *responseText = response.second;
    if (responseCode != 0) {
        return -1;
    }

    // Parse app list XML
    tinyxml2::XMLDocument appXML;
    appXML.Parse(static_cast<const char*>(g_bytes_get_data(responseText, nullptr)));
    if (appXML.Error()) {
        fprintf(stderr, "error: failed to parse Roku app list: %s\n", appXML.ErrorName());
        return -2;
    }

    // Make sure app list is not empty
    tinyxml2::XMLElement *parentElement = appXML.FirstChildElement("apps");
    if (parentElement == nullptr) {
        fprintf(stderr, "error: Roku app list is empty\n");
        return -3;
    }

    // Iterate through apps and add them to the app list
    int appsFound = 0;
    tinyxml2::XMLElement *nextApp = parentElement->FirstChildElement("app");
    while (appsFound < maxApps && nextApp != nullptr) {
        RokuApp app = appList[appsFound];

        const char* name = nextApp->GetText();
        if (name != nullptr) {
            strlcpy(app.name, name, 31);
        } else {
            strcpy(app.name, "");
        }

        strlcpy(app.id, safeAttrFromXMLElement(nextApp, "id"), 14);
        strlcpy(app.type, safeAttrFromXMLElement(nextApp, "type"), 5);
        strlcpy(app.version, safeAttrFromXMLElement(nextApp, "version"), 22);

        // Add app to app list, then increment next app pointer and number of apps found
        appList[appsFound++] = app;
        nextApp = nextApp->NextSiblingElement("app");
    }

    // Clean up and return number of apps found
    g_bytes_unref(responseText);
    return appsFound;
}

rokuecp::RokuApp rokuecp::getActiveRokuApp(const RokuDevice *device) {
    RokuApp app {};

    // Request active-app from device and check for errors
    std::pair<int, GBytes*> response = sendRequest(std::string(device->url) + "/query/active-app", SOUP_METHOD_GET);
    int responseCode = response.first;
    GBytes *responseText = response.second;
    if (responseCode != 0) {
        return app;
    }

    // Parse active app XML and get app element
    tinyxml2::XMLDocument appXML;
    appXML.Parse(static_cast<const char*>(g_bytes_get_data(responseText, nullptr)));
    if (appXML.Error()) {
        fprintf(stderr, "error: failed to parse Roku app info: %s\n", appXML.ErrorName());
        return app;
    }

    // Make sure app is not empty
    tinyxml2::XMLElement *parentElement = appXML.FirstChildElement("active-app");
    if (parentElement == nullptr) {
        fprintf(stderr, "error: active Roku app is empty\n");
        return app;
    }
    tinyxml2::XMLElement *appElement = parentElement->FirstChildElement("app");
    if (appElement == nullptr) {
        fprintf(stderr, "error: active Roku app is empty\n");
        return app;
    }

    // Fill in app with data from XML
    strlcpy(app.name, safeTextFromChildXMLElement(parentElement, "app"), 31);
    strlcpy(app.id, safeAttrFromXMLElement(appElement, "id"), 14);
    strlcpy(app.type, safeAttrFromXMLElement(appElement, "type"), 5);
    strlcpy(app.version, safeAttrFromXMLElement(appElement, "version"), 22);

    // Clean up and return app
    g_bytes_unref(responseText);
    return app;
}

rokuecp::RokuAppIcon rokuecp::getRokuAppIcon(const RokuDevice *device, const RokuApp *app) {
    // Create empty icon and request icon from device
    RokuAppIcon icon {};
    std::pair<int, GBytes*> request = sendRequest(std::string(device->url) + "/query/icon/" + std::string(app->id), SOUP_METHOD_GET);

    // fill icon data and size, then return
    gsize size;
    icon.data = static_cast<const unsigned char*>(g_bytes_get_data(request.second, &size));
    icon.size = static_cast<unsigned long long int>(size);
    return icon;
}

int rokuecp::sendCustomRokuInput(const RokuDevice *device, const int params, const char* names[], const char* values[]) {
    // Construct input request string with base and params
    std::string url = std::string(device->url) + "/input?";
    for (int i = 0; i < params; i++) {
        // escape disallowed URL characters
        const char* name = g_uri_escape_string(names[i], nullptr, true);
        const char* value = g_uri_escape_string(values[i], nullptr, true);

        // add parameters to URL
        url += std::string(name) + '=' + value;
        if (i < (params - 1)) {
            url += '&';
        }
    }

    // Return result of input request
    return std::get<0>(sendRequest(url, SOUP_METHOD_POST));
}

int rokuecp::rokuSearch(const RokuDevice *device, const char* keyword, const RokuSearchParams *params) {
    if (!device->hasSearchSupport) {
        fprintf(stderr, "error: device does not support search.\n");
        return -1;
    }

    std::string url = std::string(device->url) + "/search/browse?keyword=";
    if (*keyword == '\0') {
        fprintf(stderr, "error: cannot run search with empty keyword\n");
        return -2;
    }
    url += std::string(g_uri_escape_string(keyword, nullptr, true));

    switch (params->type) {
        case MOVIE:
            url += "&type=movie";
            break;
        case SHOW:
            url += "&type=tv-show";
            break;
        case PERSON:
            url += "&type=person";
            break;
        case APP:
            url += "&type=channel";
            break;
        case GAME:
            url += "&type=game";
            break;
        default:
            break;
    }

    if (params->includeUnavailable) {
        url += "&show-unavailable=true";
    }
    if (params->autoLaunch) {
        url += "&launch=true";
    }
    if (params->autoSelect) {
        url += "&match-any=true";
    }

    if (params->season != 0) {
        url += "&season=" + std::to_string(params->season);
    }
    if (*params->tmsID != '\0') {
        url += "&tmsid=" + std::string(params->tmsID);
    }

    if (**params->providerIDs != '\0') {
        url += "&provider-id=" + std::string(params->providerIDs[0]);
        for (int i = 1; i < 8; i++) {
            if (*params->providerIDs[i] != '\0') {
                url += ',' + std::string(params->providerIDs[i]);
            }
        }
    }

    return sendRequest(url, SOUP_METHOD_POST).first;
}

int rokuecp::rokuTypeString(const RokuDevice *device, const wchar_t* string) {
    int errorCode = 0;

    // Iterate through wide string
    while (*string) {
        // Convert each wide char into a null-terminated multibyte char array for URL escaping
        char cstr[MB_CUR_MAX + 1];
        int len = wctomb(cstr, *string);
        cstr[len] = '\0';

        // Escape each character and send the key "Lit_{character}" to the Roku device, checking for errors
        std::string key = std::string("Lit_") + g_uri_escape_string(cstr, nullptr, false);
        errorCode = rokuSendKey(device, key.c_str());
        if (errorCode != 0) {
            fwprintf(stderr, L"error: failed to send key %lc to Roku\n", *string);
        }

        string++;
    }

    return errorCode;
}