/*
 * Copyright (C) 2022 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "UdfpsHandler"

#include <aidl/android/hardware/biometrics/fingerprint/BnFingerprint.h>
#include <android-base/logging.h>
#include <android-base/unique_fd.h>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <fstream>
#include <poll.h>
#include <thread>

#include "UdfpsHandler.h"

#include <linux/xiaomi_touch.h>
#include <display/drm/mi_disp.h>

// Fingerprint hwmodule commands
#define COMMAND_NIT 10
#define PARAM_NIT_UDFPS 1
#define PARAM_NIT_NONE 0

// Touchscreen and HBM
#define TOUCH_DEV_PATH "/dev/xiaomi-touch"
#define DISP_FEATURE_PATH "/dev/mi_display/disp_feature"

#define FOD_STATUS_OFF 0
#define FOD_STATUS_ON 1

#define PARAM_FOD_PRESSED 1
#define PARAM_FOD_RELEASED 0

using ::aidl::android::hardware::biometrics::fingerprint::AcquiredInfo;

static bool readBool(int fd) {
    char c;
    int rc;

    rc = lseek(fd, 0, SEEK_SET);
    if (rc) {
        LOG(ERROR) << "failed to seek fd, err: " << rc;
        return false;
    }

    rc = read(fd, &c, sizeof(char));
    if (rc != 1) {
        LOG(ERROR) << "failed to read bool from fd, err: " << rc;
        return false;
    }

    return c != '0';
}

struct disp_base displayBasePrimary = {
    .flag = 0,
    .disp_id = MI_DISP_PRIMARY,
};

struct disp_event_req displayEventRequest = {
    .base = displayBasePrimary,
    .type = MI_DISP_EVENT_FOD,
};

class XiaomiUdfpsHandler : public UdfpsHandler {
public:
    void init(fingerprint_device_t* device) {
        mDevice = device;
        touchUniqueFd = android::base::unique_fd(open(TOUCH_DEV_PATH, O_RDWR));
        dispFeatureFd = android::base::unique_fd(open(DISP_FEATURE_PATH, O_RDWR));
    }

    void onFingerDown(uint32_t /*x*/, uint32_t /*y*/, float /*minor*/, float /*major*/) {
        LOG(INFO) << __func__;
        setFodStatus(FOD_STATUS_ON);
        setFingerDown(true);
    }

    void onFingerUp() {
        LOG(INFO) << __func__;
        setFodStatus(FOD_STATUS_OFF);
        setFingerDown(false);
    }

    void onAcquired(int32_t result, int32_t vendorCode) {
        LOG(INFO) << __func__ << " result: " << result << " vendorCode: " << vendorCode;
        if (static_cast<AcquiredInfo>(result) == AcquiredInfo::GOOD) {
            onFingerUp();
        }
    }

    void cancel() {
        LOG(INFO) << __func__;
        onFingerUp();
    }

private:
    fingerprint_device_t* mDevice;
    android::base::unique_fd touchUniqueFd;
    android::base::unique_fd dispFeatureFd;

    void setFodStatus(int value) {
        int buf[MAX_BUF_SIZE] = {MI_DISP_PRIMARY, Touch_Fod_Enable, value};
        ioctl(touchUniqueFd.get(), TOUCH_IOC_SET_CUR_VALUE, &buf);
    }

    void setFingerDown(bool pressed) {
        int buf[MAX_BUF_SIZE] = {MI_DISP_PRIMARY, THP_FOD_DOWNUP_CTL, pressed ? PARAM_FOD_PRESSED : PARAM_FOD_RELEASED};
        ioctl(touchUniqueFd.get(), TOUCH_IOC_SET_CUR_VALUE, &buf);

        struct disp_feature_req req = {
            .base = displayBasePrimary,
            .feature_id = DISP_FEATURE_LOCAL_HBM,
            .feature_val = pressed ? LOCAL_HBM_NORMAL_WHITE_1000NIT : LOCAL_HBM_OFF_TO_NORMAL,
        };
        ioctl(dispFeatureFd.get(), MI_DISP_IOCTL_SET_FEATURE, &req);

        mDevice->extCmd(mDevice, COMMAND_NIT, pressed ? PARAM_NIT_UDFPS : PARAM_NIT_NONE);
    }
};

static UdfpsHandler* create() {
    return new XiaomiUdfpsHandler();
}

static void destroy(UdfpsHandler* handler) {
    delete handler;
}

extern "C" UdfpsHandlerFactory UDFPS_HANDLER_FACTORY = {
    .create = create,
    .destroy = destroy,
};
