/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright(c) 1999 - 2025 Intel Corporation. */

#ifndef _IXGBE_HV_VF_H_
#define _IXGBE_HV_VF_H_

/* On Hyper-V, to reset, we need to read from this offset
 * from the PCI config space. This is the mechanism used on
 * Hyper-V to support PF/VF communication.
 */
#define IXGBE_HV_RESET_OFFSET           0x201

#include "ixgbe_vf.h"

s32 ixgbevf_hv_init_ops_vf(struct ixgbe_hw *hw);
s32 ixgbevf_hv_check_mac_link_vf(struct ixgbe_hw *hw, ixgbe_link_speed *speed,
				 bool *link_up, bool autoneg_wait_to_complete);
s32 ixgbevf_hv_set_uc_addr_vf(struct ixgbe_hw *hw, u32 index, u8 *addr);
s32 ixgbevf_hv_update_mc_addr_list_vf(struct ixgbe_hw *hw, u8 *mc_addr_list,
				      u32 mc_addr_count, ixgbe_mc_addr_itr,
				      bool clear);
s32 ixgbevf_hv_update_xcast_mode(struct ixgbe_hw *hw, int xcast_mode);
s32 ixgbevf_hv_set_vfta_vf(struct ixgbe_hw *hw, u32 vlan, u32 vind,
			   bool vlan_on, bool vlvf_bypass);
s32 ixgbevf_hv_set_rlpml_vf(struct ixgbe_hw *hw, u16 max_size);
int ixgbevf_hv_negotiate_api_version_vf(struct ixgbe_hw *hw, int api);

extern s32 ixgbevf_hv_reset_hw_vf(struct ixgbe_hw *hw);
extern s32 ixgbevf_hv_set_rar_vf(struct ixgbe_hw *hw, u32 index, u8 *addr,
				 u32 vmdq, u32 enable_addr);
#endif /* _IXGBE_HV_VF_H_ */
