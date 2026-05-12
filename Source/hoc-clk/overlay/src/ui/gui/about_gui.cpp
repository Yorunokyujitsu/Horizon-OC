/*
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
 */


#include "about_gui.h"
#include "../format.h"
#include <tesla.hpp>
#include <string>
#include "cat.h"
#include "ult_ext.h"

// tsl::elm::ListItem* custRevItem = NULL;
tsl::elm::ListItem* kipVersionItem = NULL;
tsl::elm::ListItem* SpeedoItem = NULL;
tsl::elm::ListItem* IddqItem = NULL;
tsl::elm::ListItem* DramModule = NULL;
tsl::elm::ListItem* sysdockStatusItem = NULL;
tsl::elm::ListItem* saltyNXStatusItem = NULL;
tsl::elm::ListItem* RETROStatusItem = NULL;
tsl::elm::ListItem* waferCordsItem = NULL;
tsl::elm::ListItem* ramVoltItem = NULL;
tsl::elm::ListItem* eristaPLLXItem = NULL;
tsl::elm::ListItem* dispVoltItem = NULL;
tsl::elm::ListItem* ramBWItemAll = NULL;
tsl::elm::ListItem* ramBWItemCpu = NULL;
tsl::elm::ListItem* ramBWItemGpu = NULL;
tsl::elm::ListItem* ramBWItemMax = NULL;
tsl::elm::ListItem* bqtempitem = NULL;
tsl::elm::ListItem* aotagTempItem = NULL;
tsl::elm::ListItem* cTypeItem = NULL;

ImageElement* CatImage = NULL;
HideableCategoryHeader* CatHeader = NULL;
HideableCustomDrawer* CatSpacer = NULL;
int lightosClickCount = 0;

AboutGui::AboutGui()
{
    memset(strings, 0, sizeof(strings));
}

AboutGui::~AboutGui()
{
}

