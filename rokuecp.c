/*
 * rokuecp: Interact with Roku devices using ECP
 * Copyright 2025 Ben Westover <me@benthetechguy.net>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "rokuecp.h"
#include <libgssdp/gssdp-resource-browser.h>
#include <libsoup/soup-session.h>
#include <libxml/tree.h>

// Not all platforms have strlcpy (ahem... MinGW)
#ifdef NO_STRLCPY
size_t strlcpy(char* dest, const char* src, size_t size) {
    strncpy(dest, src, size - 1);
    dest[size - 1] = '\0';
    size_t srclen = strlen(src);
    if (srclen < size - 1) {
        return srclen;
    }
    return size - 1;
}
#endif

/** @internal
 * SSDP quit GMainLoop callback: Run after 5 seconds in the loop looking for Roku devices, quits the loop.
 * @param user_data Pointer to the main loop.
 * @return Whether quitting the loop was successful.
 */
static gboolean ssdpQuitGMainLoopCallback(gpointer user_data) {
    g_main_loop_quit(user_data);
    return G_SOURCE_REMOVE;
}

/** @internal
 * User data to pass to the SSDP resource_available callback
 */
struct ssdpResourceAvailableCallbackData {
    GMainLoop* mainLoop; /**< Pointer to the main loop */
    int devicesFound; /**< Number of devices found */
    char** deviceList; /**< List of device URL strings to populate */
    const size_t maxDevices; /**< Max number of devices that can be found */
    const size_t deviceStrSize; /**< Max size of each URL string */
};

/** @internal
 * SSDP resource_available callback: When SSDP finds a Roku device, add it to the deviceList and increment devicesFound.
 * @param self Pointer to the SSDP resource browser.
 * @param usn USN of the found resource.
 * @param locations List of strings with URLs to the resource.
 * @param user_data Pointer to ssdpResourceAvailableCallbackData struct with data to update
 */
static void ssdpResourceAvailableCallback(GSSDPResourceBrowser* self, const char* usn, const GList* locations, void* user_data) {
    struct ssdpResourceAvailableCallbackData* data = user_data;
    // Stop if the end of the device list has been reached
    if (data->devicesFound >= data->maxDevices - 1) {
        g_main_loop_quit(data->mainLoop);
    }

    // Update the list and increment devicesFound, then update the device find state
    strlcpy(data->deviceList[data->devicesFound++], locations->data, data->deviceStrSize);
}

/** @internal
 * Send a GET or POST request to the given URL
 * @param url string containing the URL to request
 * @param method type of request to send (e.g. "GET" or "POST")
 * @param response Pointer to GBytes pointer, to send response data to
 * @return libsoup error code, or HTTP status code, or 0 if the status is 200 OK
 */
static int sendRequest(const char* url, const char* method, GBytes** response) {
    // Set up session and send request
    SoupSession* session = soup_session_new();
    SoupMessage* msg = soup_message_new(method, url);
    GError* error = NULL;
    GBytes* request = soup_session_send_and_read(session, msg, NULL, &error);
    if (response) {
        *response = request;
    } else {
        g_bytes_unref(request);
    }
    SoupStatus status = soup_message_get_status(msg);

    // Clean up and report error, if any
    g_object_unref(session);
    g_object_unref(msg);
    if (error) {
        int errorCode = error->code;
        g_error_free(error);
        return errorCode;
    }
    if (status == SOUP_STATUS_OK) {
        return 0;
    }
    return status;
}

/** @internal
 * A mapping from an XML element name to a string it will be copied to
 */
struct xmlElementToStringMap {
    const char* elementName; /**< Name of the XML element to copy content of */
    char* destString; /**< String to copy to */
    size_t destSize; /**< Size of destString in bytes */
};

/** @internal
 * Extract data from an XML node and copy results to specified strings
 * @param parentElement XML element to traverse child elements of
 * @param attrMode true if extracting from element attributes, false if extracting from element content
 * @param map Specification for what elements to look for and where to copy them
 * @param mapSize Number of mappings in the array
 */
