/*------------------------------------------------------------------------------
 * MediaTek Inc. (C) 2018. All rights reserved.
 *
 * Copyright Statement:
 *
 * This software/firmware and related documentation ("MediaTek Software") are
 * protected under relevant copyright laws. The information contained herein is
 * confidential and proprietary to MediaTek Inc. and/or its licensors. Without
 * the prior written permission of MediaTek inc. and/or its licensors, any
 * reproduction, modification, use or disclosure of MediaTek Software, and
 * information contained herein, in whole or in part, shall be strictly
 * prohibited.
 *
 * BY OPENING THIS FILE, RECEIVER HEREBY UNEQUIVOCALLY ACKNOWLEDGES AND AGREES
 * THAT THE SOFTWARE/FIRMWARE AND ITS DOCUMENTATIONS ("MEDIATEK SOFTWARE")
 * RECEIVED FROM MEDIATEK AND/OR ITS REPRESENTATIVES ARE PROVIDED TO RECEIVER
 * ON AN "AS-IS" BASIS ONLY. MEDIATEK EXPRESSLY DISCLAIMS ANY AND ALL
 * WARRANTIES, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE OR
 * NONINFRINGEMENT. NEITHER DOES MEDIATEK PROVIDE ANY WARRANTY WHATSOEVER WITH
 * RESPECT TO THE SOFTWARE OF ANY THIRD PARTY WHICH MAY BE USED BY,
 * INCORPORATED IN, OR SUPPLIED WITH THE MEDIATEK SOFTWARE, AND RECEIVER AGREES
 * TO LOOK ONLY TO SUCH THIRD PARTY FOR ANY WARRANTY CLAIM RELATING THERETO.
 * RECEIVER EXPRESSLY ACKNOWLEDGES THAT IT IS RECEIVER'S SOLE RESPONSIBILITY TO
 * OBTAIN FROM ANY THIRD PARTY ALL PROPER LICENSES CONTAINED IN MEDIATEK
 * SOFTWARE. MEDIATEK SHALL ALSO NOT BE RESPONSIBLE FOR ANY MEDIATEK SOFTWARE
 * RELEASES MADE TO RECEIVER'S SPECIFICATION OR TO CONFORM TO A PARTICULAR
 * STANDARD OR OPEN FORUM. RECEIVER'S SOLE AND EXCLUSIVE REMEDY AND MEDIATEK'S
 * ENTIRE AND CUMULATIVE LIABILITY WITH RESPECT TO THE MEDIATEK SOFTWARE
 * RELEASED HEREUNDER WILL BE, AT MEDIATEK'S OPTION, TO REVISE OR REPLACE THE
 * MEDIATEK SOFTWARE AT ISSUE, OR REFUND ANY SOFTWARE LICENSE FEES OR SERVICE
 * CHARGE PAID BY RECEIVER TO MEDIATEK FOR SUCH MEDIATEK SOFTWARE AT ISSUE.
 *
 * The following software/firmware and/or related documentation ("MediaTek
 * Software") have been modified by MediaTek Inc. All revisions are subject to
 * any receiver's applicable license agreements with MediaTek Inc.
 *----------------------------------------------------------------------------*/

#define SCA_TOOL_VERSION            "Chakra SCA V1.3.1 "
////////////////////////////////////////////////////////////////////////////////
// DRAM memory map
// //
// // Every Module Memory Mapping need 4 define
// // and check code in "msAPI_Memory_DumpMemoryMap"
// // 1. XXX_AVAILABLE : For get available memory start address
// // 2. XXX_ADR       : Real Address with Alignment
// // 3. XXX_GAP_CHK   : For Check Memory Gap, for avoid memory waste
// // 4. XXX_LEN       : For the Memory size of this Module usage
// ////////////////////////////////////////////////////////////////////////////////
#define MIU_DRAM_LEN                0x0040000000
#define MIU_DRAM_LEN0               0x0040000000
#define MIU_DRAM_LEN1               0x0000000000
#define MIU_DRAM_LEN2               0x0000000000
#define MIU_DRAM_LEN3               0x0000000000
#define ENABLE_MIU_1                0
#define ENABLE_MIU_2                0
#define ENABLE_MIU_3                0
#define MIU_INTERVAL                0x0080000000
#define MIU_INTERVAL2               0x0000000000
#define MIU_INTERVAL3               0x0000000000
#define MIU0_MMAP_BOUNDARY                0x0000000000
#define MIU1_MMAP_BOUNDARY                0x0000000000
#define MIU2_MMAP_BOUNDARY                0x0000000000
#define MIU3_MMAP_BOUNDARY                0x0000000000
#define SW_UPGRADE                0x000B827000
////////////////////////////////////////////////////////////////////////////////
//MIU SETTING
//////////////////////////////////////////////////////////////////////////////////
#define MIU0_GROUP_SELMIU           0000:0000:0000:0000:0000:0000
#define MIU0_GROUP_PRIORITY         1:0:2:3
#define MIU1_GROUP_SELMIU           4F6A:3FF0:0000:0000:0000:FE0F
#define MIU1_GROUP_PRIORITY         1:0:2:3
////////////////////////////////////////////////////////////////////////////////
//MEMORY TYPE
//////////////////////////////////////////////////////////////////////////////////
#define MIU0                        (0x0000)
#define MIU1                        (0x0001 << 0)
#define MIU2                        (0x0001 << 1)
#define MIU3                        (0x0001 << 2)
#define SW                          (0x0000)
#define HW                          (0x0001 << 3)
#define SW_HW                       (0x0001 << 4)
#define UNCACHED                    (0x0000)
#define WRITE_THROUGH               (0x0001 << 5)
#define WRITE_COMBINING             (0x0001 << 6)
#define WRITE_PROTECT               (0x0001 << 7)
#define WRITE_BACK                  (0x0001 << 8)
#define CO_BUF_L0                   (0x0000)
#define CO_BUF_L1                   (0x0001 << 9)
#define CO_BUF_L2                   (0x0001 << 10)
#define CO_BUF_L3                   (0x0001 << 11)
#define CO_BUF_L4                   (0x0001 << 12)
#define CO_BUF_L5                   (0x0001 << 13)
#define CO_BUF_L6                   (0x0001 << 14)
#define CO_BUF_L7                   (0x0001 << 15)
#define CO_BUF_L8                   (0x0001 << 16)
#define CO_BUF_L9                   (0x0001 << 17)
#define CO_BUF_L10                  (0x0001 << 18)
#define CO_BUF_L11                  (0x0001 << 19)
#define MIU0_LAYER_COUNT            (3)
#define MIU1_LAYER_COUNT            (0)
#define MIU2_LAYER_COUNT            (0)
#define MIU3_LAYER_COUNT            (0)
#define TYPE_NONE                   (0x0000 << 10)
#define CMA                         (0x0001 << 10)
//MIU_0_START
/* PM_OFFSET   */
#define PM_OFFSET_LAYER                        0
#define PM_OFFSET_AVAILABLE                    0x0000000000
#define PM_OFFSET_ADR                          0x0000000000  //Alignment 0x00000
#define PM_OFFSET_GAP_CHK                      0x0000000000
#define PM_OFFSET_LEN                          0x0000200000
#define PM_OFFSET_MEMORY_TYPE                  (MIU0 | SW | CO_BUF_L0 | UNCACHED | TYPE_NONE)
#define PM_OFFSET_CMA_HID                      0

