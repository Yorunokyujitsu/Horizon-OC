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
 *
 */

/* --------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <p-sam@d3vs.net>, <natinusala@gmail.com>, <m4x@m4xw.net>
 * wrote this file. As long as you retain this notice you can do whatever you
 * want with this stuff. If you meet any of us some day, and you think this
 * stuff is worth it, you can buy us a beer in return.  - The sys-clk authors
 * --------------------------------------------------------------------------
 */

#define TESLA_INIT_IMPL
#include <tesla.hpp>

#include "ui/gui/fatal_gui.h"
#include "ui/gui/main_gui.h"

extern "C" {
    // Ty to MasaGratoR for this!
    // This is done to save some space as they have no practical use in our case
    void *__real___cxa_throw(void *thrown_exception, void *pvar, void (*dest)(void *));
    void *__real__Unwind_Resume();
    void *__real___gxx_personality_v0();

    void __wrap___cxa_throw(void *thrown_exception, void *pvar, void (*dest)(void *)) {
        abort();
    }

    void __wrap__Unwind_Resume() {
        return;
    }

    void __wrap___gxx_personality_v0() {
        return;
    }
}

class AppOverlay : public tsl::Overlay {
    public:
    AppOverlay() {
    }
    ~AppOverlay() {
    }

    // virtual void initServices() override {
    //     rgltrInitialize();
    // }

    virtual void exitServices() override {
        hocclkIpcExit();
    }

    virtual std::unique_ptr<tsl::Gui> loadInitialGui() override {
        uint32_t apiVersion;
        smInitialize();

        tsl::hlp::ScopeGuard smGuard([] { smExit(); });

        if (!hocclkIpcRunning()) {
            return initially<FatalGui>("hoc-clk 모듈 실행 불가\n\n"
                                       "\n"
                                       "모듈, 오버레이가 올바르게\n\n"
                                       "설치, 활성화 되었습니까?",
                                       "");
        }

        if (R_FAILED(hocclkIpcInitialize()) || R_FAILED(hocclkIpcGetAPIVersion(&apiVersion))) {
            return initially<FatalGui>("hoc-clk 모듈 연결 불가\n\n"
                                       "\n"
                                       "모듈, 오버레이가 올바르게\n\n"
                                       "설치, 활성화 되었습니까?",
                                       "");
        }

        if (HOCCLK_IPC_API_VERSION != apiVersion) {
            return initially<FatalGui>("hoc-clk 모듈과 오버레이\n\n"
                                       "버전이 일치하지 않습니다.\n\n"
                                       "\n"
                                       "모듈과 호환되는 버전의\n\n"
                                       "오버레이로 업데이트하세요.",
                                       "");
        }

        return initially<MainGui>();
    }
};

int main(int argc, char **argv) {
    return tsl::loop<AppOverlay>(argc, argv);
}