static void fillFromXML(const xmlNode* parentElement, bool attrMode, struct xmlElementToStringMap map[], const size_t mapSize) {
    // Start by making every dest string empty to avoid returning garbage data
    for (size_t i = 0; i < mapSize; i++) {
        *map[i].destString = '\0';
    }

    if (attrMode) {
        // Get every attribute listed in the map and copy to corresponding string
        xmlChar* value;
        for (size_t i = 0; i < mapSize; i++) {
            if (xmlNodeGetAttrValue(parentElement, (const xmlChar*) map[i].elementName, NULL, &value) == 0) {
                strlcpy(map[i].destString, (char*) value, map[i].destSize);
                xmlFree(value);
            }
        }
    } else {
        // Go through every child element and process those that match an element in the map
        for (const xmlNode* node = parentElement->children; node != NULL; node = node->next) {
            for (size_t i = 0; i < mapSize; i++) {
                if (strcmp((char*) node->name, map[i].elementName) == 0) {
                    xmlChar* content = xmlNodeGetContent(node);
                    if (content) {
                        strlcpy(map[i].destString, (char*) content, map[i].destSize);
                        xmlFree(content);
                    }
                    break;
                }
            }
        }
    }
}

int findRokuDevices(const char* interface, const size_t maxDevices, const size_t urlStringSize, char* deviceList[]) {
    // Set up gssdp to look for Roku devices
    GError* error = NULL;
    GSSDPClient* ssdpClient = gssdp_client_new_full(interface, NULL, 0, GSSDP_UDA_VERSION_1_0, &error);
    if (error) {
        int errorCode = error->code;
        g_object_unref(ssdpClient);
        g_error_free(error);
        return -errorCode;
    }
    GSSDPResourceBrowser* rokuFinder = gssdp_resource_browser_new(ssdpClient, "roku:ecp");

    // Set up detection loop and callback
    GMainLoop* mainLoop = g_main_loop_new(NULL, FALSE);
    struct ssdpResourceAvailableCallbackData callbackData = {mainLoop, 0, deviceList, maxDevices, urlStringSize};
    g_signal_connect(rokuFinder, "resource-available", G_CALLBACK(ssdpResourceAvailableCallback), &callbackData);

    // Activate Roku finder and run detection loop for 5 seconds or until list is full
    gssdp_resource_browser_set_active(rokuFinder, TRUE);
    g_timeout_add_seconds(5, ssdpQuitGMainLoopCallback, mainLoop);
    g_main_loop_run(mainLoop);

    // Cleanup gssdp when loop is done and return devices found
    g_main_loop_unref(mainLoop);
    g_object_unref(rokuFinder);
    g_object_unref(ssdpClient);
    return callbackData.devicesFound;
}

int getRokuDevice(const char* url, RokuDevice* device) {
    // Fill in the device URL
    strlcpy(device->url, url, sizeof(device->url));

    // Request device-info from device and check for errors
    char queryURL[(sizeof(device->url) + sizeof("/query/device-info")) / sizeof(char) - 1];
    strcpy(queryURL, url);
    strcat(queryURL, "/query/device-info");
    GBytes* response;
    int httpError = sendRequest(queryURL, SOUP_METHOD_GET, &response);
    if (httpError == SOUP_STATUS_UNAUTHORIZED) {
        g_bytes_unref(response);
        return -3;
    }
    if (httpError) {
        g_bytes_unref(response);
        return httpError;
    }

    // Parse XML response and get device-info element
    xmlDocPtr doc = xmlReadDoc(g_bytes_get_data(response, NULL), "device-info.xml", "UTF-8", 0);
    g_bytes_unref(response);
    if (!doc) {
        return -2;
    }
    xmlNodePtr deviceInfoElement = xmlDocGetRootElement(doc);
    if (!deviceInfoElement) {
        xmlFreeDoc(doc);
        return -3;
    }

    // Fill in device fields based on device-info response
    char tmpPowerMode[8];
    char tmpIsTV[5];
    char tmpEcpMode[8];
    char tmpDeveloperMode[5];
    char tmpSearchEnabled[5];
    char tmpHasPrivateListening[5];
    char tmpHeadphonesConnected[5];
    struct xmlElementToStringMap map[14] = {
        {"user-device-name", device->name, sizeof(device->name) / sizeof(char)},
        {"user-device-location", device->location, sizeof(device->location) / sizeof(char)},
        {"friendly-model-name", device->model, sizeof(device->model) / sizeof(char)},
        {"serial-number", device->serial, sizeof(device->serial) / sizeof(char)},
        {"ui-resolution", device->resolution, sizeof(device->resolution) / sizeof(char)},
        {"wifi-mac", device->macAddress, sizeof(device->macAddress) / sizeof(char)},
        {"software-version", device->softwareVersion, sizeof(device->softwareVersion) / sizeof(char)},
        {"power-mode", tmpPowerMode, 8},
        {"is-tv", tmpIsTV, 5},
        {"ecp-setting-mode", tmpEcpMode, 8},
        {"developer-enabled", tmpDeveloperMode, 5},
        {"search-enabled", tmpSearchEnabled, 5},
        {"supports-private-listening", tmpHasPrivateListening, 5},
        {"headphones-connected", tmpHeadphonesConnected, 5},
    };
    fillFromXML(deviceInfoElement, false, map, sizeof(map) / sizeof(*map));
    device->isOn = strcmp("PowerOn", tmpPowerMode) == 0;
    device->isTV = strcmp("true", tmpIsTV) == 0;
    device->isLimited = strcmp("limited", tmpIsTV) == 0;
    device->developerMode = strcmp("true", tmpDeveloperMode) == 0;
    device->hasSearchSupport = strcmp("true", tmpSearchEnabled) == 0;
    device->hasHeadphoneSupport = strcmp("true", tmpHasPrivateListening) == 0;
    device->headphonesConnected = strcmp("true", tmpHeadphonesConnected) == 0;

    // Clean up and return
    xmlFreeDoc(doc);
    return 0;
}