/* MI_KERNEL_POOL1   */
#define MI_KERNEL_POOL1_LAYER                  0
#define MI_KERNEL_POOL1_AVAILABLE              0x0000200000
#define MI_KERNEL_POOL1_ADR                    0x0000200000  //Alignment 0x100000
#define MI_KERNEL_POOL1_GAP_CHK                0x0000000000
#define MI_KERNEL_POOL1_LEN                    0x000AB00000
#define MI_KERNEL_POOL1_MEMORY_TYPE            (MIU0 | SW | CO_BUF_L0 | UNCACHED | TYPE_NONE)
#define MI_KERNEL_POOL1_CMA_HID                0

/* MI_KERNEL1_RANGE0   */
#define MI_KERNEL1_RANGE0_LAYER                1
#define MI_KERNEL1_RANGE0_AVAILABLE            0x0000200000
#define MI_KERNEL1_RANGE0_ADR                  0x0000200000  //Alignment 0x00000
#define MI_KERNEL1_RANGE0_GAP_CHK              0x0000000000
#define MI_KERNEL1_RANGE0_LEN                  0x0007200000
#define MI_KERNEL1_RANGE0_MEMORY_TYPE          (MIU0 | SW | CO_BUF_L1 | UNCACHED | TYPE_NONE)
#define MI_KERNEL1_RANGE0_CMA_HID              0

/* MI_MBOOT_MEM_USAGE   */
#define MI_MBOOT_MEM_USAGE_LAYER               1
#define MI_MBOOT_MEM_USAGE_AVAILABLE           0x0007400000
#define MI_MBOOT_MEM_USAGE_ADR                 0x0007400000  //Alignment 0x01000
#define MI_MBOOT_MEM_USAGE_GAP_CHK             0x0000000000
#define MI_MBOOT_MEM_USAGE_LEN                 0x0002400000
#define MI_MBOOT_MEM_USAGE_MEMORY_TYPE         (MIU0 | SW | CO_BUF_L1 | UNCACHED | TYPE_NONE)
#define MI_MBOOT_MEM_USAGE_CMA_HID             0

/* MI_JPD_OUT   */
#define MI_JPD_OUT_LAYER                       1
#define MI_JPD_OUT_AVAILABLE                   0x0009800000
#define MI_JPD_OUT_ADR                         0x0009800000  //Alignment 0x01000
#define MI_JPD_OUT_GAP_CHK                     0x0000000000
#define MI_JPD_OUT_LEN                         0x0000400000
#define MI_JPD_OUT_MEMORY_TYPE                 (MIU0 | SW | CO_BUF_L1 | UNCACHED | TYPE_NONE)
#define MI_JPD_OUT_CMA_HID                     0

/* MI_JPD_READ_BUF   */
#define MI_JPD_READ_BUF_LAYER                  1
#define MI_JPD_READ_BUF_AVAILABLE              0x0009C00000
#define MI_JPD_READ_BUF_ADR                    0x0009C00000  //Alignment 0x01000
#define MI_JPD_READ_BUF_GAP_CHK                0x0000000000
#define MI_JPD_READ_BUF_LEN                    0x0000050000
#define MI_JPD_READ_BUF_MEMORY_TYPE            (MIU0 | SW | CO_BUF_L1 | UNCACHED | TYPE_NONE)
#define MI_JPD_READ_BUF_CMA_HID                0

