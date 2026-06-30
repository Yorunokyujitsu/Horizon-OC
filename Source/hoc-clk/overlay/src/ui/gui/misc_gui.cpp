/*
 *
 * Copyright (c) Souldbminer, Lightos_ and Horizon OC Contributors
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <cstdio>
#include <cstring>
#include <vector>

#include "../format.h"
#include "config_info_strings.h"
#include "fatal_gui.h"
#include "labels.h"
#include "misc_gui.h"
#include "ult_ext.h"


// This workaround *may* not be nessasary, but it seems to help with reducing stutter
static void kipDataThreadFunc(void *) {
    hocClkIpcSetKipData();
}

static Thread s_kipThread;
static bool s_kipThreadPending = false;

static void sendKipData() {
    if (s_kipThreadPending) {
        threadWaitForExit(&s_kipThread);
        threadClose(&s_kipThread);
        s_kipThreadPending = false;
    }
    if (R_SUCCEEDED(threadCreate(&s_kipThread, kipDataThreadFunc, nullptr, nullptr, 0x1000, 0x2C, -2))) {
        threadStart(&s_kipThread);
        s_kipThreadPending = true;
    }
}
#if IS_MINIMAL == 1
    #pragma message("Compiling with minimal features")
#endif

#define A_BTN "\ue0e0"
#define R_ARROW "\u2192"

class GeneralSettingsSubMenuGui;
class GovernorSettingsSubMenuGui;
class DisplaySubMenuGui;
class SafetySubMenuGui;
class RamSubmenuGui;
class RamTimingsSubmenuGui;
class RamLatenciesSubmenuGui;
class SocCustomTableSubmenuGui;
class CpuSubmenuGui;
class GpuSubmenuGui;
class GpuCustomTableSubmenuGui;
class ExperimentalSettingsSubMenuGui;

MiscGui::MiscGui() {
    this->configList = new HocClkConfigValueList{};
}

MiscGui::~MiscGui() {
    if (shouldSaveKip) {
        sendKipData();
        shouldSaveKip = false;
    }
    if (s_kipThreadPending) {
        threadWaitForExit(&s_kipThread);
        threadClose(&s_kipThread);
        s_kipThreadPending = false;
    }
    delete this->configList;
    this->configToggles.clear();
    this->configTrackbars.clear();
    this->configButtons.clear();
    this->configRanges.clear();
}

void MiscGui::addConfigToggle(HocClkConfigValue configVal, const char *altName, bool kip) {
    const char *configName = altName ? altName : hocclkFormatConfigValue(configVal, true);
    auto infoStrings = ConfigInfoStrings(configVal, IsMariko(), IsHoag());

    struct YAwareToggle : tsl::elm::ToggleListItem {
        std::vector<std::string> m_info;
        std::string m_title;
        YAwareToggle(const char *text, bool state, std::string title, std::vector<std::string> info)
            : tsl::elm::ToggleListItem(text, state), m_info(std::move(info)), m_title(std::move(title)) {
        }
        bool onClick(u64 keys) override {
            if (!m_info.empty() && (keys & HidNpadButton_Y) && !(keys & ~HidNpadButton_Y)) {
                tsl::changeTo<InfoGui>(m_title, m_info);
                return true;
            }
            return tsl::elm::ToggleListItem::onClick(keys);
        }
    };

    auto *toggle = new YAwareToggle(configName, this->configList->values[configVal], configName, std::move(infoStrings));
    if (!kip)
        toggle->setTextColor(tsl::Color(120, 235, 255, 255));
    toggle->setStateChangedListener([this, configVal, kip](bool state) {
        this->configList->values[configVal] = uint64_t(state);
        Result rc = hocclkIpcSetConfigValues(this->configList);
        if (R_FAILED(rc)) {
            FatalGui::openWithResultCode("hocclkIpcSetConfigValues", rc);
        } else if (kip) {
            shouldSaveKip = true;
        }
        this->lastContextUpdate = armGetSystemTick();
    });
    this->listElement->addItem(toggle);
    this->configToggles[configVal] = toggle;
}

void MiscGui::addConfigTrackbar(HocClkConfigValue configVal, const char *altName, const ValueRange &range, bool kip) {
    auto infoStrings = ConfigInfoStrings(configVal, IsMariko(), IsHoag());
    struct IndexedBar : tsl::elm::NamedStepTrackBar {
        std::vector<std::string> m_info;
        std::string m_title;
        IndexedBar(const char *label, const ValueRange &r, std::string title, std::vector<std::string> info)
            : tsl::elm::NamedStepTrackBar("", { "" }, true, label), m_info(std::move(info)), m_title(std::move(title)) {
            m_stepDescriptions.clear();
            u32 numSteps = (r.max - r.min) / r.step + 1;
            for (u32 i = 0; i < numSteps; i++) {
                u32 disp = (r.min + i * r.step) / r.divisor;
                std::string s = std::to_string(disp);
                if (!r.suffix.empty())
                    s += " " + r.suffix;
                m_stepDescriptions.push_back(s);
            }
            m_numSteps = (u8)m_stepDescriptions.size();
            m_selection = m_stepDescriptions[0];
        }
        bool handleInput(u64 keysDown, u64 keysHeld, const HidTouchState &touchPos, HidAnalogStickState leftJoyStick,
                         HidAnalogStickState rightJoyStick) override {
            if (!m_info.empty() && (keysDown & HidNpadButton_Y) && !(keysDown & ~HidNpadButton_Y)) {
                tsl::changeTo<InfoGui>(m_title, m_info);
                return true;
            }
            return tsl::elm::NamedStepTrackBar::handleInput(keysDown, keysHeld, touchPos, leftJoyStick, rightJoyStick);
        }
    };
    const char *name = altName ? altName : hocclkFormatConfigValue(configVal, true);
    auto *bar = new IndexedBar(name, range, name, std::move(infoStrings));
    u32 cur = (u32)this->configList->values[configVal];
    u16 curStep = 0;
    if (cur >= range.min && cur <= range.max && range.step > 0 && (cur - range.min) % range.step == 0)
        curStep = (u16)((cur - range.min) / range.step);
    bar->setProgress(curStep);
    bar->setValueChangedListener([this, configVal, kip, range](u16 v) {
        this->configList->values[configVal] = range.min + (u32)v * range.step;
        Result rc = hocclkIpcSetConfigValues(this->configList);
        if (R_FAILED(rc))
            FatalGui::openWithResultCode("hocclkIpcSetConfigValues", rc);
        if (kip)
            shouldSaveKip = true;
    });
    this->listElement->addItem(bar);
}

void MiscGui::addMappedConfigTrackbar(HocClkConfigValue configVal, const char *altName, std::vector<u32> vals,
                                      std::initializer_list<std::string> names, bool kip) {
    const char *name = altName ? altName : hocclkFormatConfigValue(configVal, true);
    auto infoStrings = ConfigInfoStrings(configVal, IsMariko(), IsHoag());

    struct YAwareTrackBar : tsl::elm::NamedStepTrackBar {
        std::vector<std::string> m_info;
        std::string m_title;
        YAwareTrackBar(const char *label, std::initializer_list<std::string> steps, std::string title, std::vector<std::string> info)
            : tsl::elm::NamedStepTrackBar("", steps, true, label), m_info(std::move(info)), m_title(std::move(title)) {
        }
        bool handleInput(u64 keysDown, u64 keysHeld, const HidTouchState &touchPos, HidAnalogStickState leftJoyStick,
                         HidAnalogStickState rightJoyStick) override {
            if (!m_info.empty() && (keysDown & HidNpadButton_Y) && !(keysDown & ~HidNpadButton_Y)) {
                tsl::changeTo<InfoGui>(m_title, m_info);
                return true;
            }
            return tsl::elm::NamedStepTrackBar::handleInput(keysDown, keysHeld, touchPos, leftJoyStick, rightJoyStick);
        }
    };

    auto *bar = new YAwareTrackBar(name, names, name, std::move(infoStrings));
    u32 cur = (u32)this->configList->values[configVal];
    u16 curIdx = 0;
    for (u16 i = 0; i < (u16)vals.size(); i++) {
        if (vals[i] == cur) {
            curIdx = i;
            break;
        }
    }
    bar->setProgress(curIdx);
    bar->setValueChangedListener([this, configVal, kip, vals](u16 idx) {
        if (idx < (u16)vals.size())
            this->configList->values[configVal] = vals[idx];
        Result rc = hocclkIpcSetConfigValues(this->configList);
        if (R_FAILED(rc))
            FatalGui::openWithResultCode("hocclkIpcSetConfigValues", rc);
        if (kip)
            shouldSaveKip = true;
    });
    this->listElement->addItem(bar);
}

void MiscGui::addConfigButton(HocClkConfigValue configVal, const char *altName, const ValueRange &range, const std::string &categoryName,
                              const ValueThresholds *thresholds, const std::map<uint32_t, std::string> &labels,
                              const std::vector<NamedValue> &namedValues, bool showDefaultValue, bool kip) {
    const char *configName = altName ? altName : hocclkFormatConfigValue(configVal, true);
    auto infoStrings = ConfigInfoStrings(configVal, IsMariko(), IsHoag());

    tsl::elm::ListItem *listItem = new tsl::elm::ListItem(configName);
    if (!kip)
        listItem->setTextColor(tsl::Color(120, 235, 255, 255));

    uint64_t currentValue = this->configList->values[configVal];
    char valueText[32];
    if (currentValue == 0 && showDefaultValue) {
        snprintf(valueText, sizeof(valueText), "%s", VALUE_DEFAULT_TEXT);
    } else {
        bool foundNamedValue = false;
        for (const auto &namedValue : namedValues) {
            if (currentValue == namedValue.value) {
                snprintf(valueText, sizeof(valueText), "%s", namedValue.name.c_str());
                foundNamedValue = true;
                break;
            }
        }

        if (!foundNamedValue) {
            uint64_t displayValue = currentValue / range.divisor;
            if (!range.suffix.empty()) {
                snprintf(valueText, sizeof(valueText), "%lu %s", displayValue, range.suffix.c_str());
            } else {
                snprintf(valueText, sizeof(valueText), "%lu", displayValue);
            }
        }
    }
    listItem->setValue(valueText);

    ValueThresholds thresholdsCopy = (thresholds ? *thresholds : ValueThresholds{});

    listItem->setClickListener([this, configVal, range, categoryName, thresholdsCopy, labels, showDefaultValue, kip,
                                infoStrings = std::move(infoStrings), configName = std::string(configName)](u64 keys) {
        if (!infoStrings.empty() && (keys & HidNpadButton_Y) && !(keys & ~HidNpadButton_Y)) {
            tsl::changeTo<InfoGui>(configName, infoStrings);
            return true;
        }

        if ((keys & HidNpadButton_A) == 0)
            return false;

        std::uint32_t currentValue = this->configList->values[configVal];

        // Look up live namedValues so relabeling in refresh() is reflected
        auto nvIt = this->configNamedValues.find(configVal);
        const std::vector<NamedValue> &liveNamedValues = (nvIt != this->configNamedValues.end()) ? nvIt->second : std::vector<NamedValue>();

        if (thresholdsCopy.warning != 0 || thresholdsCopy.danger != 0) {

            tsl::changeTo<ValueChoiceGui>(
                currentValue, range, categoryName,
                [this, configVal, kip](std::uint32_t value) {
                    this->configList->values[configVal] = value;
                    Result rc = hocclkIpcSetConfigValues(this->configList);
                    if (R_FAILED(rc)) {
                        FatalGui::openWithResultCode("hocclkIpcSetConfigValues", rc);
                        return false;
                    }
                    if (kip) {
                        shouldSaveKip = true;
                    }
                    this->lastContextUpdate = armGetSystemTick();
                    return true;
                },
                thresholdsCopy, true, labels, liveNamedValues, showDefaultValue);
        } else {

            tsl::changeTo<ValueChoiceGui>(
                currentValue, range, categoryName,
                [this, configVal, kip](std::uint32_t value) {
                    this->configList->values[configVal] = value;
                    Result rc = hocclkIpcSetConfigValues(this->configList);
                    if (R_FAILED(rc)) {
                        FatalGui::openWithResultCode("hocclkIpcSetConfigValues", rc);
                        return false;
                    }
                    if (kip) {
                        shouldSaveKip = true;
                    }
                    this->lastContextUpdate = armGetSystemTick();
                    return true;
                },
                ValueThresholds(), false, labels, liveNamedValues, showDefaultValue);
        }

        return true;
    });

    this->listElement->addItem(listItem);
    this->configButtons[configVal] = listItem;
    this->configRanges[configVal] = range;
    this->configNamedValues[configVal] = namedValues;
}

void MiscGui::addConfigButtonS(HocClkConfigValue configVal, const char *altName, const ValueRange &range, const std::string &categoryName,
                               const ValueThresholds *thresholds, const std::map<uint32_t, std::string> &labels,
                               const std::vector<NamedValue> &namedValues, bool showDefaultValue, const char *subText, bool kip) {
    const char *configName = altName ? altName : hocclkFormatConfigValue(configVal, true);
    auto infoStrings = ConfigInfoStrings(configVal, IsMariko(), IsHoag());
    tsl::elm::ListItem *listItem = new tsl::elm::ListItem("");
    if (!kip)
        listItem->setTextColor(tsl::Color(120, 235, 255, 255));

    uint64_t currentValue = this->configList->values[configVal];
    char valueText[32];
    if (currentValue == 0 && showDefaultValue) {
        snprintf(valueText, sizeof(valueText), "%s", VALUE_DEFAULT_TEXT);
    } else {
        bool foundNamedValue = false;
        for (const auto &namedValue : namedValues) {
            if (currentValue == namedValue.value) {
                snprintf(valueText, sizeof(valueText), "%s", namedValue.name.c_str());
                foundNamedValue = true;
                break;
            }
        }

        if (!foundNamedValue) {
            uint64_t displayValue = currentValue / range.divisor;
            if (!range.suffix.empty()) {
                snprintf(valueText, sizeof(valueText), "%lu %s", displayValue, range.suffix.c_str());
            } else {
                snprintf(valueText, sizeof(valueText), "%lu", displayValue);
            }
        }
    }

    listItem->setText(valueText);
    listItem->setValue(subText ? subText : "");

    ValueThresholds thresholdsCopy = (thresholds ? *thresholds : ValueThresholds{});

    listItem->setClickListener([this, configVal, range, categoryName, thresholdsCopy, labels, showDefaultValue, kip,
                                infoStrings = std::move(infoStrings), configName = std::string(configName)](u64 keys) {
        if (!infoStrings.empty() && (keys & HidNpadButton_Y) && !(keys & ~HidNpadButton_Y)) {
            tsl::changeTo<InfoGui>(configName, infoStrings);
            return true;
        }

        if ((keys & HidNpadButton_A) == 0)
            return false;

        std::uint32_t currentValue = this->configList->values[configVal];

        // Look up live namedValues so relabeling in refresh() is reflected
        auto nvIt = this->configNamedValues.find(configVal);
        const std::vector<NamedValue> &liveNamedValues = (nvIt != this->configNamedValues.end()) ? nvIt->second : std::vector<NamedValue>();

        if (thresholdsCopy.warning != 0 || thresholdsCopy.danger != 0) {

            tsl::changeTo<ValueChoiceGui>(
                currentValue, range, categoryName,
                [this, configVal, kip](std::uint32_t value) {
                    this->configList->values[configVal] = value;
                    Result rc = hocclkIpcSetConfigValues(this->configList);
                    if (R_FAILED(rc)) {
                        FatalGui::openWithResultCode("hocclkIpcSetConfigValues", rc);
                        return false;
                    }
                    if (kip) {
                        shouldSaveKip = true;
                    }
                    this->lastContextUpdate = armGetSystemTick();
                    return true;
                },
                thresholdsCopy, true, labels, liveNamedValues, showDefaultValue);
        } else {

            tsl::changeTo<ValueChoiceGui>(
                currentValue, range, categoryName,
                [this, configVal, kip](std::uint32_t value) {
                    this->configList->values[configVal] = value;
                    Result rc = hocclkIpcSetConfigValues(this->configList);
                    if (R_FAILED(rc)) {
                        FatalGui::openWithResultCode("hocclkIpcSetConfigValues", rc);
                        return false;
                    }
                    if (kip) {
                        shouldSaveKip = true;
                    }
                    this->lastContextUpdate = armGetSystemTick();
                    return true;
                },
                ValueThresholds(), false, labels, liveNamedValues, showDefaultValue);
        }

        return true;
    });

    this->listElement->addItem(listItem);
    this->configButtons[configVal] = listItem;
    this->configRanges[configVal] = range;
    this->configNamedValues[configVal] = namedValues;
    this->configButtonSKeys.insert(configVal);
    if (subText)
        this->configButtonSSubtext[configVal] = std::string(subText);
}

void MiscGui::updateConfigToggles() {
    for (const auto &[value, toggle] : this->configToggles) {
        if (toggle != nullptr)
            toggle->setState(this->configList->values[value]);
    }
}

void MiscGui::addFreqButton(HocClkConfigValue configVal, const char *altName, HocClkModule module, const std::map<uint32_t, std::string> &labels) {
    const char *configName = altName ? altName : hocclkFormatConfigValue(configVal, true);
    auto infoStrings = ConfigInfoStrings(configVal, IsMariko(), IsHoag());

    tsl::elm::ListItem *listItem = new tsl::elm::ListItem(configName);

    uint64_t currentMHz = this->configList->values[configVal];
    char valueText[32];
    snprintf(valueText, sizeof(valueText), "%lu MHz", currentMHz);
    listItem->setValue(valueText);

    listItem->setClickListener(
        [this, configVal, module, labels, infoStrings = std::move(infoStrings), configName = std::string(configName)](u64 keys) {
            if (!infoStrings.empty() && (keys & HidNpadButton_Y) && !(keys & ~HidNpadButton_Y)) {
                tsl::changeTo<InfoGui>(configName, infoStrings);
                return true;
            }

            if ((keys & HidNpadButton_A) == 0)
                return false;

            std::uint32_t hzList[HOCCLK_FREQ_LIST_MAX];
            std::uint32_t hzCount;

            Result rc = hocclkIpcGetFreqList(module, hzList, HOCCLK_FREQ_LIST_MAX, &hzCount);
            if (R_FAILED(rc)) {
                FatalGui::openWithResultCode("hocclkIpcGetFreqList", rc);
                return false;
            }

            std::uint32_t currentHz = this->configList->values[configVal] * 1'000'000;

            tsl::changeTo<FreqChoiceGui>(
                currentHz, hzList, hzCount, module,
                [this, configVal](std::uint32_t hz) {
                    uint64_t mhz = hz / 1'000'000;
                    this->configList->values[configVal] = mhz;

                    Result rc = hocclkIpcSetConfigValues(this->configList);
                    if (R_FAILED(rc)) {
                        FatalGui::openWithResultCode("hocclkIpcSetConfigValues", rc);
                        return false;
                    }

                    this->lastContextUpdate = armGetSystemTick();
                    return true;
                },
                false, labels);

            return true;
        });

    this->listElement->addItem(listItem);
    this->configButtons[configVal] = listItem;

    this->configRanges[configVal] = ValueRange(0, 0, 0, "MHz", 1);
}

void MiscGui::listUI() {
    Result rc = hocclkIpcGetConfigValues(configList);
    if (R_FAILED(rc)) [[unlikely]] {
        FatalGui::openWithResultCode("hocclkIpcGetConfigValues", rc);
        return;
    }

    ValueThresholds thresholdsDisabled(0, 0);
    std::vector<NamedValue> noNamedValues = {};

    this->listElement->addItem(new CompactCategoryHeader("설정     \uE150 파란 옵션은 재부팅 없이 적용됩니다"));

    /*tsl::elm::CustomDrawer *rebootSetWarning = new tsl::elm::CustomDrawer([](tsl::gfx::Renderer *renderer, s32 x, s32 y, s32 w, s32 h) {
        renderer->drawString("\uE150 Settings marked in blue", false, x + 20, y + 30, 18, tsl::style::color::ColorText);
        renderer->drawString("don't require a reboot to apply!", false, x + 20, y + 50, 18, tsl::style::color::ColorText);
        renderer->drawString("You can also press \ue0e3 to show", false, x + 20, y + 70, 18, tsl::style::color::ColorText);
        renderer->drawString("information about each setting.", false, x + 20, y + 90, 18, tsl::style::color::ColorText);
    });
    rebootSetWarning->setBoundaries(0, 0, tsl::cfg::FramebufferWidth, 110);
    this->listElement->addItem(rebootSetWarning);*/

    tsl::elm::ListItem *sysmoduleSettingsSubMenu = new tsl::elm::ListItem("기본 옵션");
    sysmoduleSettingsSubMenu->setClickListener([](u64 keys) {
        if (keys & HidNpadButton_A) {
            tsl::changeTo<GeneralSettingsSubMenuGui>();
            return true;
        }
        return false;
    });
    sysmoduleSettingsSubMenu->setValue(R_ARROW);
    this->listElement->addItem(sysmoduleSettingsSubMenu);

    tsl::elm::ListItem *governorSettingsSubMenu = new tsl::elm::ListItem("클럭 제어 옵션");
    governorSettingsSubMenu->setClickListener([](u64 keys) {
        if (keys & HidNpadButton_A) {
            tsl::changeTo<GovernorSettingsSubMenuGui>();
            return true;
        }
        return false;
    });
    governorSettingsSubMenu->setValue(R_ARROW);
    this->listElement->addItem(governorSettingsSubMenu);

    tsl::elm::ListItem *safetySubmenu = new tsl::elm::ListItem("안전 옵션");
    safetySubmenu->setClickListener([](u64 keys) {
        if (keys & HidNpadButton_A) {
            tsl::changeTo<SafetySubMenuGui>();
            return true;
        }
        return false;
    });
    safetySubmenu->setValue(R_ARROW);
    this->listElement->addItem(safetySubmenu);

    tsl::elm::ListItem *ramSubmenu = new tsl::elm::ListItem("RAM 옵션");
    ramSubmenu->setClickListener([](u64 keys) {
        if (keys & HidNpadButton_A) {
            tsl::changeTo<RamSubmenuGui>();
            return true;
        }
        return false;
    });
    ramSubmenu->setValue(R_ARROW);
    this->listElement->addItem(ramSubmenu);

    tsl::elm::ListItem *cpuSubmenu = new tsl::elm::ListItem("CPU 옵션");
    cpuSubmenu->setClickListener([](u64 keys) {
        if (keys & HidNpadButton_A) {
            tsl::changeTo<CpuSubmenuGui>();
            return true;
        }
        return false;
    });
    cpuSubmenu->setValue(R_ARROW);
    this->listElement->addItem(cpuSubmenu);

    tsl::elm::ListItem *gpuSubmenu = new tsl::elm::ListItem("GPU 옵션");
    gpuSubmenu->setClickListener([](u64 keys) {
        if (keys & HidNpadButton_A) {
            tsl::changeTo<GpuSubmenuGui>();
            return true;
        }
        return false;
    });
    gpuSubmenu->setValue(R_ARROW);
    this->listElement->addItem(gpuSubmenu);

    tsl::elm::ListItem *displaySubMenu = new tsl::elm::ListItem("주사율 옵션");
    displaySubMenu->setClickListener([](u64 keys) {
        if (keys & HidNpadButton_A) {
            tsl::changeTo<DisplaySubMenuGui>();
            return true;
        }
        return false;
    });
    displaySubMenu->setValue(R_ARROW);
    this->listElement->addItem(displaySubMenu);

    if (this->configList->values[HocClkConfigValue_EnableExperimentalSettings]) {
        tsl::elm::ListItem *experimentalSubMenu = new tsl::elm::ListItem("실험실 옵션");
        experimentalSubMenu->setClickListener([](u64 keys) {
            if (keys & HidNpadButton_A) {
                tsl::changeTo<ExperimentalSettingsSubMenuGui>();
                return true;
            }
            return false;
        });
        experimentalSubMenu->setValue(R_ARROW);
        this->listElement->addItem(experimentalSubMenu);
    }
}