int rokuSendKey(const RokuDevice* device, const char* key) {
    // Disallow sending keys meant for TVs to non-TV devices
    if (!device->isTV) {
        for (int i = 0; i < 12; i++) {
            const char* tvOnlyKeys[12] = {"VolumeUp", "VolumeDown", "VolumeMute", "PowerOff", "ChannelUp", "ChannelDown",
                                          "InputTuner", "InputHDMI1", "InputHDMI2", "InputHDMI3", "InputHDMI4", "InputAV1"};
            if (strcmp(key, tvOnlyKeys[i]) == 0) {
                return -1;
            }
        }
    }
    if (device->isLimited) {
        return -2;
    }
    // Return result of keypress request
    char* url = malloc(sizeof("/keypress/") + (strlen(device->url) + strlen(key)) * sizeof(char));
    strcpy(url, device->url);
    strcat(url, "/keypress/");
    strcat(url, key);
    int result = sendRequest(url, SOUP_METHOD_POST, NULL);
    free(url);
    if (result == SOUP_STATUS_UNAUTHORIZED) {
        return -3;
    }
    return result;
}

int getRokuTVChannels(const RokuDevice* device, const int maxChannels, RokuTVChannel channelList[]) {
    if (!device->isTV) {
        return -4;
    }
    if (device->isLimited) {
        return -5;
    }

    // Request tv-channels from device and check for errors
    char queryURL[(sizeof(device->url) + sizeof("/query/tv-channels")) / sizeof(char) - 1];
    strcpy(queryURL, device->url);
    strcat(queryURL, "/query/tv-channels");
    GBytes* response;
    int httpError = sendRequest(queryURL, SOUP_METHOD_GET, &response);
    if (httpError == SOUP_STATUS_UNAUTHORIZED) {
        g_bytes_unref(response);
        return -6;
    }
    if (httpError) {
        g_bytes_unref(response);
        return -1;
    }

    // Parse channel list XML
    xmlDocPtr doc = xmlReadDoc(g_bytes_get_data(response, NULL), "tv-channels.xml", "UTF-8", 0);
    g_bytes_unref(response);
    if (!doc) {
        return -2;
    }
    xmlNodePtr rootElement = xmlDocGetRootElement(doc);
    if (!rootElement) {
        xmlFreeDoc(doc);
        return -3;
    }

    // Iterate through channels and add them to the channel list
    int channelsFound = 0;
    for (const xmlNode* nextChannel = rootElement->children; nextChannel != NULL && channelsFound < maxChannels; nextChannel = nextChannel->next) {
        // Skip non-element nodes
        if (nextChannel->type != XML_ELEMENT_NODE) {
            continue;
        }

        char tmpPhysicalChannel[3];
        char tmpFrequency[7];
        struct xmlElementToStringMap map[] = {
            {"channel-id", channelList[channelsFound].id, sizeof(channelList->id) / sizeof(char)},
            {"broadcast-network-label", channelList[channelsFound].network, sizeof(channelList->network) / sizeof(char)},
            {"name", channelList[channelsFound].name, sizeof(channelList->name) / sizeof(char)},
            {"type", channelList[channelsFound].type, sizeof(channelList->type) / sizeof(char)},
            {"physical-channel", tmpPhysicalChannel, 3},
            {"physical-frequency", tmpFrequency, 7},
        };
        fillFromXML(nextChannel, false, map, sizeof(map) / sizeof(*map));
        if (*tmpPhysicalChannel) {
            channelList[channelsFound].physicalChannel = strtoul(tmpPhysicalChannel, NULL, 10);
        } else {
            channelList[channelsFound].physicalChannel = 0;
        }
        if (*tmpFrequency) {
            channelList[channelsFound].frequency = strtoul(tmpFrequency, NULL, 10) * 1000UL;
        } else {
            channelList[channelsFound].frequency = 0;
        }

        channelsFound++;
    }

    // Clean up and return number of channels found
    xmlFreeDoc(doc);
    return channelsFound;
}