/* MI_JPD_INTER_BUF   */
#define MI_JPD_INTER_BUF_LAYER                 1
#define MI_JPD_INTER_BUF_AVAILABLE             0x0009C50000
#define MI_JPD_INTER_BUF_ADR                   0x0009C50000  //Alignment 0x01000
#define MI_JPD_INTER_BUF_GAP_CHK               0x0000000000
#define MI_JPD_INTER_BUF_LEN                   0x0000500000
#define MI_JPD_INTER_BUF_MEMORY_TYPE           (MIU0 | SW | CO_BUF_L1 | UNCACHED | TYPE_NONE)
#define MI_JPD_INTER_BUF_CMA_HID               0

/* EMAC_MEM   */
#define EMAC_MEM_LAYER                         0
#define EMAC_MEM_AVAILABLE                     0x000AD00000
#define EMAC_MEM_ADR                           0x000AD00000  //Alignment 0x00000
#define EMAC_MEM_GAP_CHK                       0x0000000000
#define EMAC_MEM_LEN                           0x0000100000
#define EMAC_MEM_MEMORY_TYPE                   (MIU0 | SW | CO_BUF_L0 | UNCACHED | TYPE_NONE)
#define EMAC_MEM_CMA_HID                       0

/* MI_BOOTLOGO_BUF   */
#define MI_BOOTLOGO_BUF_LAYER                  0
#define MI_BOOTLOGO_BUF_AVAILABLE              0x000AE00000
#define MI_BOOTLOGO_BUF_ADR                    0x000AE00000  //Alignment 0x01000
#define MI_BOOTLOGO_BUF_GAP_CHK                0x0000000000
#define MI_BOOTLOGO_BUF_LEN                    0x0000400000
#define MI_BOOTLOGO_BUF_MEMORY_TYPE            (MIU0 | SW | CO_BUF_L0 | UNCACHED | TYPE_NONE)
#define MI_BOOTLOGO_BUF_CMA_HID                0

/* MI_KERMODULE_COBUF0   */
#define MI_KERMODULE_COBUF0_LAYER              0
#define MI_KERMODULE_COBUF0_AVAILABLE          0x000B200000
#define MI_KERMODULE_COBUF0_ADR                0x000B200000  //Alignment 0x01000
#define MI_KERMODULE_COBUF0_GAP_CHK            0x0000000000
#define MI_KERMODULE_COBUF0_LEN                0x0003263000
#define MI_KERMODULE_COBUF0_MEMORY_TYPE        (MIU0 | SW | CO_BUF_L0 | UNCACHED | TYPE_NONE)
#define MI_KERMODULE_COBUF0_CMA_HID            0

/* MI_TSP_FW_BUF   */
#define MI_TSP_FW_BUF_LAYER                    1
#define MI_TSP_FW_BUF_AVAILABLE                0x000B200000
#define MI_TSP_FW_BUF_ADR                      0x000B200000  //Alignment 0x01000
#define MI_TSP_FW_BUF_GAP_CHK                  0x0000000000
#define MI_TSP_FW_BUF_LEN                      0x0000007000
#define MI_TSP_FW_BUF_MEMORY_TYPE              (MIU0 | SW | CO_BUF_L1 | UNCACHED | TYPE_NONE)
#define MI_TSP_FW_BUF_CMA_HID                  0

/* MI_TSP_VQ_BUF   */
#define MI_TSP_VQ_BUF_LAYER                    1
#define MI_TSP_VQ_BUF_AVAILABLE                0x000B207000
#define MI_TSP_VQ_BUF_ADR                      0x000B207000  //Alignment 0x01000
#define MI_TSP_VQ_BUF_GAP_CHK                  0x0000000000
#define MI_TSP_VQ_BUF_LEN                      0x0000020000
#define MI_TSP_VQ_BUF_MEMORY_TYPE              (MIU0 | SW | CO_BUF_L1 | UNCACHED | TYPE_NONE)
#define MI_TSP_VQ_BUF_CMA_HID                  0

/* MI_CACHED_POOL   */
#define MI_CACHED_POOL_LAYER                   1
#define MI_CACHED_POOL_AVAILABLE               0x000B227000
#define MI_CACHED_POOL_ADR                     0x000B227000  //Alignment 0x01000
#define MI_CACHED_POOL_GAP_CHK                 0x0000000000
#define MI_CACHED_POOL_LEN                     0x0000080000
#define MI_CACHED_POOL_MEMORY_TYPE             (MIU0 | SW | CO_BUF_L1 | WRITE_BACK | TYPE_NONE)
#define MI_CACHED_POOL_CMA_HID                 0

/* MI_NON_CACHED_POOL   */
#define MI_NON_CACHED_POOL_LAYER               1
#define MI_NON_CACHED_POOL_AVAILABLE           0x000B2A7000
#define MI_NON_CACHED_POOL_ADR                 0x000B2A7000  //Alignment 0x01000
#define MI_NON_CACHED_POOL_GAP_CHK             0x0000000000
#define MI_NON_CACHED_POOL_LEN                 0x0000180000
#define MI_NON_CACHED_POOL_MEMORY_TYPE         (MIU0 | SW | CO_BUF_L1 | UNCACHED | TYPE_NONE)
#define MI_NON_CACHED_POOL_CMA_HID             0

/* MI_KER_CACHED_POOL   */
#define MI_KER_CACHED_POOL_LAYER               1
#define MI_KER_CACHED_POOL_AVAILABLE           0x000B427000
#define MI_KER_CACHED_POOL_ADR                 0x000B427000  //Alignment 0x01000
#define MI_KER_CACHED_POOL_GAP_CHK             0x0000000000
#define MI_KER_CACHED_POOL_LEN                 0x0000010000
#define MI_KER_CACHED_POOL_MEMORY_TYPE         (MIU0 | SW | CO_BUF_L1 | WRITE_BACK | TYPE_NONE)
#define MI_KER_CACHED_POOL_CMA_HID             0

