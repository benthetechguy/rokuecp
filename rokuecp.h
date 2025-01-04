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

#ifdef __cplusplus
/** RokuECP namespace */
namespace rokuecp {
    extern "C" {
#else
#include <wchar.h>
#include <stdbool.h>
#endif
        /** Object containing information about a Roku Device */
        typedef struct {
            char name[122]; /**< C-string name of the device, up to Roku's 120-character maximum */
            char location[16]; /**< C-string location of the device (like "Bedroom") up to 15 characters */
            char url[30]; /**< C-string ECP URL for the device (like "http://192.168.1.162:8060/") up to 29 characters */
            char model[32]; /**< C-string device model, up to 31 characters */
            char serial[14]; /**< C-string device serial number, 13 characters */
            bool isTV; /**< bool which is true if the device is a Roku TV */
            bool isOn; /**< bool which is true if the device is currently powered on */
            bool developerMode; /**< bool which is true if the device has developer mode enabled */
            bool hasSearchSupport; /**< bool which is true if the device has search support */
            bool hasHeadphoneSupport; /**< bool which is true if the device supports Private Listening */
            bool headphonesConnected; /**< bool which is true if the device is currently in Private Listening mode */
            char resolution[8]; /**< C-string device resolution (like "1080p") up to 7 characters */
            char macAddress[18]; /**< C-string device MAC address, 17 characters */
            char softwareVersion[10]; /**< C-string device software version, up to 9 characters */
        } RokuDevice;

        /** Object containing information about a TV channel on a Roku device */
        typedef struct {
            char id[8]; /**< C-string channel ID, usually the channel number (like "3.1"), up to 7 characters */
            char name[8]; /**< C-string channel short name, up to 7 characters */
            char type[14]; /**< C-string channel type (like "air-digital") up to 13 characters */
            bool isHidden; /**< bool which is true if the channel was hidden by the user */
            bool isFavorite; /**< bool which is true if the channel is on the user's favorite list */
            unsigned short int physicalChannel; /**< integer physical RF channel number (2-69) */
            unsigned long int frequency; /**< integer channel frequency in Hz (6-8 million) */
        } RokuTVChannel;

        /** Object containing information about a TV program on a Roku device */
        typedef struct {
            char title[112]; /**< C-string program title, up to 111 characters */
            char description[256]; /**< C-string program description, up to 255 characters */
            char rating[15]; /**< C-string program rating (like TV-14) up to 14 characters */
            bool hasCC; /**< bool which is true if the program has closed captions available */
        } RokuTVProgram;

        /**
         * Object containing extended information about a TV channel.
         * This info is only available if the channel is currently active.
         */
        typedef struct {
            RokuTVChannel channel; /**< RokuTVChannel object representing the channel */
            /**
             * bool which is true if the channel is currently playing on the TV.
             * If it is false, nothing but the channel attribute will be populated.
             */
            bool isActive;

            RokuTVProgram program; /**< RokuTVProgram object representing the currently playing program */
            bool signalReceived; /**< bool which is false if there is currently no signal */
            char resolution[8]; /**< C-string resolution at which the channel is available (like 1080i) up to 7 characters */
            unsigned short int signalQuality; /**< integer signal quality level from 0-100 */
            signed short int signalStrength; /**< signed integer signal strength in dB */
        } RokuExtTVChannel;

        /** Object containing information about a Roku channel (app) */
        typedef struct {
            char id[14]; /**< C-string Roku app ID, up to 13 characters */
            char name[31]; /**< C-string app name, up to 30 characters */
            char type[5]; /**< C-string app type (usually "appl") up to 4 characters */
            char version[22]; /**< C-string app version, up to 21 characters */
        } RokuApp;

        /** Object containing a Roku channel (app) icon */
        typedef struct {
            const unsigned char* data; /**< pointer to a series of bytes (unsigned char) with the icon data */
            unsigned long long int size; /**< 64-bit integer number of bytes pointed to by the data attribute */
        } RokuAppIcon;

        /** enum Roku search filter (movie, TV show, person, app, game, or none) */
        typedef enum {
            MOVIE,
            SHOW,
            PERSON,
            APP,
            GAME,
            NONE
        } RokuSearchType;

        /** Object containing information about a Roku search to be performed. All fields are optional. */
        typedef struct {
            RokuSearchType type; /**< enum Roku search filter */
            bool includeUnavailable; /**< bool which is true if results that are unavailable in your region should still be included */
            char tmsID[15]; /**< C-string containing the TMS ID of the movie or show to search for, 14 characters */
            unsigned short int season; /**< integer season of the show to search for */
            bool autoSelect; /**< bool which is true if the first result should automatically be selected */
            bool autoLaunch; /**< bool which is true if the first provider in providerIDs with a result found should be launched automatically */
            char providerIDs[14][8]; /**< array of up to 8 C-strings containing Roku app IDs (up to 13 characters) for providers to look for results from (like "12" for Netflix) */
        } RokuSearchParams;

        /**
         * Find Roku devices on the network using SSDP
         * @param maxDevices Maximum integer number of devices to look for
         * @param deviceList Array (of size maxDevices) of C-strings (size 30), which will be updated to contain the
         *                   ECP URLs of found Roku Devices. Remaining elements, if any, will be made empty strings.
         * @return Integer number of devices found within five seconds, or a negative gssdp error code
         */
        int findRokuDevices(int maxDevices, char* deviceList[]);

        /**
         * Get information about a Roku Device from its ECP URL
         * @param url C-string containing the Roku Device's ECP URL (like "http://192.168.1.162:8060/")
         * @return RokuDevice object representing the device
         */
        RokuDevice getRokuDevice(const char* url);

        /**
         * Send a keypress to a Roku Device, emulating the press of a button on a Roku Remote
         * @param device Pointer to RokuDevice object representing the chosen device to send the keypress to
         * @param key C-string containing the key code to send to the Roku. Accepted keys are listed at
         *            https://developer.roku.com/docs/developer-program/dev-tools/external-control-api.md#keypress-key-values.
         * @return Result of POST request or -1 if the selected key isn't valid for that device type
         */
        int rokuSendKey(const RokuDevice *device, const char* key);

        /**
         * Get a list of TV channels accessible from a given Roku device
         * @param device Pointer to RokuDevice object representing the device to list the channels of
         * @param maxChannels Integer maximum number of channels to list
         * @param channelList Array (of size maxChannels) of RokuTVChannel objects which will be updated to contain listed channels
         * @return Integer number of channels found, or one of the following error codes: -1 if the GET request failed
         *                                                                              , -2 if XML parsing failed
         *                                                                              , -3 if channel list is empty
         *                                                                              , -4 if the device is not a TV.
         */
        int getRokuTVChannels(const RokuDevice *device, int maxChannels, RokuTVChannel channelList[]);

        /**
         * Get either the current or last active TV channel on a given Roku device
         * @param device Pointer to RokuDevice object representing the device to list the active channel of
         * @return RokuTVChannel object representing the current or last active TV channel
         */
        RokuExtTVChannel getActiveRokuTVChannel(const RokuDevice *device);

        /**
         * Get a list of apps on a given Roku device
         * @param device Pointer to RokuDevice object representing the device to list the apps on
         * @param maxApps Integer maximum number of apps to list
         * @param appList Array (of size maxApps) of RokuApp objects which will be updated to contain listed apps
         * @return Integer number of apps found, or -1 if the GET request failed, -2 if XML parsing failed, or -3 if app list is empty
        */
        int getRokuApps(const RokuDevice *device, int maxApps, RokuApp appList[]);

        /**
         * Get the current active app on a given Roku device
         * @param device Pointer to RokuDevice object representing the device to list the active app of
         * @return RokuApp object representing the current active app (Home if no app is active)
         */
        RokuApp getActiveRokuApp(const RokuDevice *device);

        /**
         * Get a given app's icon
         * @param device Pointer to RokuDevice object representing the device on which the app is installed
         * @param app Pointer to RokuApp object representing the app to get the icon of
         * @return RokuAppIcon object representing the app icon
         */
        RokuAppIcon getRokuAppIcon(const RokuDevice *device, const RokuApp *app);

        /**
         * Send custom input to the currently active app on a given Roku device
         * @param device Pointer to RokuDevice object representing the device to send input to
         * @param params Integer number of parameters to send
         * @param names Array (size params) of C-strings with the names of the parameters
         * @param values Array (size params) of C-strings with the values of the parameters
         * @return Result of input POST request
         */
        int sendCustomRokuInput(const RokuDevice *device, int params, const char* names[], const char* values[]);

        /**
         * Run search for a movie, TV show, person, or app. Either display the results or auto-launch the first one.
         * @param device Pointer to RokuDevice object representing the device to run the search on
         * @param keyword C-string with the movie/show title, app name, person name, or other keyword to be searched
         * @param params Pointer to RokuSearchParams object describing the parameters of the search
         * @return Result of search POST request, or -1 if device does not support searches, or -2 if keyword is empty
         */
        int rokuSearch(const RokuDevice *device, const char* keyword, const RokuSearchParams *params);

        /**
         * Send Unicode string to Roku device as a series of keyboard keypresses
         * @param device Pointer to RokuDevice object representing the device to send the string to
         * @param string Wide Unicode C-string to send
         * @return Result of the last keypress request
         */
        int rokuTypeString(const RokuDevice *device, const wchar_t* string);
#ifdef __cplusplus
    } // extern "C"
} //namespace rokuecp
#endif

#endif //ROKUECP_H
