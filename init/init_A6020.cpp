/*
   Copyright (c) 2014, The Linux Foundation. All rights reserved.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are
   met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above
      copyright notice, this list of conditions and the following
      disclaimer in the documentation and/or other materials provided
      with the distribution.
    * Neither the name of The Linux Foundation nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
   WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
   ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
   BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
   CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
   SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
   BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
   WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
   OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
   IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#define _REALLY_INCLUDE_SYS__SYSTEM_PROPERTIES_H_
#include <sys/_system_properties.h>
#include <stdio.h>
#include <stdlib.h>

#include <android-base/properties.h>
#include <android-base/strings.h>

#include "property_service.h"
#include "vendor_init.h"

#define ISMATCH(a,b)    (!strncmp(a,b,PROP_VALUE_MAX))

using android::init::property_set;

// copied from build/tools/releasetools/ota_from_target_files.py
// but with "." at the end and empty entry
std::vector<std::string> ro_product_props_default_source_order = {
    "",
    "vendor.",
    "system.",
};

void property_override(char const prop[], char const value[], bool add = true)
{
    auto pi = (prop_info *) __system_property_find(prop);

    if (pi != nullptr) {
        __system_property_update(pi, value, strlen(value));
    } else if (add) {
        __system_property_add(prop, strlen(prop), value, strlen(value));
    }
}

void override_device_props(char const model[], char const fingerprint[])
{
    const auto set_ro_product_prop = [](const std::string &source,
            const std::string &prop, const std::string &value) {
        auto prop_name = "ro.product." + source + prop;
        property_override(prop_name.c_str(), value.c_str(), false);
    };

    const auto set_ro_build_prop = [](const std::string &source,
            const std::string &prop, const std::string &value) {
        auto prop_name = "ro." + source + "build." + prop;
        property_override(prop_name.c_str(), value.c_str(), false);
    };

    for (const auto &source : ro_product_props_default_source_order)
    {
        set_ro_product_prop(source, "device", model);
        set_ro_product_prop(source, "model", "Lenovo " + model);
        set_ro_product_prop(source, "name", model);
        set_ro_build_prop(source, "product", model);
        set_ro_build_prop(source, "fingerprint", fingerprint);
    }
}

void set_model_config(bool fhd, int ram, bool dualsim = true)
{
    if (ram == 3) {
        // There's only one 3GB variant - and it has 1080p panel :)
        property_set("ro.sf.lcd_density", "460");

        /* Dalvik properties for 1080p 3GB RAM
         *
         * https://github.com/CyanogenMod/android_frameworks_native/blob/cm-14.1/build/phone-xxhdpi-3072-dalvik-heap.mk
         */
        property_set("dalvik.vm.heapstartram", "8m");
        property_set("dalvik.vm.heapgrowthlimit", "288m");
        property_set("dalvik.vm.heapram", "768m");
        property_set("dalvik.vm.heaptargetutilization", "0.75");
        property_set("dalvik.vm.heapminfree", "512k");
        property_set("dalvik.vm.heapmaxfree", "8m");

        // Cached apps limit
        property_set("ro.vendor.qti.sys.fw.bg_apps_limit", "25");
    } else {
        if (fhd) {
            property_set("ro.sf.lcd_density", "460");

            /* Dalvik properties for 1080p 2GB RAM
             *
             * https://github.com/CyanogenMod/android_frameworks_native/blob/cm-14.1/build/phone-xxhdpi-2048-dalvik-heap.mk
             */
            property_set("dalvik.vm.heapstartram", "16m");
            property_set("dalvik.vm.heapgrowthlimit", "192m");
            property_set("dalvik.vm.heapram", "512m");
            property_set("dalvik.vm.heaptargetutilization", "0.75");
            property_set("dalvik.vm.heapminfree", "2m");
            property_set("dalvik.vm.heapmaxfree", "8m");
        } else {
            property_set("ro.sf.lcd_density", "300");

            /* Dalvik properties for 720p 2GB RAM
             *
             * https://github.com/CyanogenMod/android_frameworks_native/blob/cm-14.1/build/phone-xhdpi-2048-dalvik-heap.mk
             */
            property_set("dalvik.vm.heapstartram", "8m");
            property_set("dalvik.vm.heapgrowthlimit", "192m");
            property_set("dalvik.vm.heapram", "512m");
            property_set("dalvik.vm.heaptargetutilization", "0.75");
            property_set("dalvik.vm.heapminfree", "512k");
            property_set("dalvik.vm.heapmaxfree", "8m");
        }

        // Cached apps limit
        property_set("ro.vendor.qti.sys.fw.bg_apps_limit", "17");
    }

    if (dualsim) {
        property_set("persist.radio.multisim.config", "dsds");
        property_set("ro.telephony.default_network", "9,9");
    } else {
        property_set("ro.telephony.default_network", "9");
    }
}