/* MI_KER_NON_CACHED_POOL   */
#define MI_KER_NON_CACHED_POOL_LAYER           1
#define MI_KER_NON_CACHED_POOL_AVAILABLE       0x000B437000
#define MI_KER_NON_CACHED_POOL_ADR             0x000B437000  //Alignment 0x01000
#define MI_KER_NON_CACHED_POOL_GAP_CHK         0x0000000000
#define MI_KER_NON_CACHED_POOL_LEN             0x0000100000
#define MI_KER_NON_CACHED_POOL_MEMORY_TYPE     (MIU0 | SW | CO_BUF_L1 | UNCACHED | TYPE_NONE)
#define MI_KER_NON_CACHED_POOL_CMA_HID         0

/* MI_MAD_ADV_BUF   */
#define MI_MAD_ADV_BUF_LAYER                   1
#define MI_MAD_ADV_BUF_AVAILABLE               0x000B537000
#define MI_MAD_ADV_BUF_ADR                     0x000B537000  //Alignment 0x01000
#define MI_MAD_ADV_BUF_GAP_CHK                 0x0000000000
#define MI_MAD_ADV_BUF_LEN                     0x0001AF0000
#define MI_MAD_ADV_BUF_MEMORY_TYPE             (MIU0 | SW | CO_BUF_L1 | UNCACHED | TYPE_NONE)
#define MI_MAD_ADV_BUF_CMA_HID                 0

/* MI_MAD_SE_BUF   */
#define MI_MAD_SE_BUF_LAYER                    1
#define MI_MAD_SE_BUF_AVAILABLE                0x000D027000
#define MI_MAD_SE_BUF_ADR                      0x000D027000  //Alignment 0x00000
#define MI_MAD_SE_BUF_GAP_CHK                  0x0000000000
#define MI_MAD_SE_BUF_LEN                      0x0000000000
#define MI_MAD_SE_BUF_MEMORY_TYPE              (MIU0 | SW | CO_BUF_L1 | UNCACHED | TYPE_NONE)
#define MI_MAD_SE_BUF_CMA_HID                  0

/* MI_MAD_DEC_BUF   */
#define MI_MAD_DEC_BUF_LAYER                   1
#define MI_MAD_DEC_BUF_AVAILABLE               0x000D027000
#define MI_MAD_DEC_BUF_ADR                     0x000D027000  //Alignment 0x01000
#define MI_MAD_DEC_BUF_GAP_CHK                 0x0000000000
#define MI_MAD_DEC_BUF_LEN                     0x0000000000
#define MI_MAD_DEC_BUF_MEMORY_TYPE             (MIU0 | SW | CO_BUF_L1 | UNCACHED | TYPE_NONE)
#define MI_MAD_DEC_BUF_CMA_HID                 0

/* MI_AUDIO_CPU_1   */
#define MI_AUDIO_CPU_1_LAYER                   1
#define MI_AUDIO_CPU_1_AVAILABLE               0x000D027000
#define MI_AUDIO_CPU_1_ADR                     0x000D027000  //Alignment 0x01000
#define MI_AUDIO_CPU_1_GAP_CHK                 0x0000000000
#define MI_AUDIO_CPU_1_LEN                     0x0000400000
#define MI_AUDIO_CPU_1_MEMORY_TYPE             (MIU0 | SW | CO_BUF_L1 | WRITE_BACK | TYPE_NONE)
#define MI_AUDIO_CPU_1_CMA_HID                 0

/* MI_AUDIO_CPU_2   */
#define MI_AUDIO_CPU_2_LAYER                   1
#define MI_AUDIO_CPU_2_AVAILABLE               0x000D427000
#define MI_AUDIO_CPU_2_ADR                     0x000D427000  //Alignment 0x01000
#define MI_AUDIO_CPU_2_GAP_CHK                 0x0000000000
#define MI_AUDIO_CPU_2_LEN                     0x0000400000
#define MI_AUDIO_CPU_2_MEMORY_TYPE             (MIU0 | SW | CO_BUF_L1 | WRITE_BACK | TYPE_NONE)
#define MI_AUDIO_CPU_2_CMA_HID                 0

/* MI_MENULOAD_BUF   */
#define MI_MENULOAD_BUF_LAYER                  1
#define MI_MENULOAD_BUF_AVAILABLE              0x000D827000
#define MI_MENULOAD_BUF_ADR                    0x000D827000  //Alignment 0x01000
#define MI_MENULOAD_BUF_GAP_CHK                0x0000000000
#define MI_MENULOAD_BUF_LEN                    0x0000020000
#define MI_MENULOAD_BUF_MEMORY_TYPE            (MIU0 | SW | CO_BUF_L1 | UNCACHED | TYPE_NONE)
#define MI_MENULOAD_BUF_CMA_HID                0

/* MI_PM51_VAR_MEM   */
#define MI_PM51_VAR_MEM_LAYER                  1
#define MI_PM51_VAR_MEM_AVAILABLE              0x000D847000
#define MI_PM51_VAR_MEM_ADR                    0x000D847000  //Alignment 0x01000
#define MI_PM51_VAR_MEM_GAP_CHK                0x0000000000
#define MI_PM51_VAR_MEM_LEN                    0x0000001000
#define MI_PM51_VAR_MEM_MEMORY_TYPE            (MIU0 | SW | CO_BUF_L1 | UNCACHED | TYPE_NONE)
#define MI_PM51_VAR_MEM_CMA_HID                0

