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

#ifndef ROKUECP_H
#define ROKUECP_H

#include <stdbool.h>
#include <stdint.h>
#include <wchar.h>

/** Information about a Roku Device */
typedef struct {
    char name[121]; /**< Name of the device, up to Roku's 120-character maximum */
    char location[61]; /**< Location of the device (like "Bedroom") up to Roku's 60-character maximum */
    char url[30]; /**< ECP URL for the device (like "http://192.168.1.162:8060/") up to 29 characters */
    char model[31]; /**< Device model, up to 30 characters */
    char serial[14]; /**< Device serial number, 13 characters */
    bool isTV; /**< true if the device is a Roku TV */
    bool isOn; /**< true if the device is currently powered on */
    bool isLimited; /**< true if the device's "Control by mobile apps" setting is "Limited" */
    bool developerMode; /**< true if the device has developer mode enabled */
    bool hasSearchSupport; /**< true if the device has search support */
    bool hasHeadphoneSupport; /**< true if the device supports Private Listening */
    bool headphonesConnected; /**< true if the device is currently in Private Listening mode */
    char resolution[8]; /**< Device resolution (like "1080p") up to 7 characters */
    char macAddress[18]; /**< Device MAC address, 17 characters */
    char softwareVersion[10]; /**< Device software version, up to 9 characters */
} RokuDevice;

/** Information about a TV channel on a Roku device */
typedef struct {
    char id[8]; /**< Channel ID, usually the channel number (like "3.1"), up to 7 characters */
    char name[8]; /**< Channel short name, up to 7 characters */
    char type[14]; /**< Channel type (like "air-digital") up to 13 characters */
    char network[25]; /**< Broadcast network label (Antenna, Cable, etc.) up to 24 characters */
    uint8_t physicalChannel; /**< integer physical RF channel number (2-69) */
    unsigned long frequency; /**< integer channel frequency in Hz (54-806 million) */
} RokuTVChannel;

/** Information about a TV program on a Roku device */
typedef struct {
    char title[112]; /**< Program title, up to 111 characters */
    char description[256]; /**< Program description, up to 255 characters */
    char rating[15]; /**< Program rating (like TV-14) up to 14 characters */
    bool hasCC; /**< true if the program has closed captions available */
} RokuTVProgram;

/**
 * Extended information about a TV channel.
 * This info is only available if the channel is currently active.
 */
typedef struct {
    RokuTVChannel channel; /**< The base channel info */
    /**
     * true if the channel is currently playing on the TV. If false,
     * nothing but the channel attribute will be populated.
     */
    bool isActive;

    RokuTVProgram program; /**< The currently playing program */
    bool signalReceived; /**< false if there is currently no signal */
    char resolution[8]; /**< Resolution at which the channel is available (like 1080i) up to 7 characters */
    uint8_t signalQuality; /**< Signal quality level from 0-100 */
    int8_t signalStrength; /**< Signal strength in dB */
} RokuExtTVChannel;

/** Information about a Roku channel (app) */
typedef struct {
    char id[14]; /**< Roku app ID, up to 13 characters */
    char name[31]; /**< App name, up to 30 characters */
    char type[5]; /**< App type (usually "appl") up to 4 characters */
    char version[22]; /**< App version, up to 21 characters */
} RokuApp;

/** A Roku channel (app) icon */
typedef struct {
    const unsigned char* data; /**< pointer to icon data */
    unsigned long size; /**< number of bytes pointed to by the data attribute */
} RokuAppIcon;

/** Information about a Roku search to be performed. All fields are optional. */
typedef struct {
    enum {
        MOVIE,
        SHOW,
        PERSON,
        APP,
        GAME,
        NONE
    } type; /**< Roku search filter (movie, TV show, person, app, game, or none) */
    bool includeUnavailable; /**< true if results that are unavailable in your region should still be included */
    char tmsID[15]; /**< TMS ID of the movie or show to search for, 14 characters */
    unsigned short season; /**< season of the show to search for */
    bool autoSelect; /**< true if the first result should automatically be selected */
    bool autoLaunch; /**< true if the first provider in providerIDs with a result found should be launched automatically */
    char providerIDs[14][8]; /**< array of up to 8 Roku app IDs (up to 13 characters) for providers to look for results from (like "12" for Netflix) */
} RokuSearchParams;