int getActiveRokuTVChannel(const RokuDevice* device, RokuExtTVChannel* channel) {
    if (!device->isTV) {
        return -3;
    }
    if (device->isLimited) {
        return -4;
    }

    // Request tv-active-channel from device and check for errors
    char queryURL[(sizeof(device->url) + sizeof("/query/tv-active-channel")) / sizeof(char) - 1];
    strcpy(queryURL, device->url);
    strcat(queryURL, "/query/tv-active-channel");
    GBytes* response;
    int httpError = sendRequest(queryURL, SOUP_METHOD_GET, &response);
    if (httpError ==  SOUP_STATUS_UNAUTHORIZED) {
        return -5;
    }
    if (httpError) {
        g_bytes_unref(response);
        return httpError;
    }

    // Parse active channel XML and get channel element
    xmlDocPtr doc = xmlReadDoc(g_bytes_get_data(response, NULL), "tv-active-channel.xml", "UTF-8", 0);
    g_bytes_unref(response);
    if (!doc) {
        return -1;
    }
    xmlNodePtr rootElement = xmlDocGetRootElement(doc);
    if (!rootElement) {
        xmlFreeDoc(doc);
        return -2;
    }
    // Make sure channel is not empty
    xmlNode* channelElement = rootElement->children;
    if (!channelElement) {
        xmlFreeDoc(doc);
        return -2;
    }
    if (channelElement->type != XML_ELEMENT_NODE) {
        channelElement = channelElement->next;
    }

    // Fill in channel with data from XML
    char tmpPhysicalChannel[3];
    char tmpFrequency[7];
    char tmpActive[6];
    struct xmlElementToStringMap map[] = {
        {"channel-id", channel->channel.id, sizeof(channel->channel.id) / sizeof(char)},
        {"broadcast-network-label", channel->channel.network, sizeof(channel->channel.network) / sizeof(char)},
        {"name", channel->channel.name, sizeof(channel->channel.name) / sizeof(char)},
        {"type", channel->channel.type, sizeof(channel->channel.type) / sizeof(char)},
        {"physical-channel", tmpPhysicalChannel, 3},
        {"physical-frequency", tmpFrequency, 7},
        {"active-input", tmpActive, 6},
    };
    fillFromXML(channelElement, false, map, sizeof(map) / sizeof(*map));
    if (*tmpPhysicalChannel) {
        channel->channel.physicalChannel = strtoul(tmpPhysicalChannel, NULL, 10);
    } else {
        channel->channel.physicalChannel = 0;
    }
    if (*tmpFrequency) {
        channel->channel.frequency = strtoul(tmpFrequency, NULL, 10) * 1000UL;
    } else {
        channel->channel.frequency = 0;
    }

    // Only fill in the rest of the data if the channel is active (inactive channels have these fields blank)
    channel->isActive = strcmp("true", tmpActive) == 0;
    if (channel->isActive) {
        char tmpHasCC[5];
        char tmpSignalState[5];
        char tmpSignalQuality[4];
        char tmpSignalStrength[5];
        struct xmlElementToStringMap ifActiveMap[] = {
            {"program-title", channel->program.title, sizeof(channel->program.title) / sizeof(char)},
            {"program-description", channel->program.description, sizeof(channel->program.description) / sizeof(char)},
            {"program-ratings", channel->program.rating, sizeof(channel->program.rating) / sizeof(char)},
            {"program-has-cc", tmpHasCC, 5},
            {"signal-mode", channel->resolution, sizeof(channel->resolution) / sizeof(char)},
            {"signal-state", tmpSignalState, 5},
            {"signal-quality", tmpSignalQuality, 4},
            {"signal-strength", tmpSignalStrength, 5},
        };
        fillFromXML(channelElement, false, ifActiveMap, sizeof(ifActiveMap) / sizeof(*map));
        channel->program.hasCC = strcmp("true", tmpHasCC) == 0;
        channel->signalReceived = strcmp("none", tmpSignalState);
        if (*tmpSignalQuality) {
            channel->signalQuality = strtoul(tmpSignalQuality, NULL, 10);
        } else {
            channel->signalQuality = 0;
        }
        if (*tmpSignalStrength) {
            channel->signalStrength = (int8_t) strtol(tmpSignalStrength, NULL, 10);
        } else {
            channel->signalStrength = 0;
        }
    } else {
        *channel->program.title = '\0';
        *channel->program.description = '\0';
        *channel->program.rating = '\0';
        *channel->resolution = '\0';
        channel->program.hasCC = false;
        channel->signalReceived = false;
        channel->signalQuality = 0;
        channel->signalStrength = 0;
    }

    // Clean up and return active channel
    xmlFreeDoc(doc);
    return 0;
}