class GeneralSettingsSubMenuGui : public MiscGui {
    public:
    GeneralSettingsSubMenuGui() {
    }

    protected:
    void listUI() override {
        Result rc = hocclkIpcGetConfigValues(this->configList);
        if (R_FAILED(rc)) [[unlikely]] {
            FatalGui::openWithResultCode("hocclkIpcGetConfigValues", rc);
            return;
        }
        this->listElement->addItem(new CompactCategoryHeader("기본 옵션"));

        ValueThresholds thresholdsDisabled(0, 0);
        std::vector<NamedValue> ramVoltDispModes = {
            NamedValue("VDD2", RamDisplayMode_VDD2),
            NamedValue("VDDQ", RamDisplayMode_VDDQ),
        };

        if (IsMariko()) {
            addConfigButton(HocClkConfigValue_RAMVoltDisplayMode, "RAM 표시 전압", ValueRange(0, 12, 1, "", 0), "RAM 표시 전압",
                            &thresholdsDisabled, {}, ramVoltDispModes, false);
        }

        std::vector<NamedValue> RamDisplayUnitValues = {
            NamedValue("MHz", RamDisplayUnit_MHz),
            NamedValue("MT/s", RamDisplayUnit_MTs),
            NamedValue("MHz, MT/s", RamDisplayUnit_MHzMTs),
        };
        addConfigButton(HocClkConfigValue_RamDisplayUnit, "RAM 표시 단위", ValueRange(0, 0, 2, "", 0), "RAM 표시 단위", &thresholdsDisabled, {},
                        RamDisplayUnitValues, false

        );

        addConfigButton(HocClkConfigValue_PollingIntervalMs, "폴링 간격", ValueRange(50, 1000, 50, "ms", 1), "폴링 간격",
                        &thresholdsDisabled, {}, {}, false);

        tsl::elm::CustomDrawer *exSetWarning = new tsl::elm::CustomDrawer([](tsl::gfx::Renderer *renderer, s32 x, s32 y, s32 w, s32 h) {
            renderer->drawString("", false, x + 20, y + 30, 18, tsl::style::color::ColorText);
        });
        exSetWarning->setBoundaries(0, 0, tsl::cfg::FramebufferWidth, 30);
        this->listElement->addItem(exSetWarning);
        this->listElement->addItem(new CompactCategoryHeader("실험적 기능        \uE150 사용시 주의가 필요합니다!"));

        addConfigToggle(HocClkConfigValue_EnableExperimentalSettings, nullptr);
    }
};

class ExperimentalSettingsSubMenuGui : public MiscGui {
    public:
    ExperimentalSettingsSubMenuGui() {
    }

    protected:
    void listUI() override {
        Result rc = hocclkIpcGetConfigValues(this->configList);
        if (R_FAILED(rc)) [[unlikely]] {
            FatalGui::openWithResultCode("hocclkIpcGetConfigValues", rc);
            return;
        }
        this->listElement->addItem(new CompactCategoryHeader("실험적 기능"));
        ValueThresholds thresholdsDisabled(0, 0);
        if (IsMariko()) {
            addConfigToggle(HocClkConfigValue_MarikoMiddleFreqs, nullptr, true);
            addConfigToggle(HocClkConfigValue_LiveCpuUv, nullptr);
        }
        std::vector<NamedValue> gpuSchedMethodValues = {
            NamedValue("INI", GpuSchedulingOverrideMethod_Ini),
            NamedValue("NV 서비스", GpuSchedulingOverrideMethod_NvService),
        };
        addConfigButton(HocClkConfigValue_GPUSchedulingMethod, "GPU 스케줄링 방식", ValueRange(0, 0, 1, "", 0),
                        "GPU 스케줄링 방식", &thresholdsDisabled, {}, gpuSchedMethodValues, false);

        std::vector<NamedValue> ramRFMeasurementMethods = {
            NamedValue("PLL", MemoryFrequencyMeasurementMode_PLL),
            NamedValue("Actmon", MemoryFrequencyMeasurementMode_Actmon),
        };
        addConfigButton(HocClkConfigValue_MemoryFrequencyMeasurementMode, "RAM 주파수 측정", ValueRange(0, 0, 1, "", 0),
                        "RAM 주파수 측정", &thresholdsDisabled, {}, ramRFMeasurementMethods, false);

        tsl::elm::CustomDrawer *chargeWarningText = new tsl::elm::CustomDrawer([](tsl::gfx::Renderer *renderer, s32 x, s32 y, s32 w, s32 h) {
            renderer->drawString("\uE150 충전 전류를 강제로 변경할 경우, 배터리", false, x + 15, y + 50, 18, tsl::warningTextColor);
            renderer->drawString("또는 충전기에 손상을 줄 수 있습니다!", false, x + 38, y + 70, 18, tsl::warningTextColor);
        });
        chargeWarningText->setBoundaries(0, 0, tsl::cfg::FramebufferWidth, 90);
        this->listElement->addItem(chargeWarningText);

        if (!IsHoag()) {
            std::vector<NamedValue> chargerCurrents = {
                NamedValue("비활성화", 0),  NamedValue("1024 mA", 1024), NamedValue("1280 mA", 1280), NamedValue("1536 mA", 1536),
                NamedValue("1792 mA", 1792), NamedValue("2048 mA", 2048), NamedValue("2304 mA", 2304), NamedValue("2560 mA", 2560),
                NamedValue("2816 mA", 2816), NamedValue("3072 mA", 3072),
            };

            ValueThresholds chargerThresholds(2048, 2049);

            addConfigButton(HocClkConfigValue_BatteryChargeCurrent, "충전 전류 강제 설정", ValueRange(0, 0, 1, "", 0), "충전 전류 강제 설정",
                            &chargerThresholds, {}, chargerCurrents, false);
        } else {
            std::vector<NamedValue> chargerCurrents = {
                NamedValue("비활성화", 0),  NamedValue("1024 mA", 1024), NamedValue("1280 mA", 1280), NamedValue("1536 mA", 1536),
                NamedValue("1664 mA", 1664),  // Why Nintendo?
                NamedValue("1792 mA", 1792), NamedValue("2048 mA", 2048), NamedValue("2304 mA", 2304), NamedValue("2560 mA", 2560),
            };

            ValueThresholds chargerThresholds(1664, 1793);

            addConfigButton(HocClkConfigValue_BatteryChargeCurrent, "충전 전류 강제 설정", ValueRange(0, 0, 1, "", 0), "충전 전류 강제 설정",
                            &chargerThresholds, {}, chargerCurrents, false);
        }

        tsl::elm::CustomDrawer *inputLimitWarningText = new tsl::elm::CustomDrawer([](tsl::gfx::Renderer *renderer, s32 x, s32 y, s32 w, s32 h) {
            renderer->drawString("\uE150 설정 시 전력 소모 증가로 인하여", false, x + 15, y + 50, 18, tsl::style::color::ColorText);
            renderer->drawString("기판에 부담을 줄 수 있습니다.", false, x + 38, y + 70, 18, tsl::style::color::ColorText);
            renderer->drawString("최대 1500 mA 사용을 권장합니다.", false, x + 38, y + 90, 18, tsl::style::color::ColorText);
        });
        inputLimitWarningText->setBoundaries(0, 0, tsl::cfg::FramebufferWidth, 110);
        this->listElement->addItem(inputLimitWarningText);

        // having an option for hoag would be cool and disabling 100-500 and 2000+
        std::vector<NamedValue> inputCurrentLimits = {
            NamedValue("비활성화", 0),
            NamedValue("100mA", 100),
            NamedValue("150mA", 150),
            NamedValue("500mA", 500),
            NamedValue("900mA", 900, "라이트 (기본)"),
            NamedValue("1200mA", 1200, "기본"),
            NamedValue("1500mA", 1500),
            NamedValue("2000mA", 2000),
            NamedValue("3000mA", 3000),
        };

        ValueThresholds inputLimitThresholds(1500, 1501);

        addConfigButton(HocClkConfigValue_InputCurrentLimit, "입력 전류 강제 제한", ValueRange(0, 0, 1, "", 0),
                        "입력 전류 강제 제한", &inputLimitThresholds, {}, inputCurrentLimits, false);

        if (IsAula()) {
            std::vector<NamedValue> displayClrPreset = {
                NamedValue("설정 안 함", AulaDisplayColorMode_DoNotOverride),
                NamedValue("Basic", AulaDisplayColorMode_Basic),
                NamedValue("Saturated", AulaDisplayColorMode_Saturated),
                NamedValue("Washed", AulaDisplayColorMode_Washed),
                NamedValue("Natural", AulaDisplayColorMode_Natural),
                NamedValue("Vivid", AulaDisplayColorMode_Vivid),
                NamedValue("Washed", AulaDisplayColorMode_Night0, "Night"),
                NamedValue("Basic", AulaDisplayColorMode_Night1, "Night"),
                NamedValue("Natural", AulaDisplayColorMode_Night2, "Night"),
                NamedValue("Vivid", AulaDisplayColorMode_Night3, "Night"),
            };

            addConfigButton(HocClkConfigValue_AulaDisplayColorPreset, "색상 프리셋", ValueRange(0, 1, 1, "", 0), "색상 프리셋",
                            &thresholdsDisabled, {}, displayClrPreset, false, false);
        }
    }
};

class GovernorSettingsSubMenuGui : public MiscGui {
    public:
    GovernorSettingsSubMenuGui() {
    }

    protected:
    void listUI() override {
        Result rc = hocclkIpcGetConfigValues(this->configList);
        if (R_FAILED(rc)) [[unlikely]] {
            FatalGui::openWithResultCode("hocclkIpcGetConfigValues", rc);
            return;
        }
        this->listElement->addItem(new CompactCategoryHeader("클럭 제어 옵션"));
        ValueThresholds thresholdsDisabled(0, 0);

        std::vector<NamedValue> GovernorMinHz = {
            NamedValue("510 MHz", 510000000), NamedValue("612 MHz", 612000000), NamedValue("714 MHz", 714000000),
            NamedValue("816 MHz", 816000000), NamedValue("918 MHz", 918000000), NamedValue("1020 MHz", 1020000000),
        };

        addConfigButton(HocClkConfigValue_CpuGovernorMinimumFreq, "CPU 최소 클럭", ValueRange(0, 0, 1, "", 0),
                        "CPU 최소 클럭", &thresholdsDisabled, {}, GovernorMinHz, false);
    }
};

class DisplaySubMenuGui : public MiscGui {
    public:
    DisplaySubMenuGui() {
    }

    protected:
    void listUI() override {
        Result rc = hocclkIpcGetConfigValues(this->configList);
        if (R_FAILED(rc)) [[unlikely]] {
            FatalGui::openWithResultCode("hocclkIpcGetConfigValues", rc);
            return;
        }
        ValueThresholds thresholdsDisabled(0, 0);

        BaseMenuGui::refresh();  // get latest context
        if (!this->context)
            return;

        this->listElement->addItem(new CompactCategoryHeader("주사율 옵션"));
        addConfigToggle(HocClkConfigValue_OverwriteRefreshRate, nullptr);
        if (!this->context->isUsingRetroSuper) {
            tsl::elm::CustomDrawer *warningText = new tsl::elm::CustomDrawer([](tsl::gfx::Renderer *renderer, s32 x, s32 y, s32 w, s32 h) {
                renderer->drawString("\uE150 안전하지 않은 주사율 사용 시 디스플레이에", false, x, y + 50, 18, tsl::warningTextColor);
                renderer->drawString("부담이나 손상을 줄 수 있습니다!", false, x + 23, y + 70, 18, tsl::warningTextColor);
            });

            warningText->setBoundaries(0, 0, tsl::cfg::FramebufferWidth, 110);
            this->listElement->addItem(warningText);
            ValueThresholds displayThresholds(60, 65);
            addConfigButton(HocClkConfigValue_MaxDisplayClockH, "휴대 모드 최대 주사율", ValueRange(60, IsAula() ? 65 : 75, 1, " Hz", 1),
                            "주사율", &displayThresholds, {}, {}, false);
        }
        if (!IsAula()) {
            tsl::elm::CustomDrawer *warningTextDV = new tsl::elm::CustomDrawer([](tsl::gfx::Renderer *renderer, s32 x, s32 y, s32 w, s32 h) {
                renderer->drawString("\uE150 디스플레이 패널 손상을 방지하려면", false, x, y + 50, 18, tsl::warningTextColor);
                renderer->drawString("전압 조정에 주의하십시오!", false, x + 23, y + 70, 18, tsl::warningTextColor);
            });
            warningTextDV->setBoundaries(0, 0, tsl::cfg::FramebufferWidth, 110);
            this->listElement->addItem(warningTextDV);
            addConfigButton(HocClkConfigValue_DisplayVoltage, "표시 전압", ValueRange(800, 1200, 25, " mV", 1), "표시 전압",
                            &thresholdsDisabled, {}, {}, false);
        }
    }
};

