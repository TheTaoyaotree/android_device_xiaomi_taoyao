/*
 * Copyright (C) 2022 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "UdfpsHandler.xiaomi_sm8350"

#include <aidl/android/hardware/biometrics/fingerprint/BnFingerprint.h>
#include <android-base/logging.h>
#include <android-base/unique_fd.h>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <fstream>
#include <poll.h>
#include <thread>

#include "UdfpsHandler.h"
#include "xiaomi_touch.h"

// Fingerprint hwmodule commands
#define COMMAND_NIT 10
#define PARAM_NIT_UDFPS 1
#define PARAM_NIT_NONE 0

// Touchscreen and HBM
#define DISP_PARAM_PATH "/sys/devices/virtual/mi_display/disp_feature/disp-DSI-0/disp_param"
#define FOD_PRESS_STATUS_PATH "/sys/class/touch/touch_dev/fod_press_status"

#define FOD_HBM_MODE "9"
#define FOD_HBM_OFF "0"
#define FOD_HBM_ON "1"
#define FOD_STATUS_OFF 0
#define FOD_STATUS_ON 1

#define COMMAND_FOD_PRESS_STATUS 1
#define PARAM_FOD_PRESSED 1
#define PARAM_FOD_RELEASED 0

#define TOUCH_DEV_PATH "/dev/xiaomi-touch"
#define TOUCH_ID 0
#define TOUCH_MAGIC 'T'
#define TOUCH_IOC_SET_CUR_VALUE _IO(TOUCH_MAGIC, SET_CUR_VALUE)
#define TOUCH_IOC_GET_CUR_VALUE _IO(TOUCH_MAGIC, GET_CUR_VALUE)

using ::aidl::android::hardware::biometrics::fingerprint::AcquiredInfo;

template <typename T>
static void set(const std::string& path, const T& value) {
    std::ofstream file(path);
    file << value;
}

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

class XiaomiUdfpsHandler : public UdfpsHandler {
  public:
    void init(fingerprint_device_t* device) {
        mDevice = device;
        touchUniqueFd = android::base::unique_fd(open(TOUCH_DEV_PATH, O_RDWR));

        std::thread([this]() {
            int fodPressStatusFd = open(FOD_PRESS_STATUS_PATH, O_RDONLY);

            if (fodPressStatusFd < 0) {
                LOG(ERROR) << "failed to open fodPressStatusFd, err: " << fodPressStatusFd;
                return;
            }

            struct pollfd fds[1] = {
                {fodPressStatusFd, .events = POLLERR | POLLPRI, .revents = 0},
            };

            while (true) {
                int rc = poll(fds, 1, -1);
                if (rc < 0) {
                    if (fds[0].revents & POLLERR) {
                        LOG(ERROR) << "failed to poll fodPressStatusFd, err: " << rc;
                    }
                    continue;
                }

                if (fds[0].revents & (POLLERR | POLLPRI)) {
                    bool pressState = readBool(fodPressStatusFd);
                    mDevice->extCmd(mDevice, COMMAND_FOD_PRESS_STATUS, pressState ? PARAM_FOD_PRESSED : PARAM_FOD_RELEASED);
                }
            }
        }).detach();
    }

    void onFingerDown(uint32_t /*x*/, uint32_t /*y*/, float /*minor*/, float /*major*/) {
        LOG(INFO) << __func__;
        setFingerDown(true);
    }

    void onFingerUp() {
        LOG(INFO) << __func__;
        setFingerDown(false);
    }

    void onAcquired(int32_t result, int32_t vendorCode) {
        LOG(INFO) << __func__ << " result: " << result << " vendorCode: " << vendorCode;
        if (static_cast<AcquiredInfo>(result) == AcquiredInfo::GOOD) {
            setFingerDown(false);
        } else if (vendorCode == 21) {
            setFodStatus(FOD_STATUS_ON);
        }
    }

    void cancel() {
        LOG(INFO) << __func__;
        setFingerDown(false);
        setFodStatus(FOD_STATUS_OFF);
    }

  private:
    fingerprint_device_t* mDevice;
    android::base::unique_fd touchUniqueFd;

    void setFodStatus(int value) {
        int buf[MAX_BUF_SIZE] = {TOUCH_ID, TOUCH_FOD_ENABLE, value};
        ioctl(touchUniqueFd.get(), TOUCH_IOC_SET_CUR_VALUE, &buf);
    }

    void setFingerDown(bool pressed) {
        mDevice->extCmd(mDevice, COMMAND_NIT, pressed ? PARAM_NIT_UDFPS : PARAM_NIT_NONE);

        int buf[MAX_BUF_SIZE] = {TOUCH_ID, THP_FOD_DOWNUP_CTL, pressed ? 1 : 0};
        ioctl(touchUniqueFd.get(), TOUCH_IOC_SET_CUR_VALUE, &buf);

        set(DISP_PARAM_PATH, std::string(FOD_HBM_MODE) + " " + (pressed ? FOD_HBM_ON : FOD_HBM_OFF));
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