int launchRokuTVChannel(const RokuDevice* device, const RokuTVChannel* channel) {
    if (!device->isTV) {
        return -2;
    }
    const char* paramNames[3] = {"chan", "lcn", "ch"};
    const char* paramValues[3] = {channel->id, channel->id, channel->id};
    RokuAppLaunchParams launchParams = {
        "tvinput.dtv",
        "",
        NO_TYPE,
        paramNames,
        paramValues,
        3
    };
    return launchRokuApp(device, &launchParams);
}

int getRokuApps(const RokuDevice* device, const int maxApps, RokuApp appList[]) {
    if (device->isLimited) {
        return -4;
    }

    // Request apps from device and check for errors
    char queryURL[(sizeof(device->url) + sizeof("/query/apps")) / sizeof(char) - 1];
    strcpy(queryURL, device->url);
    strcat(queryURL, "/query/apps");
    GBytes* response;
    int httpError = sendRequest(queryURL, SOUP_METHOD_GET, &response);
    if (httpError == SOUP_STATUS_UNAUTHORIZED) {
        g_bytes_unref(response);
        return -5;
    }
    if (httpError) {
        g_bytes_unref(response);
        return -1;
    }

    // Parse app list XML and get app element
    xmlDocPtr doc = xmlReadDoc(g_bytes_get_data(response, NULL), "apps.xml", "UTF-8", 0);
    g_bytes_unref(response);
    if (!doc) {
        return -2;
    }
    xmlNodePtr rootElement = xmlDocGetRootElement(doc);
    if (!rootElement) {
        xmlFreeDoc(doc);
        return -3;
    }

    // Iterate through apps and add them to the app list
    int appsFound = 0;
    for (const xmlNode* nextApp = rootElement->children; nextApp != NULL && appsFound < maxApps; nextApp = nextApp->next) {
        // Skip non-element nodes
        if (nextApp->type != XML_ELEMENT_NODE) {
            continue;
        }
        // Get app name from XML element content
        xmlChar* name = xmlNodeGetContent(nextApp);
        if (name) {
            strlcpy(appList[appsFound].name, (char*) name, sizeof(appList->name) / sizeof(char));
            xmlFree(name);
        } else {
            *appList[appsFound].name = '\0';
        }
        // Fill in app with data from XML
        struct xmlElementToStringMap map[] = {
            {"id", appList[appsFound].id, sizeof(appList->id) / sizeof(char)},
            {"type", appList[appsFound].type, sizeof(appList->type) / sizeof(char)},
            {"version", appList[appsFound].version, sizeof(appList->version) / sizeof(char)},
        };
        fillFromXML(nextApp, true, map, sizeof(map) / sizeof(*map));

        appsFound++;
    }

    // Clean up and return number of apps found
    xmlFreeDoc(doc);
    return appsFound;
}