/* MI_PM51_MEM   */
#define MI_PM51_MEM_LAYER                      1
#define MI_PM51_MEM_AVAILABLE                  0x000D848000
#define MI_PM51_MEM_ADR                        0x000D850000  //Alignment 0x10000
#define MI_PM51_MEM_GAP_CHK                    0x0000008000
#define MI_PM51_MEM_LEN                        0x0000010000
#define MI_PM51_MEM_MEMORY_TYPE                (MIU0 | SW | CO_BUF_L1 | UNCACHED | TYPE_NONE)
#define MI_PM51_MEM_CMA_HID                    0

/* MI_GOP_REGDMA_BASE   */
#define MI_GOP_REGDMA_BASE_LAYER               1
#define MI_GOP_REGDMA_BASE_AVAILABLE           0x000D860000
#define MI_GOP_REGDMA_BASE_ADR                 0x000D860000  //Alignment 0x04000
#define MI_GOP_REGDMA_BASE_GAP_CHK             0x0000000000
#define MI_GOP_REGDMA_BASE_LEN                 0x0000002000
#define MI_GOP_REGDMA_BASE_MEMORY_TYPE         (MIU0 | SW | CO_BUF_L1 | UNCACHED | TYPE_NONE)
#define MI_GOP_REGDMA_BASE_CMA_HID             0

/* MI_MCU_DS   */
#define MI_MCU_DS_LAYER                        1
#define MI_MCU_DS_AVAILABLE                    0x000D862000
#define MI_MCU_DS_ADR                          0x000D862000  //Alignment 0x01000
#define MI_MCU_DS_GAP_CHK                      0x0000000000
#define MI_MCU_DS_LEN                          0x0000120000
#define MI_MCU_DS_MEMORY_TYPE                  (MIU0 | SW | CO_BUF_L1 | UNCACHED | TYPE_NONE)
#define MI_MCU_DS_CMA_HID                      0

/* MI_VDEC_AEON   */
#define MI_VDEC_AEON_LAYER                     1
#define MI_VDEC_AEON_AVAILABLE                 0x000D982000
#define MI_VDEC_AEON_ADR                       0x000D990000  //Alignment 0x10000
#define MI_VDEC_AEON_GAP_CHK                   0x000000E000
#define MI_VDEC_AEON_LEN                       0x0000440000
#define MI_VDEC_AEON_MEMORY_TYPE               (MIU0 | SW | CO_BUF_L1 | UNCACHED | TYPE_NONE)
#define MI_VDEC_AEON_CMA_HID                   0

/* MI_VDEC_CC_RAW_DATA   */
#define MI_VDEC_CC_RAW_DATA_LAYER              1
#define MI_VDEC_CC_RAW_DATA_AVAILABLE          0x000DDD0000
#define MI_VDEC_CC_RAW_DATA_ADR                0x000DDD0000  //Alignment 0x01000
#define MI_VDEC_CC_RAW_DATA_GAP_CHK            0x0000000000
#define MI_VDEC_CC_RAW_DATA_LEN                0x000001A000
#define MI_VDEC_CC_RAW_DATA_MEMORY_TYPE        (MIU0 | SW | CO_BUF_L1 | UNCACHED | TYPE_NONE)
#define MI_VDEC_CC_RAW_DATA_CMA_HID            0

/* MI_VDEC_SHARE_MEM   */
#define MI_VDEC_SHARE_MEM_LAYER                1
#define MI_VDEC_SHARE_MEM_AVAILABLE            0x000DDEA000
#define MI_VDEC_SHARE_MEM_ADR                  0x000DDEA000  //Alignment 0x01000
#define MI_VDEC_SHARE_MEM_GAP_CHK              0x0000000000
#define MI_VDEC_SHARE_MEM_LEN                  0x0000060000
#define MI_VDEC_SHARE_MEM_MEMORY_TYPE          (MIU0 | SW | CO_BUF_L1 | UNCACHED | TYPE_NONE)
#define MI_VDEC_SHARE_MEM_CMA_HID              0

/* MI_DISP_PQ_MEMORY   */
#define MI_DISP_PQ_MEMORY_LAYER                1
#define MI_DISP_PQ_MEMORY_AVAILABLE            0x000DE4A000
#define MI_DISP_PQ_MEMORY_ADR                  0x000DE4A000  //Alignment 0x01000
#define MI_DISP_PQ_MEMORY_GAP_CHK              0x0000000000
#define MI_DISP_PQ_MEMORY_LEN                  0x0000007000
#define MI_DISP_PQ_MEMORY_MEMORY_TYPE          (MIU0 | SW | CO_BUF_L1 | UNCACHED | TYPE_NONE)
#define MI_DISP_PQ_MEMORY_CMA_HID              0

/* MI_GE_VQCMD_BUF   */
#define MI_GE_VQCMD_BUF_LAYER                  1
#define MI_GE_VQCMD_BUF_AVAILABLE              0x000DE51000
#define MI_GE_VQCMD_BUF_ADR                    0x000DE51000  //Alignment 0x01000
#define MI_GE_VQCMD_BUF_GAP_CHK                0x0000000000
#define MI_GE_VQCMD_BUF_LEN                    0x0000080000
#define MI_GE_VQCMD_BUF_MEMORY_TYPE            (MIU0 | SW | CO_BUF_L1 | UNCACHED | TYPE_NONE)
#define MI_GE_VQCMD_BUF_CMA_HID                0