class SafetySubMenuGui : public MiscGui {
    public:
    SafetySubMenuGui() {
    }

    protected:
    void listUI() override {
        Result rc = hocclkIpcGetConfigValues(this->configList);
        if (R_FAILED(rc)) [[unlikely]] {
            FatalGui::openWithResultCode("hocclkIpcGetConfigValues", rc);
            return;
        }
        this->listElement->addItem(new CompactCategoryHeader("안전 옵션"));
        addConfigToggle(HocClkConfigValue_UncappedClocks, nullptr);
        addConfigToggle(HocClkConfigValue_ThermalThrottle, nullptr);

#if IS_MINIMAL == 0
        ValueThresholds throttleThresholds(70, 80);
        addConfigButton(HocClkConfigValue_ThermalThrottleThreshold, "제한 온도", ValueRange(50, 85, 1, "°C", 1), "온도",
                        &throttleThresholds);
#endif
    }
};

class RamSubmenuGui : public MiscGui {
    public:
    RamSubmenuGui() {
    }

    protected:
    void listUI() override {
        BaseMenuGui::refresh();
        if (!this->context)
            return;

        Result rc = hocclkIpcGetConfigValues(this->configList);
        if (R_FAILED(rc)) [[unlikely]] {
            FatalGui::openWithResultCode("hocclkIpcGetConfigValues", rc);
            return;
        }
        ValueThresholds thresholdsDisabled(0, 0);
        std::vector<NamedValue> noNamedValues = {};

        this->listElement->addItem(new CompactCategoryHeader("RAM 옵션"));

        addMappedConfigTrackbar(KipConfigValue_emcDvbShift, "DVB 조정값",
                                { 0xFFFFFFFCu, 0xFFFFFFFDu, 0xFFFFFFFEu, 0xFFFFFFFFu, 0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u },
                                { "-4", "-3", "-2", "-1", " 0", "1", "2", "3", "4", "5", "6", "7", "8" });

        if (IsMariko()) {
            u32 socSpeedo = this->context->speedos[HocClkSpeedo_SOC];
            std::string autoText = "1000 mV";
            if (socSpeedo <= 1597) {
                autoText = "1050 mV";
            } else if (socSpeedo <= 1708) {
                autoText = "1025 mV";
            } else if (socSpeedo >= 1709) {
                autoText = "1000 mV";
            }

            std::vector<NamedValue> marikovmaxconf = {
                NamedValue("설정 안 함", 0, autoText),
                NamedValue("1000 mV", 1000),
                NamedValue("1025 mV", 1025),
                NamedValue("1050 mV", 1050),
                NamedValue("1075 mV", 1075),
                NamedValue("1100 mV", 1100),
                NamedValue("1125 mV", 1125),
                NamedValue("1150 mV", 1150),
                NamedValue("1175 mV", 1175),
                NamedValue("1200 mV", 1200),
            };
            ValueThresholds marikovmaxT(1075, 1150);

            addConfigButton(KipConfigValue_marikoSocVmax, "SoC 최대 전압", ValueRange(0, 12, 1, "", 0), "SoC 최대 전압", &marikovmaxT, {},
                            marikovmaxconf, false, true);
        }

        addConfigToggle(KipConfigValue_hpMode, "HP 모드", true);

        std::map<uint32_t, std::string> emc_voltage_label = {
            { 1100000, "기본 (Mariko)" }, { 1125000, "기본 (Erista)" }, { 1175000, "표준" },
            { 1212500, "안전 최대치 (Mariko)" }, { 1237500, "안전 최대치 (Erista)" }, { 1250000, "불안정 최대치" },
        };

        ValueThresholds vdd2Thresholds(IsMariko() ? 1212500 : 1237500, IsMariko() ? 1250000 : 1275000);
        addConfigButton(KipConfigValue_commonEmcMemVolt, "VDD2 전압", ValueRange(912500, 1350000, 12500, "mV", 1000, 1), "VDD2 전압",
                        &vdd2Thresholds, emc_voltage_label, noNamedValues, false, true);

        if (IsMariko()) {
            ValueThresholds vddqThresholds(675000, 725000);
            addConfigButton(KipConfigValue_marikoEmcVddqVolt, "VDDQ 전압", ValueRange(400000, 750000, 5000, "mV", 1000), "VDDQ 전압",
                            &vddqThresholds, {}, {}, false, true);
        }

        if (IsMariko()) {
            std::vector<NamedValue> stepMode = {
                NamedValue("66 MHz", 0),
                NamedValue("100 MHz", 1),
                NamedValue("133 MHz", 3),  // Mantain compatability
                NamedValue("JEDEC", 2),
            };

            addConfigButton(KipConfigValue_stepMode, "단계별 조정 모드", ValueRange(0, 0, 2, "", 0), "단계별 조정 모드", &thresholdsDisabled, {}, stepMode, false,
                            true);
        }

        std::vector<NamedValue> emcMaxClock = {};
        RamDisplayUnit unit = (RamDisplayUnit)this->configList->values[HocClkConfigValue_RamDisplayUnit];

        if (IsErista()) {
            emcMaxClock = {
                NamedValue("Disabled", 1600000),
                NamedValue("1633 MHz", 1633000),
                NamedValue("1666 MHz", 1666000),
                NamedValue("1700 MHz", 1700000),
                NamedValue("1733 MHz", 1733000),
                // NamedValue("1766 MHz", 1766000),
                NamedValue("1800 MHz", 1800000),
                NamedValue("1833 MHz", 1833000),
                NamedValue("1862 MHz", 1862400, "JEDEC"),
                NamedValue("1881 MHz", 1881600),
                NamedValue("1900 MHz", 1900800),
                NamedValue("1920 MHz", 1920000),
                NamedValue("1939 MHz", 1939200),
                NamedValue("1958 MHz", 1958400),
                NamedValue("1977 MHz", 1977600),
                NamedValue("1996 MHz", 1996800, "JEDEC"),
                NamedValue("2016 MHz", 2016000),
                NamedValue("2035 MHz", 2035200),
                NamedValue("2054 MHz", 2054400),
                NamedValue("2073 MHz", 2073600),
                NamedValue("2092 MHz", 2092800),
                NamedValue("2112 MHz", 2112000),
                NamedValue("2131 MHz", 2131200, "JEDEC"),
                NamedValue("2150 MHz", 2150400),
                NamedValue("2169 MHz", 2169600),
                NamedValue("2188 MHz", 2188800),
                NamedValue("2208 MHz", 2208000),
                NamedValue("2227 MHz", 2227200),
                NamedValue("2246 MHz", 2246400),
                NamedValue("2265 MHz", 2265600),
                NamedValue("2284 MHz", 2284800),
                NamedValue("2304 MHz", 2304000),
                NamedValue("2323 MHz", 2323200),
                NamedValue("2342 MHz", 2342400),
                NamedValue("2361 MHz", 2361600),
                NamedValue("2380 MHz", 2380800),
                NamedValue("2400 MHz", 2400000, "JEDEC"),
            };
        } else {
            emcMaxClock = {
                NamedValue("1600 MHz", 1600000),
                NamedValue("1633 MHz", 1633000),
                NamedValue("1666 MHz", 1666000),
                NamedValue("1700 MHz", 1700000),
                NamedValue("1733 MHz", 1733000),
                NamedValue("1766 MHz", 1766000),
                NamedValue("1800 MHz", 1800000),
                NamedValue("1833 MHz", 1833000),
                NamedValue("1866 MHz", 1866000, "JEDEC"),
                NamedValue("1900 MHz", 1900000),
                NamedValue("1933 MHz", 1933000),
                NamedValue("1966 MHz", 1966000),
                NamedValue("1996 MHz", 1996800, "JEDEC"),
                NamedValue("2000 MHz", 2000000),
                NamedValue("2033 MHz", 2033000),
                NamedValue("2066 MHz", 2066000),
                NamedValue("2100 MHz", 2100000),
                NamedValue("2133 MHz", 2133000, "JEDEC"),
                NamedValue("2166 MHz", 2166000),
                NamedValue("2200 MHz", 2200000),
                NamedValue("2233 MHz", 2233000),
                NamedValue("2266 MHz", 2266000),
                NamedValue("2300 MHz", 2300000),
                NamedValue("2333 MHz", 2333000),
                NamedValue("2366 MHz", 2366000),
                NamedValue("2400 MHz", 2400000, "JEDEC"),
                NamedValue("2433 MHz", 2433000),
                NamedValue("2466 MHz", 2466000),
                NamedValue("2500 MHz", 2500000),
                NamedValue("2533 MHz", 2533000),
                NamedValue("2566 MHz", 2566000),
                NamedValue("2600 MHz", 2600000),
                NamedValue("2633 MHz", 2633000),
                NamedValue("2666 MHz", 2666000, "JEDEC"),
                NamedValue("2700 MHz", 2700000),
                NamedValue("2733 MHz", 2733000),
                NamedValue("2766 MHz", 2766000),
                NamedValue("2800 MHz", 2800000),
                NamedValue("2833 MHz", 2833000),
                NamedValue("2866 MHz", 2866000),
                NamedValue("2900 MHz", 2900000),
                NamedValue("2933 MHz", 2933000, "JEDEC"),
                NamedValue("2966 MHz", 2966000),
                NamedValue("3000 MHz", 3000000),
                NamedValue("3033 MHz", 3033000),
                NamedValue("3066 MHz", 3066000),
                NamedValue("3100 MHz", 3100000),
                NamedValue("3133 MHz", 3133000),
                NamedValue("3166 MHz", 3166000),
                NamedValue("3200 MHz", 3200000, "JEDEC"),
                NamedValue("3233 MHz", 3233000, "높은 speedo 값 필요!"),
                NamedValue("3266 MHz", 3266000, "높은 speedo 값 필요!"),
                NamedValue("3300 MHz", 3300000, "높은 speedo 값 필요!"),
            };
        }

        for (auto &nv : emcMaxClock) {
            if (nv.name != "Disabled") {
                nv.name = formatMemClockKhzLabel(nv.value, unit);
            }
        }

        if (IsMariko()) {
            addConfigButton(KipConfigValue_marikoEmcMaxClock, "최대 클럭", ValueRange(0, 1, 1, "", 1), "최대 클럭", &thresholdsDisabled, {},
                            emcMaxClock, false, true);
        } else {
            addConfigButton(KipConfigValue_eristaEmcMaxClock, "최대 클럭", ValueRange(0, 1, 1, "", 1), "최대 클럭", &thresholdsDisabled, {},
                            emcMaxClock, false, true);
        }

        tsl::elm::ListItem *latenciesSubmenu = new tsl::elm::ListItem("지연");
        latenciesSubmenu->setClickListener([](u64 keys) {
            if (keys & HidNpadButton_A) {
                tsl::changeTo<RamLatenciesSubmenuGui>();
                return true;
            }
            return false;
        });
        latenciesSubmenu->setValue(R_ARROW);
        this->listElement->addItem(latenciesSubmenu);

        tsl::elm::ListItem *timingsSubmenu = new tsl::elm::ListItem("타이밍");
        timingsSubmenu->setClickListener([](u64 keys) {
            if (keys & HidNpadButton_A) {
                tsl::changeTo<RamTimingsSubmenuGui>();
                return true;
            }
            return false;
        });
        timingsSubmenu->setValue(R_ARROW);
        this->listElement->addItem(timingsSubmenu);

        if (IsMariko()) {
            tsl::elm::ListItem *socVoltageTable = new tsl::elm::ListItem("SoC 전압 테이블");
            socVoltageTable->setClickListener([](u64 keys) {
                if (keys & HidNpadButton_A) {
                    tsl::changeTo<SocCustomTableSubmenuGui>();
                    return true;
                }
                return false;
            });
            socVoltageTable->setValue(R_ARROW);
            this->listElement->addItem(socVoltageTable);
        }
    }
};

class RamTimingsSubmenuGui : public MiscGui {
    public:
    RamTimingsSubmenuGui() {
    }