int getActiveRokuApp(const RokuDevice* device, RokuApp* app) {
    // Request active-app from device and check for errors
    char queryURL[(sizeof(device->url) + sizeof("/query/active-app")) / sizeof(char) - 1];
    strcpy(queryURL, device->url);
    strcat(queryURL, "/query/active-app");
    GBytes* response;
    int httpError = sendRequest(queryURL, SOUP_METHOD_GET, &response);
    if (httpError == SOUP_STATUS_UNAUTHORIZED) {
        g_bytes_unref(response);
        return -3;
    }
    if (httpError) {
        g_bytes_unref(response);
        return httpError;
    }

    // Parse active app XML and get app element
    xmlDocPtr doc = xmlReadDoc(g_bytes_get_data(response, NULL), "active-app.xml", "UTF-8", 0);
    g_bytes_unref(response);
    if (!doc) {
        return -1;
    }
    xmlNodePtr rootElement = xmlDocGetRootElement(doc);
    if (!rootElement) {
        xmlFreeDoc(doc);
        return -2;
    }
    // Make sure app is not empty
    xmlNodePtr appElement = rootElement->children;
    if (!appElement) {
        xmlFreeDoc(doc);
        return -2;
    }
    if (appElement->type != XML_ELEMENT_NODE) {
        appElement = appElement->next;
    }

    // Get app name from XML element content
    xmlChar* name = xmlNodeGetContent(appElement);
    if (name) {
        strlcpy(app->name, (char*) name, sizeof(app->name) / sizeof(char));
        xmlFree(name);
    } else {
        *app->name = '\0';
    }
    // Fill in app with data from XML
    struct xmlElementToStringMap map[] = {
        {"id", app->id, sizeof(app->id) / sizeof(char)},
        {"type", app->type, sizeof(app->type) / sizeof(char)},
        {"version", app->version, sizeof(app->version) / sizeof(char)},
    };
    fillFromXML(appElement, true, map, sizeof(map) / sizeof(*map));

    // Clean up and return app
    xmlFreeDoc(doc);
    return 0;
}

int launchRokuApp(const RokuDevice* device, const RokuAppLaunchParams* params) {
    GString* url = g_string_sized_new((strlen(device->url) + strlen(params->appID)) * sizeof(char) + sizeof("/launch/"));
    g_string_assign(url, device->url);
    g_string_append(url, "/launch/");
    g_string_append(url, params->appID);

    bool hasContentID = *params->contentID != '\0';
    bool hasMediaType = params->mediaType != NO_TYPE;
    bool hasOtherParams = params->numOtherParams != 0;

    if (hasContentID) {
        g_string_append(url, "?contentId=");
        g_string_append_uri_escaped(url, params->contentID, NULL, TRUE);
    }
    if (hasMediaType) {
        if (hasContentID) {
            g_string_append_c(url, '&');
        } else {
            g_string_append_c(url, '?');
        }
    }
    switch (params->mediaType) {
        case FILM:
            g_string_append(url, "MediaType=movie");
            break;
        case SERIES:
            g_string_append(url, "MediaType=series");
            break;
        case SEASON:
            g_string_append(url, "MediaType=season");
            break;
        case EPISODE:
            g_string_append(url, "MediaType=episode");
            break;
        case SHORT_FORM_VIDEO:
            g_string_append(url, "MediaType=shortFormVideo");
            break;
        case TV_SPECIAL:
            g_string_append(url, "MediaType=tvSpecial");
            break;
        default:
            break;
    }

    if (hasOtherParams) {
        if (hasContentID || hasMediaType) {
            g_string_append_c(url, '&');
        } else {
            g_string_append_c(url, '?');
        }
    }
    for (size_t i = 0; i < params->numOtherParams; i++) {
        g_string_append_uri_escaped(url, params->otherParamNames[i], NULL, TRUE);
        g_string_append_c(url, '=');
        g_string_append_uri_escaped(url, params->otherParamValues[i], NULL, TRUE);
        if (i != params->numOtherParams - 1) {
            g_string_append_c(url, '&');
        }
    }

    int httpError = sendRequest(url->str, SOUP_METHOD_POST, NULL);
    g_string_free(url, TRUE);
    if (httpError == SOUP_STATUS_UNAUTHORIZED) {
        return -1;
    }
    return httpError;
}

int getRokuAppIcon(const RokuDevice* device, const RokuApp* app, RokuAppIcon* icon) {
    if (device->isLimited) {
        return -1;
    }
    // Request icon from device
    char* url = malloc(sizeof("/query/icon/") + (strlen(device->url) + strlen(app->id)) * sizeof(char));
    strcpy(url, device->url);
    strcat(url, "/query/icon/");
    strcat(url, app->id);
    GBytes* response;
    int httpError = sendRequest(url, SOUP_METHOD_GET, &response);
    free(url);

    // fill icon data and size while freeing GBytes
    gsize size;
    icon->data = g_bytes_unref_to_data(response, &size);
    icon->size = size;
    if (httpError == SOUP_STATUS_UNAUTHORIZED) {
        return -2;
    }
    return httpError;
}