void AboutGui::listUI()
{
    BaseMenuGui::refresh();

    if (!this->context)
        return;

    this->listElement->addItem(
        new tsl::elm::CategoryHeader("전압")
    );

    ramVoltItem =
        new tsl::elm::ListItem("RAM 전압");

    if(IsMariko()) {
        this->listElement->addItem(ramVoltItem);
    }
    dispVoltItem =
        new tsl::elm::ListItem("표시 전압");
    this->listElement->addItem(dispVoltItem);

    this->listElement->addItem(
        new tsl::elm::CategoryHeader("온도")
    );
    eristaPLLXItem =
        new tsl::elm::ListItem("PLLX 온도");
    if(this->context->temps[HocClkThermalSensor_AO] > 0) { // Only show if the value is valid (not -126, which means not patched)
        this->listElement->addItem(eristaPLLXItem);
    }

    aotagTempItem =
        new tsl::elm::ListItem("AOTAG 온도");
    this->listElement->addItem(aotagTempItem);

    bqtempitem =
        new tsl::elm::ListItem("BQ24193 온도");
    this->listElement->addItem(bqtempitem);

    this->listElement->addItem(
        new tsl::elm::CategoryHeader("RAM 대역폭")
    );

    ramBWItemMax =
        new tsl::elm::ListItem("피크");
    this->listElement->addItem(ramBWItemMax);

    ramBWItemAll =
        new tsl::elm::ListItem("전체");
    this->listElement->addItem(ramBWItemAll);

    ramBWItemCpu =
        new tsl::elm::ListItem("CPU");
    this->listElement->addItem(ramBWItemCpu);

    ramBWItemGpu =
        new tsl::elm::ListItem("GPU");
    this->listElement->addItem(ramBWItemGpu);


    this->listElement->addItem(
        new tsl::elm::CategoryHeader("하드웨어 정보")
    );

    cTypeItem =
        new tsl::elm::ListItem("기기");
    this->listElement->addItem(cTypeItem);

    SpeedoItem =
        new tsl::elm::ListItem("Speedo");
    this->listElement->addItem(SpeedoItem);

    IddqItem =
        new tsl::elm::ListItem("IDDQ");
    this->listElement->addItem(IddqItem);

    DramModule =
        new tsl::elm::ListItem("RAM 모듈");
    this->listElement->addItem(DramModule);

    waferCordsItem =
        new tsl::elm::ListItem("웨이퍼 위치");
    this->listElement->addItem(waferCordsItem);

    if(IsHoag()) {
        RETROStatusItem =
            new tsl::elm::ListItem("주사율");
        this->listElement->addItem(RETROStatusItem);
    }

    this->listElement->addItem(
        new tsl::elm::CategoryHeader("소프트웨어 정보")
    );

    // custRevItem = new tsl::elm::ListItem("CUST revision:");
    // this->listElement->addItem(custRevItem);

    kipVersionItem = new tsl::elm::ListItem("KIP 버전");
    this->listElement->addItem(kipVersionItem);

    if(!IsHoag()) {
        sysdockStatusItem =
            new tsl::elm::ListItem("sys-dock");
        this->listElement->addItem(sysdockStatusItem);
    }

    saltyNXStatusItem =
        new tsl::elm::ListItem("SaltyNX");
    this->listElement->addItem(saltyNXStatusItem);

    this->listElement->addItem(new tsl::elm::CategoryHeader("크레딧"));
        tsl::elm::CustomDrawer* devCredits = new tsl::elm::CustomDrawer([](tsl::gfx::Renderer *renderer, s32 x, s32 y, s32 w, s32 h) {
            renderer->drawString("개발: Souldbminer, Lightos_", false, x + 20, y + 30, 18, tsl::style::color::ColorText);
            renderer->drawString("기여: Dom, Blaise25", false, x + 20, y + 70, 18, tsl::style::color::ColorText);
            renderer->drawString("번역: Yorunokyujitsu", false, x + 20, y + 50, 18, tsl::style::color::ColorText);
        });
        devCredits->setBoundaries(0, 0, tsl::cfg::FramebufferWidth, 90);
        this->listElement->addItem(devCredits);

    /*this->listElement->addItem(
        new tsl::elm::CategoryHeader("Developers")
    );

    this->listElement->addItem(
        new tsl::elm::ListItem("Souldbminer")
    );

    // Create special clickable item for Lightos
    auto lightosItem = new tsl::elm::ListItem("Lightos_");
    lightosItem->setClickListener([this](u64 keys) -> bool {
        if (keys & HidNpadButton_A) {
            lightosClickCount++;
            if (lightosClickCount >= 10) {
                if (CatImage != NULL) CatImage->setVisible(true);
                if (CatHeader != NULL) CatHeader->setVisible(true);
                if (CatSpacer != NULL) CatSpacer->setVisible(true);
            }
            return true;
        }
        return false;
    });
    this->listElement->addItem(lightosItem);

    // ---- Contributors ----
    this->listElement->addItem(
        new tsl::elm::CategoryHeader("Contributors")
    );

    this->listElement->addItem(
        new tsl::elm::ListItem("Dom")
    );

    this->listElement->addItem(
        new tsl::elm::ListItem("Blaise25")
    );

    this->listElement->addItem(
        new tsl::elm::ListItem("tetetete-ctrl")
    );

    this->listElement->addItem(
        new tsl::elm::ListItem("B3711")
    );

    this->listElement->addItem(
        new tsl::elm::ListItem("TDRR")
    );

    this->listElement->addItem(
        new tsl::elm::ListItem("MasaGratoR")
    );

    // ---- Testers ----
    this->listElement->addItem(
        new tsl::elm::CategoryHeader("Testers")
    );

    this->listElement->addItem(
        new tsl::elm::ListItem("Samybigio2011")
    );

    this->listElement->addItem(
        new tsl::elm::ListItem("arcdelta")
    );

    this->listElement->addItem(
        new tsl::elm::ListItem("Miki1305")
    );

    this->listElement->addItem(
        new tsl::elm::ListItem("Happy")
    );

    this->listElement->addItem(
        new tsl::elm::ListItem("Winnerboi77")
    );

    this->listElement->addItem(
        new tsl::elm::ListItem("Blaise25")
    );

    this->listElement->addItem(
        new tsl::elm::ListItem("WE1ZARD")
    );

    this->listElement->addItem(
        new tsl::elm::ListItem("Alvise")
    );

    this->listElement->addItem(
        new tsl::elm::ListItem("agjeococh")
    );

    this->listElement->addItem(
        new tsl::elm::ListItem("Xenshen")
    );

    this->listElement->addItem(
        new tsl::elm::ListItem("Frost")
    );

    // ---- Special Thanks ----
    this->listElement->addItem(
        new tsl::elm::CategoryHeader("Special Thanks")
    );

    this->listElement->addItem(
        new tsl::elm::ListItem("SciresM - Atmosphere CFW")
    );

    this->listElement->addItem(
        new tsl::elm::ListItem("KazushiMe - Switch OC Suite")
    );

    this->listElement->addItem(
        new tsl::elm::ListItem("hanai3Bi - Switch OC Suite & EOS")
    );

    this->listElement->addItem(
        new tsl::elm::ListItem("NaGaa95 - L4T-OC-Kernel")
    );

    this->listElement->addItem(
        new tsl::elm::ListItem("RetroNX - sys-clk")
    );

    this->listElement->addItem(
        new tsl::elm::ListItem("ppkantorski - Ultrahand")
    );

    this->listElement->addItem(
        new tsl::elm::ListItem("CtCaer - Hekate, L4T and Proper Timings")
    );*/

    // Create cat elements but hide them initially
    CatHeader = new HideableCategoryHeader("Cat");
    CatHeader->setVisible(false);
    this->listElement->addItem(CatHeader);

    CatImage = new ImageElement(CAT_DATA, CAT_WIDTH, CAT_HEIGHT);
    CatImage->setVisible(false);
    this->listElement->addItem(CatImage);

    CatSpacer = new HideableCustomDrawer(75);
    CatSpacer->setVisible(false);
    this->listElement->addItem(CatSpacer);
}