    protected:
    void listUI() override {
        Result rc = hocclkIpcGetConfigValues(this->configList);
        if (R_FAILED(rc)) [[unlikely]] {
            FatalGui::openWithResultCode("hocclkIpcGetConfigValues", rc);
            return;
        }
        this->listElement->addItem(new CompactCategoryHeader("RAM 타이밍"));

        addConfigTrackbar(KipConfigValue_t1_tRCD, "t1 tRCD", ValueRange(0, 7, 1));
        addConfigTrackbar(KipConfigValue_t2_tRP, "t2 tRP", ValueRange(0, 7, 1));
        addConfigTrackbar(KipConfigValue_t3_tRAS, "t3 tRAS", ValueRange(0, 9, 1));
        addConfigTrackbar(KipConfigValue_t4_tRRD, "t4 tRRD", ValueRange(0, 6, 1));
        addConfigTrackbar(KipConfigValue_t5_tRFC, "t5 tRFC", ValueRange(0, IsErista() ? 5u : 10u, 1));
        addConfigTrackbar(KipConfigValue_t6_tRTW, "t6 tRTW", ValueRange(0, 9, 1));
        addConfigTrackbar(KipConfigValue_t7_tWTR, "t7 tWTR", ValueRange(0, 9, 1));
        addConfigTrackbar(KipConfigValue_t8_tREFI, "t8 tREFI", ValueRange(0, 6, 1));

        /* Yes this is duplicated code, yes I don't care. */
        std::vector<NamedValue> timingTbreakFreqs = {
            NamedValue("비활성화",       0),
            NamedValue("1633 MHz", 1633000),
            NamedValue("1666 MHz", 1666000),
            NamedValue("1700 MHz", 1700000),
            NamedValue("1733 MHz", 1733000),
            NamedValue("1766 MHz", 1766000),
            NamedValue("1800 MHz", 1800000),
            NamedValue("1833 MHz", 1833000),
            NamedValue("1866 MHz", 1866000, "JEDEC"),
            NamedValue("1900 MHz", 1900000),
            NamedValue("1933 MHz", 1933000),
            NamedValue("1966 MHz", 1966000),
            NamedValue("1996 MHz", 1996800, "JEDEC"),
            NamedValue("2000 MHz", 2000000),
            NamedValue("2033 MHz", 2033000),
            NamedValue("2066 MHz", 2066000),
            NamedValue("2100 MHz", 2100000),
            NamedValue("2133 MHz", 2133000, "JEDEC"),
            NamedValue("2166 MHz", 2166000),
            NamedValue("2200 MHz", 2200000),
            NamedValue("2233 MHz", 2233000),
            NamedValue("2266 MHz", 2266000),
            NamedValue("2300 MHz", 2300000),
            NamedValue("2333 MHz", 2333000),
            NamedValue("2366 MHz", 2366000),
            NamedValue("2400 MHz", 2400000, "JEDEC"),
            NamedValue("2433 MHz", 2433000),
            NamedValue("2466 MHz", 2466000),
            NamedValue("2500 MHz", 2500000),
            NamedValue("2533 MHz", 2533000),
            NamedValue("2566 MHz", 2566000),
            NamedValue("2600 MHz", 2600000),
            NamedValue("2633 MHz", 2633000),
            NamedValue("2666 MHz", 2666000, "JEDEC"),
            NamedValue("2700 MHz", 2700000),
            NamedValue("2733 MHz", 2733000),
            NamedValue("2766 MHz", 2766000),
            NamedValue("2800 MHz", 2800000),
            NamedValue("2833 MHz", 2833000),
            NamedValue("2866 MHz", 2866000),
            NamedValue("2900 MHz", 2900000),
            NamedValue("2933 MHz", 2933000, "JEDEC"),
            NamedValue("2966 MHz", 2966000),
            NamedValue("3000 MHz", 3000000),
            NamedValue("3033 MHz", 3033000),
            NamedValue("3066 MHz", 3066000),
            NamedValue("3100 MHz", 3100000),
            NamedValue("3133 MHz", 3133000),
            NamedValue("3166 MHz", 3166000),
            NamedValue("3200 MHz", 3200000, "JEDEC"),
            NamedValue("3233 MHz", 3233000, "높은 speedo 값 필요!"),
            NamedValue("3266 MHz", 3266000, "높은 speedo 값 필요!"),
            NamedValue("3300 MHz", 3300000, "높은 speedo 값 필요!"),
            // NamedValue("3333MHz (Needs extreme Speedo/PLL)", 3333000),
            // NamedValue("3366MHz (Needs extreme Speedo/PLL)", 3366000),
            // NamedValue("3400MHz (Needs extreme Speedo/PLL)", 3400000),
            // NamedValue("3433MHz (Needs ridiculous Speedo/PLL)", 3433000),
            // NamedValue("3466MHz (Needs ridiculous Speedo/PLL)", 3466000),
            // NamedValue("3500MHz (Needs ridiculous Speedo/PLL)", 3500000),
        };
        RamDisplayUnit unit = (RamDisplayUnit)this->configList->values[HocClkConfigValue_RamDisplayUnit];

        for (size_t i = 1; i < timingTbreakFreqs.size(); ++i) {
            auto &nv = timingTbreakFreqs[i];
            nv.name = formatMemClockKhzLabel(nv.value, unit);
        }

        tsl::elm::CustomDrawer *blankRamtiming = new tsl::elm::CustomDrawer([](tsl::gfx::Renderer *renderer, s32 x, s32 y, s32 w, s32 h) {
            renderer->drawString("", false, x + 20, y + 30, 18, tsl::style::color::ColorText);
        });
        blankRamtiming->setBoundaries(0, 0, tsl::cfg::FramebufferWidth, 30);
        this->listElement->addItem(blankRamtiming);

        ValueThresholds thresholdsDisabled(0, 0);
        this->listElement->addItem(new CompactCategoryHeader("고급 옵션"));
        if (IsMariko()) {
            // tBreak / low-high timing graph (live, reads config each frame)
            {
                HocClkConfigValueList *cfgPtr = this->configList;
                auto *tbreakGraph = new tsl::elm::CustomDrawer([cfgPtr](tsl::gfx::Renderer *renderer, s32 x, s32 y, s32 w, s32 h) {
                    const s32 t6 = (s32)cfgPtr->values[KipConfigValue_t6_tRTW];
                    const s32 t7 = (s32)cfgPtr->values[KipConfigValue_t7_tWTR];
                    const s32 lt6 = (s32)cfgPtr->values[KipConfigValue_low_t6_tRTW];
                    const s32 lt7 = (s32)cfgPtr->values[KipConfigValue_low_t7_tWTR];
                    const s32 t2c = (s32)cfgPtr->values[KipConfigValue_t2_tRP_cap];
                    const uint32_t tbk = (uint32_t)cfgPtr->values[KipConfigValue_timingEmcTbreak];

                    const tsl::Color cT6 = tsl::Color(4, 14, 15, 15);
                    const tsl::Color cT7 = tsl::Color(15, 9, 2, 15);
                    const tsl::Color cT2 = tsl::Color(12, 4, 15, 15);
                    const tsl::Color cAxis = tsl::Color(5, 5, 5, 15);
                    const tsl::Color cTbk = tsl::Color(7, 7, 7, 10);

                    const s32 gx = x + 52;
                    const s32 gw = w - 64;
                    const s32 gy = y + 14;
                    const s32 gh = 72;
                    const s32 axisY = gy + gh;

                    // Y: value 0 = bottom, value 9 = top
                    auto valY = [&](s32 v) -> s32 { return axisY - v * gh / 9; };

                    constexpr uint32_t kRMin = 1600000u, kRMax = 3300000u;
                    auto freqX = [&](uint32_t kHz) -> s32 {
                        if (kHz <= kRMin)
                            return gx;
                        if (kHz >= kRMax)
                            return gx + gw;
                        return gx + (s32)((uint64_t)(kHz - kRMin) * (uint32_t)gw / (kRMax - kRMin));
                    };

                    // Y-axis guide lines at 0, 3, 6, 9
                    for (int v : { 0, 3, 6, 9 }) {
                        char buf[4];
                        snprintf(buf, sizeof(buf), "%d", v);
                        renderer->drawString(buf, false, x + 4, valY(v) + 5, 12, cAxis);
                        renderer->drawRect(gx, valY(v), gw, 1, tsl::Color(3, 3, 3, 15));
                    }
                    renderer->drawRect(gx, gy, 1, gh + 1, cAxis);
                    renderer->drawRect(gx, axisY, gw, 1, cAxis);

                    // tBreak vertical divider
                    if (tbk != 0) {
                        s32 tx = freqX(tbk);
                        renderer->drawRect(tx, gy, 1, gh, cTbk);
                    }

                    // Step line: lowVal below tBreak, hiVal at/above tBreak
                    auto drawTimingLine = [&](s32 lowVal, s32 hiVal, const tsl::Color &c) {
                        if (tbk == 0 || tbk <= kRMin) {
                            s32 yy = valY(hiVal) + 1;
                            renderer->drawRect(gx, yy, gw, 2, c);
                            renderer->drawCircle(gx, yy + 1, 3, true, c);
                            renderer->drawCircle(gx + gw - 1, yy + 1, 3, true, c);
                        } else {
                            s32 tx = freqX(tbk);
                            s32 yLow = valY(lowVal) + 1;
                            s32 yHi = valY(hiVal) + 1;
                            renderer->drawRect(gx, yLow, tx - gx, 2, c);
                            renderer->drawRect(tx, yHi, gx + gw - tx, 2, c);
                            if (yLow != yHi) {
                                s32 topY = yLow < yHi ? yLow : yHi;
                                s32 botY = yLow > yHi ? yLow : yHi;
                                renderer->drawRect(tx, topY, 2, botY - topY + 2, c);
                            }
                            renderer->drawCircle(gx, yLow + 1, 3, true, c);
                            renderer->drawCircle(tx, yLow + 1, 3, true, c);
                            renderer->drawCircle(tx, yHi + 1, 3, true, c);
                            renderer->drawCircle(gx + gw - 1, yHi + 1, 3, true, c);
                        }
                    };

                    drawTimingLine(lt6, t6, cT6);
                    drawTimingLine(lt7, t7, cT7);

                    // t2 tRP cap: constant line
                    s32 yT2 = valY(t2c) + 1;
                    renderer->drawRect(gx, yT2, gw, 2, cT2);
                    renderer->drawCircle(gx, yT2 + 1, 3, true, cT2);
                    renderer->drawCircle(gx + gw - 1, yT2 + 1, 3, true, cT2);

                    // X-axis ruler with sideways bitmap-font labels
                    static const uint8_t kDigBmp[10][5] = {
                        { 7, 5, 5, 5, 7 }, { 6, 2, 2, 2, 7 }, { 7, 1, 7, 4, 7 }, { 7, 1, 3, 1, 7 }, { 5, 5, 7, 1, 1 },
                        { 7, 4, 7, 1, 7 }, { 7, 4, 7, 5, 7 }, { 7, 1, 1, 2, 2 }, { 7, 5, 7, 5, 7 }, { 7, 5, 7, 1, 7 },
                    };
                    const s32 pix = 2, charH = 3 * pix, charW = 5 * pix, charGap = 1;
                    auto drawSidewaysMHz = [&](uint32_t mhz, s32 cx, s32 startY, const tsl::Color &c) {
                        char buf[8];
                        snprintf(buf, sizeof(buf), "%u", mhz);
                        s32 ox = cx - charW / 2;
                        for (int ci = 0; buf[ci]; ci++) {
                            int d = buf[ci] - '0';
                            if (d < 0 || d > 9)
                                continue;
                            s32 cy = startY + ci * (charH + charGap);
                            for (int r = 0; r < 5; r++)
                                for (int col = 0; col < 3; col++)
                                    if ((kDigBmp[d][r] >> (2 - col)) & 1)
                                        renderer->drawRect(ox + (4 - r) * pix, cy + col * pix, pix, pix, c);
                        }
                    };
                    static const uint32_t kRulerMHz[] = {
                        1600, 1733, 1866, 2000, 2133, 2266, 2400, 2533, 2666, 2800, 2933, 3066, 3200, 3300,
                    };
                    for (uint32_t mhz : kRulerMHz) {
                        s32 fx = freqX(mhz * 1000u);
                        renderer->drawRect(fx, axisY, 1, 4, cAxis);
                        drawSidewaysMHz(mhz, fx, axisY + 6, cAxis);
                    }

                    // Legend
                    s32 ly = y + h - 14;
                    renderer->drawRect(gx, ly, 14, 3, cT6);
                    renderer->drawString("t6 tRTW", false, gx + 17, ly + 5, 12, cT6);
                    renderer->drawRect(gx + 80, ly, 14, 3, cT7);
                    renderer->drawString("t7 tWTR", false, gx + 97, ly + 5, 12, cT7);
                    renderer->drawRect(gx + 165, ly, 14, 3, cT2);
                    renderer->drawString("t2 cap", false, gx + 182, ly + 5, 12, cT2);
                });
                tbreakGraph->setBoundaries(0, 0, tsl::cfg::FramebufferWidth, 150);
                this->listElement->addItem(tbreakGraph);
            }

            addConfigButton(KipConfigValue_timingEmcTbreak, "RAM 타이밍 tBreak", ValueRange(0, 1, 1, "", 1), "tBreak", &thresholdsDisabled, {},
                            timingTbreakFreqs, false, true);
            addConfigTrackbar(KipConfigValue_low_t1_tRCD, "Low t1 tRCD", ValueRange(0, 7, 1));
            addConfigTrackbar(KipConfigValue_low_t2_tRP, "Low t2 tRP", ValueRange(0, 7, 1));
            addConfigTrackbar(KipConfigValue_low_t3_tRAS, "Low t3 tRAS", ValueRange(0, 9, 1));
            addConfigTrackbar(KipConfigValue_low_t4_tRRD, "Low t4 tRRD", ValueRange(0, 6, 1));
            addConfigTrackbar(KipConfigValue_low_t5_tRFC, "Low t5 tRFC", ValueRange(0, 10, 1));
            addConfigTrackbar(KipConfigValue_low_t6_tRTW, "Low t6 tRTW", ValueRange(0, 9, 1));
            addConfigTrackbar(KipConfigValue_low_t7_tWTR, "Low t7 tWTR", ValueRange(0, 9, 1));
            addConfigTrackbar(KipConfigValue_low_t8_tREFI, "Low t8 tREFI", ValueRange(0, 6, 1));
            {
                auto *spacer = new tsl::elm::CustomDrawer([](tsl::gfx::Renderer *, s32, s32, s32, s32) {});
                spacer->setBoundaries(0, 0, tsl::cfg::FramebufferWidth, 8);
                this->listElement->addItem(spacer);
            }
            addConfigTrackbar(KipConfigValue_t2_tRP_cap, "1333WL t2 RP 제한값", ValueRange(0, 8, 1));
        }
        addMappedConfigTrackbar(KipConfigValue_t6_tRTW_fine_tune, "t6 tRTW 미세 조정", { 0xFFFFFFFEu, 0xFFFFFFFFu, 0u, 1u, 2u },
                                { "-2", "-1", " 0", "+1", "+2" });
        addMappedConfigTrackbar(KipConfigValue_t7_tWTR_fine_tune, "t7 tWTR 미세 조정", { 0xFFFFFFFDu, 0xFFFFFFFEu, 0xFFFFFFFFu, 0u, 1u, 2u, 3u },
                                { "-3", "-2", "-1", " 0", "+1", "+2", "+3" });
    }
};

class RamLatenciesSubmenuGui : public MiscGui {
    public:
    RamLatenciesSubmenuGui() {
    }

    tsl::elm::Element *baseUI() override {
        auto *list = new TopAnchoredList();
        this->listElement = list;
        this->listUI();
        return list;
    }

    protected:
    void normalizeLatencies(const HocClkConfigValue keysArr[4]) {
        uint32_t maxClock = IsMariko() ? (uint32_t)this->configList->values[KipConfigValue_marikoEmcMaxClock]
                                       : (uint32_t)this->configList->values[KipConfigValue_eristaEmcMaxClock];
        uint32_t vals[4];

        for (int i = 0; i < 4; i++) {
            vals[i] = (uint32_t)this->configList->values[keysArr[i]];
            if (vals[i] == 0xFFFFFFFFu)
                vals[i] = maxClock;
        }

        uint32_t currentLimit = 0;
        for (int i = 3; i >= 0; i--) {
            if (vals[i] != 0) {
                if (currentLimit != 0 && vals[i] > currentLimit) {
                    vals[i] = currentLimit;
                }
                currentLimit = vals[i];
            }
        }

        uint32_t last = 0;
        for (int i = 0; i < 4; i++) {
            if (vals[i] == 0)
                continue;

            if (vals[i] < last)
                vals[i] = last;
            if (vals[i] > maxClock)
                vals[i] = maxClock;

            last = vals[i];
        }

        for (int i = 0; i < 4; i++) {
            this->configList->values[keysArr[i]] = vals[i];
        }
    }