typedef struct {
    char appID[14]; /**< ID of Roku app to launch, up to 13 characters */
    char contentID[256]; /**< Optional unique identifier for a specific piece of content (empty string if none) up to 255 characters */
    enum {
        FILM,
        SERIES,
        SEASON,
        EPISODE,
        SHORT_FORM_VIDEO,
        TV_SPECIAL,
        NO_TYPE
    } mediaType; /**< Type of contentID (movie, TV show, season, episode, short-form video, TV special, or none) */
    const char** otherParamNames; /**< Array of names for other parameters to pass to app */
    const char** otherParamValues; /**< Array of values for these extra parameters */
    size_t numOtherParams; /**< Number of extra parameters */
} RokuAppLaunchParams;

/**
 * Find Roku devices on the network using SSDP
 * @param maxDevices Maximum number of devices to look for
 * @param urlStringSize Size of destination URL strings (recommended 30)
 * @param deviceList Array (of size maxDevices) of strings (size urlStringSize), which will be updated to contain the
 *                   ECP URLs of found Roku Devices. Remaining elements, if any, will be made empty strings.
 * @return Number of devices found within five seconds, or a negated gssdp error code
 */
int findRokuDevices(size_t maxDevices, size_t urlStringSize, char* deviceList[]);

/**
 * Get information about a Roku Device from its ECP URL
 * @param url The Roku Device's ECP URL (like "http://192.168.1.162:8060/")
 * @param device RokuDevice pointer to store device info in
 * @return libsoup error code for device-info request, or one of the following error codes: -1 if XML parsing failed
 *                                                                                        , -2 if the device info is empty
 *                                                                                        , -3 if the device has ECP disabled.
 */
int getRokuDevice(const char* url, RokuDevice* device);

/**
 * Send a keypress to a Roku Device, emulating the press of a button on a Roku Remote
 * @note This does not work if the device is in Limited mode.
 * @param device Pointer to RokuDevice to send the keypress to
 * @param key The key code to send to the Roku. Accepted keys are listed at
 *            https://developer.roku.com/docs/developer-program/dev-tools/external-control-api.md#keypress-key-values.
 * @return Result of POST request or one of the following error codes: -1 if the selected key isn't valid for that device type
 *                                                                   , -2 if the device is in Limited mode,
 *                                                                   , -3 if the device has ECP disabled.
 */
int rokuSendKey(const RokuDevice *device, const char* key);

/**
 * Get a list of TV channels accessible from a given Roku device
 * @note This does not work if the device is in Limited mode.
 * @param device Pointer to RokuDevice to list the channels of
 * @param maxChannels Maximum number of channels to list
 * @param channelList Array (of size maxChannels) of RokuTVChannels which will be updated to contain listed channels
 * @return Number of channels found, or one of the following error codes: -1 if the GET request failed
 *                                                                      , -2 if XML parsing failed
 *                                                                      , -3 if channel list is empty
 *                                                                      , -4 if the device is not a TV
 *                                                                      , -5 if the device is in Limited mode
 *                                                                      , -6 if the device has ECP disabled.
 */
int getRokuTVChannels(const RokuDevice *device, int maxChannels, RokuTVChannel channelList[]);

/**
 * Get either the current or last active TV channel on a given Roku device
 * @note This does not work if the device is in Limited mode.
 * @param device Pointer to RokuDevice to list the active channel of
 * @param channel Pointer to RokuExtTVChannel to store info about the current or last active TV channel
 * @return libsoup error code for tv-active-channel request, or one of the following error codes: -1 if XML parsing failed
 *                                                                                              , -2 if channel element is empty
 *                                                                                              , -3 if the device is not a TV
 *                                                                                              , -4 if the device is in Limited mode
 *                                                                                              , -5 if the device has ECP disabled.
 */
