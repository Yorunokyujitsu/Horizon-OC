/*
 * Copyright (c) Lightos_
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

#include <stratosphere.hpp>
#include "../mtc_timing_value.hpp"
#include "timing_tables.hpp"

namespace ams::ldr::hoc::pcv::mariko {

    void GetRext() {
        if (auto r = FindRext()) {
            rext = r->rext;
            return;
        }

        /* Fallback. */
        rext = 0x1A;
    }

    void CalculateMrw2() {
        static const u8 rlMapDBI[8] = {
            6, 12, 16, 22, 28, 32, 36, 40
        };

        static const u8 wlMapSetA[8] = {
            4, 6, 8, 10, 12, 14, 16, 18
        };

        u32 rlIndex = 0;
        u32 wlIndex = 0;

        for (u32 i = 0; i < std::size(rlMapDBI); ++i) {
            if (rlMapDBI[i] == RL) {
                rlIndex = i;
                break;
            }
        }

        for (u32 i = 0; i < std::size(wlMapSetA); ++i) {
            if (wlMapSetA[i] == WL) {
                wlIndex = i;
                break;
            }
        }

        mrw2 = static_cast<u8>(((rlIndex & 0x7) | ((wlIndex & 0x7) << 3) | ((0 & 0x1) << 6)));
    }

    void CalculateTimings(double tCK_avg) {
        GetRext();

        tR2P  = CEIL((RL * 0.426) - 2.0);
        tR2W  = FLOOR(FLOOR((5.0 / tCK_avg) + ((FLOOR(48.0 / WL) - 0.478) * 3.0)) / 1.501) + RL - (C.t6_tRTW * 3) + finetRTW;
        tRTM  = FLOOR((10.0 + RL) + (3.502 / tCK_avg)) + FLOOR(7.489 / tCK_avg);
        tRATM = CEIL((tRTM - 10.0) + (RL * 0.426));

        rdv               = RL + FLOOR((5.105 / tCK_avg) + 17.017);
        qpop              = rdv - 14;
        quse_width        = CEIL(((4.897 / tCK_avg) - FLOOR(2.538 / tCK_avg)) + 3.782);
        quse              = FLOOR(RL + ((5.082 / tCK_avg) + FLOOR(2.560 / tCK_avg))) - CEIL(4.820 / tCK_avg);
        einput_duration   = FLOOR(9.936 / tCK_avg) + 5.0 + quse_width;
        einput            = quse - CEIL(9.928 / tCK_avg);
        u32 qrst_duration = FLOOR(8.399 - tCK_avg);
        u32 qrstLow       = MAX(static_cast<s32>(einput - qrst_duration - 2), static_cast<s32>(0));
        qrst              = PACK_U32(qrst_duration, qrstLow);
        ibdly             = PACK_U32_NIBBLE_HIGH_BYTE_LOW(1, quse - qrst_duration - 2.0);
        qsafe             = (einput_duration + 3) + MAX(MIN(qrstLow * rdv, qrst_duration + qrst_duration), einput);
        tW2P              = (CEIL(WL * 1.7303) * 2) - 5;
        tWTPDEN           = CEIL(((1.803 / tCK_avg) + MAX(RL + (2.694 / tCK_avg), static_cast<double>(tW2P))) + (BL / 2));
        tW2R              = FLOOR(MAX((5.020 / tCK_avg) + 1.130, WL - MAX(-CEIL(0.258 * (WL - RL)), 1.964)) * 1.964) + WL - CEIL(tWTR / tCK_avg) + finetWTR;
        tWTM              = CEIL(WL + ((7.570 / tCK_avg) + 8.753));
        tWATM             = (tWTM + (FLOOR(WL / 0.816) * 2.0)) - 4.0;

        wdv = WL;
        wsv = WL - 2;
        wev = 0xA + (WL - 14);

        u32 obdlyHigh = 3 / FLOOR(MIN(static_cast<double>(2), tCK_avg * (WL - 7)));
        u32 obdlyLow  = MAX(WL - FLOOR((126.0 / CEIL(tCK_avg + 8.601))), 0.0);
        obdly         = PACK_U32_NIBBLE_HIGH_BYTE_LOW(obdlyHigh, obdlyLow);

        pdex2rw  = CEIL((CEIL(12.335 - tCK_avg) + (7.430 / tCK_avg) - CEIL(tCK_avg * 11.361)));

        tCLKSTOP = FLOOR(MIN(8.488 / tCK_avg, 23.0)) + 8.0;

        u32 tMMRI = tRCD + (tCK_avg * 3);
        pdex2mrr  = tMMRI + 10;

        CalculateMrw2();
    }

}