    void listUI() override {
        ValueThresholds thresholdsDisabled(0, 0);
        Result rc = hocclkIpcGetConfigValues(this->configList);
        if (R_FAILED(rc)) [[unlikely]] {
            FatalGui::openWithResultCode("hocclkIpcGetConfigValues", rc);
            return;
        }

        uint32_t maxClock = IsMariko() ? (uint32_t)this->configList->values[KipConfigValue_marikoEmcMaxClock]
                                       : (uint32_t)this->configList->values[KipConfigValue_eristaEmcMaxClock];
        RamDisplayUnit unit = (RamDisplayUnit)this->configList->values[HocClkConfigValue_RamDisplayUnit];

        static std::vector<uint32_t> kFreqOptions = {};
        if (IsMariko()) {
            kFreqOptions = {
                1633000, 1666000, 1700000, 1733000, 1766000, 1800000, 1833000, 1866000, 1900000, 1933000, 1966000, 1996800, 2000000,
                2033000, 2066000, 2100000, 2133000, 2166000, 2200000, 2233000, 2266000, 2300000, 2333000, 2366000, 2400000, 2433000,
                2466000, 2500000, 2533000, 2566000, 2600000, 2633000, 2666000, 2700000, 2733000, 2766000, 2800000, 2833000, 2866000,
                2900000, 2933000, 2966000, 3000000, 3033000, 3066000, 3100000, 3133000, 3166000, 3200000, 3233000, 3266000, 3300000,
            };
        } else {
            kFreqOptions = {
                1633000, 1666000, 1700000, 1733000, 1800000, 1833000, 1862400, 1881600, 1900800, 1920000, 1939200, 1958400,
                1977600, 1996800, 2016000, 2035200, 2054400, 2073600, 2092800, 2112000, 2131200, 2150400, 2169600, 2188800,
                2208000, 2227200, 2246400, 2265600, 2284800, 2304000, 2323200, 2342400, 2361600, 2380800, 2400000,
            };
        }

        static const HocClkConfigValue kLatencyRKeys[4] = {
            KipConfigValue_read_latency_1333,
            KipConfigValue_read_latency_1600,
            KipConfigValue_read_latency_1866,
            KipConfigValue_read_latency_2133,
        };
        static const HocClkConfigValue kLatencyWKeys[4] = {
            KipConfigValue_write_latency_1333,
            KipConfigValue_write_latency_1600,
            KipConfigValue_write_latency_1866,
            KipConfigValue_write_latency_2133,
        };

        static const char *kTierLabels[4] = { "1333 최대 지연", "1600 최대 지연", "1866 최대 지연", "2133 최대 지연" };

        auto buildNamedValues = [&](int tierIdx) -> std::vector<NamedValue> {
            std::vector<NamedValue> nv;
            nv.push_back(NamedValue("⋯", 0u));
            if (tierIdx == 3) {
                nv.push_back(NamedValue(formatMemClockKhzLabel(maxClock, unit), maxClock));
                nv.push_back(NamedValue(formatMemClockKhzLabel(maxClock, unit), 0xFFFFFFFFu));
            } else {
                for (uint32_t freq : kFreqOptions) {
                    if (freq > maxClock)
                        continue;
                    nv.push_back(NamedValue(formatMemClockKhzLabel(freq, unit), freq));
                }
                nv.push_back(NamedValue(formatMemClockKhzLabel(maxClock, unit), maxClock));
                nv.push_back(NamedValue(formatMemClockKhzLabel(maxClock, unit), 0xFFFFFFFFu));
            }
            return nv;
        };

        auto makeValueText = [&](uint32_t rawVal) -> std::string {
            if (rawVal == 0)
                return "-";
            if (rawVal == 0xFFFFFFFFu)
                return formatMemClockKhzLabel(maxClock, unit);
            return formatMemClockKhzLabel(rawVal, unit);
        };

        auto addLatencyRow = [&](const char *label, int tierIdx, const HocClkConfigValue keysArr[4]) {
            HocClkConfigValue thisKey = keysArr[tierIdx];
            uint32_t currentVal = (uint32_t)this->configList->values[thisKey];

            tsl::elm::ListItem *item = new tsl::elm::ListItem(label);
            item->setValue(makeValueText(currentVal));

            item->setClickListener([this, tierIdx, thisKey, keysArr, label](u64 keys) -> bool {
                auto infoStrings = ConfigInfoStrings(thisKey, IsMariko(), IsHoag());
                if (!infoStrings.empty() && (keys & HidNpadButton_Y) && !(keys & ~HidNpadButton_Y)) {
                    tsl::changeTo<InfoGui>(std::string(label), infoStrings);
                    return true;
                }
                if ((keys & HidNpadButton_A) == 0)
                    return false;

                uint32_t vals[4];
                for (int i = 0; i < 4; i++)
                    vals[i] = (uint32_t)this->configList->values[keysArr[i]];

                uint32_t maxClock = IsMariko() ? (uint32_t)this->configList->values[KipConfigValue_marikoEmcMaxClock]
                                               : (uint32_t)this->configList->values[KipConfigValue_eristaEmcMaxClock];
                RamDisplayUnit unit = (RamDisplayUnit)this->configList->values[HocClkConfigValue_RamDisplayUnit];

                auto resolveVal = [maxClock](uint32_t v) -> uint32_t { return (v == 0xFFFFFFFFu) ? maxClock : v; };

                if (tierIdx == 3) {
                    bool maxOccupied = false;
                    for (int i = 0; i < 3; i++) {
                        if (resolveVal(vals[i]) == maxClock) {
                            maxOccupied = true;
                            break;
                        }
                    }

                    std::vector<NamedValue> opts;
                    opts.push_back(NamedValue("-", 0u));

                    if (!maxOccupied) {
                        opts.push_back(NamedValue(formatMemClockKhzLabel(maxClock, unit), maxClock));
                    }

                    uint32_t displayCurrent = resolveVal(vals[3]);
                    if (maxOccupied && displayCurrent == maxClock) {
                        displayCurrent = 0;
                    }

                    tsl::changeTo<ValueChoiceGui>(
                        displayCurrent, ValueRange(0, 0, 1, "", 1), std::string("2133 최대 지연"),
                        [this, thisKey, keysArr](uint32_t chosen) -> bool {
                            this->configList->values[thisKey] = chosen;
                            Result rc = hocclkIpcSetConfigValues(this->configList);
                            if (R_FAILED(rc)) {
                                FatalGui::openWithResultCode("hocclkIpcSetConfigValues", rc);
                                return false;
                            }
                            shouldSaveKip = true;
                            this->lastContextUpdate = armGetSystemTick();
                            return true;
                        },
                        ValueThresholds(), false, std::map<uint32_t, std::string>{}, opts, false, false);
                    return true;
                }

                uint32_t lowerBound = 0;
                for (int i = 0; i < tierIdx; i++) {
                    uint32_t v = resolveVal(vals[i]);
                    if (v != 0 && v > lowerBound)
                        lowerBound = v;
                }

                uint32_t upperBound = 0;
                for (int i = tierIdx + 1; i < 4; i++) {
                    uint32_t v;
                    if (i == 3) {
                        uint32_t r = resolveVal(vals[i]);
                        v = (r != 0) ? maxClock : 0;
                    } else {
                        v = resolveVal(vals[i]);
                    }
                    if (v != 0 && (upperBound == 0 || v < upperBound))
                        upperBound = v;
                }

                std::vector<NamedValue> opts;
                opts.push_back(NamedValue("-", 0u));
                for (uint32_t freq : kFreqOptions) {
                    if (freq <= lowerBound)
                        continue;
                    if (freq > maxClock)
                        continue;
                    if (upperBound != 0 && freq >= upperBound)
                        continue;
                    opts.push_back(NamedValue(formatMemClockKhzLabel(freq, unit), freq));
                }

                uint32_t displayCurrent = resolveVal(vals[tierIdx]);
                bool currentInList = false;
                for (auto &nv : opts)
                    if (nv.value == displayCurrent) {
                        currentInList = true;
                        break;
                    }
                if (!currentInList)
                    displayCurrent = 0;

                tsl::changeTo<ValueChoiceGui>(
                    displayCurrent, ValueRange(0, 0, 1, "", 1), std::string("최대 지연"),
                    [this, thisKey, keysArr](uint32_t chosen) -> bool {
                        this->configList->values[thisKey] = chosen;
                        normalizeLatencies(keysArr);
                        Result rc = hocclkIpcSetConfigValues(this->configList);
                        if (R_FAILED(rc)) {
                            FatalGui::openWithResultCode("hocclkIpcSetConfigValues", rc);
                            return false;
                        }
                        shouldSaveKip = true;
                        this->lastContextUpdate = armGetSystemTick();
                        return true;
                    },
                    ValueThresholds(), false, std::map<uint32_t, std::string>{}, opts, false, false);
                return true;
            });

            this->listElement->addItem(item);
            this->configButtons[thisKey] = item;
            this->configRanges[thisKey] = ValueRange(0, 0, 1, "", 1);
            this->configNamedValues[thisKey] = buildNamedValues(tierIdx);
        };

        this->listElement->addItem(new CompactCategoryHeader("지연 그래프"));

        {
            HocClkConfigValueList *cfgPtr = this->configList;
            bool mariko = IsMariko();

            auto *graph = new tsl::elm::CustomDrawer([cfgPtr, mariko](tsl::gfx::Renderer *renderer, s32 x, s32 y, s32 w, s32 h) {
                static const HocClkConfigValue kR[4] = {
                    KipConfigValue_read_latency_1333,
                    KipConfigValue_read_latency_1600,
                    KipConfigValue_read_latency_1866,
                    KipConfigValue_read_latency_2133,
                };
                static const HocClkConfigValue kW[4] = {
                    KipConfigValue_write_latency_1333,
                    KipConfigValue_write_latency_1600,
                    KipConfigValue_write_latency_1866,
                    KipConfigValue_write_latency_2133,
                };

                uint32_t capMax =
                    mariko ? (uint32_t)cfgPtr->values[KipConfigValue_marikoEmcMaxClock] : (uint32_t)cfgPtr->values[KipConfigValue_eristaEmcMaxClock];

                uint32_t rv[4], wv[4];
                for (int i = 0; i < 4; i++) {
                    rv[i] = (uint32_t)cfgPtr->values[kR[i]];
                    wv[i] = (uint32_t)cfgPtr->values[kW[i]];
                    if (rv[i] == 0xFFFFFFFFu)
                        rv[i] = capMax;
                    if (wv[i] == 0xFFFFFFFFu)
                        wv[i] = capMax;
                }

                const tsl::Color cRead = tsl::Color(4, 14, 15, 15);
                const tsl::Color cWrite = tsl::Color(15, 9, 2, 15);
                const tsl::Color cMerge = tsl::Color(5, 15, 4, 15);
                const tsl::Color cAxis = tsl::Color(5, 5, 5, 15);

                const s32 gx = x + (mariko ? 52 : 47);
                const s32 gw = w - (mariko ? 64 : 59);
                const s32 gy = y + 14;
                const s32 gh = 54;
                const s32 th = gh / 3;
                const s32 axisY = gy + gh;

                auto tierY = [&](int i) -> s32 { return gy + gh - i * th; };
                const uint32_t kRMin = 1600000u;
                const uint32_t kRMax = mariko ? 3300000u : 2400000u;
                auto freqX = [&](uint32_t kHz) -> s32 {
                    if (kHz <= kRMin)
                        return gx;
                    if (kHz >= kRMax)
                        return gx + gw;
                    return gx + (s32)((uint64_t)(kHz - kRMin) * (uint32_t)gw / (kRMax - kRMin));
                };

                const char *tierLabels[4] = { "1333", "1600", "1866", "2133" };
                for (int i = 0; i < 4; i++) {
                    renderer->drawString(tierLabels[i], false, x + 4, tierY(i) + 5, 12, cAxis);
                    renderer->drawRect(gx, tierY(i), gw, 1, cAxis);
                }
                renderer->drawRect(gx, gy, 1, gh + 1, cAxis);
                renderer->drawRect(gx, axisY, gw, 1, cAxis);

                struct LatSeg {
                    int tier;
                    uint32_t start, end;
                };
                auto buildSegs = [&](const uint32_t *vals) -> std::vector<LatSeg> {
                    struct Pt {
                        int tier;
                        uint32_t freq;
                    };
                    Pt pts[4];
                    int n = 0;
                    for (int i = 0; i < 4; i++)
                        if (vals[i] != 0)
                            pts[n++] = { i, vals[i] };
                    if (n == 0)
                        return {};
                    std::vector<LatSeg> segs;
                    uint32_t prev = kRMin;
                    for (int k = 0; k < n; k++) {
                        if (pts[k].freq > prev)
                            segs.push_back({ pts[k].tier, prev, pts[k].freq });
                        prev = pts[k].freq;
                    }
                    if (prev < kRMax)
                        segs.push_back({ pts[n - 1].tier, prev, kRMax });
                    return segs;
                };

                auto rSegs = buildSegs(rv);
                auto wSegs = buildSegs(wv);

                auto drawSeriesSegs = [&](const std::vector<LatSeg> &segs, const std::vector<LatSeg> &other, const tsl::Color &c, s32 yOff) {
                    for (const auto &seg : segs) {
                        s32 ty = tierY(seg.tier) + yOff;
                        s32 tyMrg = tierY(seg.tier);
                        struct Iv {
                            uint32_t s, e;
                        };
                        Iv ovlp[4];
                        int no = 0;
                        for (const auto &os : other) {
                            if (os.tier != seg.tier)
                                continue;
                            uint32_t s = seg.start > os.start ? seg.start : os.start;
                            uint32_t e = seg.end < os.end ? seg.end : os.end;
                            if (s < e)
                                ovlp[no++] = { s, e };
                        }
                        for (int a = 1; a < no; a++)
                            for (int b = a; b > 0 && ovlp[b - 1].s > ovlp[b].s; b--) {
                                auto t = ovlp[b];
                                ovlp[b] = ovlp[b - 1];
                                ovlp[b - 1] = t;
                            }
                        uint32_t cur = seg.start;
                        for (int oi = 0; oi < no; oi++) {
                            if (cur < ovlp[oi].s) {
                                s32 x0 = freqX(cur), x1 = freqX(ovlp[oi].s);
                                if (x1 > x0)
                                    renderer->drawRect(x0, ty, x1 - x0, 2, c);
                            }
                            s32 x0 = freqX(ovlp[oi].s), x1 = freqX(ovlp[oi].e);
                            if (x1 > x0)
                                renderer->drawRect(x0, tyMrg, x1 - x0, 2, cMerge);
                            cur = ovlp[oi].e;
                        }
                        if (cur < seg.end) {
                            s32 x0 = freqX(cur), x1 = freqX(seg.end);
                            if (x1 > x0)
                                renderer->drawRect(x0, ty, x1 - x0, 2, c);
                        }
                    }
                    for (int k = 0; k + 1 < (int)segs.size(); k++) {
                        if (segs[k].end != segs[k + 1].start)
                            continue;
                        uint32_t transFreq = segs[k].end;
                        bool otherHere = false;
                        for (int j = 0; j + 1 < (int)other.size(); j++)
                            if (other[j].end == transFreq && other[j + 1].start == transFreq) {
                                otherHere = true;
                                break;
                            }
                        s32 fx = freqX(transFreq);
                        s32 y1 = tierY(segs[k].tier) + yOff;
                        s32 y2 = tierY(segs[k + 1].tier) + yOff;
                        s32 topY = y1 < y2 ? y1 : y2;
                        s32 botY = y1 > y2 ? y1 : y2;
                        if (botY > topY)
                            renderer->drawRect(fx, topY, 2, botY - topY + 2, otherHere ? cMerge : c);
                    }
                };

                drawSeriesSegs(rSegs, wSegs, cRead, 0);
                drawSeriesSegs(wSegs, rSegs, cWrite, 0);

                static const uint8_t kDigBmp[10][5] = {
                    { 7, 5, 5, 5, 7 },  // 0
                    { 6, 2, 2, 2, 7 },  // 1
                    { 7, 1, 7, 4, 7 },  // 2
                    { 7, 1, 3, 1, 7 },  // 3
                    { 5, 5, 7, 1, 1 },  // 4
                    { 7, 4, 7, 1, 7 },  // 5
                    { 7, 4, 7, 5, 7 },  // 6
                    { 7, 1, 1, 2, 2 },  // 7
                    { 7, 5, 7, 5, 7 },  // 8
                    { 7, 5, 7, 1, 7 },  // 9
                };
                const s32 charGap = 1;

                auto drawSidewaysMHz = [&](uint32_t mhz, s32 cx, s32 startY, const tsl::Color &c, s32 pixSize) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "%u", mhz);
                    s32 cW = 5 * pixSize;
                    s32 cH = 3 * pixSize;
                    s32 originX = cx - cW / 2;
                    for (int ci = 0; buf[ci]; ci++) {
                        int d = buf[ci] - '0';
                        if (d < 0 || d > 9)
                            continue;
                        s32 cy = startY + ci * (cH + charGap);
                        for (int r = 0; r < 5; r++) {
                            for (int col = 0; col < 3; col++) {
                                if (!((kDigBmp[d][r] >> (2 - col)) & 1))
                                    continue;
                                renderer->drawRect(originX + (4 - r) * pixSize, cy + col * pixSize, pixSize, pixSize, c);
                            }
                        }
                    }
                };

                if (mariko) {
                    static const uint32_t kRulerMHz[] = {
                        1600, 1733, 1866, 2000, 2133, 2266, 2400, 2533, 2666, 2800, 2933, 3066, 3200, 3300,
                    };
                    for (uint32_t mhz : kRulerMHz) {
                        s32 fx = freqX(mhz * 1000u);
                        renderer->drawRect(fx, axisY, 1, 4, cAxis);
                        drawSidewaysMHz(mhz, fx, axisY + 6, cAxis, 2);
                    }
                } else {
                    static const uint32_t kEristaRulerMHz[] = {
                        1600, 1666, 1733, 1800, 1866, 1933, 2000, 2066, 2133, 2200, 2266, 2333, 2400,
                    };
                    for (uint32_t mhz : kEristaRulerMHz) {
                        s32 fx = freqX(mhz * 1000u);
                        renderer->drawRect(fx, axisY, 1, 4, cAxis);
                        drawSidewaysMHz(mhz, fx, axisY + 6, cAxis, 2);
                    }
                }

                // Breakpoint dots
                for (int i = 0; i < 4; i++) {
                    s32 ty = tierY(i) + 1;
                    bool merged = (rv[i] != 0 && rv[i] == wv[i]);
                    if (merged) {
                        renderer->drawCircle(freqX(rv[i]), ty, 4, true, cMerge);
                    } else {
                        if (rv[i])
                            renderer->drawCircle(freqX(rv[i]), ty, 4, true, cRead);
                        if (wv[i])
                            renderer->drawCircle(freqX(wv[i]), ty, 4, true, cWrite);
                    }
                }

                const s32 ly = axisY + 46;
                renderer->drawRect(gx, ly, 14, 3, cRead);
                renderer->drawString("Read", false, gx + 17, ly + 5, 12, cRead);
                renderer->drawRect(gx + 60, ly, 14, 3, cWrite);
                renderer->drawString("Write", false, gx + 77, ly + 5, 12, cWrite);
                renderer->drawRect(gx + 125, ly, 14, 3, cMerge);
                renderer->drawString("Same", false, gx + 142, ly + 5, 12, cMerge);
            });
            graph->setBoundaries(0, 0, tsl::cfg::FramebufferWidth, 150);
            this->listElement->addItem(graph);
        }

        this->listElement->addItem(new CompactCategoryHeader("읽기"));
        for (int i = 0; i < 4; i++)
            addLatencyRow(kTierLabels[i], i, kLatencyRKeys);

        tsl::elm::CustomDrawer *blankLatency = new tsl::elm::CustomDrawer([](tsl::gfx::Renderer *renderer, s32 x, s32 y, s32 w, s32 h) {
            renderer->drawString("", false, x + 20, y + 30, 18, tsl::style::color::ColorText);
        });
        blankLatency->setBoundaries(0, 0, tsl::cfg::FramebufferWidth, 30);
        this->listElement->addItem(blankLatency);

        this->listElement->addItem(new CompactCategoryHeader("쓰기"));
        for (int i = 0; i < 4; i++)
            addLatencyRow(kTierLabels[i], i, kLatencyWKeys);
    }
};

class CpuSubmenuGui : public MiscGui {
    public:
    CpuSubmenuGui() {
    }