int getActiveRokuTVChannel(const RokuDevice *device, RokuExtTVChannel* channel);

/**
 * Launch a given Live TV channel on a given Roku device
 * @param device Pointer to RokuDevice to launch channel on
 * @param channel Pointer to RokuTVChannel to launch
 * @return libsoup error code for launch request, or -1 if the device has ECP disabled, or -2 if the device is not a TV.
 */
int launchRokuTVChannel(const RokuDevice* device, const RokuTVChannel* channel);

/**
 * Get a list of apps on a given Roku device
 * @note This does not work if the device is in Limited mode.
 * @param device Pointer to RokuDevice to list the apps on
 * @param maxApps Maximum number of apps to list
 * @param appList Array (of size maxApps) of RokuApps which will be updated to contain listed apps
 * @return Number of apps found, or one of the following error codes: -1 if the GET request failed
 *                                                                  , -2 if XML parsing failed
 *                                                                  , -3 if app list is empty
 *                                                                  , -4 if the device is in Limited mode
 *                                                                  , -5 if the device has ECP disabled.
*/
int getRokuApps(const RokuDevice *device, int maxApps, RokuApp appList[]);

/**
 * Get the current active app on a given Roku device
 * @param device Pointer to RokuDevice to list the active app of
 * @param app Pointer to RokuApp to store info about the current active app (Home if no app is active)
 * @return libsoup error code for active-app request, or one of the following error codes: -1 if XML parsing failed
 *                                                                                       , -2 if app element is empty
 *                                                                                       , -3 if the device has ECP disabled.
 */
int getActiveRokuApp(const RokuDevice *device, RokuApp* app);

/**
 * Launch a given app on a given Roku device
 * @param device Pointer to RokuDevice to launch the app on
 * @param params App ID and optional parameters to launch with
 * @return libsoup error code for launch request, or -1 if the device has ECP disabled.
 */
int launchRokuApp(const RokuDevice *device, const RokuAppLaunchParams* params);

/**
 * Get a given app's icon
 * @note This does not work if the device is in Limited mode.
 * @param device Pointer to RokuDevice on which the app is installed
 * @param app Pointer to RokuApp to get the icon of
 * @param icon Pointer to RokuAppIcon to store the app icon
 * @return libsoup error code for active-app request, or -1 if the device is in Limited mode, or -2 if the device has ECP disabled.
 */
int getRokuAppIcon(const RokuDevice *device, const RokuApp *app, RokuAppIcon *icon);

/**
 * Send custom input to the currently active app on a given Roku device
 * @param device Pointer to RokuDevice to send input to
 * @param params Number of parameters to send
 * @param names Array (size params) of strings with the names of the parameters
 * @param values Array (size params) of strings with the values of the parameters
 * @return Result of input POST request, or -1 if the device is in limited mode, or -2 if the device has ECP disabled.
 */
int sendCustomRokuInput(const RokuDevice *device, size_t params, const char* names[], const char* values[]);

/**
 * Run search for a movie, TV show, person, or app. Either display the results or auto-launch the first one.
 * @note This does not work if the device is in Limited mode; it will return -1.
 * @param device Pointer to RokuDevice to run the search on
 * @param keyword Movie/show title, app name, person name, or other keyword to be searched
 * @param params Pointer to RokuSearchParams describing the parameters of the search
 * @return Result of search POST request, or -1 if device does not support searches, or -2 if keyword is empty
*/
int rokuSearch(const RokuDevice *device, const char* keyword, const RokuSearchParams *params);

/**
 * Send Unicode string to Roku device as a series of keyboard keypresses
 * @note This does not work if the device is in Limited mode.
 * @note This function depends on locale. Many special characters will fail to send if the standard C locale is used.
 * @param device Pointer to RokuDevice to send the string to
 * @param string Wide Unicode string to send
 * @return Result of the last keypress request, or -1 if the device is in Limited mode, or -2 if the device has ECP disabled.
 */
int rokuTypeString(const RokuDevice *device, const wchar_t* string);

#endif //ROKUECP_H