/* MI_TSP_SECT_BUF   */
#define MI_TSP_SECT_BUF_LAYER                  1
#define MI_TSP_SECT_BUF_AVAILABLE              0x000DED1000
#define MI_TSP_SECT_BUF_ADR                    0x000DED1000  //Alignment 0x01000
#define MI_TSP_SECT_BUF_GAP_CHK                0x0000000000
#define MI_TSP_SECT_BUF_LEN                    0x0000400000
#define MI_TSP_SECT_BUF_MEMORY_TYPE            (MIU0 | SW | CO_BUF_L1 | UNCACHED | TYPE_NONE)
#define MI_TSP_SECT_BUF_CMA_HID                0

/* MI_TSP_SVQ_BUF   */
#define MI_TSP_SVQ_BUF_LAYER                   1
#define MI_TSP_SVQ_BUF_AVAILABLE               0x000E2D1000
#define MI_TSP_SVQ_BUF_ADR                     0x000E2D1000  //Alignment 0x01000
#define MI_TSP_SVQ_BUF_GAP_CHK                 0x0000000000
#define MI_TSP_SVQ_BUF_LEN                     0x00000C0000
#define MI_TSP_SVQ_BUF_MEMORY_TYPE             (MIU0 | SW | CO_BUF_L1 | UNCACHED | TYPE_NONE)
#define MI_TSP_SVQ_BUF_CMA_HID                 0

/* MI_DISP_AUTO_UPLOAD   */
#define MI_DISP_AUTO_UPLOAD_LAYER              1
#define MI_DISP_AUTO_UPLOAD_AVAILABLE          0x000E391000
#define MI_DISP_AUTO_UPLOAD_ADR                0x000E391000  //Alignment 0x01000
#define MI_DISP_AUTO_UPLOAD_GAP_CHK            0x0000000000
#define MI_DISP_AUTO_UPLOAD_LEN                0x0000001000
#define MI_DISP_AUTO_UPLOAD_MEMORY_TYPE        (MIU0 | SW | CO_BUF_L1 | UNCACHED | TYPE_NONE)
#define MI_DISP_AUTO_UPLOAD_CMA_HID            0

/* MI_DISP_GUCD_DMA   */
#define MI_DISP_GUCD_DMA_LAYER                 1
#define MI_DISP_GUCD_DMA_AVAILABLE             0x000E392000
#define MI_DISP_GUCD_DMA_ADR                   0x000E392000  //Alignment 0x01000
#define MI_DISP_GUCD_DMA_GAP_CHK               0x0000000000
#define MI_DISP_GUCD_DMA_LEN                   0x000000C000
#define MI_DISP_GUCD_DMA_MEMORY_TYPE           (MIU0 | SW | CO_BUF_L1 | UNCACHED | TYPE_NONE)
#define MI_DISP_GUCD_DMA_CMA_HID               0

/* MI_DISP_HDR_HDMI_SHM   */
#define MI_DISP_HDR_HDMI_SHM_LAYER             1
#define MI_DISP_HDR_HDMI_SHM_AVAILABLE         0x000E39E000
#define MI_DISP_HDR_HDMI_SHM_ADR               0x000E39E000  //Alignment 0x01000
#define MI_DISP_HDR_HDMI_SHM_GAP_CHK           0x0000000000
#define MI_DISP_HDR_HDMI_SHM_LEN               0x0000004000
#define MI_DISP_HDR_HDMI_SHM_MEMORY_TYPE       (MIU0 | SW | CO_BUF_L1 | UNCACHED | TYPE_NONE)
#define MI_DISP_HDR_HDMI_SHM_CMA_HID           0

/* MI_DISP_AUTO_DOWNLOAD   */
#define MI_DISP_AUTO_DOWNLOAD_LAYER            1
#define MI_DISP_AUTO_DOWNLOAD_AVAILABLE        0x000E3A2000
#define MI_DISP_AUTO_DOWNLOAD_ADR              0x000E3A2000  //Alignment 0x01000
#define MI_DISP_AUTO_DOWNLOAD_GAP_CHK          0x0000000000
#define MI_DISP_AUTO_DOWNLOAD_LEN              0x00000C1000
#define MI_DISP_AUTO_DOWNLOAD_MEMORY_TYPE      (MIU0 | SW | CO_BUF_L1 | UNCACHED | TYPE_NONE)
#define MI_DISP_AUTO_DOWNLOAD_CMA_HID          0

/* MI_GOP_GWIN_RB   */
#define MI_GOP_GWIN_RB_LAYER                   0
#define MI_GOP_GWIN_RB_AVAILABLE               0x000E463000
#define MI_GOP_GWIN_RB_ADR                     0x000E463000  //Alignment 0x01000
#define MI_GOP_GWIN_RB_GAP_CHK                 0x0000000000
#define MI_GOP_GWIN_RB_LEN                     0x0004000000
#define MI_GOP_GWIN_RB_MEMORY_TYPE             (MIU0 | SW | CO_BUF_L0 | UNCACHED | TYPE_NONE)
#define MI_GOP_GWIN_RB_CMA_HID                 0