    protected:
    void listUI() override {
        Result rc = hocclkIpcGetConfigValues(this->configList);  // populate config list early otherwise wont work
        if (R_FAILED(rc)) [[unlikely]] {
            FatalGui::openWithResultCode("hocclkIpcGetConfigValues", rc);
            return;
        }

        ValueThresholds thresholdsDisabled(0, 0);
        ValueThresholds mCpuClockThresholds(1963500, 2397000);
        ValueThresholds mCpuClockThresholdsUV(2397000, 2499000);
        ValueThresholds eCpuClockThresholds(1785000, 2091000);
        ValueThresholds eCpuClockThresholdsUV(2091000, 2193000);

        this->listElement->addItem(new CompactCategoryHeader("CPU 옵션"));
        if (IsMariko()) {
            addConfigTrackbar(KipConfigValue_marikoCpuUVLow, "저전압 언더볼트", ValueRange(0, 8, 1));
            addConfigTrackbar(KipConfigValue_marikoCpuUVHigh, "고전압 언더볼트", ValueRange(0, 12, 1));

            std::vector<NamedValue> marikoTableConf = { // NamedValue("Auto", 0),
                                                        NamedValue("기본", 1), NamedValue("1581MHz Tbreak", 2), NamedValue("1683MHz Tbreak", 3),
                                                        NamedValue("Extreme 테이블", 4)
            };

            addConfigButton(KipConfigValue_tableConf, "UV 테이블", ValueRange(0, 12, 1, "", 0), "UV 테이블", &thresholdsDisabled, {},
                            marikoTableConf, false, true);

            addConfigButton(KipConfigValue_marikoCpuLowVmin, "저전압 최소값", ValueRange(550, 750, 5, "mV", 1), "최소 전압", &thresholdsDisabled, {},
                            {}, false, true);

            addConfigButton(KipConfigValue_marikoCpuHighVmin, "고전압 최소값", ValueRange(650, 900, 5, "mV", 1), "최소 전압", &thresholdsDisabled, {},
                            {}, false, true);

            ValueThresholds mCpuVoltThresholds(1160, 1180);
            addConfigButton(KipConfigValue_marikoCpuMaxVolt, "최대 전압", ValueRange(1000, 1200, 5, "mV", 1), "최대 전압",
                            &mCpuVoltThresholds, {}, {}, false, true);

            std::vector<NamedValue> maxClkOptions = {
                NamedValue("1963 MHz", 1963500), NamedValue("2091 MHz", 2091000), NamedValue("2193 MHz", 2193000), NamedValue("2295 MHz", 2295000),
                NamedValue("2397 MHz", 2397000), NamedValue("2499 MHz", 2499000), NamedValue("2601 MHz", 2601000), NamedValue("2703 MHz", 2703000),
            };

            addConfigButton(KipConfigValue_marikoCpuMaxClock, "최대 클럭", ValueRange(0, 0, 1, "", 1), "최대 클럭",
                            this->configList->values[KipConfigValue_marikoCpuUVHigh] ? &mCpuClockThresholdsUV : &mCpuClockThresholds, {},
                            maxClkOptions, false, true);

            std::vector<NamedValue> ClkOptions = {
                NamedValue("1963 MHz", 1963500), NamedValue("2091 MHz", 2091000), NamedValue("2193 MHz", 2193000), NamedValue("2295 MHz", 2295000),
                NamedValue("2397 MHz", 2397000), NamedValue("2499 MHz", 2499000), NamedValue("2601 MHz", 2601000), NamedValue("2703 MHz", 2703000),
            };
            std::vector<NamedValue> ClkOptionsRamOc = {
                NamedValue("1122 MHz", 1122000), NamedValue("1224 MHz", 1224000), NamedValue("1326 MHz", 1326000), NamedValue("1428 MHz", 1428000),
                NamedValue("1581 MHz", 1581000), NamedValue("1683 MHz", 1683000), NamedValue("1785 MHz", 1785000), NamedValue("1887 MHz", 1887000),
                NamedValue("1963 MHz", 1963500), NamedValue("2091 MHz", 2091000), NamedValue("2193 MHz", 2193000), NamedValue("2295 MHz", 2295000),
                NamedValue("2397 MHz", 2397000), NamedValue("2499 MHz", 2499000), NamedValue("2601 MHz", 2601000), NamedValue("2703 MHz", 2703000),
            };

            addConfigButton(KipConfigValue_marikoCpuBoostClock, "부스트 클럭", ValueRange(0, 0, 1, "", 1), "부스트 클럭",
                            this->configList->values[KipConfigValue_marikoCpuUVHigh] ? &mCpuClockThresholdsUV : &mCpuClockThresholds, {}, ClkOptions,
                            false, true);

            std::vector<NamedValue> emcMaxClock = {
                NamedValue("1600 MHz", 1600000),
                NamedValue("1633 MHz", 1633000),
                NamedValue("1666 MHz", 1666000),
                NamedValue("1700 MHz", 1700000),
                NamedValue("1733 MHz", 1733000),
                NamedValue("1766 MHz", 1766000),
                NamedValue("1800 MHz", 1800000),
                NamedValue("1833 MHz", 1833000),
                NamedValue("1866 MHz", 1866000, "JEDEC"),
                NamedValue("1900 MHz", 1900000),
                NamedValue("1933 MHz", 1933000),
                NamedValue("1966 MHz", 1966000),
                NamedValue("1996 MHz", 1996800, "JEDEC"),
                NamedValue("2000 MHz", 2000000),
                NamedValue("2033 MHz", 2033000),
                NamedValue("2066 MHz", 2066000),
                NamedValue("2100 MHz", 2100000),
                NamedValue("2133 MHz", 2133000, "JEDEC"),
                NamedValue("2166 MHz", 2166000),
                NamedValue("2200 MHz", 2200000),
                NamedValue("2233 MHz", 2233000),
                NamedValue("2266 MHz", 2266000),
                NamedValue("2300 MHz", 2300000),
                NamedValue("2333 MHz", 2333000),
                NamedValue("2366 MHz", 2366000),
                NamedValue("2400 MHz", 2400000, "JEDEC"),
                NamedValue("2433 MHz", 2433000),
                NamedValue("2466 MHz", 2466000),
                NamedValue("2500 MHz", 2500000),
                NamedValue("2533 MHz", 2533000),
                NamedValue("2566 MHz", 2566000),
                NamedValue("2600 MHz", 2600000),
                NamedValue("2633 MHz", 2633000),
                NamedValue("2666 MHz", 2666000, "JEDEC"),
                NamedValue("2700 MHz", 2700000),
                NamedValue("2733 MHz", 2733000),
                NamedValue("2766 MHz", 2766000),
                NamedValue("2800 MHz", 2800000),
                NamedValue("2833 MHz", 2833000),
                NamedValue("2866 MHz", 2866000),
                NamedValue("2900 MHz", 2900000),
                NamedValue("2933 MHz", 2933000, "JEDEC"),
                NamedValue("2966 MHz", 2966000),
                NamedValue("3000 MHz", 3000000),
                NamedValue("3033 MHz", 3033000),
                NamedValue("3066 MHz", 3066000),
                NamedValue("3100 MHz", 3100000),
                NamedValue("3133 MHz", 3133000),
                NamedValue("3166 MHz", 3166000),
                NamedValue("3200 MHz", 3200000, "JEDEC"),
                NamedValue("3233 MHz", 3233000, "높은 speedo 값 필요!"),
                NamedValue("3266 MHz", 3266000, "높은 speedo 값 필요!"),
                NamedValue("3300 MHz", 3300000, "높은 speedo 값 필요!"),
            };
            addConfigToggle(HocClkConfigValue_AutoRAMCPUOverclock, "RAM 연동 CPU 자동 OC");
            addConfigButton(HocClkConfigValue_AutoRamCpuCpuOCFreq, "자동 OC CPU 클럭", ValueRange(0, 0, 1, "", 1), "CPU 클럭",
                            &thresholdsDisabled, {}, ClkOptionsRamOc, false, false);
            addConfigButton(HocClkConfigValue_AutoRamCpuRamOCThreshold, "자동 OC 램 임계값", ValueRange(0, 0, 1, "", 1), "RAM 클럭",
                            &thresholdsDisabled, {}, emcMaxClock, false, false);
        } else {
            addConfigTrackbar(KipConfigValue_eristaCpuUV, "언더볼트", ValueRange(0, 5, 1));

            addConfigToggle(KipConfigValue_eristaCpuUnlock, "제한 해제", true);

            addConfigButton(KipConfigValue_eristaCpuVmin, "최소 전압", ValueRange(750, 900, 25, "mV", 1), "최소 전압", &thresholdsDisabled, {}, {},
                            false, true);

            ValueThresholds eCpuVoltThresholds(1235, 1260);
            addConfigButton(KipConfigValue_eristaCpuMaxVolt, "최대 전압", ValueRange(1120, 1260, 5, "mV", 1), "최대 전압",
                            &eCpuVoltThresholds, {}, {}, false, true);

            std::vector<NamedValue> maxClkOptions = {
                NamedValue("1785 MHz", 1785), NamedValue("1887 MHz", 1887), NamedValue("1989 MHz", 1989), NamedValue("2091 MHz", 2091),
                NamedValue("2193 MHz", 2193), NamedValue("2295 MHz", 2295), NamedValue("2397 MHz", 2397),
            };
            ValueThresholds eCpuMaxClockThresholds(1785, 2091);
            addConfigButton(HocClkConfigValue_EristaMaxCpuClock, "CPU 최대 클럭", ValueRange(0, 0, 1, "", 1), "CPU 최대 클럭",
                            &eCpuMaxClockThresholds, {}, maxClkOptions, false);
            std::vector<NamedValue> ClkOptionsE = {
                NamedValue("1785 MHz", 1785000), NamedValue("1887 MHz", 1887000), NamedValue("1989 MHz", 1989000), NamedValue("2091 MHz", 2091000),
                NamedValue("2193 MHz", 2193000), NamedValue("2295 MHz", 2295000), NamedValue("2397 MHz", 2397000),
            };
            addConfigButton(KipConfigValue_eristaCpuBoostClock, "부스트 클럭", ValueRange(0, 0, 1, "", 1), "부스트 클럭",
                            this->configList->values[KipConfigValue_eristaCpuUV] ? &eCpuClockThresholdsUV : &eCpuClockThresholds, {}, ClkOptionsE,
                            false, true);
            addConfigToggle(HocClkConfigValue_LiveCpuUv, nullptr);
        }
        addConfigToggle(HocClkConfigValue_OverwriteBoostMode, nullptr);
    }
};

class GpuSubmenuGui : public MiscGui {
    public:
    GpuSubmenuGui() {
    }

    protected:
    void listUI() override {
        Result rc = hocclkIpcGetConfigValues(this->configList);
        if (R_FAILED(rc)) [[unlikely]] {
            FatalGui::openWithResultCode("hocclkIpcGetConfigValues", rc);
            return;
        }
        ValueThresholds thresholdsDisabled(0, 0);
        std::vector<NamedValue> noNamedValues = {};

        this->listElement->addItem(new CompactCategoryHeader("GPU 옵션"));

        std::vector<NamedValue> gpuUvConfM = {
            NamedValue("언더볼트 설정 안 함", 0, "HOS 기본값"),
            NamedValue("SLT 테이블", 1),
            NamedValue("HiOPT 테이블", 2, "HOC 기본값"),
            NamedValue("HiOPT - 15 mV", 3),
            NamedValue("고전압 UV 테이블", 4),
        };

        std::vector<NamedValue> gpuUvConfE = {
            NamedValue("언더볼트 설정 안 함", 0),
            NamedValue("SLT 테이블", 1),
            NamedValue("HiOPT 테이블", 2),
        };
        std::vector<NamedValue> mGpuVoltsVmin = {
            NamedValue("480 mV", 480), NamedValue("485 mV", 485), NamedValue("490 mV", 490), NamedValue("495 mV", 495), NamedValue("500 mV", 500),
            NamedValue("505 mV", 505), NamedValue("510 mV", 510), NamedValue("515 mV", 515), NamedValue("520 mV", 520), NamedValue("525 mV", 525),
            NamedValue("530 mV", 530), NamedValue("535 mV", 535), NamedValue("540 mV", 540), NamedValue("545 mV", 545), NamedValue("550 mV", 550),
            NamedValue("555 mV", 555), NamedValue("560 mV", 560), NamedValue("565 mV", 565), NamedValue("570 mV", 570), NamedValue("575 mV", 575),
            NamedValue("580 mV", 580), NamedValue("585 mV", 585), NamedValue("590 mV", 590), NamedValue("595 mV", 595), NamedValue("600 mV", 600),
            NamedValue("605 mV", 605), NamedValue("610 mV", 610), NamedValue("615 mV", 615), NamedValue("620 mV", 620), NamedValue("625 mV", 625),
            NamedValue("630 mV", 630), NamedValue("635 mV", 635), NamedValue("640 mV", 640), NamedValue("645 mV", 645), NamedValue("650 mV", 650),
            NamedValue("655 mV", 655), NamedValue("660 mV", 660), NamedValue("665 mV", 665), NamedValue("670 mV", 670), NamedValue("675 mV", 675),
            NamedValue("680 mV", 680), NamedValue("685 mV", 685), NamedValue("690 mV", 690), NamedValue("695 mV", 695), NamedValue("700 mV", 700),
            NamedValue("705 mV", 705), NamedValue("710 mV", 710), NamedValue("715 mV", 715), NamedValue("720 mV", 720), NamedValue("725 mV", 725),
            NamedValue("730 mV", 730), NamedValue("735 mV", 735), NamedValue("740 mV", 740), NamedValue("745 mV", 745), NamedValue("750 mV", 750),
            NamedValue("755 mV", 755), NamedValue("760 mV", 760), NamedValue("765 mV", 765), NamedValue("770 mV", 770), NamedValue("775 mV", 775),
            NamedValue("780 mV", 780), NamedValue("785 mV", 785), NamedValue("790 mV", 790), NamedValue("795 mV", 795)
        };

        if (IsErista()) {
            addConfigButton(KipConfigValue_eristaGpuUV, "UV 테이블", ValueRange(0, 1, 1, "", 1), "UV 테이블", &thresholdsDisabled,
                            {}, gpuUvConfE, false, true);
            addConfigButton(KipConfigValue_eristaGpuVmin, "최소 전압", ValueRange(675, 875, 5, "mV", 1), "최소 전압",
                            &thresholdsDisabled, {}, {}, false, true);
        } else {
            addConfigButton(KipConfigValue_marikoGpuUV, "UV 테이블", ValueRange(0, 1, 1, "", 1), "UV 테이블", &thresholdsDisabled,
                            {}, gpuUvConfM, false, true);

            // tsl::elm::ListItem* vminCalcBtn = new tsl::elm::ListItem("Calculate GPU Vmin");
            // vminCalcBtn->setClickListener([this](u64 keys) {
            //     if (keys & HidNpadButton_A) {
            //         Result rc = hocClkIpcCalculateGpuVmin();
            //         if (R_FAILED(rc)) {
            //             FatalGui::openWithResultCode("hocClkIpcCalculateGpuVmin", rc);
            //             return false;
            //         }
            //         return true;
            //     }
            //     return false;
            // });

            addConfigButton(KipConfigValue_marikoGpuVmin, "최소 전압", ValueRange(0, 0, 0, "0", 1), "최소 전압", &thresholdsDisabled, {}, mGpuVoltsVmin,
                            false, true);
            ValueThresholds MgpuVmaxThresholds(805, 850);
            addConfigButton(KipConfigValue_marikoGpuVmax, "최대 전압", ValueRange(800, 960, 5, "mV", 1), "최대 전압",
                            &MgpuVmaxThresholds, {}, {}, false, true);
        }

        std::vector<NamedValue> gpuOffset = {
            NamedValue("-50 mV", static_cast<u32>(-50)),
            NamedValue("-45 mV", static_cast<u32>(-45)),
            NamedValue("-40 mV", static_cast<u32>(-40)),
            NamedValue("-35 mV", static_cast<u32>(-35)),
            NamedValue("-30 mV", static_cast<u32>(-30)),
            NamedValue("-25 mV", static_cast<u32>(-25)),
            NamedValue("-20 mV", static_cast<u32>(-20)),
            NamedValue("-15 mV", static_cast<u32>(-15)),
            NamedValue("-10 mV", static_cast<u32>(-10)),
            NamedValue("-5 mV", static_cast<u32>(-5)),
            NamedValue("0 mV", 0),
            NamedValue("5 mV", 5),
            NamedValue("10 mV", 10),
            NamedValue("15 mV", 15),
            NamedValue("20 mV", 20),
            NamedValue("25 mV", 25),
            NamedValue("30 mV", 30),
            NamedValue("35 mV", 35),
            NamedValue("40 mV", 40),
            NamedValue("45 mV", 45),
            NamedValue("50 mV", 50),
        };

        addConfigButton(KipConfigValue_commonGpuVoltOffset, "전압 오프셋", ValueRange(0, 50, 5, "mV", 1), "전압 오프셋",
                        &thresholdsDisabled, {}, gpuOffset, false, true);

        std::vector<NamedValue> gpuSchedValues = {
            NamedValue("설정 안 함", GpuSchedulingMode_DoNotOverride),
            NamedValue("활성화 (기본)", GpuSchedulingMode_Enabled, "96.6% 제한"),
            NamedValue("비활성화", GpuSchedulingMode_Disabled, "99.7% 제한"),
        };

        addConfigButton(HocClkConfigValue_GPUScheduling, "스케줄링", ValueRange(0, 0, 1, "", 0), "스케줄링",
                        &thresholdsDisabled, {}, gpuSchedValues, false);

        std::vector<NamedValue> dvfsOffset = {
            NamedValue("-80 mV", 0xFFFFFFB0), NamedValue("-75 mV", 0xFFFFFFB5), NamedValue("-70 mV", 0xFFFFFFBA), NamedValue("-65 mV", 0xFFFFFFBF),
            NamedValue("-60 mV", 0xFFFFFFC4), NamedValue("-55 mV", 0xFFFFFFC9), NamedValue("-50 mV", 0xFFFFFFCE), NamedValue("-45 mV", 0xFFFFFFD3),
            NamedValue("-40 mV", 0xFFFFFFD8), NamedValue("-35 mV", 0xFFFFFFDD), NamedValue("-30 mV", 0xFFFFFFE2), NamedValue("-25 mV", 0xFFFFFFE7),
            NamedValue("-20 mV", 0xFFFFFFEC), NamedValue("-15 mV", 0xFFFFFFF1), NamedValue("-10 mV", 0xFFFFFFF6), NamedValue(" -5 mV", 0xFFFFFFFB),
            NamedValue("비활성화", 0),        NamedValue(" +5 mV", 5),          NamedValue("+10 mV", 10),         NamedValue("+15 mV", 15),
            NamedValue("+20 mV", 20),
        };

        std::vector<NamedValue> dvfsValues = {
            NamedValue("비활성화", DVFSMode_Disabled),
            NamedValue("PCV 제어 우회", DVFSMode_Hijack),
            // NamedValue("Official Service", DVFSMode_OfficialService),
        };

        addConfigButton(HocClkConfigValue_DVFSMode, "DVFS 모드", ValueRange(0, 0, 1, "", 0), "DVFS 모드", &thresholdsDisabled, {}, dvfsValues,
                        false);

        addConfigButton(HocClkConfigValue_DVFSOffset, "DVFS 오프셋", ValueRange(0, 12, 1, "", 0), "DVFS 오프셋", &thresholdsDisabled, {},
                        dvfsOffset, false);

        tsl::elm::ListItem *customTableSubmenu = new tsl::elm::ListItem("전압 테이블");
        customTableSubmenu->setClickListener([](u64 keys) {
            if (keys & HidNpadButton_A) {
                tsl::changeTo<GpuCustomTableSubmenuGui>();
                return true;
            }
            return false;
        });
        customTableSubmenu->setValue(R_ARROW);
        this->listElement->addItem(customTableSubmenu);
    }
};

class GpuCustomTableSubmenuGui : public MiscGui {
    public:
    GpuCustomTableSubmenuGui() {
    }