void vendor_load_properties()
{
    std::string platform;
    std::string device;
    char cmdlinebuff[128];
    char board_id[32];
    char panel_id[32];
    FILE *fp;

    fp = fopen("/proc/cmdline", "r");

    // Get board and panel ID
    char *boardindex = strstr(cmdlinebuff, "board_id=");
    strncpy(board_id, strtok(boardindex + 9, ":"), 32);
    char *panelindex = strstr(cmdlinebuff, "panel=");
    strncpy(panel_id, strtok(panelindex + 28, ":"), 32);

    fclose(fp);

    //
    // Variant detection
    //
    if (ISMATCH(board_id, "S82918B1")){
        if (ISMATCH(panel_id, "ili9881c_720p_video") || ISMATCH(panel_id, "hx8394f_boe_720p_video")) {
            // A6020a40 HW39 variant - SD616, 2GB, 720p
            override_device_props("A6020a40", "Lenovo/A6020a40/A6020a40:5.1.1/LMY47V/A6020a40_S105_161128_ROW:user/release-keys");
            set_model_config(false, 2);
        } else {
            // panel_id = "otm1901a_tm_1080p_video"
            // A6020a46 Indonesian variant - SD616, 2GB, 1080p
            override_device_props("A6020a46", "Lenovo/A6020a46/A6020a46:5.1.1/LMY47V/A6020a46_S042_160516_ROW:user/release-keys");
            set_model_config(true, 2);
        }
    } else if (ISMATCH(board_id, "S82918E1")){
        // A6020a41 - SD415, 2GB, 720p, single sim
        override_device_props("A6020a41", "Lenovo/A6020a41/A6020a41:5.1.1/LMY47V/A6020a41_S_S028_160922_ROW:user/release-keys");
        set_model_config(false, 2, false);
    } else if (ISMATCH(board_id, "S82918F1")){
        // A6020l36 - SD616, 2GB, 1080p
        override_device_props("A6020l36", "Lenovo/A6020l36/A6020l36:5.1.1/LMY47V/A6020l36_S035_160622_LAS:user/release-keys");
        set_model_config(true, 2);
    } else if (ISMATCH(board_id, "S82918G1")){
        // A6020l37 - SD616, 2GB, 1080p, single sim
        override_device_props("A6020l37", "Lenovo/A6020l37/A6020l37:5.1.1/LMY47V/A6020l37_S024_160914_LAS:user/release-keys");
        set_model_config(true, 2, false);
    } else if (ISMATCH(board_id, "S82918H1")){
        // A6020a46 Indian variant - SD616, 3GB, 1080p
        override_device_props("A6020a46", "Lenovo/A6020a46/A6020a46:5.1.1/LMY47V/A6020a46_S105_161124_ROW:user/release-keys");
        set_model_config(true, 3);
    } else {
        // Use A6020a40 as default - SD415, 2GB, 720p
        // board_id = "S82918D1"
        override_device_props("A6020a40", "Lenovo/A6020a40/A6020a40:5.1.1/LMY47V/A6020a40_S102_161123_ROW:user/release-keys");
        set_model_config(false, 2);
    }

    // Init a dummy BT MAC address, will be overwritten later
    property_set("ro.boot.btmacaddr", "00:00:00:00:00:00");
}