/* E_MMAP_ID_IOMMU_MPU_TABLE   */
#define E_MMAP_ID_IOMMU_MPU_TABLE_LAYER        0
#define E_MMAP_ID_IOMMU_MPU_TABLE_AVAILABLE    0x0012463000
#define E_MMAP_ID_IOMMU_MPU_TABLE_ADR          0x0012463000  //Alignment 0x01000
#define E_MMAP_ID_IOMMU_MPU_TABLE_GAP_CHK      0x0000000000
#define E_MMAP_ID_IOMMU_MPU_TABLE_LEN          0x0000220000
#define E_MMAP_ID_IOMMU_MPU_TABLE_MEMORY_TYPE  (MIU0 | SW | CO_BUF_L0 | UNCACHED | TYPE_NONE)
#define E_MMAP_ID_IOMMU_MPU_TABLE_CMA_HID      0

/* MI_VDEC_HDR_INFO   */
#define MI_VDEC_HDR_INFO_LAYER                 0
#define MI_VDEC_HDR_INFO_AVAILABLE             0x0012683000
#define MI_VDEC_HDR_INFO_ADR                   0x0012683000  //Alignment 0x01000
#define MI_VDEC_HDR_INFO_GAP_CHK               0x0000000000
#define MI_VDEC_HDR_INFO_LEN                   0x000042A000
#define MI_VDEC_HDR_INFO_MEMORY_TYPE           (MIU0 | SW | CO_BUF_L0 | UNCACHED | TYPE_NONE)
#define MI_VDEC_HDR_INFO_CMA_HID               0

/* MI_LINUX_FBDEV_BUF   */
#define MI_LINUX_FBDEV_BUF_LAYER               0
#define MI_LINUX_FBDEV_BUF_AVAILABLE           0x0012AAD000
#define MI_LINUX_FBDEV_BUF_ADR                 0x0012AAD000  //Alignment 0x01000
#define MI_LINUX_FBDEV_BUF_GAP_CHK             0x0000000000
#define MI_LINUX_FBDEV_BUF_LEN                 0x0001A00000
#define MI_LINUX_FBDEV_BUF_MEMORY_TYPE         (MIU0 | SW | CO_BUF_L0 | UNCACHED | TYPE_NONE)
#define MI_LINUX_FBDEV_BUF_CMA_HID             0

/* MI_LINUX_FBDEV_BUF1   */
#define MI_LINUX_FBDEV_BUF1_LAYER              0
#define MI_LINUX_FBDEV_BUF1_AVAILABLE          0x00144AD000
#define MI_LINUX_FBDEV_BUF1_ADR                0x00144AD000  //Alignment 0x01000
#define MI_LINUX_FBDEV_BUF1_GAP_CHK            0x0000000000
#define MI_LINUX_FBDEV_BUF1_LEN                0x0001800000
#define MI_LINUX_FBDEV_BUF1_MEMORY_TYPE        (MIU0 | SW | CO_BUF_L0 | UNCACHED | TYPE_NONE)
#define MI_LINUX_FBDEV_BUF1_CMA_HID            0

/* MI_CAP_VIDEO_MORPHY_POOL   */
#define MI_CAP_VIDEO_MORPHY_POOL_LAYER         0
#define MI_CAP_VIDEO_MORPHY_POOL_AVAILABLE     0x0015CAD000
#define MI_CAP_VIDEO_MORPHY_POOL_ADR           0x0015CAD000  //Alignment 0x01000
#define MI_CAP_VIDEO_MORPHY_POOL_GAP_CHK       0x0000000000
#define MI_CAP_VIDEO_MORPHY_POOL_LEN           0x0000C00000
#define MI_CAP_VIDEO_MORPHY_POOL_MEMORY_TYPE   (MIU0 | SW | CO_BUF_L0 | UNCACHED | TYPE_NONE)
#define MI_CAP_VIDEO_MORPHY_POOL_CMA_HID       0

/* MI_KERNEL_POOL2   */
#define MI_KERNEL_POOL2_LAYER                  0
#define MI_KERNEL_POOL2_AVAILABLE              0x00168AD000
#define MI_KERNEL_POOL2_ADR                    0x0016C00000  //Alignment 0x400000
#define MI_KERNEL_POOL2_GAP_CHK                0x0000353000
#define MI_KERNEL_POOL2_LEN                    0x0027100000
#define MI_KERNEL_POOL2_MEMORY_TYPE            (MIU0 | SW | CO_BUF_L0 | UNCACHED | TYPE_NONE)
#define MI_KERNEL_POOL2_CMA_HID                0

/* MI_EXTIN_VD_COMB_BUF   */
#define MI_EXTIN_VD_COMB_BUF_LAYER             1
#define MI_EXTIN_VD_COMB_BUF_AVAILABLE         0x0016C00000
#define MI_EXTIN_VD_COMB_BUF_ADR               0x0016C00000  //Alignment 0x01000
#define MI_EXTIN_VD_COMB_BUF_GAP_CHK           0x0000000000
#define MI_EXTIN_VD_COMB_BUF_LEN               0x0000400000
#define MI_EXTIN_VD_COMB_BUF_MEMORY_TYPE       (MIU0 | SW | CO_BUF_L1 | UNCACHED | CMA)
#define MI_EXTIN_VD_COMB_BUF_CMA_HID           23

/* MI_OTHERS_RETURN   */
#define MI_OTHERS_RETURN_LAYER                 2
#define MI_OTHERS_RETURN_AVAILABLE             0x0016C00000
#define MI_OTHERS_RETURN_ADR                   0x0016C00000  //Alignment 0x400000
#define MI_OTHERS_RETURN_GAP_CHK               0x0000000000
#define MI_OTHERS_RETURN_LEN                   0x0000400000
#define MI_OTHERS_RETURN_MEMORY_TYPE           (MIU0 | SW | CO_BUF_L2 | UNCACHED | CMA)
#define MI_OTHERS_RETURN_CMA_HID               23