    protected:
    void listUI() override {

        Result rc = hocclkIpcGetConfigValues(this->configList);  // populate config list early otherwise wont work
        if (R_FAILED(rc)) [[unlikely]] {
            FatalGui::openWithResultCode("hocclkIpcGetConfigValues", rc);
            return;
        }

        this->listElement->addItem(new CompactCategoryHeader("커스텀 테이블 (mV)"));

        ValueThresholds MgpuVmaxThresholds(800, 850);
        ValueThresholds EgpuVmaxThresholds(950, 975);

        std::vector<NamedValue> mGpuVolts = {
            NamedValue("비활성화", 2000), NamedValue("자동", 0),    NamedValue("480 mV", 480), NamedValue("485 mV", 485), NamedValue("490 mV", 490),
            NamedValue("495 mV", 495),     NamedValue("500 mV", 500), NamedValue("505 mV", 505), NamedValue("510 mV", 510), NamedValue("515 mV", 515),
            NamedValue("520 mV", 520),     NamedValue("525 mV", 525), NamedValue("530 mV", 530), NamedValue("535 mV", 535), NamedValue("540 mV", 540),
            NamedValue("545 mV", 545),     NamedValue("550 mV", 550), NamedValue("555 mV", 555), NamedValue("560 mV", 560), NamedValue("565 mV", 565),
            NamedValue("570 mV", 570),     NamedValue("575 mV", 575), NamedValue("580 mV", 580), NamedValue("585 mV", 585), NamedValue("590 mV", 590),
            NamedValue("595 mV", 595),     NamedValue("600 mV", 600), NamedValue("605 mV", 605), NamedValue("610 mV", 610), NamedValue("615 mV", 615),
            NamedValue("620 mV", 620),     NamedValue("625 mV", 625), NamedValue("630 mV", 630), NamedValue("635 mV", 635), NamedValue("640 mV", 640),
            NamedValue("645 mV", 645),     NamedValue("650 mV", 650), NamedValue("655 mV", 655), NamedValue("660 mV", 660), NamedValue("665 mV", 665),
            NamedValue("670 mV", 670),     NamedValue("675 mV", 675), NamedValue("680 mV", 680), NamedValue("685 mV", 685), NamedValue("690 mV", 690),
            NamedValue("695 mV", 695),     NamedValue("700 mV", 700), NamedValue("705 mV", 705), NamedValue("710 mV", 710), NamedValue("715 mV", 715),
            NamedValue("720 mV", 720),     NamedValue("725 mV", 725), NamedValue("730 mV", 730), NamedValue("735 mV", 735), NamedValue("740 mV", 740),
            NamedValue("745 mV", 745),     NamedValue("750 mV", 750), NamedValue("755 mV", 755), NamedValue("760 mV", 760), NamedValue("765 mV", 765),
            NamedValue("770 mV", 770),     NamedValue("775 mV", 775), NamedValue("780 mV", 780), NamedValue("785 mV", 785), NamedValue("790 mV", 790),
            NamedValue("795 mV", 795),     NamedValue("800 mV", 800), NamedValue("805 mV", 805), NamedValue("810 mV", 810), NamedValue("815 mV", 815),
            NamedValue("820 mV", 820),     NamedValue("825 mV", 825), NamedValue("830 mV", 830), NamedValue("835 mV", 835), NamedValue("840 mV", 840),
            NamedValue("845 mV", 845),     NamedValue("850 mV", 850), NamedValue("855 mV", 855), NamedValue("860 mV", 860), NamedValue("865 mV", 865),
            NamedValue("870 mV", 870),     NamedValue("875 mV", 875), NamedValue("880 mV", 880), NamedValue("885 mV", 885), NamedValue("890 mV", 890),
            NamedValue("895 mV", 895),     NamedValue("900 mV", 900), NamedValue("905 mV", 905), NamedValue("910 mV", 910), NamedValue("915 mV", 915),
            NamedValue("920 mV", 920),     NamedValue("925 mV", 925), NamedValue("930 mV", 930), NamedValue("935 mV", 935), NamedValue("940 mV", 940),
            NamedValue("945 mV", 945),     NamedValue("950 mV", 950), NamedValue("955 mV", 955), NamedValue("960 mV", 960),
        };

        std::vector<NamedValue> eGpuVolts = {
            NamedValue("비활성화", 2000), NamedValue("자동", 0),    NamedValue("675 mV", 675), NamedValue("680 mV", 680), NamedValue("685 mV", 685),
            NamedValue("690 mV", 690),     NamedValue("695 mV", 695), NamedValue("700 mV", 700), NamedValue("705 mV", 705), NamedValue("710 mV", 710),
            NamedValue("715 mV", 715),     NamedValue("720 mV", 720), NamedValue("725 mV", 725), NamedValue("730 mV", 730), NamedValue("735 mV", 735),
            NamedValue("740 mV", 740),     NamedValue("745 mV", 745), NamedValue("750 mV", 750), NamedValue("755 mV", 755), NamedValue("760 mV", 760),
            NamedValue("765 mV", 765),     NamedValue("770 mV", 770), NamedValue("775 mV", 775), NamedValue("780 mV", 780), NamedValue("785 mV", 785),
            NamedValue("790 mV", 790),     NamedValue("795 mV", 795), NamedValue("800 mV", 800), NamedValue("805 mV", 805), NamedValue("810 mV", 810),
            NamedValue("815 mV", 815),     NamedValue("820 mV", 820), NamedValue("825 mV", 825), NamedValue("830 mV", 830), NamedValue("835 mV", 835),
            NamedValue("840 mV", 840),     NamedValue("845 mV", 845), NamedValue("850 mV", 850), NamedValue("855 mV", 855), NamedValue("860 mV", 860),
            NamedValue("865 mV", 865),     NamedValue("870 mV", 870), NamedValue("875 mV", 875), NamedValue("880 mV", 880), NamedValue("885 mV", 885),
            NamedValue("890 mV", 890),     NamedValue("895 mV", 895), NamedValue("900 mV", 900), NamedValue("905 mV", 905), NamedValue("910 mV", 910),
            NamedValue("915 mV", 915),     NamedValue("920 mV", 920), NamedValue("925 mV", 925), NamedValue("930 mV", 930), NamedValue("935 mV", 935),
            NamedValue("940 mV", 940),     NamedValue("945 mV", 945), NamedValue("950 mV", 950), NamedValue("955 mV", 955), NamedValue("960 mV", 960),
            NamedValue("965 mV", 965),     NamedValue("970 mV", 970), NamedValue("975 mV", 975), NamedValue("980 mV", 980), NamedValue("985 mV", 985),
            NamedValue("990 mV", 990),     NamedValue("995 mV", 995),
        };

        std::vector<NamedValue> mGpuVolts_noAuto = {
            NamedValue("비활성화", 2000), NamedValue("480 mV", 480), NamedValue("485 mV", 485), NamedValue("490 mV", 490), NamedValue("495 mV", 495),
            NamedValue("500 mV", 500),     NamedValue("505 mV", 505), NamedValue("510 mV", 510), NamedValue("515 mV", 515), NamedValue("520 mV", 520),
            NamedValue("525 mV", 525),     NamedValue("530 mV", 530), NamedValue("535 mV", 535), NamedValue("540 mV", 540), NamedValue("545 mV", 545),
            NamedValue("550 mV", 550),     NamedValue("555 mV", 555), NamedValue("560 mV", 560), NamedValue("565 mV", 565), NamedValue("570 mV", 570),
            NamedValue("575 mV", 575),     NamedValue("580 mV", 580), NamedValue("585 mV", 585), NamedValue("590 mV", 590), NamedValue("595 mV", 595),
            NamedValue("600 mV", 600),     NamedValue("605 mV", 605), NamedValue("610 mV", 610), NamedValue("615 mV", 615), NamedValue("620 mV", 620),
            NamedValue("625 mV", 625),     NamedValue("630 mV", 630), NamedValue("635 mV", 635), NamedValue("640 mV", 640), NamedValue("645 mV", 645),
            NamedValue("650 mV", 650),     NamedValue("655 mV", 655), NamedValue("660 mV", 660), NamedValue("665 mV", 665), NamedValue("670 mV", 670),
            NamedValue("675 mV", 675),     NamedValue("680 mV", 680), NamedValue("685 mV", 685), NamedValue("690 mV", 690), NamedValue("695 mV", 695),
            NamedValue("700 mV", 700),     NamedValue("705 mV", 705), NamedValue("710 mV", 710), NamedValue("715 mV", 715), NamedValue("720 mV", 720),
            NamedValue("725 mV", 725),     NamedValue("730 mV", 730), NamedValue("735 mV", 735), NamedValue("740 mV", 740), NamedValue("745 mV", 745),
            NamedValue("750 mV", 750),     NamedValue("755 mV", 755), NamedValue("760 mV", 760), NamedValue("765 mV", 765), NamedValue("770 mV", 770),
            NamedValue("775 mV", 775),     NamedValue("780 mV", 780), NamedValue("785 mV", 785), NamedValue("790 mV", 790), NamedValue("795 mV", 795),
            NamedValue("800 mV", 800),     NamedValue("805 mV", 805), NamedValue("810 mV", 810), NamedValue("815 mV", 815), NamedValue("820 mV", 820),
            NamedValue("825 mV", 825),     NamedValue("830 mV", 830), NamedValue("835 mV", 835), NamedValue("840 mV", 840), NamedValue("845 mV", 845),
            NamedValue("850 mV", 850),     NamedValue("855 mV", 855), NamedValue("860 mV", 860), NamedValue("865 mV", 865), NamedValue("870 mV", 870),
            NamedValue("875 mV", 875),     NamedValue("880 mV", 880), NamedValue("885 mV", 885), NamedValue("890 mV", 890), NamedValue("895 mV", 895),
            NamedValue("900 mV", 900),     NamedValue("905 mV", 905), NamedValue("910 mV", 910), NamedValue("915 mV", 915), NamedValue("920 mV", 920),
            NamedValue("925 mV", 925),     NamedValue("930 mV", 930), NamedValue("935 mV", 935), NamedValue("940 mV", 940), NamedValue("945 mV", 945),
            NamedValue("950 mV", 950),     NamedValue("955 mV", 955), NamedValue("960 mV", 960),
        };

        std::vector<NamedValue> eGpuVolts_noAuto = {
            NamedValue("비활성화", 2000), NamedValue("700 mV", 700), NamedValue("705 mV", 705), NamedValue("710 mV", 710), NamedValue("715 mV", 715),
            NamedValue("720 mV", 720),     NamedValue("725 mV", 725), NamedValue("730 mV", 730), NamedValue("735 mV", 735), NamedValue("740 mV", 740),
            NamedValue("745 mV", 745),     NamedValue("750 mV", 750), NamedValue("755 mV", 755), NamedValue("760 mV", 760), NamedValue("765 mV", 765),
            NamedValue("770 mV", 770),     NamedValue("775 mV", 775), NamedValue("780 mV", 780), NamedValue("785 mV", 785), NamedValue("790 mV", 790),
            NamedValue("795 mV", 795),     NamedValue("800 mV", 800), NamedValue("805 mV", 805), NamedValue("810 mV", 810), NamedValue("815 mV", 815),
            NamedValue("820 mV", 820),     NamedValue("825 mV", 825), NamedValue("830 mV", 830), NamedValue("835 mV", 835), NamedValue("840 mV", 840),
            NamedValue("845 mV", 845),     NamedValue("850 mV", 850), NamedValue("855 mV", 855), NamedValue("860 mV", 860), NamedValue("865 mV", 865),
            NamedValue("870 mV", 870),     NamedValue("875 mV", 875), NamedValue("880 mV", 880), NamedValue("885 mV", 885), NamedValue("890 mV", 890),
            NamedValue("895 mV", 895),     NamedValue("900 mV", 900), NamedValue("905 mV", 905), NamedValue("910 mV", 910), NamedValue("915 mV", 915),
            NamedValue("920 mV", 920),     NamedValue("925 mV", 925), NamedValue("930 mV", 930), NamedValue("935 mV", 935), NamedValue("940 mV", 940),
            NamedValue("945 mV", 945),     NamedValue("950 mV", 950), NamedValue("955 mV", 955), NamedValue("960 mV", 960), NamedValue("965 mV", 965),
            NamedValue("970 mV", 970),     NamedValue("975 mV", 975), NamedValue("980 mV", 980), NamedValue("985 mV", 985), NamedValue("990 mV", 990),
            NamedValue("995 mV", 995),
        };

        if (IsMariko()) {

            tsl::elm::CustomDrawer *warningText = new tsl::elm::CustomDrawer([](tsl::gfx::Renderer *renderer, s32 x, s32 y, s32 w, s32 h) {
                renderer->drawString("\uE150 적절한 언더볼팅 없이 1305 MHz 이상", false, x + 15, y + 20, 18, tsl::warningTextColor);
                renderer->drawString("클럭을 설정할 경우, 콘솔의 성능 저하 및", false, x + 38, y + 40, 18, tsl::warningTextColor);
                renderer->drawString("치명적인 손상이 발생할 수 있습니다!", false, x + 38, y + 60, 18, tsl::warningTextColor);
            });
            warningText->setBoundaries(0, 0, tsl::cfg::FramebufferWidth, 90);
            this->listElement->addItem(warningText);

            addConfigButton(KipConfigValue_g_volt_76800, "76.8 MHz", ValueRange(0, 0, 0, "0", 1), "전압", &MgpuVmaxThresholds, {}, mGpuVolts, false,
                            true);
            addConfigButton(KipConfigValue_g_volt_153600, "153.6 MHz", ValueRange(0, 0, 0, "0", 1), "전압", &MgpuVmaxThresholds, {}, mGpuVolts,
                            false, true);
            addConfigButton(KipConfigValue_g_volt_230400, "230.4 MHz", ValueRange(0, 0, 0, "0", 1), "전압", &MgpuVmaxThresholds, {}, mGpuVolts,
                            false, true);
            addConfigButton(KipConfigValue_g_volt_307200, "307.2 MHz", ValueRange(0, 0, 0, "0", 1), "전압", &MgpuVmaxThresholds, {}, mGpuVolts,
                            false, true);
            addConfigButton(KipConfigValue_g_volt_384000, "384.0 MHz", ValueRange(0, 0, 0, "0", 1), "전압", &MgpuVmaxThresholds, {}, mGpuVolts,
                            false, true);
            addConfigButton(KipConfigValue_g_volt_460800, "460.8 MHz", ValueRange(0, 0, 0, "0", 1), "전압", &MgpuVmaxThresholds, {}, mGpuVolts,
                            false, true);
            addConfigButton(KipConfigValue_g_volt_537600, "537.6 MHz", ValueRange(0, 0, 0, "0", 1), "전압", &MgpuVmaxThresholds, {}, mGpuVolts,
                            false, true);
            addConfigButton(KipConfigValue_g_volt_614400, "614.4 MHz", ValueRange(0, 0, 0, "0", 1), "전압", &MgpuVmaxThresholds, {}, mGpuVolts,
                            false, true);
            addConfigButton(KipConfigValue_g_volt_691200, "691.2 MHz", ValueRange(0, 0, 0, "0", 1), "전압", &MgpuVmaxThresholds, {}, mGpuVolts,
                            false, true);
            addConfigButton(KipConfigValue_g_volt_768000, "768.0 MHz", ValueRange(0, 0, 0, "0", 1), "전압", &MgpuVmaxThresholds, {}, mGpuVolts,
                            false, true);
            addConfigButton(KipConfigValue_g_volt_844800, "844.8 MHz", ValueRange(0, 0, 0, "0", 1), "전압", &MgpuVmaxThresholds, {}, mGpuVolts,
                            false, true);
            addConfigButton(KipConfigValue_g_volt_921600, "921.6 MHz", ValueRange(0, 0, 0, "0", 1), "전압", &MgpuVmaxThresholds, {}, mGpuVolts,
                            false, true);
            addConfigButton(KipConfigValue_g_volt_998400, "998.4 MHz", ValueRange(0, 0, 0, "0", 1), "전압", &MgpuVmaxThresholds, {}, mGpuVolts,
                            false, true);
            if (this->configList->values[KipConfigValue_marikoGpuUV] >= GPUUVLevel_SLT) {
                addConfigButton(KipConfigValue_g_volt_1075200, "1075.2 MHz", ValueRange(0, 0, 0, "0", 1), "전압", &MgpuVmaxThresholds, {},
                                mGpuVolts, false, true);
                if (this->configList->values[KipConfigValue_marikoGpuUV] >= GPUUVLevel_HiOPT)
                    addConfigButton(KipConfigValue_g_volt_1152000, "1152.0 MHz", ValueRange(0, 0, 0, "0", 1), "전압", &MgpuVmaxThresholds, {},
                                    mGpuVolts, false, true);
                if (this->configList->values[KipConfigValue_marikoGpuUV] >= GPUUVLevel_HighUV) {
                    addConfigButton(KipConfigValue_g_volt_1228800, "1228.8 MHz", ValueRange(0, 0, 0, "0", 1), "전압", &MgpuVmaxThresholds, {},
                                    mGpuVolts, false, true);
                    addConfigButton(KipConfigValue_g_volt_1267200, "1267.2 MHz", ValueRange(0, 0, 0, "0", 1), "전압", &MgpuVmaxThresholds, {},
                                    mGpuVolts, false, true);
                    addConfigButton(KipConfigValue_g_volt_1305600, "1305.6 MHz", ValueRange(0, 0, 0, "0", 1), "전압", &MgpuVmaxThresholds, {},
                                    mGpuVolts, false, true);
                    addConfigButton(KipConfigValue_g_volt_1344000, "1344.0 MHz", ValueRange(0, 0, 0, "0", 1), "전압", &MgpuVmaxThresholds, {},
                                    mGpuVolts_noAuto, false, true);
                    addConfigButton(KipConfigValue_g_volt_1382400, "1382.4 MHz", ValueRange(0, 0, 0, "0", 1), "전압", &MgpuVmaxThresholds, {},
                                    mGpuVolts_noAuto, false, true);
                    addConfigButton(KipConfigValue_g_volt_1420800, "1420.8 MHz", ValueRange(0, 0, 0, "0", 1), "전압", &MgpuVmaxThresholds, {},
                                    mGpuVolts_noAuto, false, true);
                    addConfigButton(KipConfigValue_g_volt_1459200, "1459.2 MHz", ValueRange(0, 0, 0, "0", 1), "전압", &MgpuVmaxThresholds, {},
                                    mGpuVolts_noAuto, false, true);
                    addConfigButton(KipConfigValue_g_volt_1497600, "1497.6 MHz", ValueRange(0, 0, 0, "0", 1), "전압", &MgpuVmaxThresholds, {},
                                    mGpuVolts_noAuto, false, true);
                    addConfigButton(KipConfigValue_g_volt_1536000, "1536.0 MHz", ValueRange(0, 0, 0, "0", 1), "전압", &MgpuVmaxThresholds, {},
                                    mGpuVolts_noAuto, false, true);
                }
            }
        } else {

            tsl::elm::CustomDrawer* warningText = new tsl::elm::CustomDrawer([](tsl::gfx::Renderer *renderer, s32 x, s32 y, s32 w, s32 h) {
                renderer->drawString("\uE150 적절한 언더볼팅 없이 921 MHz 이상", false, x + 15, y + 20, 18, tsl::warningTextColor);
                renderer->drawString("클럭을 설정할 경우, 콘솔의 성능 저하 및", false, x + 38, y + 40, 18, tsl::warningTextColor);
                renderer->drawString("치명적인 손상이 발생할 수 있습니다!", false, x + 38, y + 60, 18, tsl::warningTextColor);
            });
            warningText->setBoundaries(0, 0, tsl::cfg::FramebufferWidth, 90);
            this->listElement->addItem(warningText);

            addConfigButton(KipConfigValue_g_volt_e_76800, "76.8 MHz", ValueRange(0, 0, 0, "0", 1), "전압", &EgpuVmaxThresholds, {}, eGpuVolts,
                            false, true);
            addConfigButton(KipConfigValue_g_volt_e_115200, "115.2 MHz", ValueRange(0, 0, 0, "0", 1), "전압", &EgpuVmaxThresholds, {}, eGpuVolts,
                            false, true);
            addConfigButton(KipConfigValue_g_volt_e_153600, "153.6 MHz", ValueRange(0, 0, 0, "0", 1), "전압", &EgpuVmaxThresholds, {}, eGpuVolts,
                            false, true);
            addConfigButton(KipConfigValue_g_volt_e_192000, "192.0 MHz", ValueRange(0, 0, 0, "0", 1), "전압", &EgpuVmaxThresholds, {}, eGpuVolts,
                            false, true);
            addConfigButton(KipConfigValue_g_volt_e_230400, "230.4 MHz", ValueRange(0, 0, 0, "0", 1), "전압", &EgpuVmaxThresholds, {}, eGpuVolts,
                            false, true);
            addConfigButton(KipConfigValue_g_volt_e_268800, "268.8 MHz", ValueRange(0, 0, 0, "0", 1), "전압", &EgpuVmaxThresholds, {}, eGpuVolts,
                            false, true);
            addConfigButton(KipConfigValue_g_volt_e_307200, "307.2 MHz", ValueRange(0, 0, 0, "0", 1), "전압", &EgpuVmaxThresholds, {}, eGpuVolts,
                            false, true);
            addConfigButton(KipConfigValue_g_volt_e_345600, "345.6 MHz", ValueRange(0, 0, 0, "0", 1), "전압", &EgpuVmaxThresholds, {}, eGpuVolts,
                            false, true);
            addConfigButton(KipConfigValue_g_volt_e_384000, "384.0 MHz", ValueRange(0, 0, 0, "0", 1), "전압", &EgpuVmaxThresholds, {}, eGpuVolts,
                            false, true);
            addConfigButton(KipConfigValue_g_volt_e_422400, "422.4 MHz", ValueRange(0, 0, 0, "0", 1), "전압", &EgpuVmaxThresholds, {}, eGpuVolts,
                            false, true);
            addConfigButton(KipConfigValue_g_volt_e_460800, "460.8 MHz", ValueRange(0, 0, 0, "0", 1), "전압", &EgpuVmaxThresholds, {}, eGpuVolts,
                            false, true);
            addConfigButton(KipConfigValue_g_volt_e_499200, "499.2 MHz", ValueRange(0, 0, 0, "0", 1), "전압", &EgpuVmaxThresholds, {}, eGpuVolts,
                            false, true);
            addConfigButton(KipConfigValue_g_volt_e_537600, "537.6 MHz", ValueRange(0, 0, 0, "0", 1), "전압", &EgpuVmaxThresholds, {}, eGpuVolts,
                            false, true);
            addConfigButton(KipConfigValue_g_volt_e_576000, "576.0 MHz", ValueRange(0, 0, 0, "0", 1), "전압", &EgpuVmaxThresholds, {}, eGpuVolts,
                            false, true);
            addConfigButton(KipConfigValue_g_volt_e_614400, "614.4 MHz", ValueRange(0, 0, 0, "0", 1), "전압", &EgpuVmaxThresholds, {}, eGpuVolts,
                            false, true);
            addConfigButton(KipConfigValue_g_volt_e_652800, "652.8 MHz", ValueRange(0, 0, 0, "0", 1), "전압", &EgpuVmaxThresholds, {}, eGpuVolts,
                            false, true);
            addConfigButton(KipConfigValue_g_volt_e_691200, "691.2 MHz", ValueRange(0, 0, 0, "0", 1), "전압", &EgpuVmaxThresholds, {}, eGpuVolts,
                            false, true);
            addConfigButton(KipConfigValue_g_volt_e_729600, "729.6 MHz", ValueRange(0, 0, 0, "0", 1), "전압", &EgpuVmaxThresholds, {}, eGpuVolts,
                            false, true);
            addConfigButton(KipConfigValue_g_volt_e_768000, "768.0 MHz", ValueRange(0, 0, 0, "0", 1), "전압", &EgpuVmaxThresholds, {}, eGpuVolts,
                            false, true);
            addConfigButton(KipConfigValue_g_volt_e_806400, "806.4 MHz", ValueRange(0, 0, 0, "0", 1), "전압", &EgpuVmaxThresholds, {}, eGpuVolts,
                            false, true);
            addConfigButton(KipConfigValue_g_volt_e_844800, "844.8 MHz", ValueRange(0, 0, 0, "0", 1), "전압", &EgpuVmaxThresholds, {}, eGpuVolts,
                            false, true);
            addConfigButton(KipConfigValue_g_volt_e_883200, "883.2 MHz", ValueRange(0, 0, 0, "0", 1), "전압", &EgpuVmaxThresholds, {}, eGpuVolts,
                            false, true);
            addConfigButton(KipConfigValue_g_volt_e_921600, "921.6 MHz", ValueRange(0, 0, 0, "0", 1), "전압", &EgpuVmaxThresholds, {}, eGpuVolts,
                            false, true);
            if (this->configList->values[KipConfigValue_eristaGpuUV] >= GPUUVLevel_SLT)
                addConfigButton(KipConfigValue_g_volt_e_960000, "960.0 MHz", ValueRange(0, 0, 0, "0", 1), "전압", &EgpuVmaxThresholds, {},
                                eGpuVolts, false, true);
            if (this->configList->values[KipConfigValue_eristaGpuUV] >= GPUUVLevel_HiOPT) {
                addConfigButton(KipConfigValue_g_volt_e_998400, "998.4 MHz", ValueRange(0, 0, 0, "0", 1), "전압", &EgpuVmaxThresholds, {},
                                eGpuVolts, false, true);
                addConfigButton(KipConfigValue_g_volt_e_1036800, "1036.8 MHz", ValueRange(0, 0, 0, "0", 1), "전압", &EgpuVmaxThresholds, {},
                                eGpuVolts_noAuto, false, true);
                addConfigButton(KipConfigValue_g_volt_e_1075200, "1075.2 MHz", ValueRange(0, 0, 0, "0", 1), "전압", &EgpuVmaxThresholds, {},
                                eGpuVolts_noAuto, false, true);
            }
        }
    }
};