int sendCustomRokuInput(const RokuDevice* device, const size_t params, const char* names[], const char* values[]) {
    if (device->isLimited) {
        return -1;
    }
    // Construct input request string with base and params
    GString* url = g_string_sized_new(strlen(device->url) * sizeof(char) + sizeof("/input?"));
    g_string_assign(url, device->url);
    g_string_append(url, "/input?");
    for (int i = 0; i < params; i++) {
        // add parameters to URL, escaping disallowed URL characters
        g_string_append_uri_escaped(url, names[i], NULL, TRUE);
        g_string_append_c(url, '=');
        g_string_append_uri_escaped(url, values[i], NULL, TRUE);
        if (i < params - 1) {
            g_string_append_c(url, '&');
        }
    }

    // Clean up and return result of input request
    int httpError = sendRequest(url->str, SOUP_METHOD_POST, NULL);
    g_string_free(url, TRUE);
    if (httpError == SOUP_STATUS_UNAUTHORIZED) {
        return -2;
    }
    return httpError;
}

int rokuSearch(const RokuDevice* device, const char* keyword, const RokuSearchParams* params) {
    if (!device->hasSearchSupport || device->isLimited) {
        return -1;
    }

    GString* url = g_string_sized_new((strlen(device->url) + strlen(keyword)) * sizeof(char) + sizeof("/search/browse?keyword="));
    g_string_assign(url, device->url);
    g_string_append(url, "/search/browse?keyword=");
    if (*keyword == '\0') {
        g_string_free(url, TRUE);
        return -2;
    }
    g_string_append_uri_escaped(url, keyword, NULL, TRUE);

    switch (params->type) {
        case MOVIE:
            g_string_append(url, "&type=movie");
            break;
        case SHOW:
            g_string_append(url, "&type=tv-show");
            break;
        case PERSON:
            g_string_append(url, "&type=person");
            break;
        case APP:
            g_string_append(url, "&type=channel");
            break;
        case GAME:
            g_string_append(url, "&type=game");
            break;
        default:
            break;
    }

    if (params->includeUnavailable) {
        g_string_append(url, "&show-unavailable=true");
    }
    if (params->autoLaunch) {
        g_string_append(url, "&launch=true");
    }
    if (params->autoSelect) {
        g_string_append(url, "&match-any=true");
    }

    if (params->season != 0) {
        g_string_append(url, "&season=");
        g_string_append_printf(url, "%u", params->season);
    }
    if (*params->tmsID) {
        g_string_append(url, "&tmsid=");
        g_string_append(url, params->tmsID);
    }

    if (*params->providerIDs[0]) {
        g_string_append(url, "&provider-id=");
        g_string_append(url, params->providerIDs[0]);
        for (int i = 1; i < 8; i++) {
            if (*params->providerIDs[i]) {
                g_string_append_c(url, ',');
                g_string_append(url, params->providerIDs[i]);
            }
        }
    }

    int httpError = sendRequest(url->str, SOUP_METHOD_POST, NULL);
    g_string_free(url, TRUE);
    if (httpError == SOUP_STATUS_UNAUTHORIZED) {
        return -1;
    }
    return httpError;
}

int rokuTypeString(const RokuDevice* device, const wchar_t* string) {
    if (device->isLimited) {
        return -1;
    }
    int errorCode = 0;

    // Iterate through wide string
    while (*string) {
        // Convert each wide char into a multibyte string for URL escaping
        char mbstr[MB_CUR_MAX + 1];
        int len = wctomb(mbstr, *string);
        // If this is an "invalid character" in the current locale, skip it
        if (len == -1) {
            string++;
            continue;
        }
        mbstr[len] = '\0';

        // Escape each character and send the key "Lit_{character}" to the Roku device, checking for errors
        const char* escaped = g_uri_escape_string(mbstr, NULL, FALSE);
        char* key = malloc(sizeof("Lit_") + strlen(escaped) * sizeof(char));
        strcpy(key, "Lit_");
        strcat(key, escaped);
        errorCode = rokuSendKey(device, key);
        free(key);
        if (errorCode == -3) {
            return -2;
        }

        string++;
    }

    return errorCode;
}