std::string AboutGui::formatRamModule() {
    switch (this->context->dramID) {
        case 0: return "HB-MGCH 4GB";
        case 4: return "HM-MGCH 6GB";
        case 7: return "HM-MGXX 8GB";

        case 1: return "NLE 4GB";
        case 2: return "WT:C 4GB";

        case 3:
        case 5 ... 6: return "NEE 4GB";

        case 8:
        case 12: return "AM-MGCJ 4GB";
        case 9:
        case 13: return "AM-MGCJ 8GB";

        case 10:
        case 14: return "NME 4GB";

        case 11:
        case 15: return "WT:E 4GB";

        case 17:
        case 19:
        case 24: return "AA-MGCL 4GB";

        case 18:
        case 23:
        case 28: return "AA-MGCL 8GB";

        case 20 ... 22: return "AB-MGCL 4GB";

        case 25 ... 27: return "WT:F 4GB";

        case 29 ... 31: return "x267 4GB";

        case 32 ... 34: return "WT:B 4GB";

        default: return "알 수 없음";
    }
}

void AboutGui::update()
{
    BaseMenuGui::update();
}

void AboutGui::refresh()
{
    BaseMenuGui::refresh();

    if (!this->context)
        return;
    // Format strings once per refresh
    sprintf(strings[0], "%u%u%u", this->context->speedos[HocClkSpeedo_CPU], this->context->speedos[HocClkSpeedo_GPU], this->context->speedos[HocClkSpeedo_SOC]);
    // This is how hekate does it
    sprintf(strings[1], "%u%u%u", this->context->iddq[HocClkSpeedo_CPU], this->context->iddq[HocClkSpeedo_GPU], this->context->iddq[HocClkSpeedo_SOC]);
    SpeedoItem->setValue(strings[0]);
    IddqItem->setValue(strings[1]);
    DramModule->setValue(formatRamModule());

    // custRevItem->setValue(std::to_string(this->context->custRev));

    kipVersionItem->setValue(std::to_string((this->context->kipVersion / 100) % 10) + "." + std::to_string((this->context->kipVersion / 10) % 10) + "." + std::to_string( this->context->kipVersion % 10) + " (Cust Rev " + std::to_string(this->context->custRev) + ")");
    if(!IsHoag())
        sysdockStatusItem->setValue(this->context->isSysDockInstalled ? "설치됨" : "설치 안 됨");

    saltyNXStatusItem->setValue(this->context->isSaltyNXInstalled ? "설치됨" : "설치 안 됨");

    if(IsHoag())
        RETROStatusItem->setValue(this->context->isUsingRetroSuper ? "설치됨" : "설치 안 됨");

    sprintf(strings[2], "X: %dY: %d", this->context->waferX, this->context->waferY);
    waferCordsItem->setValue(strings[2]);

    s32 millis = context->temps[HocClkThermalSensor_PLLX];
    sprintf(strings[3], "%u.%u °C", millis / 1000U, (millis % 1000U) / 100U);
    eristaPLLXItem->setValue(strings[3]);

    millis = context->temps[HocClkThermalSensor_AO];
    if(millis > 0) {
        sprintf(strings[11], "%u.%u °C", millis / 1000U, (millis % 1000U) / 100U);
    } else if (millis == -125) {
        sprintf(strings[11], "유효하지 않음");
    } else if (millis == -126) {
        sprintf(strings[11], "패치되지 않음");
    }
    aotagTempItem->setValue(strings[11]);

    sprintf(strings[4], "%u.%u / %u mV", context->voltages[HocClkVoltage_EMCVDD2] / 1000U, (context->voltages[HocClkVoltage_EMCVDD2] % 1000U) / 100U, context->voltages[HocClkVoltage_EMCVDDQ] / 1000);
    ramVoltItem->setValue(strings[4]);

    sprintf(strings[5], "%u.%u mV", context->voltages[HocClkVoltage_Display] / 1000U, (context->voltages[HocClkVoltage_Display] % 1000U) / 100U);
    dispVoltItem->setValue(strings[5]);

    sprintf(strings[6], "%u MB/s", context->partLoad[HocClkPartLoad_RamBWAll]);
    ramBWItemAll->setValue(strings[6]);

    sprintf(strings[7], "%u MB/s", context->partLoad[HocClkPartLoad_RamBWCpu]);
    ramBWItemCpu->setValue(strings[7]);

    sprintf(strings[8], "%u MB/s", context->partLoad[HocClkPartLoad_RamBWGpu]);
    ramBWItemGpu->setValue(strings[8]);

    sprintf(strings[9], "%u MB/s", context->partLoad[HocClkPartLoad_RamBWPeak]);
    ramBWItemMax->setValue(strings[9]);

    switch(context->temps[HocClkThermalSensor_BQ24193]) {
        case BQ24193Temp_Normal:
            strcpy(strings[10], "정상");
            break;
        case BQ24193Temp_Warm:
            strcpy(strings[10], "온도 상승");
            break;
        case BQ24193Temp_Hot:
            strcpy(strings[10], "고온");
            break;
        case BQ24193Temp_Overheat:
            strcpy(strings[10], "과열");
            break;
        default:
            strcpy(strings[10], "알 수 없음");
    }

    bqtempitem->setValue(strings[10]);

    cTypeItem->setValue(hocClkFormatConsoleType(this->context->consoleType, true));

}
