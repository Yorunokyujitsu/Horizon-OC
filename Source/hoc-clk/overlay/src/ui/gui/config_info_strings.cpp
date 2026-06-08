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

#include "config_info_strings.h"

std::vector<std::string> ConfigInfoStrings(HocClkConfigValue val, bool isMariko, bool isHoag)
{
    switch (val)
    {
        case HocClkConfigValue_PollingIntervalMs:
            return {
                "클럭 적용, 온도·전압 폴링, 로그 기록(활성화된 경우)을 수행하는 주기입니다 (밀리초 단위).",
                "값이 높을수록 설정 변경 후 반영까지 지연이 커질 수 있으며, 값이 낮을수록 시스템 모듈의 메모리 사용량이 증가할 수 있습니다.",
                "기본값: 300 ms"
            };

        case HocClkConfigValue_RamDisplayUnit:
            return {
                "RAM 주파수 값을 표시할 때 사용할 단위입니다.",
                "옵션:",
                "- MHz: 메가헤르츠 (예: 1600 MHz)",
                "- MT/s: 초당 메가트랜스퍼 (예: 3200 MT/s)",
                "- MHz, MT/s: MHz와 MT/s 모두 표시",
                "기본값: MHz"
            };

        case HocClkConfigValue_RAMVoltDisplayMode:
            return {
                "RAM 전압 값을 표시하는 방식입니다.",
                "옵션:",
                "- VDD2: VDD2 전압을 표시",
                "- VDDQ: VDDQ 전압을 표시",
                "기본값: VDD2"
            };

        case HocClkConfigValue_EnableExperimentalSettings:
            return {
                "아직 테스트 중이며 불안정하거나 정상적으로 동작하지 않을 수 있는 설정들을 표시합니다.",
                "주의해서 사용하시길 바랍니다."
            };

        case HocClkConfigValue_MarikoMiddleFreqs:
            return {
                "1228MHz 이하의 GPU 클럭에서 76.8 MHz 대신 38.4 MHz 단위의 세밀한 주파수 조정을 사용할 수 있도록 합니다.",
                "기본값: 꺼짐"
            };

        case HocClkConfigValue_LiveCpuUv:
            return {
                "재부팅 없이 CPU 언더볼트 설정을 변경할 수 있도록 합니다.",
                "기본값: 꺼짐"
            };

        case HocClkConfigValue_GPUSchedulingMethod:
            return {
                "GPU 스케줄링 오버라이드에 사용할 방식입니다.",
                "옵션:",
                "- INI: system_settings.ini를 통해 오버라이드합니다.",
                "- NV 서비스: nvservices 시스템 모듈을 통한 오버라이드 (실험적 기능).",
                "기본값: INI"
            };

        case HocClkConfigValue_MemoryFrequencyMeasurementMode:
            return {
                "RAM의 실제 주파수를 측정하는 방식입니다.",
                "옵션:",
                "- PLL: PLLMB 및 PLLM 기준으로 측정합니다 (더 정확함).",
                "- Actmon: Actmon 기준으로 측정합니다 (덜 정확함).",
                "기본값: PLL"
            };

        case HocClkConfigValue_BatteryChargeCurrent:
            return {
                "배터리로 들어가는 충전 전류를 강제로 변경합니다. 주의해서 사용하세요!",
                isHoag ? "기본값: 1664 mA" : "기본값: 2048 mA"
            };


        case HocClkConfigValue_InputCurrentLimit:
            return {
                "충전기 입력 전류의 최대값을 덮어씁니다.",
                isHoag ? "기본값: 900 mA" : "기본값: 1200 mA" 
            };

        case HocClkConfigValue_AulaDisplayColorPreset:
            return {
                "현재 디스플레이 색상 프리셋입니다.",
                "옵션:",
                "- Saturated: Vivid 기반의 매우 강한 채도.",
                "- Washed: 색이 빠진 듯한 흐린 색감",
                "- Basic: 실제에 가까운 색상 프로필",
                "- Natural: 이름과 달리 채도가 추가된 색감",
                "- Vivid: 채도가 강조된 색감",
                "기본값: 설정 안 함 (Basic)"
            };

        case HocClkConfigValue_CpuGovernorMinimumFreq:
            return {
                "CPU 거버너가 허용할 최소 주파수입니다.",
                "기본값: 612 MHz"
            };

        case HocClkConfigValue_OverwriteRefreshRate:
            return {
                "디스플레이 주사율 관련 기능의 활성화 여부를 설정합니다.",
                "활성화하면 디스플레이 주사율을 변경하고 관련 기능을 사용할 수 있습니다.",
                "이 기능은 동일한 역할을 하는 FPSLocker의 기능과 충돌합니다.",
                "기본값: 꺼짐"
            };

        case HocClkConfigValue_MaxDisplayClockH:
            return {
                "휴대 모드에서의 최대 디스플레이 클럭 주파수입니다 (Hz 단위).",
                "경고: 이 값을 변경하면 시스템 불안정이나 디스플레이 손상이 발생할 수 있습니다.",
                "기본값: 60 Hz"
            };

        case HocClkConfigValue_DisplayVoltage:
            return {
                "디스플레이 패널에 공급되는 전압입니다 (mV 단위).",
                "경고: 이 값을 변경하면 시스템이 불안정해질 수 있습니다.",
                "기본값: 1200 mV"
            };

        case HocClkConfigValue_UncappedClocks:
            if(isMariko) {
                return {
                    "활성화 시, 설정된 클럭 제한을 해제합니다.",
                    "경고: 적절한 언더볼트 없이 이를 활성화하면 기기에 손상을 줄 수 있습니다. 주의해서 사용하세요!",
                    "클럭 제한:",
                    "- 휴대 모드:",
                    "  - GPU (No UV): 614 MHz",
                    "  - GPU (SLT): 691 MHz",
                    "  - GPU (HiOPT): 768 MHz",
                    "  - GPU (HiOPT - 15mV): 844 MHz",
                    "  - GPU (High UV): 921 MHz",
                    "- USB 충전기",
                    "  - GPU (No UV): 844 MHz",
                    "  - GPU (SLT): 921 MHz",
                    "  - GPU (HiOPT): 998 MHz",
                    "  - GPU (HiOPT - 15mV): 1075 MHz",
                    "  - GPU (High UV): 1152 MHz",
                    "- 정격 충전기 / 독 모드:",
                    "  - 클럭 제한 없음",
                    "기본값: 꺼짐"
                };
            } else {
                return {
                    "활성화 시, 설정된 클럭 제한을 해제합니다.",
                    "경고: 적절한 언더볼트 없이 이를 활성화하면 기기에 손상을 줄 수 있습니다. 주의해서 사용하세요!",
                    "클럭 제한:",
                    "- 휴대 모드:",
                    "  - GPU: 460 MHz",
                    "  - CPU: 1581 MHz",
                    "- USB 충전기",
                    "  - GPU: 768 MHz",
                    "- 정격 충전기 / 독 모드:",
                    "  - 클럭 제한 없음",
                    "기본값: 꺼짐"
                };
            }

        case HocClkConfigValue_ThermalThrottle:
            return {
                "활성화 시 임계값 도달 후 기본 클럭으로 복원됩니다.",
                "기본값: 켜짐",
            };

        case HocClkConfigValue_ThermalThrottleThreshold:
            return {
                "Thermal Throttle가 활성화된 상태에서, 기본 클럭으로 복원되기 위한 온도 임계값입니다 (°C 단위).",
                "기본값: 70°C"
            };

        case KipConfigValue_emcDvbShift:
            return {
                "각 단계는 SoC 전압 테이블에 25mV를 추가하거나 감소시킵니다.",
                "콘솔은 SoC Speedo 값에 따라 등급이 구분됩니다. 구간은 다음과 같습니다:",
                " - Speedo 1487-1598: 브래킷 0",
                " - Speedo 1598-1709: 브래킷 1",
                " - Speedo 1709-1820: 브래킷 2",
                "기본값: 0"
            };

        case KipConfigValue_marikoSocVmax:
            return {
                "DVB로 조정된 테이블이 사용할 수 있는 최대 SoC 전압입니다.",
                "기본값: 설정 안 함"
            };

        case KipConfigValue_hpMode:
            return {
                "활성화 시, RAM의 절전 모드를 비활성화합니다. 대기 시간이 대폭 개선됩니다.",
                "기본값: 꺼짐"
            };

        case KipConfigValue_commonEmcMemVolt:
            return {
                "RAM VDD2 전압입니다.",
                "이 값을 높인다고 해서 최대 주파수가 상승하지는 않지만, 타이밍 감소에 도움을 줄 수 있습니다.",
                "RAM 언더볼팅은 무의미하며 오히려 성능과 안정성을 떨어뜨릴 수 있습니다.",
                "기본값: 1175 mV"
            };

        case KipConfigValue_marikoEmcVddqVolt:
            return {
                "RAM VDDQ 전압입니다.",
                "이 값을 높이는 것이 도움이 될 수도 있으나, 일반적으로는 기본값으로도 충분합니다.",
                "RAM 언더볼팅은 무의미하며 성능과 안정성을 저해할 수 있습니다.",
                "기본값: 600 mV"
            };

        case KipConfigValue_stepMode:
            return {
                "RAM 클럭이 조절되는 단계값입니다.",
                "옵션 (예시 포함):",
                " - 66 MHz: 66 MHz 단위로 변경 (예: 1600, 1666, 1733 등)",
                " - 100 MHz: 100 MHz 단위로 변경 (예: 1600, 1700, 1800 등)",
                " - 133 MHz: 66 MHz 단위로 변경 (예: 1600, 1733, 1866 등)",
                " - JEDEC:",
                "   - 1600, 1866, 1996, 2133, 2400, 2666, 2933, 3200 MHz 주파수가 사용됩니다.",
                "선택한 스텝 모드와 관계없이 항상 최대 RAM 클럭을 사용할 수 있으나, 중간 단계의 주파수들은 선택한 스텝 모드에 의해 제한됩니다.",
                "이 설정은 성능에 직접적인 영향을 주지 않으며, 어떤 옵션을 선택할지는 주로 개인 취향에 따릅니다.",
                "Horizon OS의 특정 제한으로 인해 33 MHz 스텝 모드는 불가능합니다.",
                "기본값: 66 MHz",
            };

        case KipConfigValue_marikoEmcMaxClock:
            return {
                "사용 가능한 최대 RAM 주파수입니다.",
                "높은 주파수는 시스템 불안정을 유발할 수 있으므로, 값을 점진적으로 올리며 안정성 테스트를 진행하세요.",
                "기본값: 2133 MHz"
            };

        case KipConfigValue_eristaEmcMaxClock:
            return {
                "해당 슬롯에서 사용되는 RAM 주파수입니다. 높은 주파수는 시스템 불안정을 유발할 수 있으므로, 값을 점진적으로 올리며 안정성 테스트를 진행하세요.",
                "기본값: 비활성화 (1600 MHz)"
            };

        case KipConfigValue_t1_tRCD:
            return {
                "RAS-to-CAS 지연 (t1/tRCD)",
                "기본값: 0"
            };

        case KipConfigValue_t2_tRP:
            return {
                "Row 프리차지 시간 (t2/tRP)",
                "기본값: 0"
            };

        case KipConfigValue_t3_tRAS:
            return {
                "Row 액티브 시간 (t3/tRAS)",
                "기본값: 0"
            };

        case KipConfigValue_t4_tRRD:
            return {
                "Row 리프레시 시간 (t4/tRRD)",
                "기본값: 0"
            };

        case KipConfigValue_t5_tRFC:
            return {
                "리프레시 사이클 시간 (t5/tRFC)",
                "기본값: 0"
            };

        case KipConfigValue_t6_tRTW:
            return {
                "읽기→쓰기 지연 (t6High/tRTW - 높은 브래킷)",
                "기본값: 0"
            };

        case KipConfigValue_t7_tWTR:
            return {
                "쓰기→읽기 지연 (t7High/tWTR - 높은 브래킷)",
                "기본값: 0"
            };

        case KipConfigValue_t8_tREFI:
            return {
                "리프레시 명령 간격 (t8/tREFI)",
                "기본값: 0"
            };

        case KipConfigValue_timingEmcTbreak:
            return {
                "t6 및 t7 값이 낮은 브래킷과 높은 브래킷으로 나뉘는 기준 주파수입니다.",
                "예시:",
                "Tbreak가 1866 MHz로 설정되어 있고, t6Low가 4, t6High가 2로 설정된 경우",
                "- 1866 MHz 미만에서 t6 값이 4로 적용",
                "- 1866 MHz 초과에서 t6 값이 2로 적용",
                "기본값: 비활성화"
            };

        case KipConfigValue_low_t6_tRTW:
            return {
                "읽기→쓰기 지연 (t6Low/tRTW - 낮은 브래킷)",
                "기본값: 0"
            };

        case KipConfigValue_low_t7_tWTR:
            return {
                "쓰기→읽기 지연 (t7Low/tWTR - 낮은 브래킷)",
                "기본값: 0"
            };
        case KipConfigValue_t2_tRP_cap:
            return {
                "1333WL이 사용될 때 t2에 적용되는 상한값입니다.",
                "기본값으로도 대부분의 RAM에서 충분하지만, 일부 RAM은 더 낮은 값이 필요할 수 있습니다.",
                "기본값: 2"
            };

        case KipConfigValue_t6_tRTW_fine_tune:
            return {
                "t6의 원시 계산 값을 미세 조정합니다.",
                "기본값: 0"
            };

        case KipConfigValue_t7_tWTR_fine_tune:
            return {
                "t7의 원시 계산 값을 미세 조정합니다.",
                "기본값: 0"
            };

        case KipConfigValue_write_latency_1333:
        case KipConfigValue_write_latency_1600:
        case KipConfigValue_write_latency_1866:
        case KipConfigValue_write_latency_2133:
        case KipConfigValue_read_latency_1333:
        case KipConfigValue_read_latency_1600:
        case KipConfigValue_read_latency_1866:
        case KipConfigValue_read_latency_2133:
            return {
                "지연 브래킷 설정입니다.",
                "예시:",
                "1333을 2000 MHz, 1600을 2500 MHz, 1866을 2766 MHz, 2133을 2933 MHz로 설정한 경우",
                "- 2000 MHz 미만에서는 1333이 사용되고, 2033~2500 MHz에서는 1600, 2533~2766 MHz에서는 1866, 2800~2933 MHz에서는 2133이 사용됩니다.",
                "이 값들 중 일부는 생략해도 정상 동작합니다. (예를 들어 1333을 -로 설정하면, 2000 MHz 미만에서도 1600 지연 시간이 사용됩니다.)",
                "이 설정들을 모두 생략하면 지연 시간은 다음과 같이 자동 계산됩니다:",
                "1633-1866 MHz→1866 WRL",
                "1900+ MHz - 2133 WRL",
                "이 설정들은 읽기 및 쓰기 지연 시간 모두에 적용되며, 필요에 따라 브래킷 값을 서로 조합해서 사용할 수 있습니다.",
                "기본값: ⋯"
            };
        case KipConfigValue_marikoCpuUVLow:
            return {
                "tBreak 이전에 사용되는 CPU 언더볼트 레벨입니다.",
                "기본값: 0"
            };

        case KipConfigValue_marikoCpuUVHigh:
            return {
                "tBreak 이후에 사용되는 CPU 언더볼트 레벨입니다.",
                "기본값: 0"
            };

        case KipConfigValue_tableConf:
            return {
                "현재 사용 중인 언더볼트 테이블입니다. tBreak 기준점은 다음과 같습니다:",
                "1581 MHz tBreak 및 1683 MHz tBreak 테이블은 각각의 tBreak 기준을 사용합니다.",
                "그 외의 다른 테이블들은 1581 MHz를 tBreak 기준으로 사용합니다.",
                "\"기본\" 테이블은 1963 MHz를 초과하는 주파수를 포함하지 않으므로 언더볼트가 제대로 적용되지 않을 수 있습니다."
            };

        case KipConfigValue_marikoCpuLowVmin:
            return {
                "tBreak 이전에 사용되는 CPU 최소 전압 값입니다.",
                "기본값: 620 mV"
            };

        case KipConfigValue_marikoCpuHighVmin:
            return {
                "tBreak 이전에 사용되는 CPU 최소 전압 값입니다.",
                "기본값: 750 mV"
            };


        case KipConfigValue_marikoCpuMaxVolt:
            return {
                "CPU가 사용할 수 있는 최대 전압입니다.",
                "이 설정은 주의해서 변경하세요!",
                "기본값: 1120 mV"
            };

        case KipConfigValue_marikoCpuMaxClock:
            return {
                "사용 가능한 최대 CPU 클럭입니다.",
                "기본값: 1963 MHz"
            };

        case KipConfigValue_marikoCpuBoostClock:
            return {
                "\"부스트 모드\"에서 CPU에 사용되는 클럭입니다.",
                "기본값: 1963 MHz"
            };

        case KipConfigValue_eristaCpuUV:
            return {
                "CPU 언더볼트 레벨입니다.",
                "기본값: 0"
            };

        case KipConfigValue_eristaCpuUnlock:
            return {
                "안전하지 않은 CPU 클럭의 잠금을 해제합니다.",
                "기본값: 꺼짐"
            };

        case KipConfigValue_eristaCpuVmin:
            return {
                "최소 CPU 전압입니다.",
                "기본값: 825 mV"
            };

        case KipConfigValue_eristaCpuMaxVolt:
            return {
                "최대 CPU 전압입니다.",
                "기본값: 1235 mV"
            };

        case HocClkConfigValue_EristaMaxCpuClock:
            return {
                "사용 가능한 최대 CPU 클럭입니다.",
                "기본값: 1785 MHz"
            };

        case KipConfigValue_eristaCpuBoostClock:
            return {
                "\"부스트 모드\"에서 CPU에 사용되는 클럭입니다.",
                "기본값: 1785 MHz"
            };
        case HocClkConfigValue_AutoRAMCPUOverclock:
            return {
                "활성화 시 RAM 클럭이 설정된 임계값 이상일 때, 증가한 전압 요구사항을 충족하기 위해 CPU 클럭을 설정된 오버클럭 주파수로 자동 조정합니다.",
                "기본값: ON"
            };

        case HocClkConfigValue_AutoRamCpuCpuOCFreq:
            return {
                "자동 CPU 오버클럭이 활성화되고 RAM 임계값에 도달했을 때 적용할 CPU 클럭(MHz)입니다.",
                "기본값: 1683 MHz"
            };

        case HocClkConfigValue_AutoRamCpuRamOCThreshold:
            return {
                "자동 CPU 오버클럭이 활성화되는 RAM 클럭 임계값(MHz)입니다.",
                "기본값: 2133MHz"
            };

        case HocClkConfigValue_OverwriteBoostMode:
            return {
                "활성화하면 각 프로필이 부스트 모드 설정을 오버라이드(강제 지정)할 수 있습니다.",
                "기본값: 꺼짐"
            };

        case KipConfigValue_marikoGpuUV:
            return {
                "GPU 언더볼트 레벨입니다.",
                "옵션:",
                " - 설정 안 함: 언더볼트를 적용하지 않고 HOS 기본 전압 사용",
                " - SLT 테이블: NVIDIA 커스텀 SLT 테이블",
                " - HiOPT: L4T 커스텀 HiOPT 테이블",
                " - HiOPT - 15 mV: 15 mV 오프셋이 적용된 L4T 커스텀 HiOPT 테이블",
                " - High UV: 가장 높은 수준의 언더볼트 테이블",
                "기본값: HiOPT"
            };

        case KipConfigValue_marikoGpuVmin:
            return {
                "최소 GPU 전압입니다.",
                "참고: DVFS 모드가 PCV 제어 우회로 설정된 경우, RAM 클럭 변경 시 DVFS에 의해 이 값이 변경될 수 있습니다.",
                "기본값: 610 mV"
            };

        case KipConfigValue_marikoGpuVmax:
            return {
                "최대 GPU 전압입니다.",
                "기본값: 800 mV"
            };
        
        case HocClkConfigValue_DVFSMode:
            return {
                "GPU DVFS에 사용되는 모드입니다.",
                "RAM 클럭 변경으로 인해 요구 사양이 높아질 때 GPU 최소 전압을 조정합니다.",
                "옵션:",
                "- 비활성화: 비활성화됨",
                "- PCV 제어 우회: 오버라이드를 위해 PCV를 하이재킹함",
                "기본값: PCV 제어 우회"
            };

        case HocClkConfigValue_DVFSOffset:
            return {
                "PCV 제어 우회 모드에서 RAM 클럭 변경으로 인해 요구 사양이 높아질 때, GPU 최소 전압에 더하거나 뺄 오프셋 값입니다.",
                "기본값: 0 mV (비활성화)"
            };

        case KipConfigValue_eristaGpuUV:
            return {
                "GPU 언더볼트 레벨입니다.",
                "옵션:",
                " - 설정 안 함: 언더볼트를 적용하지 않고 HOS 기본 전압 사용",
                " - SLT 테이블: NVIDIA 커스텀 SLT 테이블",
                " - HiOPT: L4T 커스텀 HiOPT 테이블",
                "기본값: 설정 안 함"
            };

        case KipConfigValue_eristaGpuVmin:
            return {
                "최소 GPU 전압입니다.",
                "기본값: 810 mV (Erista는 5mV 대신 6.5mV 단위로 조정되므로 실제로는 812mV가 적용됨)"
            };

        case KipConfigValue_commonGpuVoltOffset:
            return {
                "모든 자동 GPU 전압에 더하거나 뺄 오프셋 값입니다.",
                "기본값: 0 mV (비활성화)"
            };

        case HocClkConfigValue_GPUScheduling:
            return {
                "GPU 클럭에 사용할 스케줄링 방식입니다.",
                "옵션:",
                "- 설정 안 함: 기존 스케줄링 모드를 오버라이드하지 않음",
                "- 비활성화: GPU 스케줄링 비활성화, 최대 GPU 부하 99.7%",
                "- 활성화: GPU 스케줄링 활성화, 최대 GPU 부하 96.5%",
                "기본값: 설정 안 함"
            };
        case KipConfigValue_marikoGpuBootVolt:
            return {
                "부팅 시점 및 온도가 20°C 미만일 때 GPU에 공급되는 전압입니다 (mV 단위).",
                "경고: 이 값을 변경하면 시스템이 불안정해질 수 있습니다.",
                "기본값: 800 mV"
            };
        default:
            return {};
    }
}