class SocCustomTableSubmenuGui : public MiscGui {
    public:
    SocCustomTableSubmenuGui() {}

    protected:
    void listUI() override {

        Result rc = hocclkIpcGetConfigValues(this->configList);  // populate config list early otherwise wont work
        if (R_FAILED(rc)) [[unlikely]] {
            FatalGui::openWithResultCode("hocclkIpcGetConfigValues", rc);
            return;
        }

        this->listElement->addItem(new CompactCategoryHeader("SoC 전압 설정"));

        ValueThresholds voltageThresholds(1075, 1150);

        std::vector<NamedValue> socVolts = {
            NamedValue("설정 안 함", 0),

            NamedValue("637 mV", 637),   NamedValue("650 mV", 650),   NamedValue("662 mV", 662),   NamedValue("675 mV", 675),   NamedValue("687 mV", 687),
            NamedValue("700 mV", 700),   NamedValue("712 mV", 712),   NamedValue("725 mV", 725),   NamedValue("737 mV", 737),   NamedValue("750 mV", 750),
            NamedValue("762 mV", 762),   NamedValue("775 mV", 775),   NamedValue("787 mV", 787),   NamedValue("800 mV", 800),   NamedValue("812 mV", 812),
            NamedValue("825 mV", 825),   NamedValue("837 mV", 837),   NamedValue("850 mV", 850),   NamedValue("862 mV", 862),   NamedValue("875 mV", 875),
            NamedValue("887 mV", 887),   NamedValue("900 mV", 900),   NamedValue("912 mV", 912),   NamedValue("925 mV", 925),   NamedValue("937 mV", 937),
            NamedValue("950 mV", 950),   NamedValue("962 mV", 962),   NamedValue("975 mV", 975),   NamedValue("987 mV", 987),   NamedValue("1000 mV", 1000),
            NamedValue("1012 mV", 1012), NamedValue("1025 mV", 1025), NamedValue("1037 mV", 1037), NamedValue("1050 mV", 1050), NamedValue("1062 mV", 1062),
            NamedValue("1075 mV", 1075), NamedValue("1087 mV", 1087), NamedValue("1100 mV", 1100), NamedValue("1112 mV", 1112), NamedValue("1125 mV", 1125),
            NamedValue("1137 mV", 1137), NamedValue("1150 mV", 1150),
        };

        addConfigButton(KipConfigValue_g_soc_volt_1866000, "1866 MHz", ValueRange(0, 0, 0, "0", 1), "전압", &voltageThresholds, {}, socVolts, false, true);
        addConfigButton(KipConfigValue_g_soc_volt_2000000, "2000 MHz", ValueRange(0, 0, 0, "0", 1), "전압", &voltageThresholds, {}, socVolts, false, true);
        addConfigButton(KipConfigValue_g_soc_volt_2133000, "2133 MHz", ValueRange(0, 0, 0, "0", 1), "전압", &voltageThresholds, {}, socVolts, false, true);
        addConfigButton(KipConfigValue_g_soc_volt_2200000, "2200 MHz", ValueRange(0, 0, 0, "0", 1), "전압", &voltageThresholds, {}, socVolts, false, true);
        addConfigButton(KipConfigValue_g_soc_volt_2266000, "2266 MHz", ValueRange(0, 0, 0, "0", 1), "전압", &voltageThresholds, {}, socVolts, false, true);
        addConfigButton(KipConfigValue_g_soc_volt_2333000, "2333 MHz", ValueRange(0, 0, 0, "0", 1), "전압", &voltageThresholds, {}, socVolts, false, true);
        addConfigButton(KipConfigValue_g_soc_volt_2400000, "2400 MHz", ValueRange(0, 0, 0, "0", 1), "전압", &voltageThresholds, {}, socVolts, false, true);
        addConfigButton(KipConfigValue_g_soc_volt_2433000, "2433 MHz", ValueRange(0, 0, 0, "0", 1), "전압", &voltageThresholds, {}, socVolts, false, true);
        addConfigButton(KipConfigValue_g_soc_volt_2466000, "2466 MHz", ValueRange(0, 0, 0, "0", 1), "전압", &voltageThresholds, {}, socVolts, false, true);
        addConfigButton(KipConfigValue_g_soc_volt_2533000, "2533 MHz", ValueRange(0, 0, 0, "0", 1), "전압", &voltageThresholds, {}, socVolts, false, true);
        addConfigButton(KipConfigValue_g_soc_volt_2566000, "2566 MHz", ValueRange(0, 0, 0, "0", 1), "전압", &voltageThresholds, {}, socVolts, false, true);
        addConfigButton(KipConfigValue_g_soc_volt_2600000, "2600 MHz", ValueRange(0, 0, 0, "0", 1), "전압", &voltageThresholds, {}, socVolts, false, true);
        addConfigButton(KipConfigValue_g_soc_volt_2666000, "2666 MHz", ValueRange(0, 0, 0, "0", 1), "전압", &voltageThresholds, {}, socVolts, false, true);
        addConfigButton(KipConfigValue_g_soc_volt_2700000, "2700 MHz", ValueRange(0, 0, 0, "0", 1), "전압", &voltageThresholds, {}, socVolts, false, true);
        addConfigButton(KipConfigValue_g_soc_volt_2733000, "2733 MHz", ValueRange(0, 0, 0, "0", 1), "전압", &voltageThresholds, {}, socVolts, false, true);
        addConfigButton(KipConfigValue_g_soc_volt_2766000, "2766 MHz", ValueRange(0, 0, 0, "0", 1), "전압", &voltageThresholds, {}, socVolts, false, true);
        addConfigButton(KipConfigValue_g_soc_volt_2800000, "2800 MHz", ValueRange(0, 0, 0, "0", 1), "전압", &voltageThresholds, {}, socVolts, false, true);
        addConfigButton(KipConfigValue_g_soc_volt_2833000, "2833 MHz", ValueRange(0, 0, 0, "0", 1), "전압", &voltageThresholds, {}, socVolts, false, true);
        addConfigButton(KipConfigValue_g_soc_volt_2900000, "2900 MHz", ValueRange(0, 0, 0, "0", 1), "전압", &voltageThresholds, {}, socVolts, false, true);
        addConfigButton(KipConfigValue_g_soc_volt_2933000, "2933 MHz", ValueRange(0, 0, 0, "0", 1), "전압", &voltageThresholds, {}, socVolts, false, true);
        addConfigButton(KipConfigValue_g_soc_volt_3000000, "3000 MHz", ValueRange(0, 0, 0, "0", 1), "전압", &voltageThresholds, {}, socVolts, false, true);
        addConfigButton(KipConfigValue_g_soc_volt_3033000, "3033 MHz", ValueRange(0, 0, 0, "0", 1), "전압", &voltageThresholds, {}, socVolts, false, true);
        addConfigButton(KipConfigValue_g_soc_volt_3100000, "3100 MHz", ValueRange(0, 0, 0, "0", 1), "전압", &voltageThresholds, {}, socVolts, false, true);
        addConfigButton(KipConfigValue_g_soc_volt_3133000, "3133 MHz", ValueRange(0, 0, 0, "0", 1), "전압", &voltageThresholds, {}, socVolts, false, true);
        addConfigButton(KipConfigValue_g_soc_volt_3166000, "3166 MHz", ValueRange(0, 0, 0, "0", 1), "전압", &voltageThresholds, {}, socVolts, false, true);
        addConfigButton(KipConfigValue_g_soc_volt_3200000, "3200 MHz", ValueRange(0, 0, 0, "0", 1), "전압", &voltageThresholds, {}, socVolts, false, true);
    }
};

static std::string getValueDisplayText(uint64_t currentValue, const ValueRange &range, const std::vector<NamedValue> &namedValues) {
    char valueText[32];

    for (const auto &namedValue : namedValues) {
        if (currentValue == namedValue.value) {
            return namedValue.name;
        }
    }

    if (currentValue == 0) {
        snprintf(valueText, sizeof(valueText), "%s", VALUE_DEFAULT_TEXT);
    } else {
        uint64_t displayValue = currentValue / range.divisor;
        if (!range.suffix.empty()) {
            snprintf(valueText, sizeof(valueText), "%lu %s", displayValue, range.suffix.c_str());
        } else {
            snprintf(valueText, sizeof(valueText), "%lu", displayValue);
        }
    }
    return std::string(valueText);
}

void MiscGui::refresh() {
    BaseMenuGui::refresh();

    if (this->context && ++frameCounter >= 60) {
        frameCounter = 0;

        Result rc = hocclkIpcGetConfigValues(this->configList);
        if (R_FAILED(rc)) [[unlikely]] {
            FatalGui::openWithResultCode("hocclkIpcGetConfigValues", rc);
            return;
        }
        updateConfigToggles();

        // relabel when display unit changes
        RamDisplayUnit unit = (RamDisplayUnit)this->configList->values[HocClkConfigValue_RamDisplayUnit];
        constexpr HocClkConfigValue emcKeys[] = {
            KipConfigValue_marikoEmcMaxClock,
            KipConfigValue_eristaEmcMaxClock,
        };
        for (auto key : emcKeys) {
            auto it = this->configNamedValues.find(key);
            if (it != this->configNamedValues.end()) {
                for (auto &nv : it->second)
                    if (nv.name != "비활성화")
                        nv.name = formatMemClockKhzLabel(nv.value, unit);
            }
        }

        for (const auto &[configVal, button] : this->configButtons) {
            uint64_t currentValue = this->configList->values[configVal];
            const ValueRange &range = this->configRanges[configVal];

            auto namedValuesIt = this->configNamedValues.find(configVal);
            const std::vector<NamedValue> &namedValues =
                (namedValuesIt != this->configNamedValues.end()) ? namedValuesIt->second : std::vector<NamedValue>();

            char valueText[32];

            bool foundNamedValue = false;
            for (const auto &namedValue : namedValues) {
                if (currentValue == namedValue.value) {
                    snprintf(valueText, sizeof(valueText), "%s", namedValue.name.c_str());
                    foundNamedValue = true;
                    break;
                }
            }

            if (!foundNamedValue) {
                uint64_t displayValue = currentValue / range.divisor;
                if (!range.suffix.empty()) {
                    snprintf(valueText, sizeof(valueText), "%lu %s", displayValue, range.suffix.c_str());
                } else {
                    snprintf(valueText, sizeof(valueText), "%lu", displayValue);
                }
            }

            if (this->configButtonSKeys.count(configVal)) {
                button->setText(valueText);
                auto subtextIt = this->configButtonSSubtext.find(configVal);
                if (subtextIt != this->configButtonSSubtext.end())
                    button->setValue(subtextIt->second);
                else
                    button->setValue("");
            } else {
                button->setValue(valueText);
            }
        }
    }
}