/* MI_VDEC_FRAME_BUF   */
#define MI_VDEC_FRAME_BUF_LAYER                2
#define MI_VDEC_FRAME_BUF_AVAILABLE            0x0017000000
#define MI_VDEC_FRAME_BUF_ADR                  0x0017000000  //Alignment 0x400000
#define MI_VDEC_FRAME_BUF_GAP_CHK              0x0000000000
#define MI_VDEC_FRAME_BUF_LEN                  0x0000000000
#define MI_VDEC_FRAME_BUF_MEMORY_TYPE          (MIU0 | SW | CO_BUF_L2 | WRITE_BACK | TYPE_NONE)
#define MI_VDEC_FRAME_BUF_CMA_HID              0

/* E_MMAP_ID_ARMFW_MEM   */
#define E_MMAP_ID_ARMFW_MEM_LAYER              0
#define E_MMAP_ID_ARMFW_MEM_AVAILABLE          0x003DD00000
#define E_MMAP_ID_ARMFW_MEM_ADR                0x003DD00000  //Alignment 0x10000
#define E_MMAP_ID_ARMFW_MEM_GAP_CHK            0x0000000000
#define E_MMAP_ID_ARMFW_MEM_LEN                0x0000040000
#define E_MMAP_ID_ARMFW_MEM_MEMORY_TYPE        (MIU0 | SW | CO_BUF_L0 | UNCACHED | TYPE_NONE)
#define E_MMAP_ID_ARMFW_MEM_CMA_HID            0

/* E_MMAP_ID_OPTEE_MEM   */
#define E_MMAP_ID_OPTEE_MEM_LAYER              0
#define E_MMAP_ID_OPTEE_MEM_AVAILABLE          0x003DD40000
#define E_MMAP_ID_OPTEE_MEM_ADR                0x003DE00000  //Alignment 0x100000
#define E_MMAP_ID_OPTEE_MEM_GAP_CHK            0x00000C0000
#define E_MMAP_ID_OPTEE_MEM_LEN                0x0001B00000
#define E_MMAP_ID_OPTEE_MEM_MEMORY_TYPE        (MIU0 | SW | CO_BUF_L0 | UNCACHED | TYPE_NONE)
#define E_MMAP_ID_OPTEE_MEM_CMA_HID            0

/* E_MMAP_ID_HW_AES_BUF   */
#define E_MMAP_ID_HW_AES_BUF_LAYER             0
#define E_MMAP_ID_HW_AES_BUF_AVAILABLE         0x003F900000
#define E_MMAP_ID_HW_AES_BUF_ADR               0x003F900000  //Alignment 0x100000
#define E_MMAP_ID_HW_AES_BUF_GAP_CHK           0x0000000000
#define E_MMAP_ID_HW_AES_BUF_LEN               0x0000200000
#define E_MMAP_ID_HW_AES_BUF_MEMORY_TYPE       (MIU0 | SW | CO_BUF_L0 | UNCACHED | TYPE_NONE)
#define E_MMAP_ID_HW_AES_BUF_CMA_HID           0

/* E_MMAP_ID_OPTEE_MEM_SHM   */
#define E_MMAP_ID_OPTEE_MEM_SHM_LAYER          0
#define E_MMAP_ID_OPTEE_MEM_SHM_AVAILABLE      0x003FB00000
#define E_MMAP_ID_OPTEE_MEM_SHM_ADR            0x003FB00000  //Alignment 0x100000
#define E_MMAP_ID_OPTEE_MEM_SHM_GAP_CHK        0x0000000000
#define E_MMAP_ID_OPTEE_MEM_SHM_LEN            0x0000500000
#define E_MMAP_ID_OPTEE_MEM_SHM_MEMORY_TYPE    (MIU0 | SW | CO_BUF_L0 | UNCACHED | TYPE_NONE)
#define E_MMAP_ID_OPTEE_MEM_SHM_CMA_HID        0

/* MI_DUMMY_FAKE   */
#define MI_DUMMY_FAKE_LAYER                    0
#define MI_DUMMY_FAKE_AVAILABLE                0x0040000000
#define MI_DUMMY_FAKE_ADR                      0x0040000000  //Alignment 0x01000
#define MI_DUMMY_FAKE_GAP_CHK                  0x0000000000
#define MI_DUMMY_FAKE_LEN                      0x0020000000
#define MI_DUMMY_FAKE_MEMORY_TYPE              (MIU0 | SW | CO_BUF_L0 | UNCACHED | TYPE_NONE)
#define MI_DUMMY_FAKE_CMA_HID                  0

/* MI_FAKEMEM   */
#define MI_FAKEMEM_LAYER                       2
#define MI_FAKEMEM_AVAILABLE                   0x0040000000
#define MI_FAKEMEM_ADR                         0x0040000000  //Alignment 0x10000000
#define MI_FAKEMEM_GAP_CHK                     0x0000000000
#define MI_FAKEMEM_LEN                         0x0020000000
#define MI_FAKEMEM_MEMORY_TYPE                 (MIU0 | SW | CO_BUF_L2 | UNCACHED | CMA)
#define MI_FAKEMEM_CMA_HID                     30

//MIU_1_START
//MIU_END
//MIU_2_START
//MIU_2_END
//MIU_3_START
//MIU_3_END
#define MIU0_END_ADR                           0x0040000000
