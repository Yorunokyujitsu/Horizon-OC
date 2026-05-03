/*
 * drivers/thermal/tegra_aotag.c
 *
 * TEGRA AOTAG (Always-On Thermal Alert Generator) driver.
 *
 * Copyright (c) 2014 - 2017, NVIDIA CORPORATION.  All rights reserved.
 *
 * Copyright (c) 1994, Linus Torvalds
 * 
 * Copyright (c) 2026, Souldbminer
 * 
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include "aotag.hpp"
#include "mem_map.hpp"
#include <notification.h>
#include "file_utils.hpp"

namespace aotag {
    #define PMC_BASE 0x7000E400
    #define TEGRA_FUSE_CP_REV_0_3          (3)
    #define FUSE_CP_REV                    0x190
    u64 fuseVa = 0;
    bool wasInit = false;
    inline int tegra_fuse_readl(unsigned long base, u32* value) {
        *value = *reinterpret_cast<volatile u32*>(fuseVa + base);
        return 0;
    }
    #define REG_SET(r, mask, value)	\
        ((r & ~(mask##_MASK)) | ((value<<(mask##_POS_START)) & mask##_MASK))

    #define REG_GET(r, mask) \
        ((r & mask##_MASK) >> mask##_POS_START)

    #define FUSE_TSENSOR_CALIB_CP_TS_BASE_MASK  0x1fff
    #define FUSE_TSENSOR_CALIB_FT_TS_BASE_MASK (0x1fff << 13)
    #define FUSE_TSENSOR_CALIB_FT_TS_BASE_SHIFT	  13
    #define CALIB_COEFFICIENT                     1000000LL
    #define FUSE_CACHE_OFFSET 0x800
    #define FUSE_TSENSOR_COMMON 0xA80
    #define SENSOR_CONFIG2_THERMA_SHIFT 16
    #define SENSOR_CONFIG2_THERMB_SHIFT 0


    static const struct TSensorFuse tegra_aotag_fuse = {
        .fuse_base_cp_mask = 0x3ff << 11,
        .fuse_base_cp_shift = 11,
        .fuse_base_ft_mask = (u32)0x7ff << 21,
        .fuse_base_ft_shift = 21,
        .fuse_shift_ft_mask = 0x1f << 6,
        .fuse_shift_ft_shift = 6,
        .fuse_spare_realignment = 0,
    };

    static struct TSensorConfig tegra_aotag_config = {
        .tall = 76,
        .tiddq_en = 1,
        .ten_count = 16,
        .pdiv = 8,
        .pdiv_ate = 8,
        .tsample = 9,
        .tsample_ate = 39,
    };

    static struct TSensorConfig tegra210b01_aotag_config = {
        .tall = 76,
        .tiddq_en = 1,
        .ten_count = 16,
        .pdiv = 12,
        .pdiv_ate = 6,
        .tsample = 19,
        .tsample_ate = 39,
    };

    /*
        Calculate AOTAG coeffs:
        offset_C = 5000
        halfA = low millis * 2 - offset_C * 2
        halfB = high millis * 2 - offset_C * 2

        count_A = (aotag_half_A + |thermb|) × 8192 / therma
        count_B = (aotag_half_B + |thermb|) × 8192 / therma
        therma_target = (soctherm_half_B - soctherm_half_A) × 8192 / (count_B - count_A)

        thermb_target = (soctherm_half_A - D×2) - therma_target × count_A / 8192

        alpha = therma_target x 1,000,000 / therma_raw

        beta  = thermb_target x 1,000,000 - thermb_raw × alpha
    */

    static const struct FuseCorrCoeff tegra_aotag_coeff = {
        .alpha = 1290000,
        .beta = 40500000,
    };

    static const struct FuseCorrCoeff tegra210b01_aotag_coeff = {
        .alpha = 1290000,
        .beta = 40500000,
    };

    struct aotag_sensor_info_t {
        struct TSensorConfig *config;
        const struct TSensorFuse *fuse;
        const struct FuseCorrCoeff *coeff;
        s32 therm_a;
        s32 therm_b;
    };

    struct aotag_platform_data {
        struct TSensorConfig *config;
        const struct FuseCorrCoeff *coeff;
    };

    static struct aotag_platform_data tegra210_plat_data = {
        .config = &tegra_aotag_config,
        .coeff = &tegra_aotag_coeff,
    };

    static struct aotag_platform_data tegra210b01_plat_data = {
        .config = &tegra210b01_aotag_config,
        .coeff = &tegra210b01_aotag_coeff,
    };


    struct aotag_sensor_info_t aotag_sensor_info = {
        .config = NULL,
        .fuse = NULL,
        .coeff = NULL,
        .therm_a = 0,
        .therm_b = 0,
    };

    struct aotag_sensor_info_t *info = &aotag_sensor_info;
    struct aotag_platform_data *pdata = NULL;

    u32 tegra_pmc_readl(unsigned long offset) {
        SecmonArgs args = {};
        args.X[0] = 0xF0000002;
        args.X[1] = PMC_BASE + offset;
        svcCallSecureMonitor(&args);

        if (args.X[1] == (PMC_BASE + offset)) { // if param 1 is identical read failed
            return 0;
        }

        return args.X[1];

    }
    void tegra_pmc_writel(u32 value, unsigned long offset) {
        SecmonArgs args = {};
        args.X[0] = 0xF0000002;
        args.X[1] = PMC_BASE + offset;
        args.X[2] = 0xFFFFFFFF;
        args.X[3] = (value);
        svcCallSecureMonitor(&args);
    }

    Result MapAddress(u64 &va, const u64 &physAddr, const char *name) {
        Result mapResult = QueryMemoryMapping(&va, physAddr, 0x1000);
        if (R_FAILED(mapResult)) {
            fileUtils::LogLine("[aotag] Failed to map %s! %u", name, R_DESCRIPTION(mapResult));
        }

        return mapResult;
    }

    static inline void set_bit(unsigned long nr, volatile void * addr) {
        int *m = ((int *) addr) + (nr >> 5);
        *m |= 1 << (nr & 31);
    }


    static inline void clear_bit(unsigned long nr, volatile void * addr) {
        int *m = ((int *) addr) + (nr >> 5);
        *m &= ~(1 << (nr & 31));
    }
    
    static inline s32 sign_extend32(u32 value, int index) {
        u8 shift = 31 - index;
        return (s32) (value << shift) >> shift;
    }

    static inline s64 div64_s64(s64 dividend, s64 divisor) {
        return dividend / divisor;
    }

    static s64 div64_s64_precise(s64 a, s32 b) {
        s64 r, al;

        al = a << 16;

        r = div64_s64(al * 2 + 1, 2 * b);
        return r >> 16;
    }

    void CalcTSensorCalib(const TSensorConfig *cfg, TSensorSharedCalib *shared, const FuseCorrCoeff *corr, u32 *calibration, u32 offset) {
        u32 val, calib;
        s32 actual_tsensor_ft, actual_tsensor_cp;
        s32 delta_sens, delta_temp;
        s32 mult, div;
        s16 therma, thermb;
        s64 temp;
        tegra_fuse_readl(offset + FUSE_CACHE_OFFSET, &val);

        actual_tsensor_cp = (shared->base_cp * 64) + sign_extend32(val, 12);
        val = (val & FUSE_TSENSOR_CALIB_FT_TS_BASE_MASK) >> FUSE_TSENSOR_CALIB_FT_TS_BASE_SHIFT;
        actual_tsensor_ft = (shared->base_ft * 32) + sign_extend32(val, 12);

        delta_sens = actual_tsensor_ft - actual_tsensor_cp;
        delta_temp = shared->actual_temp_ft - shared->actual_temp_cp;

        mult = cfg->pdiv * cfg->tsample_ate;
        div = cfg->tsample * cfg->pdiv_ate;

        temp = (s64)delta_temp * (1LL << 13) * mult;
        therma = div64_s64_precise(temp, (s64)delta_sens * div);

        temp = ((s64)actual_tsensor_ft * shared->actual_temp_cp) - ((s64)actual_tsensor_cp * shared->actual_temp_ft);
        thermb = div64_s64_precise(temp, delta_sens);

        temp = (s64)therma * corr->alpha;
        therma = div64_s64_precise(temp, CALIB_COEFFICIENT);

        temp = (s64)thermb * corr->alpha + corr->beta;
        thermb = div64_s64_precise(temp, CALIB_COEFFICIENT);

        calib = ((u16)therma << SENSOR_CONFIG2_THERMA_SHIFT) | ((u16)thermb << SENSOR_CONFIG2_THERMB_SHIFT);
        *calibration = calib;
    }

    #define NOMINAL_CALIB_FT 105
    #define NOMINAL_CALIB_CP  25


    void CalcSharedCal(const TSensorFuse *tfuse, TSensorSharedCalib *shared) {
        s32 shifted_cp, shifted_ft;
        u32 val;
        tegra_fuse_readl(FUSE_TSENSOR_COMMON, &val);

        shared->base_cp = (val & tfuse->fuse_base_cp_mask) >> tfuse->fuse_base_cp_shift;
        shared->base_ft = (val & tfuse->fuse_base_ft_mask) >> tfuse->fuse_base_ft_shift;

        shifted_ft = (val & tfuse->fuse_shift_ft_mask) >> tfuse->fuse_shift_ft_shift;
        shifted_ft = sign_extend32(shifted_ft, 4);

        if (tfuse->fuse_spare_realignment) {
            tegra_fuse_readl(tfuse->fuse_spare_realignment + FUSE_CACHE_OFFSET, &val);
        }

        shifted_cp = sign_extend32(val, 5);

        shared->actual_temp_cp = 2 * NOMINAL_CALIB_CP + shifted_cp;
        shared->actual_temp_ft = 2 * NOMINAL_CALIB_FT + shifted_ft;
    }

    void init(bool isMariko) {
        constexpr u64 FusePa = 0x7000F000;
        R_UNLESS(MapAddress(fuseVa, FusePa, "fuse"));

        if (isMariko) {
            // u32 major, minor, rev;
            pdata = &tegra210b01_plat_data;
            info->config = &tegra210b01_aotag_config;
            // tegra_fuse_readl(FUSE_CP_REV, &rev);
            // minor = rev & 0x1f;
            // major = (rev >> 5) & 0x3f;
            // if (major == 0 && minor < TEGRA_FUSE_CP_REV_0_3) {
            //     info->config->tsample_ate -= 1;
            // }
        } else {
            info->config = &tegra_aotag_config;
            pdata = &tegra210_plat_data;
        }

        info->fuse = &tegra_aotag_fuse;
        info->coeff = pdata->coeff;

        struct aotag_sensor_info_t *ps_info = &aotag_sensor_info;
        struct TSensorSharedCalib shared_fuses;
        u32 therm_ab;

        CalcSharedCal(ps_info->fuse, &shared_fuses);
        CalcTSensorCalib(ps_info->config, &shared_fuses,
            ps_info->coeff, &therm_ab, AOTAG_FUSE_ADDR);

        ps_info->therm_a = REG_GET(therm_ab, CONFIG2_THERM_A);
        ps_info->therm_b = REG_GET(therm_ab, CONFIG2_THERM_B);

        tegra_pmc_writel(therm_ab, PMC_TSENSOR_CONFIG2);

        struct aotag_sensor_info_t *i = info;

        unsigned long r = 0;
        r = REG_SET(r, CONFIG0_TALL, i->config->tall);
        tegra_pmc_writel(r, PMC_TSENSOR_CONFIG0);

        r = 0;
        r = REG_SET(r, CONFIG1_TEN_COUNT, i->config->ten_count);
        r = REG_SET(r, CONFIG1_TIDDQ_EN, i->config->tiddq_en);
        r = REG_SET(r, CONFIG1_TSAMPLE, (i->config->tsample - 1));
        set_bit(CONFIG1_TEMP_ENABLE_POS, &r);
        tegra_pmc_writel(r, PMC_TSENSOR_CONFIG1);

        r = 0;
        r = REG_SET(r, TSENSOR_PDIV, i->config->pdiv);
        tegra_pmc_writel(r, PMC_TSENSOR_PDIV0);

        r = 0;
        set_bit(CFG_TAG_EN_POS, &r);
        clear_bit(CFG_DISABLE_CLK_POS, &r);
        tegra_pmc_writel(r, PMC_AOTAG_CFG);
        fileUtils::LogLine("[aotag] Init complete!");
        wasInit = true;
    }
    s32 getTemp()
    {
        if(!wasInit)
            return -125;
        u32 regval = 0, abs = 0, fraction = 0, valid = 0, sign = 0;
        s32 temp = 0;
        regval = tegra_pmc_readl(PMC_TSENSOR_STATUS1);
        valid = REG_GET(regval, STATUS1_TEMP_VALID);

        if (!valid) {
            return -125;
        }
        abs = REG_GET(regval, STATUS1_TEMP_ABS);
        fraction = REG_GET(regval, STATUS1_TEMP_FRAC);
        sign = REG_GET(regval, STATUS1_TEMP_SIGN);
        temp = (abs*1000) + (fraction*500);
        if (sign)
            temp = (-1) * (temp);
        return temp;
    }

    bool isInitialized() {
        return wasInit;
    }
}