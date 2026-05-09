/*
 * Copyright (c) NVIDIA
 *
 * Copyright (c) Souldbminer
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


#include "gm20b.hpp"
#include "../mapping/mem_map.hpp"

namespace gm20b {
    u64 gpu_base = 0;
    #define GPU_PA 0x57000000
    #define GPU_SIZE 0x1000000
    #define GPU_TRIM_SYS_GPCPLL_CFG             0x0
    #define GPU_TRIM_SYS_GPCPLL_COEFF 0x4
    #define GPU_TRIM_SYS_GPCPLL_CFG2            0x8
    #define GPU_TRIM_SYS_GPCPLL_DVFS0           0x10
    #define GPU_TRIM_SYS_GPCPLL_DVFS1           0x14
    #define GPU_TRIM_SYS_GPCPLL_NDIV_SLOWDOWN   0x1c
    #define GPU_TRIM_SYS_SEL_VCO                0x2c
    #define GPU_TRIM_SYS_GPC2CLK_OUT            0x250
    #define GPC_BCAST(x)                (*(volatile u32 *)(gpu_base + 0x132800ul + (x)))
    #define GPU_TRIM_SYS_GPCPLL(x) (*(volatile u32 *)(gpu_base + 0x137000ul + (x)))
    #define GPC_BCAST_GPCPLL_DVFS2      0x20
    #define GPC_BCAST_NDIV_SLOWDOWN_DBG 0xa0

    #define GPCPLL_CFG_ENABLE           BIT(0)
    #define GPCPLL_CFG_IDDQ             BIT(1)
    #define GPCPLL_CFG_SYNC_MODE        BIT(2)
    #define GPCPLL_CFG_LOCK             BIT(17)

    #define GPCPLL_CFG2_SDM_DIN_MASK        0x000000FF
    #define GPCPLL_CFG2_SDM_DIN_NEW_MASK    0x007FFF00

    #define GPCPLL_DVFS0_DFS_COEFF_MASK     0x0000007F

    #define NDIV_SLOWDOWN_SLOWDOWN_USING_PLL BIT(22)
    #define NDIV_SLOWDOWN_EN_DYNRAMP         BIT(23)

    #define DYNRAMP_DONE_SYNCED             BIT(24)

    #define SEL_VCO_GPC2CLK_OUT_BIT         BIT(0)
    #define GPC2CLK_OUT_VCODIV_MASK         0x00003F00
    #define GPC2CLK_OUT_VCODIV1             0x00000100
    #define GPC2CLK_OUT_VCODIV2             0x00000200

    #define GPCPLL_DVFS2_DFS_EXT_STROBE     BIT(16)

    static inline void _gpu_mask(u32 reg, u32 mask, u32 val) {
        u32 tmp = GPU_TRIM_SYS_GPCPLL(reg);
        GPU_TRIM_SYS_GPCPLL(reg) = (tmp & ~mask) | (val & mask);
        (void)GPU_TRIM_SYS_GPCPLL(reg);
    }

    static inline void _gbc_mask(u32 reg, u32 mask, u32 val) {
        u32 tmp = GPC_BCAST(reg);
        GPC_BCAST(reg) = (tmp & ~mask) | (val & mask);
        (void)GPC_BCAST(reg);
    }

    static bool _gpu_pllg_slide(u32 new_divn) {
        u32 coeff = GPU_TRIM_SYS_GPCPLL(GPU_TRIM_SYS_GPCPLL_COEFF);
        u32 cur_divn = (coeff >> 8) & 0xFF;

        if (new_divn == cur_divn)
            return true;

        _gbc_mask(GPC_BCAST_GPCPLL_DVFS2, GPCPLL_DVFS2_DFS_EXT_STROBE, GPCPLL_DVFS2_DFS_EXT_STROBE);
        _gpu_mask(GPU_TRIM_SYS_GPCPLL_DVFS0, GPCPLL_DVFS0_DFS_COEFF_MASK, 0);
        usleep(1);
        _gbc_mask(GPC_BCAST_GPCPLL_DVFS2, GPCPLL_DVFS2_DFS_EXT_STROBE, 0);

        _gpu_mask(GPU_TRIM_SYS_GPC2CLK_OUT, GPC2CLK_OUT_VCODIV_MASK, GPC2CLK_OUT_VCODIV2);
        _gpu_mask(GPU_TRIM_SYS_GPC2CLK_OUT, GPC2CLK_OUT_VCODIV_MASK, GPC2CLK_OUT_VCODIV2);
        (void)GPU_TRIM_SYS_GPCPLL(GPU_TRIM_SYS_GPC2CLK_OUT);
        usleep(2);

        _gpu_mask(GPU_TRIM_SYS_GPCPLL_NDIV_SLOWDOWN,
                NDIV_SLOWDOWN_SLOWDOWN_USING_PLL, NDIV_SLOWDOWN_SLOWDOWN_USING_PLL);

        _gpu_mask(GPU_TRIM_SYS_GPCPLL_CFG2, GPCPLL_CFG2_SDM_DIN_NEW_MASK, 0);

        coeff = (coeff & ~(0xFF << 8)) | (new_divn << 8);
        usleep(1);
        GPU_TRIM_SYS_GPCPLL(GPU_TRIM_SYS_GPCPLL_COEFF) = coeff;

        usleep(1);
        _gpu_mask(GPU_TRIM_SYS_GPCPLL_NDIV_SLOWDOWN,
                NDIV_SLOWDOWN_EN_DYNRAMP, NDIV_SLOWDOWN_EN_DYNRAMP);

        bool success = false;
        for (u32 i = 0; i < 500; i++) {
            if (GPC_BCAST(GPC_BCAST_NDIV_SLOWDOWN_DBG) & DYNRAMP_DONE_SYNCED) {
                success = true;
                break;
            }
            usleep(1);
        }

        _gpu_mask(GPU_TRIM_SYS_GPCPLL_CFG2, GPCPLL_CFG2_SDM_DIN_MASK, 0);

        _gpu_mask(GPU_TRIM_SYS_GPCPLL_NDIV_SLOWDOWN,
                NDIV_SLOWDOWN_SLOWDOWN_USING_PLL | NDIV_SLOWDOWN_EN_DYNRAMP, 0);
        (void)GPU_TRIM_SYS_GPCPLL(GPU_TRIM_SYS_GPCPLL_NDIV_SLOWDOWN);

        usleep(2);
        _gpu_mask(GPU_TRIM_SYS_GPC2CLK_OUT, GPC2CLK_OUT_VCODIV_MASK, GPC2CLK_OUT_VCODIV1);
        _gpu_mask(GPU_TRIM_SYS_GPC2CLK_OUT, GPC2CLK_OUT_VCODIV_MASK, GPC2CLK_OUT_VCODIV1);
        (void)GPU_TRIM_SYS_GPCPLL(GPU_TRIM_SYS_GPC2CLK_OUT);

        return success;
    }

    bool setClock(u32 khz) {
        if(!gpu_base) {
            QueryMemoryMapping(&gpu_base, GPU_PA, 0x1000000);
        }
        const u32 osc = 38400000;
        u32 hz = khz * 1000;

        u32 coeff = GPU_TRIM_SYS_GPCPLL(GPU_TRIM_SYS_GPCPLL_COEFF);
        u32 divm  =  coeff        & 0xFF;
        u32 divp  = (coeff >> 16) & 0x3F;

        if (divm == 0 || divp == 0)
            return false;

        u32 new_divn = (u64)hz * divm * divp * 2 / osc;

        if (!_gpu_pllg_slide(new_divn))
            return false;

        return true;
    }
}